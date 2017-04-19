#include "catch.hpp"
#include "boson/boson.h"
#include "boson/logger.h"
#include <iostream>

TEST_CASE("Logger coverage", "[coverage]") {
  boson::debug::logger_instance(&std::cout);
  boson::debug::log("{}","");
  boson::debug::log("Logging useless data {}",0);
}
