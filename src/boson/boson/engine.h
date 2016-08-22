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
  using thread_id_t = std::atomic<std::size_t>;

  friend class context::engine_proxy<StackTraits>;

  thread_list_t threads_;
  size_t max_nb_cores_;
  thread_id_t current_thread_id_{0};

  /**
   * Registers a new thread
   *
   * This function is called from any new thread which 
   * is in creation state. The goal here is to get a numerical
   * id to avoid paying for a std::[unordered_]map find or a 
   * vector lookup. This price will be paid with a thread_local
   * id
   */
  size_t register_thread_id() {
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
    command_t command { context::thread_command_type::finish, nullptr };
    threads_[0]->thread.push_command(command);
    threads_[0]->thread.execute_commands();

    threads_[0]->std_thread.join();
  };

  template <class Function> void start(Function&& function) {
    // Select a thread
    command_t command { context::thread_command_type::add_routine, nullptr };
    threads_[0]->thread.push_command(command);
    threads_[0]->thread.execute_commands();
  };
};

};

#endif  // BOSON_ENGINE_H_ 
