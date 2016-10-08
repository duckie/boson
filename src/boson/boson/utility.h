#ifndef BOSON_UTILITY_H_
#define BOSON_UTILITY_H_
#include <type_traits>
#include <utility>

namespace boson {

/**
 * Copies the argument
 *
 * This is usefull for force copying arguments passed to channels
 */
template <class T>
std::decay_t<T> dup(T arg) {
  return arg;
}

}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
