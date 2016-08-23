# C++ Project template
#
# Proudly bootstrapped by git@github.com:duckie/cpp_project_template.git
#

# Version guard
cmake_minimum_required (VERSION 3.2)

# Cmake modules
include(CheckCXXSourceCompiles)
include(CheckCXXCompilerFlag)
include(ExternalProject)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")
include(ProjectGlobalMacros)
include(ProjectTestFunctions)

# Project configuration
project (boson C CXX ASM)
enable_testing()
set(CMAKE_VERBOSE_MAKEFILE OFF)

# Check for C++11/14 support
project_check_cpp_version()

## Snippets to enforce a C++ version
#if (not CPP14_SUPPORT)
#  message(FATAL_ERROR "C++14 support is required.")
#endif()
if (NOT CPP11_SUPPORT)
  message(FATAL_ERROR "C++11 support is required.")
endif()

# Trace compilation for tools (ex: YouCompleteMe)
set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
configure_file(${PROJECT_SOURCE_DIR}/cmake/BuildConfig.json.in ${PROJECT_SOURCE_DIR}/BuildConfig.json)

# Tooling - some must be declared before adding other subdirectories
project_enable_coverage_build()
project_enable_sanitizer_build()
project_enable_clang_format()
project_enable_documentation()

# Generate configuration


### MANUAL_INCLUDE_DIRS is useful to force ides to find some includes (useless for compilation) ###
### To be used by editing the cache manually
include_directories(${MANUAL_INCLUDE_DIRS})

project_add_3rdparty(catch)
project_add_3rdparty(json_backbone)

# By default we put binaries into this directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

if (BOSON_USE_VALGRIND)
  add_definitions(-DBOSON_USE_VALGRIND)
endif()

project_add_module(test)
project_add_module(boson)
project_add_module(examples)
