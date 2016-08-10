# This file provides function to add tests tp ctest
# with additional tooling

##
# project_add_test has the same effect as add_test, but with additional effects :
#  - It add a prefix before COMMAND if you want one (ex:run valgrind or perf)
#      Use the PROJECT_TEST_COMMANDS_PREFIX to add a prefix. PROJECT_TEST_COMMANDS_PREFIX will be parsed
#      in such a way that it can be a full command with quoted arguments
#
# It supports both complete syntaxes of add_test transparently
#
separate_arguments(PARSED_PROJECT_TEST_COMMANDS_PREFIX UNIX_COMMAND ${PROJECT_TEST_COMMANDS_PREFIX})
function(project_add_test test_name)
  # Parse command line arguments to support various add_test syntaxes 
  set(options OPTIONAL FAST)
  set(oneValueArgs NAME WORKING_DIRECTORY)
  set(multiValueArgs COMMAND CONFIGURATIONS)
  cmake_parse_arguments(project_add_test "${options}" "${oneValueArgs}" "${multiValueArgs}" ${test_name} ${ARGN})

  if ("${project_add_test_NAME}" STREQUAL "")
    # This is the short definition
    add_test(${test_name} ${PARSED_PROJECT_TEST_COMMANDS_PREFIX} ${ARGN})
  else()
    # Classic standard definitiom
    add_test(NAME ${project_add_test_NAME} 
      COMMAND ${PARSED_PROJECT_TEST_COMMANDS_PREFIX} ${project_add_test_COMMAND}
      CONFIGURATIONS ${project_add_test_CONFIGURATIONS}
      WORKING_DIRECTORY ${project_add_test_WORKING_DIRECTORY}
      )
  endif()
endfunction(project_add_test)
