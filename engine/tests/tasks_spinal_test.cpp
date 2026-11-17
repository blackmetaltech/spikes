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


// ---- novice benches: built the way the lab builds them (default part iaf_psc_alpha, default delay 2 ms, all_to_all wiring, only the numbers of the
// task's recipe()) so the recipe, the hints and the verdicts are checked against what a player can actually draw ----
struct Bench
{
  json c = { { "schema_version", 1 }, { "name", "novice bench" }, { "resolution_ms", 0.1 }, { "seed", 1 }, { "populations", json::array() },
    { "projections", json::array() }, { "inputs", json::array() }, { "outputs", json::array() }, { "record", { { "spikes", "all" } } } };
  Bench& pop( const std::string& id, int n, json params = json::object(), const std::string& model = "iaf_psc_alpha" )
  {
    c[ "populations" ].push_back( { { "id", id }, { "model", model }, { "n", n }, { "params", params } } );
    return *this;
  }
  Bench& proj( const std::string& pre, const std::string& post, double w, double delay = 2.0, double cv = 0.0 )
  {
    json p = { { "pre", pre }, { "post", post }, { "rule", "all_to_all" }, { "synapse", { { "synapse_model", "static_synapse" }, { "weight", w }, { "delay", delay } } } };
    if ( cv > 0 )  // the lab's spread slider writes the all-to-all pairs as `edges` with a weight_cv on the projection
    {
      int npre = 0, npost = 0;
      for ( auto& q : c[ "populations" ] )
      {
        if ( q[ "id" ] == pre ) npre = q[ "n" ];
        if ( q[ "id" ] == post ) npost = q[ "n" ];
      }
      json edges = json::array();
      for ( int i = 0; i < npre; ++i )
        for ( int j = 0; j < npost; ++j )
          edges.push_back( { i, j, w, delay } );
      p = { { "pre", pre }, { "post", post }, { "edges", edges }, { "weight_cv", cv } };
    }
    c[ "projections" ].push_back( p );
    return *this;
  }
  Bench& in( const std::string& name, const std::string& kind, double weight, std::vector< std::string > pops )
  {
    json t = json::array();
    for ( auto& p : pops )
      t.push_back( { { "pop", p } } );
    c[ "inputs" ].push_back( { { "name", name }, { "kind", kind }, { "targets", t }, { "weight", weight }, { "value", 0.0 } } );
    return *this;
  }
  Bench& out( const std::string& name, std::vector< std::string > pops )
  {
    json n = json::array();
    for ( auto& p : pops )
      n.push_back( { { "pop", p } } );
    c[ "outputs" ].push_back( { { "name", name }, { "kind", "rate" }, { "neurons", n } } );
    return *this;
  }
};

/// The stretch-reflex part of every spinal recipe (stretch_reflex' own recipe: the drive weight is a parameter).
static Bench reflex_bench( double drive, double inhibition = 320 )
{
  Bench b;
  
  const double cv = 0.3, mn_w = 240;  // the recipe: Ia > MN 240 with a weight spread (CV) of 0.3
  const json slow = { { "tau_m", 60.0 } };
  b.pop( "IaF", 6, slow ).pop( "IaE", 6, slow ).pop( "MNF", 20 ).pop( "MNE", 20 ).pop( "INF", 4 ).pop( "INE", 4 );
  b.in( "ag_Ia", "dc", 3, { "IaF" } ).in( "ant_Ia", "dc", 3, { "IaE" } ).in( "drive", "dc", drive, { "MNF", "MNE" } );
  b.proj( "IaF", "MNF", mn_w, 2, cv ).proj( "IaE", "MNE", mn_w, 2, cv ).proj( "IaF", "INF", 240 ).proj( "IaE", "INE", 240 ).proj( "INF", "MNE", -inhibition ).proj( "INE", "MNF", -inhibition );
  b.out( "ag_motor", { "MNF" } ).out( "ant_motor", { "MNE" } );
  return b;
}

static json novice_bench( const std::string& task )
{
  if ( task == "stretch_reflex" )
    return reflex_bench( 320 ).c;
  if ( task == "renshaw" )
  {
    Bench b = reflex_bench( 150, 500 );
    b.pop( "DESC", 2, json::object(), "parrot_neuron" ).pop( "RF", 4 ).pop( "RE", 4 );
    b.in( "noise", "poisson", 100, { "DESC" } );
    b.proj( "DESC", "MNF", 30 ).proj( "DESC", "MNE", 30 ).proj( "MNF", "RF", 80 ).proj( "MNE", "RE", 80 ).proj( "RF", "MNF", -160 ).proj( "RE", "MNE", -160 )
        .proj( "RF", "INF", -40 ).proj( "RE", "INE", -40 ).proj( "RF", "RE", -160 ).proj( "RE", "RF", -160 );
    return b.c;
  }
  if ( task == "posture" )
  {
    const double cv = 0, pw = 150;
    Bench b;
    const json slow = { { "tau_m", 60.0 } };
    b.pop( "IaF", 4, slow ).pop( "IaE", 4, slow ).pop( "IIF", 3, slow ).pop( "IIE", 3, slow ).pop( "MNF", 20 ).pop( "MNE", 20 ).pop( "INF", 2 ).pop( "INE", 2 ).pop( "TONIC", 6, slow ).pop( "GAM", 4 );
    b.in( "ag_Ia", "dc", 3, { "IaF" } ).in( "ant_Ia", "dc", 3, { "IaE" } ).in( "ag_II", "dc", 3, { "IIF" } ).in( "ant_II", "dc", 3, { "IIE" } ).in( "drive", "dc", 300, { "TONIC" } );
    b.proj( "IaF", "MNF", pw, 2, cv ).proj( "IIF", "MNF", pw, 2, cv ).proj( "IaE", "MNE", pw, 2, cv ).proj( "IIE", "MNE", pw, 2, cv );
    b.proj( "IaF", "INF", 80 ).proj( "IaE", "INE", 80 ).proj( "INF", "MNE", -320 ).proj( "INE", "MNF", -320 );
    b.proj( "TONIC", "MNF", 70, 2, cv ).proj( "TONIC", "GAM", 960 );
    b.out( "ag_motor", { "MNF" } ).out( "ant_motor", { "MNE" } ).out( "gamma_motor", { "GAM" } );
    return b.c;
  }
  if ( task == "reach" )  // the recipe's starting point (first burst and hold command; the brake and the clamp are the player's)
  {
    Bench b;
    const json slow = { { "tau_m", 60.0 } }, burst = { { "tau_syn_ex", 50.0 }, { "tau_syn_in", 50.0 } };
    b.pop( "IaF", 6, slow ).pop( "IaE", 6, slow ).pop( "MNF", 20 ).pop( "MNE", 20 ).pop( "INF", 4 ).pop( "INE", 4 ).pop( "TGT", 3, slow ).pop( "GO", 3, slow ).pop( "B1", 4, burst );
    b.in( "ag_Ia", "dc", 3, { "IaF" } ).in( "ant_Ia", "dc", 3, { "IaE" } ).in( "drive", "dc", 250, { "MNF", "MNE" } ).in( "target", "dc", 3, { "TGT" } ).in( "go", "dc", 3, { "GO" } );
    b.proj( "IaF", "MNF", 160 ).proj( "IaE", "MNE", 160 ).proj( "IaF", "INF", 80 ).proj( "IaE", "INE", 80 ).proj( "INF", "MNE", -160 ).proj( "INE", "MNF", -160 );
    b.proj( "TGT", "MNF", 1500 ).proj( "TGT", "MNE", -450 ).proj( "GO", "B1", 600 ).proj( "B1", "MNF", 800 ).proj( "B1", "MNE", -400 );
    b.out( "ag_motor", { "MNF" } ).out( "ant_motor", { "MNE" } );
    return b.c;
  }
  return json::object();
}
/// renshaw with plain (integrating) cells for DESC and a drive of 200: what a novice builds first
static json renshaw_plain_desc()
{
  json c = novice_bench( "renshaw" );
  for ( auto& p : c[ "populations" ] )
    if ( p[ "id" ] == "DESC" )
      p[ "model" ] = "iaf_psc_alpha";
  for ( auto& i : c[ "inputs" ] )
    if ( i[ "name" ] == "drive" )
      i[ "weight" ] = 200;
  return c;
}


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

  if ( argc >= 3 && std::string( argv[ 1 ] ) == "bench" )  // tuning aid: run the novice bench of a task (bench <task> [seeds])
  {
    json c = std::string( argv[ 2 ] ) == "renshaw_plain" ? renshaw_plain_desc() : novice_bench( argv[ 2 ] );
    if ( const char* w = std::getenv( "BENCHW" ) )  // BENCHW="pre>post=weight": override one link's weight
    {
      std::string a = w;
      const auto gt = a.find( '>' ), eq = a.find( '=' );
      for ( auto& pr : c[ "projections" ] )
        if ( pr[ "pre" ] == a.substr( 0, gt ) && pr[ "post" ] == a.substr( gt + 1, eq - gt - 1 ) )
          pr[ "synapse" ][ "weight" ] = std::stod( a.substr( eq + 1 ) );
    }
    std::stringstream ss( argc > 3 ? argv[ 3 ] : "1,2,3,4,5" );
    for ( std::string tok; std::getline( ss, tok, ',' ); )
      print( "novice", std::stoull( tok ), run_one( std::string( argv[ 2 ] ).substr( 0, std::string( argv[ 2 ] ).find( "_plain" ) ), c, std::stoull( tok ) ) );
    return 0;
  }

  if ( argc >= 4 && std::string( argv[ 1 ] ) == "trace" )  // tuning aid: trace <task> <seed> every 100 ms of the novice bench's angle
  {
    json c = novice_bench( argv[ 2 ] );
    c[ "seed" ] = 1000 * std::stoull( argv[ 3 ] ) + 1;
    Engine e;
    if ( !e.load( c.dump() ) ) { std::printf( "load error %s\n", e.error().c_str() ); return 1; }
    auto t = make_task( argv[ 2 ] );
    t->reset( std::stoull( argv[ 3 ] ) );
    for ( long s = 0; s < static_cast< long >( t->duration_ms() ); ++s )
    {
      t->step( e, 1.0 );
      if ( s % 100 == 99 )
      {
        auto r = t->body().readouts;
        std::printf( "%5ld th=%.2f", s + 1, r[ "theta_rad" ] );
        if ( r.count( "err_rad" ) ) std::printf( " err=%.2f", r[ "err_rad" ] );
        std::printf( "\n" );
      }
      if ( s % 500 == 499 ) e.drain_spikes();
    }
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

  // ---- recipes live in recipe(), the brief carries none; every task declares the cell the lab opens on ----
  for ( const auto& id : tasks )
  {
    auto t = make_task( id );
    CHECK_MSG( t->default_model() == "iaf_psc_alpha", id + " default_model" );
    CHECK_MSG( !t->recipe().empty(), id + " has no recipe()" );
    CHECK_MSG( t->brief().find( "A recipe that works" ) == std::string::npos, id + " brief still carries the recipe" );
    CHECK_MSG( t->recipe().find( "A recipe that works" ) == 0 || t->recipe().find( "A starting point" ) == 0, id + " recipe() should open as a recipe" );
  }

  // ---- novice benches: the recipe() numbers built the way the lab builds them (default part, default delay and wiring) pass Standard on 5 seeds ----
  for ( const auto& id : tasks )
  {
    auto t = make_task( id );
    json c = novice_bench( id );
    const bool spine = t->on_spine();  // a task moved to a branch because no novice bench passes it (lead decision): the bench is only reported
    CHECK_MSG( spine || id == "reach", id + " is off the spine" );
    for ( uint64_t seed : { 1, 2, 3, 4, 5 } )
    {
      RunResult r = run_one( id, c, seed );
      print( "novice bench", seed, r );
      CHECK_MSG( r.loaded, r.error );
      if ( spine )
        CHECK_MSG( r.score.passed, id + " novice bench seed " + std::to_string( seed ) + " (Standard): " + r.score.verdict );
    }
  }
  {
    // renshaw: plain cells for DESC swallow the volleys: the run fails with a verdict that names the cause and never shows 100
    json c = renshaw_plain_desc();
    RunResult r = run_one( "renshaw", c, 1 );
    print( "renshaw, plain DESC", 1, r );
    CHECK( r.loaded );
    CHECK( !r.score.passed );
    CHECK_MSG( r.score.value < 60.0, "a run whose pools ignore the common input must not score high: " + std::to_string( r.score.value ) );
    CHECK_MSG( r.score.verdict.find( "parrot_neuron" ) != std::string::npos, "verdict should name what to change: " + r.score.verdict );
  }
  {
    // posture: the old recipe (relay > pool 160, TONIC > MNF 240) drives the flexor pool past its ceiling and the limb sags; the verdict must not say "more tone" then
    json c = novice_bench( "posture" );
    for ( auto& pr : c[ "projections" ] )
    {
      if ( pr[ "pre" ] == "TONIC" && pr[ "post" ] == "MNF" )
        pr[ "synapse" ][ "weight" ] = 240.0;
      else if ( pr[ "post" ] == "MNF" || pr[ "post" ] == "MNE" )
        if ( pr[ "pre" ].get< std::string >().substr( 0, 2 ) == "Ia" || pr[ "pre" ].get< std::string >().substr( 0, 2 ) == "II" )
          if ( pr[ "synapse" ][ "weight" ].get< double >() > 0 )
            pr[ "synapse" ][ "weight" ] = 160.0;
    }
    RunResult r = run_one( "posture", c, 1 );
    print( "posture, too much tone", 1, r );
    CHECK( !r.score.passed );
    CHECK_MSG( r.score.verdict.find( "sags" ) != std::string::npos && r.score.verdict.find( "lower the flexor tone" ) != std::string::npos, r.score.verdict );
    for ( auto& pr : c[ "projections" ] )
      if ( pr[ "pre" ] == "TONIC" && pr[ "post" ] == "MNF" )
        pr[ "synapse" ][ "weight" ] = 0.0;
    r = run_one( "posture", c, 1 );
    print( "posture, no tone", 1, r );
    CHECK( !r.score.passed );
    CHECK_MSG( r.score.verdict.find( "more flexor tone" ) != std::string::npos, r.score.verdict );
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
