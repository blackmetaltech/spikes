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
//   * mechanism: every level checks the mechanism it teaches through Engine::structure() (docs/REQ-0.4.md rule 1); the degenerate circuits that lack it live in
//     tasks_cheap_test (cheap_rows_a.inc), the lesions below remove one part of the reference at a time;
//   * no numbers: no verdict carries a number and hint tiers 1-2 carry none (tier 3 no number with a unit); only recipe() has them (rule 2);
//   * the novice (rule 4, non-circular): a scripted player that reads only the brief's keywords and the verdict's words, starts from the lab's defaults and
//     searches coarsely in at most 8 runs; the levels it cannot solve are listed (kNoviceUnsolved) and any change in that list fails the test;
//   * the modes scale only the tolerance: a marginal bench passes in guided and fails in standard (or passes in standard and fails in researcher).
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include "check.h"
#include "nlohmann/json.hpp"
#include "micro_lab.h"
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

// every verdict and hint a run produces, for the "no numbers" scan at the end
std::set< std::pair< std::string, std::string > > g_verdicts;
std::set< std::tuple< std::string, int, std::string > > g_hints;

// The adaptive cell's "Adapting" preset, which the half-centre verdict names.
json ADAPT_PRESET() { return { { "a", 0.0 }, { "b", 60.0 }, { "tau_w", 100.0 }, { "V_reset", -58.0 } }; }

// Levels the scripted novice (verdict words only, <= 8 runs) cannot solve: filled in from the table the test prints, so that a change in either
// direction is noticed (see "the novice" below).
const std::set< std::string > kNoviceUnsolved = {};

Result run_circuit( const std::string& id, const json& circuit_in, uint64_t seed, const std::string& mode = "standard", bool quick = false )
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
  const long steps = static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() );
  for ( long s = 0; s < steps; ++s )
    t->step( e, 1.0 );
  r.score = t->score();
  r.metrics = t->metrics();
  r.hints = t->hints();
  r.error = e.error();
  g_verdicts.insert( { id, r.score.verdict } );
  for ( const auto& h : r.hints ) g_hints.insert( { id, h.tier, h.text } );
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
  // The lab's own first build: the cell the parts picker opens the level on (Task::default_model()), with the preset it carries.
  std::string task_id;
  Bench() = default;
  explicit Bench( const std::string& task ) : task_id( task ) {}
  microlab::Part dflt() const { return microlab::part_of( make_task( task_id )->default_model() ); }
  Bench& cell( const std::string& id, int n = 1, json extra = json::object() )  // a default-part cell (extra: parameters the player changes)
  {
    auto p = dflt();
    for ( auto it = extra.begin(); it != extra.end(); ++it ) p.params[ it.key() ] = it.value();
    return pop( id, p.model, n, p.params );
  }
  // a sensor with the weight of the lab's first connection (the task's declared weight, else 300 pA per unit dc)
  Bench& sense( const std::string& name, std::vector< std::string > pops, double weight = -1 )
  {
    auto t = make_task( task_id );
    auto k = t->channel_kinds()[ name ];
    return in( name, k.kind, weight >= 0 ? weight : microlab::sensor_weight( *t, name, dflt() ), std::move( pops ) );
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
    // the lab opens the level on this part; the reference circuit is built of the same cell type, and the recipe lives apart from the brief
    CHECK_MSG( !t->default_model().empty(), id + " declares default_model" );
    CHECK_MSG( t->recipe().size() > 60, id + " has a recipe (concrete numbers, kept out of the brief)" );
    CHECK_MSG( t->brief().find( " pA" ) == std::string::npos && t->brief().find( " nS" ) == std::string::npos, id + ": the brief carries numbers that belong to recipe()" );
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
    if ( id == "m_pacemaking" )  // the lab opens on a plain cell: finding the pacemaker card in the parts bin is the puzzle
      CHECK_MSG( ref[ "populations" ][ 0 ][ "model" ] == "sa_node_pacemaker" && microlab::part_of( t->default_model() ).model == "iaf_psc_alpha", id + ": the lab must open on a plain cell, the reference is the SA node cell" );
    else
      CHECK_MSG( ref[ "populations" ][ 0 ][ "model" ] == microlab::part_of( t->default_model() ).model, id + ": default_model " + t->default_model() + " is the reference's first cell type" );
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

  // ------------------------------------------------------------ the novice: non-circular (docs/REQ-0.4.md rule 4)
  // The scripted player may read ONLY the brief (keywords, to decide the structure the brief describes) and the words of the verdict (to decide which
  // knob to move and which way). It never sees recipe(), hint tier 3 or a reference circuit, and the verdicts carry no numbers. Every build starts from
  // the lab's defaults (the part the lab opens on, its default synapse weight, the first-connect weight of each sensor). Its search is coarse and
  // bounded: a knob moves by a fixed factor (x3 for a weight that has to grow by orders of magnitude, x1.4 otherwise, x2 for a time constant) while the
  // verdict keeps saying the same thing, and by bisection once two verdicts have bracketed it. At most 8 runs; a pass on seed 1 is then confirmed on
  // seeds 2-5 (the game's TEST x5). Levels it cannot solve in 8 runs are listed, not hidden.
  auto lc = []( std::string x ) { for ( auto& ch : x ) ch = static_cast< char >( std::tolower( static_cast< unsigned char >( ch ) ) ); return x; };
  auto says = [ & ]( const std::string& v, const char* w ) { return lc( v ).find( lc( w ) ) != std::string::npos; };
  struct Knob
  {
    double v = 1, lo = 0, hi = 0, cap = 1e18;
    bool has_lo = false, has_hi = false;
    void up( double f )
    {
      has_lo = true;
      lo = v;
      v = has_hi ? std::sqrt( lo * hi ) : std::min( cap, v * f );
    }
    void down( double f )
    {
      has_hi = true;
      hi = v;
      v = has_lo ? std::sqrt( lo * hi ) : v / f;
    }
  };
  struct State
  {
    std::map< std::string, Knob > k;
    std::set< std::string > f;
    double operator()( const std::string& n ) const { return k.at( n ).v; }
    Knob& knob( const std::string& n, double v0, double cap = 1e18 ) { auto& x = k[ n ]; x.v = v0; x.cap = cap; return x; }
  };
  struct Play
  {
    int runs = 0;
    bool solved = false;
    std::string last;
  };
  // the brief must describe what the novice builds: each keyword is looked up in the brief, so a brief that stops saying it breaks the novice
  auto reads = [ & ]( const std::string& task, std::initializer_list< const char* > words ) {
    const std::string b = make_task( task )->brief();
    for ( const char* w : words ) CHECK_MSG( says( b, w ), task + ": the brief no longer says '" + w + "', which the novice builds from" );
  };
  auto play = [ & ]( const std::string& task, State st, std::function< json( const State& ) > build, std::function< void( State&, const std::string& ) > react ) {
    Play pl;
    std::printf( "%s novice (lab defaults, coarse search)\n", task.c_str() );
    for ( int run = 1; run <= 8 && !pl.solved; ++run )
    {
      pl.runs = run;
      const json c = build( st );
      Result r1 = run_circuit( task, c, 1, "standard", true );  // CHECK runs the quick episode
      CHECK_MSG( r1.loaded, task + " novice build does not load: " + r1.error );
      if ( !r1.loaded ) break;
      std::string verdict = r1.score.verdict;
      bool pass = r1.score.passed;
      for ( uint64_t seed = 2; pass && seed <= 5; ++seed )
      {
        Result r = run_circuit( task, c, seed, "standard", true );
        if ( !r.score.passed ) { pass = false; verdict = r.score.verdict; }
      }
      std::printf( "  run %d: %5.1f %s  %s\n", run, r1.score.value, pass ? "PASS (5 seeds)" : "fail", verdict.c_str() );
      pl.last = verdict;
      if ( pass ) { pl.solved = true; break; }
      react( st, verdict );
    }
    return pl;
  };
  std::map< std::string, Play > novice;
  const json MODACH = { { "modulator", "ACh" }, { "gain", 0.012 }, { "tau_ms", 500.0 } };
  auto lab_w = []( const char* task ) { return microlab::part_of( make_task( task )->default_model() ).weight; };

  {  // m_threshold: one cell, one knob (the pulse weight): the verdict says whether the step or the blip is the problem
    reads( "m_threshold", { "weight", "pulse", "silent", "step" } );
    State s;
    s.knob( "w", microlab::sensor_weight( *make_task( "m_threshold" ), "pulse", microlab::part_of( make_task( "m_threshold" )->default_model() ) ) );
    novice[ "m_threshold" ] = play( "m_threshold", s,
      []( const State& s ) { return Bench( "m_threshold" ).cell( "N1" ).sense( "pulse", { "N1" }, s( "w" ) ).out( "spike", { "N1" } ).c; },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "strengthen" ) ) s.k[ "w" ].up( 1.4 );
        else if ( says( v, "weaken" ) ) s.k[ "w" ].down( 1.4 );
      } );
  }
  {  // m_excitation: two cells, an excitatory link with a weight and a delay
    reads( "m_excitation", { "second cell", "weight", "delay", "echo" } );
    State s;
    s.knob( "w", lab_w( "m_excitation" ) );
    s.knob( "d", 2.0, 20.0 );
    novice[ "m_excitation" ] = play( "m_excitation", s,
      []( const State& s ) {
        return Bench( "m_excitation" ).cell( "N1" ).cell( "N2" ).sense( "tap", { "N1" } ).out( "source", { "N1" } ).out( "echo", { "N2" } ).proj( "N1", "N2", s( "w" ), s( "d" ) ).c;
      },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "delay" ) ) { if ( says( v, "lengthen" ) ) s.k[ "d" ].up( 1.5 ); else s.k[ "d" ].down( 1.5 ); }
        else if ( says( v, "strengthen the link" ) ) s.k[ "w" ].up( 3 );
      } );
  }
  {  // m_convergence: three sensors onto one cell
    reads( "m_convergence", { "in_a", "in_b", "in_c", "neuron", "same time" } );
    State s;
    s.knob( "w", microlab::sensor_weight( *make_task( "m_convergence" ), "in_a", microlab::part_of( make_task( "m_convergence" )->default_model() ) ) );
    novice[ "m_convergence" ] = play( "m_convergence", s,
      []( const State& s ) { return Bench( "m_convergence" ).cell( "N1" ).sense( "in_a", { "N1" }, s( "w" ) ).sense( "in_b", { "N1" }, s( "w" ) ).sense( "in_c", { "N1" }, s( "w" ) ).out( "out", { "N1" } ).c; },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "weaken" ) ) s.k[ "w" ].down( 1.4 );
        else if ( says( v, "strengthen" ) ) s.k[ "w" ].up( 1.4 );
      } );
  }
  {  // m_inhibition: stop > an inhibitory cell > the cell that fires
    reads( "m_inhibition", { "inhibitory", "neuron of its own", "stop" } );
    State s;
    s.knob( "w", lab_w( "m_inhibition" ) );
    novice[ "m_inhibition" ] = play( "m_inhibition", s,
      []( const State& s ) { return Bench( "m_inhibition" ).cell( "N1" ).cell( "N2" ).sense( "drive", { "N1" } ).sense( "stop", { "N2" } ).out( "out", { "N1" } ).proj( "N2", "N1", -s( "w" ) ).c; },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "weaken" ) ) s.k[ "w" ].down( 1.4 );
        else if ( says( v, "strengthen" ) ) s.k[ "w" ].up( 3 );
      } );
  }
  {  // m_subtraction: plus excites the output cell, minus goes through an inhibitory cell onto the same output cell
    reads( "m_subtraction", { "excitation from plus", "inhibition from minus", "output cell" } );
    State s;
    s.knob( "w", lab_w( "m_subtraction" ) );
    novice[ "m_subtraction" ] = play( "m_subtraction", s,
      []( const State& s ) { return Bench( "m_subtraction" ).cell( "N1" ).cell( "N2" ).sense( "plus", { "N1" } ).sense( "minus", { "N2" } ).out( "diff", { "N1" } ).proj( "N2", "N1", -s( "w" ) ).c; },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "weaken" ) ) s.k[ "w" ].down( 1.4 );
        else if ( says( v, "stronger" ) ) s.k[ "w" ].up( 3 );
      } );
  }
  {  // m_adaptation: the adaptive cell the brief names (the lab opens on it); the knobs the brief names: b and tau_w
    reads( "m_adaptation", { "adaptive exponential", "tau_w", "step" } );
    State s;
    s.knob( "tau_w", 144.0, 400.0 );  // the part's own value, and the slider's top
    s.knob( "step", microlab::sensor_weight( *make_task( "m_adaptation" ), "step", microlab::part_of( make_task( "m_adaptation" )->default_model() ) ) );
    novice[ "m_adaptation" ] = play( "m_adaptation", s,
      []( const State& s ) {
        json extra = json::object();
        if ( s.f.count( "touched_tau" ) ) extra[ "tau_w" ] = s( "tau_w" );
        return Bench( "m_adaptation" ).cell( "N1", 1, extra ).sense( "step", { "N1" }, s( "step" ) ).out( "spikes", { "N1" } ).c;
      },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "strengthen the step" ) ) s.k[ "step" ].up( 1.4 );
        else if ( says( v, "lower the adaptation" ) ) { s.f.insert( "touched_tau" ); s.k[ "tau_w" ].down( 1.4 ); }
        else if ( says( v, "tau_w" ) ) { s.f.insert( "touched_tau" ); s.k[ "tau_w" ].up( 2 ); }
      } );
  }
  {  // m_half_centre: two cells, each inhibiting the other, the same drive on both
    reads( "m_half_centre", { "inhibit each other", "drive", "left", "right", "wear" } );
    State s;
    s.knob( "drive", microlab::sensor_weight( *make_task( "m_half_centre" ), "drive", microlab::part_of( make_task( "m_half_centre" )->default_model() ) ) );
    s.knob( "link", lab_w( "m_half_centre" ) );
    novice[ "m_half_centre" ] = play( "m_half_centre", s,
      []( const State& s ) {
        const json extra = s.f.count( "adapting" ) ? ADAPT_PRESET() : json::object();
        return Bench( "m_half_centre" ).cell( "N1", 1, extra ).cell( "N2", 1, extra ).sense( "drive", { "N1", "N2" }, s( "drive" ) ).out( "left", { "N1" } ).out( "right", { "N2" } )
          .proj( "N1", "N2", -s( "link" ) ).proj( "N2", "N1", -s( "link" ) ).c;
      },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "adapting preset" ) && !s.f.count( "adapting" ) ) s.f.insert( "adapting" );
        else if ( says( v, "strengthen the drive" ) ) s.k[ "drive" ].up( 1.4 );
        else if ( says( v, "strengthen the links" ) ) s.k[ "link" ].up( 2 );
        else if ( says( v, "weaker inhibition" ) ) s.k[ "link" ].down( 2 );
      } );
  }
  {  // m_pacemaking: the brief says to look in the parts bin for a pacemaker cell; the lab opens on a plain cell, so the first build is that
    reads( "m_pacemaking", { "pacemaker", "parts bin", "beat" } );
    State s;
    novice[ "m_pacemaking" ] = play( "m_pacemaking", s,
      []( const State& s ) {  // the verdict says "choose a cell from the parts bin described as a pacemaker": the card is the SA node
        return s.f.count( "sa" ) ? Bench().pop( "N1", "sa_node_pacemaker" ).out( "beat", { "N1" } ).c : Bench( "m_pacemaking" ).cell( "N1" ).out( "beat", { "N1" } ).c;
      },
      [ & ]( State& s, const std::string& v ) { if ( says( v, "pacemaker" ) ) s.f.insert( "sa" ); } );
  }
  {  // m_coupling: two pacemaker cells (the lab opens on them), one on each channel, linked both ways by excitatory synapses
    reads( "m_coupling", { "one cell for each channel", "couple", "excitatory synapses" } );
    State s;
    s.knob( "w", lab_w( "m_coupling" ) );
    novice[ "m_coupling" ] = play( "m_coupling", s,
      []( const State& s ) { return Bench( "m_coupling" ).cell( "N1" ).cell( "N2" ).out( "a", { "N1" } ).out( "b", { "N2" } ).proj( "N1", "N2", s( "w" ) ).proj( "N2", "N1", s( "w" ) ).c; },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "stronger" ) || says( v, "strengthen" ) ) s.k[ "w" ].up( 3 );
      } );
  }
  {  // m_neuromodulation: vagus > a cell > 'vagal' (a modulator output), a pacemaker on 'beat', nothing between them
    reads( "m_neuromodulation", { "vagus", "vagal", "pacemaker cell on 'beat'" } );
    State s;
    s.knob( "n", 1.0, 64.0 );
    novice[ "m_neuromodulation" ] = play( "m_neuromodulation", s,
      [ & ]( const State& s ) {
        return Bench( "m_neuromodulation" ).cell( "N1", static_cast< int >( std::lround( s( "n" ) ) ) ).pop( "N2", "sa_node_pacemaker" ).sense( "vagus", { "N1" } ).out( "beat", { "N2" } ).out( "vagal", { "N1" }, MODACH ).c;
      },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "too much" ) || says( v, "fewer cells" ) ) s.k[ "n" ].down( 2 );
        else if ( says( v, "more cells" ) ) s.k[ "n" ].up( 2 );
      } );
  }
  // the three big levels: the brief describes the structure (convergence / ON minus OFF / crossed inhibition); the novice picks group sizes of "several"
  {
    reads( "reflex", { "several receptor cells", "motor cells", "convergence" } );
    State s;
    s.knob( "w", lab_w( "reflex" ) );
    novice[ "reflex" ] = play( "reflex", s,
      []( const State& s ) {
        return Bench( "reflex" ).cell( "SN", 6 ).cell( "MN", 4 ).sense( "touch", { "SN" } ).out( "withdraw", { "MN" } ).proj( "SN", "MN", s( "w" ) ).c;
      },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "too jumpy" ) || says( v, "false" ) || says( v, "fired on noise" ) ) s.k[ "w" ].down( 1.4 );
        else s.k[ "w" ].up( 1.4 );
      } );
  }
  {
    reads( "worm", { "ASER to excite turn_left", "ASEL to inhibit it", "inhibition" } );
    State s;
    s.knob( "w", lab_w( "worm" ) );
    novice[ "worm" ] = play( "worm", s,
      []( const State& s ) {
        return Bench( "worm" ).cell( "A", 4 ).cell( "B", 4 ).cell( "TL", 4 ).cell( "TR", 1 ).sense( "ASER", { "A" } ).sense( "ASEL", { "B" } ).out( "turn_left", { "TL" } ).out( "turn_right", { "TR" } )
          .proj( "A", "TL", s( "w" ) ).proj( "B", "TL", -s( "w" ) ).c;
      },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "never fired" ) || says( v, "strong enough" ) || says( v, "too late" ) ) s.k[ "w" ].up( 2 );
        else s.k[ "w" ].up( 1.4 );
      } );
  }
  {
    reads( "lamprey", { "mutual inhibition", "left", "right", "drive" } );
    State s;
    s.knob( "drive", microlab::sensor_weight( *make_task( "lamprey" ), "drive", microlab::part_of( make_task( "lamprey" )->default_model() ) ) );
    s.knob( "link", lab_w( "lamprey" ) );
    novice[ "lamprey" ] = play( "lamprey", s,
      []( const State& s ) {
        const json extra = s.f.count( "adapting" ) ? ADAPT_PRESET() : json::object();
        return Bench( "lamprey" ).cell( "L", 4, extra ).cell( "R", 4, extra ).sense( "drive", { "L", "R" }, s( "drive" ) ).out( "left_motor", { "L" } ).out( "right_motor", { "R" } )
          .proj( "L", "R", -s( "link" ) ).proj( "R", "L", -s( "link" ) ).c;
      },
      [ & ]( State& s, const std::string& v ) {
        if ( says( v, "silent" ) ) s.k[ "drive" ].up( 1.4 );
        else if ( says( v, "overlap" ) || says( v, "not alternating" ) ) { if ( !s.f.count( "adapting" ) ) s.f.insert( "adapting" ); else s.k[ "link" ].up( 2 ); }
        else if ( says( v, "too slowly" ) ) s.k[ "drive" ].up( 1.4 );
        else s.f.insert( "adapting" );
      } );
  }

  std::printf( "\nnovice (lab defaults, verdict words only, at most 8 runs, 5 seeds to pass):\n" );
  std::set< std::string > unsolved;
  for ( auto& [ k, pl ] : novice )
  {
    std::printf( "  %-20s %s after %d run%s\n", k.c_str(), pl.solved ? "solved" : "NOT SOLVED", pl.runs, pl.runs == 1 ? "" : "s" );
    if ( !pl.solved ) unsolved.insert( k );
  }
  // the honest list: levels this novice cannot solve are named here, so that a change that makes one solvable (or one unsolvable) shows up
  const std::set< std::string >& expected_unsolved = kNoviceUnsolved;
  for ( auto& k : unsolved ) CHECK_MSG( expected_unsolved.count( k ), "the novice used to solve " + k + " and no longer does: " + novice[ k ].last );
  for ( auto& k : expected_unsolved ) CHECK_MSG( unsolved.count( k ), "the novice now solves " + k + ": update NOVICE_EXPECTED_UNSOLVED" );

  // ------------------------------------------------------------ no numbers in verdicts or hint tiers 1-2, no unit numbers in tier 3
  {
    auto digits = []( std::string v ) {
      for ( const char* ok : { "plus 3", "minus 0", "minus 2", "minus 3", "plus 2", "plus 0", "against minus", "5 seeds" } )
        for ( size_t p; ( p = v.find( ok ) ) != std::string::npos; ) v.erase( p, std::strlen( ok ) );
      for ( char ch : v ) if ( std::isdigit( static_cast< unsigned char >( ch ) ) ) return true;
      return false;
    };
    int n = 0;
    for ( const auto& [ id, v ] : g_verdicts )
    {
      ++n;
      CHECK_MSG( !digits( v ), id + ": a verdict carries a number: " + v );
    }
    for ( const auto& [ id, tier, text ] : g_hints )
    {
      if ( tier <= 2 ) CHECK_MSG( !digits( text ), id + ": hint tier " + std::to_string( tier ) + " carries a number: " + text );
      else
        for ( const char* u : { " pA", " nS", " ms", " Hz", "per minute" } )
          for ( size_t p = text.find( u ); p != std::string::npos; p = text.find( u, p + 1 ) )
            CHECK_MSG( p == 0 || !std::isdigit( static_cast< unsigned char >( text[ p - 1 ] ) ), id + ": hint tier 3 gives a number with a unit: " + text );
    }
    std::printf( "\nscanned %d distinct verdicts and %zu hints for numbers\n", n, g_hints.size() );
  }

  // ------------------------------------------------------------ modes scale only the tolerance
  {
    // m_threshold at 640 pA of pulse weight: the blip (0.5 +- 0.03 guided) stays under the rheobase in guided, crosses it on some seeds in standard
    // (0.6 +- 0.05) and on every seed in researcher (0.72 +- 0.05)
    json b640 = Bench().pop( "N1" ).in( "pulse", "dc", 640, { "N1" } ).out( "spike", { "N1" } ).c;
    int pg = 0, ps = 0, pr = 0;
    for ( uint64_t seed = 1; seed <= 5; ++seed )
    {
      pg += run_circuit( "m_threshold", b640, seed, "guided" ).score.passed;
      ps += run_circuit( "m_threshold", b640, seed, "standard" ).score.passed;
      pr += run_circuit( "m_threshold", b640, seed, "researcher" ).score.passed;
    }
    CHECK_MSG( pg == 5 && ps < 5 && pr == 0, "m_threshold at 640: guided " + std::to_string( pg ) + "/5, standard " + std::to_string( ps ) + "/5, researcher " + std::to_string( pr ) + "/5" );
    // the stimulus differs from seed to seed (so the pass band is an intersection, not a number to memorise): the same weight passes some seeds and not others
    json b520 = Bench().pop( "N1" ).in( "pulse", "dc", 520, { "N1" } ).out( "spike", { "N1" } ).c;
    int p520 = 0;
    for ( uint64_t seed = 1; seed <= 12; ++seed ) p520 += run_circuit( "m_threshold", b520, seed ).score.passed;
    CHECK_MSG( p520 > 0 && p520 < 12, "m_threshold at 520 should pass on some seeds and fail on others, passed " + std::to_string( p520 ) + "/12" );
    // m_adaptation: tau_w 200 ms gives a rate ratio of 2.9: enough for guided (2.5), not for standard (3.5)
    json a200 = Bench().pop( "N1", "aeif_cond_alpha", 1, { { "tau_w", 200.0 } } ).in( "step", "dc", 300, { "N1" } ).out( "spikes", { "N1" } ).c;
    for ( uint64_t seed = 1; seed <= 5; ++seed )
    {
      CHECK( run_circuit( "m_adaptation", a200, seed, "guided" ).score.passed );
      CHECK( !run_circuit( "m_adaptation", a200, seed, "standard" ).score.passed );
    }
  }

  // ------------------------------------------------------------ CHECK time: the worm's quick episode scores the same pass as the full one
  {
    auto t = make_task( "worm" );
    CHECK( t->quick_duration_ms() < t->duration_ms() );
    json ref_c = load_circuit( "worm" );
    json weak = ref_c;
    for ( auto& p : weak[ "projections" ] ) p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * 0.1;
    for ( uint64_t seed = 1; seed <= 3; ++seed )
      for ( const json* c : { &ref_c, &weak } )
      {
        Result q = run_circuit( "worm", *c, seed, "standard", true ), f = run_circuit( "worm", *c, seed, "standard", false );
        CHECK_MSG( q.score.passed == f.score.passed, "worm seed " + std::to_string( seed ) + ": quick " + ( q.score.passed ? "pass" : "fail" ) + " but full " + ( f.score.passed ? "pass" : "fail" ) );
      }
    std::printf( "worm quick episode %.1f s of %.0f s\n", t->quick_duration_ms() / 1000, t->duration_ms() / 1000 );
  }

  return chk::finish( "tasks_micro_a_test" );
}
