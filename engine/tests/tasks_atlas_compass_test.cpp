// The compass chapter of the Atlas wing (chapter a_compass; research/atlas-a.md 4d): a_signs, a_places, a_gain, dx_a_compass.
// MEASUREMENT MODE (a runner; the numbers of research/atlas-a.md 4d): ATLAS_MEASURE=signs|places|gain|dx, ATLAS_SEEDS=N (default 40), ATLAS_FIRST=first seed (default 1),
//   ATLAS_SCALES="0.2,0.4,..." (gain); prints one line per run starting with "M ".
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
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
std::string circuits_dir() { return std::string( SPIKE_SOURCE_DIR ) + "/godot/circuits/"; }
json load_file( const std::string& name ) { return json::parse( chk::slurp( circuits_dir() + name ) ); }
bool has_digit( const std::string& s ) { return std::any_of( s.begin(), s.end(), []( unsigned char c ) { return std::isdigit( c ); } ); }
std::set< std::string > g_texts_checked;
void no_digit( const std::string& what, const std::string& text )
{
  CHECK_MSG( not has_digit( text ), what + ": a digit in '" + text + "'" );
  g_texts_checked.insert( text );
}
int env_int( const char* k, int d ) { const char* v = std::getenv( k ); return v && *v ? std::atoi( v ) : d; }

struct Res
{
  bool loaded = false, passed = false;
  double value = 0;
  std::string verdict, error;
  std::map< std::string, double > parts;
  std::vector< Task::Hint > hints;
  std::map< std::string, std::map< std::string, double > > hz;   // window name -> population -> mean Hz per cell (only when asked)
};

/// One run of a task on a circuit, as the game's TEST and spike-run do (the task seed reseeds the circuit: 1000 * seed + circuit seed).
Res run( const std::string& id, const json& c_in, uint64_t seed = 1, bool quick = false, const std::string& variant = "", bool spikes = false, bool reseed = true, long steps_override = 0 )
{
  Res r;
  Engine e;
  json c = c_in;
  if ( reseed ) c[ "seed" ] = 1000 * seed + c_in.value( "seed", 1 );
  r.loaded = e.load( c.dump() );
  if ( not r.loaded ) { r.error = e.error(); return r; }
  auto t = make_task( id );
  t->set_mode( "standard" );
  t->set_quick( quick );
  if ( not variant.empty() ) t->set_variant( variant );
  t->reset( seed );
  const long steps = steps_override > 0 ? steps_override : static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() );
  const Structure st = e.structure();
  std::map< int32_t, std::string > pop_of;
  std::map< std::string, int > size_of;
  for ( const auto& p : st.populations )
  {
    size_of[ p.id ] = p.n;
    for ( int32_t k = 0; k < p.n; ++k ) pop_of[ p.first_index + k ] = p.id;
  }
  // windows after the first cue (cue 200 to 700 ms): the cue, the middle, the end of the first trial
  const std::vector< std::pair< std::string, std::pair< double, double > > > win = { { "cue", { 400, 700 } }, { "mid", { 1500, 2000 } }, { "late", { 3200, 3700 } } };
  std::map< std::string, std::map< std::string, double > > n;
  for ( long s = 0; s < steps; ++s )
  {
    t->step( e, 1.0 );
    if ( spikes )
      for ( const auto& sp : e.drain_spikes() )
        for ( const auto& w : win )
          if ( sp.t_ms > w.second.first && sp.t_ms <= w.second.second ) n[ w.first ][ pop_of[ sp.neuron ] ] += 1;
  }
  for ( const auto& w : win )
    for ( const auto& [ pid, cnt ] : n[ w.first ] ) r.hz[ w.first ][ pid ] = cnt / size_of[ pid ] / ( ( w.second.second - w.second.first ) / 1000.0 );
  const Score sc = t->score();
  r.passed = sc.passed; r.value = sc.value; r.verdict = sc.verdict; r.parts = sc.parts; r.hints = t->hints();
  return r;
}

json set_group( json c, const std::string& id, const std::string& key, json v )
{
  for ( auto& g : c[ "groups" ] ) if ( g[ "id" ] == id ) g[ key ] = v;
  return c;
}
double part( const Res& r, const std::string& k, double d = -99 ) { return r.parts.count( k ) ? r.parts.at( k ) : d; }
std::string per_trial( const Res& r, const std::string& key, int prec )
{
  std::ostringstream o;
  for ( int i = 0; i < static_cast< int >( part( r, "n_trials", 0 ) ); ++i )
  {
    char b[ 32 ];
    std::snprintf( b, sizeof b, "%.*f", prec, part( r, "t" + std::to_string( i ) + "_" + key ) );
    o << ( i ? "," : "" ) << b;
  }
  return o.str();
}

// ---------------------------------------------------------------- measurement mode
void measure_signs( int first, int n )
{
  const json given = load_file( "a_signs_given.json" );
  for ( int a = 0; a < 8; ++a )
  {
    json c = given;
    c = set_group( c, "EPG", "sign", a & 1 ? -1 : 1 );
    c = set_group( c, "PEG", "sign", a & 2 ? -1 : 1 );
    c = set_group( c, "D7", "sign", a & 4 ? -1 : 1 );
    for ( int s = first; s < first + n; ++s )
    {
      const Res r = run( "a_signs", c, static_cast< uint64_t >( s ) );
      std::printf( "M signs a=%d EPG=%s PEG=%s D7=%s seed=%d loaded=%d passed=%d n_held=%g held=%s kind=%s cue=%s rest=%s hz=%s strength=%s near=%s err=%s\n", a, a & 1 ? "-" : "+", a & 2 ? "-" : "+", a & 4 ? "-" : "+", s,
                   r.loaded, r.passed, part( r, "n_held" ), per_trial( r, "held", 0 ).c_str(), per_trial( r, "kind", 0 ).c_str(), per_trial( r, "cue", 0 ).c_str(), per_trial( r, "rest", 0 ).c_str(),
                   per_trial( r, "hz", 1 ).c_str(), per_trial( r, "strength", 2 ).c_str(), per_trial( r, "near", 2 ).c_str(), r.error.c_str() );
    }
  }
}
void measure_places( int first, int n )
{
  const json c = load_file( "a_places_given.json" );
  for ( int s = first; s < first + n; ++s )
  {
    const Res r = run( "a_places", c, static_cast< uint64_t >( s ) );
    std::printf( "M places seed=%d loaded=%d places=%g n_held=%g match=%g held=%s kind=%s cue=%s rest=%s hz=%s strength=%s near=%s %s\n", s, r.loaded, part( r, "places" ), part( r, "n_held" ), part( r, "match" ), per_trial( r, "held", 0 ).c_str(),
                 per_trial( r, "kind", 0 ).c_str(), per_trial( r, "cue", 0 ).c_str(), per_trial( r, "rest", 0 ).c_str(), per_trial( r, "hz", 1 ).c_str(), per_trial( r, "strength", 2 ).c_str(), per_trial( r, "near", 2 ).c_str(), r.error.c_str() );
  }
}
void measure_gain( int first, int n )
{
  json c = load_file( "a_gain_given.json" );
  c = set_group( c, "D7", "ranges", json{ { "scale", { 0.01, 8.0 } } } );
  std::vector< double > scales;
  std::stringstream ss( std::getenv( "ATLAS_SCALES" ) ? std::getenv( "ATLAS_SCALES" ) : "0.2,0.4,0.6,0.8,1.0,1.2,1.5,2,3" );
  for ( std::string tok; std::getline( ss, tok, ',' ); ) scales.push_back( std::atof( tok.c_str() ) );
  for ( double sc : scales )
    for ( int s = first; s < first + n; ++s )
    {
      const Res r = run( "a_gain", set_group( c, "D7", "scale", sc ), static_cast< uint64_t >( s ) );
      std::printf( "M gain scale=%g seed=%d loaded=%d passed=%d n_held=%g held=%s kind=%s hz=%s strength=%s near=%s rest=%s %s\n", sc, s, r.loaded, r.passed, part( r, "n_held" ), per_trial( r, "held", 0 ).c_str(), per_trial( r, "kind", 0 ).c_str(),
                   per_trial( r, "hz", 1 ).c_str(), per_trial( r, "strength", 2 ).c_str(), per_trial( r, "near", 2 ).c_str(), per_trial( r, "rest", 0 ).c_str(), r.error.c_str() );
    }
}
void measure_dx( int first, int n )
{
  const json base = load_file( "a_places_given.json" );   // every cell and connection with its weight and sign, nothing silenced
  for ( const char* lesion : { "none", "EPG", "PENa", "PENb", "PEG", "D7" } )
    for ( int s = first; s < first + n; ++s )
    {
      json c = base;
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == lesion )
          for ( auto& e : p[ "edges" ] ) e[ 2 ] = 0.0;
      const Res r = run( "dx_a_compass", c, static_cast< uint64_t >( s ), false, "", true );
      std::printf( "M dx lesion=%s seed=%d loaded=%d held=%s kind=%s hz=%s strength=%s near=%s", lesion, s, r.loaded, per_trial( r, "held", 0 ).c_str(), per_trial( r, "kind", 0 ).c_str(), per_trial( r, "hz", 1 ).c_str(),
                   per_trial( r, "strength", 2 ).c_str(), per_trial( r, "near", 2 ).c_str() );
      for ( const char* w : { "cue", "mid", "late" } )
        for ( const char* p : { "EPG", "PENa", "PENb", "PEG", "D7" } ) std::printf( " %s_%s=%.1f", w, p, r.hz.count( w ) && r.hz.at( w ).count( p ) ? r.hz.at( w ).at( p ) : 0.0 );
      std::printf( " %s\n", r.error.c_str() );
    }
}
}  // namespace

int main()
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  if ( const char* m = std::getenv( "ATLAS_MEASURE" ) )
  {
    const std::string what = m;
    const int n = env_int( "ATLAS_SEEDS", 40 ), first = env_int( "ATLAS_FIRST", 1 );
    if ( what == "signs" ) measure_signs( first, n );
    else if ( what == "places" ) measure_places( first, n );
    else if ( what == "gain" ) measure_gain( first, n );
    else if ( what == "dx" ) measure_dx( first, n );
    else CHECK_MSG( false, "unknown ATLAS_MEASURE " + what );
    return chk::finish( "tasks_atlas_compass_test (measurement)" );
  }
  const json ref = load_file( "a_signs.json" );
  const Res r = run( "a_signs", ref, 1, true );
  CHECK_MSG( r.loaded and r.passed, "a_signs reference: " + r.verdict );
  no_digit( "a_signs verdict", r.verdict );
  return chk::finish( "tasks_atlas_compass_test" );
}
