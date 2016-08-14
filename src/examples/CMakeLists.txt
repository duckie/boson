# Boost support
set(LIBRARIES)
find_package(Boost COMPONENTS context REQUIRED)
if(Boost_FOUND)
 include_directories(${Boost_INCLUDE_DIRS})
    if(CMAKE_COMPILER_IS_GNUCXX)
        set(LIBRARIES "${LIBRARIES}" "${Boost_LIBRARIES}")
    else()
        link_directories(${Boost_LIBRARY_DIRS})
    endif()
endif()

file(GLOB srcs *.cc)
list(REMOVE_ITEM srcs "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cc")

add_executable(run${PROJECT_NAME} src/main.cc)
target_link_libraries(run${PROJECT_NAME} boson ${LIBRARIES})

add_executable(basic_loop src/basic_loop.cc)
target_link_libraries(basic_loop boson ${LIBRARIES})
