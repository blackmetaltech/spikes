// Instant check: `spike-run --task` (the process SpikeTask.score_headless_async launches) honours --mode / --variant, reports hints and metrics,
// is deterministic, and is quick for the shortest task. Also the v0.3 Task defaults (ChannelKind::weight).
#include <algorithm>
#include <chrono>
#include <utility>
#include <vector>
#include <cstdio>
#include <string>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;

static bool run( const std::string& args, json& out, double* ms = nullptr )
{
  const auto t0 = std::chrono::steady_clock::now();
  const std::string cmd = std::string( SPIKE_RUN_EXE ) + " " + args + " 2>/dev/null";
  FILE* p = popen( cmd.c_str(), "r" );
  if ( not p )
    return false;
  std::string s;
  char buf[ 4096 ];
  size_t n;
  while ( ( n = fread( buf, 1, sizeof buf, p ) ) > 0 )
    s.append( buf, n );
  const int st = pclose( p );
  if ( ms )
    *ms = std::chrono::duration< double, std::milli >( std::chrono::steady_clock::now() - t0 ).count();
  out = json::parse( s, nullptr, false );
  return st == 0 and out.is_object();
}

int main()
{
  // the quickest task that has a reference circuit and runs (micro-puzzles may land after the engine)
  std::vector< std::pair< double, std::string > > by_len;
  for ( const auto& id : spike::task_ids() )
    if ( auto t = spike::make_task( id ) )
      by_len.push_back( { t->duration_ms(), id } );
  std::sort( by_len.begin(), by_len.end() );
  std::string best;
  json a, b, g, v;
  double ms = 0;
  for ( const auto& [ dur, id ] : by_len )
    if ( run( "--task " + id + " --seeds 1,2", a, &ms ) )
    {
      best = id;
      std::printf( "  quickest runnable task: %s (%.0f ms of model time)\n", id.c_str(), dur );
      break;
    }
  CHECK( not best.empty() );
  if ( best.empty() )
    return chk::finish( "test_instant" );
  std::printf( "  headless check, 2 seeds: %.0f ms wall\n", ms );
  CHECK( a.value( "mode", "" ) == "standard" and a.value( "variant", "x" ) == "" );
  CHECK( a.contains( "runs" ) and a[ "runs" ].size() == 2 );
  CHECK( a[ "runs" ][ 0 ].contains( "hints" ) and a[ "runs" ][ 0 ].contains( "metrics" ) );
  CHECK( run( "--task " + best + " --seeds 1,2", b ) );
  CHECK( a[ "runs" ][ 0 ][ "value" ] == b[ "runs" ][ 0 ][ "value" ] and a[ "runs" ][ 1 ][ "value" ] == b[ "runs" ][ 1 ][ "value" ] );  // deterministic
  CHECK( run( "--task " + best + " --seeds 1 --mode guided", g ) and g.value( "mode", "" ) == "guided" );
  CHECK( run( "--task " + best + " --seeds 1 --mode researcher", g ) and g.value( "mode", "" ) == "researcher" );
  // --circuit-seed pins the circuit seed (reported per run); the default reseed is 1000 * seed + circuit seed
  {
    json p1, p2;
    CHECK( run( "--task " + best + " --seeds 1,2 --circuit-seed 77", p1 ) and p1[ "runs" ][ 0 ][ "circuit_seed" ] == 77 and p1[ "runs" ][ 1 ][ "circuit_seed" ] == 77 );
    CHECK( run( "--task " + best + " --seeds 1,2 --circuit-seeds 5,6", p2 ) and p2[ "runs" ][ 0 ][ "circuit_seed" ] == 5 and p2[ "runs" ][ 1 ][ "circuit_seed" ] == 6 );
    CHECK( a[ "runs" ][ 0 ][ "circuit_seed" ].get< long long >() % 1000 == a[ "runs" ][ 1 ][ "circuit_seed" ].get< long long >() % 1000 );
    json bad;
    CHECK( not run( "--task " + best + " --seeds 1,2 --circuit-seeds 5,6,7", bad ) );
  }
  // a task's declared variant is accepted and echoed
  for ( const auto& id : spike::task_ids() )
    if ( auto t = spike::make_task( id ); t and not t->variants().empty() )
    {
      const std::string vid = t->variants().begin()->first;
      CHECK( run( "--task " + id + " --seeds 1 --variant " + vid, v ) and v.value( "variant", "" ) == vid );
      std::printf( "  variant %s of %s accepted\n", vid.c_str(), id.c_str() );
      break;
    }
  // v0.3 defaults: weight unset
  spike::Task::ChannelKind ck;
  CHECK( ck.weight == 0 );
  return chk::finish( "test_instant" );
}
