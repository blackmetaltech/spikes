// Heart chapter: heart_pacemaker, heart_autonomic, baroreflex.
//   * the reference circuits pass on several seeds, are deterministic, respect the 60-neuron cap and declare the task's channels;
//   * every lesion (a weight scaled to zero, a cell parameter blocked) fails with the biologically expected failure, checked on the
//     score parts, not only on `passed`;
//   * an unwired circuit says so.
// Own file (not tasks_lesions.inc) so the three tasks can be tuned without touching the shared test.
#include <chrono>
#include <cstdio>
#include <map>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

static json load( const std::string& id )
{
  return json::parse( chk::slurp( std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + reference_circuit( id ) ) );
}

// weight of every projection pre -> post times `factor`
static void scale( json& c, const std::string& pre, const std::string& post, double factor )
{
  bool hit = false;
  for ( auto& p : c[ "projections" ] )
    if ( p[ "pre" ] == pre && p[ "post" ] == post )
    {
      hit = true;
      p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * factor;
    }
  CHECK_MSG( hit, "lesion " + pre + ">" + post + " matched no projection" );
}

// multiply a numeric cell parameter of one population (NESTML default if the circuit does not set it)
static void scale_param( json& c, const std::string& pop, const std::string& key, double base, double factor )
{
  for ( auto& p : c[ "populations" ] )
    if ( p[ "id" ] == pop )
    {
      double v = p[ "params" ].contains( key ) ? p[ "params" ][ key ].get< double >() : base;
      p[ "params" ][ key ] = v * factor;
    }
}

struct Run
{
  Score score;
  BodyState body;
  bool loaded = false;
  std::string error;
};

static Run run( const std::string& task_id, const json& circuit_in, uint64_t seed )
{
  Run r;
  json c = circuit_in;
  c[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );
  Engine e;
  r.loaded = e.load( c.dump() );
  if ( !r.loaded )
  {
    r.error = e.error();
    return r;
  }
  auto t = make_task( task_id );
  t->reset( seed );
  long steps = static_cast< long >( t->duration_ms() );
  for ( long s = 0; s < steps; ++s )
    t->step( e, 1.0 );
  r.score = t->score();
  r.body = t->body();
  r.error = e.error();
  return r;
}

static double part( const Run& r, const char* k )
{
  auto it = r.score.parts.find( k );
  return it == r.score.parts.end() ? NAN : it->second;
}

static void show( const char* what, uint64_t seed, const Run& r )
{
  std::printf( "  %-34s seed %llu value %5.1f %s |", what, static_cast< unsigned long long >( seed ), r.score.value, r.score.passed ? "PASS" : "fail" );
  for ( auto& [ k, v ] : r.score.parts )
    std::printf( " %s=%.3g", k.c_str(), v );
  std::printf( "\n    %s\n", r.score.verdict.c_str() );
}

static void reference( const std::string& id, std::vector< uint64_t > seeds )
{
  json c = load( id );
  auto t = make_task( id );
  CHECK_MSG( t != nullptr, id );
  CHECK( t->chapter() == "heart" );
  CHECK( t->view() == "heart" || t->view() == "dashboard" );
  CHECK( t->par_neurons() > 0 );
  CHECK( !t->timeline().empty() );
  CHECK( !t->bands().empty() );
  CHECK( !t->brief().empty() );
  std::map< std::string, bool > in_ch, out_ch;
  for ( auto& i : c[ "inputs" ] ) in_ch[ i[ "name" ] ] = true;
  for ( auto& o : c[ "outputs" ] ) out_ch[ o[ "name" ] ] = true;
  for ( auto& s : t->sensor_channels() ) CHECK_MSG( in_ch.count( s ), id + " input " + s );
  for ( auto& m : t->motor_channels() ) CHECK_MSG( out_ch.count( m ), id + " output " + m );
  int neurons = 0;
  for ( auto& p : c[ "populations" ] ) neurons += p[ "n" ].get< int >();
  CHECK_MSG( neurons <= 60, id + " has " + std::to_string( neurons ) + " neurons" );
  std::printf( "%s (%d neurons, par %d)\n", id.c_str(), neurons, t->par_neurons() );
  double sum = 0;
  for ( uint64_t seed : seeds )
  {
    Run r = run( id, c, seed );
    show( "reference", seed, r );
    CHECK_MSG( r.loaded, r.error );
    CHECK_MSG( r.score.passed, id + " seed " + std::to_string( seed ) + ": " + r.score.verdict );
    CHECK( r.score.value >= 50.0 );
    CHECK_MSG( r.error.empty(), r.error );
    sum += r.score.value;
    // HUD readouts the game relies on
    for ( const char* k : { "hr_bpm", "t_ms", "map_mmHg", "heart_phase" } )
      CHECK_MSG( r.body.readouts.count( k ), std::string( id ) + " readout " + k );
    for ( auto& b : t->bands() )
      CHECK_MSG( r.body.readouts.count( b.name ), id + " band " + b.name + " has no readout" );
    for ( auto& m : t->modulators() )
      CHECK_MSG( r.body.readouts.count( m ), id + " modulator readout " + m );
  }
  std::printf( "  mean score %.1f\n", sum / seeds.size() );
}

// a lesioned circuit fails on every seed; `why` checks the biologically expected signature on the score parts
template < class Check >
static void lesion( const std::string& id, const char* what, const json& c, std::vector< uint64_t > seeds, Check why )
{
  std::printf( "%s lesion: %s\n", id.c_str(), what );
  for ( uint64_t seed : seeds )
  {
    Run r = run( id, c, seed );
    show( "lesioned", seed, r );
    CHECK_MSG( r.loaded, r.error );
    CHECK_MSG( !r.score.passed, id + " still passes with lesion: " + what );
    CHECK_MSG( why( r ), std::string( "wrong failure for lesion: " ) + what + " -> " + r.score.verdict );
  }
}

int main()
{
  const std::vector< uint64_t > seeds = { 1, 2, 3, 4 }, lseeds = { 1, 2 };

  // ---- heart_pacemaker
  reference( "heart_pacemaker", seeds );
  {
    json base = load( "heart_pacemaker" );
    // determinism: same seed, same score
    Run a = run( "heart_pacemaker", base, 5 ), b = run( "heart_pacemaker", base, 5 );
    CHECK( a.score.value == b.score.value && part( a, "hr_bpm" ) == part( b, "hr_bpm" ) );
    // cluster: all cells fire together, regular, HRV tiny
    for ( uint64_t s : seeds )
    {
      Run r = run( "heart_pacemaker", base, s );
      CHECK( part( r, "cells" ) >= 2 );
      CHECK( part( r, "synchrony" ) > 0.6 );
      CHECK( part( r, "cv" ) < 0.02 );
      CHECK( part( r, "rmssd_ms" ) < 20 );
    }
    json l1 = base;
    scale( l1, "SA_c", "SA_p", 0 );
    scale( l1, "SA_p", "SA_c", 0 );
    lesion( "heart_pacemaker", "gap-junction coupling removed: the cells beat at their own rates, the fastest-to-fire wins (tachycardia, irregular)",
      l1, lseeds, []( const Run& r ) { return part( r, "synchrony" ) < 0.8 && ( part( r, "hr_bpm" ) > 80 || part( r, "cv" ) > 0.05 ); } );
    json l2 = base;  // L-type Ca channel block (nifedipine-like): the upstroke fails
    scale_param( l2, "SA_c", "g_CaL", 3.53, 0.55 );
    scale_param( l2, "SA_p", "g_CaL", 3.53, 0.55 );
    lesion( "heart_pacemaker", "I_CaL blocked by 45 percent: no upstroke, asystole or slow rhythm", l2, lseeds,
      []( const Run& r ) { return part( r, "hr_bpm" ) < 60 || part( r, "beats" ) < 12; } );
    json l3 = base;  // low-threshold (T-type, Cav3.1) Ca channel block: slower diastolic depolarisation
    scale_param( l3, "SA_c", "g_CaT", 0.78, 0.0 );
    scale_param( l3, "SA_p", "g_CaT", 0.78, 0.0 );
    lesion( "heart_pacemaker", "I_CaT blocked (Cav3.1 knockout-like): bradycardia", l3, lseeds,
      []( const Run& r ) { return part( r, "hr_bpm" ) < 60; } );
    json l4 = base;  // tonic ACh (carbachol-like): vagal arrest, pauses
    for ( auto& p : l4[ "populations" ] ) p[ "params" ][ "ach" ] = 0.8;
    lesion( "heart_pacemaker", "tonic ACh 0.8 (muscarinic agonist): the node pauses, far too few beats", l4, lseeds,
      []( const Run& r ) { return part( r, "beats" ) < 12; } );
    json l5 = base;  // tonic NE (isoprenaline-like): tachycardia
    for ( auto& p : l5[ "populations" ] ) p[ "params" ][ "ne" ] = 1.0;
    lesion( "heart_pacemaker", "tonic NE 1.0 (beta agonist): sinus tachycardia above 100/min", l5, lseeds,
      []( const Run& r ) { return part( r, "hr_bpm" ) > 100; } );
    // a single cell is a sinus rhythm, but it cannot earn the cluster bonus
    json one = base;
    one[ "populations" ] = json::array( { base[ "populations" ][ 0 ] } );
    one[ "populations" ][ 0 ][ "n" ] = 1;
    one[ "populations" ][ 0 ][ "params" ][ "I_e" ] = 0.0;
    one[ "populations" ][ 0 ][ "pos" ] = json::array( { json::array( { 120, 200 } ) } );
    one[ "projections" ] = json::array();
    one[ "modulation" ] = json::array();
    one[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "SA_c" } } } );
    Run r1 = run( "heart_pacemaker", one, 1 );
    show( "single cell", 1, r1 );
    CHECK( r1.score.passed );
    CHECK( r1.score.value <= 85.1 );
    CHECK( part( r1, "synchrony" ) == 0 );
    // an unwired output is reported, not scored as a mystery zero
    json bad = base;
    bad[ "outputs" ][ 0 ][ "neurons" ] = json::array();
    Run rb = run( "heart_pacemaker", bad, 1 );
    CHECK( !rb.score.passed );
    CHECK( rb.score.verdict.find( "not connected" ) != std::string::npos );
  }

  // ---- heart_autonomic
  reference( "heart_autonomic", seeds );
  {
    json base = load( "heart_autonomic" );
    Run a = run( "heart_autonomic", base, 5 ), b = run( "heart_autonomic", base, 5 );
    CHECK( a.score.value == b.score.value && part( a, "mae_bpm" ) == part( b, "mae_bpm" ) );
    CHECK( part( a, "ach_rest" ) > 0.15 && part( a, "ach_rest" ) < 0.9 );  // vagal tone at rest, below the arrest range
    json l1 = base;
    scale( l1, "NA", "VG", 0 );
    lesion( "heart_autonomic", "vagus cut (cardiac preganglionic -> ganglion): resting tachycardia, no vagal tone", l1, lseeds,
      []( const Run& r ) { return part( r, "rest_hr" ) > 66 && part( r, "ach_rest" ) < 0.01; } );
    json l2 = base;
    scale( l2, "IML", "SG", 0 );
    lesion( "heart_autonomic", "sympathetic chain cut: the heart cannot speed up beyond vagal withdrawal", l2, lseeds,
      []( const Run& r ) { return part( r, "exercise_hr" ) < 92 && part( r, "ne_peak" ) < 0.01; } );
    json l3 = base;
    scale( l3, "CC", "IML", 0 );
    scale( l3, "LOW", "IML", 0 );
    lesion( "heart_autonomic", "no cue or error input to the sympathetic preganglionic cells: exercise rate not reached", l3, lseeds,
      []( const Run& r ) { return part( r, "exercise_hr" ) < 92; } );
    json l4 = base;
    scale( l4, "NA", "VG", 3 );
    lesion( "heart_autonomic", "vagal ganglion transmission x3: ACh near the arrest range, pauses and bradycardia", l4, lseeds,
      []( const Run& r ) { return part( r, "mae_bpm" ) > 6 || part( r, "recovery_hr" ) < 58 || part( r, "rest_hr" ) < 58; } );
    json bad = base;
    bad[ "outputs" ][ 2 ][ "neurons" ] = json::array();
    Run rb = run( "heart_autonomic", bad, 1 );
    CHECK( !rb.score.passed );
    CHECK( rb.score.verdict.find( "not connected" ) != std::string::npos );
  }

  // ---- baroreflex
  reference( "baroreflex", seeds );
  {
    json base = load( "baroreflex" );
    Run a = run( "baroreflex", base, 5 ), b = run( "baroreflex", base, 5 );
    CHECK( a.score.value == b.score.value && part( a, "map_min_mmHg" ) == part( b, "map_min_mmHg" ) );
    json l1 = base;
    scale( l1, "AFF", "NTS", 0 );
    lesion( "baroreflex", "baroreceptors denervated (afferent -> NTS cut): CVLM silent, RVLM unopposed, pressure runs high and unregulated", l1, lseeds,
      []( const Run& r ) { return part( r, "map_max_mmHg" ) > 110; } );
    json l2 = base;
    scale( l2, "RVLM", "IML", 0 );
    lesion( "baroreflex", "RVLM -> sympathetic preganglionic cut: no sympathetic tone, hypotension that worsens on standing", l2, lseeds,
      []( const Run& r ) { return part( r, "map_min_mmHg" ) < 70 && part( r, "in_band" ) < 0.5; } );
    json l3 = base;
    scale( l3, "NTS", "CVLM", 0 );
    lesion( "baroreflex", "NTS -> CVLM cut: RVLM never inhibited (sympathetic maximal)", l3, lseeds,
      []( const Run& r ) { return part( r, "map_max_mmHg" ) > 110; } );
    json l4 = base;
    scale( l4, "NTS", "CVLM", 0.5 );
    lesion( "baroreflex", "reflex gain halved (NTS -> CVLM x0.5, as in a stiff, hypertensive baroreflex): regulated too high", l4, lseeds,
      []( const Run& r ) { return part( r, "map_max_mmHg" ) > 100; } );
    json l5 = base;
    scale( l5, "IML", "SG", 0.0 );
    lesion( "baroreflex", "sympathetic ganglion cut (preganglionic -> postganglionic): same as sympathectomy", l5, lseeds,
      []( const Run& r ) { return part( r, "map_min_mmHg" ) < 70; } );
    // the vagal limb alone is not enough but not needed for pressure: its loss changes heart rate, not the verdict
    json v = base;
    scale( v, "NA", "VG", 0 );
    Run rv = run( "baroreflex", v, 1 );
    show( "vagal limb cut (still regulates)", 1, rv );
    CHECK( part( rv, "hr_mean_bpm" ) >= part( run( "baroreflex", base, 1 ), "hr_mean_bpm" ) - 1.0 );
    json bad = base;
    bad[ "outputs" ][ 2 ][ "neurons" ] = json::array();
    Run rb = run( "baroreflex", bad, 1 );
    CHECK( !rb.score.passed );
    CHECK( rb.score.verdict.find( "not connected" ) != std::string::npos );
  }
  return chk::finish( "tasks_heart_test" );
}
