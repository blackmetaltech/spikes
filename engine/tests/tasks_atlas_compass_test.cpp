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
std::string per_trial( const Res& r, const std::string& key, int prec, const std::string& prefix = "" )
{
  std::ostringstream o;
  for ( int i = 0; i < static_cast< int >( part( r, prefix + "n_trials", 0 ) ); ++i )
  {
    char b[ 32 ];
    std::snprintf( b, sizeof b, "%.*f", prec, part( r, prefix + "t" + std::to_string( i ) + "_" + key ) );
    o << ( i ? "," : "" ) << b;
  }
  return o.str();
}

// ---------------------------------------------------------------- measurement mode
void measure_signs( int first, int n )
{
  const json given = load_file( "a_signs_given.json" );
  std::set< int > only;   // ATLAS_ONLY="2,4": just these assignments
  if ( const char* o = std::getenv( "ATLAS_ONLY" ) ) { std::stringstream os( o ); for ( std::string tok; std::getline( os, tok, ',' ); ) only.insert( std::atoi( tok.c_str() ) ); }
  for ( int a = 0; a < 8; ++a )
  {
    if ( !only.empty() && !only.count( a ) ) continue;
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
  const std::string grp = std::getenv( "ATLAS_GROUP" ) ? std::getenv( "ATLAS_GROUP" ) : "D7";   // the group whose scale is swept (made free here; the others keep scale 1)
  if ( grp != "D7" ) c = set_group( c, "D7", "scale", 1.0 );   // the other groups are swept with the Delta7 gain at the reference value
  c = set_group( c, grp, "free", json::array( { "scale" } ) );
  c = set_group( c, grp, "ranges", json{ { "scale", { 0.01, 8.0 } } } );
  std::vector< double > scales;
  std::stringstream ss( std::getenv( "ATLAS_SCALES" ) ? std::getenv( "ATLAS_SCALES" ) : "0.2,0.4,0.6,0.8,1.0,1.2,1.5,2,3" );
  for ( std::string tok; std::getline( ss, tok, ',' ); ) scales.push_back( std::atof( tok.c_str() ) );
  for ( double sc : scales )
    for ( int s = first; s < first + n; ++s )
    {
      const Res r = run( "a_gain", set_group( c, grp, "scale", sc ), static_cast< uint64_t >( s ) );
      std::printf( "M gain group=%s scale=%g seed=%d loaded=%d passed=%d n_held=%g held=%s kind=%s hz=%s strength=%s near=%s rest=%s %s\n", grp.c_str(), sc, s, r.loaded, r.passed, part( r, "n_held" ), per_trial( r, "held", 0 ).c_str(), per_trial( r, "kind", 0 ).c_str(),
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
      std::printf( "M dx lesion=%s seed=%d loaded=%d held=%s kind=%s hz=%s strength=%s near=%s", lesion, s, r.loaded, per_trial( r, "held", 0, "run_" ).c_str(), per_trial( r, "kind", 0, "run_" ).c_str(), per_trial( r, "hz", 1, "run_" ).c_str(),
                   per_trial( r, "strength", 2, "run_" ).c_str(), per_trial( r, "near", 2, "run_" ).c_str() );
      for ( const char* w : { "cue", "mid", "late" } )
        for ( const char* p : { "EPG", "PENa", "PENb", "PEG", "D7" } ) std::printf( " %s_%s=%.1f", w, p, r.hz.count( w ) && r.hz.at( w ).count( p ) ? r.hz.at( w ).at( p ) : 0.0 );
      std::printf( " %s\n", r.error.c_str() );
    }
}

// ---------------------------------------------------------------- helpers of the checks
bool rejected( const Res& r ) { return r.parts.count( "circuit_rejected" ) and r.parts.at( "circuit_rejected" ) == 1.0; }
void expect_rejected( const std::string& task, const json& c, const std::string& label, const std::string& word )
{
  const Res r = run( task, c, 1, true, "", false, false, 5 );   // the guard rejects on the first slice
  CHECK_MSG( r.loaded, label + ": does not load: " + r.error );
  CHECK_MSG( rejected( r ) and not r.passed, label + ": not rejected (" + r.verdict + ")" );
  CHECK_MSG( r.value <= 49.0, label + ": value not capped" );
  CHECK_MSG( r.verdict.find( word ) != std::string::npos, label + ": verdict '" + r.verdict + "' lacks '" + word + "'" );
  no_digit( label, r.verdict );
}
std::vector< uint64_t > seed_list()
{
  std::vector< uint64_t > v = { 1, 2, 3, 4, 5, 101, 102, 103 };
  if ( const int n = env_int( "ATLAS_SEEDS", 0 ); n > 0 )
  {
    v.clear();
    for ( int k = 1; k <= n; ++k ) v.push_back( static_cast< uint64_t >( k ) );
  }
  return v;
}
json with_signs( json c, int epg, int peg, int d7 )
{
  c = set_group( c, "EPG", "sign", epg );
  c = set_group( c, "PEG", "sign", peg );
  return set_group( c, "D7", "sign", d7 );
}
bool spike_run( const std::string& args, json& out )
{
  const std::string cmd = std::string( SPIKE_RUN_EXE ) + " " + args + " 2>/dev/null";
  FILE* p = popen( cmd.c_str(), "r" );
  if ( not p ) return false;
  std::string s;
  char buf[ 4096 ];
  size_t n;
  while ( ( n = fread( buf, 1, sizeof buf, p ) ) > 0 ) s.append( buf, n );
  const int st = pclose( p );
  out = json::parse( s, nullptr, false );
  return st == 0 and out.is_object();
}

// ---------------------------------------------------------------- 1. the circuits and their digests
void test_digests()
{
  std::printf( "the circuits and their digests\n" );
  for ( const char* id : { "a_signs", "a_gain", "a_places" } )
  {
    const std::string given = std::string( id ) + "_given.json", ref = std::string( id ) + ".json";
    LockedDigest dg, dr;
    std::string err;
    CHECK_MSG( circuit_locked_digest( load_file( given ).dump(), dg, err ) and circuit_locked_digest( load_file( ref ).dump(), dr, err ), std::string( id ) + ": " + err );
    CHECK_MSG( dg == dr, std::string( id ) + ": the given and the reference circuits differ in something locked or not free" );
    auto t = make_task( id );
    CHECK_MSG( t->given().digest == dg, std::string( id ) + ": Task::given().digest is not the digest of its given circuit: run spike-run --locked-digest on it and update the task" );
    CHECK( t->given().circuit == given );
    json out;
    CHECK( spike_run( "--locked-digest " + circuits_dir() + given, out ) and out.value( "wiring", "" ) == dg.wiring and out.value( "types", "" ) == dg.types and out.value( "pops", "" ) == dg.pops and out.value( "channels", "" ) == dg.channels );
    const json c = load_file( given );
    int cells = 0;
    for ( const auto& p : c[ "populations" ] ) { cells += p[ "n" ].get< int >(); CHECK( p[ "locked" ] == true ); }
    for ( const auto& p : c[ "projections" ] ) CHECK( p[ "locked" ] == true );
    CHECK_MSG( cells == 148, std::string( id ) + ": 148 cells" );
    CHECK( c[ "meta" ].contains( "source" ) and c[ "inputs" ].size() == 17 and c[ "outputs" ].size() == 16 );
  }
  const json signs = load_file( "a_signs_given.json" ), gain = load_file( "a_gain_given.json" );
  CHECK( signs[ "groups" ].size() == 4 and gain[ "groups" ].size() == 4 and not load_file( "a_places_given.json" ).contains( "groups" ) );
  for ( const auto& g : signs[ "groups" ] )
  {
    const bool sign_free = g[ "id" ] != "PEN";
    CHECK_MSG( g[ "free" ] == ( sign_free ? json::array( { "sign" } ) : json::array() ) and g[ "scale" ] == 1.0, "a_signs: the cards: " + g[ "id" ].get< std::string >() );
  }
  for ( const auto& g : gain[ "groups" ] ) CHECK_MSG( g[ "free" ] == ( g[ "id" ] == "D7" ? json::array( { "scale" } ) : json::array() ) and g[ "sign" ] == ( g[ "id" ] == "D7" ? -1 : 1 ), "a_gain: the cards" );
  // the given signs are the wrong ones, the reference the right ones
  for ( const auto& g : signs[ "groups" ] ) CHECK( g[ "sign" ] == 1 );
  for ( const auto& g : load_file( "a_signs.json" )[ "groups" ] ) CHECK( g[ "sign" ] == ( g[ "id" ] == "D7" ? -1 : 1 ) );
}

// ---------------------------------------------------------------- 2. tampering
void test_tampering()
{
  std::printf( "tampering with the locked compass (each must be rejected, verdict without numbers)\n" );
  const std::string wiring = "wiring", cells = "cells", type = "cell type", chan = "inputs and outputs";
  const json ref = load_file( "a_signs.json" );
  const std::string t = "a_signs";
  { json c = ref; c[ "projections" ][ 0 ][ "edges" ].push_back( { 0, 0, 1.0, 2.0 } ); expect_rejected( t, c, "an edge added to a locked projection", wiring ); }
  { json c = ref; c[ "projections" ][ 2 ][ "edges" ].erase( 0 ); expect_rejected( t, c, "an edge removed", wiring ); }
  { json c = ref; c[ "projections" ][ 3 ][ "edges" ][ 0 ][ 2 ] = c[ "projections" ][ 3 ][ "edges" ][ 0 ][ 2 ].get< double >() * 1.000001; expect_rejected( t, c, "an edge re-weighted by one part in a million", wiring ); }
  { json c = ref; c[ "projections" ][ 3 ][ "edges" ][ 0 ][ 3 ] = 3.0; expect_rejected( t, c, "a delay changed", wiring ); }
  { json c = ref; c[ "projections" ][ 3 ].erase( "locked" ); expect_rejected( t, c, "the locked flag removed from a projection", wiring ); }
  { json c = ref; c[ "projections" ].erase( 5 ); expect_rejected( t, c, "a projection removed", wiring ); }
  { json c = ref; c[ "populations" ][ 4 ][ "params" ][ "I_e" ] = 100.0; expect_rejected( t, c, "a locked population given another background current", cells ); }
  { json c = ref; c[ "populations" ][ 4 ][ "n" ] = 10; c[ "populations" ][ 4 ].erase( "pos" ); expect_rejected( t, c, "a locked population resized", cells ); }
  { json c = ref; c[ "populations" ].push_back( { { "id", "X" }, { "model", "iaf_psc_alpha" }, { "n", 1 } } ); expect_rejected( t, c, "a population added", cells ); }
  { json c = set_group( ref, "PEN", "sign", -1 ); expect_rejected( t, c, "the P-EN sign flipped (not free)", type ); }
  { json c = set_group( ref, "PEN", "free", { "sign" } ); expect_rejected( t, c, "P-EN made free to sign", type ); }
  { json c = set_group( ref, "EPG", "free", { "sign", "scale" } ); c = set_group( c, "EPG", "ranges", { { "scale", { 0.5, 2.0 } } } ); expect_rejected( t, c, "E-PG made free to scale", type ); }
  { json c = ref; c[ "groups" ].erase( 0 ); expect_rejected( t, c, "a card removed", type ); }
  { json c = ref; c[ "inputs" ][ 0 ][ "weight" ] = 900.0; expect_rejected( t, c, "the cue weight changed", chan ); }
  { json c = ref; c[ "inputs" ][ 16 ][ "weight" ] = -500.0; expect_rejected( t, c, "the reset weight changed", chan ); }
  { json c = ref; c[ "outputs" ][ 3 ][ "neurons" ] = json::array( { { { "pop", "EPG" }, { "idx", { 0 } } } } ); expect_rejected( t, c, "a heading read from fewer cells", chan ); }
  // the three signs and the positions are free: no rejection
  for ( int a = 0; a < 8; ++a )
  {
    const Res r = run( t, with_signs( ref, a & 1 ? -1 : 1, a & 2 ? -1 : 1, a & 4 ? -1 : 1 ), 1, true, "", false, false, 5 );
    CHECK_MSG( r.loaded and not rejected( r ), "signs " + std::to_string( a ) + " are free values: " + r.verdict );
  }
  { json c = ref; c[ "name" ] = "mine"; c[ "seed" ] = 5; c[ "populations" ][ 0 ][ "pos" ][ 0 ] = { 10, 10 };
    const Res r = run( t, c, 1, true, "", false, false, 5 );
    CHECK_MSG( r.loaded and not rejected( r ), "positions, name and seed are free: " + r.verdict ); }
  // a_gain: the sign of every card is fixed, the scale of one is free inside its range only
  const json gref = load_file( "a_gain.json" );
  { json c = set_group( gref, "D7", "sign", 1 ); expect_rejected( "a_gain", c, "a_gain: the Delta7 sign flipped (not free)", type ); }
  { json c = set_group( gref, "D7", "scale", 99.0 ); expect_rejected( "a_gain", c, "a_gain: the scale above its range", "range" ); }
  { json c = set_group( gref, "D7", "scale", 0.0001 ); expect_rejected( "a_gain", c, "a_gain: the scale below its range", "range" ); }
  { json c = set_group( gref, "D7", "ranges", { { "scale", { 0.01, 99.0 } } } ); expect_rejected( "a_gain", c, "a_gain: the range widened", type ); }
  { json c = set_group( gref, "PEN", "free", { "scale" } ); c = set_group( c, "PEN", "ranges", { { "scale", { 0.5, 2.0 } } } ); expect_rejected( "a_gain", c, "a_gain: another cell type made free", type ); }
  { json c = gref; c[ "projections" ][ 3 ][ "edges" ][ 0 ][ 2 ] = c[ "projections" ][ 3 ][ "edges" ][ 0 ][ 2 ].get< double >() * 1.5; expect_rejected( "a_gain", c, "a_gain: an edge re-weighted", wiring ); }
  // a_places: nothing is free
  const json pref = load_file( "a_places.json" );
  { json c = pref; c[ "projections" ][ 3 ][ "edges" ][ 0 ][ 2 ] = c[ "projections" ][ 3 ][ "edges" ][ 0 ][ 2 ].get< double >() * 1.5; expect_rejected( "a_places", c, "a_places: an edge re-weighted", wiring ); }
  { json c = pref; c[ "groups" ] = json::array( { { { "id", "g" }, { "pops", { "EPG" } }, { "sign", 1 }, { "scale", 1.0 }, { "free", json::array() } } } ); expect_rejected( "a_places", c, "a_places: a card added", type ); }
}

// ---------------------------------------------------------------- 3. a_signs
void test_signs()
{
  const json given = load_file( "a_signs_given.json" );
  const auto seeds = seed_list();
  std::printf( "a_signs: the eight sign assignments on %zu seeds\n", seeds.size() );
  auto t = make_task( "a_signs" );
  CHECK( t->level_kind() == "atlas" and t->on_spine() and t->parent().empty() and t->chapter() == "a_compass" and not t->brief().empty() and not t->recipe().empty() and not t->notebook().citation.empty() );
  CHECK( t->sensor_channels().size() == 17 and t->motor_channels().size() == 16 and t->channel_kinds().count( "clear" ) and t->channel_kinds().count( "cue0" ) );
  for ( int a = 0; a < 8; ++a )
  {
    const int epg = a & 1 ? -1 : 1, peg = a & 2 ? -1 : 1, d7 = a & 4 ? -1 : 1;
    const bool right = epg == 1 and peg == 1 and d7 == -1;
    int passes = 0;
    for ( uint64_t seed : seeds )
    {
      const Res r = run( "a_signs", with_signs( given, epg, peg, d7 ), seed );
      CHECK_MSG( r.loaded and not rejected( r ), r.error + r.verdict );
      no_digit( "a_signs verdict", r.verdict );
      passes += r.passed ? 1 : 0;
      if ( right ) CHECK_MSG( r.passed and r.value == 100.0 and r.parts.at( "n_held" ) == 5.0, "the reference signs, seed " + std::to_string( seed ) + ": " + r.verdict );
      else
      {
        CHECK_MSG( not r.passed and r.value < 100.0, "assignment " + std::to_string( a ) + " seed " + std::to_string( seed ) + " must fail: " + r.verdict );
        CHECK_MSG( r.verdict.find( "wrong" ) != std::string::npos, "the verdict says a sign is wrong: " + r.verdict );
      }
    }
    std::printf( "  E-PG %c P-EG %c Delta7 %c: %d of %zu pass\n", epg > 0 ? '+' : '-', peg > 0 ? '+' : '-', d7 > 0 ? '+' : '-', passes, seeds.size() );
  }
  const Res r = run( "a_signs", given );
  CHECK( r.hints.size() == 3 and r.hints[ 1 ].target == "D7" );
  for ( const auto& h : r.hints ) no_digit( "a_signs hint " + std::to_string( h.tier ), h.text );
}

// ---------------------------------------------------------------- 4. a_places
void test_places()
{
  const json c = load_file( "a_places_given.json" );
  auto t = make_task( "a_places" );
  const auto d = t->diagnosis();
  std::printf( "a_places: the count of resting places, one commit per candidate\n" );
  CHECK( t->level_kind() == "diagnosis" and not t->on_spine() and t->parent() == "a_signs" and t->chapter() == "a_compass" );
  CHECK( d.bet and d.budget == 3 and d.candidates.size() == 4 and d.answer == "about6" and d.candidates.front() != d.answer and d.probes.size() >= 1 and d.probes.size() <= 2 );
  for ( const auto& k : d.candidates ) CHECK( d.labels.count( k ) and d.explain.count( k ) );
  for ( const auto& [ k, v ] : d.explain ) no_digit( "a_places explain", v );
  for ( const auto& [ k, v ] : d.labels ) no_digit( "a_places label", v );
  auto seeds = seed_list();
  if ( not std::getenv( "ATLAS_SEEDS" ) ) seeds.resize( 5 );
  for ( uint64_t seed : seeds )
  {
    const Res r = run( "a_places", c, seed );
    const double places = r.parts.at( "places" );
    std::printf( "  seed %3d: %g resting places, held %g of 16 cues\n", static_cast< int >( seed ), places, r.parts.at( "n_held" ) );
    CHECK_MSG( places >= 4 and places <= 9, "the count of resting places is 4 to 9 (about six)" );
    CHECK_MSG( r.parts.at( "match" ) == 1.0, "the count matches the answer" );
    CHECK_MSG( not r.passed and r.verdict.rfind( "No prediction", 0 ) == 0, "no commit: " + r.verdict );
    no_digit( "a_places verdict", r.verdict );
  }
  // one commit per candidate (the run does not depend on the commit)
  int passing = 0;
  for ( const auto& k : d.candidates )
  {
    const Res r = run( "a_places", c, 1, false, k );
    CHECK_MSG( r.loaded and not rejected( r ), k + ": " + r.verdict );
    no_digit( "a_places verdict", r.verdict );
    if ( r.passed )
    {
      ++passing;
      CHECK_MSG( k == d.answer and r.value == 100.0 and r.verdict.rfind( "Correct", 0 ) == 0, "a right commit scores in full: " + r.verdict );
    }
    else CHECK_MSG( r.value == 0.0 and r.verdict.rfind( "Not that count", 0 ) == 0, k + ": " + r.verdict );
  }
  CHECK_MSG( passing == 1, "exactly one commit passes" );
  { const Res r = run( "a_places", c, 1, false, "about6@5:1" );   // a right bet after a wrong one and two instrument reads too many costs more
    CHECK_MSG( r.passed and r.value == 100.0 - 15.0 - 3.0 * 2, "the case rule: " + std::to_string( r.value ) ); }
  { const Res r = run( "a_places", c, 1, false, "about6@revealed", false, true, 100 ); CHECK( not r.passed and r.verdict.find( "revealed" ) != std::string::npos ); no_digit( "revealed", r.verdict ); }
  const Res r = run( "a_places", c, 1, false, "", false, true, 100 );
  CHECK( r.hints.size() == 3 );
  for ( const auto& h : r.hints ) no_digit( "a_places hint " + std::to_string( h.tier ), h.text );
}

// ---------------------------------------------------------------- 5. a_gain
/// What the measurements of research/atlas-a.md 4d say about the Delta7 scale at the cues of a_gain: the bump holds from 0.30 up to the top of the range, and from 0.25 down it does not.
bool gain_holds( double v ) { return v >= 0.30 - 1e-9; }

void test_gain()
{
  const json given = load_file( "a_gain_given.json" ), ref = load_file( "a_gain.json" );
  auto t = make_task( "a_gain" );
  CHECK( t->level_kind() == "atlas" and t->on_spine() and t->parent().empty() and t->chapter() == "a_compass" and not t->brief().empty() and not t->recipe().empty() and not t->notebook().citation.empty() );
  auto seeds = seed_list();
  if ( not std::getenv( "ATLAS_SEEDS" ) ) seeds.resize( 3 );
  std::printf( "a_gain: every value of the stepper (steps of 0.05 from the bottom to the top of the range) on %zu seeds\n", seeds.size() );
  double lo = 0, hi = 0;
  for ( const auto& g : given[ "groups" ] ) if ( g[ "id" ] == "D7" ) { lo = g[ "ranges" ][ "scale" ][ 0 ].get< double >(); hi = g[ "ranges" ][ "scale" ][ 1 ].get< double >(); }
  CHECK_MSG( lo > 0 and hi > lo, "the range of the Delta7 scale" );
  for ( double v = lo; v <= hi + 1e-9; v += 0.05 )
  {
    int passes = 0;
    for ( uint64_t seed : seeds )
    {
      const Res r = run( "a_gain", set_group( ref, "D7", "scale", v ), seed );
      CHECK_MSG( r.loaded and not rejected( r ), r.error + r.verdict );
      no_digit( "a_gain verdict", r.verdict );
      passes += r.passed ? 1 : 0;
      CHECK_MSG( r.passed == gain_holds( v ), "scale " + std::to_string( v ) + " seed " + std::to_string( seed ) + ": " + r.verdict );
      if ( not r.passed ) CHECK_MSG( r.verdict.find( "too weak" ) != std::string::npos, "the verdict says the inhibition is too weak: " + r.verdict );
    }
    std::printf( "  scale %.2f: %d of %zu pass\n", v, passes, seeds.size() );
  }
  { const Res r = run( "a_gain", given, 1 );   // the given scale fails
    CHECK_MSG( not r.passed and r.value < 100.0, "the given scale must fail: " + r.verdict ); }
  { const Res r = run( "a_gain", ref, 1 );   // the reference passes in full
    CHECK_MSG( r.passed and r.value == 100.0 and r.parts.at( "n_held" ) == r.parts.at( "n_trials" ), "the reference passes: " + r.verdict ); }
  const Res r = run( "a_gain", given, 1, true, "", false, true, 100 );
  CHECK( r.hints.size() == 3 and r.hints[ 1 ].target == "D7" );
  for ( const auto& h : r.hints ) no_digit( "a_gain hint " + std::to_string( h.tier ), h.text );
}

// ---------------------------------------------------------------- 6. dx_a_compass
void test_dx()
{
  const json patient = load_file( "dx_a_compass.json" ), healthy = load_file( "a_places_given.json" );
  auto t = make_task( "dx_a_compass" );
  const auto d = t->diagnosis();
  std::printf( "dx_a_compass: the five lesions by what the scopes show\n" );
  CHECK( t->level_kind() == "diagnosis" and not t->on_spine() and t->parent() == "a_gain" and t->chapter() == "a_compass" and d.circuit == "dx_a_compass.json" );
  CHECK( d.candidates.size() == 5 and d.answer == "pop:D7" and d.candidates.front() != d.answer and not d.bet );
  CHECK( d.probes.size() >= 1 and d.probes.size() <= 2 );
  for ( const auto& c : d.candidates ) CHECK( d.labels.count( c ) and d.explain.count( c ) and t->brief().find( c ) != std::string::npos );
  for ( const auto& [ k, v ] : d.explain ) no_digit( "dx explain", v );
  for ( const auto& c : d.candidates )   // exactly one candidate passes
  {
    auto task = make_task( "dx_a_compass" );
    task->set_variant( c );
    const Score s = task->score();
    CHECK_MSG( s.passed == ( c == d.answer ), c );
    no_digit( "dx verdict", s.verdict );
  }
  // the patient: every cell and connection, the answer's outputs at weight 0, the others as in the healthy compass
  const auto weight_out = [ & ]( const json& c, const std::string& pre ) {
    double w = 0;
    for ( const auto& p : c[ "projections" ] ) if ( p[ "pre" ] == pre ) for ( const auto& e : p[ "edges" ] ) w += std::fabs( e[ 2 ].get< double >() );
    return w;
  };
  CHECK( patient[ "projections" ].size() == healthy[ "projections" ].size() and patient[ "populations" ].size() == healthy[ "populations" ].size() );
  for ( const char* k : { "EPG", "PENa", "PENb", "PEG", "D7" } )
    CHECK_MSG( ( weight_out( patient, k ) == 0.0 ) == ( "pop:" + std::string( k ) == d.answer ) and ( weight_out( patient, k ) == 0.0 or std::fabs( weight_out( patient, k ) - weight_out( healthy, k ) ) < 1e-6 ),
               std::string( "only the answer's outputs are silent: " ) + k );
  // the signature: mean Hz per cell in the windows after the first cue (cue 200-700 ms; "late" = 3200-3700 ms)
  auto seeds = seed_list();
  if ( not std::getenv( "ATLAS_SEEDS" ) ) seeds.resize( 3 );
  for ( const char* lesion : { "none", "EPG", "PENa", "PENb", "PEG", "D7" } )
    for ( uint64_t seed : seeds )
    {
      json c = healthy;
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == lesion )
          for ( auto& e : p[ "edges" ] ) e[ 2 ] = 0.0;
      const Res r = run( "dx_a_compass", c, seed, false, "", true );
      CHECK_MSG( r.loaded, r.error );
      auto hz = [ & ]( const char* pop ) { return r.hz.count( "late" ) && r.hz.at( "late" ).count( pop ) ? r.hz.at( "late" ).at( pop ) : 0.0; };
      const double held = r.parts.count( "run_n_held" ) ? r.parts.at( "run_n_held" ) : -1.0;
      std::printf( "    %-4s seed %3d: held %g of 2, late Hz EPG %.1f PENa %.1f PENb %.1f PEG %.1f D7 %.1f\n", lesion, static_cast< int >( seed ), held, hz( "EPG" ), hz( "PENa" ), hz( "PENb" ), hz( "PEG" ), hz( "D7" ) );
      const std::string l = lesion;
      if ( l == "D7" )
      {
        CHECK_MSG( held == 0.0 and hz( "EPG" ) > 150.0 and hz( "D7" ) > 150.0, "Delta7 silenced: the compass cells over-active, the Delta7 cells fire" );
        CHECK_MSG( r.parts.at( "run_passed" ) == 0.0, "the patient fails its world" );
      }
      else if ( l == "EPG" ) CHECK_MSG( held == 0.0 and hz( "PEG" ) < 0.5 and hz( "D7" ) < 0.5 and hz( "EPG" ) > 10.0 and hz( "EPG" ) < 100.0, "E-PG silenced: P-EG and Delta7 silent, the compass cells not over-active" );
      else CHECK_MSG( held == 2.0 and hz( "EPG" ) > 10.0 and hz( "EPG" ) < 40.0 and hz( "D7" ) > 40.0, l + " silenced: the ring still holds its bump at an ordinary rate" );
    }
  { const Res r = run( "dx_a_compass", patient, 1, false, "", true );   // the patient itself
    CHECK_MSG( r.loaded and r.parts.at( "run_n_held" ) == 0.0 and r.hz.at( "late" ).at( "EPG" ) > 150.0 and r.hz.at( "late" ).at( "D7" ) > 150.0, "the patient: compass cells over-active, Delta7 fires" ); }
  const Res r = run( "dx_a_compass", patient, 1, true, "", false, true, 100 );
  CHECK( r.hints.size() == 3 );
  for ( const auto& h : r.hints ) no_digit( "dx hint " + std::to_string( h.tier ), h.text );
  CHECK( not t->notebook().citation.empty() and t->concepts_new().size() == 1 );
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
  test_digests();
  test_tampering();
  test_signs();
  test_places();
  test_gain();
  test_dx();
  std::printf( "%zu distinct texts checked for digits\n", g_texts_checked.size() );
  return chk::finish( "tasks_atlas_compass_test" );
}
