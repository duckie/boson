set(catch_exe_source_list)
add_custom_target(build-test)
macro(add_project_test name)
  if("${ARGV1}" STREQUAL "CATCH")
    list(APPEND catch_exe_source_list ${name}.cc)
  else()
    add_executable(${name} ${name}.cc)
    target_link_libraries(${name} ${PROJECT_NAME} ${CMAKE_THREAD_LIBS_INIT})
    add_dependencies(build-test ${name})
    add_test(${name} ${name})  
  endif()
endmacro()

# Reference test sources
add_project_test(test1 CATCH)

# Create main test executable
add_executable(unit_tests ${catch_exe_source_list})
target_link_libraries(unit_tests ${PROJECT_NAME} testlib catch)
add_dependencies(build-test unit_tests)
project_add_test(lib${PROJECT_NAME}_unit_tests ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/unit_tests)
