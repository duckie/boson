#include "catch.hpp"
#include "projectlib/project.h"

TEST_CASE("Project", "[project]") {
  SECTION("section") {
    REQUIRE(project::api() == true);
  }
}
