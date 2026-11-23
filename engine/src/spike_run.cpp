// spike-run <circuit.json> <ms> [--threads n]: loads a circuit, runs it, prints one JSON line per spike:
//   {"neuron":3,"t":12.5,"step":125}   (neuron = circuit-level index, t in ms, step = NEST time stamp in steps)
// sorted by (step, neuron). A summary line goes to stderr so stdout holds spikes only.
//
// spike-run --locked-digest <circuit.json> prints the circuit's LockedDigest (0.6, Atlas).
// spike-run --list-tasks prints the registered task ids, one per line. spike-run --list-variants <task> prints the ids that task accepts for --variant.
// spike-run --task <id> [--quick] [--jobs n]   (--quick: run Task::quick_duration_ms() instead of the full episode; --jobs: seeds run in parallel child processes, default = cores, 1 = in-process)
// spike-run --task <id> [--circuit file.json] [--seed n | --seeds 1,2,3] [--circuit-seed n | --circuit-seeds a,b,c] [--lesion pre>post*k ...] [--require any|all] [--circuits-dir dir]
//   [--circuit-seed n | --circuit-seeds a,b,c]: pin the circuit seed (one for all seeds, or one per seed) instead of the 1000 * seed + circuit seed reseed,
//   so an instant check scores the same circuit seed as the live run.
//   scores a circuit on a task headless, exactly as the game does (same 1 ms slices, a seed also reseeds the circuit: 1000 * seed + circuit seed),
//   and prints one JSON object on stdout: per-seed value / passed / verdict / parts / neurons / spikes / rate_hz_per_neuron, plus mean and pass count.
//   --mode guided|standard|researcher and --variant <id> set the v0.3 difficulty mode and challenge variant (Task::set_mode / set_variant) before reset;
//   each run also reports the task's hints and metrics. The game's instant check (SpikeTask.score_headless_async) runs this CLI in a worker thread.
//   The circuit defaults to the task's reference circuit (looked up in $SPIKE_CIRCUITS_DIR, --circuits-dir, or a circuits/ folder above the cwd or
//   the executable). Seeds default to 1. A lesion scales the weight of every projection pre->post (0 deletes it). Exit status: 0 ok, 1 load or run
//   error, 2 usage, 3 the --require condition failed (any: at least one seed passed, all: every seed passed). Used by CI and scripts/smoke.sh.
#include <cstdio>
#include "spike/theory.h"
#include <cerrno>
#include <deque>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <thread>
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "nlohmann/json.hpp"
#include "spike/engine.h"
#include "spike/task.h"

using nlohmann::json;

static bool exists( const std::string& p )
{
  std::ifstream f( p );
  return static_cast< bool >( f );
}

/// The folder that holds the reference circuits: explicit, $SPIKE_CIRCUITS_DIR, then circuits/ (or godot/circuits/) in the cwd and up to 6 parents, then the same from argv[0].
static std::string find_circuits_dir( const std::string& given, const std::string& rel, const char* argv0 )
{
  std::vector< std::string > cands;
  if ( not given.empty() )
    cands.push_back( given );
  if ( const char* e = std::getenv( "SPIKE_CIRCUITS_DIR" ) )
    cands.push_back( e );
  std::string up = "";
  for ( int i = 0; i < 7; ++i, up += "../" )
  {
    cands.push_back( up + "circuits" );
    cands.push_back( up + "godot/circuits" );  // the Godot project is the single source (repo-root circuits/ is a symlink to it)
  }
  std::string exe = argv0;
  auto slash = exe.find_last_of( '/' );
  if ( slash != std::string::npos )
  {
#ifdef __APPLE__
    cands.push_back( exe.substr( 0, slash + 1 ) + "../Resources/circuits" );  // Spike.app: spike-run is in Contents/Frameworks, circuits in Contents/Resources
#endif
    up = exe.substr( 0, slash + 1 );
    for ( int i = 0; i < 7; ++i, up += "../" )
    {
      cands.push_back( up + "circuits" );  // exported game: circuits/ next to spike-run; dev tree: godot/bin/../circuits
      cands.push_back( up + "godot/circuits" );
    }
  }
  for ( const auto& c : cands )
    if ( exists( c + "/" + rel ) )
      return c;
  return "";
}

/// Strict number parsing for the command line: the whole token must be a number (no "x", no "12abc"), else false.
static bool parse_ull( const std::string& tok, unsigned long long& out )
{
  if ( tok.empty() or tok[ 0 ] == '-' or tok[ 0 ] == '+' )
    return false;
  char* end = nullptr;
  errno = 0;
  out = std::strtoull( tok.c_str(), &end, 10 );
  return errno == 0 and end and *end == '\0';
}

static bool parse_double( const std::string& tok, double& out )
{
  if ( tok.empty() )
    return false;
  char* end = nullptr;
  errno = 0;
  out = std::strtod( tok.c_str(), &end );
  return errno == 0 and end and *end == '\0' and std::isfinite( out );
}

/// True when the JSON text nests brackets deeper than `limit` (a cheap scan, so a hostile file never reaches a recursive walk).
static bool too_deep( const std::string& text, int limit = 64 )
{
  int depth = 0;
  bool in_str = false, esc = false;
  for ( char ch : text )
  {
    if ( in_str )
    {
      if ( esc )
        esc = false;
      else if ( ch == '\\' )
        esc = true;
      else if ( ch == '"' )
        in_str = false;
    }
    else if ( ch == '"' )
      in_str = true;
    else if ( ch == '[' or ch == '{' )
    {
      if ( ++depth > limit )
        return true;
    }
    else if ( ch == ']' or ch == '}' )
      --depth;
  }
  return false;
}

/// Seeds: 1 .. kMaxTaskSeed. The circuit is reseeded with 1000 * seed + its own seed, which NEST accepts up to 2^32 - 1.
static constexpr unsigned long long kMaxTaskSeed = 4000000ULL;

struct Lesion
{
  std::string pre, post;
  double factor;
};

static int run_task_mode_impl( int argc, char** argv );

static int run_task_mode( int argc, char** argv )
{
  try
  {
    return run_task_mode_impl( argc, argv );
  }
  catch ( const std::exception& e )
  {
    std::fprintf( stderr, "spike-run: %s\n", e.what() );
    return 1;
  }
}

static int run_task_mode_impl( int argc, char** argv )
{
  std::string task_id, circuit_file, dir, require, mode, variant;
  std::vector< unsigned long long > seeds, circuit_seeds;
  std::vector< Lesion > lesions;
  int threads = 1, jobs = 0;
  bool quick = false;
#ifdef _WIN32
  int worker = -1;
#endif
  auto usage = [] {
    std::fprintf( stderr, "usage: spike-run --task <id> [--circuit file.json] [--seed n | --seeds 1,2,3] [--lesion pre>post*k]... "
                          "[--mode guided|standard|researcher] [--variant id] [--require any|all] [--circuits-dir dir] [--threads n] [--quick] [--jobs n]\n" );
    return 2;
  };
  for ( int i = 1; i < argc; ++i )
  {
    std::string a = argv[ i ];
    const bool more = i + 1 < argc;
    if ( a == "--task" and more )
      task_id = argv[ ++i ];
    else if ( a == "--circuit" and more )
      circuit_file = argv[ ++i ];
    else if ( a == "--circuits-dir" and more )
      dir = argv[ ++i ];
    else if ( a == "--require" and more )
      require = argv[ ++i ];
    else if ( a == "--mode" and more )
      mode = argv[ ++i ];
    else if ( a == "--variant" and more )
      variant = argv[ ++i ];
    else if ( a == "--threads" and more )
    {
      unsigned long long v = 0;
      if ( not parse_ull( argv[ ++i ], v ) or v < 1 or v > 1024 )
      {
        std::fprintf( stderr, "spike-run: --threads needs a whole number of at least 1\n" );
        return 2;
      }
      threads = static_cast< int >( v );
    }
    else if ( a == "--jobs" and more )
    {
      unsigned long long v = 0;
      if ( not parse_ull( argv[ ++i ], v ) or v > 1024 )
      {
        std::fprintf( stderr, "spike-run: --jobs needs a whole number (0 = one per core)\n" );
        return 2;
      }
      jobs = static_cast< int >( v );
    }
    else if ( a == "--quick" )
      quick = true;
#ifdef _WIN32
    else if ( a == "--spike-worker" and more )  // internal (Windows has no fork): run exactly seed index N in this process, print its JSON, exit
      worker = std::atoi( argv[ ++i ] );
#endif
    else if ( ( a == "--seed" or a == "--seeds" ) and more )
    {
      std::stringstream ss( argv[ ++i ] );
      for ( std::string tok; std::getline( ss, tok, ',' ); )
      {
        unsigned long long v = 0;
        if ( not parse_ull( tok, v ) or v < 1 or v > kMaxTaskSeed )
        {
          std::fprintf( stderr, "spike-run: seed '%s' is not a whole number from 1 to %llu\n", tok.c_str(), kMaxTaskSeed );
          return 2;
        }
        seeds.push_back( v );
      }
    }
    else if ( ( a == "--circuit-seed" or a == "--circuit-seeds" ) and more )
    {
      std::stringstream ss( argv[ ++i ] );
      for ( std::string tok; std::getline( ss, tok, ',' ); )
      {
        unsigned long long v = 0;
        if ( not parse_ull( tok, v ) or v < 1 or v > 4294967295ULL )
        {
          std::fprintf( stderr, "spike-run: circuit seed '%s' is not a whole number from 1 to 4294967295\n", tok.c_str() );
          return 2;
        }
        circuit_seeds.push_back( v );
      }
    }
    else if ( a == "--lesion" and more )
    {
      std::string l = argv[ ++i ];
      auto gt = l.find( '>' ), st = l.find( '*' );
      if ( gt == std::string::npos )
        return usage();
      Lesion x{ l.substr( 0, gt ), l.substr( gt + 1, st == std::string::npos ? std::string::npos : st - gt - 1 ), 0.0 };
      if ( st != std::string::npos and not parse_double( l.substr( st + 1 ), x.factor ) )
      {
        std::fprintf( stderr, "spike-run: --lesion %s: the factor after '*' is not a number\n", l.c_str() );
        return 2;
      }
      lesions.push_back( x );
    }
    else
    {
      std::fprintf( stderr, "spike-run: unknown argument '%s'\n", a.c_str() );
      return usage();
    }
  }
  if ( task_id.empty() or ( not require.empty() and require != "any" and require != "all" ) )
    return usage();
  if ( seeds.empty() )
    seeds.push_back( 1 );
  if ( not circuit_seeds.empty() and circuit_seeds.size() != 1 and circuit_seeds.size() != seeds.size() )
  {
    std::fprintf( stderr, "spike-run: --circuit-seeds needs one value (for every seed) or one per seed (%zu)\n", seeds.size() );
    return 2;
  }
  auto probe = spike::make_task( task_id );
  if ( not probe )
  {
    std::fprintf( stderr, "spike-run: unknown task '%s'\n", task_id.c_str() );
    return 2;
  }
  if ( not mode.empty() and mode != "guided" and mode != "standard" and mode != "researcher" )
  {
    std::fprintf( stderr, "spike-run: unknown --mode '%s' (guided, standard or researcher)\n", mode.c_str() );
    return 2;
  }
  if ( not variant.empty() )
  {
    bool known = probe->variants().count( variant ) > 0;
    for ( const auto& c : probe->diagnosis().candidates )
      known = known or c == variant;
    if ( not known )
    {
      std::fprintf( stderr, "spike-run: task '%s' has no variant '%s' (spike-run --list-variants %s lists them)\n", task_id.c_str(), variant.c_str(), task_id.c_str() );
      return 2;
    }
  }
  const bool locked_level = not probe->given().digest.empty();   // 0.6 (Atlas): a lesion must not edit the file (that would be tampering); it is applied after the load
  probe.reset();
  if ( circuit_file.empty() )
  {
    const std::string rel = spike::reference_circuit( task_id );
    const std::string d = find_circuits_dir( dir, rel, argv[ 0 ] );
    if ( d.empty() )
    {
      std::fprintf( stderr, "spike-run: cannot find the reference circuit %s (set SPIKE_CIRCUITS_DIR or pass --circuit)\n", rel.c_str() );
      return 2;
    }
    circuit_file = d + "/" + rel;
  }
  std::ifstream f( circuit_file );
  if ( not f )
  {
    std::fprintf( stderr, "spike-run: cannot open %s\n", circuit_file.c_str() );
    return 2;
  }
  std::stringstream buf;
  buf << f.rdbuf();
  if ( too_deep( buf.str() ) )
  {
    std::fprintf( stderr, "spike-run: %s is nested too deeply to be a circuit\n", circuit_file.c_str() );
    return 2;
  }
  json base = json::parse( buf.str(), nullptr, false );
  if ( base.is_discarded() or not base.is_object() )
  {
    std::fprintf( stderr, "spike-run: %s is not a JSON circuit\n", circuit_file.c_str() );
    return 2;
  }
  if ( not base.contains( "seed" ) )
    base[ "seed" ] = 1;
  if ( not base[ "seed" ].is_number_integer() )
  {
    std::fprintf( stderr, "spike-run: %s: \"seed\" must be an integer\n", circuit_file.c_str() );
    return 2;
  }
  for ( const auto& l : lesions )
  {
    if ( locked_level )
      break;
    bool hit = false;
    if ( not base.contains( "projections" ) or not base[ "projections" ].is_array() )
      break;
    for ( auto& p : base[ "projections" ] )
    {
      if ( not p.is_object() or p.value( "pre", std::string() ) != l.pre or p.value( "post", std::string() ) != l.post )
        continue;
      hit = true;
      try
      {
        if ( p.contains( "synapse" ) and p[ "synapse" ].is_object() and p[ "synapse" ].contains( "weight" ) )
          p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * l.factor;
        if ( p.contains( "edges" ) )
          for ( auto& e : p[ "edges" ] )
            e[ 2 ] = e[ 2 ].get< double >() * l.factor;
      }
      catch ( const std::exception& )
      {
        std::fprintf( stderr, "spike-run: --lesion %s>%s: this projection's weights are not plain numbers, so they cannot be scaled\n", l.pre.c_str(), l.post.c_str() );
        return 2;
      }
    }
    if ( not hit )
      std::fprintf( stderr, "spike-run: warning: lesion %s>%s matched no projection\n", l.pre.c_str(), l.post.c_str() );
  }

  spike::set_threads( threads );
  json out;
  out[ "task" ] = task_id;
  out[ "mode" ] = mode.empty() ? "standard" : mode;
  out[ "variant" ] = variant;
  out[ "circuit" ] = circuit_file;
  out[ "quick" ] = quick;
  out[ "runs" ] = json::array();

  // One seed: returns the run's JSON, or {"error": "..."} on a load / run failure. Needs a fresh process for every Engine (one NEST kernel per process).
  auto run_seed = [ & ]( size_t si ) -> json {
    const auto seed = seeds[ si ];
    json c = base;
    // default: task seed also reseeds the circuit (1000 * seed + circuit seed); --circuit-seed(s) pins the circuit seed exactly (the live run's)
    c[ "seed" ] = circuit_seeds.empty() ? 1000 * seed + base[ "seed" ].get< unsigned long long >() : circuit_seeds[ circuit_seeds.size() == 1 ? 0 : si ];
    if ( c[ "seed" ].get< unsigned long long >() > 4294967295ULL or c[ "seed" ].get< unsigned long long >() < 1 )
      return { { "error", "seed " + std::to_string( seed ) + ": the circuit seed 1000 * seed + " + base[ "seed" ].dump() + " is outside 1 .. 4294967295 (use a smaller --seed)" } };
    spike::Engine e;
    if ( not e.load( c.dump() ) )
      return { { "error", "seed " + std::to_string( seed ) + ": " + e.error() } };
    if ( locked_level and not lesions.empty() )
    {
      // a locked level's lesion is an experiment on the loaded circuit, not an edit of the file: the weights of every edge of the projection pre->post are scaled in place
      const auto st = e.structure();
      const auto conns = e.connections();
      for ( const auto& l : lesions )
      {
        bool hit = false;
        for ( const auto& pi : st.projections )
        {
          if ( pi.pre_pop != l.pre or pi.post_pop != l.post )
            continue;
          hit = true;
          const json& edges = base[ "projections" ][ pi.index ][ "edges" ];
          std::vector< bool > used( conns.size(), false );
          for ( size_t k = 0; k < edges.size(); ++k )
          {
            const int32_t a = st.populations[ pi.pre_index ].first_index + edges[ k ][ 0 ].get< int32_t >();
            const int32_t b = st.populations[ pi.post_index ].first_index + edges[ k ][ 1 ].get< int32_t >();
            for ( size_t q = 0; q < conns.size(); ++q )
              if ( not used[ q ] and conns[ q ].projection == pi.index and conns[ q ].pre == a and conns[ q ].post == b )
              {
                used[ q ] = true;
                e.set_weight( pi.index, static_cast< int32_t >( k ), conns[ q ].weight * l.factor );
                break;
              }
          }
        }
        if ( not hit )
          return { { "error", "lesion " + l.pre + ">" + l.post + " matched no projection" } };
      }
    }
    auto task = spike::make_task( task_id );
    task->set_mode( mode.empty() ? "standard" : mode );
    task->set_variant( variant );
    task->set_quick( quick );
    task->reset( seed );
    unsigned long long spikes = 0;
    const double dur = quick ? task->quick_duration_ms() : task->duration_ms();
    const long steps = static_cast< long >( dur );
    for ( long s = 0; s < steps; ++s )
    {
      task->step( e, 1.0 );
      if ( not e.last_step_ok() and not e.error().empty() )
        break;  // a failed step is final: report it instead of running on
      if ( s % 500 == 499 )
        spikes += e.drain_spikes().size();
    }
    spikes += e.drain_spikes().size();
    if ( not e.error().empty() )
      return { { "error", "seed " + std::to_string( seed ) + ": " + e.error() } };
    const auto sc = task->score();
    json r;
    r[ "seed" ] = seed;
    r[ "circuit_seed" ] = c[ "seed" ];
    r[ "value" ] = sc.value;
    r[ "passed" ] = sc.passed;
    r[ "verdict" ] = sc.verdict;
    r[ "parts" ] = json::object();
    for ( const auto& [ k, v ] : sc.parts )
      r[ "parts" ][ k ] = v;
    r[ "hints" ] = json::array();
    for ( const auto& h : task->hints() )
      r[ "hints" ].push_back( { { "tier", h.tier }, { "text", h.text }, { "target", h.target } } );
    r[ "metrics" ] = json::object();
    for ( const auto& [ k, m ] : task->metrics() )
      r[ "metrics" ][ k ] = { { "value", m.value }, { "unit", m.unit }, { "lower_is_better", m.lower_is_better } };
    r[ "neurons" ] = e.neuron_count();
    r[ "spikes" ] = spikes;
    r[ "duration_ms" ] = dur;
    r[ "rate_hz_per_neuron" ] = e.neuron_count() > 0 ? spikes / double( e.neuron_count() ) / ( dur * 0.001 ) : 0.0;
    return r;
  };

#ifdef _WIN32
  if ( worker >= 0 )
  {
    if ( static_cast< size_t >( worker ) >= seeds.size() )
      return 2;
    _setmode( _fileno( stdout ), _O_BINARY );
    const std::string s = run_seed( static_cast< size_t >( worker ) ).dump();
    return std::fwrite( s.data(), 1, s.size(), stdout ) == s.size() and std::fflush( stdout ) == 0 ? 0 : 1;
  }
#endif
  // Seeds run in parallel child processes (fork before any engine exists in this process), at most `jobs` at a time; results keep seed order.
  std::vector< json > results( seeds.size() );
  if ( jobs <= 0 )
  {
#ifdef _WIN32
    const unsigned nc = std::thread::hardware_concurrency();
#else
    const long nc = sysconf( _SC_NPROCESSORS_ONLN );
#endif
    jobs = nc > 0 ? static_cast< int >( nc ) : 1;
  }
  jobs = std::max( 1, std::min< int >( jobs, static_cast< int >( seeds.size() ) ) );
  if ( jobs == 1 )
  {
    for ( size_t si = 0; si < seeds.size(); ++si )
    {
      results[ si ] = run_seed( si );
      if ( results[ si ].contains( "error" ) )
        break;
    }
  }
  else
#ifdef _WIN32
  {
    // No fork on Windows: each seed runs in a child process, this same executable started with `--spike-worker <index>` (same command line, so the
    // same circuit, mode and seeds), which prints that seed's JSON on stdout. Every seed gets a fresh process and a fresh NEST kernel, as with fork.
    struct Child
    {
      HANDLE proc;
      int fd;
      size_t si;
    };
    std::deque< Child > active;
    size_t next = 0;
    bool stop = false;
    auto drain = [ & ]() {
      Child ch = active.front();
      active.pop_front();
      std::string data;
      char buf[ 65536 ];
      for ( int n; ( n = read( ch.fd, buf, sizeof buf ) ) > 0; )
        data.append( buf, static_cast< size_t >( n ) );
      close( ch.fd );
      DWORD code = 1;
      WaitForSingleObject( ch.proc, INFINITE );
      GetExitCodeProcess( ch.proc, &code );
      CloseHandle( ch.proc );
      json r = code == 0 ? json::parse( data, nullptr, false ) : json( json::value_t::discarded );
      results[ ch.si ] = r.is_discarded() ? json{ { "error", "seed " + std::to_string( seeds[ ch.si ] ) + ": worker process failed" } } : r;
      if ( results[ ch.si ].contains( "error" ) )
        stop = true;
    };
    std::wstring self( 32768, L'\0' );
    self.resize( GetModuleFileNameW( nullptr, self.data(), static_cast< DWORD >( self.size() ) ) );
    const std::wstring base_cmd = GetCommandLineW();
    while ( not stop and ( next < seeds.size() or not active.empty() ) )
    {
      while ( not stop and next < seeds.size() and static_cast< int >( active.size() ) < jobs )
      {
        SECURITY_ATTRIBUTES sa{ sizeof sa, nullptr, TRUE };
        HANDLE rd = nullptr, wr = nullptr, err = nullptr, in = nullptr;
        if ( not CreatePipe( &rd, &wr, &sa, 0 ) )
        {
          results[ next ] = { { "error", "pipe failed" } };
          stop = true;
          break;
        }
        SetHandleInformation( rd, HANDLE_FLAG_INHERIT, 0 );
        // inheritable copies of our own stderr / stdin for the child (ours may not be inheritable), listed explicitly with the pipe
        for ( auto [ std_id, out_h ] : { std::pair< DWORD, HANDLE* >{ STD_ERROR_HANDLE, &err }, std::pair< DWORD, HANDLE* >{ STD_INPUT_HANDLE, &in } } )
        {
          HANDLE h = GetStdHandle( std_id );
          if ( h and h != INVALID_HANDLE_VALUE )
            DuplicateHandle( GetCurrentProcess(), h, GetCurrentProcess(), out_h, 0, TRUE, DUPLICATE_SAME_ACCESS );
        }
        std::vector< HANDLE > hl{ wr };
        if ( err )
          hl.push_back( err );
        if ( in )
          hl.push_back( in );
        SIZE_T asz = 0;
        InitializeProcThreadAttributeList( nullptr, 1, 0, &asz );
        std::vector< char > ab( asz );
        auto* al = reinterpret_cast< LPPROC_THREAD_ATTRIBUTE_LIST >( ab.data() );
        STARTUPINFOEXW si{};
        si.StartupInfo.cb = sizeof si;
        si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        si.StartupInfo.hStdOutput = wr;
        si.StartupInfo.hStdError = err ? err : wr;
        si.StartupInfo.hStdInput = in ? in : INVALID_HANDLE_VALUE;
        si.lpAttributeList = al;
        std::wstring cmd = base_cmd + L" --spike-worker " + std::to_wstring( next );
        PROCESS_INFORMATION pi{};
        const bool ok = InitializeProcThreadAttributeList( al, 1, 0, &asz )
          and UpdateProcThreadAttribute( al, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, hl.data(), hl.size() * sizeof( HANDLE ), nullptr, nullptr )
          and CreateProcessW( self.c_str(), cmd.data(), nullptr, nullptr, TRUE, EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si.StartupInfo, &pi );
        if ( al )
          DeleteProcThreadAttributeList( al );
        CloseHandle( wr );
        if ( err )
          CloseHandle( err );
        if ( in )
          CloseHandle( in );
        if ( not ok )
        {
          CloseHandle( rd );
          results[ next ] = { { "error", "could not start a worker process" } };
          stop = true;
          break;
        }
        CloseHandle( pi.hThread );
        active.push_back( { pi.hProcess, _open_osfhandle( reinterpret_cast< intptr_t >( rd ), _O_RDONLY | _O_BINARY ), next++ } );
      }
      if ( not active.empty() )
        drain();
    }
    while ( not active.empty() )
      drain();
  }
#else
  {
    struct Child
    {
      pid_t pid;
      int fd;
      size_t si;
    };
    std::deque< Child > active;
    size_t next = 0;
    bool stop = false;
    auto drain = [ & ]() {
      Child ch = active.front();
      active.pop_front();
      std::string data;
      char buf[ 65536 ];
      for ( ssize_t n; ( n = read( ch.fd, buf, sizeof buf ) ) != 0; )
      {
        if ( n < 0 and errno == EINTR )
          continue;
        if ( n < 0 )
          break;
        data.append( buf, static_cast< size_t >( n ) );
      }
      close( ch.fd );
      int status = 0;
      waitpid( ch.pid, &status, 0 );
      json r = json::parse( data, nullptr, false );
      results[ ch.si ] = r.is_discarded() ? json{ { "error", "seed " + std::to_string( seeds[ ch.si ] ) + ": worker process failed" } } : r;
      if ( results[ ch.si ].contains( "error" ) )
        stop = true;
    };
    while ( not stop and ( next < seeds.size() or not active.empty() ) )
    {
      while ( not stop and next < seeds.size() and static_cast< int >( active.size() ) < jobs )
      {
        int pfd[ 2 ];
        if ( pipe( pfd ) != 0 )
        {
          results[ next ] = { { "error", "pipe failed" } };
          stop = true;
          break;
        }
        std::fflush( nullptr );
        const pid_t pid = fork();
        if ( pid == 0 )
        {
          close( pfd[ 0 ] );
          const std::string s = run_seed( next ).dump();
          for ( size_t off = 0; off < s.size(); )
          {
            const ssize_t n = write( pfd[ 1 ], s.data() + off, s.size() - off );
            if ( n <= 0 )
              _exit( 1 );
            off += static_cast< size_t >( n );
          }
          _exit( 0 );
        }
        close( pfd[ 1 ] );
        if ( pid < 0 )
        {
          close( pfd[ 0 ] );
          results[ next ] = { { "error", "fork failed" } };
          stop = true;
          break;
        }
        active.push_back( { pid, pfd[ 0 ], next++ } );
      }
      if ( not active.empty() )
        drain();
    }
    while ( not active.empty() )
      drain();
  }
#endif
  double sum = 0;
  int passes = 0;
  for ( size_t si = 0; si < seeds.size(); ++si )
  {
    const json& r = results[ si ];
    if ( r.is_null() )
      continue;
    if ( r.contains( "error" ) )
    {
      std::fprintf( stderr, "spike-run: %s\n", r[ "error" ].get< std::string >().c_str() );
      return 1;
    }
    out[ "runs" ].push_back( r );
    sum += r[ "value" ].get< double >();
    passes += r[ "passed" ].get< bool >() ? 1 : 0;
  }
  out[ "n" ] = seeds.size();
  out[ "passes" ] = passes;
  out[ "mean_value" ] = sum / double( seeds.size() );
  std::printf( "%s\n", out.dump().c_str() );
  if ( require == "any" and passes == 0 )
    return 3;
  if ( require == "all" and passes != int( seeds.size() ) )
    return 3;
  return 0;
}

static int main_impl( int argc, char** argv )
{
  if ( argc == 2 and std::string( argv[ 1 ] ) == "--list-tasks" )  // one task id per line, in campaign order
  {
    for ( const auto& id : spike::task_ids() )
      std::printf( "%s\n", id.c_str() );
    return 0;
  }
  if ( argc == 3 and std::string( argv[ 1 ] ) == "--list-variants" )  // the ids a task accepts for --variant (a diagnosis case's candidates), one per line
  {
    auto task = spike::make_task( argv[ 2 ] );
    if ( not task )
    {
      std::fprintf( stderr, "unknown task %s\n", argv[ 2 ] );
      return 2;
    }
    for ( const auto& v : task->variants() )
      std::printf( "%s\n", v.first.c_str() );
    for ( const auto& c : task->diagnosis().candidates )  // a diagnosis case is answered through --variant too (never prints which one is right)
      std::printf( "%s\n", c.c_str() );
    return 0;
  }
  if ( argc == 3 and std::string( argv[ 1 ] ) == "--locked-digest" )  // 0.6 (Atlas): the LockedDigest of a circuit file, as JSON (what a locked level's Task::given() carries)
  {
    std::ifstream f( argv[ 2 ] );
    if ( not f )
    {
      std::fprintf( stderr, "spike-run: cannot open %s\n", argv[ 2 ] );
      return 2;
    }
    std::stringstream buf;
    buf << f.rdbuf();
    spike::LockedDigest d;
    std::string err;
    if ( not spike::circuit_locked_digest( buf.str(), d, err ) )
    {
      std::fprintf( stderr, "spike-run: %s: %s\n", argv[ 2 ], err.c_str() );
      return 1;
    }
    std::printf( "%s\n", json{ { "pops", d.pops }, { "wiring", d.wiring }, { "types", d.types }, { "channels", d.channels } }.dump().c_str() );
    return 0;
  }
  for ( int i = 1; i < argc; ++i )
    if ( std::string( argv[ i ] ) == "--task" )
      return run_task_mode( argc, argv );
  if ( argc < 3 )
  {
    std::fprintf( stderr, "usage: spike-run <circuit.json> <ms> [--threads n]\n       spike-run --task <id> [--circuit f.json] [--seed n | --seeds 1,2,3] [--lesion pre>post*k] [--require any|all]\n" );
    return 2;
  }
  int threads = 1;
  // 0.6 (dev aid, add-only): --sample key@neuron|out:name|mod:name[,..] --every ms prints {"t":..,"<what>":value,..} lines; --set t_ms:channel=value sets an input at that model time
  std::vector< std::string > sample;
  std::string phase_arg;   // --phase neuron[:i_ext]: print the fixed points and the onset type of a 2-D cell, then run
  double every = 10.0;
  struct SetAt { double t; std::string ch; double v; bool done = false; };
  std::vector< SetAt > sets;
  for ( int i = 3; i < argc; ++i )
    if ( std::string( argv[ i ] ) == "--threads" and i + 1 < argc )
    {
      unsigned long long v = 0;
      if ( not parse_ull( argv[ ++i ], v ) or v < 1 or v > 1024 )
      {
        std::fprintf( stderr, "spike-run: --threads needs a whole number of at least 1\n" );
        return 2;
      }
      threads = static_cast< int >( v );
    }
    else if ( std::string( argv[ i ] ) == "--sample" and i + 1 < argc )
    {
      std::stringstream q( argv[ ++i ] );
      for ( std::string tok; std::getline( q, tok, ',' ); ) sample.push_back( tok );
    }
    else if ( std::string( argv[ i ] ) == "--phase" and i + 1 < argc )
      phase_arg = argv[ ++i ];
    else if ( std::string( argv[ i ] ) == "--every" and i + 1 < argc )
    {
      if ( not parse_double( argv[ ++i ], every ) or not( every > 0 ) )
      {
        std::fprintf( stderr, "spike-run: --every needs a number of ms > 0\n" );
        return 2;
      }
    }
    else if ( std::string( argv[ i ] ) == "--set" and i + 1 < argc )
    {
      const std::string a = argv[ ++i ];
      const size_t c1 = a.find( ':' ), c2 = a.find( '=' );
      if ( c1 == std::string::npos or c2 == std::string::npos ) { std::fprintf( stderr, "spike-run: --set t_ms:channel=value\n" ); return 2; }
      double t = 0, v = 0;
      if ( not parse_double( a.substr( 0, c1 ), t ) or not parse_double( a.substr( c2 + 1 ), v ) ) { std::fprintf( stderr, "spike-run: --set t_ms:channel=value needs numbers for t_ms and value\n" ); return 2; }
      sets.push_back( { t, a.substr( c1 + 1, c2 - c1 - 1 ), v } );
    }
    else
    {
      std::fprintf( stderr, "spike-run: unknown argument '%s'\n", argv[ i ] );
      return 2;
    }
  std::ifstream f( argv[ 1 ] );
  if ( not f )
  {
    std::fprintf( stderr, "spike-run: cannot open %s\n", argv[ 1 ] );
    return 2;
  }
  std::stringstream ss;
  ss << f.rdbuf();
  std::string text = ss.str();
  {  // accept a golden-trace file (engine/tests/golden/*.json) too: run its "circuit"
    auto j = too_deep( text ) ? nlohmann::json() : nlohmann::json::parse( text, nullptr, false );
    if ( j.is_object() and j.contains( "circuit" ) and not j.contains( "schema_version" ) )
      text = j[ "circuit" ].dump();
  }
  double ms = 0;
  if ( not parse_double( argv[ 2 ], ms ) or not( ms > 0 ) or ms > 1e8 )
  {
    std::fprintf( stderr, "spike-run: <ms> must be a number from 0 to 1e8 (got '%s')\n", argv[ 2 ] );
    return 2;
  }

  if ( too_deep( text ) )
  {
    std::fprintf( stderr, "spike-run: %s is nested too deeply to be a circuit\n", argv[ 1 ] );
    return 1;
  }
  spike::set_threads( threads );
  spike::Engine e;
  if ( not e.load( text ) )
  {
    std::fprintf( stderr, "spike-run: %s\n", e.error().c_str() );
    return 1;
  }
  if ( not phase_arg.empty() )
  {
    const size_t c = phase_arg.find( ':' );
    const int n = std::atoi( phase_arg.substr( 0, c ).c_str() );
    const double ie = c == std::string::npos ? 0.0 : std::atof( phase_arg.substr( c + 1 ).c_str() );
    const auto pp = spike::theory::phase_plane_of( e, n, ie );
    std::fprintf( stderr, "phase plane of neuron %d (%s, I = %g): %s\n", n, pp.model.c_str(), pp.i_total, pp.ok ? "ok" : pp.note.c_str() );
    for ( const auto& f : pp.fixed ) std::fprintf( stderr, "  fixed point V %.3f %s %.3f  %s (trace %.4g det %.4g)\n", f.v, pp.w_name.c_str(), f.w, f.kind.c_str(), f.trace, f.det );
    if ( pp.ok )
    {
      std::map< std::string, double > p;
      for ( const auto& k : spike::theory::phase_plane_params( pp.model ) ) p[ k ] = e.neuron_state( n, k );
      const auto o = spike::theory::onset( pp.model, p, 0.0, pp.model == "izhikevich" ? 100.0 : 3000.0, 800 );
      std::fprintf( stderr, "  onset: %s at %g (%s, rest resonance %.1f Hz)\n", o.found ? "found" : "none", o.current, o.kind.c_str(), o.freq_hz );
    }
  }
  // 1 ms slices like the closed loop (identical spikes to one long run)
  const double res = e.resolution_ms();
  double done = 0;
  double next_sample = 0;
  while ( done + 1e-9 < ms )
  {
    for ( auto& st : sets )
      if ( not st.done and st.t <= done + 1e-9 )
      {
        st.done = true;
        e.set_input( st.ch, st.v );
        if ( not e.error().empty() )
        {
          std::fprintf( stderr, "spike-run: %s\n", e.error().c_str() );
          return 1;
        }
      }
    double dt = ms - done < 1.0 ? ms - done : 1.0;
    dt = std::max( res, std::round( dt / res ) * res );
    e.step( dt );
    done += dt;
    if ( not e.last_step_ok() )
    {
      std::fprintf( stderr, "spike-run: %s\n", e.error().c_str() );
      return 1;
    }
    for ( const auto& s : e.drain_spikes() )
      std::printf( "{\"neuron\":%d,\"t\":%.6f,\"step\":%lld}\n", s.neuron, s.t_ms, static_cast< long long >( std::llround( s.t_ms / res ) ) );
    if ( not sample.empty() and done + 1e-9 >= next_sample )
    {
      next_sample += every;
      std::printf( "{\"t\":%.3f", done );
      for ( const auto& w : sample )
      {
        double v = 0;
        if ( w.rfind( "out:", 0 ) == 0 ) v = e.output_value( w.substr( 4 ) );
        else if ( w.rfind( "cnt:", 0 ) == 0 ) v = e.output_count( w.substr( 4 ) );
        else if ( w.rfind( "mod:", 0 ) == 0 ) v = e.modulator( w.substr( 4 ) );
        else { const size_t at = w.find( '@' ); v = e.neuron_state( std::atoi( w.substr( at + 1 ).c_str() ), w.substr( 0, at ) ); }
        std::printf( ",\"%s\":%.6g", w.c_str(), v );
      }
      std::printf( "}\n" );
    }
  }
  auto st = e.stats();
  std::fprintf( stderr, "spike-run: t=%.3f ms spikes=%llu checksum=%016llx dropped=%llu\n", st.t_ms, static_cast< unsigned long long >( st.spikes ),
    static_cast< unsigned long long >( st.checksum ), static_cast< unsigned long long >( e.dropped() ) );
  return 0;
}

int main( int argc, char** argv )
{
  try
  {
    return main_impl( argc, argv );
  }
  catch ( const std::exception& e )  // bad values must end as a message and exit status 1, never as a core dump
  {
    std::fprintf( stderr, "spike-run: %s\n", e.what() );
    return 1;
  }
}
