// spike-run <circuit.json> <ms> [--threads n]: loads a circuit, runs it, prints one JSON line per spike:
//   {"neuron":3,"t":12.5,"step":125}   (neuron = circuit-level index, t in ms, step = NEST time stamp in steps)
// sorted by (step, neuron). A summary line goes to stderr so stdout holds spikes only.
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include "nlohmann/json.hpp"
#include "spike/engine.h"

int main( int argc, char** argv )
{
  if ( argc < 3 )
  {
    std::fprintf( stderr, "usage: spike-run <circuit.json> <ms> [--threads n]\n" );
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
