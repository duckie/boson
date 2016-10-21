#include "catch.hpp"
#include "boson/utility.h"
#include <utility>

using namespace boson;
using namespace std;

namespace {
struct A {};
struct B {};
struct C {};

int a1(int) { return 0; }
int a2(int&) { return 0;}
int a3(int const&) { return 0; }

int f1(A,B,C);
int f2(A,B&,C);
int f3(A,B const&,C);
}

TEST_CASE("Check templates","[static]") {
  auto fl1 = [](A a, B b, C c) -> void {};
  auto fl2 = [](A a, B &b, C c) -> void {};
  auto fl3 = [](A a, B const& b, C c) -> void {};
  auto fg1 = [](auto a, auto b, auto c) -> void {};
  auto fg2 = [](auto a, auto& b, auto c) -> void {};
  auto fg3 = [](auto a, auto const& b, auto c) -> void {
    static_assert(std::is_same<B const&, decltype(b)>::value,"");
  };

  static_assert(std::is_same<int, decltype(a1(declval<value_arg<int const&>>()))>::value,"");
  //static_assert(std::is_same<int, decltype(a2(declval<value_arg<int>>()))>::value,"");  // Must not compile
  //static_assert(std::is_same<int, decltype(a2(declval<value_arg<int>>()))>::value,"");  // Must not compile

  
  static_assert(std::is_same<int, decltype(declval<decltype(f1)>()(declval<A>(),declval<value_arg<B>>(),declval<C>()))>::value,"");

  //static_assert(has_operator<decltype(fl1)>::value,"");
  //static_assert(has_operator<decltype(fg1)>::value,"");

  // Funtion alway takes by copy
  //static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(f1),A,B,C>::type>::value,"");
  //static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(f1),A,B&,C>::type>::value,"");
  //static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(f1),A,B const&,C>::type>::value,"");
//
  //// Function takes by ref if possible
  ////static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(f2),A,B,C>::type>::value,"");  // Must not compile
  //static_assert(std::is_same<tuple<A,B&,C>, typename extract_tuple_arguments<decltype(f2),A,B&,C>::type>::value,"");
  ////static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(f2),A,B const&,C>::type>::value,"");   // Must not compile
  //
//
  //// Takes by reference only
  //static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(f3),A,B,C>::type>::value,"");
  //static_assert(std::is_same<tuple<A,B&,C>, typename extract_tuple_arguments<decltype(f3),A,B&,C>::type>::value,"");
  //static_assert(std::is_same<tuple<A,B const&,C>, typename extract_tuple_arguments<decltype(f3),A,B const&,C>::type>::value,"");
  //
  //// Auto lambdas are considered as copy
//
  //// Funtion alway takes by copy
  //static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(fl1),A,B,C>::type>::value,"");
  //static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(fl1),A,B&,C>::type>::value,"");
  //static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(fl1),A,B const&,C>::type>::value,"");
//
  //// Function takes by ref if possible
  ////static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(f2),A,B,C>::type>::value,"");  // Must not compile
  //static_assert(std::is_same<tuple<A,B&,C>, typename extract_tuple_arguments<decltype(fl2),A,B&,C>::type>::value,"");
  ////static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(f2),A,B const&,C>::type>::value,"");   // Must not compile
  //
//
  //// Takes by reference only
  //static_assert(std::is_same<tuple<A,B,C>, typename extract_tuple_arguments<decltype(fl3),A,B,C>::type>::value,"");
  //static_assert(std::is_same<tuple<A,B&,C>, typename extract_tuple_arguments<decltype(fl3),A,B&,C>::type>::value,"");
  //static_assert(std::is_same<tuple<A,B const&,C>, typename extract_tuple_arguments<decltype(fl3),A,B const&,C>::type>::value,"");  // Not happy it compiles :(

  CHECK(true);
}
