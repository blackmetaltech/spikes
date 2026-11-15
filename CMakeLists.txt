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
foreach(d python godot)
  if(EXISTS "${CMAKE_SOURCE_DIR}/${d}/CMakeLists.txt")
    add_subdirectory(${d})
  endif()
endforeach()
