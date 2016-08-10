file(GLOB srcs *.cc)
list(REMOVE_ITEM srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc")

add_executable(run${PROJECT_NAME} src/main.cc ${srcs})
target_link_libraries(run${PROJECT_NAME} ${PROJECT_NAME})
