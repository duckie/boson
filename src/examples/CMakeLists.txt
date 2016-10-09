# Boost support
set(LIBRARIES)
#find_package(Boost COMPONENTS context REQUIRED)
#if(Boost_FOUND)
 #include_directories(${Boost_INCLUDE_DIRS})
    #if(CMAKE_COMPILER_IS_GNUCXX)
        #set(LIBRARIES "${LIBRARIES}" "${Boost_LIBRARIES}")
    #else()
        #link_directories(${Boost_LIBRARY_DIRS})
    #endif()
#endif()

file(GLOB srcs *.cc)
list(REMOVE_ITEM srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc")

add_executable(run${PROJECT_NAME} src/main.cc)
target_link_libraries(run${PROJECT_NAME} boson ${LIBRARIES})

add_executable(basic_loop src/basic_loop.cc)
target_link_libraries(basic_loop boson ${LIBRARIES})

add_executable(timed_loop src/timed_loop.cc)
target_link_libraries(timed_loop boson ${LIBRARIES})

add_executable(rw_loop src/rw_loop.cc)
target_link_libraries(rw_loop boson ${LIBRARIES})

add_executable(mutex_loop src/mutex_loop.cc)
target_link_libraries(mutex_loop boson ${LIBRARIES})

add_executable(queue_loop src/queue_loop.cc)
target_link_libraries(queue_loop boson ${LIBRARIES})

add_executable(channel_loop src/channel_loop.cc)
target_link_libraries(channel_loop boson ${LIBRARIES})

add_executable(channel_loop2 src/channel_loop2.cc)
target_link_libraries(channel_loop2 boson ${LIBRARIES})

add_executable(socket_server src/socket_server.cc)
target_link_libraries(socket_server boson ${LIBRARIES})
