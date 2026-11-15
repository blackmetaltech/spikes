# spike-gd: the Godot GDExtension (godot-cpp) exposing spike::engine as SpikeSim and spike::Task as SpikeTask.
# Added by the root CMakeLists (SPIKE_BUILD_GDEXT). godot-cpp comes from third_party/godot-cpp (scripts/build-gd.sh clones it);
# without it this directory is skipped with a note, so the engine build never depends on Godot.
if(NOT TARGET spike::engine)
  message(FATAL_ERROR "gdext needs spike::engine; add it through the repo root CMakeLists.txt")
endif()
set(SPIKE_GD_DIR ${CMAKE_CURRENT_LIST_DIR})
set(GODOT_CPP_DIR ${SPIKE_GD_DIR}/../third_party/godot-cpp)
if(NOT EXISTS ${GODOT_CPP_DIR}/CMakeLists.txt)
  message(STATUS "gdext skipped: third_party/godot-cpp missing (run scripts/build-gd.sh to clone it)")
  return()
endif()
if(NOT TARGET godot-cpp)
  add_subdirectory(${GODOT_CPP_DIR} ${CMAKE_CURRENT_BINARY_DIR}/godot-cpp)
endif()

# exception-using helpers (analysis) live in their own static target: godot-cpp compiles with -fno-exceptions
add_library(spike_gd_bridge STATIC ${SPIKE_GD_DIR}/src/bridge.cpp)
target_compile_features(spike_gd_bridge PRIVATE cxx_std_20)
set_target_properties(spike_gd_bridge PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_link_libraries(spike_gd_bridge PUBLIC spike::engine)

add_library(spike_gd SHARED ${SPIKE_GD_DIR}/src/register_types.cpp ${SPIKE_GD_DIR}/src/spike_sim.cpp ${SPIKE_GD_DIR}/src/spike_task.cpp)
target_compile_features(spike_gd PRIVATE cxx_std_20)
target_include_directories(spike_gd SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/json)
target_link_libraries(spike_gd PRIVATE godot::cpp spike_gd_bridge spike::engine)
set_target_properties(spike_gd PROPERTIES
  OUTPUT_NAME "spike_gd"
  POSITION_INDEPENDENT_CODE ON
  LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/gd-out)
