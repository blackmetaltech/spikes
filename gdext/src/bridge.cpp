#include "bridge.h"
#include "spike/theory.h"
#include "spike/analysis.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <cerrno>
#include <climits>
#include <csignal>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace spike_gd
{
static std::string error_json( const std::string& what )
{
  std::string m;
  for ( unsigned char c : what )
  {
    if ( c == '"' || c == '\\' )
      ( m += '\\' ) += static_cast< char >( c );
    else if ( c < 0x20 )
      m += ' ';
    else
      m += static_cast< char >( c );
  }
  return "{\"error\": \"" + m + "\"}";
}

static spike::analysis::Sign to_sign( int sign )
{
  return sign == 1 ? spike::analysis::Sign::Excitatory : sign == 2 ? spike::analysis::Sign::Inhibitory : spike::analysis::Sign::Any;
}

std::string motif_json( const spike::Engine& engine, int sign, int n_null, unsigned long long seed )
{
  try
  {
    auto edges = spike::analysis::edges_from_engine( engine );
    auto r = spike::analysis::motif_zscores( edges, engine.neuron_count(), to_sign( sign ), n_null, seed );
    return spike::analysis::motif_result_json( r );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

std::string motif_json_edges( const std::vector< double >& triples, int n, int sign, int n_null, unsigned long long seed )
{
  try
  {
    std::vector< spike::analysis::Edge > edges;
    for ( size_t i = 0; i + 2 < triples.size(); i += 3 )
      edges.push_back( { static_cast< int32_t >( triples[ i ] ), static_cast< int32_t >( triples[ i + 1 ] ), triples[ i + 2 ] } );
    auto r = spike::analysis::motif_zscores( edges, n, to_sign( sign ), n_null, seed );
    return spike::analysis::motif_result_json( r );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

Stats spike_stats( const std::vector< spike::Spike >& spikes, int n, double t0_ms, double t1_ms )
{
  Stats s;
  s.rates = spike::analysis::firing_rates( spikes, n, t0_ms, t1_ms );
  s.cv = spike::analysis::isi_cv( spikes, n, t0_ms, t1_ms );
  s.chi = spike::analysis::golomb_chi( spikes, n, t0_ms, t1_ms );
  return s;
}

std::string burst_json( const std::vector< spike::Spike >& spikes, int n, const std::vector< int >& neurons, double t0_ms, double t1_ms,
  double max_isi_start_ms, double max_isi_end_ms, double min_ibi_ms, double min_duration_ms, int min_spikes )
{
  try
  {
    spike::analysis::BurstParams p;
    p.max_isi_start_ms = max_isi_start_ms;
    p.max_isi_end_ms = max_isi_end_ms;
    p.min_ibi_ms = min_ibi_ms;
    p.min_duration_ms = min_duration_ms;
    p.min_spikes = min_spikes;
    const std::vector< int32_t > sel( neurons.begin(), neurons.end() );
    const auto t = spike::analysis::pooled_spike_times( spikes, n, sel, t0_ms, t1_ms );
    return spike::analysis::bursts_json( spike::analysis::detect_bursts( t, p ), static_cast< int64_t >( t.size() ), p );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

std::string prc_json( const std::vector< spike::Spike >& spikes, int n, const std::vector< int >& neurons, const std::vector< double >& perturb_ms,
  double period_ms, double merge_gap_ms, int nbins )
{
  try
  {
    const std::vector< int32_t > sel( neurons.begin(), neurons.end() );
    return spike::analysis::prc_json( spike::analysis::prc_estimate( spikes, n, sel, perturb_ms, period_ms, merge_gap_ms ), nbins );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

static void json_array( std::string& out, const char* key, const std::vector< double >& v )
{
  out += std::string( ",\"" ) + key + "\":[";
  char buf[ 40 ];
  for ( size_t i = 0; i < v.size(); ++i )
  {
    std::snprintf( buf, sizeof buf, "%.10g", v[ i ] );
    out += ( i ? "," : "" );
    out += buf;
  }
  out += "]";
}

std::string prc_experiment_json( spike::Engine& engine, const std::string& channel, double baseline, double pulse_value, double pulse_ms, int n_phases,
  const std::vector< int >& neurons, double merge_gap_ms, int repeats, int nbins )
{
  try
  {
    spike::analysis::PrcExperiment cfg;
    cfg.channel = channel;
    cfg.baseline = baseline;
    cfg.pulse_value = pulse_value;
    cfg.pulse_ms = pulse_ms;
    cfg.n_phases = n_phases;
    cfg.neurons.assign( neurons.begin(), neurons.end() );
    cfg.merge_gap_ms = merge_gap_ms;
    cfg.repeats = repeats;
    const auto r = spike::analysis::run_prc_experiment( engine, cfg );
    if ( !r.ok )
      return error_json( r.error );
    std::string j = spike::analysis::prc_json( r.prc, nbins );
    j.pop_back();  // reopen the object
    json_array( j, "spikes_ms", r.spike_ms );
    json_array( j, "pulses_ms", r.pulse_ms );
    return j + "}";
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

std::string sta_json( const std::vector< double >& stimulus, double dt_ms, const std::vector< double >& spike_times_ms, double before_ms, double after_ms )
{
  const spike::theory::Sta r = spike::theory::spike_triggered_average( stimulus, dt_ms, spike_times_ms, before_ms, after_ms );
  std::string out = "{\"mean\":[";
  char buf[256];
  for ( size_t k = 0; k < r.mean.size(); ++k )
  {
    std::snprintf( buf, sizeof buf, "%s%.9g", k ? "," : "", r.mean[ k ] );
    out += buf;
  }
  std::snprintf( buf, sizeof buf, "],\"n\":%d,\"before_ms\":%.6g,\"dt_ms\":%.6g,\"stim_mean\":%.9g,\"stim_sd\":%.9g}", r.n_used, r.before_ms, r.dt_ms, r.stim_mean, r.stim_sd );
  return out + buf;
}

std::string hrv_json( const std::vector< double >& beat_times_ms )
{
  try
  {
    return spike::analysis::hrv_json( spike::analysis::hrv_analyze( beat_times_ms ) );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

static std::string motif4_run( const spike::analysis::Graph& g, int n_null, unsigned long long seed, double max_work )
{
  const double work = spike::analysis::motif4_work_estimate( g ) * ( 1.0 + std::max( n_null, 0 ) );
  if ( max_work > 0 && work > max_work )
    return error_json( "4-node census too expensive for this graph (estimated work " + std::to_string( static_cast< long long >( work ) )
      + " > limit " + std::to_string( static_cast< long long >( max_work ) ) + "); use fewer null samples or the 3-node census" );
  return spike::analysis::motif4_result_json( spike::analysis::motif4_zscores( g, n_null, seed ) );
}

std::string motif4_json( const spike::Engine& engine, int sign, int n_null, unsigned long long seed, double max_work )
{
  try
  {
    auto edges = spike::analysis::edges_from_engine( engine );
    return motif4_run( spike::analysis::make_graph( edges, engine.neuron_count(), to_sign( sign ) ), n_null, seed, max_work );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

std::string motif4_json_edges( const std::vector< double >& triples, int n, int sign, int n_null, unsigned long long seed, double max_work )
{
  try
  {
    std::vector< spike::analysis::Edge > edges;
    for ( size_t i = 0; i + 2 < triples.size(); i += 3 )
      edges.push_back( { static_cast< int32_t >( triples[ i ] ), static_cast< int32_t >( triples[ i + 1 ] ), triples[ i + 2 ] } );
    return motif4_run( spike::analysis::make_graph( edges, n, to_sign( sign ) ), n_null, seed, max_work );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

// ---- v0.3 instant check ----
namespace
{
struct Job
{
  std::thread th;
  std::atomic< bool > done{ false };
  std::atomic< bool > cancelled{ false };
  std::mutex mu;  // guards pid
  pid_t pid = 0;
  bool ok = false;
  std::string error, out;
  double ms = 0;
};
std::mutex g_mu;
std::map< long, std::unique_ptr< Job > > g_jobs;
long g_next = 1;
std::string g_runner;
std::vector< std::string > g_dirs;  // extra search directories (e.g. the project's res://bin, globalized)

bool exists( const std::string& p ) { return !p.empty() && access( p.c_str(), X_OK ) == 0; }
std::string dirname_of( const std::string& p )
{
  const auto i = p.rfind( '/' );
  return i == std::string::npos ? std::string( "." ) : ( i == 0 ? std::string( "/" ) : p.substr( 0, i ) );
}
std::string self_exe_dir()
{
  char buf[ 4096 ];
  const ssize_t n = readlink( "/proc/self/exe", buf, sizeof buf - 1 );
  return n > 0 ? dirname_of( std::string( buf, size_t( n ) ) ) : std::string();
}
std::string self_lib_dir()  // the directory of this shared library (the GDExtension), via dladdr on one of its own functions
{
  Dl_info info;
  if ( dladdr( reinterpret_cast< void* >( &headless_set_runner ), &info ) && info.dli_fname )
  {
    char real[ PATH_MAX ];
    if ( realpath( info.dli_fname, real ) )
      return dirname_of( real );
    return dirname_of( info.dli_fname );
  }
  return std::string();
}
// Resolution order: explicit set_headless_runner path, $SPIKE_RUN, next to the GDExtension library, next to the executable, added search dirs
// (the project's res://bin), a development tree (./, ../build/...), then PATH. Returns "" if nothing executable was found.
std::string find_runner( std::string* tried = nullptr )
{
  std::vector< std::string > cands;
  if ( !g_runner.empty() )
    cands.push_back( g_runner );
  if ( const char* e = std::getenv( "SPIKE_RUN" ) )
    if ( *e )
      cands.push_back( e );
  std::vector< std::string > dirs;
  const std::string lib = self_lib_dir(), exe = self_exe_dir();
  for ( const std::string& d : { lib, exe } )
    if ( !d.empty() )
    {
      dirs.push_back( d );
      dirs.push_back( d + "/bin" );
    }
  for ( const auto& d : g_dirs )
    dirs.push_back( d );
  for ( const auto& d : dirs )
    cands.push_back( d + "/spike-run" );
  for ( const char* c : { "./spike-run", "./bin/spike-run", "../spike-run", "build/bin/spike-run", "build/engine/spike-run", "../build/bin/spike-run", "../build/engine/spike-run", "../../build/bin/spike-run" } )
    cands.push_back( c );
  if ( const char* path = std::getenv( "PATH" ) )
  {
    std::stringstream ss( path );
    for ( std::string d; std::getline( ss, d, ':' ); )
      if ( !d.empty() )
        cands.push_back( d + "/spike-run" );
  }
  for ( const auto& c : cands )
  {
    if ( tried )
      *tried += ( tried->empty() ? "" : ", " ) + c;
    if ( exists( c ) )
      return c;
  }
  return std::string();
}
bool word( const std::string& s )
{
  for ( unsigned char c : s )
    if ( !( std::isalnum( c ) || c == '_' || c == '-' ) )
      return false;
  return true;
}
// mkstemp in $TMPDIR (default /tmp): unique, mode 0600, created exclusively. Returns the open fd (or -1) and the path.
int make_temp( const char* tag, std::string& path )
{
  const char* t = std::getenv( "TMPDIR" );
  std::string tmpl = std::string( t && *t ? t : "/tmp" ) + "/spike_hl_" + tag + "_XXXXXX";
  std::vector< char > buf( tmpl.begin(), tmpl.end() );
  buf.push_back( 0 );
  const int fd = mkstemp( buf.data() );
  if ( fd >= 0 )
    path = buf.data();
  return fd;
}
void kill_job( Job& j )
{
  std::lock_guard< std::mutex > l( j.mu );
  if ( j.pid > 0 )
  {
    kill( -j.pid, SIGKILL );  // the child leads its own process group
    kill( j.pid, SIGKILL );
  }
}
struct Cleanup
{
  ~Cleanup()
  {
    for ( auto& kv : g_jobs )
    {
      kv.second->cancelled = true;
      kill_job( *kv.second );
    }
    for ( auto& kv : g_jobs )
      if ( kv.second->th.joinable() )
        kv.second->th.join();
  }
} g_cleanup;
}  // namespace

void headless_set_runner( const std::string& path )
{
  std::lock_guard< std::mutex > l( g_mu );
  g_runner = path;
}

void headless_add_search_dir( const std::string& dir )
{
  std::lock_guard< std::mutex > l( g_mu );
  if ( !dir.empty() && std::find( g_dirs.begin(), g_dirs.end(), dir ) == g_dirs.end() )
    g_dirs.push_back( dir );
}

std::string headless_runner_path()
{
  std::lock_guard< std::mutex > l( g_mu );
  return find_runner();
}

long headless_start( const std::string& task_id, const std::string& circuit_json, const std::vector< long long >& seeds_in, const std::string& mode,
  const std::string& variant, std::string& err, const std::vector< long long >& circuit_seeds, bool quick )
{
  if ( task_id.empty() || !word( task_id ) || !word( mode ) || !word( variant ) )
  {
    err = "bad task id, mode or variant";
    return 0;
  }
  std::vector< long long > seeds = seeds_in;
  if ( seeds.empty() )
    seeds.push_back( 1 );
  if ( !circuit_seeds.empty() && circuit_seeds.size() != 1 && circuit_seeds.size() != seeds.size() )
  {
    err = "circuit_seeds must be empty, one value, or one per seed";
    return 0;
  }
  std::string runner, tried;
  {
    std::lock_guard< std::mutex > l( g_mu );
    runner = find_runner( &tried );
  }
  if ( runner.empty() )
  {
    err = "spike-run not found (looked in: " + tried + ")";
    return 0;
  }
  std::string cfile, efile;
  const int cfd = make_temp( "c", cfile );
  if ( cfd < 0 )
  {
    err = "cannot create a temp file";
    return 0;
  }
  {
    size_t off = 0;
    bool wrote = true;
    while ( off < circuit_json.size() )
    {
      const ssize_t n = write( cfd, circuit_json.data() + off, circuit_json.size() - off );
      if ( n <= 0 )
      {
        wrote = false;
        break;
      }
      off += size_t( n );
    }
    close( cfd );
    if ( !wrote )
    {
      unlink( cfile.c_str() );
      err = "cannot write " + cfile;
      return 0;
    }
  }
  const int efd = make_temp( "e", efile );
  if ( efd < 0 )
  {
    unlink( cfile.c_str() );
    err = "cannot create a temp file";
    return 0;
  }
  std::vector< std::string > args{ runner, "--task", task_id, "--circuit", cfile, "--seeds" };
  auto join = []( const std::vector< long long >& v ) {
    std::string o;
    for ( size_t i = 0; i < v.size(); ++i )
      o += ( i ? "," : "" ) + std::to_string( v[ i ] );
    return o;
  };
  args.push_back( join( seeds ) );
  if ( !circuit_seeds.empty() )
  {
    args.push_back( "--circuit-seeds" );
    args.push_back( join( circuit_seeds ) );
  }
  if ( !mode.empty() )
  {
    args.push_back( "--mode" );
    args.push_back( mode );
  }
  if ( !variant.empty() )
  {
    args.push_back( "--variant" );
    args.push_back( variant );
  }
  if ( quick )
    args.push_back( "--quick" );
  int pfd[ 2 ];
  if ( pipe( pfd ) != 0 )
  {
    close( efd );
    unlink( cfile.c_str() );
    unlink( efile.c_str() );
    err = "pipe failed";
    return 0;
  }
  fcntl( pfd[ 0 ], F_SETFD, FD_CLOEXEC );
  fcntl( pfd[ 1 ], F_SETFD, FD_CLOEXEC );
  fcntl( efd, F_SETFD, FD_CLOEXEC );
  std::vector< char* > argv;
  for ( auto& a : args )
    argv.push_back( const_cast< char* >( a.c_str() ) );
  argv.push_back( nullptr );
  posix_spawn_file_actions_t fa;
  posix_spawnattr_t attr;
  posix_spawn_file_actions_init( &fa );
  posix_spawnattr_init( &attr );
  posix_spawn_file_actions_adddup2( &fa, pfd[ 1 ], 1 );
  posix_spawn_file_actions_adddup2( &fa, efd, 2 );
  posix_spawnattr_setflags( &attr, POSIX_SPAWN_SETPGROUP );
  posix_spawnattr_setpgroup( &attr, 0 );  // own process group, so cancel can kill the whole tree
  pid_t pid = 0;
  const int rc = posix_spawn( &pid, runner.c_str(), &fa, &attr, argv.data(), environ );
  posix_spawn_file_actions_destroy( &fa );
  posix_spawnattr_destroy( &attr );
  close( pfd[ 1 ] );
  close( efd );
  if ( rc != 0 )
  {
    close( pfd[ 0 ] );
    unlink( cfile.c_str() );
    unlink( efile.c_str() );
    err = "cannot start " + runner + ": " + std::strerror( rc );
    return 0;
  }
  auto job = std::make_unique< Job >();
  Job* jp = job.get();
  jp->pid = pid;
  const int rfd = pfd[ 0 ];
  jp->th = std::thread( [ jp, rfd, pid, cfile, efile ] {
    const auto t0 = std::chrono::steady_clock::now();
    std::string out;
    char buf[ 4096 ];
    ssize_t n;
    while ( ( n = read( rfd, buf, sizeof buf ) ) > 0 || ( n < 0 && errno == EINTR ) )
      if ( n > 0 )
        out.append( buf, size_t( n ) );
    close( rfd );
    int status = -1;
    while ( waitpid( pid, &status, 0 ) < 0 && errno == EINTR )
    {
    }
    {
      std::lock_guard< std::mutex > l( jp->mu );
      jp->pid = 0;  // reaped: a late cancel must not signal a recycled pid
    }
    std::string e;
    {
      std::ifstream f( efile );
      std::stringstream ss;
      ss << f.rdbuf();
      e = ss.str();
    }
    unlink( cfile.c_str() );
    unlink( efile.c_str() );
    jp->ms = std::chrono::duration< double, std::milli >( std::chrono::steady_clock::now() - t0 ).count();
    jp->ok = !jp->cancelled && WIFEXITED( status ) && WEXITSTATUS( status ) == 0 && !out.empty();
    jp->out = out;
    jp->error = jp->ok ? "" : ( jp->cancelled ? "cancelled" : ( e.empty() ? "spike-run failed (status " + std::to_string( status ) + ")" : e ) );
    jp->done = true;
  } );
  std::lock_guard< std::mutex > l( g_mu );
  const long id = g_next++;
  g_jobs[ id ] = std::move( job );
  return id;
}

bool headless_cancel( long id )
{
  std::unique_ptr< Job > j;
  {
    std::lock_guard< std::mutex > l( g_mu );
    auto it = g_jobs.find( id );
    if ( it == g_jobs.end() )
      return false;
    j = std::move( it->second );
    g_jobs.erase( it );
  }
  const bool was_running = !j->done;
  j->cancelled = true;
  kill_job( *j );
  if ( j->th.joinable() )
    j->th.join();  // prompt: the child is dead, the thread only reaps it and deletes the temp files
  return was_running;
}

std::string headless_poll( long id )
{
  std::unique_ptr< Job > j;
  {
    std::lock_guard< std::mutex > l( g_mu );
    auto it = g_jobs.find( id );
    if ( it == g_jobs.end() )
      return "{\"done\":true,\"ok\":false,\"error\":\"unknown job\",\"ms\":0}";
    if ( !it->second->done )
      return "{\"done\":false,\"ok\":false,\"error\":\"\",\"ms\":0}";
    j = std::move( it->second );
    g_jobs.erase( it );
  }
  if ( j->th.joinable() )
    j->th.join();
  if ( !j->ok )
    return std::string( "{\"done\":true,\"ok\":false,\"ms\":" ) + std::to_string( j->ms ) + ",\"error\":" + error_json( j->error ).substr( 10 );
  return std::string( "{\"done\":true,\"ok\":true,\"error\":\"\",\"ms\":" ) + std::to_string( j->ms ) + ",\"result\":" + j->out + "}";
}
} // namespace spike_gd
