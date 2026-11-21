// The reviewers' novice sequences (full review, gameplay finding 3): circuits a new player builds from the brief, in the order they try them. A verdict that says "strengthen X" is no help when X's
// input never fires: every run of a sequence must either pass or say, in words, WHICH group never fires (the shared SourceWatch of task_util.h), or tell something that is true of the circuit.
// The sequences are the reviewers' tools/sr2.py, sr3.py, pupil2.py and ch1c.py, rebuilt here; tasks_novice_test run <table> prints the verdicts.
#include <cstdio>
#include <string>
#include <vector>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace
{
json pop( const std::string& id, int n = 1, const std::string& model = "iaf_psc_alpha", json params = json::object() )
{
  return { { "id", id }, { "model", model }, { "n", n }, { "params", params } };
}
json proj( const std::string& a, const std::string& b, double w, double d = 2.0 )
{
  return { { "pre", a }, { "post", b }, { "rule", "all_to_all" }, { "synapse", { { "synapse_model", "static_synapse" }, { "weight", w }, { "delay", d } } } };
}
json inp( const std::string& name, const std::string& kind, double w, std::vector< std::string > targets, double value = 0 )
{
  json t = json::array();
  for ( auto& x : targets ) t.push_back( { { "pop", x } } );
  return { { "name", name }, { "kind", kind }, { "weight", w }, { "value", value }, { "targets", t } };
}
json out( const std::string& name, std::vector< std::string > pops )
{
  json n = json::array();
  for ( auto& x : pops ) n.push_back( { { "pop", x } } );
  return { { "name", name }, { "kind", "rate" }, { "neurons", n } };
}
json circ( std::vector< json > pops, std::vector< json > projs, std::vector< json > ins, std::vector< json > outs )
{
  return { { "schema_version", 1 }, { "name", "novice" }, { "resolution_ms", 0.1 }, { "seed", 1 }, { "populations", pops }, { "projections", projs }, { "inputs", ins }, { "outputs", outs },
    { "record", { { "spikes", "all" } } } };
}

struct Res
{
  bool passed = false;
  std::string verdict;
};
Res run( const std::string& id, const json& c, uint64_t seed = 1 )
{
  Res r;
  Engine e;
  json cc = c;
  cc[ "seed" ] = 1000 * seed + 1;
  if ( !e.load( cc.dump() ) ) { r.verdict = "LOAD: " + e.error(); return r; }
  auto t = make_task( id );
  t->set_quick( true );
  t->reset( seed );
  for ( long s = 0; s < static_cast< long >( t->quick_duration_ms() ); ++s ) t->step( e, 1.0 );
  const Score sc = t->score();
  r.passed = sc.passed;
  r.verdict = sc.verdict;
  return r;
}
bool names_silent( const std::string& v ) { return v.find( "never fire" ) != std::string::npos; }
bool table = false;
void judge( const std::string& task, const std::string& label, const Res& r, bool must_name_or_pass )
{
  if ( table ) std::printf( "%-16s %-34s %s | %s\n", task.c_str(), label.c_str(), r.passed ? "PASS" : "fail", r.verdict.substr( 0, 200 ).c_str() );
  CHECK_MSG( r.verdict.rfind( "LOAD:", 0 ) != 0, task + " / " + label + ": " + r.verdict );
  if ( must_name_or_pass ) CHECK_MSG( r.passed || names_silent( r.verdict ), task + " / " + label + ": neither passes nor names the silent group: " + r.verdict );
}
}  // namespace

int main( int argc, char** argv )
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  table = argc > 1 && std::string( argv[ 1 ] ) == "table";
  // ---- stretch_reflex (sr2.py): the Ia fibres reach the interneurons at the lab's first-connect weight, so the interneurons (and the "reciprocal inhibition" they carry) stay silent
  for ( int iaw : { 3, 10 } )
    for ( int inn : { 1, 3 } )
      for ( int inh : { -1000, -2500 } )
      {
        json c = circ( { pop( "MNF", 6 ), pop( "MNE", 6 ), pop( "INF", inn ), pop( "INE", inn ) }, { proj( "INF", "MNE", inh, 1.0 ), proj( "INE", "MNF", inh, 1.0 ) },
          { inp( "ag_Ia", "dc", iaw, { "MNF", "INF" }, 40 ), inp( "ant_Ia", "dc", iaw, { "MNE", "INE" }, 40 ), inp( "drive", "dc", 320, { "MNF", "MNE" }, 1 ) },
          { out( "ag_motor", { "MNF" } ), out( "ant_motor", { "MNE" } ) } );
        judge( "stretch_reflex", "Ia " + std::to_string( iaw ) + " IN " + std::to_string( inn ) + " inh " + std::to_string( inh ), run( "stretch_reflex", c ), iaw == 3 );   // (at weight 10 the Ia fibres fire the interneurons: other, true verdicts)
      }
  // ... and once the interneurons get the steady drive too (sr3.py), a moderate inhibition passes
  {
    json c = circ( { pop( "MNF", 6 ), pop( "MNE", 6 ), pop( "INF" ), pop( "INE" ) }, { proj( "INF", "MNE", -300, 1.0 ), proj( "INE", "MNF", -300, 1.0 ) },
      { inp( "ag_Ia", "dc", 3, { "MNF", "INF" }, 40 ), inp( "ant_Ia", "dc", 3, { "MNE", "INE" }, 40 ), inp( "drive", "dc", 320, { "MNF", "MNE", "INF", "INE" }, 1 ) },
      { out( "ag_motor", { "MNF" } ), out( "ant_motor", { "MNE" } ) } );
    const Res r = run( "stretch_reflex", c );
    judge( "stretch_reflex", "interneurons driven, inh -300", r, false );
    CHECK_MSG( r.passed, "stretch_reflex: the novice who drives the interneurons must pass: " + r.verdict );
  }
  // ---- pupil (pupil2.py): a long arc with the dilator held by the threat cue only
  for ( int w : { 480, 640, 1280 } )
    for ( int d : { 10, 25, 50 } )
    {
      json c = circ( { pop( "RGC", 4 ), pop( "PON", 4 ), pop( "EW", 4 ), pop( "CG", 4 ), pop( "HYP", 4 ), pop( "IML", 4 ), pop( "SCG", 4 ) },
        { proj( "RGC", "PON", w, d ), proj( "PON", "EW", w, d ), proj( "EW", "CG", w, d ), proj( "HYP", "IML", w ), proj( "IML", "SCG", w ), proj( "PON", "IML", -w ) },
        { inp( "retina", "poisson", 280, { "RGC" } ), inp( "threat", "poisson", 280, { "HYP" } ) }, { out( "sphincter", { "CG" } ), out( "dilator", { "SCG" } ) } );
      judge( "pupil", "w " + std::to_string( w ) + " delay " + std::to_string( d ), run( "pupil", c ), false );
    }
  // ---- worm (ch1c.py): ASER excites the turn cell, ASEL inhibits it through a cell
  for ( int ntl : { 1, 4 } )
    for ( int wi : { -80, -500, -1500 } )
    {
      json c = circ( { pop( "TL", ntl ), pop( "IN" ) }, { proj( "IN", "TL", wi, 1.0 ) }, { inp( "ASER", "poisson", 1000, { "TL" }, 5 ), inp( "ASEL", "poisson", 1000, { "IN" }, 5 ) }, { out( "turn_left", { "TL" } ) } );
      judge( "worm", "TL " + std::to_string( ntl ) + " inh " + std::to_string( wi ), run( "worm", c ), false );
    }
  // ... and the novice who leaves the ASEL weight at a token value: the inhibitory cell never hears ASEL, so no inhibition weight matters (the verdict says which group is silent)
  for ( int wi : { -500, -1500 } )
  {
    json c = circ( { pop( "TL", 2 ), pop( "IN" ) }, { proj( "IN", "TL", wi, 1.0 ) }, { inp( "ASER", "poisson", 1000, { "TL" }, 5 ), inp( "ASEL", "poisson", 20, { "IN" }, 5 ) }, { out( "turn_left", { "TL" } ) } );
    judge( "worm", "ASEL weight token, inh " + std::to_string( wi ), run( "worm", c ), true );
  }
  // ---- w_latch: a cell that already excites itself is told to excite itself MORE STRONGLY, not "make two cells excite each other"
  for ( double w : { 100.0, 400.0 } )
  {
    json c = circ( { pop( "R" ), pop( "S" ) }, { proj( "R", "R", w ), proj( "S", "R", -2560 ) }, { inp( "touch", "poisson", 1000, { "R" }, 150 ), inp( "stop", "poisson", 1000, { "S" }, 150 ) }, { out( "reverse", { "R" } ) } );
    const Res r = run( "w_latch", c );
    judge( "w_latch", "self excitation " + std::to_string( static_cast< int >( w ) ), r, false );
    if ( !r.passed ) CHECK_MSG( r.verdict.find( "more strongly" ) != std::string::npos || names_silent( r.verdict ) || r.verdict.find( "never makes" ) != std::string::npos, "w_latch: a cell that already excites itself must be told 'more strongly': " + r.verdict );
  }
  return chk::finish( "tasks_novice_test" );
}
