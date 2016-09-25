set(headers
fmt/format.h
fmt/ostream.h
fmt/posix.h
fmt/time.h
)

add_library(fmt
fmt/format.cc
fmt/ostream.cc
fmt/posix.cc
${headers}
)

target_include_directories(fmt PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(fmt PROPERTIES PUBLIC_HEADER "${headers}")
