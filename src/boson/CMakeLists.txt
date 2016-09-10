set(LIBRARIES json_backbone)

Find_Package(Threads REQUIRED)
set(LIBRARIES "${LIBRARIES}" ${CMAKE_THREAD_LIBS_INIT})

# TODO: Get platform agnostic
add_library(boson_fcontext OBJECT
  src/asm/make_x86_64_sysv_elf_gas.S 
  src/asm/jump_x86_64_sysv_elf_gas.S
  #src/asm/ontop_x86_64_sysv_elf_gas.S # Not used actually
  )

# Import configuration options into code
file(GLOB lib_sources src/*.cc src/internal/*.cc)
file(GLOB lib_linux_sources src/linux/*.cc)
set(lib_sources ${lib_sources} ${lib_linux_sources})

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/boson ${CMAKE_CURRENT_SOURCE_DIR}/src/linux)
add_library(boson ${lib_sources} $<TARGET_OBJECTS:boson_fcontext>)
target_compile_options(boson PUBLIC -nostdlib)
target_link_libraries(boson ${LIBRARIES})
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(test)
