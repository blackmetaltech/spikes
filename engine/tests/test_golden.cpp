// Golden spike-checksum traces: engine/tests/golden/*.json = { circuit, ms, checksum (hex), spikes }.
// GOLDEN_UPDATE=1 rewrites checksum and spike count from the current engine. Checksums are tied to the pinned
// NEST commit (scripts/build-nest.sh), the resolution and thread count 1.
#include <cstdlib>
#include <dirent.h>
#include <algorithm>
#include <vector>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

int main()
{
  const bool update = std::getenv( "GOLDEN_UPDATE" ) != nullptr;
  std::vector< std::string > files;
  if ( DIR* d = opendir( SPIKE_GOLDEN_DIR ) )
  {
    while ( auto* ent = readdir( d ) )
    {
      std::string n = ent->d_name;
      if ( n.size() > 5 and n.substr( n.size() - 5 ) == ".json" )
        files.push_back( n );
    }
    closedir( d );
  }
  std::sort( files.begin(), files.end() );
  CHECK( files.size() >= 3 );
  Engine e;
  for ( const auto& f : files )
  {
    std::string path = std::string( SPIKE_GOLDEN_DIR ) + "/" + f;
    json g = json::parse( chk::slurp( path ) );
    if ( not e.load( g[ "circuit" ].dump() ) )
    {
      std::printf( "  %s: load failed: %s\n", f.c_str(), e.error().c_str() );
      ++chk::failures();
      continue;
    }
    const double ms = g[ "ms" ].get< double >();
    for ( double t = 0; t < ms - 1e-9; t += 1.0 )
      e.step( 1.0 );
    RunStats s = e.stats();
    char hex[ 32 ];
    std::snprintf( hex, sizeof hex, "%016llx", (unsigned long long)s.checksum );
    std::printf( "  %-14s %6llu spikes  checksum %s\n", f.c_str(), (unsigned long long)s.spikes, hex );
    CHECK_MSG( s.spikes > 0, f );
    if ( update )
    {
      g[ "checksum" ] = hex;
      g[ "spikes" ] = s.spikes;
      std::ofstream( path ) << g.dump( 1 ) << "\n";
    }
    else
    {
      CHECK_MSG( g[ "spikes" ].get< uint64_t >() == s.spikes, f + ": spike count differs from golden" );
      CHECK_MSG( g[ "checksum" ].get< std::string >() == hex, f + ": checksum differs from golden (GOLDEN_UPDATE=1 to regenerate)" );
    }
  }
  return chk::finish( "test_golden" );
}
