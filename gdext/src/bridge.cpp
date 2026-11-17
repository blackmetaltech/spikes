#include "bridge.h"
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
#include <unistd.h>

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
  bool ok = false;
  std::string error, out;
  double ms = 0;
};
std::mutex g_mu;
std::map< long, std::unique_ptr< Job > > g_jobs;
long g_next = 1;
std::string g_runner;

bool exists( const std::string& p ) { return access( p.c_str(), X_OK ) == 0; }
std::string find_runner()
{
  std::string r = g_runner;
  if ( r.empty() )
    if ( const char* e = std::getenv( "SPIKE_RUN" ) )
      r = e;
  if ( !r.empty() )
    return r;
  for ( const char* c : { "./spike-run", "./bin/spike-run", "../spike-run", "build/bin/spike-run", "build/engine/spike-run", "../build/bin/spike-run", "../build/engine/spike-run", "../../build/bin/spike-run" } )
    if ( exists( c ) )
      return c;
  return "spike-run";  // PATH
}
std::string shq( const std::string& s )
{
  std::string o = "'";
  for ( char c : s )
    if ( c == '\'' )
      o += "'\\''";
    else
      o += c;
  return o + "'";
}
bool word( const std::string& s )
{
  for ( unsigned char c : s )
    if ( !( std::isalnum( c ) || c == '_' || c == '-' ) )
      return false;
  return true;
}
struct Cleanup
{
  ~Cleanup()
  {
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

long headless_start( const std::string& task_id, const std::string& circuit_json, const std::vector< long long >& seeds_in, const std::string& mode,
  const std::string& variant, std::string& err )
{
  if ( task_id.empty() || !word( task_id ) || !word( mode ) || !word( variant ) )
  {
    err = "bad task id, mode or variant";
    return 0;
  }
  std::vector< long long > seeds = seeds_in;
  if ( seeds.empty() )
    seeds.push_back( 1 );
  static std::atomic< long > counter{ 0 };
  const std::string base = "/tmp/spike_hl_" + std::to_string( long( getpid() ) ) + "_" + std::to_string( ++counter );
  const std::string cfile = base + ".json";
  {
    std::ofstream f( cfile );
    if ( !f )
    {
      err = "cannot write " + cfile;
      return 0;
    }
    f << circuit_json;
  }
  std::string runner;
  {
    std::lock_guard< std::mutex > l( g_mu );
    runner = find_runner();
  }
  std::string cmd = shq( runner ) + " --task " + shq( task_id ) + " --circuit " + shq( cfile ) + " --seeds ";
  for ( size_t i = 0; i < seeds.size(); ++i )
    cmd += ( i ? "," : "" ) + std::to_string( seeds[ i ] );
  if ( !mode.empty() )
    cmd += " --mode " + shq( mode );
  if ( !variant.empty() )
    cmd += " --variant " + shq( variant );
  cmd += " 2>" + shq( base + ".err" );
  auto job = std::make_unique< Job >();
  Job* jp = job.get();
  jp->th = std::thread( [ jp, cmd, cfile, base ] {
    const auto t0 = std::chrono::steady_clock::now();
    std::string out;
    int status = -1;
    if ( FILE* p = popen( cmd.c_str(), "r" ) )
    {
      char buf[ 4096 ];
      size_t n;
      while ( ( n = fread( buf, 1, sizeof buf, p ) ) > 0 )
        out.append( buf, n );
      status = pclose( p );
    }
    std::string e;
    {
      std::ifstream f( base + ".err" );
      std::stringstream ss;
      ss << f.rdbuf();
      e = ss.str();
    }
    std::remove( cfile.c_str() );
    std::remove( ( base + ".err" ).c_str() );
    jp->ms = std::chrono::duration< double, std::milli >( std::chrono::steady_clock::now() - t0 ).count();
    jp->ok = status == 0 && !out.empty();
    jp->out = out;
    jp->error = jp->ok ? "" : ( e.empty() ? "spike-run failed (status " + std::to_string( status ) + ")" : e );
    jp->done = true;
  } );
  std::lock_guard< std::mutex > l( g_mu );
  const long id = g_next++;
  g_jobs[ id ] = std::move( job );
  return id;
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
