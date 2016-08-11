#ifndef BOSON_ROUTINE_H_
#define BOSON_ROUTINE_H_
#pragma once

namespace boson {

/**
 * routine represents a signel unit of execution
 *
 */
template <class Function>
class routine {
  Function task_;

public:
  routine();
  routine(routine const&) = delete;
  routine(routine&&) = default;
  routine& operator=(routine const&) = delete;
  routine& operator=(routine&&) = default;




};

};

#endif  // BOSON_ROUTINE_H_
