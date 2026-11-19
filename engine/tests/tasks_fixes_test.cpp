// Playtest fixes (research/PLAYTEST-0.5.md): circuits a novice builds from the briefs that the structure checks used to reject, and verdicts that must agree with the parts.
//   w_latch   a cell that excites ITSELF (a self-loop) is a latch: the check must accept it (it holds at a few hundred Hz).
//   worm      the brief says turning one way is enough: a circuit that wires only turn_left must pass.
//   l_burst   an intrinsically bursting cell (Izhikevich IB) that fires one long run is not "a plain cell": the verdict follows the measurement.
//   aplysia_gill  a shock that makes the touch stronger, but not by enough, is not "no stronger".
#include <cstdio>
#include <string>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace
{
struct Run
{
  bool loaded = false;
  Score score;
};
Run run_circuit( const std::string& id, const std::string& text, uint64_t seed = 1 )
{
  Run r;
  Engine e;
  json c = json::parse( text );
  c[ "seed" ] = 1000 * seed + c.value( "seed", 1 );
  r.loaded = e.load( c.dump() );
  if ( !r.loaded ) return r;
  auto t = make_task( id );
  t->set_quick( true );
  t->reset( seed );
  const long steps = static_cast< long >( t->quick_duration_ms() );
  for ( long s = 0; s < steps; ++s ) t->step( e, 1.0 );
  r.score = t->score();
  return r;
}
std::string lower( std::string s )
{
  for ( auto& ch : s ) ch = static_cast< char >( std::tolower( static_cast< unsigned char >( ch ) ) );
  return s;
}
}  // namespace

int main()
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  const std::string latch = R"JSON({"schema_version":1,"name":"novice w_latch","resolution_ms":0.1,"seed":7,"populations":[{"id":"A","model":"iaf_psc_alpha","n":1,"params":{}},{"id":"S","model":"iaf_psc_alpha","n":1,"params":{}}],"projections":[{"pre":"A","post":"A","rule":"all_to_all","synapse":{"synapse_model":"static_synapse","weight":1600,"delay":2.0}},{"pre":"S","post":"A","rule":"all_to_all","synapse":{"synapse_model":"static_synapse","weight":-1600,"delay":2.0}}],"inputs":[{"name":"touch","kind":"poisson","targets":[{"pop":"A"}],"weight":600,"value":0.0},{"name":"stop","kind":"poisson","targets":[{"pop":"S"}],"weight":600,"value":0.0}],"outputs":[{"name":"reverse","kind":"rate","neurons":[{"pop":"A"}]}],"record":{"spikes":"all"}})JSON";
  const std::string worm_one_way = R"JSON({"schema_version":1,"name":"novice worm","resolution_ms":0.1,"seed":7,"populations":[{"id":"TL","model":"iaf_psc_alpha","n":4,"params":{}},{"id":"INH","model":"iaf_psc_alpha","n":4,"params":{}}],"projections":[{"pre":"INH","post":"TL","rule":"all_to_all","synapse":{"synapse_model":"static_synapse","weight":-200,"delay":2.0}}],"inputs":[{"name":"ASEL","kind":"poisson","targets":[{"pop":"INH"}],"weight":280,"value":0.0},{"name":"ASER","kind":"poisson","targets":[{"pop":"TL"}],"weight":280,"value":0.0}],"outputs":[{"name":"turn_left","kind":"rate","neurons":[{"pop":"TL"}]},{"name":"turn_right","kind":"rate","neurons":[]}],"record":{"spikes":"all"}})JSON";
  const std::string burst_ib = R"JSON({"schema_version":1,"name":"novice l_burst","resolution_ms":0.1,"seed":7,"populations":[{"id":"HN","model":"izhikevich","n":1,"params":{"a":0.02,"b":0.2,"c":-55.0,"d":4.0}}],"projections":[],"inputs":[{"name":"drive","kind":"dc","targets":[{"pop":"HN"}],"weight":10,"value":0.0}],"outputs":[{"name":"burst","kind":"rate","neurons":[{"pop":"HN"}]}],"record":{"spikes":"all"}})JSON";
  const std::string gill_weak = R"JSON({"schema_version":1,"name":"novice aplysia_gill","resolution_ms":0.1,"seed":7,"populations":[{"id":"SN","model":"iaf_psc_alpha","n":6,"params":{}},{"id":"MN","model":"iaf_psc_alpha","n":1,"params":{}},{"id":"FN","model":"iaf_psc_alpha","n":1,"params":{}}],"projections":[{"pre":"SN","post":"MN","rule":"all_to_all","synapse":{"synapse_model":"tsodyks2_synapse","weight":1600,"delay":2.0,"U":0.4,"u":0.4,"x":1.0,"tau_rec":1000.0,"tau_fac":0.0}}],"inputs":[{"name":"touch","kind":"poisson","targets":[{"pop":"SN"}],"weight":1000,"value":0.0},{"name":"shock","kind":"poisson","targets":[{"pop":"FN"}],"weight":1000,"value":0.0}],"outputs":[{"name":"gill","kind":"rate","neurons":[{"pop":"MN"}]},{"name":"serotonin","kind":"modulator","neurons":[{"pop":"FN"}],"modulator":"NE","gain":0.05,"tau_ms":1000}],"record":{"spikes":"all"},"modulation":[{"modulator":"NE","projection":0,"param":"U","gain":1.0,"min":0.0,"max":1.0}]})JSON";

  { auto r = run_circuit( "w_latch", latch ); CHECK_MSG( r.loaded, "w_latch circuit loads" ); CHECK_MSG( r.score.passed, "w_latch: a self-excitation latch passes: " + r.score.verdict ); }
  { auto r = run_circuit( "worm", worm_one_way ); CHECK_MSG( r.loaded, "worm circuit loads" ); CHECK_MSG( r.score.passed, "worm: turning one way (turn_right unconnected) passes: " + r.score.verdict ); }
  {
    auto r = run_circuit( "l_burst", burst_ib );
    CHECK_MSG( r.loaded, "l_burst circuit loads" );
    CHECK_MSG( !r.score.passed, "l_burst: a long tonic run is not a burst" );
    CHECK_MSG( lower( r.score.verdict ).find( "plain" ) == std::string::npos, "l_burst: an IB cell is not called a plain cell: " + r.score.verdict );
  }
  {
    auto r = run_circuit( "aplysia_gill", gill_weak );
    CHECK_MSG( r.loaded, "aplysia_gill circuit loads" );
    CHECK_MSG( !r.score.passed, "aplysia_gill: sensitisation too small" );
    CHECK_MSG( lower( r.score.verdict ).find( "no stronger" ) == std::string::npos, "aplysia_gill: a stronger touch is not 'no stronger': " + r.score.verdict );
  }
  return chk::finish( "tasks_fixes_test" );
}
