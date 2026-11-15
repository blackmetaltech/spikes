// Runs the reference circuits through the real engine on their tasks, and checks that deliberately broken variants fail.
//
//   tasks_circuits_test                      the CTest: reference circuits pass on 3 seeds, lesioned ones fail
//   tasks_circuits_test run <task> <circuit.json> [seeds=1,2,3] [pre>post*factor ...]
//                                            tuning aid: run one circuit, print score parts and per-population rates.
//                                            A lesion scales the weight of every projection pre->post (0 deletes it).
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

static json load_circuit( const std::string& file )
{
  return json::parse( chk::slurp( file ) );
}

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
  if ( !hit )
    std::printf( "  WARNING: lesion %s>%s matched no projection\n", l.pre.c_str(), l.post.c_str() );
}

struct RunResult { Score score; std::map< std::string, double > rate_hz; double wall_s = 0; bool loaded = false; std::string error; };

static RunResult run_one( const std::string& task_id, const json& circuit_in, uint64_t seed )
{
  RunResult r;
  Engine e;
  json circuit = circuit_in;
  circuit[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );  // the task seed also reseeds NEST's noise and the initial draws
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
  long steps = static_cast< long >( task->duration_ms() );
  for ( long s = 0; s < steps; ++s )
  {
    task->step( e, 1.0 );
    if ( std::getenv( "TASK_TRACE" ) && s % 50 == 49 )
    {
      auto b = task->body();
      std::printf( "  t=%6.0f", b.readouts.count( "t_ms" ) ? b.readouts[ "t_ms" ] : 0.0 );
      for ( auto& [ k, v ] : b.readouts ) if ( k != "t_ms" ) std::printf( " %s=%.3g", k.c_str(), v );
      std::printf( "\n" );
    }
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
  r.error = e.error();  // empty when every set_input/step succeeded
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
  for ( auto& [ k, v ] : r.score.parts ) std::printf( " %s=%.3g", k.c_str(), v );
  std::printf( "  (%.1fs)\n    %s\n    rates Hz:", r.wall_s, r.score.verdict.c_str() );
  for ( auto& [ k, v ] : r.rate_hz ) std::printf( " %s=%.1f", k.c_str(), v );
  std::printf( "\n" );
  if ( !r.error.empty() )
    std::printf( "    ENGINE ERROR: %s\n", r.error.c_str() );
}

static std::string circuits_dir() { return std::string( SPIKE_SOURCE_DIR ) + "/circuits/"; }

struct Case { std::string task; std::vector< Lesion > lesions; const char* what; };

int main( int argc, char** argv )
{
  if ( argc >= 4 && std::string( argv[ 1 ] ) == "run" )
  {
    std::string task = argv[ 2 ];
    json c = load_circuit( argv[ 3 ] );
    std::vector< uint64_t > seeds;
    int i = 4;
    std::string sl = ( argc > 4 && std::string( argv[ 4 ] ).find( '>' ) == std::string::npos ) ? argv[ i++ ] : "1,2,3";
    std::stringstream ss( sl );
    for ( std::string tok; std::getline( ss, tok, ',' ); ) seeds.push_back( std::stoull( tok ) );
    for ( ; i < argc; ++i )
    {
      std::string a = argv[ i ];
      auto gt = a.find( '>' ), st = a.find( '*' );
      apply( c, { a.substr( 0, gt ), a.substr( gt + 1, st - gt - 1 ), std::stod( a.substr( st + 1 ) ) } );
    }
    for ( auto s : seeds ) print( task, s, run_one( task, c, s ) );
    return 0;
  }

  // reference circuits pass on several seeds
  for ( const auto& id : task_ids() )
  {
    json c = load_circuit( circuits_dir() + reference_circuit( id ) );
    auto t = make_task( id );
    // channel contract: the circuit declares every channel the task uses
    std::map< std::string, bool > in_ch, out_ch;
    for ( auto& i : c[ "inputs" ] ) in_ch[ i[ "name" ] ] = true;
    for ( auto& o : c[ "outputs" ] ) out_ch[ o[ "name" ] ] = true;
    for ( auto& s : t->sensor_channels() ) CHECK_MSG( in_ch.count( s ), id + " input " + s );
    for ( auto& m : t->motor_channels() ) CHECK_MSG( out_ch.count( m ), id + " output " + m );
    int neurons = 0;
    for ( auto& p : c[ "populations" ] ) neurons += p[ "n" ].get< int >();
    CHECK_MSG( neurons <= 60, id + " has " + std::to_string( neurons ) + " neurons" );
    std::printf( "%s (%d neurons)\n", id.c_str(), neurons );
    double sum = 0;
    for ( uint64_t seed : { 1, 2, 3 } )
    {
      RunResult r = run_one( id, c, seed );
      print( "reference", seed, r );
      CHECK_MSG( r.loaded, r.error );
      CHECK_MSG( r.score.passed, id + " seed " + std::to_string( seed ) + ": " + r.score.verdict );
      CHECK( r.score.value >= 50.0 );
      sum += r.score.value;
    }
    std::printf( "  mean score %.1f\n", sum / 3 );
  }

  // lesions: the task is about the circuit, so removing the key mechanism must fail it
  const std::vector< Case > cases = {
#include "tasks_lesions.inc"
  };
  for ( const auto& cs : cases )
  {
    json c = load_circuit( circuits_dir() + reference_circuit( cs.task ) );
    for ( const auto& l : cs.lesions ) apply( c, l );
    std::printf( "%s lesion: %s\n", cs.task.c_str(), cs.what );
    for ( uint64_t seed : { 1, 2 } )
    {
      RunResult r = run_one( cs.task, c, seed );
      print( "lesioned", seed, r );
      CHECK_MSG( r.loaded, r.error );
      CHECK_MSG( !r.score.passed, std::string( cs.task ) + " still passes with lesion: " + cs.what );
    }
  }
  return chk::finish( "tasks_circuits_test" );
}
