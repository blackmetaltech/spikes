// spike-run <circuit.json> <ms> [--threads n]: loads a circuit, runs it, prints one JSON line per spike:
//   {"neuron":3,"t":12.5,"step":125}   (neuron = circuit-level index, t in ms, step = NEST time stamp in steps)
// sorted by (step, neuron). A summary line goes to stderr so stdout holds spikes only.
//
// spike-run --list-tasks prints the registered task ids, one per line.
// spike-run --task <id> [--circuit file.json] [--seed n | --seeds 1,2,3] [--lesion pre>post*k ...] [--require any|all] [--circuits-dir dir]
//   scores a circuit on a task headless, exactly as the game does (same 1 ms slices, a seed also reseeds the circuit: 1000 * seed + circuit seed),
//   and prints one JSON object on stdout: per-seed value / passed / verdict / parts / neurons / spikes / rate_hz_per_neuron, plus mean and pass count.
//   The circuit defaults to the task's reference circuit (looked up in $SPIKE_CIRCUITS_DIR, --circuits-dir, or a circuits/ folder above the cwd or
//   the executable). Seeds default to 1. A lesion scales the weight of every projection pre->post (0 deletes it). Exit status: 0 ok, 1 load or run
//   error, 2 usage, 3 the --require condition failed (any: at least one seed passed, all: every seed passed). Used by CI and scripts/smoke.sh.
#include <cstdio>
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

/// The folder that holds the reference circuits: explicit, $SPIKE_CIRCUITS_DIR, then circuits/ in the cwd and up to 6 parents, then the same from argv[0].
static std::string find_circuits_dir( const std::string& given, const std::string& rel, const char* argv0 )
{
  std::vector< std::string > cands;
  if ( not given.empty() )
    cands.push_back( given );
  if ( const char* e = std::getenv( "SPIKE_CIRCUITS_DIR" ) )
    cands.push_back( e );
  std::string up = "";
  for ( int i = 0; i < 7; ++i, up += "../" )
    cands.push_back( up + "circuits" );
  std::string exe = argv0;
  auto slash = exe.find_last_of( '/' );
  if ( slash != std::string::npos )
  {
    up = exe.substr( 0, slash + 1 );
    for ( int i = 0; i < 7; ++i, up += "../" )
      cands.push_back( up + "circuits" );
  }
  for ( const auto& c : cands )
    if ( exists( c + "/" + rel ) )
      return c;
  return "";
}

struct Lesion
{
  std::string pre, post;
  double factor;
};

static int run_task_mode( int argc, char** argv )
{
  std::string task_id, circuit_file, dir, require;
  std::vector< unsigned long long > seeds;
  std::vector< Lesion > lesions;
  int threads = 1;
  auto usage = [] {
    std::fprintf( stderr, "usage: spike-run --task <id> [--circuit file.json] [--seed n | --seeds 1,2,3] [--lesion pre>post*k]... "
                          "[--require any|all] [--circuits-dir dir] [--threads n]\n" );
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
    else if ( a == "--threads" and more )
      threads = std::atoi( argv[ ++i ] );
    else if ( ( a == "--seed" or a == "--seeds" ) and more )
    {
      std::stringstream ss( argv[ ++i ] );
      for ( std::string tok; std::getline( ss, tok, ',' ); )
        if ( not tok.empty() )
          seeds.push_back( std::strtoull( tok.c_str(), nullptr, 10 ) );
    }
    else if ( a == "--lesion" and more )
    {
      std::string l = argv[ ++i ];
      auto gt = l.find( '>' ), st = l.find( '*' );
      if ( gt == std::string::npos )
        return usage();
      Lesion x{ l.substr( 0, gt ), l.substr( gt + 1, st == std::string::npos ? std::string::npos : st - gt - 1 ), 0.0 };
      if ( st != std::string::npos )
        x.factor = std::atof( l.c_str() + st + 1 );
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
  if ( not spike::make_task( task_id ) )
  {
    std::fprintf( stderr, "spike-run: unknown task '%s'\n", task_id.c_str() );
    return 2;
  }
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
  json base = json::parse( buf.str(), nullptr, false );
  if ( base.is_discarded() or not base.is_object() )
  {
    std::fprintf( stderr, "spike-run: %s is not a JSON circuit\n", circuit_file.c_str() );
    return 2;
  }
  for ( const auto& l : lesions )
  {
    bool hit = false;
    for ( auto& p : base[ "projections" ] )
    {
      if ( p[ "pre" ] != l.pre or p[ "post" ] != l.post )
        continue;
      hit = true;
      if ( p.contains( "synapse" ) and p[ "synapse" ].contains( "weight" ) )
        p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * l.factor;
      if ( p.contains( "edges" ) )
        for ( auto& e : p[ "edges" ] )
          e[ 2 ] = e[ 2 ].get< double >() * l.factor;
    }
    if ( not hit )
      std::fprintf( stderr, "spike-run: warning: lesion %s>%s matched no projection\n", l.pre.c_str(), l.post.c_str() );
  }

  spike::set_threads( threads );
  json out;
  out[ "task" ] = task_id;
  out[ "circuit" ] = circuit_file;
  out[ "runs" ] = json::array();
  double sum = 0;
  int passes = 0;
  for ( auto seed : seeds )
  {
    json c = base;
    c[ "seed" ] = 1000 * seed + base.value( "seed", 1 );
    spike::Engine e;
    if ( not e.load( c.dump() ) )
    {
      std::fprintf( stderr, "spike-run: seed %llu: %s\n", seed, e.error().c_str() );
      return 1;
    }
    auto task = spike::make_task( task_id );
    task->reset( seed );
    unsigned long long spikes = 0;
    const long steps = static_cast< long >( task->duration_ms() );
    for ( long s = 0; s < steps; ++s )
    {
      task->step( e, 1.0 );
      if ( s % 500 == 499 )
        spikes += e.drain_spikes().size();
    }
    spikes += e.drain_spikes().size();
    if ( not e.error().empty() )
    {
      std::fprintf( stderr, "spike-run: seed %llu: %s\n", seed, e.error().c_str() );
      return 1;
    }
    const auto sc = task->score();
    json r;
    r[ "seed" ] = seed;
    r[ "value" ] = sc.value;
    r[ "passed" ] = sc.passed;
    r[ "verdict" ] = sc.verdict;
    r[ "parts" ] = json::object();
    for ( const auto& [ k, v ] : sc.parts )
      r[ "parts" ][ k ] = v;
    r[ "neurons" ] = e.neuron_count();
    r[ "spikes" ] = spikes;
    r[ "duration_ms" ] = task->duration_ms();
    r[ "rate_hz_per_neuron" ] = e.neuron_count() > 0 ? spikes / double( e.neuron_count() ) / ( task->duration_ms() * 0.001 ) : 0.0;
    out[ "runs" ].push_back( r );
    sum += sc.value;
    passes += sc.passed ? 1 : 0;
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

int main( int argc, char** argv )
{
  if ( argc == 2 and std::string( argv[ 1 ] ) == "--list-tasks" )  // one task id per line, in campaign order
  {
    for ( const auto& id : spike::task_ids() )
      std::printf( "%s\n", id.c_str() );
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
  for ( int i = 3; i < argc; ++i )
    if ( std::string( argv[ i ] ) == "--threads" and i + 1 < argc )
      threads = std::atoi( argv[ ++i ] );
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
    auto j = nlohmann::json::parse( text, nullptr, false );
    if ( j.is_object() and j.contains( "circuit" ) and not j.contains( "schema_version" ) )
      text = j[ "circuit" ].dump();
  }
  double ms = std::atof( argv[ 2 ] );

  spike::set_threads( threads );
  spike::Engine e;
  if ( not e.load( text ) )
  {
    std::fprintf( stderr, "spike-run: %s\n", e.error().c_str() );
    return 1;
  }
  // 1 ms slices like the closed loop (identical spikes to one long run)
  const double res = e.resolution_ms();
  double done = 0;
  while ( done + 1e-9 < ms )
  {
    double dt = ms - done < 1.0 ? ms - done : 1.0;
    dt = std::max( res, std::round( dt / res ) * res );
    e.step( dt );
    done += dt;
    if ( not e.error().empty() )
    {
      std::fprintf( stderr, "spike-run: %s\n", e.error().c_str() );
      return 1;
    }
    for ( const auto& s : e.drain_spikes() )
      std::printf( "{\"neuron\":%d,\"t\":%.6f,\"step\":%lld}\n", s.neuron, s.t_ms, static_cast< long long >( std::llround( s.t_ms / res ) ) );
  }
  auto st = e.stats();
  std::fprintf( stderr, "spike-run: t=%.3f ms spikes=%llu checksum=%016llx dropped=%llu\n", st.t_ms, static_cast< unsigned long long >( st.spikes ),
    static_cast< unsigned long long >( st.checksum ), static_cast< unsigned long long >( e.dropped() ) );
  return 0;
}
