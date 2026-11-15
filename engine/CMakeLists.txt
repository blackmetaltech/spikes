# libspike: NEST-embedded closed-loop engine.
#   spike::nest    NEST static libs + headers (third_party/nest-build, from scripts/build-nest.sh)
#   spike::engine  the engine (+ every .cpp in src/tasks and src/analysis, picked up by glob)
#   spike-run      CLI: spike-run <circuit.json> <ms> [--threads n]
set(NEST_SRC "${CMAKE_SOURCE_DIR}/third_party/nest-simulator")
set(NEST_BLD "${CMAKE_SOURCE_DIR}/third_party/nest-build")

set(NEST_LIBS "")
foreach(l models/libmodels.a nestkernel/libnestkernel.a libnestutil/libnestutil.a)
  if(NOT EXISTS "${NEST_BLD}/${l}")
    message(FATAL_ERROR "NEST static library missing: ${NEST_BLD}/${l}. Run scripts/build-nest.sh first.")
  endif()
  list(APPEND NEST_LIBS "${NEST_BLD}/${l}")
endforeach()
find_library(GSL_LIB gsl REQUIRED)
find_library(GSLCBLAS_LIB gslcblas REQUIRED)
find_package(Threads REQUIRED)

add_library(spike_nest INTERFACE)
add_library(spike::nest ALIAS spike_nest)
target_include_directories(spike_nest SYSTEM INTERFACE
  ${NEST_SRC}/thirdparty ${NEST_SRC}/libnestutil ${NEST_BLD}/libnestutil
  ${NEST_SRC}/models ${NEST_SRC}/nestkernel ${NEST_SRC}/nestkernel/spatial ${NEST_BLD}/nestkernel)
target_link_libraries(spike_nest INTERFACE
  -Wl,--start-group ${NEST_LIBS} -Wl,--end-group ${GSL_LIB} ${GSLCBLAS_LIB} Threads::Threads)

file(GLOB TASK_SRC CONFIGURE_DEPENDS src/tasks/*.cpp)
file(GLOB ANALYSIS_SRC CONFIGURE_DEPENDS src/analysis/*.cpp)
add_library(spike_engine STATIC src/circuit.cpp src/ring_backend.cpp src/engine.cpp ${TASK_SRC} ${ANALYSIS_SRC})
add_library(spike::engine ALIAS spike_engine)
set_target_properties(spike_engine PROPERTIES POSITION_INDEPENDENT_CODE ON OUTPUT_NAME spike)
target_include_directories(spike_engine PUBLIC include PRIVATE src)
target_include_directories(spike_engine SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party/json)
target_link_libraries(spike_engine PUBLIC spike::nest)
target_compile_options(spike_engine PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_executable(spike-run src/spike_run.cpp)
target_link_libraries(spike-run PRIVATE spike::engine)

if(SPIKE_BUILD_TESTS)
  add_executable(feasibility_loop tests/feasibility_loop.cpp)
  target_link_libraries(feasibility_loop PRIVATE spike::nest)
  # every other engine/tests/*.cpp (including tasks_*.cpp, analysis_*.cpp from other agents) is one CTest test
  file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
  foreach(src ${TEST_SRC})
    get_filename_component(name ${src} NAME_WE)
    if(name STREQUAL "feasibility_loop")
      continue()
    endif()
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE spike::engine)
    target_include_directories(${name} PRIVATE tests)
    target_include_directories(${name} SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/json)
    target_compile_definitions(${name} PRIVATE
      SPIKE_SOURCE_DIR="${CMAKE_SOURCE_DIR}" SPIKE_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/golden"
      SPIKE_RUN_EXE="$<TARGET_FILE:spike-run>")
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(${name} PROPERTIES TIMEOUT 600)
  endforeach()
endif()
