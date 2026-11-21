// The reference circuit of every level, in every mode, on the seeds of the game's TEST x5 (101 to 105), headless and deterministic:
//   (1) a quick pass implies a full pass   CHECK runs the quick episode and a pass is recorded as a level pass; the full episode (TEST) must agree (full review, P1-3)
//   (2) the stars are reachable            in standard and researcher the reference passes the FULL episode on all five TEST seeds (full review, P1-4)
// Usage: tasks_quick_full_test <part> <parts> [task,task,...]   part k of n takes the tasks k, k+n, ... of task_ids() (diagnosis cases are decided by the submitted answer and are skipped);
// every (task, mode, seed) is a forked child, QF_JOBS (default 3) at a time, so one failure cannot take the others down and the wall time stays a few minutes.
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace
{
struct Out
{
  bool loaded = false, passed = false;
  double value = 0;
  std::string verdict;
};

Out run( const std::string& id, const json& circuit_in, uint64_t seed, bool quick, const std::string& mode )
{
  Out r;
  Engine e;
  json c = circuit_in;
  c[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );   // as the game's TEST and spike-run reseed the circuit with the world
  r.loaded = e.load( c.dump() );
  if ( !r.loaded ) { r.verdict = e.error(); return r; }
  auto t = make_task( id );
  t->set_mode( mode );
  t->set_quick( quick );
  t->reset( seed );
  const long steps = static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() );
  for ( long s = 0; s < steps; ++s ) t->step( e, 1.0 );
  const Score sc = t->score();
  r.passed = sc.passed;
  r.value = sc.value;
  r.verdict = sc.verdict;
  return r;
}

struct Job
{
  std::string task, mode;
  uint64_t seed;
};

/// Child: prints one line "<task> <mode> <seed> quick=<0|1> full=<0|1|-> | text" to fd.
void child( const Job& j, int fd )
{
  const std::string path = std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + reference_circuit( j.task );
  json c = json::parse( chk::slurp( path ) );
  const bool stars = j.mode != "guided";
  const Out q = run( j.task, c, j.seed, true, j.mode );
  Out f;
  bool have_full = false;
  if ( stars || q.passed ) { f = run( j.task, c, j.seed, false, j.mode ); have_full = true; }
  std::ostringstream o;
  o << j.task << " " << j.mode << " " << j.seed << " quick=" << ( q.passed ? 1 : 0 ) << " full=" << ( have_full ? ( f.passed ? "1" : "0" ) : "-" ) << " | quick " << static_cast< int >( q.value )
    << ": " << q.verdict.substr( 0, 90 ) << " | full " << static_cast< int >( f.value ) << ": " << f.verdict.substr( 0, 90 ) << "\n";
  const std::string s = o.str();
  if ( write( fd, s.data(), s.size() ) < 0 ) _exit( 3 );
  _exit( q.loaded && ( !have_full || f.loaded ) ? 0 : 2 );
}
}  // namespace

int main( int argc, char** argv )
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  const int part = argc > 1 ? std::atoi( argv[ 1 ] ) : 0, parts = argc > 2 ? std::max( 1, std::atoi( argv[ 2 ] ) ) : 1;
  std::set< std::string > only;
  if ( argc > 3 )
  {
    std::stringstream ss( argv[ 3 ] );
    for ( std::string t; std::getline( ss, t, ',' ); ) only.insert( t );
  }
  std::vector< std::string > tasks;
  int idx = 0;
  for ( const auto& id : task_ids() )
  {
    auto t = make_task( id );
    if ( t->level_kind() == "diagnosis" ) continue;
    if ( !only.empty() ? only.count( id ) : ( idx++ % parts == part ) ) tasks.push_back( id );
  }
  std::vector< Job > jobs;
  for ( const auto& t : tasks )
    for ( const char* m : { "standard", "researcher", "guided" } )
      for ( uint64_t s = 101; s <= 105; ++s ) jobs.push_back( { t, m, s } );
  const int slots = std::getenv( "QF_JOBS" ) ? std::atoi( std::getenv( "QF_JOBS" ) ) : 3;
  std::printf( "%zu tasks, %zu runs, %d at a time\n", tasks.size(), jobs.size(), slots );

  int pipefd[ 2 ];
  if ( pipe( pipefd ) != 0 ) return 1;
  size_t next = 0;
  int running = 0, finished = 0;
  std::map< std::string, int > star_fail;   // task/mode -> number of seeds whose full run failed
  std::string buf;
  auto drain = [ & ]() {
    char tmp[ 4096 ];
    ssize_t n;
    while ( ( n = read( pipefd[ 0 ], tmp, sizeof tmp ) ) > 0 ) { buf.append( tmp, static_cast< size_t >( n ) ); if ( n < static_cast< ssize_t >( sizeof tmp ) ) break; }
  };

  fcntl( pipefd[ 0 ], F_SETFL, O_NONBLOCK );   // non-blocking read end
  while ( next < jobs.size() || running > 0 )
  {
    while ( running < slots && next < jobs.size() )
    {
      const pid_t p = fork();
      if ( p == 0 ) { close( pipefd[ 0 ] ); child( jobs[ next ], pipefd[ 1 ] ); }
      ++next;
      ++running;
    }
    int status = 0;
    if ( waitpid( -1, &status, 0 ) > 0 )
    {
      --running;
      ++finished;
      CHECK_MSG( WIFEXITED( status ) && WEXITSTATUS( status ) == 0, "a run crashed or did not load" );
    }
    drain();
  }
  drain();
  std::stringstream ss( buf );
  for ( std::string line; std::getline( ss, line ); )
  {
    const bool q = line.find( "quick=1" ) != std::string::npos, f1 = line.find( "full=1" ) != std::string::npos, f0 = line.find( "full=0" ) != std::string::npos;
    const bool guided = line.find( " guided " ) != std::string::npos;
    const std::string head = line.substr( 0, line.find( " quick=" ) );
    if ( q && f0 ) CHECK_MSG( false, "a quick PASS but a full FAIL: " + line );
    if ( !guided && !f1 ) CHECK_MSG( false, "the reference does not pass the full episode (the stars are out of reach): " + line );
  }
  std::printf( "%d runs finished\n", finished );
  return chk::finish( "tasks_quick_full_test" );
}
