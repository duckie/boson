#include "catch.hpp"
#include "boson/exception.h"
#include <cstdio>
#include <iostream>
#include <unistd.h>

using namespace boson;
using namespace std::literals;

TEST_CASE("Exception class coverage", "[coverage]") {
  try {
    throw boson::exception(boson::exception("boson error"));
  }
  catch(boson::exception error) {
    CHECK(std::string(error.what()) == "boson error");
  }

  boson::exception e1("Roger");
  boson::exception e2("Marcel");
  boson::exception e3("Robert");
  e1 = e2;
  e2 = std::move(e3);

  boson::exception e4(e1);
  std::string test("Roberte");
  boson::exception e5(test);

  // Cover dynamic dtor
  std::unique_ptr<std::exception> pointer(new  boson::exception(""));
}
