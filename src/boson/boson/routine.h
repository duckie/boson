#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

namespace boson {

/**
 * routine represents a signel unit of execution
 *
 */
template <class Function, class StackTraits = context::default_stack_traits>
class routine {
  Function func_;
  stack_context

public:
 routine(Function&& func) : func_(std::forward<Function>(func)) {
 }

 routine(routine const&) = delete;
 routine(routine&&) = default;
 routine& operator=(routine const&) = delete;
 routine& operator=(routine&&) = default;

 void resume();
};

};

#endif  // BOSON_ROUTINE_H_
