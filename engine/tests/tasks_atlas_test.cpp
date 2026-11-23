// Locked wiring and cell-type groups (0.6, the Atlas wing; docs/CIRCUIT-FORMAT.md "Locked parts and groups").
//   1. the format: groups apply sign and scale to the weights of every projection out of their populations; locked flags; what Engine::structure() reports; load errors;
//      a file without groups behaves as before.
//   2. the level contract (Task::given, make_task's guard): the development-only level atlas_probe rejects every tampering with what is locked or not free, in the engine
//      and in spike-run (a hand-edited file fails the same way), with a plain verdict that names no number; honest edits (the free values, positions, names) are not tampering.
//   3. the level itself: reference passes (seeds 101..105, quick and full, three modes), the given circuit and the cheap variants fail, lesions fail for behaviour.
#include <unistd.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <tuple>
#include <fstream>
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

// ---------------------------------------------------------------- 1. the format
json tiny( const json& groups = json() )
{
  json c = { { "schema_version", 1 }, { "name", "tiny" }, { "resolution_ms", 0.1 }, { "seed", 3 },
    { "populations", { { { "id", "A" }, { "model", "iaf_psc_alpha" }, { "n", 2 } }, { { "id", "B" }, { "model", "iaf_psc_alpha" }, { "n", 2 } }, { { "id", "C" }, { "model", "iaf_psc_alpha" }, { "n", 1 } } } },
    { "projections", { { { "pre", "A" }, { "post", "C" }, { "edges", { { 0, 0, 40.0, 1.5 }, { 1, 0, -60.0, 1.5 } } } },
                       { { "pre", "B" }, { "post", "C" }, { "rule", "all_to_all" }, { "synapse", { { "weight", 25.0 }, { "delay", 2.0 } } } },
                       { { "pre", "C" }, { "post", "A" }, { "edges", { { 0, 0, 10.0, 1.0 } } } } } },
    { "inputs", json::array() }, { "outputs", json::array() } };
  if ( not groups.is_null() ) c[ "groups" ] = groups;
  return c;
}

bool loads( const json& c, std::string* err = nullptr )
{
  Engine e;
  const bool ok = e.load( c.dump() );
  if ( err ) *err = e.error();
  return ok;
}

void test_groups_apply()
{
  {
  Engine e;
  // A: sign -1, scale 2 (edges 40 and -60 become -80 and -120: |w| x scale x sign); B: sign +1, scale 0.5 on a rule projection (25 -> 12.5); C: no group (unchanged)
  json g = json::array( { { { "id", "ga" }, { "pops", { "A" } }, { "sign", -1 }, { "scale", 2.0 }, { "free", { "sign", "scale" } }, { "ranges", { { "scale", { 0.5, 4.0 } } } } },
                          { { "id", "gb" }, { "pops", { "B" } }, { "sign", 1 }, { "scale", 0.5 } } } );
  CHECK_MSG( e.load( tiny( g ).dump() ), e.error() );
  const Structure st = e.structure();
  CHECK( st.groups.size() == 2 );
  CHECK( st.projections[ 0 ].sign == -1 and std::fabs( st.projections[ 0 ].w_min - -120.0 ) < 1e-9 and std::fabs( st.projections[ 0 ].w_max - -80.0 ) < 1e-9 );
  CHECK( st.projections[ 1 ].sign == 1 and std::fabs( st.projections[ 1 ].w_mean - 12.5 ) < 1e-9 );
  CHECK( st.projections[ 2 ].sign == 1 and std::fabs( st.projections[ 2 ].w_mean - 10.0 ) < 1e-9 );
  const GroupInfo* ga = st.group( "ga" );
  CHECK( ga != nullptr and ga->sign == -1 and ga->scale == 2.0 and ga->free_sign and ga->free_scale and ga->scale_min == 0.5 and ga->scale_max == 4.0 and ga->scale_in_range );
  CHECK( ga != nullptr and ga->free.size() == 2 and ga->free[ 0 ] == "sign" and ga->free[ 1 ] == "scale" and ga->pops.size() == 1 and ga->pops[ 0 ] == "A" );
  const GroupInfo* gb = st.group( "gb" );
  CHECK( gb != nullptr and not gb->free_sign and not gb->free_scale and gb->free.empty() );
  CHECK( st.group( "nope" ) == nullptr );
  CHECK( st.population( "A" )->group == "ga" and st.population( "B" )->group == "gb" and st.population( "C" )->group.empty() );
  CHECK( st.group_of( "A" ) == ga and st.group_of( "C" ) == nullptr );
  CHECK( not st.population( "A" )->locked and not st.projections[ 0 ].locked );
  }

  // the live NEST weights are the effective ones, and the stored magnitude is |w| (a stored -60 under sign +1 is +60)
  json g2 = json::array( { { { "id", "ga" }, { "pops", { "A" } }, { "sign", 1 }, { "scale", 1.0 } } } );
  {
    Engine e2;
    CHECK_MSG( e2.load( tiny( g2 ).dump() ), e2.error() );
    double wsum = 0;
    for ( const auto& c : e2.connections() )
      if ( c.projection == 0 ) wsum += c.weight;
    CHECK_MSG( std::fabs( wsum - 100.0 ) < 1e-9, "sum of the weights of A>C is " + std::to_string( wsum ) );
  }

  // a file without groups behaves exactly as before: the stored weights are the weights
  {
    Engine e3;
    CHECK( e3.load( tiny().dump() ) );
    const Structure s3 = e3.structure();
    CHECK( s3.groups.empty() and std::fabs( s3.projections[ 0 ].w_min - -60.0 ) < 1e-9 and std::fabs( s3.projections[ 0 ].w_max - 40.0 ) < 1e-9 );
  }
}

void test_groups_weight_spread()
{
  // weight_sd is scaled with the group's scale, weight_cv is relative: the same z draws, so the spread edges are scale x (|w| + sd z) x sign
  json plain = tiny();
  plain[ "projections" ][ 0 ][ "weight_sd" ] = 5.0;
  json grouped = tiny( json::array( { { { "id", "ga" }, { "pops", { "A" } }, { "sign", 1 }, { "scale", 2.0 } } } ) );
  grouped[ "projections" ][ 0 ] = { { "pre", "A" }, { "post", "C" }, { "weight_sd", 5.0 }, { "edges", { { 0, 0, 40.0, 1.5 }, { 1, 0, 60.0, 1.5 } } } };
  plain[ "projections" ][ 0 ] = grouped[ "projections" ][ 0 ];
  double wa = 0, wb = 0;
  { Engine a; CHECK( a.load( plain.dump() ) ); for ( const auto& c : a.connections() ) if ( c.projection == 0 ) wa += c.weight; }
  { Engine b; CHECK( b.load( grouped.dump() ) ); for ( const auto& c : b.connections() ) if ( c.projection == 0 ) wb += c.weight; }
  CHECK_MSG( std::fabs( wb - 2.0 * wa ) < 1e-6, "grouped spread sum " + std::to_string( wb ) + " vs twice the plain " + std::to_string( wa ) );
}

void test_locked_flags()
{
  json c = tiny();
  c[ "populations" ][ 1 ][ "locked" ] = true;
  c[ "projections" ][ 0 ][ "locked" ] = true;
  {
    Engine e;
    CHECK_MSG( e.load( c.dump() ), e.error() );
    const Structure st = e.structure();
    CHECK( st.population( "B" )->locked and not st.population( "A" )->locked );
    CHECK( st.projections[ 0 ].locked and not st.projections[ 1 ].locked and not st.projections[ 2 ].locked );
  }
  // an old file (no flags) reads as unlocked everywhere
  Engine e2;
  CHECK( e2.load( tiny().dump() ) );
  for ( const auto& p : e2.structure().populations ) CHECK( not p.locked );
  for ( const auto& p : e2.structure().projections ) CHECK( not p.locked );
}

void test_format_errors()
{
  auto bad = [ & ]( json c, const std::string& word, const std::string& label ) {
    std::string err;
    const bool ok = loads( c, &err );
    CHECK_MSG( not ok, label + ": loaded" );
    CHECK_MSG( err.find( word ) != std::string::npos, label + ": the error '" + err + "' does not mention '" + word + "'" );
  };
  auto grp = []( json g ) { return json::array( { g } ); };
  json base = { { "id", "g" }, { "pops", { "A" } } };
  auto with = [ & ]( const std::string& k, json v ) { json g = base; g[ k ] = v; return tiny( grp( g ) ); };
  CHECK( loads( tiny( grp( base ) ) ) );                                                       // a group with nothing but id and pops is the identity
  bad( with( "sign", 0 ), "groups[0].sign", "sign 0" );
  bad( with( "sign", 2 ), "groups[0].sign", "sign 2" );
  bad( with( "sign", "-" ), "groups[0].sign", "sign text" );
  bad( with( "scale", 0 ), "groups[0].scale", "scale 0" );
  bad( with( "scale", -1.0 ), "groups[0].scale", "negative scale" );
  bad( with( "free", { "tau" } ), "unknown free parameter", "unknown free parameter" );
  bad( with( "free", { "scale" } ), "range", "free scale without a range" );
  bad( with( "free", { "sign", "sign" } ), "twice", "free listed twice" );
  json r = base; r[ "free" ] = { "scale" }; r[ "ranges" ] = { { "scale", { 2.0, 1.0 } } };
  bad( tiny( grp( r ) ), "ranges", "range hi < lo" );
  r[ "ranges" ] = { { "scale", { 0.0, 1.0 } } };
  bad( tiny( grp( r ) ), "ranges", "range from 0" );
  json r2 = base; r2[ "free" ] = json::array(); r2[ "ranges" ] = { { "scale", { 0.5, 2.0 } } };
  bad( tiny( grp( r2 ) ), "not in the free list", "range for a parameter that is not free" );
  bad( with( "pops", { "Z" } ), "unknown population", "unknown population" );
  bad( with( "pops", json::array() ), "groups[0].pops", "empty pops" );
  bad( with( "colour", "red" ), "unknown key", "unknown key" );
  bad( tiny( json::array( { base, base } ) ), "unique", "duplicate group id" );
  json other = { { "id", "h" }, { "pops", { "A" } } };
  bad( tiny( json::array( { base, other } ) ), "already in a group", "a population in two groups" );
  json c = tiny(); c[ "groups" ] = { { "id", "g" } };
  bad( c, "groups", "a group that is not a list" );
  json lk = tiny(); lk[ "projections" ][ 1 ][ "locked" ] = true;
  bad( lk, "locked projection lists its edges", "a locked rule projection" );
  json lk2 = tiny(); lk2[ "projections" ][ 0 ][ "locked" ] = "yes";
  bad( lk2, "projections[0].locked", "locked as text" );
  json lk3 = tiny(); lk3[ "populations" ][ 0 ][ "locked" ] = 1;
  bad( lk3, "populations[0].locked", "locked as a number" );
  json wt = tiny( grp( base ) ); wt[ "projections" ][ 1 ] = { { "pre", "A" }, { "post", "C" }, { "rule", "all_to_all" }, { "synapse", { { "weight", { { "distribution", "normal" } } } } } };
  bad( wt, "plain number", "a distribution weight out of a group" );
  // a free value outside its range still loads (the sandbox may hold it); the level is what refuses it
  json oor = base; oor[ "free" ] = { "scale" }; oor[ "ranges" ] = { { "scale", { 0.5, 1.0 } } }; oor[ "scale" ] = 3.0;
  Engine e;
  CHECK( e.load( tiny( grp( oor ) ).dump() ) );
  CHECK( e.structure().group( "g" ) != nullptr and not e.structure().group( "g" )->scale_in_range );
}

// ---------------------------------------------------------------- 2. the level contract
struct Res
{
  bool loaded = false, passed = false;
  double value = 0;
  std::string verdict, error;
  std::map< std::string, double > parts;
  std::vector< Task::Hint > hints;
};

Res run_task( const json& c, uint64_t seed = 101, bool quick = true, const std::string& mode = "standard",
              const std::vector< std::tuple< std::string, std::string, double > >& lesions = {} )
{
  Res r;
  Engine e;
  json cc = c;
  r.loaded = e.load( cc.dump() );
  if ( not r.loaded ) { r.error = e.error(); return r; }
  // a lesion is an experiment on the loaded circuit, not an edit of the file (the file would no longer be the given circuit)
  const Structure st = e.structure();
  const auto conns = e.connections();
  for ( const auto& [ pre, post, factor ] : lesions )
    for ( const auto& pi : st.projections )
      if ( pi.pre_pop == pre and pi.post_pop == post )
      {
        const json& edges = c[ "projections" ][ pi.index ][ "edges" ];
        for ( size_t k = 0; k < edges.size(); ++k )
          for ( const auto& q : conns )
            if ( q.projection == pi.index and q.pre == st.populations[ pi.pre_index ].first_index + edges[ k ][ 0 ].get< int >() and q.post == st.populations[ pi.post_index ].first_index + edges[ k ][ 1 ].get< int >() )
            { e.set_weight( pi.index, static_cast< int >( k ), q.weight * factor ); break; }
      }
  auto t = make_task( "atlas_probe" );
  t->set_mode( mode );
  t->set_quick( quick );
  t->reset( seed );
  const long steps = static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() );
  for ( long s = 0; s < steps; ++s ) t->step( e, 1.0 );
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

void expect_rejected( const json& c, const std::string& label, const std::string& word )
{
  const Res r = run_task( c );
  CHECK_MSG( r.loaded, label + ": does not load: " + r.error );
  CHECK_MSG( rejected( r ) and not r.passed, label + ": not rejected (" + r.verdict + ")" );
  CHECK_MSG( r.value <= 49.0, label + ": value not capped" );
  CHECK_MSG( r.verdict.find( word ) != std::string::npos, label + ": verdict '" + r.verdict + "' lacks '" + word + "'" );
  CHECK_MSG( not has_digit( r.verdict ), label + ": a number in the verdict: " + r.verdict );
  std::printf( "  rejected: %-48s | %.60s...\n", label.c_str(), r.verdict.c_str() );
}

void test_digests()
{
  const json ref = load_file( "atlas_probe.json" ), given = load_file( "atlas_probe_given.json" );
  LockedDigest dr, dg;
  std::string err;
  CHECK( circuit_locked_digest( ref.dump(), dr, err ) and circuit_locked_digest( given.dump(), dg, err ) );
  CHECK_MSG( dr == dg, "the reference and the given circuit differ in something locked or not free" );
  auto t = make_task( "atlas_probe" );
  CHECK_MSG( t->given().digest == dg, "Task::given().digest is not the digest of atlas_probe_given.json: run spike-run --locked-digest on it and update atlas_probe.cpp" );
  CHECK( t->given().circuit == "atlas_probe_given.json" and t->level_kind() == "atlas" );
  { Engine e; CHECK( e.load( given.dump() ) and e.locked_digest() == dg ); }
  // free values, positions, names, notes and the seed are not part of the digest; a whole-valued float equals the integer
  json c = ref;
  c = set_group( c, "I", "scale", 0.5 );
  c = set_group( c, "I", "sign", 1 );
  c[ "name" ] = "renamed"; c[ "seed" ] = 99; c[ "meta" ][ "notes" ] = "other";
  c[ "populations" ][ 0 ][ "pos" ][ 0 ] = { 1, 2 };
    LockedDigest d2;
  CHECK( circuit_locked_digest( c.dump(), d2, err ) and d2 == dg );
  // each tampering moves exactly the section it belongs to
  auto sec = [ & ]( json t ) {
    LockedDigest d;
    CHECK( circuit_locked_digest( t.dump(), d, err ) );
    return std::make_tuple( d.pops != dg.pops, d.wiring != dg.wiring, d.types != dg.types, d.channels != dg.channels );
  };
  json t1 = ref; t1[ "projections" ][ 0 ][ "edges" ][ 0 ][ 2 ] = 331.0;
  CHECK( sec( t1 ) == std::make_tuple( false, true, false, false ) );
  json t2 = ref; t2[ "populations" ][ 3 ][ "n" ] = 3; t2[ "populations" ][ 3 ].erase( "pos" );
  CHECK( std::get< 0 >( sec( t2 ) ) );
  json t3 = set_group( ref, "P", "scale", 2.0 );
  CHECK( sec( t3 ) == std::make_tuple( false, false, true, false ) );
  json t4 = ref; t4[ "inputs" ][ 0 ][ "weight" ] = 301.0;
  CHECK( sec( t4 ) == std::make_tuple( false, false, false, true ) );
}

void test_tampering()
{
  const json ref = load_file( "atlas_probe.json" );
  std::printf( "tampering with the locked circuit (each must be rejected, verdict without numbers)\n" );
  const std::string wiring = "wiring", cells = "cells", type = "cell type", chan = "inputs and outputs";
  { json c = ref; c[ "projections" ][ 0 ][ "edges" ].push_back( { 2, 0, 330.0, 2.0 } ); expect_rejected( c, "an edge added to a locked projection", wiring ); }
  { json c = ref; c[ "projections" ][ 0 ][ "edges" ].erase( 3 ); expect_rejected( c, "an edge removed", wiring ); }
  { json c = ref; c[ "projections" ][ 2 ][ "edges" ][ 1 ][ 2 ] = 140.0; expect_rejected( c, "an edge re-weighted (stored magnitude)", wiring ); }
  { json c = ref; c[ "projections" ][ 2 ][ "edges" ][ 1 ][ 2 ] = 150.0 * 1.000001; expect_rejected( c, "an edge re-weighted by one part in a million", wiring ); }
  { json c = ref; c[ "projections" ][ 1 ][ "edges" ][ 0 ][ 3 ] = 3.0; expect_rejected( c, "a delay changed", wiring ); }
  { json c = ref; c[ "projections" ][ 1 ][ "synapse" ][ "synapse_model" ] = "tsodyks2_synapse"; expect_rejected( c, "the synapse model changed", wiring ); }
  { json c = ref; c[ "projections" ][ 0 ][ "weight_cv" ] = 0.05; expect_rejected( c, "a weight spread added", wiring ); }
  { json c = ref; c[ "projections" ].push_back( { { "pre", "P" }, { "post", "I" }, { "edges", { { 0, 0, 10.0, 2.0 } } } } ); expect_rejected( c, "a new projection added", wiring ); }
  { json c = ref; c[ "projections" ][ 2 ].erase( "locked" ); expect_rejected( c, "the locked flag removed from a projection", wiring ); }
  { json c = ref; c[ "projections" ].erase( 0 ); expect_rejected( c, "a projection removed", wiring ); }
  { json c = ref; c[ "modulation" ] = json::array( { { { "modulator", "NE" }, { "projection", 0 }, { "param", "weight" }, { "gain", 1.0 } } } ); expect_rejected( c, "a modulation row added", wiring ); }
  { json c = ref; c[ "populations" ][ 3 ][ "n" ] = 3; c[ "populations" ][ 3 ].erase( "pos" ); expect_rejected( c, "a locked population resized", cells ); }
  { json c = ref; c[ "populations" ][ 2 ][ "model" ] = "iaf_psc_exp"; expect_rejected( c, "a locked population's model changed", cells ); }
  { json c = ref; c[ "populations" ][ 0 ][ "params" ] = { { "I_e", 50.0 } }; expect_rejected( c, "a locked population given a parameter", cells ); }
  { json c = ref; c[ "populations" ][ 0 ].erase( "locked" ); expect_rejected( c, "the locked flag removed from a population", cells ); }
  { json c = ref; c[ "populations" ].push_back( { { "id", "X" }, { "model", "iaf_psc_alpha" }, { "n", 1 } } ); expect_rejected( c, "a population added", cells ); }
  { json c = ref; c[ "resolution_ms" ] = 0.05; expect_rejected( c, "the resolution changed", cells ); }
  { json c = set_group( ref, "P", "scale", 2.0 ); expect_rejected( c, "a fixed type's scale changed", type ); }
  { json c = set_group( ref, "N", "sign", -1 ); expect_rejected( c, "a fixed type's sign changed", type ); }
  { json c = set_group( ref, "P", "free", { "scale" } ); c = set_group( c, "P", "ranges", { { "scale", { 0.1, 9.0 } } } ); expect_rejected( c, "a fixed type declared free", type ); }
  { json c = set_group( ref, "I", "ranges", { { "scale", { 0.1, 9.0 } } } ); expect_rejected( c, "the allowed range widened", type ); }
  { json c = set_group( ref, "I", "free", { "scale" } ); expect_rejected( c, "a free parameter dropped from the list", type ); }
  { json c = ref; c[ "groups" ].erase( 0 ); expect_rejected( c, "a group removed", type ); }
  { json c = ref; c[ "groups" ].push_back( { { "id", "X" }, { "pops", json::array() } } ); c[ "groups" ].back()[ "pops" ] = { "M" }; expect_rejected( c, "a group added", type ); }
  { json c = ref; c[ "groups" ][ 2 ][ "pops" ] = { "I", "M" }; expect_rejected( c, "a cell type regrouped", type ); }
  { json c = ref; c.erase( "groups" ); expect_rejected( c, "the groups deleted", type ); }
  { json c = ref; c[ "inputs" ][ 0 ][ "targets" ] = json::array( { { { "pop", "M" } } } ); expect_rejected( c, "an input retargeted to the output cells", chan ); }
  { json c = ref; c[ "inputs" ][ 1 ][ "weight" ] = 120.0; expect_rejected( c, "an input weight changed", chan ); }
  { json c = ref; c[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "P" } } } ); expect_rejected( c, "the output read from other cells", chan ); }
  { json c = ref; c[ "inputs" ].erase( 1 ); expect_rejected( c, "an input removed", "" ); }
  {  // the tampered circuit that would pass without the lock: every live weight is exactly the reference's (so only the digest tells them apart)
    auto weights = []( const json& c ) {
      std::vector< double > w;
      Engine e;
      if ( not e.load( c.dump() ) ) return w;
      for ( const auto& q : e.connections() ) w.push_back( q.weight );
      return w;
    };
    json c = set_group( ref, "I", "scale", 0.25 );
    for ( auto& e : c[ "projections" ][ 2 ][ "edges" ] ) e[ 2 ] = e[ 2 ].get< double >() * 0.98 / 0.25;
    const auto a = weights( ref ), b = weights( c );
    CHECK_MSG( a.size() == b.size() and not a.empty(), "tampered twin: same number of connections" );
    for ( size_t i = 0; i < a.size() and i < b.size(); ++i ) CHECK_MSG( std::fabs( a[ i ] - b[ i ] ) < 1e-9, "tampered twin: live weight " + std::to_string( i ) + " differs" );
    expect_rejected( c, "a twin of the reference by stored weights (identical live weights)", wiring );
  }
  // a free value outside its range
  for ( double s : { 0.1, 2.5, 5.0 } )
  {
    json c = set_group( ref, "I", "scale", s );
    expect_rejected( c, "a free scale outside the range (" + std::to_string( s ) + ")", "range" );
  }
  // what is NOT tampering: the free values anywhere inside the range, positions, names, the seed
  for ( double s : { 0.25, 0.5, 0.98, 1.0, 2.0 } )
  {
    const Res r = run_task( set_group( ref, "I", "scale", s ) );
    CHECK_MSG( r.loaded and not rejected( r ), "scale " + std::to_string( s ) + " inside the range was rejected: " + r.verdict );
  }
  { json c = ref; c[ "name" ] = "mine"; c[ "seed" ] = 5; c[ "populations" ][ 0 ][ "pos" ][ 0 ] = { 10, 10 }; c[ "meta" ][ "notes" ] = "x";
    const Res r = run_task( c );
    CHECK_MSG( r.loaded and not rejected( r ) and r.passed, "positions, name, notes and seed are free: " + r.verdict ); }
}

// a hand-edited file fails in spike-run exactly as in the engine
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

void test_spike_run()
{
  const json ref = load_file( "atlas_probe.json" );
  const auto tmp = std::filesystem::temp_directory_path() / ( "spike_atlas_test_" + std::to_string( getpid() ) + ".json" );
  auto score = [ & ]( const json& c, const std::string& extra, json& out ) {
    std::ofstream( tmp ) << c.dump();
    return spike_run( "--task atlas_probe --circuit " + tmp.string() + " --seeds 101 --quick " + extra, out );
  };
  json out;
  CHECK( score( ref, "", out ) and out[ "passes" ] == 1 );
  json t = ref; t[ "projections" ][ 2 ][ "edges" ][ 0 ][ 2 ] = 160.0;
  CHECK( score( t, "", out ) and out[ "passes" ] == 0 and out[ "runs" ][ 0 ][ "parts" ].value( "circuit_rejected", 0.0 ) == 1.0 );
  CHECK_MSG( not has_digit( out[ "runs" ][ 0 ][ "verdict" ].get< std::string >() ), "digits in the spike-run verdict" );
  t = set_group( ref, "I", "scale", 7.0 );
  CHECK( score( t, "", out ) and out[ "passes" ] == 0 and out[ "runs" ][ 0 ][ "parts" ].value( "circuit_rejected", 0.0 ) == 1.0 );
  // a lesion in spike-run is an experiment on the loaded circuit: it fails the level for its behaviour, and is not read as tampering
  CHECK( score( ref, "--lesion 'P>M*0'", out ) and out[ "passes" ] == 0 and out[ "runs" ][ 0 ][ "parts" ].value( "circuit_rejected", 0.0 ) == 0.0 );
  CHECK( score( ref, "--lesion 'I>M*0'", out ) and out[ "passes" ] == 0 and out[ "runs" ][ 0 ][ "parts" ].value( "circuit_rejected", 0.0 ) == 0.0 );
  json dg;
  CHECK( spike_run( "--locked-digest " + ( circuits_dir() + "atlas_probe_given.json" ), dg ) and dg.contains( "wiring" ) and dg[ "wiring" ].get< std::string >().size() == 32 );
  std::filesystem::remove( tmp );
}

// ---------------------------------------------------------------- 3. the level
void test_level()
{
  const json ref = load_file( "atlas_probe.json" ), given = load_file( "atlas_probe_given.json" );
  std::printf( "the level: reference, the given circuit, cheap variants, lesions\n" );
  for ( const char* mode : { "guided", "standard", "researcher" } )
    for ( bool quick : { true, false } )
      for ( uint64_t seed = 101; seed <= 105; ++seed )
      {
        const Res r = run_task( ref, seed, quick, mode );
        CHECK_MSG( r.passed, std::string( "reference " ) + mode + ( quick ? " quick" : " full" ) + " seed " + std::to_string( seed ) + ": " + r.verdict );
      }
  // fails: the given circuit (wrong on both), all types excitatory, the sign right but the scale at an extreme, the sign wrong but the scale right
  struct Row { std::string label; json c; std::string word; };
  std::vector< Row > rows = {
    { "the given circuit as handed over", given, "inhibitory" },
    { "every type excitatory (the interneuron too), scale in the band", set_group( ref, "I", "sign", 1 ), "inhibitory" },
    { "inhibitory sign, scale at the lowest allowed", set_group( ref, "I", "scale", 0.25 ), "weak" },
    { "inhibitory sign, scale at the highest allowed", set_group( ref, "I", "scale", 2.0 ), "strong" },
    { "inhibitory sign, scale left at the given value", set_group( ref, "I", "scale", 0.5 ), "weak" } };
  for ( const auto& row : rows )
    for ( uint64_t seed : { 101, 102 } )
      for ( const char* mode : { "guided", "standard", "researcher" } )
      {
        const Res r = run_task( row.c, seed, true, mode );
        CHECK_MSG( r.loaded and not r.passed, row.label + " passed in " + mode );
        CHECK_MSG( r.verdict.find( row.word ) != std::string::npos, row.label + ": verdict lacks '" + row.word + "': " + r.verdict );
        CHECK_MSG( not has_digit( r.verdict ), row.label + ": digits in the verdict" );
        CHECK_MSG( not rejected( r ), row.label + ": read as tampering, but it is an honest value" );
      }
  // lesions of the reference: every projection matters
  for ( const auto& l : std::vector< std::tuple< std::string, std::string, double > >{ { "P", "M", 0.0 }, { "N", "I", 0.0 }, { "I", "M", 0.0 }, { "P", "M", 0.1 } } )
  {
    const Res r = run_task( ref, 101, true, "standard", { l } );
    CHECK_MSG( r.loaded and not r.passed and not rejected( r ), std::get< 0 >( l ) + ">" + std::get< 1 >( l ) + " lesioned: " + r.verdict );
    CHECK_MSG( not has_digit( r.verdict ), "digits in a lesion verdict" );
  }
  // hints: three tiers, no digit in tiers 1 to 3, tier 2 names a population
  const Res r = run_task( given );
  CHECK( r.hints.size() == 3 );
  for ( const auto& h : r.hints ) CHECK_MSG( not has_digit( h.text ), "digit in hint " + std::to_string( h.tier ) );
  CHECK( r.hints.size() == 3 and r.hints[ 1 ].target == "I" );
  auto t = make_task( "atlas_probe" );
  CHECK( not t->brief().empty() and not t->recipe().empty() and t->channel_kinds().count( "plus" ) );
  // the stepper of the editor moves in 0.05: the reference's free value is off that grid, so a bench the player sets by hand never equals the reference
  double s = 0;
  for ( const auto& g : ref[ "groups" ] ) if ( g[ "id" ] == "I" ) s = g[ "scale" ].get< double >();
  CHECK_MSG( std::fabs( s / 0.05 - std::round( s / 0.05 ) ) > 0.05, "the reference scale lies on the editor's 0.05 grid" );
}
}  // namespace

int main()
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  test_groups_apply();
  test_groups_weight_spread();
  test_locked_flags();
  test_format_errors();
  test_digests();
  test_tampering();
  test_spike_run();
  test_level();
  return chk::finish( "tasks_atlas_test" );
}
