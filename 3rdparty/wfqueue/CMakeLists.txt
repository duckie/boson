set(headers
wfqueue/primitives.h
wfqueue/align.h
)

add_library(wfqueue
wfqueue/xxhash.c
${headers}
)

target_include_directories(wfqueue PUBLIC SYSTEM ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(wfqueue PROPERTIES PUBLIC_HEADER "${headers}")
