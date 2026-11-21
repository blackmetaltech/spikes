// Internal task registration. Each task .cpp registers itself:
//   SPIKE_REGISTER_TASK( reflex, 10, "reflex_arc.json", make_reflex )
// `order` sorts the campaign (10s invertebrates, 100s spinal, 200s heart, 300s breathing, 400s autonomic).
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "spike/task.h"

// ---- honesty contract in the task layer (add-only notes) -------------------------------------------------------------------------------
// make_task() (registry.cpp) wraps every task in a guard: a circuit may feed a level only through the level's own sensor_channels(); any other input
// channel (a recorded "spike_generator" train, an extra generator) fails the run (value capped, verdict says the circuit brings its own stimulus, part
// "circuit_rejected" = 1); an unknown mode or challenge variant fails the run (part "bad_request"); a run that did not pass never reads 100.
// task_util.h: weight_floor() / functional(): a projection (or a sensor, Wiring) weaker than the functional floor of its target's model is token wiring and
// is not a connection in ANY graph helper (micro::Graph, mech::, mechb::, tl::CellGraph). owned_level(): disturbances the task owns are scaled by the circuit's
// input weight so that turning the weight down does not remove them. Scripted tasks overwrite what the circuit file says about their stimulus with
// Spec::stimuli (Engine::set_input_param on a tuned_poisson input). SourceWatch: "does the group that should do the work ever fire?", for verdicts that say
// "strengthen X" (they say, in words, that X's input never fires, before telling the player to change a weight).
// Task::Diagnosis (include/spike/task.h, dx_common.h): candidates come in a stable hash order of (case, candidate) and the answer is never first;
// Diagnosis::probes lists the one or two probes (scope a population / cut a lesion) that tell the top candidates apart: evidence for the game to CHECK before
// the third star, never to display.

namespace spike::tasks
{
struct Entry
{
  std::string id;
  int order;
  std::string circuit;
  std::function< std::unique_ptr< Task >() > make;
};
std::vector< Entry >& registry();
struct Registrar
{
  explicit Registrar( Entry e );
};
std::unique_ptr< Task > make_reflex();
std::unique_ptr< Task > make_lamprey();
std::unique_ptr< Task > make_worm();
} // namespace spike::tasks

// ID must equal the .cpp file's stem (CMake generates a reference to spike_task_anchor_<stem> so the static
// library keeps the object file and its registrar).
#define SPIKE_REGISTER_TASK( ID, ORDER, CIRCUIT, FACTORY ) \
  static ::spike::tasks::Registrar spike_task_reg_##ID( { #ID, ORDER, CIRCUIT, [] { return FACTORY(); } } ); \
  extern "C" int spike_task_anchor_##ID; \
  int spike_task_anchor_##ID = 0;
