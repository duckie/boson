#ifndef BOSON_ENGINE_H_
#define BOSON_ENGINE_H_
#pragma once

#include <vector>
#include <memory>
#include <iostream>
#include <tuple>
#include <thread>
#include <future>
#include "routine.h"
#include "thread.h"

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
  using proxy_t = context::engine_proxy<StackTraits>;
  using thread_view_t = thread_t;
  using thread_list_t = std::vector<thread_view_t>;
  using thread_id_t = size_t;

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
  thread_id_t register_thread_id() {
    auto new_id = ++current_thread_id_;
    return new_id;
  }

public:
  engine(size_t max_nb_cores = 1) : max_nb_cores_{max_nb_cores} {
    threads_.reserve(max_nb_cores + 1);
    // Start all threads directly
  }
  engine(engine const&) = delete;
  engine(engine&&) = default;
  engine& operator=(engine const&) = delete;
  engine& operator=(engine&&) = default;

  ~engine() {
    loop();
  };

  template <class Function> void start(Function&& function) {
  };

  void loop() {
  }

};

};

#endif  // BOSON_ENGINE_H_ 
