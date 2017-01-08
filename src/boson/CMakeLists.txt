set(LIBRARIES fmt wfqueue)

Find_Package(Threads REQUIRED)
set(LIBRARIES "${LIBRARIES}" ${CMAKE_THREAD_LIBS_INIT})

# TODO: Get platform agnostic
add_library(boson_fcontext OBJECT
  src/asm/make_x86_64_sysv_elf_gas.S 
  src/asm/jump_x86_64_sysv_elf_gas.S
  #src/asm/ontop_x86_64_sysv_elf_gas.S # Not used actually
  )

# Remove some specific warnings
# - About unknown pragmas
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pragmas")
endif()

# Import configuration options into code
file(GLOB lib_sources 
  src/*.cc 
  src/internal/*.cc 
  src/queues/simple.cc 
  src/net/*.cc
  )
file(GLOB lib_linux_sources src/linux/*.cc)
set(lib_sources ${lib_sources} ${lib_linux_sources})
set(lib_queues_sources src/queues/lcrq.cc)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/boson ${CMAKE_CURRENT_SOURCE_DIR}/src/linux)

# Boson lib
add_library(boson ${lib_sources} $<TARGET_OBJECTS:boson_fcontext>)
target_compile_options(boson PUBLIC -nostdlib)
target_link_libraries(boson ${LIBRARIES} -lpthread)
target_include_directories(boson PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

# Boson queue lib
add_library(bosonqueues ${lib_queues_sources})
target_compile_options(bosonqueues PUBLIC -nostdlib)
target_link_libraries(bosonqueues ${LIBRARIES})
target_include_directories(bosonqueues PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(test)
