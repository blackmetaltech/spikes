// Internal task registration. Each task .cpp registers itself:
//   SPIKE_REGISTER_TASK( reflex, 10, "reflex_arc.json", make_reflex )
// `order` sorts the campaign (10s invertebrates, 100s spinal, 200s heart, 300s breathing, 400s autonomic).
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "spike/task.h"

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
