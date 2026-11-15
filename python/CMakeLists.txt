# pytest as a CTest test. The C++ binaries are passed in so the parity and C++-vs-Python motif tests don't skip.
find_package(Python3 COMPONENTS Interpreter)
if(Python3_FOUND)
  add_test(NAME python_pytest COMMAND ${Python3_EXECUTABLE} -m pytest -q ${CMAKE_CURRENT_SOURCE_DIR}/tests
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  set_tests_properties(python_pytest PROPERTIES TIMEOUT 600
    ENVIRONMENT "SPIKE_RUN=$<TARGET_FILE:spike-run>;SPIKE_ANALYSIS_TEST=$<TARGET_FILE:analysis_test>")
endif()
