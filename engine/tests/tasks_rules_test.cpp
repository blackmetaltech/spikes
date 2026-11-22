// Rules that hold across every level, checked from the data (no episode is run, so this is fast):
//   par         par_neurons() == the neurons of the level's reference circuit + par_slack() (slack declared, never negative), the challenge text names the par and,
//               when there is slack, says how many cells the reference has; a "half_neurons" variant halves the par rounded UP (the lab asks the engine for it)
//   parents     every level that is not on the spine declares the spine level of its chapter it hangs off (Task::parent); a spine level declares none
//   parts bin   every model that a reference circuit uses has a card in the parts bin (id == model), and the chapter that opens that card is no later than the level's
//               own (godot/game/catalog.gd UNLOCK, godot/game/campaign.gd CHAPTERS): a player can build what the reference builds
//   briefs      the briefs of the levels that used to carry a wiring recipe keep the goal and the idea (the wiring is in the hint ladder and the recipe)
//   stimulus    an input the level does not declare (what + Noise used to add on a level) keeps the run from passing, holds the score under the pass mark and the
//               verdict says why, in words (no digits)
//   limits      the neuron limit the game falls back on (godot/game/circuits.gd FALLBACK_MAX_NEURONS) and the Python package's (python/spike/circuit.py MAX_NEURONS)
//               equal the engine's max_circuit_neurons()
#include <algorithm>
#include <cstdio>
#include <map>
#include <regex>
#include <set>
#include <string>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/catalog.h"
#include "spike/engine.h"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

static const std::string kSrc = SPIKE_SOURCE_DIR;

static json reference_of( const std::string& id )
{
  const std::string ref = reference_circuit( id );
  return ref.empty() ? json() : json::parse( chk::slurp( kSrc + "/circuits/" + ref ) );
}

static int neurons_of( const json& c )
{
  int n = 0;
  for ( const auto& p : c[ "populations" ] )
    n += p.value( "n", 1 );
  return n;
}

static void test_par()
{
  for ( const std::string& id : task_ids() )
  {
    auto t = make_task( id );
    if ( t->level_kind() == "diagnosis" )
      continue;  // a case has no neuron budget: the patient's circuit is given
    const json c = reference_of( id );
    const int par = t->par_neurons(), slack = t->par_slack(), ref = neurons_of( c );
    CHECK_MSG( slack >= 0, id + ": negative slack" );
    CHECK_MSG( par == ref + slack, id + ": par " + std::to_string( par ) + " must be the reference's " + std::to_string( ref ) + " cells + the declared slack " + std::to_string( slack ) );
    const auto ch = t->challenge();
    CHECK_MSG( ch.max_neurons == par, id + ": the challenge budget is the par" );
    CHECK_MSG( ch.text.find( par == 1 ? "single neuron" : std::to_string( par ) ) != std::string::npos, id + ": the challenge text names the par: " + ch.text );
    if ( slack > 0 )
      CHECK_MSG( ch.text.find( "reference circuit has " + std::to_string( ref ) ) != std::string::npos, id + ": the slack is stated: " + ch.text );
    else
      CHECK_MSG( ch.text.find( "reference" ) == std::string::npos, id + ": no slack, nothing to state: " + ch.text );
    if ( t->variants().count( "half_neurons" ) )
    {
      t->set_variant( "half_neurons" );
      CHECK_MSG( t->par_neurons() == ( par + 1 ) / 2, id + ": half_neurons halves the par rounded up (" + std::to_string( par ) + " -> " + std::to_string( t->par_neurons() ) + ")" );
    }
  }
  auto hp = make_task( "heart_pacemaker" );  // an odd par (7): the rounding is visible
  hp->set_variant( "half_neurons" );
  CHECK( hp->par_neurons() == 4 );
}

static void test_parents()
{
  std::map< std::string, std::pair< bool, std::string > > spine;  // id -> {on spine, chapter}
  for ( const std::string& id : task_ids() )
  {
    auto t = make_task( id );
    spine[ id ] = { t->on_spine() and t->level_kind() != "diagnosis", t->chapter() };
  }
  for ( const std::string& id : task_ids() )
  {
    auto t = make_task( id );
    const std::string p = t->parent();
    if ( spine[ id ].first )
    {
      CHECK_MSG( p.empty(), id + ": a spine level has no parent" );
      continue;
    }
    CHECK_MSG( not p.empty(), id + ": a level off the spine must declare the spine level it hangs off" );
    CHECK_MSG( spine.count( p ) and spine[ p ].first, id + ": parent " + p + " is not a spine level" );
    CHECK_MSG( spine.count( p ) and spine[ p ].second == t->chapter(), id + ": parent " + p + " is in another chapter" );
  }
  CHECK( make_task( "posture" )->parent() == "stretch_reflex" and make_task( "reach" )->parent() == "stretch_reflex" );
  CHECK( make_task( "dx_clonus" )->parent() == "renshaw" );
}

/// The `"id": "chapter"` pairs of the UNLOCK constant of godot/game/catalog.gd.
static std::map< std::string, std::string > unlock_table()
{
  const std::string s = chk::slurp( kSrc + "/godot/game/catalog.gd" );
  const size_t a = s.find( "const UNLOCK := {" ), b = s.find( '}', a );
  std::map< std::string, std::string > m;
  const std::string body = s.substr( a, b - a );
  const std::regex pair_re( "\"([A-Za-z0-9_]+)\": \"([a-z_]+)\"" );
  for ( std::sregex_iterator it( body.begin(), body.end(), pair_re ), end; it != end; ++it )
    m[ ( *it )[ 1 ] ] = ( *it )[ 2 ];
  return m;
}

/// Chapter ids of godot/game/campaign.gd CHAPTERS in order, with the wing ("" for the body chapters).
static std::vector< std::pair< std::string, bool > > chapter_table()
{
  const std::string s = chk::slurp( kSrc + "/godot/game/campaign.gd" );
  const size_t a = s.find( "const CHAPTERS := [" ), b = s.find( "## Planned lineup", a );
  const std::string body = s.substr( a, b - a );
  std::vector< std::pair< std::string, bool > > out;
  std::regex re( "\\{\"id\": \"([a-z_]+)\"[^\\n]*" );
  for ( std::sregex_iterator it( body.begin(), body.end(), re ), end; it != end; ++it )
    out.push_back( { ( *it )[ 1 ], ( *it ).str().find( "\"wing\"" ) != std::string::npos } );
  return out;
}

static void test_parts_bin()
{
  const auto unlock = unlock_table();
  const auto chapters = chapter_table();
  CHECK( chapters.size() >= 15 and unlock.size() > 20 );
  auto index = [ & ]( const std::string& ch ) { for ( size_t i = 0; i < chapters.size(); ++i ) if ( chapters[ i ].first == ch ) return int( i ); return -1; };
  auto wing = [ & ]( const std::string& ch ) { return index( ch ) >= 0 and chapters[ index( ch ) ].second; };
  for ( const std::string& id : task_ids() )
  {
    const json c = reference_of( id );
    const std::string lc = make_task( id )->chapter();
    CHECK_MSG( index( lc ) >= 0, id + ": unknown chapter " + lc );
    std::set< std::string > models;
    for ( const auto& p : c[ "populations" ] )
      models.insert( p[ "model" ].get< std::string >() );
    for ( const std::string& m : models )
    {
      const CatalogModel* card = catalog_find( m );
      CHECK_MSG( card != nullptr and card->model == m, id + ": its reference uses " + m + ", which has no parts-bin card" );
      const std::string cc = unlock.count( m ) ? unlock.at( m ) : "invertebrates";   // catalog.gd: an unlisted card belongs to the first chapter
      // available on the level: the same chapter, the first chapter, or an earlier body chapter (a Bestiary or Theory chapter opens after the first chapter only)
      const bool ok = cc == lc or cc == "invertebrates" or ( not wing( lc ) and not wing( cc ) and index( cc ) >= 0 and index( cc ) <= index( lc ) );
      CHECK_MSG( ok, id + " (chapter " + lc + "): the card " + m + " only opens with chapter " + cc );
    }
  }
}

static void test_limits()
{
  const long engine = max_circuit_neurons();
  CHECK( engine == 20000 );
  auto number_after = [ & ]( const std::string& file, const std::string& key ) {
    const std::string s = chk::slurp( kSrc + file );
    std::smatch m;
    return std::regex_search( s, m, std::regex( key + "\\s*(?::=|=)\\s*([0-9_]+)" ) ) ? std::stol( m[ 1 ].str() ) : -1L;
  };
  CHECK_MSG( number_after( "/godot/game/circuits.gd", "FALLBACK_MAX_NEURONS" ) == engine, "the game's fallback neuron limit differs from the engine's" );
  CHECK_MSG( number_after( "/python/spike/circuit.py", "MAX_NEURONS" ) == engine, "the Python package's neuron limit differs from the engine's" );
}

static void test_briefs()
{
  for ( const char* id : { "worm", "peristalsis", "m_wave", "m_reciprocal", "m_timed_bursts", "m_recurrent", "m_feedforward" } )
  {
    const std::string b = make_task( id )->brief();
    for ( const char* leak : { "Wire ", "wire ", "Build a chain", "second route", "set to inhibitory", "Start simple", "to excite turn_left", "to inhibit it", "descending chain", "Feedback: ", "Feed-forward: " } )
      CHECK_MSG( b.find( leak ) == std::string::npos, std::string( id ) + ": the brief gives wiring away (\"" + leak + "\")" );
    CHECK_MSG( not make_task( id )->recipe().empty(), std::string( id ) + ": the wiring lives in the recipe" );
  }
}

static void test_own_stimulus()
{
  json c = reference_of( "reflex" );
  c[ "inputs" ].push_back( { { "name", "noise1" }, { "kind", "poisson" }, { "targets", json::array( { { { "pop", "SN" } } } ) }, { "weight", 40.0 }, { "value", 30.0 } } );
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  auto t = make_task( "reflex" );
  t->reset( 1 );
  for ( int i = 0; i < 300; ++i )
    t->step( e, 1.0 );
  const Score sc = t->score();
  CHECK( not sc.passed and sc.value <= 49.0 );
  CHECK( sc.parts.count( "circuit_rejected" ) and sc.parts.at( "circuit_rejected" ) == 1.0 );
  CHECK_MSG( sc.verdict.find( "stimulus of its own" ) != std::string::npos and sc.verdict.find( "cannot pass" ) != std::string::npos and sc.verdict.find( "sandbox" ) != std::string::npos, sc.verdict );
  CHECK_MSG( std::none_of( sc.verdict.begin(), sc.verdict.end(), []( char ch ) { return ch >= '0' and ch <= '9'; } ), "no digits in a verdict: " + sc.verdict );
}

int main()
{
  test_briefs();
  test_own_stimulus();
  test_par();
  test_parents();
  test_parts_bin();
  test_limits();
  return chk::finish( "tasks_rules_test" );
}
