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
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>  // _NSGetExecutablePath
#include <crt_externs.h>  // _NSGetEnviron
#endif

#ifdef __APPLE__
#define environ ( *_NSGetEnviron() )  // `environ` is not linkable from a dylib on macOS
#else
extern char** environ;
#endif

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
#ifdef _WIN32
  int pid = 0;  // process id while the child runs, 0 once reaped
  HANDLE proc = nullptr, jobobj = nullptr;  // the child, and the Win32 job object that holds its whole process tree (guarded by mu)
#else
  pid_t pid = 0;
#endif
  bool ok = false;
  std::string error, out;
  double ms = 0;
};
std::mutex g_mu;
std::map< long, std::unique_ptr< Job > > g_jobs;
long g_next = 1;
std::string g_runner;
std::vector< std::string > g_dirs;  // extra search directories (e.g. the project's res://bin, globalized)

#ifdef _WIN32
// Win32: paths are UTF-8 inside this file (Godot hands them over that way) and wide for the system calls.
std::wstring wide( const std::string& s )
{
  if ( s.empty() )
    return std::wstring();
  const int n = MultiByteToWideChar( CP_UTF8, 0, s.data(), int( s.size() ), nullptr, 0 );
  std::wstring w( size_t( n > 0 ? n : 0 ), L'\0' );
  if ( n > 0 )
    MultiByteToWideChar( CP_UTF8, 0, s.data(), int( s.size() ), w.data(), n );
  return w;
}
std::string narrow( const std::wstring& w )
{
  if ( w.empty() )
    return std::string();
  const int n = WideCharToMultiByte( CP_UTF8, 0, w.data(), int( w.size() ), nullptr, 0, nullptr, nullptr );
  std::string s( size_t( n > 0 ? n : 0 ), '\0' );
  if ( n > 0 )
    WideCharToMultiByte( CP_UTF8, 0, w.data(), int( w.size() ), s.data(), n, nullptr, nullptr );
  return s;
}
bool exists( const std::string& p )  // a file that is not a directory (there is no execute bit to test)
{
  if ( p.empty() )
    return false;
  const DWORD a = GetFileAttributesW( wide( p ).c_str() );
  return a != INVALID_FILE_ATTRIBUTES && !( a & FILE_ATTRIBUTE_DIRECTORY );
}
std::string dirname_of( const std::string& p )
{
  const auto i = p.find_last_of( "/\\" );
  return i == std::string::npos ? std::string( "." ) : ( i == 0 ? std::string( "/" ) : p.substr( 0, i ) );
}
std::string module_dir( HMODULE m )
{
  std::wstring buf( 32768, L'\0' );
  const DWORD n = GetModuleFileNameW( m, buf.data(), DWORD( buf.size() ) );
  if ( n == 0 || n >= buf.size() )
    return std::string();
  std::string d = dirname_of( narrow( buf.substr( 0, n ) ) );
  std::replace( d.begin(), d.end(), '\\', '/' );  // forward slashes, as Godot reports paths
  return d;
}
std::string self_exe_dir() { return module_dir( nullptr ); }
std::string self_lib_dir()  // the directory of this DLL (the GDExtension), from the address of one of its own functions
{
  HMODULE m = nullptr;
  if ( GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast< LPCWSTR >( &headless_set_runner ), &m ) )
    return module_dir( m );
  return std::string();
}
#else
bool exists( const std::string& p ) { return !p.empty() && access( p.c_str(), X_OK ) == 0; }
std::string dirname_of( const std::string& p )
{
  const auto i = p.rfind( '/' );
  return i == std::string::npos ? std::string( "." ) : ( i == 0 ? std::string( "/" ) : p.substr( 0, i ) );
}
std::string self_exe_dir()
{
  char buf[ 4096 ];
#ifdef __APPLE__
  uint32_t sz = sizeof buf;
  if ( _NSGetExecutablePath( buf, &sz ) != 0 )
    return std::string();
  char real[ PATH_MAX ];
  return dirname_of( realpath( buf, real ) ? std::string( real ) : std::string( buf ) );
#else
  const ssize_t n = readlink( "/proc/self/exe", buf, sizeof buf - 1 );
  return n > 0 ? dirname_of( std::string( buf, size_t( n ) ) ) : std::string();
#endif
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
#endif
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
#ifdef _WIN32
    for ( std::string d; std::getline( ss, d, ';' ); )
#else
    for ( std::string d; std::getline( ss, d, ':' ); )
#endif
      if ( !d.empty() )
        cands.push_back( d + "/spike-run" );
  }
#ifdef _WIN32
  for ( auto& c : cands )  // the runner is spike-run.exe here (an explicit path or $SPIKE_RUN may already end in .exe)
    if ( c.size() < 4 || _stricmp( c.c_str() + c.size() - 4, ".exe" ) != 0 )
      c += ".exe";
#endif
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
#ifdef _WIN32
// GetTempFileNameW in the user's temp dir (%TMP%/%TEMP%): unique, created exclusively; opened for writing here. Returns the fd (or -1) and the UTF-8 path.
int make_temp( const char* tag, std::string& path )
{
  wchar_t dir[ MAX_PATH + 1 ];
  const DWORD n = GetTempPathW( MAX_PATH, dir );
  if ( n == 0 || n > MAX_PATH )
    return -1;
  wchar_t file[ MAX_PATH + 1 ];
  const std::wstring prefix = L"sp" + wide( tag ).substr( 0, 1 );
  if ( GetTempFileNameW( dir, prefix.c_str(), 0, file ) == 0 )
    return -1;
  const int fd = _wopen( file, _O_WRONLY | _O_BINARY );
  if ( fd >= 0 )
    path = narrow( file );
  else
    DeleteFileW( file );
  return fd;
}
void kill_job( Job& j )
{
  std::lock_guard< std::mutex > l( j.mu );
  if ( j.pid > 0 && j.jobobj )
    TerminateJobObject( j.jobobj, 1 );  // the whole tree: the child and anything it started
  else if ( j.pid > 0 && j.proc )
    TerminateProcess( j.proc, 1 );
}
#else
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
#endif
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
#ifdef _WIN32
  // Win32: stdout is an anonymous pipe, stderr the temp file, stdin NUL; only those three handles are inherited (a handle list), so concurrent jobs
  // never leak pipe ends into each other's children. The child runs in a job object (kill-on-close) so cancel takes the whole tree down, like the
  // process group on POSIX, and without a console window (this process is the GUI game).
  int pfd[ 2 ] = { -1, -1 };
  int pid = 0;
  HANDLE hproc = nullptr, hjob = nullptr;
  {
    auto quote = []( const std::string& a ) {  // CommandLineToArgvW rules
      if ( !a.empty() && a.find_first_of( " \t\n\v\"" ) == std::string::npos )
        return a;
      std::string q = "\"";
      for ( size_t i = 0;; ++i )
      {
        size_t bs = 0;
        for ( ; i < a.size() && a[ i ] == '\\'; ++i )
          ++bs;
        if ( i == a.size() )
        {
          q.append( bs * 2, '\\' );
          break;
        }
        if ( a[ i ] == '"' )
          q.append( bs * 2 + 1, '\\' ).push_back( '"' );
        else
          q.append( bs, '\\' ).push_back( a[ i ] );
      }
      return q + "\"";
    };
    std::string cmd;
    for ( const auto& a : args )
      cmd += ( cmd.empty() ? "" : " " ) + quote( a );
    std::wstring wcmd = wide( cmd );
    wcmd.push_back( L'\0' );
    SECURITY_ATTRIBUTES sa{ sizeof sa, nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr, he = nullptr, nul = INVALID_HANDLE_VALUE;
    bool ok = CreatePipe( &rd, &wr, &sa, 0 ) != 0;
    if ( ok )
      SetHandleInformation( rd, HANDLE_FLAG_INHERIT, 0 );
    ok = ok && DuplicateHandle( GetCurrentProcess(), reinterpret_cast< HANDLE >( _get_osfhandle( efd ) ), GetCurrentProcess(), &he, 0, TRUE, DUPLICATE_SAME_ACCESS );
    nul = CreateFileW( L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr );
    ok = ok && nul != INVALID_HANDLE_VALUE;
    DWORD lerr = 0;
    PROCESS_INFORMATION pi{};
    if ( ok )
    {
      SIZE_T asz = 0;
      InitializeProcThreadAttributeList( nullptr, 1, 0, &asz );
      std::vector< char > ab( asz );
      auto* al = reinterpret_cast< LPPROC_THREAD_ATTRIBUTE_LIST >( ab.data() );
      HANDLE hl[ 3 ] = { nul, wr, he };
      STARTUPINFOEXW si{};
      si.StartupInfo.cb = sizeof si;
      si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
      si.StartupInfo.hStdInput = nul;
      si.StartupInfo.hStdOutput = wr;
      si.StartupInfo.hStdError = he;
      si.lpAttributeList = al;
      ok = InitializeProcThreadAttributeList( al, 1, 0, &asz ) && UpdateProcThreadAttribute( al, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, hl, sizeof hl, nullptr, nullptr )
        && CreateProcessW( wide( runner ).c_str(), wcmd.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT | CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr,
             &si.StartupInfo, &pi );
      if ( !ok )
        lerr = GetLastError();
      DeleteProcThreadAttributeList( al );
    }
    else
      lerr = GetLastError();
    if ( wr )
      CloseHandle( wr );
    if ( he )
      CloseHandle( he );
    if ( nul != INVALID_HANDLE_VALUE )
      CloseHandle( nul );
    close( efd );
    if ( ok )
    {
      hjob = CreateJobObjectW( nullptr, nullptr );
      if ( hjob )
      {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if ( !SetInformationJobObject( hjob, JobObjectExtendedLimitInformation, &li, sizeof li ) || !AssignProcessToJobObject( hjob, pi.hProcess ) )
        {
          CloseHandle( hjob );  // cancel then falls back to TerminateProcess on the child alone
          hjob = nullptr;
        }
      }
      ResumeThread( pi.hThread );
      CloseHandle( pi.hThread );
      hproc = pi.hProcess;
      pid = int( pi.dwProcessId );
      pfd[ 0 ] = _open_osfhandle( reinterpret_cast< intptr_t >( rd ), _O_RDONLY | _O_BINARY );
    }
    else
    {
      if ( rd )
        CloseHandle( rd );
      unlink( cfile.c_str() );
      unlink( efile.c_str() );
      err = "cannot start " + runner + ": Windows error " + std::to_string( lerr );
      return 0;
    }
  }
#else
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
#endif
  auto job = std::make_unique< Job >();
  Job* jp = job.get();
  jp->pid = pid;
#ifdef _WIN32
  jp->proc = hproc;
  jp->jobobj = hjob;
#endif
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
#ifdef _WIN32
    {
      DWORD code = 1;
      WaitForSingleObject( jp->proc, INFINITE );
      GetExitCodeProcess( jp->proc, &code );
      status = int( code );
    }
#else
    while ( waitpid( pid, &status, 0 ) < 0 && errno == EINTR )
    {
    }
#endif
    {
      std::lock_guard< std::mutex > l( jp->mu );
      jp->pid = 0;  // reaped: a late cancel must not signal a recycled pid
#ifdef _WIN32
      CloseHandle( jp->proc );
      jp->proc = nullptr;
      if ( jp->jobobj )
        CloseHandle( jp->jobobj );  // kill-on-close: nothing the child left behind survives
      jp->jobobj = nullptr;
#endif
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
    #ifdef _WIN32
    jp->ok = !jp->cancelled && status == 0 && !out.empty();
#else
    jp->ok = !jp->cancelled && WIFEXITED( status ) && WEXITSTATUS( status ) == 0 && !out.empty();
#endif
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
