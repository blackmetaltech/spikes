# libspike: NEST-embedded closed-loop engine.
#   spike::nest    NEST static libs + headers (third_party/nest-build, from scripts/build-nest.sh)
#   spike::engine  the engine (+ every .cpp in src/tasks, src/analysis and src/body, picked up by glob)
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
# GSL: linked statically by default (libgsl.a / libgslcblas.a from libgsl-dev) so the exported game does not need libgsl.so.27 on the player's machine.
option(SPIKE_STATIC_GSL "Link GSL statically (needs libgsl.a, built -fPIC as Debian/Ubuntu do)" ON)
if(SPIKE_STATIC_GSL)
  find_library(GSL_LIB NAMES libgsl.a REQUIRED)
  find_library(GSLCBLAS_LIB NAMES libgslcblas.a REQUIRED)
else()
  find_library(GSL_LIB gsl REQUIRED)
  find_library(GSLCBLAS_LIB gslcblas REQUIRED)
endif()
find_package(Threads REQUIRED)

add_library(spike_nest INTERFACE)
add_library(spike::nest ALIAS spike_nest)
target_include_directories(spike_nest SYSTEM INTERFACE
  ${NEST_SRC}/thirdparty ${NEST_SRC}/libnestutil ${NEST_BLD}/libnestutil
  ${NEST_SRC}/models ${NEST_SRC}/nestkernel ${NEST_SRC}/nestkernel/spatial ${NEST_BLD}/nestkernel)
# GNU ld needs the static archives grouped; Apple's linker resolves archives in any order and rejects --start-group
set(SPIKE_LD_GROUP_BEGIN "-Wl,--start-group")
set(SPIKE_LD_GROUP_END "-Wl,--end-group")
if(APPLE)
  set(SPIKE_LD_GROUP_BEGIN "")
  set(SPIKE_LD_GROUP_END "")
endif()
target_link_libraries(spike_nest INTERFACE
  ${SPIKE_LD_GROUP_BEGIN} ${NEST_LIBS} ${SPIKE_LD_GROUP_END} ${GSL_LIB} ${GSLCBLAS_LIB} Threads::Threads)

if(WIN32)
  # MinGW lays out bit-fields like MSVC by default; NEST (built with the same flag) and its headers need the GCC layout (SpikeData is exactly 8 bytes)
  target_compile_options(spike_nest INTERFACE -mno-ms-bitfields)
endif()

file(GLOB TASK_SRC CONFIGURE_DEPENDS src/tasks/*.cpp)
# Self-registering tasks: reference each task's anchor so the static library keeps its object file.
set(_anchors "")
set(_calls "")
foreach(f ${TASK_SRC})
  file(READ ${f} _txt)
  get_filename_component(_n ${f} NAME_WE)
  if(_txt MATCHES "SPIKE_REGISTER_TASK\\( *${_n} *," )
    get_filename_component(_stem ${f} NAME_WE)
    string(APPEND _anchors "extern \"C\" int spike_task_anchor_${_stem};\n")
    string(APPEND _calls " + spike_task_anchor_${_stem}")
  endif()
endforeach()
file(CONFIGURE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/task_anchors.inc CONTENT "${_anchors}static inline int spike_task_anchors() { return 0${_calls}; }\n")
file(GLOB ANALYSIS_SRC CONFIGURE_DEPENDS src/analysis/*.cpp)
file(GLOB BODY_SRC CONFIGURE_DEPENDS src/body/*.cpp)
file(GLOB CATALOG_SRC CONFIGURE_DEPENDS src/catalog/*.cpp)
list(FILTER CATALOG_SRC EXCLUDE REGEX "spike_catalog\\.cpp$")  # that one is the spike-catalog tool main()
# --- NESTML custom cell models (models/nestml -> scripts/build-nestml.sh -> third_party/nestml-gen) ---
# Generated C++ is compiled into libspike when present; otherwise src/modules/nestml_models.cpp is a stub.
set(NESTML_GEN "${CMAKE_SOURCE_DIR}/third_party/nestml-gen" CACHE PATH "NESTML-generated C++ (scripts/build-nestml.sh)")
file(GLOB NESTML_MODEL_SRC CONFIGURE_DEPENDS ${NESTML_GEN}/*.cpp)
file(GLOB NESTML_MODEL_DEFS CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/models/nestml/*.nestml)
if(NESTML_MODEL_SRC AND EXISTS "${NESTML_GEN}/nestml_register.inc")
  foreach(m ${NESTML_MODEL_DEFS})
    if(${m} IS_NEWER_THAN "${NESTML_GEN}/.stamp")
      message(WARNING "${m} is newer than the generated code: run scripts/build-nestml.sh")
    endif()
  endforeach()
  set(SPIKE_HAVE_NESTML TRUE)
else()
  message(STATUS "NESTML models not generated (scripts/build-nestml.sh): sa_node_pacemaker, butera_burster, icc_slow_wave unavailable")
endif()
# --- end NESTML ---
add_library(spike_engine STATIC src/circuit.cpp src/ring_backend.cpp src/engine.cpp src/modules/nestml_models.cpp ${TASK_SRC} ${ANALYSIS_SRC} ${BODY_SRC} ${CATALOG_SRC} ${NESTML_MODEL_SRC})
add_library(spike::engine ALIAS spike_engine)
set_target_properties(spike_engine PROPERTIES POSITION_INDEPENDENT_CODE ON OUTPUT_NAME spike)
target_include_directories(spike_engine PUBLIC include PRIVATE src ${CMAKE_CURRENT_BINARY_DIR}/gen)
target_include_directories(spike_engine SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party/json)
target_link_libraries(spike_engine PUBLIC spike::nest)
if(SPIKE_HAVE_NESTML)  # NESTML
  target_compile_definitions(spike_engine PRIVATE SPIKE_HAVE_NESTML=1)
  target_include_directories(spike_engine PRIVATE ${NESTML_GEN})
  set_source_files_properties(${NESTML_MODEL_SRC} PROPERTIES COMPILE_OPTIONS "-w")  # generated code
endif()
target_compile_options(spike_engine PRIVATE -Wall -Wextra -Wno-unused-parameter)

add_executable(spike-run src/spike_run.cpp)
target_link_libraries(spike-run PRIVATE spike::engine)
if(WIN32)
  # scripts/build-windows.sh builds with MinGW (UCRT64): one self-contained exe that needs only Windows system DLLs.
  target_link_options(spike-run PRIVATE -static -static-libgcc -static-libstdc++)
  # UTF-8 as the ANSI code page (Windows 10 1903+): argv, getenv and the std::ifstream paths of a user name with accents work as on Linux
  enable_language(RC)
  target_sources(spike-run PRIVATE windows/spike-run.rc)
  target_include_directories(spike-run PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/windows)  # windres finds the manifest there
endif()
# spike-run ships next to the extension: find sibling libraries, and carry its own C++ runtime
set_target_properties(spike-run PROPERTIES BUILD_RPATH "$ORIGIN" INSTALL_RPATH "$ORIGIN")
if(UNIX AND NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_options(spike-run PRIVATE -static-libstdc++ -static-libgcc)
endif()

# spike-catalog: measures every parts-bin card (f-I curve, V_m trace) -> catalog/thumbs.json
add_executable(spike-catalog src/catalog/spike_catalog.cpp)
target_link_libraries(spike-catalog PRIVATE spike::engine)

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
    if(name STREQUAL "test_catalog")  # runs the spike-catalog tool
      add_dependencies(${name} spike-catalog)
      target_compile_definitions(${name} PRIVATE SPIKE_CATALOG_EXE="$<TARGET_FILE:spike-catalog>")
    endif()
    if(name STREQUAL "tasks_cheap_test")  # one CTest entry per rows file (cheap_rows_{a,b,c,d,e,f,g,h}.inc) so none exceeds the timeout
      foreach(part a b c d e f g h)
        add_test(NAME ${name}_${part} COMMAND ${name} ${part} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(${name}_${part} PROPERTIES TIMEOUT 1500)
      endforeach()
      continue()
    endif()
    if(name STREQUAL "tasks_quick_full_test")  # the reference of every task, 3 modes x TEST seeds: four CTest entries (tasks k, k+4, ...), each forking its own jobs
      foreach(part 0 1 2 3)
        add_test(NAME ${name}_${part} COMMAND ${name} ${part} 4 WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(${name}_${part} PROPERTIES TIMEOUT 3000)
      endforeach()
      continue()
    endif()
    if(name STREQUAL "tasks_heldout_test")  # held-out-seed robustness of the two spinal levels that used to be marginal: one CTest entry per level
      foreach(task stretch_reflex boss_spinal_sprint)
        add_test(NAME ${name}_${task} COMMAND ${name} ${task} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
        set_tests_properties(${name}_${task} PROPERTIES TIMEOUT 3000)
      endforeach()
      continue()
    endif()
    add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
    set_tests_properties(${name} PROPERTIES TIMEOUT 1500)
    if(name MATCHES "^test_nestml_")  # skipped (exit 77) when the NESTML models were not generated
      set_tests_properties(${name} PROPERTIES SKIP_RETURN_CODE 77)
    endif()
  endforeach()
  # hostile-circuit fuzz corpus (engine/tests/fuzz): every case must end with exit status 0 or 1 inside a time and memory limit
  add_test(NAME fuzz_circuits COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/run_fuzz.py $<TARGET_FILE:spike-run>)
  set_tests_properties(fuzz_circuits PROPERTIES TIMEOUT 900)
  add_test(NAME evidence_circuits COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/tests/fuzz/run_evidence.py $<TARGET_FILE:spike-run> ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_tests_properties(evidence_circuits PROPERTIES TIMEOUT 600)  # the script judges CPU time, so load does not fail it
  # NESTML: engine vs PyNEST spike parity (skips when PyNEST or scripts/build-nestml.sh --pynest output is missing)
  if(SPIKE_HAVE_NESTML)
    add_test(NAME test_nestml_parity COMMAND python3 ${CMAKE_SOURCE_DIR}/models/nestml/test_parity.py $<TARGET_FILE:spike-run>)
    set_tests_properties(test_nestml_parity PROPERTIES TIMEOUT 1500 SKIP_RETURN_CODE 77)
  endif()
endif()

# install: `cmake --install build --component game --prefix <dir>` puts spike-run where the extension lives (<dir>/bin)
install(TARGETS spike-run RUNTIME DESTINATION bin COMPONENT game)
