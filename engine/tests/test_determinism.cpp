// Same seed -> same checksum; different seed -> different checksum; slicing the run does not change spikes.
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static RunStats run( Engine& e, json c, long seed, double slice, double total )
{
  c[ "seed" ] = seed;
  if ( not e.load( c.dump() ) )
  {
    std::printf( "  load failed: %s\n", e.error().c_str() );
    ++chk::failures();
    return {};
  }
  for ( double t = 0; t < total - 1e-9; t += slice )
    e.step( slice );
  return e.stats();
}

int main()
{
  json c = json::parse( chk::slurp( std::string( SPIKE_GOLDEN_DIR ) + "/balanced_1k.json" ) )[ "circuit" ];
  Engine e;
  RunStats a = run( e, c, 42, 1.0, 300 );
  RunStats b = run( e, c, 42, 1.0, 300 );
  RunStats d = run( e, c, 43, 1.0, 300 );
  RunStats s = run( e, c, 42, 25.0, 300 );  // different slicing, same spikes
  std::printf( "  seed 42: %llu spikes, checksum %016llx / %016llx; seed 43: %016llx\n", (unsigned long long)a.spikes, (unsigned long long)a.checksum,
    (unsigned long long)b.checksum, (unsigned long long)d.checksum );
  CHECK( a.spikes > 100 );
  CHECK( a.checksum == b.checksum and a.spikes == b.spikes );
  CHECK( a.checksum != d.checksum );
  CHECK( a.checksum == s.checksum );
  return chk::finish( "test_determinism" );
}
