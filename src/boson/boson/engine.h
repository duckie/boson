#ifndef BOSON_ENGINE_H_
#define BOSON_ENGINE_H_
#pragma once

#include <list>
#include <memory>
#include <iostream>
#include "routine.h"

namespace boson {

/**
 * engine encapsulates an instance of the boson runtime
 *
 */
template <class StackTraits = stack::default_stack_traits>
class engine {
  using routine_t = routine<StackTraits>;
  std::list<std::unique_ptr<routine_t>> scheduled_routines_;

public:
  engine() = default;
  engine(engine const&) = delete;
  engine(engine&&) = default;
  engine& operator=(engine const&) = delete;
  engine& operator=(engine&&) = default;

  ~engine() {
    loop();
  };

  template <class Function> void start(Function&& function) {
    scheduled_routines_.emplace_back(new routine<StackTraits>(std::forward<Function>(function)));
  };

  void loop() {
    while(!scheduled_routines_.empty()) {
      // For now; we schedule them in order
      std::unique_ptr<routine_t> current_routine{scheduled_routines_.front().release()};
      scheduled_routines_.pop_front();
      current_routine->resume();
      if (routine_status::finished != current_routine->status()) {
        // If not finished, then we reschedule it
        scheduled_routines_.emplace_back(std::move(current_routine));
      }
    }
  }

};

};

#endif  // BOSON_ENGINE_H_ 
