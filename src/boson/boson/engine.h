#ifndef BOSON_ENGINE_H_
#define BOSON_ENGINE_H_
#pragma once

#include <list>
#include "routine.h"

namespace boson {

/**
 * engine encapsulates an instance of the boson runtime
 *
 */
class engine {
  std::list<routine> scheduled_routines_;
  routine* current_routine_ = nullptr;

public:
  engine();
  engine(engine const&) = delete;
  engine(engine&&) = default;
  engine& operator=(engine const&) = delete;
  engine& operator=(engine&&) = default;

  template <class Function> start(Function&& function) {
  };

};

};

#endif  // BOSON_ENGINE_H_ 
