#ifndef BOSON_STD_EXPERIMENTAL_CHRONO_H_
#define BOSON_STD_EXPERIMENTAL_CHRONO_H_

#include <cstdint>
#include <type_traits>
#include <chrono>
#include "type_traits.h"

namespace boson {
namespace experimental {
namespace chrono {

template <class T>
struct is_duration : std::false_type {};
template <class Rep, class Period>
struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

template <class To, class Rep, class Period, class = enable_if_t<is_duration<To>{}>>
constexpr To ceil(const std::chrono::duration<Rep, Period>& d) {
  To t = std::chrono::duration_cast<To>(d);
  if (t < d) return t + To{1};
  return t;
}

template <class To, class Clock, class FromDuration, class = std::enable_if_t<is_duration<To>{}>>
constexpr std::chrono::time_point<Clock, To> ceil(
    const std::chrono::time_point<Clock, FromDuration>& tp) {
  return std::chrono::time_point<Clock, To>{ceil<To>(tp.time_since_epoch())};
}
}
}
}

#endif
