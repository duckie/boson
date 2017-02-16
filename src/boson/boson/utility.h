#ifndef BOSON_UTILITY_H_
#define BOSON_UTILITY_H_
#include <type_traits>
#include <utility>
#include <tuple>
#include "std/experimental/apply.h"

namespace boson {

template <class T>
struct value_arg {
  value_arg(value_arg const&) = delete;
  value_arg(value_arg &&) = delete;
  operator std::decay_t<T> ();
  operator std::decay_t<T> const& () const = delete;
};

struct routine_placeholder_probe {};

template <class Ret, class F, class InTuple, class SeqHead, class SeqTail, std::size_t Index>
struct extract_tuple_argument_impl;
template <class Ret, class F, class InTuple, size_t Index, size_t... IHead, size_t... ITail>
struct extract_tuple_argument_impl<Ret, F, InTuple, std::index_sequence<IHead...>,
                                   std::index_sequence<ITail...>, Index> {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wundefined-internal"
#pragma GCC diagnostic ignored "-Wundefined-inline"
  template <typename T>
  static constexpr auto check_take(T*) -> typename std::is_same<
      decltype(std::declval<F>()(
          std::declval<std::tuple_element_t<IHead, InTuple>>()..., std::declval<T>(),
          std::declval<std::tuple_element_t<ITail + Index + 1, InTuple>>()...)),
      Ret>::type;
  template <typename>
  static constexpr std::false_type check_take(...);
  using Arg = std::tuple_element_t<Index, InTuple>;
  static constexpr bool take_copy = decltype(check_take<value_arg<Arg>>(0))::value;
  static constexpr bool is_generic = decltype(check_take<routine_placeholder_probe>(0))::value;
  using type = std::conditional_t<(!take_copy && is_generic) || (take_copy && !is_generic),
                                  std::decay_t<Arg>, Arg>;
#pragma GCC diagnostic pop
};

template <class Ret, class F, class InTuple, size_t Index>
struct extract_tuple_argument {
  using type = typename extract_tuple_argument_impl<
      Ret, F, InTuple, std::make_index_sequence<Index>,
      std::make_index_sequence<std::tuple_size<InTuple>::value - 1 - Index>, Index>::type;
};

template <class Ret, class F, class InTuple, class Seq>
struct extract_tuple_arguments_impl;
template <class Ret, class F, class InTuple, size_t... Index>
struct extract_tuple_arguments_impl<Ret, F, InTuple, std::index_sequence<Index...>> {
  using type = std::tuple<typename extract_tuple_argument<Ret, F, InTuple, Index>::type...>;
};

template <class F, class... Args>
struct extract_tuple_arguments {
  using type =
      typename extract_tuple_arguments_impl<decltype(std::declval<F>()(std::declval<Args>()...)), F,
                                            std::tuple<Args...>,
                                            std::make_index_sequence<sizeof...(Args)>>::type;
};

template <class T>
struct is_unique_ptr : public std::false_type {};
template <class T>
struct is_unique_ptr<std::unique_ptr<T>> : public std::true_type {};

}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
