// Heart chapter: heart_pacemaker, heart_autonomic, baroreflex.
//   * the reference circuits pass on several seeds, are deterministic, respect the 60-neuron cap and declare the task's channels;
//   * every lesion (a weight scaled to zero, a cell parameter blocked) fails with the biologically expected failure, checked on the
//     score parts, not only on `passed`;
//   * an unwired circuit says so.
// Own file (not tasks_lesions.inc) so the three tasks can be tuned without touching the shared test.
#include <cctype>
#include <chrono>
#include <cstdio>
#include <map>
#include <regex>
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
  std::vector< Task::Hint > hints;
  BodyState body;
  bool loaded = false;
  std::string error;
};

static Run run( const std::string& task_id, const json& circuit_in, uint64_t seed, bool quick = false )
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
  t->set_quick( quick );
  t->reset( seed );
  long steps = static_cast< long >( quick ? t->quick_duration_ms() : t->duration_ms() );
  for ( long s = 0; s < steps; ++s )
    t->step( e, 1.0 );
  r.score = t->score();
  r.hints = t->hints();
  r.body = t->body();
  r.error = e.error();
  return r;
}

static double part( const Run& r, const char* k )
{
  auto it = r.score.parts.find( k );
  return it == r.score.parts.end() ? NAN : it->second;
}

// Honesty contract rule 2: verdicts and tier 1-3 hints carry no parameter numbers (a weight, a bias, a gain, a unit of current or conductance); only recipe() does.
// (Observed behaviour such as "62 beats/min" and the pass band the brief states are not parameters.)
static bool has_parameter_number( const std::string& t )
{
  static const std::regex re( R"((-?[0-9]+(\.[0-9]+)?\s*(pA|nS|pF|ms delay))|((bias|weight|gain|I_e|g_[A-Za-z]+|tau)\s*(of|at|=)?\s*-?[0-9]))", std::regex::icase );
  return std::regex_search( t, re );
}
static void no_numbers( const std::string& what, const Run& r )
{
  CHECK_MSG( !has_parameter_number( r.score.verdict ), what + ": the verdict carries a parameter number: " + r.score.verdict );
  CHECK_MSG( !r.hints.empty(), what + ": no hints" );
  for ( const auto& h : r.hints )
    CHECK_MSG( !has_parameter_number( h.text ), what + ": tier " + std::to_string( h.tier ) + " hint carries a parameter number: " + h.text );
}
static bool contains( const std::string& hay, const std::string& needle )
{
  std::string a = hay, b = needle;
  for ( auto& c : a ) c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
  for ( auto& c : b ) c = static_cast< char >( std::tolower( static_cast< unsigned char >( c ) ) );
  return a.find( b ) != std::string::npos;
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
  // the quick check (the game's CHECK) is a shorter episode, scored by the same rules: the reference passes it too, with the same mechanism verdict
  CHECK_MSG( t->quick_duration_ms() <= t->duration_ms(), id + " quick episode longer than the full one" );
  for ( uint64_t seed : { uint64_t( 1 ), uint64_t( 2 ), uint64_t( 3 ) } )
  {
    Run q = run( id, c, seed, true );
    show( "reference, quick check", seed, q );
    CHECK_MSG( q.score.passed, id + " quick check, seed " + std::to_string( seed ) + ": " + q.score.verdict );
    no_numbers( id + " quick reference", q );
  }
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
    no_numbers( id + " lesion: " + what, r );
    CHECK_MSG( why( r ), std::string( "wrong failure for lesion: " ) + what + " -> " + r.score.verdict );
  }
}

int main()
{
  const std::vector< uint64_t > seeds = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 }, lseeds = { 1, 2 };

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
    scale_param( l2, "SA_c", "g_CaL", 3.53, 0.4 );
    scale_param( l2, "SA_p", "g_CaL", 3.53, 0.4 );
    lesion( "heart_pacemaker", "I_CaL blocked by 60 percent: no upstroke, asystole or slow rhythm", l2, lseeds,
      []( const Run& r ) { return part( r, "hr_bpm" ) < 60 || part( r, "beats" ) < 12; } );
    json l3 = base;  // low-threshold (T-type, Cav3.1) Ca channel block: slower diastolic depolarisation
    scale_param( l3, "SA_c", "g_CaT", 1.9, 0.7 );
    scale_param( l3, "SA_p", "g_CaT", 1.9, 0.7 );
    lesion( "heart_pacemaker", "I_CaT blocked by 30 percent (Cav3.1-like): bradycardia", l3, lseeds,
      []( const Run& r ) { return part( r, "hr_bpm" ) < 60; } );
    json l3b = base;  // I_f block (ivabradine-like): slower diastolic depolarisation
    scale_param( l3b, "SA_c", "g_f", 0.34, 0.0 );
    scale_param( l3b, "SA_p", "g_f", 0.34, 0.0 );
    lesion( "heart_pacemaker", "I_f blocked (ivabradine-like): bradycardia", l3b, lseeds, []( const Run& r ) { return part( r, "hr_bpm" ) < 60; } );
    json l4 = base;  // tonic ACh (carbachol-like): on top of the resting vagal tone the task applies
    for ( auto& p : l4[ "populations" ] ) p[ "params" ][ "ach" ] = 1.5;
    lesion( "heart_pacemaker", "extra tonic ACh 1.5 (muscarinic agonist): vagal overdrive, bradycardia", l4, lseeds,
      []( const Run& r ) { return part( r, "hr_bpm" ) < 60 || part( r, "beats" ) < 12; } );
    json l5 = base;  // tonic NE (isoprenaline-like): tachycardia
    for ( auto& p : l5[ "populations" ] ) p[ "params" ][ "ne" ] = 1.0;
    lesion( "heart_pacemaker", "tonic NE 1.0 (beta agonist): sinus tachycardia above 100/min", l5, lseeds,
      []( const Run& r ) { return part( r, "hr_bpm" ) > 100; } );
    // five uncoupled IDENTICAL clones (S1 of the 0.2 critique): the task gives every cell its own size, so they drift apart:
    // a fast, irregular merged rhythm and low synchrony, never a pass and never the old 100
    json clones = base;
    clones[ "populations" ] = json::array( { base[ "populations" ][ 0 ] } );
    clones[ "populations" ][ 0 ][ "n" ] = 5;
    clones[ "populations" ][ 0 ][ "params" ][ "I_e" ] = 0.0;
    clones[ "populations" ][ 0 ][ "pos" ] = json::array( { json::array( { 100, 100 } ), json::array( { 130, 100 } ), json::array( { 100, 130 } ),
                                                            json::array( { 130, 130 } ), json::array( { 160, 100 } ) } );
    clones[ "projections" ] = json::array();
    clones[ "modulation" ] = json::array();
    clones[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "SA_c" } } } );
    lesion( "heart_pacemaker", "five uncoupled identical clones: the cells beat at their own rates, tachycardia and irregular", clones, seeds,
      []( const Run& r ) { return part( r, "synchrony" ) < 0.6 && ( part( r, "hr_bpm" ) > 80 || part( r, "cv" ) > 0.05 ) && r.score.value < 30; } );
    // a single cell beats like a heart but is not a node (REVIEW-HARSH S1: it used to pass 5/5): it cannot pass and scores below the pass bar,
    // and the verdict says there is no cluster
    json one = base;
    one[ "populations" ] = json::array( { base[ "populations" ][ 0 ] } );
    one[ "populations" ][ 0 ][ "n" ] = 1;
    one[ "populations" ][ 0 ][ "params" ][ "I_e" ] = 0.0;  // the task still gives it its own size
    one[ "populations" ][ 0 ][ "pos" ] = json::array( { json::array( { 120, 200 } ) } );
    one[ "projections" ] = json::array();
    one[ "modulation" ] = json::array();
    one[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "SA_c" } } } );
    Run r1 = run( "heart_pacemaker", one, 1 );
    show( "single cell", 1, r1 );
    CHECK( !r1.score.passed );
    CHECK( r1.score.value < 50.0 );
    CHECK( part( r1, "synchrony" ) == 0 );
    CHECK( contains( r1.score.verdict, "no cluster" ) );
    no_numbers( "heart_pacemaker single cell", r1 );
    // the same single cell wired to itself is still one cell; two groups wired by inhibition are not coupled the way the level teaches
    json lone = one;
    lone[ "projections" ] = json::array( { { { "pre", "SA_c" }, { "post", "SA_c" }, { "rule", "all_to_all" },
      { "synapse", { { "synapse_model", "static_synapse" }, { "weight", 6.0 }, { "delay", 1.0 } } } } } );
    Run rl = run( "heart_pacemaker", lone, 1 );
    CHECK( !rl.score.passed && contains( rl.score.verdict, "no cluster" ) );
    json inh = base;
    scale( inh, "SA_c", "SA_p", -1 );
    scale( inh, "SA_p", "SA_c", -1 );
    Run ri = run( "heart_pacemaker", inh, 1 );
    show( "inhibitory coupling", 1, ri );
    CHECK( !ri.score.passed && contains( ri.score.verdict, "coupled" ) && ri.score.value < 50.0 );
    // the mechanism verdict names the missing coupling and only that: cutting one direction of the coupling keeps the cluster (one cell drives the other)
    json half = base;
    scale( half, "SA_p", "SA_c", 0 );
    Run rh = run( "heart_pacemaker", half, 1 );
    show( "one direction of the coupling cut", 1, rh );
    CHECK( !contains( rh.score.verdict, "no cluster" ) );
    // a scripted novice that reads only the brief: SA-node cells from the parts bin (catalogue defaults), a group coupled to itself with excitatory
    // synapses, read out as sa_beat; it does not know the recipe's weight, so it searches a coarse grid with a fixed budget and keeps the first that passes
    {
      bool solved = false;
      for ( double w : { 0.02, 0.1, 0.5, 2.5, 12.0 } )
      {
        json nv = base;
        nv[ "populations" ] = json::array( { { { "id", "N" }, { "model", "sa_node_pacemaker" }, { "n", 4 }, { "params", json::object() },
                                               { "pos", json::array( { json::array( { 10, 10 } ), json::array( { 40, 10 } ), json::array( { 10, 40 } ), json::array( { 40, 40 } ) } ) },
                                               { "role", "neuron" } } } );
        nv[ "projections" ] = json::array( { { { "pre", "N" }, { "post", "N" }, { "rule", "all_to_all" },
          { "synapse", { { "synapse_model", "static_synapse" }, { "weight", w }, { "delay", 1.0 } } } } } );
        nv[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "N" } } } );
        nv[ "modulation" ] = json::array();
        Run rn = run( "heart_pacemaker", nv, 1, true );
        std::printf( "  novice, coupling weight %.2f: %s %.1f | %s\n", w, rn.score.passed ? "PASS" : "fail", rn.score.value, rn.score.verdict.c_str() );
        solved = solved || rn.score.passed;
        CHECK_MSG( rn.loaded, rn.error );
        no_numbers( "heart_pacemaker novice", rn );
      }
      CHECK_MSG( solved, "heart_pacemaker: the brief-only novice (grid of 5 coupling weights) cannot solve the level" );
    }
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
      []( const Run& r ) { return part( r, "exercise_hr" ) < 105 && part( r, "ne_peak" ) < 0.01; } );
    json l3 = base;
    scale( l3, "CC", "IML", 0 );
    scale( l3, "LOW", "IML", 0 );
    lesion( "heart_autonomic", "no cue or error input to the sympathetic preganglionic cells: exercise rate not reached", l3, lseeds,
      []( const Run& r ) { return part( r, "exercise_hr" ) < 105; } );
    json l4 = base;
    scale( l4, "NA", "VG", 3 );
    lesion( "heart_autonomic", "vagal ganglion transmission x3: ACh near the arrest range, pauses and bradycardia", l4, lseeds,
      []( const Run& r ) { return part( r, "mae_bpm" ) > 6 || part( r, "recovery_hr" ) < 58 || part( r, "rest_hr" ) < 58; } );
    // the two release branches, each driven by the cue or the error: naming what is missing
    json one_pool = base;  // 'sympathetic' reads the vagal cells: one pool cannot release both ACh and NE
    for ( auto& o : one_pool[ "outputs" ] )
      if ( o[ "name" ] == "sympathetic" ) o[ "neurons" ] = json::array( { { { "pop", "VG" } } } );
    Run ro = run( "heart_autonomic", one_pool, 1, true );
    CHECK( !ro.score.passed && contains( ro.score.verdict, "same cells" ) && ro.score.value < 50.0 );
    json undriven = base;
    for ( const char* pre : { "CC", "LOW", "HIGH" } ) scale( undriven, pre, "NA", 0 );
    Run ru = run( "heart_autonomic", undriven, 1, true );
    CHECK( !ru.score.passed && contains( ru.score.verdict, "vagal branch is not driven" ) );
    no_numbers( "heart_autonomic undriven", ru );
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
    // the vagal limb is not needed to hold the pressure, but it is half of the reflex: the mechanism check fails the circuit and names the limb
    json v = base;
    scale( v, "NTS", "NA", 0 );
    Run rv = run( "baroreflex", v, 1 );
    show( "vagal limb cut (pressure still held, mechanism missing)", 1, rv );
    CHECK( !rv.score.passed && rv.score.value < 50.0 && contains( rv.score.verdict, "vagal limb" ) );
    CHECK( part( rv, "in_band" ) > 0.85 );  // the run itself is fine: it is the mechanism that is missing
    json s1 = base;  // the sympathetic limb made excitatory: high pressure would raise sympathetic output (no inhibitory relay)
    for ( auto& p : s1[ "projections" ] )
      if ( p[ "pre" ] == "CVLM" && p[ "post" ] == "RVLM" ) p[ "synapse" ][ "weight" ] = 140.0;
    Run rs = run( "baroreflex", s1, 1 );
    CHECK( !rs.score.passed && contains( rs.score.verdict, "sympathetic limb" ) );
    no_numbers( "baroreflex verdicts", rv );
    no_numbers( "baroreflex verdicts", rs );
    json bad = base;
    bad[ "outputs" ][ 2 ][ "neurons" ] = json::array();
    Run rb = run( "baroreflex", bad, 1 );
    CHECK( !rb.score.passed );
    CHECK( rb.score.verdict.find( "not connected" ) != std::string::npos );
  }
  return chk::finish( "tasks_heart_test" );
}
