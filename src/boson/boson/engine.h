#ifndef BOSON_ENGINE_H_
#define BOSON_ENGINE_H_
#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <thread>
#include <tuple>
#include <vector>
#include "internal/routine.h"
#include "internal/thread.h"

namespace boson {

/**
 * engine encapsulates an instance of the boson runtime
 *
 */
class engine {
  using thread_t = internal::thread;
  using command_t = internal::thread_command;
  using proxy_t = internal::engine_proxy;

  struct thread_view {
    thread_t thread;
    std::thread std_thread;

    inline thread_view(engine& engine) : thread{engine} {
    }
  };

  using thread_view_t = thread_view;
  using thread_list_t = std::vector<std::unique_ptr<thread_view_t>>;

  friend class internal::engine_proxy;

  thread_list_t threads_;
  size_t max_nb_cores_;
  std::atomic<thread_id> current_thread_id_{0};

  // This is used to add routines from the external main thread
  //
  // Should not be used a lot.
  thread_id next_scheduled_thread_{0};

  /**
   * Registers a new thread
   *
   * This function is called from any new thread which
   * is in creation state. The goal here is to get a numerical
   * id to avoid paying for a std::[unordered_]map find or a
   * vector lookup. This price will be paid with a thread_local
   * id
   */
  thread_id register_thread_id();

 public:
  engine(size_t max_nb_cores = 1);
  engine(engine const&) = delete;
  engine(engine&&) = default;
  engine& operator=(engine const&) = delete;
  engine& operator=(engine&&) = default;
  ~engine();

  /***
   * Starts a routine into the given thread
   */
  template <class Function>
  void start(thread_id id, Function&& function);

  /**
   * Starts a routine in whatever thread the engine sees fit
   */
  template <class Function>
  void start(Function&& function);
};

// Inline/template implementations

template <class Function>
void engine::start(thread_id id, Function&& function) {
  // Select a thread
  threads_.at(id)->thread.push_command(
  command_t{internal::thread_command_type::add_routine,
                    new internal::routine{std::forward<Function>(function)}});
  //threads_.at(id)->thread.execute_commands();
};

template <class Function>
void engine::start(Function&& function) {
  // Select a thread
  this->start(next_scheduled_thread_, std::forward<Function>(function));
  next_scheduled_thread_ = (next_scheduled_thread_ + 1) % max_nb_cores_;
};

}  // namespace boson

#endif  // BOSON_ENGINE_H_
