// spike-catalog [--out catalog/thumbs.json] [--jobs n] [--only id,id]: measures every card of the parts bin (f-I curve and V_m trace)
// and writes the thumbnail data for the game. Each model is probed in its own child process (`spike-catalog --probe <id>`), because the
// NEST kernel is a process-wide singleton: one Engine per process.
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

#include "nlohmann/json.hpp"
#include "spike/catalog.h"

using nlohmann::json;

static std::string self_path( const char* argv0 )
{
  char buf[ 4096 ];
  ssize_t n = readlink( "/proc/self/exe", buf, sizeof buf - 1 );
  if ( n > 0 )
    return std::string( buf, static_cast< size_t >( n ) );
  return argv0;
}

int main( int argc, char** argv )
{
  std::string out = "catalog/thumbs.json", only;
  int jobs = 4;
  for ( int i = 1; i < argc; ++i )
  {
    std::string a = argv[ i ];
    if ( a == "--probe" and i + 1 < argc )
    {
      std::printf( "%s\n", spike::catalog_probe_json( argv[ i + 1 ] ).c_str() );
      return 0;
    }
    else if ( a == "--out" and i + 1 < argc )
      out = argv[ ++i ];
    else if ( a == "--jobs" and i + 1 < argc )
      jobs = std::max( 1, std::atoi( argv[ ++i ] ) );
    else if ( a == "--only" and i + 1 < argc )
      only = argv[ ++i ];
    else
    {
      std::fprintf( stderr, "usage: spike-catalog [--out catalog/thumbs.json] [--jobs n] [--only id,id]\n" );
      return 2;
    }
  }
  std::vector< std::string > ids;
  for ( const auto& m : spike::catalog() )
    if ( only.empty() or ( "," + only + "," ).find( "," + m.id + "," ) != std::string::npos )
      ids.push_back( m.id );

  const std::string exe = self_path( argv[ 0 ] );
  json models = json::object();
  int bad = 0;
  for ( size_t base = 0; base < ids.size(); base += static_cast< size_t >( jobs ) )
  {
    std::vector< std::pair< std::string, FILE* > > run;
    for ( size_t k = base; k < ids.size() and k < base + static_cast< size_t >( jobs ); ++k )
      run.emplace_back( ids[ k ], popen( ( "'" + exe + "' --probe '" + ids[ k ] + "' 2>/dev/null" ).c_str(), "r" ) );
    for ( auto& [ id, f ] : run )
    {
      std::string text;
      char buf[ 4096 ];
      size_t n;
      while ( f and ( n = fread( buf, 1, sizeof buf, f ) ) > 0 )
        text.append( buf, n );
      int rc = f ? pclose( f ) : -1;
      json j = json::parse( text, nullptr, false );
      if ( rc != 0 or j.is_discarded() )
        j = { { "id", id }, { "available", false }, { "error", "probe process failed" } };
      j.erase( "id" );
      const bool ok = j.value( "available", false );
      std::fprintf( stderr, "%-28s %s%s\n", id.c_str(), ok ? "ok" : "unavailable: ", ok ? "" : j.value( "error", "" ).c_str() );
      const auto* cm = spike::catalog_find( id );
      if ( not ok and not( cm and cm->nestml ) )
        ++bad;
      models[ id ] = j;
    }
  }
  json doc = { { "schema", 1 }, { "generator", "spike-catalog" }, { "resolution_ms", 0.1 }, { "models", models } };
  std::ofstream f( out );
  if ( not f )
  {
    std::fprintf( stderr, "spike-catalog: cannot write %s\n", out.c_str() );
    return 2;
  }
  f << doc.dump( 1 ) << "\n";
  std::fprintf( stderr, "wrote %s (%zu models)\n", out.c_str(), ids.size() );
  return bad ? 1 : 0;
}
