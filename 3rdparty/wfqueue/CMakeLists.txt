set(headers
wfqueue/wfqueue.h
wfqueue/lcrq.h
wfqueue/primitives.h
wfqueue/align.h
wfqueue/queue.h
)

add_library(wfqueue
#wfqueue/wfqueue.c
wfqueue/hzdptr.c
wfqueue/lcrq.c
wfqueue/xxhash.c
${headers}
)

target_include_directories(wfqueue PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
#target_compile_definitions(wfqueue PUBLIC -DWFQUEUE)
target_compile_definitions(wfqueue PUBLIC -DLCRQ)
set_target_properties(wfqueue PROPERTIES PUBLIC_HEADER "${headers}")
