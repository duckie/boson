set(headers
wfqueue/wfqueue.h
wfqueue/primitives.h
wfqueue/align.h
)

add_library(wfqueue
wfqueue/wfqueue.c
${headers}
)

target_include_directories(wfqueue PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(wfqueue PUBLIC -DWFQUEUE)
set_target_properties(wfqueue PROPERTIES PUBLIC_HEADER "${headers}")
