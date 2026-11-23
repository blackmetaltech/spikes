cmake_minimum_required(VERSION 3.24)
project(spike LANGUAGES C CXX VERSION 0.5.2)

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
  # The circuit editor (godot/tests/test_editor.gd): undo / redo, large-circuit layout, clipboard, keyboard cursor. Needs only godot.
  add_test(NAME godot_editor COMMAND ${CMAKE_SOURCE_DIR}/scripts/editor_test.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(godot_editor PROPERTIES TIMEOUT 400 SKIP_RETURN_CODE 77)
  # Plate 0's first-run logic (godot/tests/test_tutorial.gd): fresh save starts at step 1, skipped / finished never restarts. Needs only godot.
  add_test(NAME tutorial COMMAND ${CMAKE_SOURCE_DIR}/scripts/tutorial_test.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(tutorial PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
  # The Analysis screen's wiring matrix, path finder and null-model bench (godot/tests/test_wiring.gd). Needs godot and the extension.
  add_test(NAME godot_wiring COMMAND ${CMAKE_SOURCE_DIR}/scripts/wiring_test.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(godot_wiring PROPERTIES TIMEOUT 700 SKIP_RETURN_CODE 77)
  # The Atlas worm chapter's game side (godot/tests/test_atlas.gd): wing gating, glossary, silencing control, bet panel, bets through the engine. Needs godot and the extension.
  add_test(NAME godot_atlas COMMAND ${CMAKE_SOURCE_DIR}/scripts/atlas_test.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(godot_atlas PROPERTIES TIMEOUT 1000 SKIP_RETURN_CODE 77)
  add_test(NAME godot_polish COMMAND ${CMAKE_SOURCE_DIR}/scripts/polish_test.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(godot_polish PROPERTIES TIMEOUT 300 SKIP_RETURN_CODE 77)
  # The lab's specimen layers (godot/assets/specimens) against the plate drawings they are cut from; skips (77) without pycairo.
  add_test(NAME art_specimens COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/art/check_specimens.py WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(art_specimens PROPERTIES SKIP_RETURN_CODE 77)
  # These all open the one Godot project in godot/ (import cache, export); run side by side they trip over each other, so ctest -j takes them in turn.
  set_tests_properties(ui_graph save_io godot_editor tutorial godot_polish godot_wiring godot_atlas PROPERTIES RESOURCE_LOCK godot_project)
endif()
foreach(d python godot)
  if(EXISTS "${CMAKE_SOURCE_DIR}/${d}/CMakeLists.txt")
    add_subdirectory(${d})
  endif()
endforeach()
