file(GLOB lib_sources src/*.cc)
add_library(testlib ${lib_sources})
target_link_libraries(testlib catch)
