#include "boson/memory/local_ptr.h"
#include "catch.hpp"

using namespace boson::memory;

namespace {
struct A {
  static int instances(int offset = 0) {
    static int nb_instances = 0;
    return (nb_instances = nb_instances + offset);
  }
  
  A() {
    instances(1);
  }

  ~A() {
    instances(-1);
  }

};
}

TEST_CASE("Local pointer","[local_ptr]") {
  CHECK(A::instances() == 0);
  {
    local_ptr<A> a1(new A);
    CHECK(A::instances() == 1);
    CHECK(a1);
    local_ptr<A> a2 = a1;
    CHECK(a2);
    CHECK(A::instances() == 1);
  }
  CHECK(A::instances() == 0);
  {
    local_ptr<A> a1(new A);
    CHECK(A::instances() == 1);
    CHECK(a1);
    local_ptr<A> a2 = a1;
    CHECK(A::instances() == 1);
    CHECK(a2);
    
    a2.invalidate_all();
    CHECK(!a1);
    CHECK(!a2);
    CHECK(A::instances() == 0);
  }
}
