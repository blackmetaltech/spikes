// The Theory wing of 0.6 (docs/DESIGN-THEORY.md): dynamics, rates, memory and tuning, three levels per chapter (two micro-puzzles and a task).
//
//   tasks_theory_test                        the CTest
//   tasks_theory_test run <task> <circuit.json> [seeds=1,2,3] [mode=standard] [quick]   tuning aid: score one circuit, print parts and verdict
//
// Per level: the v0.3 contract (level kind, chapter, concepts that exist in the game's glossary, three hint tiers, a notebook entry with a DOI, a default model, a recipe, a quick episode no
// longer than the full one), the honesty contract (no digit in any verdict or hint of tiers 1-3: numbers live in recipe() only), the reference circuit passes on 5 seeds in guided, standard and
// researcher mode and is deterministic, the quick and the full episode agree, the CHECK (quick episode) time is printed. The negative tests (cheap circuits that must fail) are in
// cheap_rows_f.inc (tasks_cheap_test_f).
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
const Level kLevels[] = { { "d_class", "dynamics", "micro" }, { "d_phasic", "dynamics", "micro" }, { "d_switch", "dynamics", "task" } };

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

void contract( const Level& L, const std::set< std::string >& gloss )
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
    contract( L, gloss );
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
  // d_class scales its staircase to the cell: the sensor weight cannot make a slowly-starting cell look abrupt, nor hide an abrupt one
  for ( double f : { 0.1, 30.0 } )
  {
    json c = load_circuit( "d_class" );
    c[ "inputs" ][ 0 ][ "weight" ] = c[ "inputs" ][ 0 ][ "weight" ].get< double >() * f;
    Result r = run_circuit( "d_class", c, 1, "standard", true );
    CHECK_MSG( r.score.passed, "d_class with the sensor weight x" + std::to_string( f ) + " fails: " + r.score.verdict );
    c[ "populations" ][ 0 ][ "params" ] = { { "a", 0.02 }, { "b", 0.2 }, { "c", -65.0 }, { "d", 8.0 } };
    r = run_circuit( "d_class", c, 1, "standard", true );
    CHECK_MSG( !r.score.passed, "d_class: a class I cell passes with the sensor weight x" + std::to_string( f ) );
  }
  // the campaign order: a chapter's micro-puzzles come before its task
  CHECK( pos( "d_class" ) < pos( "d_phasic" ) && pos( "d_phasic" ) < pos( "d_switch" ) );

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
  return chk::finish( "tasks_theory_test" );
}
