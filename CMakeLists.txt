cmake_minimum_required(VERSION 3.24)
project(spike LANGUAGES C CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(SPIKE_BUILD_TESTS "Build engine tests and benchmark" ON)
option(SPIKE_BUILD_GDEXT "Build the GDExtension if gdext/CMakeLists.txt exists" ON)

enable_testing()

# NEST (built by scripts/build-nest.sh) + libspike, with spike::engine, spike::nest and the spike-run CLI.
add_subdirectory(engine)

# Optional parts owned by other agents: they plug in with their own CMakeLists.txt and link spike::engine.
if(SPIKE_BUILD_GDEXT AND EXISTS "${CMAKE_SOURCE_DIR}/gdext/CMakeLists.txt")
  add_subdirectory(gdext)
endif()
# The UI interaction graph (docs/INTERACTION-GRAPH.md): the JSON and its generated diagram are checked here; the headless Godot walk skips (77) when godot or the extension is missing.
if(SPIKE_BUILD_TESTS)
  add_test(NAME interaction_graph_static COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/graph/render.py --check WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  add_test(NAME ui_graph COMMAND ${CMAKE_SOURCE_DIR}/scripts/ui_graph_test.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(ui_graph PROPERTIES TIMEOUT 1800 SKIP_RETURN_CODE 77)
  # The save layer (godot/tests/test_save.gd): damaged / future / read-only saves, hostile circuits, the Reference and reference-lock repros. Needs only godot.
  add_test(NAME save_io COMMAND ${CMAKE_SOURCE_DIR}/scripts/save_test.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(save_io PROPERTIES TIMEOUT 600 SKIP_RETURN_CODE 77)
  add_test(NAME godot_polish COMMAND ${CMAKE_SOURCE_DIR}/scripts/polish_test.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(godot_polish PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
endif()
foreach(d python godot)
  if(EXISTS "${CMAKE_SOURCE_DIR}/${d}/CMakeLists.txt")
    add_subdirectory(${d})
  endif()
endforeach()
