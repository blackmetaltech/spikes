// The worm chapter of the Atlas wing (chapter a_worm; research/atlas-a.md 4b): a_path, a_motif, a_predict, dx_a_worm, and the format addition they rest on.
//   1. the format: a group with "off": true may have scale 0 and a range from 0 (the cell type can be switched off); without it both stay errors (tasks_atlas_test holds that).
//   2. the circuits: given and reference of a_path and a_predict are one locked circuit with different free values; the digests of Task::given() are the digests of the files
//      (and of `spike-run --locked-digest`); a_motif's circuit is the whole chemical wiring (302 cells) and its reference is the given circuit.
//   3. tampering with the given wiring, cells, types, channels or a free value out of range is rejected, in the engine, with a verdict that names the part and no number.
//   4. a_path: the reference passes seeds 101 to 105 (quick and full, three modes); each of the three shortest routes kept alone passes the level and the verdict says the worm does not back up.
//   5. a_predict: exactly one of the five commits passes on the circuit seeds 1 to 3 and 101 to 105, it is Diagnosis::answer, and the others fail with a verdict that says the response needs the class.
//   6. a_motif: the engine's three z-scores; exactly one null model gives a significant positive z for the feed-forward loop; it is Diagnosis::answer.
//   7. dx_a_worm: exactly one candidate passes; the four lesions are told apart by which classes fire after the touch (the recording signature the case rests on).
//   8. no verdict or hint (tiers 1 to 3) of the four carries a digit.
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
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
const std::vector< std::string > kClasses = { "AVA", "AVB", "AVD", "AVE", "PVC" };

void no_digit( const std::string& what, const std::string& text )
{
  CHECK_MSG( not has_digit( text ), what + ": a digit in '" + text + "'" );
  g_texts_checked.insert( text );
}

// ---------------------------------------------------------------- a run
struct Res
{
  bool loaded = false, passed = false;
  double value = 0;
  std::string verdict, error;
  std::map< std::string, double > parts;
  std::vector< Task::Hint > hints;
  std::map< std::string, double > class_hz;   // mean Hz per cell of every population over [500, 1200) ms (only when asked)
};

Res run( const std::string& id, const json& c_in, uint64_t seed = 101, bool quick = true, const std::string& mode = "standard", const std::string& variant = "", bool spikes = false,
         long steps_override = 0, bool reseed = true )
{
  Res r;
  Engine e;
  json c = c_in;
  if ( reseed ) c[ "seed" ] = 1000 * seed + c_in.value( "seed", 1 );   // as the game's TEST and spike-run reseed the circuit with the world
  r.loaded = e.load( c.dump() );
  if ( not r.loaded ) { r.error = e.error(); return r; }
  auto t = make_task( id );
  t->set_mode( mode );
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
  std::map< std::string, double > n_spikes;
  for ( long s = 0; s < steps; ++s )
  {
    t->step( e, 1.0 );
    if ( spikes )
      for ( const auto& sp : e.drain_spikes() )
        if ( sp.t_ms > 500 and sp.t_ms <= 1200 ) n_spikes[ pop_of[ sp.neuron ] ] += 1;
  }
  for ( const auto& [ id2, n ] : n_spikes ) r.class_hz[ id2 ] = n / size_of[ id2 ] / 0.7;
  const Score sc = t->score();
  r.passed = sc.passed; r.value = sc.value; r.verdict = sc.verdict; r.parts = sc.parts; r.hints = t->hints();
  return r;
}

bool rejected( const Res& r ) { return r.parts.count( "circuit_rejected" ) and r.parts.at( "circuit_rejected" ) == 1.0; }

json set_group( json c, const std::string& id, const std::string& key, json v )
{
  for ( auto& g : c[ "groups" ] ) if ( g[ "id" ] == id ) g[ key ] = v;
  return c;
}
json scales( json c, const std::map< std::string, double >& set, double rest )
{
  for ( auto& g : c[ "groups" ] ) g[ "scale" ] = set.count( g[ "id" ] ) ? set.at( g[ "id" ] ) : rest;
  return c;
}

void expect_rejected( const std::string& task, const json& c, const std::string& label, const std::string& word )
{
  const Res r = run( task, c, 101, true, "standard", "", false, 5, false );   // the guard rejects on the first slice
  CHECK_MSG( r.loaded, label + ": does not load: " + r.error );
  CHECK_MSG( rejected( r ) and not r.passed, label + ": not rejected (" + r.verdict + ")" );
  CHECK_MSG( r.value <= 49.0, label + ": value not capped" );
  CHECK_MSG( r.verdict.find( word ) != std::string::npos, label + ": verdict '" + r.verdict + "' lacks '" + word + "'" );
  no_digit( label, r.verdict );
}

// ---------------------------------------------------------------- 1. the format
json tiny( const json& groups )
{
  json c = { { "schema_version", 1 }, { "name", "tiny" }, { "resolution_ms", 0.1 }, { "seed", 3 },
    { "populations", { { { "id", "A" }, { "model", "iaf_psc_alpha" }, { "n", 2 } }, { { "id", "C" }, { "model", "iaf_psc_alpha" }, { "n", 1 } } } },
    { "projections", { { { "pre", "A" }, { "post", "C" }, { "edges", { { 0, 0, 40.0, 1.5 }, { 1, 0, 60.0, 1.5 } } } } } },
    { "inputs", json::array() }, { "outputs", json::array() } };
  c[ "groups" ] = groups;
  return c;
}
void test_format_off()
{
  auto loads = []( const json& c, std::string* err = nullptr ) {
    Engine e;
    const bool ok = e.load( c.dump() );
    if ( err ) *err = e.error();
    return ok;
  };
  json g = { { "id", "g" }, { "pops", { "A" } }, { "free", { "scale" } }, { "ranges", { { "scale", { 0.0, 1.0 } } } }, { "scale", 0.0 } };
  std::string err;
  CHECK_MSG( not loads( tiny( json::array( { g } ) ), &err ) and err.find( "groups[0]" ) != std::string::npos, "scale 0 and a range from 0 without off are errors: " + err );
  g[ "off" ] = true;
  {
    Engine e;
    CHECK_MSG( e.load( tiny( json::array( { g } ) ).dump() ), e.error() );
    bool zero = true;
    for ( const auto& q : e.connections() ) { zero = false; (void) q; }   // connections() lists non-zero weights only: a class at scale 0 carries nothing
    CHECK_MSG( zero, "a group at scale 0 still carries weight" );
    const Structure st0 = e.structure();
    const auto* gi = &st0.groups[ 0 ];
    CHECK( gi->scale == 0.0 and gi->scale_in_range and gi->scale_min == 0.0 and gi->scale_max == 1.0 );
  }
  g[ "scale" ] = 0.5;
  {
    Engine e2;
    CHECK( e2.load( tiny( json::array( { g } ) ).dump() ) );
    double w = 0;
    for ( const auto& q : e2.connections() ) w += q.weight;
    CHECK_MSG( std::fabs( w - 50.0 ) < 1e-9, "scale 0.5 should halve the stored weights" );
  }
  json bad = g; bad[ "off" ] = "yes";
  CHECK_MSG( not loads( tiny( json::array( { bad } ) ), &err ) and err.find( "off" ) != std::string::npos, "off must be a boolean: " + err );
  json neg = g; neg[ "scale" ] = -0.1;
  CHECK_MSG( not loads( tiny( json::array( { neg } ) ), &err ), "a negative scale stays an error even with off" );
  json range = g; range[ "ranges" ] = { { "scale", { -0.5, 1.0 } } };
  CHECK_MSG( not loads( tiny( json::array( { range } ) ), &err ), "a negative lower end stays an error even with off" );
}

// ---------------------------------------------------------------- spike-run
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

// ---------------------------------------------------------------- 2. the circuits and their digests
void test_digests()
{
  const json path_given = load_file( "a_path_given.json" ), motif = load_file( "a_motif_given.json" );
  LockedDigest d_path, d_motif;
  std::string err;
  CHECK( circuit_locked_digest( path_given.dump(), d_path, err ) and circuit_locked_digest( motif.dump(), d_motif, err ) );
  for ( const char* f : { "a_path.json", "a_predict.json", "a_predict_given.json" } )
  {
    LockedDigest d;
    CHECK( circuit_locked_digest( load_file( f ).dump(), d, err ) );
    CHECK_MSG( d == d_path, std::string( f ) + " differs from the given touch circuit in something locked or not free" );
  }
  { LockedDigest d; CHECK( circuit_locked_digest( load_file( "a_motif.json" ).dump(), d, err ) and d == d_motif ); }
  for ( const char* id : { "a_path", "a_predict" } )
  {
    auto t = make_task( id );
    CHECK_MSG( t->given().digest == d_path, std::string( id ) + ": Task::given().digest is not the digest of its given circuit: run spike-run --locked-digest on it and update the task" );
    CHECK( t->given().circuit == std::string( id ) + "_given.json" and t->level_kind() == "atlas" );
  }
  auto m = make_task( "a_motif" );
  CHECK_MSG( m->given().digest == d_motif, "a_motif: Task::given().digest is not the digest of a_motif_given.json" );
  CHECK( m->given().circuit == "a_motif_given.json" and m->diagnosis().circuit == "a_motif_given.json" );
  // spike-run prints the same digests
  json out;
  CHECK( spike_run( "--locked-digest " + circuits_dir() + "a_path_given.json", out ) and out.value( "wiring", "" ) == d_path.wiring and out.value( "types", "" ) == d_path.types and out.value( "pops", "" ) == d_path.pops
         and out.value( "channels", "" ) == d_path.channels );
  // the given circuits: 54 and 302 cells, the groups, names where the format allows, attribution
  const auto count = []( const json& c ) { int n = 0; for ( const auto& p : c[ "populations" ] ) n += p[ "n" ].get< int >(); return n; };
  CHECK( count( path_given ) == 54 and count( motif ) == 302 );
  CHECK( path_given[ "meta" ][ "cells" ].size() == 54 and motif[ "meta" ][ "cells" ].size() == 302 );
  CHECK( path_given[ "meta" ].contains( "source" ) and motif[ "meta" ].contains( "source" ) );
  CHECK( path_given[ "groups" ].size() == 5 and motif[ "groups" ].size() == 4 );
  for ( const auto& g : path_given[ "groups" ] ) CHECK( g[ "free" ] == json::array( { "scale" } ) and g[ "off" ] == true and g[ "ranges" ][ "scale" ] == json::array( { 0.0, 1.0 } ) and g[ "sign" ] == 1 );
  for ( const auto& g : motif[ "groups" ] ) CHECK( g[ "free" ].empty() );
  for ( const auto& p : path_given[ "populations" ] ) CHECK( p[ "locked" ] == true );
  for ( const auto& p : path_given[ "projections" ] ) CHECK( p[ "locked" ] == true );
  Engine e;
  CHECK( e.load( motif.dump() ) and e.locked_digest() == d_motif );
  CHECK_MSG( e.structure().populations.size() == 131, "population ids are the classes" );
}

// ---------------------------------------------------------------- 3. tampering
void test_tampering()
{
  const json ref = load_file( "a_path.json" );
  std::printf( "tampering with the locked touch circuit (each must be rejected, verdict without numbers)\n" );
  const std::string wiring = "wiring", cells = "cells", type = "cell type", chan = "inputs and outputs";
  const std::string t = "a_path";
  { json c = ref; c[ "projections" ][ 0 ][ "edges" ].push_back( { 0, 0, 40.0, 2.0 } ); expect_rejected( t, c, "an edge added to a locked projection", wiring ); }
  { json c = ref; c[ "projections" ][ 3 ][ "edges" ].erase( 0 ); expect_rejected( t, c, "an edge removed", wiring ); }
  { json c = ref; c[ "projections" ][ 5 ][ "edges" ][ 0 ][ 2 ] = c[ "projections" ][ 5 ][ "edges" ][ 0 ][ 2 ].get< double >() * 1.000001; expect_rejected( t, c, "an edge re-weighted by one part in a million", wiring ); }
  { json c = ref; c[ "projections" ][ 5 ][ "edges" ][ 0 ][ 3 ] = 3.0; expect_rejected( t, c, "a delay changed", wiring ); }
  { json c = ref; c[ "projections" ][ 5 ][ "synapse" ][ "synapse_model" ] = "tsodyks2_synapse"; expect_rejected( t, c, "the synapse model changed", wiring ); }
  { json c = ref; c[ "projections" ][ 5 ].erase( "locked" ); expect_rejected( t, c, "the locked flag removed from a projection", wiring ); }
  { json c = ref; c[ "projections" ].erase( 7 ); expect_rejected( t, c, "a projection removed", wiring ); }
  { json c = ref; c[ "populations" ][ 8 ][ "n" ] = 10; c[ "populations" ][ 8 ].erase( "pos" ); expect_rejected( t, c, "a locked population resized", cells ); }
  { json c = ref; c[ "populations" ][ 3 ][ "params" ][ "I_e" ] = 100.0; expect_rejected( t, c, "a locked population given another background current", cells ); }
  { json c = ref; c[ "populations" ][ 3 ].erase( "locked" ); expect_rejected( t, c, "the locked flag removed from a population", cells ); }
  { json c = ref; c[ "populations" ].push_back( { { "id", "X" }, { "model", "iaf_psc_alpha" }, { "n", 1 } } ); expect_rejected( t, c, "a population added", cells ); }
  { json c = set_group( ref, "AVA", "sign", -1 ); expect_rejected( t, c, "a class's sign flipped (not free)", type ); }
  { json c = set_group( ref, "AVA", "free", { "scale", "sign" } ); expect_rejected( t, c, "a class made free to sign", type ); }
  { json c = set_group( ref, "AVA", "ranges", { { "scale", { 0.0, 3.0 } } } ); expect_rejected( t, c, "the allowed range widened", type ); }
  { json c = set_group( ref, "AVA", "off", false ); c = set_group( c, "AVA", "ranges", { { "scale", { 0.01, 1.0 } } } ); c = set_group( c, "AVA", "scale", 0.5 ); expect_rejected( t, c, "off taken away (a different cell type)", type ); }
  { json c = ref; c[ "groups" ].erase( 0 ); expect_rejected( t, c, "a class removed from the groups", type ); }
  { json c = ref; c.erase( "groups" ); expect_rejected( t, c, "the groups deleted", type ); }
  { json c = ref; c[ "inputs" ][ 0 ][ "weight" ] = 900.0; expect_rejected( t, c, "the touch weight changed", chan ); }
  { json c = ref; c[ "inputs" ][ 0 ][ "targets" ] = json::array( { { { "pop", "ALM" } } } ); expect_rejected( t, c, "the touch moved off one cell class", chan ); }
  { json c = ref; c[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "DA" } } } ); expect_rejected( t, c, "the backward motors read from fewer cells", chan ); }
  { json c = scales( ref, { { "AVD", 1.5 } }, 0.0 ); expect_rejected( t, c, "a free scale above its range", "range" ); }
  // a free value anywhere in 0 to 1, positions, names, the seed are not tampering
  for ( double s : { 0.0, 0.25, 0.5, 1.0 } )
  {
    const Res r = run( t, scales( ref, { { "AVD", s } }, 0.0 ), 101, true, "standard", "", false, 0, false );
    CHECK_MSG( r.loaded and not rejected( r ), "scale " + std::to_string( s ) + " inside the range was rejected: " + r.verdict );
  }
  { json c = ref; c[ "name" ] = "mine"; c[ "seed" ] = 5; c[ "populations" ][ 0 ][ "pos" ][ 0 ] = { 10, 10 }; c[ "meta" ][ "notes" ] = "x";
    const Res r = run( t, c, 101, true, "standard", "", false, 0, false );
    CHECK_MSG( r.loaded and not rejected( r ) and r.passed, "positions, name, notes and seed are free: " + r.verdict ); }
  // the whole worm: a changed edge or a regrouped class is rejected
  const json motif = load_file( "a_motif.json" );
  { json c = motif; c[ "projections" ][ 10 ][ "edges" ][ 0 ][ 2 ] = c[ "projections" ][ 10 ][ "edges" ][ 0 ][ 2 ].get< double >() + 1.0; expect_rejected( "a_motif", c, "a_motif: a synapse count changed", wiring ); }
  { json c = motif; c[ "projections" ].erase( 10 ); expect_rejected( "a_motif", c, "a_motif: a projection removed", wiring ); }
  { json c = motif; c[ "groups" ][ 0 ][ "pops" ].erase( 0 ); expect_rejected( "a_motif", c, "a_motif: a class taken out of its cell-type block", type ); }
  { json c = motif; std::swap( c[ "groups" ][ 0 ][ "pops" ], c[ "groups" ][ 1 ][ "pops" ] ); expect_rejected( "a_motif", c, "a_motif: the blocks exchanged", type ); }
  { json c = motif; c[ "inputs" ] = json::array( { { { "name", "x" }, { "kind", "poisson" }, { "targets", json::array( { { { "pop", motif[ "populations" ][ 0 ][ "id" ] } } } ) }, { "weight", 1.0 }, { "value", 0.0 } } } );
    expect_rejected( "a_motif", c, "a_motif: a stimulus added", "" ); }
}

// ---------------------------------------------------------------- 4. a_path
void test_a_path()
{
  const json ref = load_file( "a_path.json" ), given = load_file( "a_path_given.json" );
  std::printf( "a_path: reference, routes kept alone\n" );
  for ( const char* mode : { "guided", "standard", "researcher" } )
    for ( bool quick : { true, false } )
      for ( uint64_t seed = 101; seed <= 105; ++seed )
      {
        const Res r = run( "a_path", ref, seed, quick, mode );
        CHECK_MSG( r.passed, std::string( "reference " ) + mode + ( quick ? " quick" : " full" ) + " seed " + std::to_string( seed ) + ": " + r.verdict );
        CHECK_MSG( r.parts.at( "backs_up" ) == 0.0 and r.parts.at( "a_hz" ) < 1.0, "the kept route alone should not carry the response: " + r.verdict );
        CHECK_MSG( r.verdict.find( "does not back up" ) != std::string::npos and r.verdict.find( "not yet a pathway that works" ) != std::string::npos, "the verdict must tell what the run showed: " + r.verdict );
        no_digit( "a_path verdict", r.verdict );
      }
  for ( const char* route : { "AVB", "AVD", "AVE" } )
    for ( uint64_t seed : { 1, 2, 3, 101, 102, 103, 104, 105 } )
    {
      const Res r = run( "a_path", scales( ref, { { route, 1.0 } }, 0.0 ), seed );
      CHECK_MSG( r.passed and r.parts.at( "backs_up" ) == 0.0, std::string( "route " ) + route + " seed " + std::to_string( seed ) + ": " + r.verdict );
    }
  // the given circuit has nothing silenced: the level says so; the control of the intact worm shows what the response is
  { const Res r = run( "a_path", given, 101, true, "standard", "", false, 0, true );
    CHECK( not r.passed and r.verdict.find( "Nothing is silenced" ) != std::string::npos );
    CHECK_MSG( r.parts.at( "backs_up" ) == 1.0 and r.parts.at( "b_hz" ) < 1.0 and r.parts.at( "a_base_hz" ) < 1.0 and r.parts.at( "a_after_hz" ) < 1.0, "the intact worm backs up and is silent before and after" );
    no_digit( "a_path given", r.verdict ); }
  const Res r = run( "a_path", given );
  CHECK( r.hints.size() == 3 and r.hints[ 1 ].target == "DA" );
  for ( const auto& h : r.hints ) no_digit( "a_path hint " + std::to_string( h.tier ), h.text );
  auto t = make_task( "a_path" );
  CHECK( not t->brief().empty() and not t->recipe().empty() and t->channel_kinds().count( "touch_head" ) and t->chapter() == "a_worm" and t->on_spine() and t->parent().empty() );
  CHECK( not t->notebook().citation.empty() and t->notebook().text.find( "route on paper" ) != std::string::npos );
}

// ---------------------------------------------------------------- 5. a_predict
void test_a_predict()
{
  const json given = load_file( "a_predict_given.json" ), ref = load_file( "a_predict.json" );
  auto t = make_task( "a_predict" );
  const auto d = t->diagnosis();
  std::printf( "a_predict: one commit per class, seeds 1 to 3 and 101 to 105\n" );
  CHECK( d.bet and d.budget == 3 and d.candidates.size() == 5 and d.answer == "AVE" and d.candidates.front() != d.answer );
  CHECK( std::set< std::string >( d.candidates.begin(), d.candidates.end() ) == std::set< std::string >( kClasses.begin(), kClasses.end() ) );
  for ( const auto& c : d.candidates ) CHECK( d.labels.count( c ) and d.explain.count( c ) );
  std::string survivor;
  for ( uint64_t seed : { 1, 2, 3, 101, 102, 103, 104, 105 } )
  {
    std::vector< std::string > passing;
    for ( const auto& k : kClasses )
    {
      const Res r = run( "a_predict", scales( given, { { k, 0.0 } }, 1.0 ), seed, true, "standard", k );
      CHECK_MSG( r.loaded and not rejected( r ), k + ": " + r.verdict );
      no_digit( "a_predict verdict", r.verdict );
      if ( r.passed )
      {
        passing.push_back( k );
        CHECK_MSG( r.value == 100.0 and r.verdict.rfind( "Correct", 0 ) == 0, "a right commit scores in full: " + r.verdict );
      }
      else
        CHECK_MSG( r.verdict.find( "needs this class" ) != std::string::npos, k + ": " + r.verdict );
    }
    CHECK_MSG( passing.size() == 1, "seed " + std::to_string( seed ) + ": exactly one class may be silenced without losing the response, found " + std::to_string( passing.size() ) );
    if ( passing.size() == 1 ) { if ( survivor.empty() ) survivor = passing[ 0 ]; CHECK_MSG( passing[ 0 ] == survivor, "the surviving class changes with the seed" ); }
  }
  CHECK_MSG( survivor == d.answer, "the run's surviving class (" + survivor + ") is not Diagnosis::answer (" + d.answer + ")" );
  // the commit and the lesion must agree
  { const Res r = run( "a_predict", ref, 101, true, "standard", "AVA" ); CHECK( not r.passed and r.verdict.find( "committed to AVA but silenced AVE" ) != std::string::npos ); no_digit( "mismatch", r.verdict ); }
  { const Res r = run( "a_predict", ref, 101, true, "standard", "XYZ" ); CHECK( not r.passed and r.verdict.find( "not one of the five" ) != std::string::npos ); }
  // no commit: a control run, capped; nothing silenced: no lesion; two silenced: no single bet; partial: no lesion
  { const Res r = run( "a_predict", ref ); CHECK_MSG( r.passed and r.value <= 60.0 and r.verdict.find( "control run" ) != std::string::npos, r.verdict ); no_digit( "control", r.verdict ); }
  { const Res r = run( "a_predict", given, 101, true, "standard", "AVE" ); CHECK( not r.passed and r.verdict.find( "No class is silenced" ) != std::string::npos ); }
  { const Res r = run( "a_predict", scales( given, { { "AVE", 0.0 }, { "AVB", 0.0 } }, 1.0 ), 101, true, "standard", "AVE" ); CHECK( not r.passed and r.verdict.find( "More than one class" ) != std::string::npos ); }
  { const Res r = run( "a_predict", scales( given, { { "AVE", 0.4 } }, 1.0 ), 101, true, "standard", "AVE" ); CHECK( not r.passed and r.verdict.find( "partly" ) != std::string::npos ); }
  // the cost of a commit: 100 - 15 per wrong commit - 3 per read beyond the budget, floor 25; revealed scores nothing
  { const Res r = run( "a_predict", ref, 101, true, "standard", "AVE@5:2" ); CHECK_MSG( r.passed and r.value == 64.0, "value " + std::to_string( r.value ) ); no_digit( "costed", r.verdict ); }
  { const Res r = run( "a_predict", ref, 101, true, "standard", "AVE@2:0" ); CHECK( r.passed and r.value == 100.0 ); }
  { const Res r = run( "a_predict", ref, 101, true, "standard", "AVE@40:9" ); CHECK( r.passed and r.value == 25.0 ); }
  { const Res r = run( "a_predict", ref, 101, true, "standard", "AVE@revealed" ); CHECK( not r.passed and r.value == 0.0 and r.verdict.find( "revealed" ) != std::string::npos ); no_digit( "revealed", r.verdict ); }
  const Res r = run( "a_predict", given );
  CHECK( r.hints.size() == 3 );
  for ( const auto& h : r.hints ) no_digit( "a_predict hint " + std::to_string( h.tier ), h.text );
  for ( const auto& [ k, v ] : d.explain ) no_digit( "a_predict explain", v );
  CHECK( not t->brief().empty() and not t->notebook().citation.empty() and t->chapter() == "a_worm" );
}

// ---------------------------------------------------------------- 6. a_motif
void test_a_motif()
{
  const json c = load_file( "a_motif_given.json" );
  auto t = make_task( "a_motif" );
  const auto d = t->diagnosis();
  std::printf( "a_motif: the engine's null bench on the whole wiring\n" );
  CHECK( d.bet and d.budget == 3 and d.candidates.size() == 3 and d.answer == "edges" and d.candidates.front() != d.answer );
  CHECK( t->level_kind() == "diagnosis" and not t->on_spine() and t->parent() == "a_path" and t->chapter() == "a_worm" );
  std::vector< std::string > passing;
  for ( const char* cand : { "edges", "degree", "blocks" } )
  {
    Res r = run( "a_motif", c, 101, true, "standard", cand, false, 20, false );
    CHECK_MSG( r.loaded and not rejected( r ), std::string( cand ) + ": " + r.verdict + r.error );
    no_digit( "a_motif verdict", r.verdict );
    std::printf( "  %-7s z=%7.2f significant=%d  %s\n", cand, r.parts[ std::string( "z_" ) + cand ], static_cast< int >( r.parts[ std::string( "significant_" ) + cand ] ), r.verdict.c_str() );
    if ( r.passed ) passing.push_back( cand );
    CHECK_MSG( ( r.parts[ std::string( "significant_" ) + cand ] == 1.0 ) == r.passed, "a commit passes exactly when its null model gives a significant positive z" );
    if ( not r.passed ) CHECK_MSG( r.value == 0.0 and r.verdict.rfind( "Not that one", 0 ) == 0, r.verdict );
  }
  CHECK_MSG( passing.size() == 1, "exactly one null model must give a significant positive z for the feed-forward loop, found " + std::to_string( passing.size() ) );
  CHECK_MSG( passing.size() == 1 and passing[ 0 ] == d.answer, "the engine's answer is not Diagnosis::answer" );
  { Res r = run( "a_motif", c, 101, true, "standard", "edges", false, 20, false );
    CHECK_MSG( r.parts[ "z_edges" ] > 2.0 and r.parts[ "z_degree" ] < -2.0 and r.parts[ "z_blocks" ] < -2.0, "the sign of every z-score is as research/atlas-a.md 4b reports" );
    CHECK( r.value == 100.0 and r.verdict.rfind( "Correct", 0 ) == 0 ); }
  { Res r = run( "a_motif", c, 101, true, "standard", "edges@5:2", false, 20, false ); CHECK_MSG( r.passed and r.value == 64.0, "value " + std::to_string( r.value ) ); }
  { Res r = run( "a_motif", c, 101, true, "standard", "", false, 20, false ); CHECK_MSG( r.passed and r.value == 60.0 and r.verdict.find( "control run" ) != std::string::npos and r.verdict.find( "more frequent" ) == std::string::npos, "no commit is a control run that tells nothing: " + r.verdict ); no_digit( "motif control", r.verdict ); }
  { Res r = run( "a_motif", c, 101, true, "standard", "edges@revealed", false, 20, false ); CHECK( not r.passed and r.value == 0.0 ); }
  { Res r = run( "a_motif", c, 101, true, "standard", "nonsense", false, 20, false ); CHECK( not r.passed and r.verdict.find( "not one of the three" ) != std::string::npos ); }
  // the full episode and the three modes agree
  for ( const char* mode : { "guided", "standard", "researcher" } )
  {
    Res r = run( "a_motif", c, 102, false, mode, "edges", false, 0, true );
    CHECK_MSG( r.passed, std::string( mode ) + ": " + r.verdict );
  }
  Res r = run( "a_motif", c, 101, true, "standard", "", false, 20, false );
  CHECK( r.hints.size() == 3 );
  for ( const auto& h : r.hints ) no_digit( "a_motif hint " + std::to_string( h.tier ), h.text );
  for ( const auto& [ k, v ] : d.explain ) no_digit( "a_motif explain", v );
  for ( const auto& [ k, v ] : d.labels ) no_digit( "a_motif label", v );
  CHECK( not t->brief().empty() and not t->notebook().citation.empty() );
}

// ---------------------------------------------------------------- 7. dx_a_worm
void test_dx()
{
  const json patient = load_file( "dx_a_worm.json" ), given = load_file( "a_path_given.json" );
  auto t = make_task( "dx_a_worm" );
  const auto d = t->diagnosis();
  std::printf( "dx_a_worm: the four lesions by what the scope shows\n" );
  CHECK( t->level_kind() == "diagnosis" and not t->on_spine() and t->parent() == "a_path" and t->chapter() == "a_worm" and d.circuit == "dx_a_worm.json" );
  CHECK( d.candidates.size() == 4 and d.answer == "pop:AVD" and d.candidates.front() != d.answer and not d.bet );
  CHECK( d.probes.size() >= 1 and d.probes.size() <= 2 );
  for ( const auto& c : d.candidates ) CHECK( d.labels.count( c ) and d.explain.count( c ) and t->brief().find( c ) != std::string::npos );
  for ( const auto& [ k, v ] : d.explain ) no_digit( "dx explain", v );
  // exactly one candidate passes
  for ( const auto& c : d.candidates )
  {
    auto task = make_task( "dx_a_worm" );
    task->set_variant( c );
    const Score s = task->score();
    CHECK_MSG( s.passed == ( c == d.answer ), c );
    no_digit( "dx verdict", s.verdict );
  }
  // the patient: all cells and connections, the answer's outputs at weight 0, the other classes' as given
  const auto weights_out = [ & ]( const json& c, const std::string& pre ) {
    double w = 0;
    for ( const auto& p : c[ "projections" ] ) if ( p[ "pre" ] == pre ) for ( const auto& e : p[ "edges" ] ) w += e[ 2 ].get< double >();
    return w;
  };
  CHECK( patient[ "projections" ].size() == given[ "projections" ].size() and patient[ "populations" ].size() == given[ "populations" ].size() );
  for ( const auto& k : kClasses ) CHECK_MSG( ( weights_out( patient, k ) == 0.0 ) == ( "pop:" + k == d.answer ), "only the answer's outputs are silent: " + k );
  // the signature: with a class silenced its outputs carry nothing. The motor groups are silent in all four; AVD fires clearly only when it is the silenced one.
  std::map< std::string, std::vector< std::map< std::string, double > > > by;
  for ( const char* lesion : { "AVA", "AVB", "AVD", "PVC" } )
    for ( uint64_t seed : { 1, 2, 3, 101, 102, 103, 104, 105 } )
    {
      json c = given;
      for ( auto& g : c[ "groups" ] ) g[ "scale" ] = g[ "id" ] == lesion ? 0.0 : 1.0;
      const Res r = run( "dx_a_worm", c, seed, false, "standard", "", true );
      CHECK_MSG( r.loaded, r.error );
      by[ lesion ].push_back( r.class_hz );
      std::printf( "    %-4s seed %3d:", lesion, static_cast< int >( seed ) );
      for ( const char* k : { "ALM", "AVM", "AVA", "AVB", "AVD", "AVE", "PVC", "DA", "VA", "DB", "VB" } ) std::printf( " %s %.1f", k, r.class_hz.count( k ) ? r.class_hz.at( k ) : 0.0 );
      std::printf( "\n" );
    }
  for ( const auto& [ lesion, rows ] : by )
  {
    double lo = 1e9, hi = 0;
    for ( const auto& r : rows )
    {
      for ( const char* m : { "DA", "VA", "DB", "VB" } ) CHECK_MSG( ( r.count( m ) ? r.at( m ) : 0.0 ) < 1.0, lesion + ": the motor group " + m + " must be silent" );
      const double avd = r.count( "AVD" ) ? r.at( "AVD" ) : 0.0;
      lo = std::min( lo, avd ); hi = std::max( hi, avd );
      CHECK_MSG( r.at( "ALM" ) > 20.0 and r.at( "AVM" ) > 20.0, lesion + ": the touch cells fire as usual" );
    }
    std::printf( "  lesion %-4s AVD %.1f..%.1f Hz\n", lesion.c_str(), lo, hi );
    if ( lesion == "AVD" ) CHECK_MSG( lo >= 8.0, "AVD must fire clearly when it is the silenced class" );
    else CHECK_MSG( hi <= 6.0, "AVD must stay nearly silent when " + lesion + " is the silenced class" );
  }
  { const Res r = run( "dx_a_worm", patient, 101, true, "standard", "", true, 0, true );   // the patient itself
    CHECK_MSG( r.loaded and r.parts.at( "run_passed" ) == 0.0, "the patient must fail its world" );
    CHECK_MSG( r.class_hz.count( "AVD" ) and r.class_hz.at( "AVD" ) >= 8.0, "the patient's AVD fires clearly" ); }
  const Res r = run( "dx_a_worm", patient );
  CHECK( r.hints.size() == 3 );
  for ( const auto& h : r.hints ) no_digit( "dx hint " + std::to_string( h.tier ), h.text );
  CHECK( not t->notebook().citation.empty() and t->concepts_new().size() == 1 );
}
}  // namespace

int main()
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  test_format_off();
  test_digests();
  test_tampering();
  test_a_path();
  test_a_predict();
  test_a_motif();
  test_dx();
  std::printf( "%zu distinct texts checked for digits\n", g_texts_checked.size() );
  return chk::finish( "tasks_atlas_worm_test" );
}
