#ifndef BOSON_THREAD_H_
#define BOSON_THREAD_H_
#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <deque>
#include <map>
#include <memory>
#include <thread>
#include <vector>
#include <condition_variable>
#include "boson/event_loop.h"
#include "boson/memory/local_ptr.h"
#include "boson/memory/sparse_vector.h"
#include "boson/queues/mpsc.h"
#include "boson/queues/simple.h"
#include "boson/queues/lcrq.h"
#include "boson/queues/vectorized_queue.h"
#include "boson/internal/netpoller.h"
#include "routine.h"
#include "../external/json_backbone.hpp"

namespace json_backbone {
template <>
struct is_small_type<std::unique_ptr<boson::internal::routine>> {
  constexpr static bool const value = true;
};
}

namespace boson {

class engine;
class semaphore;
using thread_id = std::size_t;

namespace internal {

enum class thread_status {
  idle,       // Thread waits to be unlocked
  busy,       // Thread executes a routine
  finishing,  // Thread no longer executes a routine and is not required to wait
  finished    // Thread no longer executes a routine and is not required to wait
};

enum class thread_command_type { add_routine, schedule_waiting_routine, finish, fd_ready };

using thread_fd_event = std::tuple<std::size_t, int, event_status, bool>;

using thread_command_data =
    json_backbone::variant<std::nullptr_t, int, routine_ptr_t,
                           std::pair<std::weak_ptr<semaphore>, std::size_t>, thread_fd_event>;
//using thread_command_data = json_backbone::variant<std::nullptr_t, int, routine_ptr_t, std::pair<semaphore*, routine*>>;

struct thread_command {
  thread_command_type type;
  thread_command_data data;
  inline thread_command(thread_command_type new_type, thread_command_data new_data)
      : type(new_type), data(std::move(new_data)) {
  }
};

/**
 * engine_proxy represents and engine view from the thread
 *
 * It encapsulates the semantics for the thread to identify
 * on the engine. currently, this semantics is an id.
 */
class engine_proxy final {
  // Use a pointer here to get free move ctor and operator
  engine* engine_;
  thread_id current_thread_id_;

 public:
  engine_proxy(engine&);
  ~engine_proxy();
  void set_id();
  routine_id get_new_routine_id();
  void notify_end();
  void notify_idle(size_t nb_suspended_routines);
  void start_routine(std::unique_ptr<routine> new_routine);
  void start_routine(thread_id target_thread, std::unique_ptr<routine> new_routine);
  
  inline thread_id get_id() const {
    return current_thread_id_;
  }
  inline engine& get_engine() const {
    return *engine_;
  }
};

// Holds pointers to routines waiting for a time out
struct timed_routines_set final {
  std::size_t nb_active = 0;
  std::deque<std::size_t> slots;
};

struct routine_slot {
  routine_local_ptr_t ptr;
  std::size_t event_index;
};

/**
 * Thread encapsulates an instance of an real thread
 *
 */
class thread : public internal::net_event_handler<uint64_t> {
  friend void detail::resume_routine(transfer_t);
  friend void boson::yield();
  friend void boson::usleep(std::chrono::microseconds);
  template <bool, bool>
  friend int boson::wait_readiness(fd_t, int);
  friend fd_t boson::open(const char*, int);
  friend fd_t boson::open(const char*, int, mode_t);
  friend fd_t boson::creat(const char*, mode_t);
  friend int boson::pipe(fd_t (&fds)[2]);
  friend int boson::pipe2(fd_t (&fds)[2], int);
  friend socket_t boson::socket(int, int, int);
  template <bool>
  friend socket_t boson::accept_impl(socket_t, sockaddr*, socklen_t*, int);
  friend int boson::close(int);
  template <class ContentType>
  friend class channel;
  friend class routine;

  friend class boson::semaphore;
  using engine_queue_t = queues::mpsc<std::unique_ptr<thread_command>>;
  //using engine_queue_t = queues::simple_queue<std::unique_ptr<thread_command>>;
  //using engine_queue_t = queues::vectorized_queue<std::unique_ptr<thread_command>>; // NOT THREAD SAFE !!

  engine_proxy engine_proxy_;
  std::deque<routine_slot> scheduled_routines_;
  thread_status status_{thread_status::idle};

  /**
   * Execution context used to jump between thread and its routines
   *
   * Useful to get it from the TLS
   */
  transfer_t context_;

  /**
   * Holds a pointer to the currently runninf routine
   *
   * Useful to get it from the TLS
   */
  routine* running_routine_;

  /**
   * Event loop managing interruptions
   */
  netpoller<uint64_t> event_loop_;

  engine_queue_t engine_queue_;
  std::atomic<std::size_t> nb_pending_commands_{0};
  int engine_event_id_;
  int self_event_id_;


  /**
   * This map stores the timers
   *
   * The idea here is to avoid additional fd creation just for timers, so we can create
   * a whole lot of them without consuming the fd limit per process
   */
  std::map<routine_time_point, timed_routines_set> timed_routines_;

  /**
   * Stores the number of suspended routines
   *
   * This number contains the number of suspended routines stored
   * in the event loop. The event loop holds the routines waiting for
   * events of its FDs.
   *
   * This also counts routines waiting in a semaphore waiters list.
   */
  size_t nb_suspended_routines_{0};

  memory::sparse_vector<routine_slot> suspended_slots_;

  /**
   * Struct to store the shared buffer
   *
   * The shared buffer is a way for the user to use a thread local
   * buffer for io without having to allocate the memory on every
   * stack
   */
  struct shared_buffer_storage {
    char* buffer;
    shared_buffer_storage(size_t size);
    ~shared_buffer_storage();
  };

  // Instances of the shared buffer
  std::map<std::size_t, shared_buffer_storage> shared_buffers_;

  /**
   * React to a request from the main scheduler
   */
  void handle_engine_event();

  /**
   * Close event handlers to free the event loop
   */
  void unregister_all_events();

  inline transfer_t& context();

  // Returns the set in which the routine has ben referenced
  // used to decrement the set nb_active member when disabling the timer
  timed_routines_set& register_timer(routine_time_point const& date, routine_slot slot);

  // Returns the slot index used to push in the semaphore waiters queue
  std::size_t register_semaphore_wait(routine_slot slot);

  // Registers a fd for reading. 
  //
  // Returns event loop event id
  //
  int register_read(int fd, routine_slot slot);

  // Registers a fd for writing
  //
  // Returns event loop event id
  //
  int register_write(int fd, routine_slot slot);

  /**
   * Unregisters the given slot
   *
   * This is used when cancelling semaphore events directly into their queue
   */
  void unregister_expired_slot(std::size_t slot_index);

  /**
   * Signals the event_loop that a fd has been closed
   *
   * This is useful to make sure new events are created
   * when the fd is reused
   */
  void unregister_fd(int fd);

  /**
   * Wakes up a waiting thread
   */
  void wakeUp();

 public:
  thread(engine& parent_engine);
  thread(thread const&) = delete;
  thread(thread&&) = default;
  thread& operator=(thread const&) = delete;
  thread& operator=(thread&&) = default;
  ~thread();

  inline thread_id id() const;
  inline engine const& get_engine() const;
  inline engine& get_engine();

  void read(fd_t fd, uint64_t data, event_status status) override;
  void write(fd_t fd, uint64_t data, event_status status) override;
  void callback() override;
  //void read(int fd, void* data, event_status status);
  //void write(int fd, void* data, event_status status);

  // called by engine
  void push_command(thread_id from, std::unique_ptr<thread_command> command);

  bool execute_scheduled_routines();

  /**
   * Executes the boson::thread
   *
   * loop() is executed in a std::thread dedicated to this object
   */
  void loop();

  /**
   * Starts a new routine
   */
  template <class Function, class... Args>
  void start_routine(Function&& func, Args&&... args) {
    engine_proxy_.start_routine(std::make_unique<routine>(engine_proxy_.get_new_routine_id(),
                                                          std::forward<Function>(func),
                                                          std::forward<Args>(args)...));
    }

    /**
     * Starts a new routine in a specific thread
     */
    template <class Function, class... Args>
    void start_routine_explicit(thread_id id, Function && func, Args && ... args) {
      engine_proxy_.start_routine(
          id, std::make_unique<routine>(engine_proxy_.get_new_routine_id(),
                                        std::forward<Function>(func), std::forward<Args>(args)...));
    }

    /**
     * Returns the currently running routine
     */
    inline routine* running_routine();

    /**
     * Returns a memory buffer suitable for a shared_buffer
     *
     * See documentation of boson::shared_buffer
     */
    char* get_shared_buffer(std::size_t minimum_size);

    void signal_fd_closed(fd_t fd);

    void schedule_routine(routine_slot&& slot);
};

/**
 * Stores current thread in the TLS
 *
 * This is not convenient, from a programmatic standpoint, to use
 * the notion of current_thread, as a current_routine would be better
 *
 * But it avoids TLS writes at each context switch
 */
thread*& current_thread();

transfer_t& thread::context() {
  return context_;
}

routine* thread::running_routine() {
  return running_routine_;
}

thread_id thread::id() const {
  return engine_proxy_.get_id();
}

engine const& thread::get_engine() const {
  return engine_proxy_.get_engine();
}

engine& thread::get_engine() {
  return engine_proxy_.get_engine();
}

}  // namespace internal

template <class Function, class... Args>
void start_explicit(thread_id id, Function&& func, Args&&... args) {
  internal::current_thread()->start_routine_explicit(id, std::forward<Function>(func),
                                            std::forward<Args>(args)...);
}

template <class Function, class... Args>
void start(Function&& func, Args&&... args) {
  internal::current_thread()->start_routine(std::forward<Function>(func),
                                            std::forward<Args>(args)...);
}

}  // namespace boson

#endif  // BOSON_THREAD_H_
