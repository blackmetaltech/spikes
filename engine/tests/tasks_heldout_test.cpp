// Held-out-seed robustness of the two spinal levels whose references used to pass only about four runs in five (boss_spinal_sprint) or missed one or two of 35 quick checks (stretch_reflex).
//   (a player who copies the idea of the reference must not fail one run in five: docs/circuits/boss_spinal_sprint.md)
// Usage: tasks_heldout_test <task>   (CTest entries tasks_heldout_test_stretch_reflex and tasks_heldout_test_boss_spinal_sprint)
// The reference circuit of the level, seeds 1-30 (the seeds a player sees while building) and 101-105 (TEST x5), the quick episode (CHECK) and the full one, in guided, standard and researcher:
// every one of those six combinations must pass on at least 34 of the 35 seeds (97 %; the goal is 95 %). Both worlds are deterministic and a mode only scales the thresholds (tol() / tol_max()),
// so the episode is simulated once per seed and episode length and scored in the three modes; seed 101 is also run from scratch in researcher mode to show that this is the same thing.
// Every seed is a forked child (QF_JOBS at a time, default 4).
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace
{
struct Sim
{
  bool loaded = false;
  Score score[ 3 ];   // guided, standard, researcher
};
const char* kModes[ 3 ] = { "guided", "standard", "researcher" };

Sim simulate( const std::string& id, const json& circuit_in, uint64_t seed, bool quick, int only_mode = -1 )
{
  Sim r;
  Engine e;
  json c = circuit_in;
  c[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );   // as the game's TEST and spike-run reseed the circuit with the world
  r.loaded = e.load( c.dump() );
  if ( !r.loaded ) return r;
  auto t = make_task( id );
  t->set_mode( only_mode >= 0 ? kModes[ only_mode ] : "standard" );
  t->set_quick( quick );
  t->reset( seed );
  const long steps = static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() );
  for ( long s = 0; s < steps; ++s ) t->step( e, 1.0 );
  for ( int m = 0; m < 3; ++m )
  {
    t->set_mode( kModes[ m ] );
    r.score[ m ] = t->score();
  }
  return r;
}

/// Child: one line "<seed> q=<ggg> f=<ggg> | verdicts" (one character per mode: 1 pass, 0 fail, x did not load).
void child( const std::string& id, uint64_t seed, int fd )
{
  const std::string path = std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + reference_circuit( id );
  const json c = json::parse( chk::slurp( path ) );
  const Sim q = simulate( id, c, seed, true ), f = simulate( id, c, seed, false );
  std::ostringstream o;
  o << seed << " q=";
  for ( int m = 0; m < 3; ++m ) o << ( q.loaded ? ( q.score[ m ].passed ? '1' : '0' ) : 'x' );
  o << " f=";
  for ( int m = 0; m < 3; ++m ) o << ( f.loaded ? ( f.score[ m ].passed ? '1' : '0' ) : 'x' );
  o << " |";
  for ( int m = 0; m < 3; ++m )
    if ( q.loaded && !q.score[ m ].passed ) o << " [quick " << kModes[ m ] << ": " << q.score[ m ].verdict.substr( 0, 100 ) << "]";
  for ( int m = 0; m < 3; ++m )
    if ( f.loaded && !f.score[ m ].passed ) o << " [full " << kModes[ m ] << ": " << f.score[ m ].verdict.substr( 0, 100 ) << "]";
  if ( seed == 101 )   // a mode only scales the thresholds: a fresh researcher run gives the same score as scoring the standard run in researcher mode
  {
    const Sim r = simulate( id, c, seed, true, 2 );
    o << ( r.loaded && r.score[ 2 ].passed == q.score[ 2 ].passed && r.score[ 2 ].value == q.score[ 2 ].value ? " same-as-fresh" : " DIFFERENT-FROM-FRESH" );
  }
  o << "\n";
  const std::string s = o.str();
  if ( write( fd, s.data(), s.size() ) < 0 ) _exit( 3 );
  _exit( q.loaded && f.loaded ? 0 : 2 );
}
}  // namespace

int main( int argc, char** argv )
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  const std::string id = argc > 1 ? argv[ 1 ] : "stretch_reflex";
  std::vector< uint64_t > seeds;
  for ( uint64_t s = 1; s <= 30; ++s ) seeds.push_back( s );
  for ( uint64_t s = 101; s <= 105; ++s ) seeds.push_back( s );
  const int slots = std::getenv( "QF_JOBS" ) ? std::atoi( std::getenv( "QF_JOBS" ) ) : 4;
  int pipefd[ 2 ];
  if ( pipe( pipefd ) != 0 ) return 1;
  fcntl( pipefd[ 0 ], F_SETFL, O_NONBLOCK );
  std::string buf;
  auto drain = [ & ]() {
    char tmp[ 4096 ];
    ssize_t n;
    while ( ( n = read( pipefd[ 0 ], tmp, sizeof tmp ) ) > 0 ) buf.append( tmp, static_cast< size_t >( n ) );
  };
  size_t next = 0;
  int running = 0;
  std::printf( "%s: %zu seeds, %d at a time\n", id.c_str(), seeds.size(), slots );
  while ( next < seeds.size() || running > 0 )
  {
    while ( running < slots && next < seeds.size() )
    {
      const pid_t p = fork();
      if ( p == 0 )
      {
        close( pipefd[ 0 ] );
        child( id, seeds[ next ], pipefd[ 1 ] );
      }
      if ( p < 0 ) return 1;
      ++next;
      ++running;
    }
    int st = 0;
    if ( waitpid( -1, &st, 0 ) > 0 )
    {
      --running;
      CHECK_MSG( WIFEXITED( st ) && WEXITSTATUS( st ) == 0, id + ": a child failed to run (status " + std::to_string( st ) + ")" );
    }
    drain();
  }
  drain();
  int count[ 2 ][ 3 ] = {};   // [quick, full][mode]
  int total = 0;
  std::stringstream ss( buf );
  for ( std::string line; std::getline( ss, line ); )
  {
    std::printf( "%s\n", line.c_str() );
    const size_t q = line.find( " q=" ), f = line.find( " f=" );
    if ( q == std::string::npos || f == std::string::npos ) continue;
    ++total;
    for ( int m = 0; m < 3; ++m )
    {
      count[ 0 ][ m ] += line[ q + 3 + static_cast< size_t >( m ) ] == '1' ? 1 : 0;
      count[ 1 ][ m ] += line[ f + 3 + static_cast< size_t >( m ) ] == '1' ? 1 : 0;
    }
    CHECK_MSG( line.find( "DIFFERENT-FROM-FRESH" ) == std::string::npos, id + ": scoring a run in another mode differs from a fresh run in that mode" );
  }
  CHECK_MSG( total == static_cast< int >( seeds.size() ), id + ": a seed did not report" );
  const int need = static_cast< int >( seeds.size() ) - 1;   // at most one failure in 35
  for ( int e = 0; e < 2; ++e )
    for ( int m = 0; m < 3; ++m )
    {
      std::printf( "%-20s %-10s %-5s %d/%d\n", id.c_str(), kModes[ m ], e == 0 ? "quick" : "full", count[ e ][ m ], total );
      CHECK_MSG( count[ e ][ m ] >= need, id + " / " + kModes[ m ] + " / " + ( e == 0 ? "quick" : "full" ) + ": the reference passes only " + std::to_string( count[ e ][ m ] ) + " of " + std::to_string( total ) + " seeds" );
    }
  return chk::finish( "tasks_heldout_test" );
}
