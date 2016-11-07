# Boost support
set(LIBRARIES)
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang" OR ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-pragmas")
endif()

file(GLOB srcs *.cc)
list(REMOVE_ITEM srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc")

macro(add_example NAME)
    add_executable(${NAME} src/${NAME}.cc)
    target_link_libraries(${NAME} boson ${LIBRARIES})
endmacro()

add_example(basic_loop)
add_example(timed_loop)
add_example(rw_loop)
add_example(mutex_loop)
add_example(channel_loop)
add_example(channel_loop2)
add_example(socket_server)
add_example(chat_server)

add_example(readme1)
add_example(readme2)
add_example(readme3)
