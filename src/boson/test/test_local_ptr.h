#include "boson/memory/local_ptr.h"
#include "catch.hpp"

using namespace boson::memory;

namespace {
struct A {
  int i,j;
};
}

TEST_CASE("Local pointer - Simple case","[local_ptr]") {
  local_ptr<A>(new A{1,1});
}
