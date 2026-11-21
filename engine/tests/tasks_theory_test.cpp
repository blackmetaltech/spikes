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
const Level kLevels[] = { { "d_class", "dynamics", "micro" }, { "d_phasic", "dynamics", "micro" }, { "d_switch", "dynamics", "task" },
  { "r_oscillate", "rates", "micro" }, { "r_choose", "rates", "micro" }, { "r_meanfield", "rates", "task" },
  { "h_complete", "memory", "micro" }, { "h_two", "memory", "micro" }, { "h_learn", "memory", "task" },
  { "t_tuned", "tuning", "micro" }, { "t_sharpen", "tuning", "micro" }, { "t_decode", "tuning", "task" } };

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
      // t_sharpen is the named exception: the ratio of two small spike counts needs many trials to be steady across seeds (the CHECK runs headless, so only the live playback is longer)
      CHECK_MSG( t->duration_ms() <= ( std::string( L.id ) == "t_sharpen" ? 15000 : 5000 ), std::string( L.id ) + ": micro episode <= 5 s" );
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
    if ( std::string( L.kind ) == "micro" ) CHECK_MSG( cells <= ( std::string( L.id ) == "t_sharpen" ? 16 : 9 ), std::string( L.id ) + ": reference size" );
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
  // ---- the tuning playtest (pt05): honest circuits that must pass on every seed, cheap ones that must not
  {
    auto pt05 = [ & ]( const std::string& f ) { return json::parse( chk::slurp( std::string( SPIKE_SOURCE_DIR ) + "/engine/tests/circuits_pt05/" + f + ".json" ) ); };
    // t_tuned: a graded Hubel-Wiesel weighting (every afferent excitatory, the preferred ones far stronger) is a valid mechanism; so is flank inhibition, and its pass sentence says so
    for ( uint64_t seed = 1; seed <= 6; ++seed )
    {
      Result g = run_circuit( "t_tuned", pt05( "tuned_e_graded_all_exc" ), seed ), f = run_circuit( "t_tuned", pt05( "tuned_c_all_exc_flank_inh" ), seed );
      CHECK_MSG( g.score.passed, "t_tuned: graded weights fail on seed " + std::to_string( seed ) + ": " + g.score.verdict );
      CHECK_MSG( f.score.passed, "t_tuned: flank inhibition fails on seed " + std::to_string( seed ) + ": " + f.score.verdict );
      CHECK_MSG( f.score.verdict.find( "inhibit" ) != std::string::npos, "t_tuned: the pass sentence does not name the inhibition: " + f.score.verdict );
      CHECK_MSG( g.score.verdict.find( "inhibit" ) == std::string::npos, "t_tuned: the pass sentence names inhibition for an excitatory circuit: " + g.score.verdict );
    }
    // equal weights to every afferent: no preference, whatever the connection count
    Result eq = run_circuit( "t_tuned", pt05( "tuned_b_all_but_edges" ), 1 );
    CHECK_MSG( !eq.score.passed, "t_tuned: equal weights to all afferents pass" );
    // afferents that are not parrot cells never fire: the verdict says so
    Result sil = run_circuit( "t_tuned", pt05( "tuned_f_iaf_afferents" ), 1 );
    CHECK_MSG( !sil.score.passed && sil.score.verdict.find( "parrot" ) != std::string::npos, "t_tuned: silent afferents: " + sil.score.verdict );
    // t_sharpen: inhibition among the cells is the mechanism (self-excitation is optional); weak, strong and interneuron variants pass on every seed, no loop and a silencing loop do not
    for ( const char* f : { "sharpen_b_inhib_only", "sharpen_c_weak_inh", "sharpen_d_strong_inh", "sharpen_f_interneuron" } )
      for ( uint64_t seed = 1; seed <= 10; ++seed )
      {
        Result r = run_circuit( "t_sharpen", pt05( f ), seed );
        CHECK_MSG( r.score.passed, std::string( "t_sharpen: " ) + f + " fails on seed " + std::to_string( seed ) + ": " + r.score.verdict );
      }
    for ( const char* f : { "sharpen_a_no_loop", "sharpen_e_selfexc_big" } )
      for ( uint64_t seed = 1; seed <= 10; ++seed ) CHECK_MSG( !run_circuit( "t_sharpen", pt05( f ), seed ).score.passed, std::string( "t_sharpen: " ) + f + " passes on seed " + std::to_string( seed ) );
  }
  // the campaign order: a chapter's micro-puzzles come before its task
  CHECK( pos( "d_class" ) < pos( "d_phasic" ) && pos( "d_phasic" ) < pos( "d_switch" ) );
  CHECK( pos( "r_oscillate" ) < pos( "r_choose" ) && pos( "r_choose" ) < pos( "r_meanfield" ) );
  CHECK( pos( "t_tuned" ) < pos( "t_sharpen" ) && pos( "t_sharpen" ) < pos( "t_decode" ) );
  CHECK( pos( "h_complete" ) < pos( "h_two" ) && pos( "h_two" ) < pos( "h_learn" ) );

  // ---- playtest circuits (godot/circuits/tests): honest circuits that a novice builds must pass, cheap tricks must fail and the verdict must say what is wrong
  {
    struct Case { const char* task; const char* file; bool pass; const char* word; };
    const Case cases[] = {
      { "h_two", "h2_hopfield_ok.json", true, "" },          // an outer-product Hopfield wiring of 8 cells in ONE population (per-edge weights): judged on the edge list between the two cell sets
      { "h_complete", "hc_hopfield.json", true, "" },        // the same, one pattern
      { "h_complete", "hc_rand.json", false, "symmetric" },  // a directed random graph completes the pattern but is not symmetric
      { "r_choose", "ch_novice1.json", true, "" },
      { "r_choose", "ch_bias.json", false, "excite itself" },        // feed-forward gain bias, loops of 0.01 (token weights: the floor names the missing self-excitation first)
      { "r_oscillate", "osc_eq.json", true, "" },            // equal time constants oscillate: the inhibition need not be slower
      { "r_oscillate", "osc_b.json", false, "too strong" },  // the loop through the inhibitory unit pins the excitation just above the drive
      { "r_meanfield", "mf_k1_tuned.json", false, "network's k" },     // K = 1 with the J tuned to match the rates: not the network's description
    };
    for ( const Case& k : cases )
    {
      const json c = json::parse( chk::slurp( std::string( SPIKE_SOURCE_DIR ) + "/godot/circuits/tests/" + k.file ) );
      for ( uint64_t seed : { 1, 2 } )
      {
        Result r = run_circuit( k.task, c, seed, "standard", false );
        CHECK_MSG( r.loaded, std::string( k.file ) + " does not load: " + r.error );
        CHECK_MSG( r.score.passed == k.pass, std::string( k.task ) + " / " + k.file + " seed " + std::to_string( seed ) + ( k.pass ? ": an honest circuit fails: " : ": a cheap trick passes: " ) + r.score.verdict );
        if ( !k.pass && *k.word )
        {
          std::string v = r.score.verdict;
          for ( auto& ch : v ) ch = static_cast< char >( std::tolower( static_cast< unsigned char >( ch ) ) );
          CHECK_MSG( v.find( k.word ) != std::string::npos, std::string( k.file ) + ": the verdict does not name the problem (" + k.word + "): " + r.score.verdict );
        }
      }
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
    // an empty bench has built nothing: score 0 and no criterion met, however vacuous a criterion is at zero activity
    json empty = b;
    empty[ "populations" ] = json::array();
    empty[ "inputs" ] = json::array();
    empty[ "outputs" ] = json::array();
    const Result er = run_circuit( L.id, empty, 1, "standard", true );
    if ( er.loaded ) CHECK_MSG( er.score.value == 0.0 && er.score.parts.count( "criteria_met" ) && er.score.parts.at( "criteria_met" ) == 0.0, std::string( L.id ) + ": the empty bench scores " + std::to_string( er.score.value ) );
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
