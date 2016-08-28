#ifndef BOSON_ENGINE_H_
#define BOSON_ENGINE_H_
#pragma once

#include <vector>
#include <memory>
#include <tuple>
#include <thread>
#include <future>
#include <atomic>
#include "routine.h"
#include "thread.h"
#include <iostream>

namespace boson {

namespace context {
}


/**
 * engine encapsulates an instance of the boson runtime
 *
 */
template <class StackTraits = stack::default_stack_traits>
class engine {
  using routine_t = routine<StackTraits>;
  using thread_t = context::thread<StackTraits>;
  using command_t = context::thread_command<StackTraits>;
  using proxy_t = context::engine_proxy<StackTraits>;

  struct thread_view {
    thread_t thread;
    std::thread std_thread;

    thread_view(engine& engine) : thread{engine} {
    }
  };

  using thread_view_t = thread_view;
  using thread_list_t = std::vector<std::unique_ptr<thread_view_t>>;

  friend class context::engine_proxy<StackTraits>;

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
  thread_id register_thread_id() {
    auto new_id = ++current_thread_id_;
    return new_id;
  }


public:
  engine(size_t max_nb_cores = 1) : max_nb_cores_{max_nb_cores} {
    // Start all threads directly
    threads_.reserve(max_nb_cores);
    for (size_t index = 0; index < max_nb_cores_; ++index) {
      threads_.emplace_back(new thread_view_t(*this));
      auto& created_thread = threads_.back();
      threads_.back()->std_thread = std::thread([&created_thread]() { created_thread->thread(); });
    }
  }
  engine(engine const&) = delete;
  engine(engine&&) = default;
  engine& operator=(engine const&) = delete;
  engine& operator=(engine&&) = default;

  ~engine() {
    // Send a request to thread to finish when they can
    for(auto& thread : threads_) {
      command_t command { context::thread_command_type::finish, nullptr };
      thread->thread.push_command(command);
      thread->thread.execute_commands();
    }

    // Join everyone
    for(auto& thread : threads_) {
      thread->std_thread.join();
    }
  };

  template <class Function> void start(thread_id id, Function&& function) {
    // Select a thread
    command_t command {context::thread_command_type::add_routine, new routine_t{std::forward<Function>(function)}};
    threads_.at(id)->thread.push_command(command);
    threads_.at(id)->thread.execute_commands();
  };

  template <class Function> void start(Function&& function) {
    // Select a thread
    this->start(next_scheduled_thread_, std::forward<Function>(function));
    next_scheduled_thread_ = (next_scheduled_thread_ + 1) % max_nb_cores_;
  };

};

};

#endif  // BOSON_ENGINE_H_ 
