#ifndef BOSON_UTILITY_H_
#define BOSON_UTILITY_H_
#include <type_traits>
#include <utility>
#include <tuple>
#include "std/experimental/apply.h"

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

template <class T>
struct value_arg {
  value_arg(value_arg const&) = delete;
  value_arg(value_arg &&) = delete;
  operator std::decay_t<T> ();
  operator std::decay_t<T> const& () const = delete;
};

//template <class T>
//struct func_traits {
  //static constexpr bool is_generic = true;
//};
template <typename T>
struct func_traits : public func_traits<decltype(&T::operator())> {};

template <typename C, typename Ret, typename... Args>
struct func_traits<Ret(C::*)(Args...) const> : func_traits<Ret(*)(Args...)>
{};

template <typename Ret, typename... Args>
struct func_traits<Ret(*)(Args...)> {
  static constexpr bool is_generic = false;
  using result_type =  Ret;
  using args_tuples_type = std::tuple<Args...>;
  template <std::size_t i>
  struct arg {
      using type = typename std::tuple_element<i, std::tuple<Args...>>::type;
  };
};


//template <class F> struct has_operator {
  //template<typename T> static constexpr auto has() -> typename std::is_same<decltype(&F::operator()), decltype(&F::operator())>::type;  
  //template<typename> static constexpr std::false_type has(...);
  //static constexpr bool value = decltype(has<F>(0))::value;
//};

//template <typename T>
//void option(T&& t) {
    //using traits = func_traits<typename std::decay<T>::type>;
    //using return_t = typename traits::result_type;         // Return type.
    //using arg0_t = typename traits::template arg<0>::type; // First arg type.
//}

struct empty {
};

template <class Ret, class F, class InTuple, class SeqHead, class SeqTail, std::size_t Index> struct extract_tuple_argument_impl;
template <class Ret, class F, class InTuple, size_t Index, size_t ... IHead, size_t ... ITail> struct extract_tuple_argument_impl<Ret, F, InTuple, std::index_sequence<IHead...>,  std::index_sequence<ITail...>, Index> {
  template<typename T> static constexpr auto check_take(T*) -> typename std::is_same< decltype( std::declval<F>()( std::declval<std::tuple_element_t<IHead,InTuple>>()... , std::declval<T>(), std::declval<std::tuple_element_t<ITail+Index+1,InTuple>>()...)), Ret>::type;  
  template<typename> static constexpr std::false_type check_take(...);
  using Arg = std::tuple_element_t<Index,InTuple>;
  static constexpr bool take_copy = decltype(check_take<value_arg<Arg>>(0))::value;
  static constexpr bool is_generic = decltype(check_take<empty>(0))::value;
  using type = std::conditional_t< (!take_copy && is_generic) || (take_copy && !is_generic), std::decay_t<Arg>, Arg>;
};

template <class Ret, class F, class InTuple, size_t Index> struct extract_tuple_argument {
  using type = typename extract_tuple_argument_impl<Ret, F, InTuple, std::make_index_sequence<Index>, std::make_index_sequence<std::tuple_size<InTuple>::value - 1 - Index>, Index>::type;
};

template <class Ret, class F, class InTuple, class Seq> struct extract_tuple_arguments_impl;
template <class Ret, class F, class InTuple, size_t ... Index> struct extract_tuple_arguments_impl<Ret, F, InTuple, std::index_sequence<Index...>> {
  using type = std::tuple<typename extract_tuple_argument<Ret, F, InTuple, Index>::type ...>;
};

template <class F, class ... Args> struct extract_tuple_arguments {
  using type = typename extract_tuple_arguments_impl<decltype(std::declval<F>()(std::declval<Args>()...)), F, std::tuple<Args...>, std::make_index_sequence<sizeof ... (Args)>>::type;
  //using type = std::conditional_t<func_traits<F>::is_generic, std::tuple<Args...>, typename func_traits<F>::args_tuples_type>;
};

}  // namespace boson

#endif  // BOSON_SYSCALLS_H_
