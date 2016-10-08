set(headers
#wfqueue/wfqueue.h
#wfqueue/lcrq.h
#wfqueue/ccqueue.h
#wfqueue/primitives.h
#wfqueue/align.h
wfqueue/queue.h
)

add_library(wfqueue
wfqueue/wfqueue.c
#wfqueue/lcrq.c
#wfqueue/ccqueue.c
#wfqueue/hzdptr.c
#wfqueue/xxhash.c
${headers}
)

target_include_directories(wfqueue PUBLIC SYSTEM ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(wfqueue PUBLIC -DWFQUEUE)
#target_compile_definitions(wfqueue PUBLIC -DLCRQ)
#target_compile_definitions(wfqueue PUBLIC -DCCQUEUE)
set_target_properties(wfqueue PROPERTIES PUBLIC_HEADER "${headers}")
