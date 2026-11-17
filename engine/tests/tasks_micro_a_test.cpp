// Micro-puzzles of concepts 1-10 (m_threshold ... m_neuromodulation; docs/DESIGN-0.3.md section 3, circuits/m_*.json, docs/circuits/m_*.md).
//
//   tasks_micro_a_test                    the CTest
//   tasks_micro_a_test run <task> <circuit.json> [seeds=1,2,3,4,5] [mode=standard]   tuning aid: score one circuit, print parts and verdict
//
// What it checks, per puzzle:
//   * the v0.3 contract: level_kind "micro", one concept introduced, at most two reused, a chapter, 2-5 s episode, 1-4 reference neurons,
//     channel_kinds for every channel, hints in three tiers (concept, place, partial recipe), one quality metric, a cited notebook entry,
//     and the campaign order (the micro-puzzle comes before the big task that uses it);
//   * the reference circuit passes on 5 seeds in guided, standard AND researcher mode, and is deterministic;
//   * every lesion fails (a cut axon, a silenced cell, a cut sensor, a changed parameter);
//   * the empty bench fails and says which channel is not wired;
//   * the novice path: the first build made with the lab's defaults fails with the verdict that tells the player what to do, and each
//     following build (the one a player makes after reading the verdict) ends in a pass in at most 8 runs (the gate of docs/DESIGN-0.3.md section 10);
//   * the modes scale only the tolerance: a marginal bench passes in guided and fails in standard (or passes in standard and fails in researcher).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace
{

// ---------------------------------------------------------------- running a circuit on a task
struct Result
{
  Score score;
  std::map< std::string, Task::Metric > metrics;
  std::vector< Task::Hint > hints;
  bool loaded = false;
  std::string error;
};

Result run_circuit( const std::string& id, const json& circuit_in, uint64_t seed, const std::string& mode = "standard" )
{
  Result r;
  Engine e;
  json c = circuit_in;
  c[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );  // the task seed also reseeds NEST and the initial draws, as in the game
  r.loaded = e.load( c.dump() );
  if ( !r.loaded )
  {
    r.error = e.error();
    return r;
  }
  auto t = make_task( id );
  t->set_mode( mode );
  t->reset( seed );
  const long steps = static_cast< long >( t->duration_ms() );
  for ( long s = 0; s < steps; ++s )
    t->step( e, 1.0 );
  r.score = t->score();
  r.metrics = t->metrics();
  r.hints = t->hints();
  r.error = e.error();
  return r;
}

json load_circuit( const std::string& id )
{
  const std::string path = std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + reference_circuit( id );
  const std::string txt = chk::slurp( path );
  CHECK_MSG( !txt.empty(), "cannot read " + path );
  return txt.empty() ? json::object() : json::parse( txt );
}

int neuron_count( const json& c )
{
  int n = 0;
  for ( auto& p : c[ "populations" ] ) n += p[ "n" ].get< int >();
  return n;
}

// ---------------------------------------------------------------- building benches the way the lab does (defaults: delay 2 ms, weight = the cell's catalogue weight)
struct Bench
{
  json c = { { "schema_version", 1 }, { "name", "novice bench" }, { "resolution_ms", 0.1 }, { "seed", 1 }, { "populations", json::array() },
    { "projections", json::array() }, { "inputs", json::array() }, { "outputs", json::array() }, { "record", { { "spikes", "all" } } } };
  Bench& pop( const std::string& id, const std::string& model = "iaf_psc_alpha", int n = 1, json params = json::object() )
  {
    c[ "populations" ].push_back( { { "id", id }, { "model", model }, { "n", n }, { "params", params } } );
    return *this;
  }
  Bench& proj( const std::string& pre, const std::string& post, double w, double delay = 2.0 )
  {
    c[ "projections" ].push_back( { { "pre", pre }, { "post", post }, { "rule", "all_to_all" },
      { "synapse", { { "synapse_model", "static_synapse" }, { "weight", w }, { "delay", delay } } } } );
    return *this;
  }
  Bench& in( const std::string& name, const std::string& kind, double weight, std::vector< std::string > pops )
  {
    json t = json::array();
    for ( auto& p : pops ) t.push_back( { { "pop", p } } );
    c[ "inputs" ].push_back( { { "name", name }, { "kind", kind }, { "targets", t }, { "weight", weight }, { "value", 0.0 } } );
    return *this;
  }
  Bench& out( const std::string& name, std::vector< std::string > pops, const json& modulator = json() )
  {
    json n = json::array();
    for ( auto& p : pops ) n.push_back( { { "pop", p } } );
    json o = { { "name", name }, { "kind", "rate" }, { "neurons", n } };
    if ( !modulator.is_null() )
    {
      o[ "kind" ] = "modulator";
      o[ "modulator" ] = modulator[ "modulator" ];
      o[ "gain" ] = modulator[ "gain" ];
      o[ "tau_ms" ] = modulator[ "tau_ms" ];
    }
    c[ "outputs" ].push_back( o );
    return *this;
  }
};

// ---------------------------------------------------------------- lesions: edits of a reference circuit that must make the puzzle fail
using Mut = std::function< void( json& ) >;

json* find_pop( json& c, const std::string& id )
{
  for ( auto& p : c[ "populations" ] ) if ( p[ "id" ] == id ) return &p;
  return nullptr;
}
Mut cut( const std::string& pre, const std::string& post, double factor = 0.0 )  // scale (or delete, factor 0) every projection pre > post
{
  return [ = ]( json& c ) {
    bool hit = false;
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == pre && p[ "post" ] == post )
      {
        hit = true;
        p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * factor;
      }
    CHECK_MSG( hit, "lesion matched no projection " + pre + ">" + post );
  };
}
Mut set_delay( const std::string& pre, const std::string& post, double d )
{
  return [ = ]( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == pre && p[ "post" ] == post ) p[ "synapse" ][ "delay" ] = d;
  };
}
Mut scale_input( const std::string& name, double factor )  // factor 0: the sensor is cut (deafferentation)
{
  return [ = ]( json& c ) {
    for ( auto& i : c[ "inputs" ] )
      if ( i[ "name" ] == name )
      {
        if ( factor == 0.0 ) i[ "targets" ] = json::array();
        else i[ "weight" ] = i[ "weight" ].get< double >() * factor;
        return;
      }
    CHECK_MSG( false, "no input " + name );
  };
}
Mut scale_all_inputs( double factor )
{
  return [ = ]( json& c ) { for ( auto& i : c[ "inputs" ] ) i[ "weight" ] = i[ "weight" ].get< double >() * factor; };
}
Mut silence( const std::string& id )  // the lesion lab's "silence population": cut its synapses, remove it from inputs and outputs
{
  return [ = ]( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == id || p[ "post" ] == id ) p[ "synapse" ][ "weight" ] = 0.0;
    for ( auto& i : c[ "inputs" ] )
    {
      json t = json::array();
      for ( auto& x : i[ "targets" ] ) if ( x[ "pop" ] != id ) t.push_back( x );
      i[ "targets" ] = t;
    }
    for ( auto& o : c[ "outputs" ] )
    {
      json t = json::array();
      for ( auto& x : o[ "neurons" ] ) if ( x[ "pop" ] != id ) t.push_back( x );
      o[ "neurons" ] = t;
    }
  };
}
Mut set_params( const std::string& id, json params )
{
  return [ = ]( json& c ) {
    json* p = find_pop( c, id );
    CHECK_MSG( p != nullptr, "no population " + id );
    if ( p ) for ( auto it = params.begin(); it != params.end(); ++it ) ( *p )[ "params" ][ it.key() ] = it.value();
  };
}
Mut set_model( const std::string& id, const std::string& model )
{
  return [ = ]( json& c ) {
    json* p = find_pop( c, id );
    if ( p ) { ( *p )[ "model" ] = model; ( *p )[ "params" ] = json::object(); }
  };
}
Mut scale_output_gain( const std::string& name, double factor )
{
  return [ = ]( json& c ) {
    for ( auto& o : c[ "outputs" ] )
      if ( o[ "name" ] == name ) o[ "gain" ] = o[ "gain" ].get< double >() * factor;
  };
}
Mut both( Mut a, Mut b ) { return [ = ]( json& c ) { a( c ); b( c ); }; }

struct Lesion { std::string task; Mut edit; const char* what; };

struct Step  // one build of the novice's path: what they built, and what the verdict has to say
{
  const char* what;
  json circuit;
  bool pass;
  const char* verdict_has;  // substring of the verdict ("" = no check)
};

}  // namespace

int main( int argc, char** argv )
{
  if ( argc >= 4 && std::string( argv[ 1 ] ) == "run" )
  {
    json c = json::parse( chk::slurp( argv[ 3 ] ) );
    std::string sl = argc > 4 ? argv[ 4 ] : "1,2,3,4,5", mode = argc > 5 ? argv[ 5 ] : "standard";
    std::stringstream ss( sl );
    for ( std::string tok; std::getline( ss, tok, ',' ); )
    {
      Result r = run_circuit( argv[ 2 ], c, std::stoull( tok ), mode );
      std::printf( "seed %s value %5.1f %s |", tok.c_str(), r.score.value, r.score.passed ? "PASS" : "fail" );
      for ( auto& [ k, v ] : r.score.parts ) std::printf( " %s=%.3g", k.c_str(), v );
      std::printf( "\n  %s\n", r.score.verdict.c_str() );
    }
    return 0;
  }

  // ------------------------------------------------------------ contract
  struct Spec
  {
    const char* id;
    const char* chapter;
    const char* concept_id;
    std::vector< std::string > used;
    const char* big_task;  // the task that uses the concept
  };
  const std::vector< Spec > specs = {
    { "m_threshold", "invertebrates", "threshold", {}, "reflex" },
    { "m_excitation", "invertebrates", "excitation", { "threshold" }, "reflex" },
    { "m_convergence", "invertebrates", "convergence", { "threshold", "excitation" }, "reflex" },
    { "m_inhibition", "invertebrates", "inhibition", { "threshold", "excitation" }, "worm" },
    { "m_subtraction", "invertebrates", "subtraction", { "inhibition", "convergence" }, "worm" },
    { "m_adaptation", "invertebrates", "adaptation", { "threshold" }, "lamprey" },
    { "m_half_centre", "invertebrates", "half_centre", { "inhibition", "adaptation" }, "lamprey" },
    { "m_pacemaking", "heart", "pacemaking", { "threshold" }, "heart_pacemaker" },
    { "m_coupling", "heart", "coupling", { "pacemaking", "excitation" }, "heart_pacemaker" },
    { "m_neuromodulation", "heart", "neuromodulation", { "pacemaking", "excitation" }, "heart_autonomic" } };

  const auto ids = task_ids();
  auto index_of = [ & ]( const std::string& id ) {
    for ( size_t i = 0; i < ids.size(); ++i )
      if ( ids[ i ] == id ) return static_cast< int >( i );
    return -1;
  };
  std::set< std::string > concepts_seen;
  for ( const auto& sp : specs )
  {
    const std::string id = sp.id;
    auto t = make_task( id );
    CHECK_MSG( t != nullptr, id + " is not registered" );
    if ( !t ) continue;
    std::printf( "%s\n", id.c_str() );
    CHECK( t->id() == id );
    CHECK( t->level_kind() == "micro" );
    CHECK( t->chapter() == sp.chapter );
    CHECK( t->on_spine() );
    CHECK_MSG( t->concepts_new().size() == 1 && t->concepts_new()[ 0 ] == sp.concept_id, id + " introduces exactly its concept" );
    CHECK_MSG( concepts_seen.insert( sp.concept_id ).second, id + ": concept already introduced by another puzzle" );
    CHECK_MSG( t->concepts_used().size() <= 2 && t->concepts_used() == sp.used, id + " concepts_used" );
    CHECK( t->duration_ms() >= 2000 && t->duration_ms() <= 5000 );
    CHECK( t->par_neurons() >= 1 && t->par_neurons() <= 4 );
    CHECK( t->view() == "dashboard" );
    CHECK( !t->title().empty() );
    CHECK_MSG( t->brief().size() > 300 && t->brief().size() < 1400, id + " brief length " + std::to_string( t->brief().size() ) );
    CHECK_MSG( t->brief().find( "recipe" ) == std::string::npos, id + ": the brief must not carry the recipe (that is hint tier 3)" );
    CHECK( !t->bands().empty() );
    // the micro-puzzle comes before the big task that uses it, and after the previous big task
    CHECK_MSG( index_of( id ) >= 0 && index_of( id ) < index_of( sp.big_task ), id + " must sort before " + sp.big_task );
    // channels
    auto kinds = t->channel_kinds();
    for ( auto& s : t->sensor_channels() ) CHECK_MSG( kinds.count( s ) && ( kinds[ s ].kind == "dc" || kinds[ s ].kind == "poisson" ), id + " kind of sensor " + s );
    for ( auto& m : t->motor_channels() ) CHECK_MSG( kinds.count( m ) && ( kinds[ m ].kind == "rate" || kinds[ m ].kind == "modulator" ), id + " kind of motor " + m );
    for ( auto& s : t->sensor_channels() ) CHECK_MSG( kinds[ s ].kind == "dc" && kinds[ s ].value > 0, id + ": a dc sensor needs a typical level" );
    CHECK( !t->motor_channels().empty() );
    // hints: three tiers, the second names a place
    auto hs = t->hints();
    CHECK_MSG( hs.size() == 3, id + " hint count" );
    for ( size_t k = 0; k < hs.size(); ++k )
    {
      CHECK( hs[ k ].tier == static_cast< int >( k + 1 ) );
      CHECK_MSG( hs[ k ].text.size() > 40, id + " hint text" );
    }
    if ( hs.size() == 3 )
    {
      CHECK_MSG( hs[ 1 ].target != "", id + ": tier 2 must name a population or channel" );
      std::set< std::string > chans;
      for ( auto& s : t->sensor_channels() ) chans.insert( s );
      for ( auto& m : t->motor_channels() ) chans.insert( m );
      CHECK_MSG( chans.count( hs[ 1 ].target ) == 1, id + ": tier 2 target is a channel of the puzzle" );
    }
    // notebook: a title, a text and a citation with a year
    auto nb = t->notebook();
    CHECK( !nb.title.empty() && nb.text.size() > 150 );
    bool year = false;
    for ( size_t i = 0; i + 3 < nb.citation.size(); ++i )
      if ( ( nb.citation.compare( i, 2, "19" ) == 0 || nb.citation.compare( i, 2, "20" ) == 0 ) && std::isdigit( nb.citation[ i + 2 ] ) && std::isdigit( nb.citation[ i + 3 ] ) ) year = true;
    CHECK_MSG( year, id + ": citation needs a year" );
    // modes change tolerance only
    for ( const char* m : { "guided", "researcher" } )
    {
      auto t2 = make_task( id );
      t2->set_mode( m );
      CHECK( t2->mode() == m );
      CHECK( t2->sensor_channels() == t->sensor_channels() && t2->motor_channels() == t->motor_channels() && t2->duration_ms() == t->duration_ms() );
      CHECK( t2->title() == t->title() && t2->concepts_new() == t->concepts_new() );
    }

    // -------------------------------------------------------- reference circuit
    json ref = load_circuit( id );
    const int n = neuron_count( ref );
    CHECK_MSG( n >= 1 && n <= 4, id + " reference has " + std::to_string( n ) + " neurons" );
    CHECK_MSG( reference_circuit( id ) == id + ".json", id + " reference circuit name" );
    std::map< std::string, bool > in_ch, out_ch;
    for ( auto& i : ref[ "inputs" ] ) in_ch[ i[ "name" ] ] = true;
    for ( auto& o : ref[ "outputs" ] ) out_ch[ o[ "name" ] ] = true;
    for ( auto& s : t->sensor_channels() ) CHECK_MSG( in_ch.count( s ), id + " reference declares sensor " + s );
    for ( auto& m : t->motor_channels() ) CHECK_MSG( out_ch.count( m ), id + " reference declares motor " + m );
    for ( const char* mode : { "guided", "standard", "researcher" } )
    {
      double sum = 0;
      for ( uint64_t seed = 1; seed <= 5; ++seed )
      {
        Result r = run_circuit( id, ref, seed, mode );
        CHECK_MSG( r.loaded, r.error );
        CHECK_MSG( r.error.empty(), r.error );
        CHECK_MSG( r.score.passed, id + " reference, " + mode + ", seed " + std::to_string( seed ) + ": " + r.score.verdict );
        CHECK( r.score.value >= 80 && r.score.value <= 100 );
        CHECK_MSG( !r.score.verdict.empty(), id + " verdict" );
        sum += r.score.value;
        if ( std::string( mode ) == "standard" && seed == 1 )
        {
          CHECK_MSG( r.metrics.size() == 1, id + ": exactly one quality metric" );
          for ( auto& [ k, m ] : r.metrics ) CHECK_MSG( std::isfinite( m.value ) && !m.unit.empty(), id + " metric " + k );
          std::printf( "  %s\n", r.score.verdict.c_str() );
        }
      }
      CHECK( sum / 5 >= 90 );
    }
    // deterministic: the same seed twice gives the same score and the same parts
    Result r1 = run_circuit( id, ref, 3 ), r2 = run_circuit( id, ref, 3 );
    CHECK( r1.score.value == r2.score.value && r1.score.parts == r2.score.parts && r1.score.verdict == r2.score.verdict );

    // -------------------------------------------------------- the empty bench fails and says what is missing
    Bench empty;
    for ( auto& s : t->sensor_channels() ) empty.in( s, kinds[ s ].kind, 300, {} );
    for ( auto& m : t->motor_channels() )
    {
      if ( kinds[ m ].kind == "modulator" )
        empty.out( m, {}, { { "modulator", kinds[ m ].modulator }, { "gain", kinds[ m ].gain }, { "tau_ms", kinds[ m ].tau_ms } } );
      else
        empty.out( m, {} );
    }
    empty.pop( "N0", "iaf_psc_alpha" );  // the engine needs at least one cell to load
    for ( const char* mode : { "guided", "standard", "researcher" } )
    {
      Result r = run_circuit( id, empty.c, 1, mode );
      CHECK_MSG( r.loaded, "empty bench loads: " + r.error );
      CHECK_MSG( !r.score.passed, id + " passes on an empty bench in " + mode );
      CHECK_MSG( r.score.verdict.find( "Not wired" ) == 0 || r.score.verdict.find( "not connected" ) != std::string::npos, id + " empty bench verdict: " + r.score.verdict );
    }
  }

  // ------------------------------------------------------------ lesions
  const std::vector< Lesion > lesions = {
    { "m_threshold", silence( "N1" ), "cell silenced (no spike)" },
    { "m_threshold", scale_input( "pulse", 0.5 ), "pulse weight halved: the step stays under the rheobase (225 pA < 375 pA)" },
    { "m_threshold", scale_input( "pulse", 1.6 ), "pulse weight x1.6: the blip is over threshold too" },
    { "m_excitation", cut( "N1", "N2" ), "the link A > B cut (no echo)" },
    { "m_excitation", cut( "N1", "N2", 0.2 ), "link weight x0.2: one spike no longer fires B" },
    { "m_excitation", set_delay( "N1", "N2", 2.0 ), "delay back to the default 2 ms: the echo comes 5.7 ms later, not 8" },
    { "m_convergence", scale_input( "in_a", 0.0 ), "in_a cut: two inputs (300 pA) never reach the rheobase" },
    { "m_convergence", scale_all_inputs( 2.0 ), "all weights x2: two inputs are enough, pairs fire" },
    { "m_convergence", scale_all_inputs( 0.5 ), "all weights x0.5: even three inputs stay under threshold" },
    { "m_inhibition", cut( "N2", "N1" ), "the inhibitory link cut (stop does nothing)" },
    { "m_inhibition", cut( "N2", "N1", 0.1 ), "inhibition x0.1: stop only slows the cell" },
    { "m_inhibition", silence( "N2" ), "inhibitory cell silenced" },
    { "m_subtraction", cut( "N2", "N1" ), "minus cut off from the output (equal inputs no longer cancel)" },
    { "m_subtraction", cut( "N2", "N1", 1.6 ), "inhibition x1.6: plus 3 against minus 2 is silenced too" },
    { "m_subtraction", scale_input( "plus", 0.0 ), "plus cut (never fires)" },
    { "m_adaptation", set_params( "N1", { { "b", 0.0 }, { "a", 0.0 } } ), "adaptation removed (a = b = 0): the rate stays flat" },
    { "m_adaptation", set_params( "N1", { { "tau_w", 30.0 } } ), "tau_w 30 ms: the adaptation current fades before it builds up" },
    { "m_adaptation", silence( "N1" ), "cell silenced" },
    { "m_half_centre", cut( "N1", "N2" ), "one of the two inhibitory links cut (L silences nothing)" },
    { "m_half_centre", both( cut( "N1", "N2" ), cut( "N2", "N1" ) ), "no inhibition at all: both sides fire together" },
    { "m_half_centre", both( set_params( "N1", { { "b", 0.0 }, { "a", 2.0 }, { "tau_w", 30.0 } } ), set_params( "N2", { { "b", 0.0 }, { "a", 2.0 }, { "tau_w", 30.0 } } ) ), "no adaptation (the Tonic preset): one side wins or both fire in step" },
    { "m_coupling", both( cut( "N1", "N2" ), cut( "N2", "N1" ) ), "coupling cut: the beats slip past each other" },
    { "m_coupling", both( cut( "N1", "N2", 0.1 ), cut( "N2", "N1", 0.1 ) ), "coupling x0.1 (the default 0.2 nS): too weak to lock" },
    { "m_pacemaking", silence( "N1" ), "cell silenced" },
    { "m_pacemaking", set_model( "N1", "iaf_psc_alpha" ), "the SA cell replaced by a plain integrate-and-fire cell (silent without input)" },
    { "m_neuromodulation", silence( "N1" ), "the vagus cell silenced (no ACh released)" },
    { "m_neuromodulation", scale_input( "vagus", 0.0 ), "vagus cut" },
    { "m_neuromodulation", scale_output_gain( "vagal", 0.05 ), "ACh gain x0.05: the level stays near 0.03" } };
  for ( const auto& l : lesions )
  {
    json c = load_circuit( l.task );
    l.edit( c );
    std::printf( "%s lesion: %s\n", l.task.c_str(), l.what );
    for ( uint64_t seed : { 1, 2 } )
    {
      Result r = run_circuit( l.task, c, seed );
      CHECK_MSG( r.loaded, r.error );
      CHECK_MSG( !r.score.passed, l.task + " still passes with lesion: " + l.what );
      std::printf( "  seed %d: %5.1f  %s\n", static_cast< int >( seed ), r.score.value, r.score.verdict.c_str() );
    }
  }

  // ------------------------------------------------------------ the novice path: builds a player makes from an empty bench, with the lab's defaults
  // (iaf_psc_alpha synapse 80 pA, aeif 8 nS, SA node 0.2 nS; delay 2 ms; dc sensors start at weight 300 pA). Every path ends in a pass within 8 runs.
  const json MODACH = { { "modulator", "ACh" }, { "gain", 0.012 }, { "tau_ms", 500.0 } };
  const json ADAPT = { { "a", 0.0 }, { "b", 60.0 }, { "tau_w", 100.0 }, { "V_reset", -58.0 } };  // the "Adapting" preset
  struct Path { std::string task; std::vector< Step > steps; };
  std::vector< Path > paths;
  paths.push_back( { "m_threshold", {
    { "pulse > N1 > spike at the start weight 300", Bench().pop( "N1" ).in( "pulse", "dc", 300, { "N1" } ).out( "spike", { "N1" } ).c, false, "never fired" },
    { "weight raised to 450", Bench().pop( "N1" ).in( "pulse", "dc", 450, { "N1" } ).out( "spike", { "N1" } ).c, true, "Clean threshold" } } } );
  paths.push_back( { "m_excitation", {
    { "tap > A > B at the default 80 pA", Bench().pop( "N1" ).pop( "N2" ).in( "tap", "dc", 300, { "N1" } ).out( "source", { "N1" } ).out( "echo", { "N2" } ).proj( "N1", "N2", 80 ).c, false, "never fired" },
    { "weight x5 (400)", Bench().pop( "N1" ).pop( "N2" ).in( "tap", "dc", 300, { "N1" } ).out( "source", { "N1" } ).out( "echo", { "N2" } ).proj( "N1", "N2", 400 ).c, false, "never fired" },
    { "weight 1500 (the verdict said: a lot)", Bench().pop( "N1" ).pop( "N2" ).in( "tap", "dc", 300, { "N1" } ).out( "source", { "N1" } ).out( "echo", { "N2" } ).proj( "N1", "N2", 1500 ).c, false, "too early" },
    { "delay + 2.3 ms as the verdict says", Bench().pop( "N1" ).pop( "N2" ).in( "tap", "dc", 300, { "N1" } ).out( "source", { "N1" } ).out( "echo", { "N2" } ).proj( "N1", "N2", 1500, 4.3 ).c, true, "echoed" } } } );
  paths.push_back( { "m_convergence", {
    { "three sensors at the start weight 300 onto one cell", Bench().pop( "N1" ).in( "in_a", "dc", 300, { "N1" } ).in( "in_b", "dc", 300, { "N1" } ).in( "in_c", "dc", 300, { "N1" } ).out( "out", { "N1" } ).c, false, "also fired for" },
    { "all three weights halved (150)", Bench().pop( "N1" ).in( "in_a", "dc", 150, { "N1" } ).in( "in_b", "dc", 150, { "N1" } ).in( "in_c", "dc", 150, { "N1" } ).out( "out", { "N1" } ).c, true, "all three inputs together" } } } );
  paths.push_back( { "m_inhibition", {
    { "stop wired through an excitatory cell", Bench().pop( "N1" ).pop( "N2" ).in( "drive", "dc", 300, { "N1" } ).in( "stop", "dc", 300, { "N2" } ).out( "out", { "N1" } ).proj( "N2", "N1", 80 ).c, false, "no difference" },
    { "stop cell set inhibitory (-80)", Bench().pop( "N1" ).pop( "N2" ).in( "drive", "dc", 300, { "N1" } ).in( "stop", "dc", 300, { "N2" } ).out( "out", { "N1" } ).proj( "N2", "N1", -80 ).c, false, "times its present weight" },
    { "weight -1000 as the verdict said", Bench().pop( "N1" ).pop( "N2" ).in( "drive", "dc", 300, { "N1" } ).in( "stop", "dc", 300, { "N2" } ).out( "out", { "N1" } ).proj( "N2", "N1", -1000 ).c, true, "Stop silences" } } } );
  paths.push_back( { "m_subtraction", {
    { "plus > out, minus > inhibitory cell > out at -80", Bench().pop( "N1" ).pop( "N2" ).in( "plus", "dc", 300, { "N1" } ).in( "minus", "dc", 300, { "N2" } ).out( "diff", { "N1" } ).proj( "N2", "N1", -80 ).c, false, "times its present weight" },
    { "inhibitory link -1100", Bench().pop( "N1" ).pop( "N2" ).in( "plus", "dc", 300, { "N1" } ).in( "minus", "dc", 300, { "N2" } ).out( "diff", { "N1" } ).proj( "N2", "N1", -1100 ).c, true, "plus minus minus" } } } );
  paths.push_back( { "m_adaptation", {
    { "step > one aeif cell (NEST default part) > spikes", Bench().pop( "N1", "aeif_cond_alpha" ).in( "step", "dc", 300, { "N1" } ).out( "spikes", { "N1" } ).c, false, "falls only a little" },
    { "tau_w raised to 400 ms", Bench().pop( "N1", "aeif_cond_alpha", 1, { { "tau_w", 400.0 } } ).in( "step", "dc", 300, { "N1" } ).out( "spikes", { "N1" } ).c, true, "The cell adapts" } } } );
  paths.push_back( { "m_half_centre", {
    { "two Adapting cells, both left excitatory", Bench().pop( "N1", "aeif_cond_alpha", 1, ADAPT ).pop( "N2", "aeif_cond_alpha", 1, ADAPT ).in( "drive", "dc", 300, { "N1", "N2" } ).out( "left", { "N1" } ).out( "right", { "N2" } ).proj( "N1", "N2", 8 ).proj( "N2", "N1", 8 ).c, false, "inhibitory" },
    { "both set inhibitory (-8 nS)", Bench().pop( "N1", "aeif_cond_alpha", 1, ADAPT ).pop( "N2", "aeif_cond_alpha", 1, ADAPT ).in( "drive", "dc", 300, { "N1", "N2" } ).out( "left", { "N1" } ).out( "right", { "N2" } ).proj( "N1", "N2", -8 ).proj( "N2", "N1", -8 ).c, true, "A half-centre" } } } );
  paths.push_back( { "m_half_centre", {  // the other tempting first build: plain cells with strong inhibition: one side wins for good
    { "plain integrate-and-fire cells, inhibition -1600", Bench().pop( "N1" ).pop( "N2" ).in( "drive", "dc", 450, { "N1", "N2" } ).out( "left", { "N1" } ).out( "right", { "N2" } ).proj( "N1", "N2", -1600 ).proj( "N2", "N1", -1600 ).c, false, "adapting" },
    { "Adapting aeif cells", Bench().pop( "N1", "aeif_cond_alpha", 1, ADAPT ).pop( "N2", "aeif_cond_alpha", 1, ADAPT ).in( "drive", "dc", 300, { "N1", "N2" } ).out( "left", { "N1" } ).out( "right", { "N2" } ).proj( "N1", "N2", -8 ).proj( "N2", "N1", -8 ).c, true, "A half-centre" } } } );
  paths.push_back( { "m_coupling", {
    { "two SA cells linked both ways at the default 0.2 nS", Bench().pop( "N1", "sa_node_pacemaker" ).pop( "N2", "sa_node_pacemaker" ).out( "a", { "N1" } ).out( "b", { "N2" } ).proj( "N1", "N2", 0.2 ).proj( "N2", "N1", 0.2 ).c, false, "Not locked" },
    { "weight 2 nS", Bench().pop( "N1", "sa_node_pacemaker" ).pop( "N2", "sa_node_pacemaker" ).out( "a", { "N1" } ).out( "b", { "N2" } ).proj( "N1", "N2", 2.0 ).proj( "N2", "N1", 2.0 ).c, true, "Locked" } } } );
  paths.push_back( { "m_pacemaking", {
    { "a plain cell on 'beat'", Bench().pop( "N1" ).out( "beat", { "N1" } ).c, false, "stays silent" },
    { "a plain cell with a bias current of 380 pA", Bench().pop( "N1", "iaf_psc_alpha", 1, { { "I_e", 380.0 } } ).out( "beat", { "N1" } ).c, false, "far too fast" },
    { "the SA node cell", Bench().pop( "N1", "sa_node_pacemaker" ).out( "beat", { "N1" } ).c, true, "A pacemaker" } } } );
  paths.push_back( { "m_neuromodulation", {
    { "vagus > cell > vagal, pacemaker on beat", Bench().pop( "N1" ).pop( "N2", "sa_node_pacemaker" ).in( "vagus", "dc", 300, { "N1" } ).out( "beat", { "N2" } ).out( "vagal", {}, MODACH ).c, false, "Not wired" },
    { "the vagal output wired to the vagus cell", Bench().pop( "N1" ).pop( "N2", "sa_node_pacemaker" ).in( "vagus", "dc", 300, { "N1" } ).out( "beat", { "N2" } ).out( "vagal", { "N1" }, MODACH ).c, true, "slows the heart" } } } );
  std::map< std::string, int > runs_to_pass;
  for ( const auto& p : paths )
  {
    std::printf( "%s novice path\n", p.task.c_str() );
    int runs = 0;
    bool passed = false;
    for ( const auto& s : p.steps )
    {
      ++runs;
      // the whole path must hold on several seeds (the game's TEST x5): a step expected to pass passes on 5 seeds, a failing one fails on seed 1
      Result r1 = run_circuit( p.task, s.circuit, 1 );
      CHECK_MSG( r1.loaded, std::string( s.what ) + ": " + r1.error );
      std::printf( "  run %d (%s): %5.1f %s\n    %s\n", runs, s.what, r1.score.value, r1.score.passed ? "PASS" : "fail", r1.score.verdict.c_str() );
      CHECK_MSG( r1.score.passed == s.pass, p.task + " step '" + s.what + "' expected " + ( s.pass ? "pass" : "fail" ) + ": " + r1.score.verdict );
      if ( *s.verdict_has )
        CHECK_MSG( r1.score.verdict.find( s.verdict_has ) != std::string::npos, p.task + " step '" + s.what + "' verdict should mention '" + s.verdict_has + "': " + r1.score.verdict );
      if ( s.pass )
      {
        for ( uint64_t seed = 2; seed <= 5; ++seed )
        {
          Result r = run_circuit( p.task, s.circuit, seed );
          CHECK_MSG( r.score.passed, p.task + " step '" + s.what + "' seed " + std::to_string( seed ) + ": " + r.score.verdict );
        }
        passed = true;
        break;
      }
    }
    CHECK_MSG( passed, p.task + ": the novice path never passes" );
    CHECK_MSG( runs <= 8, p.task + ": needs " + std::to_string( runs ) + " runs" );
    if ( runs_to_pass.count( p.task ) == 0 || runs > runs_to_pass[ p.task ] ) runs_to_pass[ p.task ] = runs;
  }

  // ------------------------------------------------------------ modes scale only the tolerance
  {
    // m_threshold: a pulse weight of 700 pA puts the 0.6 blip over the rheobase (420 > 375) in standard and researcher, under it in guided (0.5 x 700 = 350)
    json b700 = Bench().pop( "N1" ).in( "pulse", "dc", 700, { "N1" } ).out( "spike", { "N1" } ).c;
    json b580 = Bench().pop( "N1" ).in( "pulse", "dc", 580, { "N1" } ).out( "spike", { "N1" } ).c;  // blip 0.6 x 580 = 348 passes, 0.72 x 580 = 418 does not
    for ( uint64_t seed = 1; seed <= 5; ++seed )
    {
      CHECK( run_circuit( "m_threshold", b700, seed, "guided" ).score.passed );
      CHECK( !run_circuit( "m_threshold", b700, seed, "standard" ).score.passed );
      CHECK( !run_circuit( "m_threshold", b700, seed, "researcher" ).score.passed );
      CHECK( run_circuit( "m_threshold", b580, seed, "guided" ).score.passed );
      CHECK( run_circuit( "m_threshold", b580, seed, "standard" ).score.passed );
      CHECK( !run_circuit( "m_threshold", b580, seed, "researcher" ).score.passed );
    }
    // m_adaptation: tau_w 200 ms gives a rate ratio of 2.9: enough for guided (2.5), not for standard (3.5)
    json a200 = Bench().pop( "N1", "aeif_cond_alpha", 1, { { "tau_w", 200.0 } } ).in( "step", "dc", 300, { "N1" } ).out( "spikes", { "N1" } ).c;
    for ( uint64_t seed = 1; seed <= 5; ++seed )
    {
      CHECK( run_circuit( "m_adaptation", a200, seed, "guided" ).score.passed );
      CHECK( !run_circuit( "m_adaptation", a200, seed, "standard" ).score.passed );
    }
    // noise: guided has none, so a repeated seed in guided mode does not depend on the seed at all for a dc puzzle with a deterministic cell
    Result g1 = run_circuit( "m_threshold", b580, 1, "guided" ), g2 = run_circuit( "m_threshold", b580, 2, "guided" );
    CHECK( g1.score.parts[ "step_spikes" ] > 0 && g2.score.parts[ "step_spikes" ] > 0 );
  }

  std::printf( "\nnovice runs to pass (a player who follows the verdicts):\n" );
  for ( auto& [ k, v ] : runs_to_pass ) std::printf( "  %-20s %d\n", k.c_str(), v );
  return chk::finish( "tasks_micro_a_test" );
}
