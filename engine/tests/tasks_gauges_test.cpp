// Every gauge a task names (Task::bands()) must be fed: after the reference circuit has run for a while, body().readouts holds a value under that name. A gauge with no
// readout would draw 0.0 in green (the playtest found leech_heart's period and overlap, and the fly gauges, in that state); the game shows "—" for a readout that is missing, and
// this test makes sure every real task supplies its own.
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

int main()
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  int checked = 0;
  for ( const auto& id : task_ids() )
  {
    auto t = make_task( id );
    if ( !t || t->bands().empty() ) continue;
    if ( t->level_kind() == "diagnosis" ) continue;  // a patient circuit is broken on purpose: it may have no rhythm to read a period from, and the game shows a dash then
    const std::string path = std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + reference_circuit( id );
    const std::string txt = chk::slurp( path );
    if ( txt.empty() ) continue;  // a task without a reference circuit file has no gauges to feed here
    Engine e;
    json c = json::parse( txt );
    if ( !e.load( c.dump() ) ) continue;
    t->set_quick( true );
    t->reset( 1 );
    const long steps = static_cast< long >( t->quick_duration_ms() );
    for ( long s = 0; s < steps; ++s ) t->step( e, 1.0 );
    const auto ro = t->body().readouts;
    for ( const auto& b : t->bands() )
    {
      ++checked;
      CHECK_MSG( ro.count( b.name ) > 0, id + ": the gauge '" + b.name + "' has no readout (body().readouts has no such name)" );
    }
  }
  std::printf( "%d gauges checked\n", checked );
  return chk::finish( "tasks_gauges_test" );
}
