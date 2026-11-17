// v0.3 contract of the 14 original tasks (docs/DESIGN-0.3.md, REQ-0.3.md item D): concepts, level kind, difficulty modes, challenge variants,
// hint ladder, metrics and notebook note. The structural checks are fast. The reference circuits are then run in guided and researcher
// mode (seed 1 for the tasks whose episode is <= 40 s; set SPIKE_FULL=1 to run every task on seeds 1-5 in both modes plus the noise and
// perturbation variants), and each must pass: structure never changes with the mode, only the bars and the noise.
//
//   tasks_modes_test                       the CTest
//   tasks_modes_test <task> <mode> [variant] [seeds=1]     one run, prints the score parts (tuning aid)
#include <cstdio>
#include <cstdlib>
#include <set>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

static const char* kTasks[] = { "reflex", "worm", "lamprey", "stretch_reflex", "renshaw", "posture", "reach", "heart_pacemaker",
                                "heart_autonomic", "baroreflex", "breathing", "pupil", "peristalsis", "fight_or_flight" };

static const std::set< std::string > kConcepts = { "threshold", "excitation", "convergence", "inhibition", "subtraction", "adaptation",
  "half_centre", "coupling", "pacemaking", "neuromodulation", "recruitment", "reciprocal_inhibition", "recurrent_inhibition", "negative_feedback",
  "feedforward_feedback", "timed_bursts", "rhythm_gating", "travelling_wave", "divergent_command" };

static std::string circuits_dir() { return std::string( SPIKE_SOURCE_DIR ) + "/circuits/"; }

static Score run_one( const std::string& id, const std::string& mode, const std::string& variant, uint64_t seed, std::unique_ptr< Task >* keep = nullptr )
{
  json c = json::parse( chk::slurp( circuits_dir() + reference_circuit( id ) ) );
  c[ "seed" ] = 1000 * seed + c.value( "seed", 1 );
  Engine e;
  if ( !e.load( c.dump() ) )
  {
    Score s;
    s.verdict = "load error: " + e.error();
    return s;
  }
  auto t = make_task( id );
  t->set_mode( mode );
  t->set_variant( variant );
  t->reset( seed );
  const long steps = static_cast< long >( t->duration_ms() );
  for ( long i = 0; i < steps; ++i )
    t->step( e, 1.0 );
  Score s = t->score();
  if ( keep ) *keep = std::move( t );
  return s;
}

int main( int argc, char** argv )
{
  if ( argc >= 3 )
  {
    std::string id = argv[ 1 ], mode = argv[ 2 ], variant = argc > 3 ? argv[ 3 ] : "";
    const std::string sl = argc > 4 ? argv[ 4 ] : "1";
    std::stringstream ss( sl );
    for ( std::string tok; std::getline( ss, tok, ',' ); )
    {
      Score s = run_one( id, mode, variant, std::stoull( tok ) );
      std::printf( "%s %s %s seed %s value %.1f %s |", id.c_str(), mode.c_str(), variant.c_str(), tok.c_str(), s.value, s.passed ? "PASS" : "fail" );
      for ( auto& [ k, v ] : s.parts ) std::printf( " %s=%.4g", k.c_str(), v );
      std::printf( "\n  %s\n", s.verdict.c_str() );
    }
    return 0;
  }
  const bool full = std::getenv( "SPIKE_FULL" ) != nullptr;

  // ---- structure ----
  for ( const char* idc : kTasks )
  {
    const std::string id = idc;
    auto t = make_task( id );
    CHECK_MSG( t != nullptr, id );
    if ( !t ) continue;
    CHECK_MSG( t->level_kind() == "task", id );
    CHECK_MSG( t->on_spine(), id );
    for ( auto& c : t->concepts_new() ) CHECK_MSG( kConcepts.count( c ), id + " concept " + c );
    for ( auto& c : t->concepts_used() ) CHECK_MSG( kConcepts.count( c ), id + " concept " + c );
    CHECK_MSG( !t->concepts_new().empty() || !t->concepts_used().empty(), id );
    std::set< std::string > both;
    for ( auto& c : t->concepts_new() ) both.insert( c );
    for ( auto& c : t->concepts_used() ) CHECK_MSG( !both.count( c ), id + " lists " + c + " as new and used" );
    const int par = t->par_neurons();
    CHECK_MSG( par > 0, id + " has no par" );
    auto v = t->variants();
    CHECK_MSG( v.size() >= 2, id + " needs 2-3 variants" );
    CHECK_MSG( v.size() <= 3, id + " has too many variants" );
    CHECK_MSG( v.count( "half_neurons" ), id + " lacks half_neurons" );
    for ( auto& [ k, d ] : v ) CHECK_MSG( !d.empty(), id + " variant " + k );
    t->set_variant( "half_neurons" );
    CHECK_MSG( t->par_neurons() == ( par + 1 ) / 2, id + " half_neurons par " + std::to_string( t->par_neurons() ) );
    t->set_variant( "" );
    CHECK_MSG( t->par_neurons() == par, id );
    for ( const char* m : { "guided", "standard", "researcher" } )
    {
      auto u = make_task( id );
      u->set_mode( m );
      CHECK_MSG( u->mode() == m, id );
      u->reset( 1 );
      auto h = u->hints();
      CHECK_MSG( h.size() == 3, id + " mode " + m + " hints " + std::to_string( h.size() ) );
      for ( size_t i = 0; i < h.size(); ++i )
      {
        CHECK_MSG( h[ i ].tier == static_cast< int >( i ) + 1, id );
        CHECK_MSG( !h[ i ].text.empty(), id );
      }
      if ( h.size() == 3 )
      {
        // tier 2 names a place: one of the task's channels or a population of the reference circuit
        json c = json::parse( chk::slurp( circuits_dir() + reference_circuit( id ) ) );
        std::set< std::string > ids;
        for ( auto& i : c[ "inputs" ] ) ids.insert( i[ "name" ].get< std::string >() );
        for ( auto& o : c[ "outputs" ] ) ids.insert( o[ "name" ].get< std::string >() );
        for ( auto& p : c[ "populations" ] ) ids.insert( p[ "id" ].get< std::string >() );
        for ( auto& s : u->sensor_channels() ) ids.insert( s );
        for ( auto& s : u->motor_channels() ) ids.insert( s );
        for ( int i = 0; i < 8; ++i ) ids.insert( "ach" + std::to_string( i ) ), ids.insert( "no" + std::to_string( i ) );
        CHECK_MSG( h[ 1 ].target.empty() || ids.count( h[ 1 ].target ), id + " tier-2 target '" + h[ 1 ].target + "' is not a channel or population" );
      }
    }
    auto n = t->notebook();
    CHECK_MSG( !n.title.empty() && n.text.size() > 40 && !n.citation.empty(), id + " notebook" );
    std::printf( "%s: %zu variants, par %d, new [%zu], used [%zu]\n", id.c_str(), v.size(), par, t->concepts_new().size(), t->concepts_used().size() );
  }

  // ---- the reference circuit passes in every mode; metrics and hints are live after a run ----
  const std::set< std::string > heavy = { "worm", "baroreflex", "breathing", "fight_or_flight", "heart_autonomic" };
  const std::set< std::string > noisy = { "reflex", "worm", "heart_pacemaker", "renshaw" };  // tasks with a noise variant
  for ( const char* idc : kTasks )
  {
    const std::string id = idc;
    if ( heavy.count( id ) && !full ) continue;
    const std::vector< uint64_t > seeds = full ? std::vector< uint64_t >{ 1, 2, 3, 4, 5 } : std::vector< uint64_t >{ 1 };
    for ( const char* mode : { "guided", "researcher" } )
      for ( uint64_t seed : seeds )
      {
        std::unique_ptr< Task > keep;
        Score s = run_one( id, mode, "", seed, &keep );
        std::printf( "  %-16s %-10s seed %llu value %5.1f %s\n", id.c_str(), mode, ( unsigned long long )seed, s.value, s.passed ? "PASS" : "fail" );
        CHECK_MSG( s.passed, id + " reference fails in " + mode + " mode: " + s.verdict );
        if ( std::string( mode ) == "researcher" && seed == 1 )
        {
          auto m = keep->metrics();
          CHECK_MSG( !m.empty(), id + " has no quality metric" );
          for ( auto& [ k, mt ] : m ) CHECK_MSG( std::isfinite( mt.value ) && !mt.unit.empty(), id + " metric " + k );
          CHECK_MSG( keep->hints().size() == 3, id + " hints after a run" );
        }
      }
  }
  if ( full )
    for ( const std::string& id : noisy )
    {
      auto t = make_task( id );
      if ( !t->variants().count( "noise_x2" ) ) continue;
      for ( uint64_t seed : { 1, 2 } )
      {
        Score s = run_one( id, "standard", "noise_x2", seed );
        std::printf( "  %-16s noise_x2   seed %llu value %5.1f %s\n", id.c_str(), ( unsigned long long )seed, s.value, s.passed ? "PASS" : "fail" );
        CHECK_MSG( s.passed, id + " reference fails noise_x2: " + s.verdict );
      }
    }
  return chk::finish( "tasks_modes_test" );
}
