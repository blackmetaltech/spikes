// The Bestiary chapters A of 0.5 (worm, leech, aplysia): twelve levels, three per chapter on the spine and one diagnosis case each.
//
//   tasks_bestiary_a_test                    the CTest
//   tasks_bestiary_a_test run <task> <circuit.json> [seeds=1,2,3] [mode=standard] [quick]   tuning aid: score one circuit, print parts and verdict
//
// Per level: the v0.3 contract (level kind, chapter, concepts that exist in the game's glossary, three hint tiers, a notebook entry with a DOI, a default model, a recipe, a
// quick episode no longer than the full one), the honesty contract (no digit in any verdict or hint of tiers 1-3: numbers live in recipe() only), the reference circuit passes on
// 5 seeds in guided, standard and researcher mode and is deterministic, the quick and the full episode agree, the CHECK (quick episode) time is printed.
// Diagnosis cases: the patient circuit loads, every candidate is a projection of it, the right answer passes and every wrong one fails, nothing submitted and an unknown id fail.
// The negative tests (cheap circuits that must fail) are in cheap_rows_d.inc (tasks_cheap_test_d).
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

namespace
{
struct Result
{
  Score score;
  bool loaded = false;
  std::string error;
  double wall_s = 0;
};

std::set< std::string > g_text;  // every verdict and hint of tiers 1-3 seen, for the no-digit scan

Result run_circuit( const std::string& id, const json& circuit_in, uint64_t seed, const std::string& mode = "standard", bool quick = false, const std::string& answer = "" )
{
  Result r;
  const auto t0 = std::chrono::steady_clock::now();
  Engine e;
  json c = circuit_in;
  c[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );
  r.loaded = e.load( c.dump() );
  if ( !r.loaded )
  {
    r.error = e.error();
    return r;
  }
  auto t = make_task( id );
  t->set_mode( mode );
  t->set_quick( quick );
  t->reset( seed );
  const long steps = static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() );
  for ( long s = 0; s < steps; ++s ) t->step( e, 1.0 );
  if ( !answer.empty() ) t->set_variant( answer );
  r.score = t->score();
  g_text.insert( r.score.verdict );
  for ( const auto& h : t->hints() )
    if ( h.tier <= 3 ) g_text.insert( h.text );
  r.wall_s = std::chrono::duration< double >( std::chrono::steady_clock::now() - t0 ).count();
  return r;
}

json load_circuit( const std::string& id )
{
  const std::string path = std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + reference_circuit( id );
  const std::string txt = chk::slurp( path );
  CHECK_MSG( !txt.empty(), "cannot read " + path );
  return txt.empty() ? json::object() : json::parse( txt );
}

int neuron_count( const json& c )
{
  int n = 0;
  for ( auto& p : c[ "populations" ] ) n += p[ "n" ].get< int >();
  return n;
}

bool has_digit( const std::string& s )
{
  for ( char ch : s )
    if ( std::isdigit( static_cast< unsigned char >( ch ) ) ) return true;
  return false;
}

struct Level
{
  const char* id;
  const char* chapter;
  const char* kind;
};
const Level kLevels[] = { { "w_latch", "worm", "micro" }, { "w_switch", "worm", "micro" }, { "worm_touch", "worm", "task" }, { "l_burst", "leech", "micro" },
  { "l_rebound", "leech", "micro" }, { "leech_heart", "leech", "task" }, { "a_depress", "aplysia", "micro" }, { "a_facilitate", "aplysia", "micro" }, { "aplysia_gill", "aplysia", "task" } };
const char* kDx[][2] = { { "dx_worm", "worm" }, { "dx_leech", "leech" }, { "dx_aplysia", "aplysia" } };

std::set< std::string > glossary_ids()
{
  std::set< std::string > ids;
  const std::string g = chk::slurp( std::string( SPIKE_SOURCE_DIR ) + "/godot/game/campaign.gd" );
  const size_t a = g.find( "const CONCEPTS := {" );
  const size_t b = g.find( "static var pending_variant" );
  if ( a == std::string::npos || b == std::string::npos ) return ids;
  for ( size_t i = a; i < b; )
  {
    const size_t nl = g.find( '\n', i );
    const std::string line = g.substr( i, nl == std::string::npos ? std::string::npos : nl - i );
    if ( line.size() > 3 && line[ 0 ] == '\t' && line[ 1 ] == '"' )
    {
      const size_t q = line.find( '"', 2 );
      if ( q != std::string::npos && line.compare( q, 4, "\": [" ) == 0 ) ids.insert( line.substr( 2, q - 2 ) );
    }
    if ( nl == std::string::npos ) break;
    i = nl + 1;
  }
  return ids;
}

void contract( const Level& L, const std::set< std::string >& gloss, bool is_dx )
{
  auto t = make_task( L.id );
  CHECK_MSG( t != nullptr, std::string( "unknown task " ) + L.id );
  if ( !t ) return;
  CHECK_MSG( t->chapter() == L.chapter, std::string( L.id ) + ": chapter" );
  CHECK_MSG( t->level_kind() == L.kind, std::string( L.id ) + ": level_kind " + t->level_kind() );
  CHECK_MSG( !t->title().empty() && !t->brief().empty(), std::string( L.id ) + ": title and brief" );
  CHECK_MSG( t->quick_duration_ms() <= t->duration_ms() + 1e-9 && t->quick_duration_ms() > 0, std::string( L.id ) + ": quick episode" );
  CHECK_MSG( !t->notebook().title.empty() && !t->notebook().text.empty() && t->notebook().citation.find( "doi:10." ) != std::string::npos, std::string( L.id ) + ": notebook with a DOI" );
  for ( const auto& c : t->concepts_new() ) CHECK_MSG( gloss.count( c ), std::string( L.id ) + ": concept '" + c + "' is not in the glossary (godot/game/campaign.gd)" );
  for ( const auto& c : t->concepts_used() ) CHECK_MSG( gloss.count( c ), std::string( L.id ) + ": concept '" + c + "' is not in the glossary" );
  const auto h = t->hints();
  CHECK_MSG( h.size() >= 3 && h[ 0 ].tier == 1 && h[ 1 ].tier == 2 && h[ 2 ].tier == 3, std::string( L.id ) + ": three hint tiers" );
  if ( !is_dx )
  {
    CHECK_MSG( !t->default_model().empty(), std::string( L.id ) + ": default_model" );
    CHECK_MSG( !t->recipe().empty() && has_digit( t->recipe() ), std::string( L.id ) + ": recipe carries the numbers" );
    CHECK_MSG( t->on_spine(), std::string( L.id ) + ": on the spine" );
    CHECK_MSG( t->sensor_channels().size() >= 1 && t->motor_channels().size() >= 1, std::string( L.id ) + ": channels" );
    for ( const auto& c : t->sensor_channels() ) CHECK_MSG( t->channel_kinds().count( c ), std::string( L.id ) + ": channel_kind for " + c );
    CHECK_MSG( !t->metrics().empty(), std::string( L.id ) + ": a quality metric" );
    if ( std::string( L.kind ) == "micro" )
    {
      CHECK_MSG( t->duration_ms() <= 5000, std::string( L.id ) + ": micro episode <= 5 s" );
      CHECK_MSG( t->concepts_new().size() == 1, std::string( L.id ) + ": one new concept" );
    }
  }
  else
  {
    CHECK_MSG( !t->on_spine(), std::string( L.id ) + ": off the spine" );
    CHECK_MSG( t->recipe().empty(), std::string( L.id ) + ": a diagnosis has no recipe" );
    const auto d = t->diagnosis();
    CHECK_MSG( d.candidates.size() >= 4 && d.candidates.size() <= 6, std::string( L.id ) + ": 4-6 candidates" );
    CHECK_MSG( std::find( d.candidates.begin(), d.candidates.end(), d.answer ) != d.candidates.end(), std::string( L.id ) + ": answer among the candidates" );
    for ( const auto& c : d.candidates ) CHECK_MSG( d.labels.count( c ) && d.explain.count( c ), std::string( L.id ) + ": label and explanation for " + c );
    for ( const auto& [ k, v ] : d.explain ) CHECK_MSG( !has_digit( v ), std::string( L.id ) + ": no digit in the explanation of " + k );
  }
}
}  // namespace

int main( int argc, char** argv )
{
  std::setvbuf( stdout, nullptr, _IONBF, 0 );
  if ( argc >= 4 && std::string( argv[ 1 ] ) == "run" )
  {
    const json c = json::parse( chk::slurp( argv[ 3 ] ) );
    std::string seeds = argc > 4 ? argv[ 4 ] : "1,2,3";
    const std::string mode = argc > 5 ? argv[ 5 ] : "standard";
    const bool quick = argc > 6 && std::string( argv[ 6 ] ) == "quick";
    std::stringstream ss( seeds );
    for ( std::string tok; std::getline( ss, tok, ',' ); )
    {
      Result r = run_circuit( argv[ 2 ], c, std::stoull( tok ), mode, quick );
      std::printf( "seed %s value %5.1f %s |", tok.c_str(), r.score.value, r.score.passed ? "PASS" : "fail" );
      for ( auto& [ k, v ] : r.score.parts ) std::printf( " %s=%.3g", k.c_str(), v );
      std::printf( "\n  %s\n", r.score.verdict.c_str() );
    }
    return 0;
  }

  const auto gloss = glossary_ids();
  CHECK_MSG( gloss.size() > 20, "could not read the glossary of godot/game/campaign.gd" );
  const auto ids = task_ids();
  auto pos = [ & ]( const std::string& id ) { return std::find( ids.begin(), ids.end(), id ) - ids.begin(); };

  std::printf( "%-14s %-9s %6s %8s %s\n", "level", "chapter", "cells", "check_s", "reference (guided/standard/researcher x seeds 1-5, quick = full)" );
  for ( const Level& L : kLevels )
  {
    contract( L, gloss, false );
    const json ref = load_circuit( L.id );
    const int cells = neuron_count( ref );
    if ( std::string( L.kind ) == "micro" ) CHECK_MSG( cells <= 9, std::string( L.id ) + ": reference size" );
    int passes = 0, runs = 0;
    for ( const char* mode : { "guided", "standard", "researcher" } )
      for ( uint64_t seed = 1; seed <= 5; ++seed )
      {
        Result r = run_circuit( L.id, ref, seed, mode, false );
        CHECK_MSG( r.loaded, std::string( L.id ) + ": reference does not load: " + r.error );
        ++runs;
        passes += r.score.passed ? 1 : 0;
        CHECK_MSG( r.score.passed, std::string( L.id ) + " / " + mode + " seed " + std::to_string( seed ) + ": the reference fails: " + r.score.verdict );
      }
    double check_s = 0;
    for ( uint64_t seed = 1; seed <= 2; ++seed )
    {
      Result q = run_circuit( L.id, ref, seed, "standard", true ), f = run_circuit( L.id, ref, seed, "standard", false );
      check_s = std::max( check_s, q.wall_s );
      CHECK_MSG( q.score.passed == f.score.passed, std::string( L.id ) + ": the quick and the full episode disagree on seed " + std::to_string( seed ) );
    }
    {  // deterministic
      Result a = run_circuit( L.id, ref, 3 ), b = run_circuit( L.id, ref, 3 );
      CHECK_MSG( a.score.value == b.score.value && a.score.parts == b.score.parts, std::string( L.id ) + ": not deterministic" );
    }
    // the quick episode must agree with the full one on a failing circuit too (the reference with every weight 0)
    {
      json z = ref;
      for ( auto& p : z[ "projections" ] )
      {
        if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) ) p[ "synapse" ][ "weight" ] = 0.0;
        if ( p.contains( "edges" ) ) for ( auto& e : p[ "edges" ] ) e[ 2 ] = 0.0;
      }
      for ( auto& i : z[ "inputs" ] ) i[ "weight" ] = 0.0;
      Result q = run_circuit( L.id, z, 1, "standard", true ), f = run_circuit( L.id, z, 1, "standard", false );
      CHECK_MSG( !q.score.passed && !f.score.passed, std::string( L.id ) + ": a circuit with every weight 0 passes" );
    }
    std::printf( "%-14s %-9s %6d %8.2f %d/%d\n", L.id, L.chapter, cells, check_s, passes, runs );
    CHECK_MSG( check_s < 8.0, std::string( L.id ) + ": CHECK over 8 s" );
  }
  // the campaign order: a chapter's micro-puzzles come before its task, the diagnosis case after
  CHECK( pos( "w_latch" ) < pos( "w_switch" ) && pos( "w_switch" ) < pos( "worm_touch" ) && pos( "worm_touch" ) < pos( "dx_worm" ) );
  CHECK( pos( "l_burst" ) < pos( "l_rebound" ) && pos( "l_rebound" ) < pos( "leech_heart" ) && pos( "leech_heart" ) < pos( "dx_leech" ) );
  CHECK( pos( "a_depress" ) < pos( "a_facilitate" ) && pos( "a_facilitate" ) < pos( "aplysia_gill" ) && pos( "aplysia_gill" ) < pos( "dx_aplysia" ) );

  // ---- diagnosis cases
  for ( const auto& d : kDx )
  {
    const Level L = { d[ 0 ], d[ 1 ], "diagnosis" };
    contract( L, gloss, true );
    auto t = make_task( L.id );
    if ( !t ) continue;
    const auto dg = t->diagnosis();
    const json patient = json::parse( chk::slurp( std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + dg.circuit ) );
    // every candidate is a projection of the patient circuit; the answer's projections have weight 0
    std::set< std::string > have;
    for ( auto& p : patient[ "projections" ] ) have.insert( p[ "pre" ].get< std::string >() + ">" + p[ "post" ].get< std::string >() );
    for ( const auto& cand : dg.candidates )
    {
      std::stringstream ss( cand );
      for ( std::string tok; std::getline( ss, tok, '+' ); ) CHECK_MSG( have.count( tok ), std::string( L.id ) + ": candidate " + tok + " is not a projection of the patient circuit" );
    }
    Result none = run_circuit( L.id, patient, 1, "standard", true );
    CHECK_MSG( none.loaded && !none.score.passed, std::string( L.id ) + ": nothing submitted must not pass" );
    CHECK_MSG( none.score.verdict.find( "No diagnosis" ) != std::string::npos, std::string( L.id ) + ": says that nothing was submitted" );
    Result unknown = run_circuit( L.id, patient, 1, "standard", true, "no>such" );
    CHECK_MSG( !unknown.score.passed, std::string( L.id ) + ": an unknown id passes" );
    for ( uint64_t seed : { 1, 2 } )
    {
      Result ok = run_circuit( L.id, patient, seed, "standard", true, dg.answer );
      CHECK_MSG( ok.score.passed && ok.score.value == 100.0, std::string( L.id ) + ": the right answer fails: " + ok.score.verdict );
      for ( const auto& cand : dg.candidates )
        if ( cand != dg.answer )
        {
          Result bad = run_circuit( L.id, patient, seed, "standard", true, cand );
          CHECK_MSG( !bad.score.passed, std::string( L.id ) + ": the wrong answer " + cand + " passes" );
        }
    }
    // the symptom shows: the patient's own run fails the world of the underlying task
    Result inner = run_circuit( L.id, patient, 1, "standard", false, dg.answer );
    CHECK_MSG( inner.score.parts.count( "run_passed" ) && inner.score.parts.at( "run_passed" ) == 0.0, std::string( L.id ) + ": the patient's own run should fail its world" );
    std::printf( "%-14s %-9s patient circuit: %d projections, %zu candidates ok\n", L.id, L.chapter, static_cast< int >( patient[ "projections" ].size() ), dg.candidates.size() );
  }

  // ---- leech_heart: the test current and what it reads (escape or release)
  {
    const json ref = load_circuit( "leech_heart" );
    auto set_hn = [ & ]( json c, const std::map< std::string, double >& kv ) {
      for ( auto& p : c[ "populations" ] )
        if ( p[ "id" ] == "HN_L" || p[ "id" ] == "HN_R" )
          for ( const auto& [ k, v ] : kv ) p[ "params" ][ k ] = v;
      return c;
    };
    // (1) the reference is a RELEASE oscillator in every mode and seed: the driven side's active phase lengthens clearly (by about a fifth, the pass limit is 5 to 12 percent) and
    //     its silent phase does not: parts signature +1, tilt_active_pct well over the limit, tilt_silent_pct never the larger effect
    double lowest_a = 1e9, highest_s = -1e9;
    for ( const char* mode : { "guided", "standard", "researcher" } )
      for ( uint64_t seed = 1; seed <= 8; ++seed )
      {
        const Result r = run_circuit( "leech_heart", ref, seed, mode, false );
        CHECK_MSG( r.score.parts.count( "signature" ) && r.score.parts.at( "signature" ) == 1.0, std::string( "leech_heart / " ) + mode + " seed " + std::to_string( seed ) + ": the reference should read as a release oscillator: " + r.score.verdict );
        CHECK_MSG( r.score.verdict.find( "release" ) != std::string::npos, std::string( "leech_heart: the pass verdict names the mechanism: " ) + r.score.verdict );
        lowest_a = std::min( lowest_a, r.score.parts.at( "tilt_active_pct" ) );
        highest_s = std::max( highest_s, r.score.parts.at( "tilt_silent_pct" ) );
      }
    std::printf( "leech_heart signature on the reference, 3 modes x 8 seeds: active phase +%.0f %% at the least, silent phase %+.0f %% at the most\n", lowest_a, highest_s );
    CHECK_MSG( lowest_a >= 15.0, "leech_heart: the active phase of the driven side lengthens by at least 15 percent on every seed (the pass limit is 12 percent at the most)" );
    CHECK_MSG( highest_s <= 0.0, "leech_heart: the silent phase of the driven side never lengthens" );
    // (2) quick = full: the same numbers (the quick episode is the full one)
    {
      const Result q = run_circuit( "leech_heart", ref, 4, "standard", true ), f = run_circuit( "leech_heart", ref, 4, "standard", false );
      CHECK_MSG( q.score.parts == f.score.parts, "leech_heart: the quick and the full episode give the same parts" );
    }
    // (3) other release circuits read the same way: a faster, harder-adapting pair; and the same pair read straight from the interneurons (no motor cells: the cells the test
    //     current goes to are found in the structure, not by name)
    for ( const char* mode : { "standard", "researcher" } )
      for ( uint64_t seed = 1; seed <= 3; ++seed )
      {
        const Result a = run_circuit( "leech_heart", set_hn( ref, { { "b", 10.0 }, { "tau_w", 500.0 } } ), seed, mode, false );
        CHECK_MSG( a.score.passed && a.score.parts.at( "signature" ) == 1.0, std::string( "leech_heart: a harder-adapting, faster pair passes as release (" ) + mode + " seed " + std::to_string( seed ) + "): " + a.score.verdict );
        json d = ref;
        json pops = json::array(), projs = json::array();
        for ( auto& p : d[ "populations" ] ) if ( p[ "id" ].get< std::string >().rfind( "HN", 0 ) == 0 ) pops.push_back( p );
        for ( auto& p : d[ "projections" ] ) if ( p[ "pre" ].get< std::string >().rfind( "HN", 0 ) == 0 && p[ "post" ].get< std::string >().rfind( "HN", 0 ) == 0 ) projs.push_back( p );
        d[ "populations" ] = pops;
        d[ "projections" ] = projs;
        for ( auto& i : d[ "inputs" ] ) i[ "targets" ] = json::array( { { { "pop", "HN_L" } }, { { "pop", "HN_R" } } } );
        d[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "HN_L" } } } );
        d[ "outputs" ][ 1 ][ "neurons" ] = json::array( { { { "pop", "HN_R" } } } );
        const Result b = run_circuit( "leech_heart", d, seed, mode, false );
        CHECK_MSG( b.score.passed && b.score.parts.at( "signature" ) == 1.0, std::string( "leech_heart: interneurons read directly pass as release (" ) + mode + " seed " + std::to_string( seed ) + "): " + b.score.verdict );
      }
    // (4) a novice who keeps the lab's own first-connect drive (300 pA per unit instead of 480): the pair fires together; the verdict says so and says nothing about escape or release
    {
      json c = ref;
      for ( auto& i : c[ "inputs" ] ) i[ "weight" ] = 300.0;
      const Result r = run_circuit( "leech_heart", c, 1, "standard", false );
      CHECK_MSG( !r.score.passed && r.score.verdict.find( "release" ) == std::string::npos && r.score.verdict.find( "escape" ) == std::string::npos, "leech_heart: a pair that fires together is not named as release or escape: " + r.score.verdict );
    }
    // (5) the test current changes the cells it should: the tilt cells are the ones that excite 'left' and inhibit 'right' (HN_R in the reference), so driving HN_R harder lengthens
    //     'left' (the motor side that HN_R's burst frees), not 'right': checked through the sign of the part with the two sides swapped in the circuit (the outputs exchanged)
    {
      json sw = ref;
      sw[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "HE_R" } } } );
      sw[ "outputs" ][ 1 ][ "neurons" ] = json::array( { { { "pop", "HE_L" } } } );
      const Result r = run_circuit( "leech_heart", sw, 2, "standard", false );
      CHECK_MSG( r.score.parts.count( "signature" ) && r.score.parts.at( "signature" ) == 1.0, "leech_heart: the signature reads the same with the two sides swapped: " + r.score.verdict );
    }
  }

  // ---- no numbers in verdicts or hint tiers 1-3 (honesty contract rule 2)
  // (extra circuits so that many verdicts are produced: the empty bench and one cell of each level)
  for ( const Level& L : kLevels )
  {
    auto t = make_task( L.id );
    json b = { { "schema_version", 1 }, { "name", "x" }, { "resolution_ms", 0.1 }, { "seed", 1 }, { "populations", json::array() }, { "projections", json::array() },
      { "inputs", json::array() }, { "outputs", json::array() }, { "record", { { "spikes", "all" } } } };
    b[ "populations" ].push_back( { { "id", "N" }, { "model", "iaf_psc_alpha" }, { "n", 1 }, { "params", json::object() } } );
    for ( auto& s : t->sensor_channels() ) b[ "inputs" ].push_back( { { "name", s }, { "kind", "poisson" }, { "targets", json::array( { { { "pop", "N" } } } ) }, { "weight", 1000.0 }, { "value", 0.0 } } );
    for ( auto& m : t->motor_channels() ) b[ "outputs" ].push_back( { { "name", m }, { "kind", "rate" }, { "neurons", json::array( { { { "pop", "N" } } } ) } } );
    run_circuit( L.id, b, 1, "standard", true );
  }
  int digits = 0;
  for ( const auto& s : g_text )
    if ( has_digit( s ) )
    {
      ++digits;
      CHECK_MSG( false, "a verdict or hint (tiers 1-3) carries a digit: " + s );
    }
  std::printf( "%zu distinct verdicts and hints scanned, %d with a digit\n", g_text.size(), digits );
  return chk::finish( "tasks_bestiary_a_test" );
}
