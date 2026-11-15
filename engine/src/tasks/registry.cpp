// Task registry: task_ids / make_task / reference_circuit.
#include "spike/task.h"
#include "tasks.h"

namespace spike
{

std::vector< std::string > task_ids() { return { "reflex", "lamprey", "worm" }; }

std::unique_ptr< Task > make_task( const std::string& id )
{
  if ( id == "reflex" )
    return tasks::make_reflex();
  if ( id == "lamprey" )
    return tasks::make_lamprey();
  if ( id == "worm" )
    return tasks::make_worm();
  return nullptr;
}

std::string reference_circuit( const std::string& id )
{
  if ( id == "reflex" )
    return "reflex_arc.json";
  if ( id == "lamprey" )
    return "lamprey_cpg.json";
  if ( id == "worm" )
    return "worm_chemotaxis.json";
  return "";
}

} // namespace spike
