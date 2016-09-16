set(headers
double-conversion/bignum.h
double-conversion/cached-powers.h
double-conversion/diy-fp.h
double-conversion/double-conversion.h
double-conversion/fast-dtoa.h
double-conversion/fixed-dtoa.h
double-conversion/ieee.h
double-conversion/strtod.h
double-conversion/utils.h
)

add_library(double-conversion
double-conversion/bignum.cc
double-conversion/bignum-dtoa.cc
double-conversion/cached-powers.cc
double-conversion/diy-fp.cc
double-conversion/double-conversion.cc
double-conversion/fast-dtoa.cc
double-conversion/fixed-dtoa.cc
double-conversion/strtod.cc
${headers}
)

target_include_directories(double-conversion PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Add fPIC on x86_64 when supported.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-fPIC CXX_HAS_FPIC)
if(CXX_HAS_FPIC AND CMAKE_SYSTEM_PROCESSOER STREQUAL "x86_64")
  set_target_properties(double-conversion PROPERTIES COMPILE_FLAGS "-fPIC")
endif()
set_target_properties(double-conversion PROPERTIES PUBLIC_HEADER "${headers}")
