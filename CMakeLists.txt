
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
if (NOT CPP14_SUPPORT)
  message(FATAL_ERROR "C++14 support is required.")
endif()

# Check OS
if (WIN32)
  add_definitions(-DWINDOWS)
else()
endif()

# Trace compilation for tools (ex: YouCompleteMe)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
configure_file(${PROJECT_SOURCE_DIR}/cmake/BuildConfig.json.in ${PROJECT_SOURCE_DIR}/BuildConfig.json)

# Tooling - some must be declared before adding other subdirectories
project_enable_coverage_build()
project_enable_sanitizer_build()
project_enable_clang_format()
project_enable_documentation()

# Generate configuration

# By default we put binaries into this directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

### MANUAL_INCLUDE_DIRS is useful to force ides to find some includes (useless for compilation) ###
### To be used by editing the cache manually
include_directories(${MANUAL_INCLUDE_DIRS})

project_add_3rdparty(catch)
project_add_3rdparty(wfqueue)
project_add_3rdparty(queues)
project_add_3rdparty(fmt)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
endif()

if (BOSON_USE_VALGRIND)
  add_definitions(-DBOSON_USE_VALGRIND)
endif()

project_add_module(test)
project_add_module(boson)
project_add_module(examples)
