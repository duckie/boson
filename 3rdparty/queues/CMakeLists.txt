set(headers
queues/include/mpmc-bounded-queue.hpp
queues/include/mpsc-queue.hpp
queues/include/spsc-bounded-queue.hpp
queues/include/spsc-queue.hpp
)

add_library(queues INTERFACE)
target_include_directories(queues INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/queues/include)
#set_target_properties(queues PROPERTIES HEADER "${headers}")
