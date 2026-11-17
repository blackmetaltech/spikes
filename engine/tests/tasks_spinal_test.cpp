// Spinal chapter (stretch_reflex, renshaw, posture, reach): the reference circuits pass on 10 seeds, every lesion fails on 2 seeds
// with the biologically expected failure, and a run is reproducible.
//
//   tasks_spinal_test                       the CTest
//   tasks_spinal_test run <task>[=circuit.json] [seeds=1,2,3] [pre>post*factor ...]
//                                           tuning aid: run a spinal reference circuit (or a lesion of it), print the score parts.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

struct Lesion { std::string pre, post; double factor; };

static json load_circuit( const std::string& file ) { return json::parse( chk::slurp( file ) ); }

/// Scale the weight of every projection pre -> post (0 deletes it).
static void apply( json& c, const Lesion& l )
{
  bool hit = false;
  for ( auto& p : c[ "projections" ] )
  {
    if ( p[ "pre" ] != l.pre || p[ "post" ] != l.post )
      continue;
    hit = true;
    if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) )
      p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * l.factor;
    if ( p.contains( "edges" ) )
      for ( auto& e : p[ "edges" ] )
        e[ 2 ] = e[ 2 ].get< double >() * l.factor;
  }
  CHECK_MSG( hit, "lesion " + l.pre + ">" + l.post + " matched no projection" );
}

struct RunResult { Score score; std::map< std::string, double > rate_hz; double wall_s = 0; bool loaded = false; std::string error; };

static RunResult run_one( const std::string& task_id, const json& circuit_in, uint64_t seed )
{
  RunResult r;
  Engine e;
  json circuit = circuit_in;
  circuit[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );
  r.loaded = e.load( circuit.dump() );
  if ( !r.loaded )
  {
    r.error = e.error();
    return r;
  }
  auto task = make_task( task_id );
  task->reset( seed );
  std::map< std::string, long > counts;
  std::map< std::string, int > sizes;
  for ( int i = 0; i < e.neuron_count(); ++i )
    ++sizes[ e.neuron_population( i ) ];
  auto t0 = std::chrono::steady_clock::now();
  const long steps = static_cast< long >( task->duration_ms() );
  for ( long s = 0; s < steps; ++s )
  {
    task->step( e, 1.0 );
    if ( s % 500 == 499 )
      for ( auto& sp : e.drain_spikes() )
        ++counts[ e.neuron_population( sp.neuron ) ];
  }
  for ( auto& sp : e.drain_spikes() )
    ++counts[ e.neuron_population( sp.neuron ) ];
  r.wall_s = std::chrono::duration< double >( std::chrono::steady_clock::now() - t0 ).count();
  for ( auto& [ pop, n ] : sizes )
    r.rate_hz[ pop ] = counts[ pop ] / ( double )n / ( task->duration_ms() / 1000.0 );
  r.score = task->score();
  r.error = e.error();
  return r;
}

static void print( const std::string& what, uint64_t seed, const RunResult& r )
{
  if ( !r.loaded )
  {
    std::printf( "  %-28s seed %llu LOAD ERROR: %s\n", what.c_str(), ( unsigned long long )seed, r.error.c_str() );
    return;
  }
  std::printf( "  %-28s seed %llu value %5.1f %s |", what.c_str(), ( unsigned long long )seed, r.score.value, r.score.passed ? "PASS" : "fail" );
  for ( auto& [ k, v ] : r.score.parts )
    std::printf( " %s=%.3g", k.c_str(), v );
  std::printf( "  (%.1fs)\n    %s\n    rates Hz:", r.wall_s, r.score.verdict.c_str() );
  for ( auto& [ k, v ] : r.rate_hz )
    std::printf( " %s=%.1f", k.c_str(), v );
  std::printf( "\n" );
  if ( !r.error.empty() )
    std::printf( "    ENGINE ERROR: %s\n", r.error.c_str() );
}

static std::string circuits_dir() { return std::string( SPIKE_SOURCE_DIR ) + "/circuits/"; }

struct Case { std::string task; std::vector< Lesion > lesions; const char* what; bool strict = true; };  // !strict: must only score clearly below the reference (< 80)

int main( int argc, char** argv )
{
  const std::vector< std::string > tasks = { "stretch_reflex", "renshaw", "posture", "reach" };

  if ( argc >= 3 && std::string( argv[ 1 ] ) == "run" )
  {
    std::string task = argv[ 2 ], file = circuits_dir() + reference_circuit( task );
    if ( auto eq = task.find( '=' ); eq != std::string::npos )  // task=path/to/variant.json: run a variant circuit (tuning)
    {
      file = task.substr( eq + 1 );
      task = task.substr( 0, eq );
    }
    json c = load_circuit( file );
    std::vector< uint64_t > seeds;
    int i = 3;
    std::string sl = ( argc > 3 && std::string( argv[ 3 ] ).find( '>' ) == std::string::npos ) ? argv[ i++ ] : "1,2,3";
    std::stringstream ss( sl );
    for ( std::string tok; std::getline( ss, tok, ',' ); )
      seeds.push_back( std::stoull( tok ) );
    for ( ; i < argc; ++i )
    {
      std::string a = argv[ i ];
      auto gt = a.find( '>' ), st = a.find( '*' );
      apply( c, { a.substr( 0, gt ), a.substr( gt + 1, st - gt - 1 ), std::stod( a.substr( st + 1 ) ) } );
    }
    for ( auto s : seeds )
      print( task, s, run_one( task, c, s ) );
    return 0;
  }

  // ---- registry: the four tasks exist, are in the spinal chapter, in order, with a timeline, bands and a par ----
  auto ids = task_ids();
  int last_order = -1;
  for ( const auto& id : tasks )
  {
    bool found = false;
    for ( size_t k = 0; k < ids.size(); ++k )
      if ( ids[ k ] == id )
      {
        found = true;
        CHECK_MSG( static_cast< int >( k ) > last_order, id + " is out of campaign order" );
        last_order = static_cast< int >( k );
      }
    CHECK_MSG( found, id + " is not registered" );
    if ( !found )
      continue;
    auto t = make_task( id );
    CHECK( t->chapter() == "spinal" );
    CHECK( t->view() == "limb" );
    CHECK( t->par_neurons() > 0 );
    CHECK( !t->timeline().empty() );
    CHECK( !t->bands().empty() );
    CHECK( !t->brief().empty() );
    CHECK( reference_circuit( id ) == id + ".json" );
  }

  // ---- reference circuits pass on ten seeds ----
  for ( const auto& id : tasks )
  {
    json c = load_circuit( circuits_dir() + reference_circuit( id ) );
    auto t = make_task( id );
    std::map< std::string, bool > in_ch, out_ch;
    for ( auto& i : c[ "inputs" ] ) in_ch[ i[ "name" ] ] = true;
    for ( auto& o : c[ "outputs" ] ) out_ch[ o[ "name" ] ] = true;
    for ( auto& s : t->sensor_channels() ) CHECK_MSG( in_ch.count( s ), id + " input " + s );
    for ( auto& m : t->motor_channels() ) CHECK_MSG( out_ch.count( m ), id + " output " + m );
    int neurons = 0;
    for ( auto& p : c[ "populations" ] ) neurons += p[ "n" ].get< int >();
    CHECK_MSG( neurons <= 60, id + " has " + std::to_string( neurons ) + " neurons" );
    CHECK_MSG( neurons <= t->par_neurons() + 6 && neurons + 6 >= t->par_neurons(), id + " par_neurons " + std::to_string( t->par_neurons() ) + " vs " + std::to_string( neurons ) );
    std::printf( "%s (%d neurons)\n", id.c_str(), neurons );
    double sum = 0;
    for ( uint64_t seed : { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 } )
    {
      RunResult r = run_one( id, c, seed );
      print( "reference", seed, r );
      CHECK_MSG( r.loaded, r.error );
      CHECK_MSG( r.score.passed, id + " seed " + std::to_string( seed ) + ": " + r.score.verdict );
      CHECK( r.score.value >= 50.0 );
      CHECK( !r.score.verdict.empty() );
      CHECK( r.error.empty() );
      sum += r.score.value;
    }
    std::printf( "  mean score %.1f\n", sum / 10 );
    if ( id == "stretch_reflex" )  // reproducible: the same seed gives the same score
    {
      RunResult a = run_one( id, c, 2 ), b = run_one( id, c, 2 );
      CHECK( a.score.value == b.score.value );
      CHECK( a.score.parts == b.score.parts );
    }
  }

  // ---- lesions: removing the key mechanism must fail the task ----
  const std::vector< Case > cases = {
    // stretch reflex
    { "stretch_reflex", { { "Ia_F", "MN_F", 0.0 }, { "Ia_E", "MN_E", 0.0 } }, "Ia afferents cut from their motoneurons (no reflex)" },
    { "stretch_reflex", { { "Ia_F", "IN_F", 0.0 }, { "Ia_E", "IN_E", 0.0 } }, "Ia afferents cut from the Ia inhibitory interneurons (no reciprocal inhibition: co-contraction, stiff, weak reflex)" },
    { "stretch_reflex", { { "IN_F", "MN_E", 0.0 }, { "IN_E", "MN_F", 0.0 } }, "Ia inhibitory interneurons cut from the antagonist motoneurons" },
    { "stretch_reflex", { { "Ia_F", "MN_F", 4.0 }, { "Ia_E", "MN_E", 4.0 } }, "Ia -> motoneuron gain x4 (a model ceiling, not clonus: the motoneuron pools saturate against the muscle's full-excitation rate and the reflex is lost)" },
    // Renshaw
    { "renshaw", { { "MN_F", "RC_F", 0.0 }, { "MN_E", "RC_E", 0.0 } }, "motoneuron collaterals cut (no Renshaw cells driven: tremor is that of an unbraked cord)" },
    { "renshaw", { { "RC_F", "MN_F", 0.0 }, { "RC_E", "MN_E", 0.0 } }, "Renshaw cells fire but cannot inhibit the motoneurons" },
    { "renshaw", { { "DESC", "MN_F", 0.25 }, { "DESC", "MN_E", 0.25 }, { "MN_F", "RC_F", 0.0 }, { "MN_E", "RC_E", 0.0 } }, "v0.3: pools shielded from the common noise and no Renshaw cells (cannot pass by ignoring the drive: the volley test measures the noise path on the circuit itself)" },
    { "renshaw", { { "DESC", "MN_F", 0.5 }, { "DESC", "MN_E", 0.5 }, { "MN_F", "RC_F", 0.0 }, { "MN_E", "RC_E", 0.0 } }, "v0.3: half the common input and no Renshaw cells (tremor falls only because the input is smaller: no better than its own control)" },
    // posture
    { "posture", { { "Ia_F", "MN_F", 0.0 }, { "Ia_E", "MN_E", 0.0 }, { "II_F", "MN_F", 0.0 }, { "II_E", "MN_E", 0.0 } }, "spindle afferents cut (no stretch reflex: the limb has no feedback)" },
    { "posture", { { "TONIC", "GAM", 0.0 } }, "no gamma drive (spindles go slack, the length servo loses its sensor: a weaker, seed-dependent failure, so it must only score below 80 where the reference scores about 90)", false },
    { "posture", { { "TONIC", "MN_F", 0.0 } }, "no descending tone to the antigravity muscle (the limb falls)" },
    // reach
    { "reach", { { "B1", "MN_F", 0.0 } }, "first agonist burst cannot drive the flexor pool (no acceleration)" },
    { "reach", { { "B2", "MN_E", 0.0 }, { "B2", "MN_F", 0.0 } }, "antagonist burst removed (no braking: overshoot)" },
    // cutting GO alone leaves the equilibrium-point hold command, which drags the arm to the target slowly (it passes on some seeds): cut the hold instead
    { "reach", { { "TGT", "MN_F", 0.0 }, { "TGT", "MN_E", 0.0 } }, "target hold command cut (bursts only: the arm is not held on the target)" },
  };
  for ( const auto& cs : cases )
  {
    json c = load_circuit( circuits_dir() + reference_circuit( cs.task ) );
    for ( const auto& l : cs.lesions )
      apply( c, l );
    std::printf( "%s lesion: %s\n", cs.task.c_str(), cs.what );
    for ( uint64_t seed : { 1, 2 } )
    {
      RunResult r = run_one( cs.task, c, seed );
      print( "lesioned", seed, r );
      CHECK_MSG( r.loaded, r.error );
      if ( cs.strict )
        CHECK_MSG( !r.score.passed, std::string( cs.task ) + " still passes with lesion: " + cs.what );
      else
        CHECK_MSG( r.score.value < 80.0, std::string( cs.task ) + " scores " + std::to_string( r.score.value ) + " with lesion: " + cs.what );
    }
  }
  return chk::finish( "tasks_spinal_test" );
}
