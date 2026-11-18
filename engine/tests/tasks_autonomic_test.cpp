// Breathing and autonomic chapters: breathing, pupil, peristalsis, fight_or_flight.
//   * each reference circuit passes on 10 seeds (1-10), respects the 60-neuron cap, declares the task's channels, and is deterministic;
//   * every lesion (a projection scaled to zero) fails with the biologically expected failure, checked on the score parts and the verdict,
//     not only on `passed`;
//   * a task whose sensor is not connected says so;
//   * the contract fields (chapter, view, par, timeline, bands, modulators) are filled.
// Own file (not tasks_lesions.inc) so these tasks can be tuned without touching the shared test.
//
//   tasks_autonomic_test                 the CTest (10 reference seeds + lesions: about 8 minutes on a quiet machine; CTest TIMEOUT must be >= 1200)
//   tasks_autonomic_test <task>          only that task's checks (breathing | pupil | peristalsis | fight_or_flight)
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <regex>
#include <string>
#include <vector>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

static json load( const std::string& id )
{
  return json::parse( chk::slurp( std::string( SPIKE_SOURCE_DIR ) + "/circuits/" + reference_circuit( id ) ) );
}

// weight of every projection pre -> post times `factor` (plain `synapse.weight` and `edges`)
static void scale( json& c, const std::string& pre, const std::string& post, double factor )
{
  bool hit = false;
  for ( auto& p : c[ "projections" ] )
    if ( p[ "pre" ] == pre && p[ "post" ] == post )
    {
      hit = true;
      if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) )
        p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * factor;
      if ( p.contains( "edges" ) )
        for ( auto& e : p[ "edges" ] )
          e[ 2 ] = e[ 2 ].get< double >() * factor;
    }
  CHECK_MSG( hit, "lesion " + pre + ">" + post + " matched no projection" );
}

struct Run
{
  Score score;
  std::vector< Task::Hint > hints;
  BodyState body;
  bool loaded = false;
  std::string error;
  double wall_s = 0;
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
  auto t0 = std::chrono::steady_clock::now();
  auto task = make_task( task_id );
  task->set_quick( quick );
  task->reset( seed );
  long steps = static_cast< long >( quick ? task->quick_duration_ms() : task->duration_ms() );
  for ( long s = 0; s < steps; ++s )
    task->step( e, 1.0 );
  r.wall_s = std::chrono::duration< double >( std::chrono::steady_clock::now() - t0 ).count();
  r.score = task->score();
  r.hints = task->hints();
  r.body = task->body();
  r.error = e.error();
  return r;
}

static double part( const Run& r, const std::string& k )
{
  auto it = r.score.parts.find( k );
  CHECK_MSG( it != r.score.parts.end(), "missing score part " + k );
  return it == r.score.parts.end() ? 0.0 : it->second;
}

// Honesty contract rule 2: verdicts and tier 1-3 hints carry no parameter numbers (a weight, a bias, a gain, a unit of current or conductance); only recipe() does.
// (Observed behaviour such as "62 /min" and the pass band the brief states are not parameters.)
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
  std::printf( "  %-14s seed %llu value %5.1f %s (%.1fs) | %s\n", what, ( unsigned long long )seed, r.score.value, r.score.passed ? "PASS" : "fail", r.wall_s,
    r.score.verdict.c_str() );
  std::printf( "      " );
  for ( auto& [ k, v ] : r.score.parts ) std::printf( " %s=%.3g", k.c_str(), v );
  std::printf( "\n" );
}

struct Lesion
{
  const char* what;
  std::vector< std::array< const char*, 2 > > cuts;       // pre, post: weight x 0
  int seeds;
  std::function< void( const Run& ) > expect;              // biologically expected failure signature
  bool must_fail = true;                                   // false: a partial deficit that may still pass (checked by `expect`)
};

static void check_contract( const std::string& id, const json& c, const char* chapter, const char* view, int order_hint )
{
  auto t = make_task( id );
  CHECK_MSG( t != nullptr, id + " is not registered" );
  if ( !t ) return;
  CHECK( t->id() == id );
  CHECK( t->chapter() == chapter );
  CHECK( t->view() == view );
  CHECK( t->par_neurons() > 0 );
  CHECK( !t->timeline().empty() );
  CHECK( !t->bands().empty() );
  CHECK( !t->title().empty() && t->brief().size() > 200 );
  CHECK( t->duration_ms() > 0 );
  for ( auto& ev : t->timeline() ) CHECK( ev.t_ms >= 0 && ev.dur_ms > 0 && ev.t_ms + ev.dur_ms <= t->duration_ms() + 1 );
  std::map< std::string, bool > in_ch, out_ch;
  for ( auto& i : c[ "inputs" ] ) in_ch[ i[ "name" ] ] = true;
  for ( auto& o : c[ "outputs" ] ) out_ch[ o[ "name" ] ] = true;
  for ( auto& s : t->sensor_channels() ) CHECK_MSG( in_ch.count( s ), id + " input " + s );
  for ( auto& m : t->motor_channels() ) CHECK_MSG( out_ch.count( m ), id + " output " + m );
  int neurons = 0;
  for ( auto& p : c[ "populations" ] ) neurons += p[ "n" ].get< int >();
  CHECK_MSG( neurons <= 60, id + " has " + std::to_string( neurons ) + " neurons" );
  CHECK( neurons <= t->par_neurons() + 8 && neurons >= t->par_neurons() - 8 );
  // every band names a readout the HUD can draw
  BodyState b = t->body();
  for ( auto& band : t->bands() )
  {
    CHECK_MSG( b.readouts.count( band.name ), id + " band readout " + band.name );
    CHECK( band.lo < band.hi );
  }
  (void)order_hint;
}

static void reference( const std::string& id, const std::function< void( const Run&, uint64_t ) >& extra )
{
  json c = load( id );
  double sum = 0;
  std::vector< std::map< std::string, double > > parts;
  for ( uint64_t seed = 1; seed <= 10; ++seed )
  {
    Run r = run( id, c, seed );
    show( "reference", seed, r );
    CHECK_MSG( r.loaded, r.error );
    CHECK_MSG( r.score.passed, id + " seed " + std::to_string( seed ) + ": " + r.score.verdict );
    CHECK( r.score.value >= 50.0 );
    CHECK_MSG( r.error.empty(), r.error );
    extra( r, seed );
    sum += r.score.value;
    parts.push_back( r.score.parts );
  }
  std::printf( "  mean score %.1f\n", sum / 10 );
  // determinism: the same seed again gives the same numbers
  Run again = run( id, c, 1 );
  CHECK_MSG( again.score.parts == parts[ 0 ], id + " is not deterministic" );
  // the quick check (the game's CHECK) is a shorter episode, scored by the same rules: the reference passes it too
  auto qt = make_task( id );
  CHECK_MSG( qt->quick_duration_ms() <= qt->duration_ms(), id + " quick episode longer than the full one" );
  for ( uint64_t seed : { uint64_t( 1 ), uint64_t( 2 ), uint64_t( 3 ) } )
  {
    Run q = run( id, c, seed, true );
    show( "reference quick", seed, q );
    CHECK_MSG( q.score.passed, id + " quick check, seed " + std::to_string( seed ) + ": " + q.score.verdict );
    no_numbers( id + " quick reference", q );
  }
}

static void lesions( const std::string& id, const std::vector< Lesion >& list )
{
  for ( const auto& l : list )
  {
    json c = load( id );
    std::string what = l.what;
    for ( auto& cut : l.cuts ) scale( c, cut[ 0 ], cut[ 1 ], 0.0 );
    std::printf( "%s lesion: %s\n", id.c_str(), l.what );
    for ( int s = 1; s <= l.seeds; ++s )
    {
      Run r = run( id, c, s );
      show( "lesioned", s, r );
      CHECK_MSG( r.loaded, r.error );
      if ( l.must_fail )
        CHECK_MSG( !r.score.passed, id + " still passes with lesion: " + what );
      no_numbers( id + " lesion: " + what, r );
      l.expect( r );
    }
  }
}

static void unwired( const std::string& id, const std::string& input )
{
  json c = load( id );
  for ( auto& i : c[ "inputs" ] )
    if ( i[ "name" ] == input ) i[ "targets" ] = json::array();
  Run r = run( id, c, 1 );
  std::printf( "%s with '%s' unconnected: %s\n", id.c_str(), input.c_str(), r.score.verdict.c_str() );
  CHECK( !r.score.passed );
  CHECK_MSG( r.score.verdict.find( "not connected" ) != std::string::npos, "verdict: " + r.score.verdict );
}

// ----------------------------------------------------------------------------------------------------------------------------------
// a degenerate or cut circuit: not passed, below the pass bar, the verdict names the missing mechanism (a word), and no numbers in verdict or hints
static void mechanism( const std::string& id, const char* what, const json& c, const char* word, bool quick = true )
{
  Run r = run( id, c, 1, quick );
  show( what, 1, r );
  CHECK_MSG( r.loaded, r.error );
  CHECK_MSG( !r.score.passed, id + " passes: " + what );
  CHECK_MSG( r.score.value < 50.0, id + " " + what + ": value " + std::to_string( r.score.value ) );
  CHECK_MSG( contains( r.score.verdict, word ), id + " " + what + ": the verdict should name '" + word + "': " + r.score.verdict );
  no_numbers( id + ": " + what, r );
}

static void test_breathing()
{
  std::printf( "breathing\n" );
  json c = load( "breathing" );
  check_contract( "breathing", c, "breathing", "lungs", 300 );
  auto t = make_task( "breathing" );
  CHECK( t->modulators().empty() );
  CHECK( t->channel_kinds().count( "chemo" ) && t->channel_kinds()[ "chemo" ].kind == "dc" );
  reference( "breathing", [ & ]( const Run& r, uint64_t ) {
    // 12-20 breaths per minute at rest, ventilation up with CO2 and PaCO2 kept in band
    CHECK( part( r, "rest_rate_bpm" ) >= 12 && part( r, "rest_rate_bpm" ) <= 20 );
    CHECK( part( r, "rest_breaths" ) >= 5 );
    CHECK( part( r, "ve_ratio" ) >= 1.6 );
    CHECK( part( r, "paco2_rest" ) >= 35 && part( r, "paco2_rest" ) <= 45 );
    CHECK( part( r, "paco2_peak" ) <= 52 );
    CHECK( part( r, "paco2_recovery" ) <= 46 );
    CHECK( r.body.readouts.count( "rr_per_min" ) && r.body.readouts.count( "co2_mmhg" ) && r.body.readouts.count( "volume" ) );
  } );
  lesions( "breathing",
    { { "chemoreceptor afferent cut (RTN no longer drives the burst network or the phrenic pool): no CO2 response, PaCO2 runs away",
          { { "RTN", "PBC" }, { "RTN", "PHR" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "ve_ratio" ) < 1.4 );
            CHECK( part( r, "paco2_peak" ) > 60 );
          } },
      { "preBotC bursters uncoupled (no recurrent excitation): the cells burst out of step and the rhythm is lost",
          { { "PBC", "PBC" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "ve_ratio" ) < 1.4 );
            CHECK( part( r, "paco2_peak" ) > 55 );
          } },
      { "burst network not inhibiting the expiratory cells (no disinhibition): the phrenic pool is held silent at rest, apnoea and CO2 build-up",
          { { "PBC", "EXP" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "rest_breaths" ) < 5 );
            CHECK( part( r, "paco2_rest" ) > 50 );
          } },
      { "expiratory inhibition of the phrenic pool removed: tonic phrenic firing, no relaxation between bursts",
          { { "EXP", "PHR" } }, 1,
          []( const Run& r ) { CHECK( part( r, "rest_rate_bpm" ) > 20 || part( r, "rest_breaths" ) < 5 ); } } } );
  unwired( "breathing", "chemo" );
  { json d = c; scale( d, "PBC", "PBC", 0 ); mechanism( "breathing", "no recurrent coupling", d, "rhythm" ); }
  { json d = c; scale( d, "RTN", "PBC", 0 ); mechanism( "breathing", "chemo does not reach the rhythm group", d, "chemoreceptor" ); }
  {  // one tonic cell as phrenic pool (a lone cell is not a rhythm generator)
    json d = c;
    d[ "populations" ] = json::array( { json{ { "id", "T" }, { "model", "iaf_psc_alpha" }, { "n", 1 }, { "params", { { "I_e", 450.0 } } }, { "role", "neuron" } } } );
    d[ "projections" ] = json::array();
    for ( auto& i : d[ "inputs" ] ) { i[ "targets" ] = json::array( { { { "pop", "T" } } } ); i[ "weight" ] = 0.0; }
    d[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "T" } } } );
    mechanism( "breathing", "one tonic cell", d, "rhythm generator" );
  }
}

static void test_pupil()
{
  std::printf( "pupil\n" );
  json c = load( "pupil" );
  check_contract( "pupil", c, "autonomic", "eye", 400 );
  reference( "pupil", [ & ]( const Run& r, uint64_t ) {
    CHECK( part( r, "constriction_mm" ) >= 2.5 );                       // light: constricts
    CHECK( part( r, "redilation_mm" ) >= 2.5 );                         // dark: dilates again
    CHECK( part( r, "bright_mm" ) < part( r, "dim_mm" ) );
    CHECK( part( r, "constriction_latency_ms" ) >= 150 && part( r, "constriction_latency_ms" ) <= 350 );  // not a 1 ms noise crossing (S3)
    CHECK( part( r, "redilation_latency_ms" ) > 0 && part( r, "redilation_latency_ms" ) < 1500 );
    CHECK( part( r, "fright_dilation_mm" ) >= 0.8 );
    CHECK( r.body.readouts.count( "pupil_mm" ) && r.body.readouts.count( "light" ) );
  } );
  lesions( "pupil",
    { { "Edinger-Westphal cut off from the pretectum: no sphincter drive, the pupil stays wide in bright light",
          { { "PON", "EW" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "sphincter_spikes" ) < 1 );
            CHECK( part( r, "bright_mm" ) > 4.5 );
          } },
      { "ciliary ganglion cut off from the Edinger-Westphal nucleus: the same, one synapse later",
          { { "EW", "CG" } }, 1, []( const Run& r ) { CHECK( part( r, "sphincter_spikes" ) < 1 ); } },
      { "sympathetic chain cut (IML -> superior cervical ganglion): no dilator, the pupil cannot open in the dark",
          { { "IML", "SCG" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "dilator_spikes" ) < 1 );
            CHECK( part( r, "dim2_mm" ) < 6.0 );
          } },
      { "hypothalamic alarm cut from the sympathetic and parasympathetic arms: light reflex intact, no fright dilation",
          { { "HYP", "IML" }, { "HYP", "EW" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "fright_dilation_mm" ) < 0.8 );
            CHECK( part( r, "constriction_mm" ) >= 2.5 );
          } },
      { "retinal ganglion cells cut off from the pretectum: no light reflex at all",
          { { "RGC", "PON" } }, 1, []( const Run& r ) { CHECK( part( r, "constriction_mm" ) < 0.5 ); } } } );
  unwired( "pupil", "retina" );
  unwired( "pupil", "threat" );
  { json d = c; scale( d, "PON", "EW", 0 ); mechanism( "pupil", "no parasympathetic pathway", d, "parasympathetic" ); }
  { json d = c; scale( d, "HYP", "IML", 0 ); mechanism( "pupil", "no sympathetic pathway", d, "sympathetic" ); }
  {  // both outputs read the ciliary ganglion: one pool is not two muscles
    json d = c;
    for ( auto& o : d[ "outputs" ] ) if ( o[ "name" ] == "dilator" ) o[ "neurons" ] = json::array( { { { "pop", "CG" } } } );
    mechanism( "pupil", "one pool for both muscles", d, "same cells" );
  }
}

static void test_peristalsis()
{
  std::printf( "peristalsis\n" );
  json c = load( "peristalsis" );
  check_contract( "peristalsis", c, "autonomic", "gut", 410 );
  double ref_arrival = 0;
  reference( "peristalsis", [ & ]( const Run& r, uint64_t ) {
    CHECK( part( r, "arrival_s" ) > 0 && part( r, "arrival_s" ) <= 25 );
    CHECK( part( r, "progress" ) >= 0.99 );
    CHECK( part( r, "slow_waves" ) >= 5 );                          // the ICC clock ran (12/min for 40 s)
    CHECK( part( r, "ach_spikes" ) > 100 && part( r, "no_spikes" ) > 20 );  // both enteric motor arms fire
    CHECK( part( r, "net_distance" ) > 5 );
    ref_arrival = std::max( ref_arrival, part( r, "arrival_s" ) );
    CHECK( r.body.readouts.count( "bolus" ) && r.body.readouts.count( "seg0" ) && r.body.readouts.count( "seg7" ) );
  } );
  lesions( "peristalsis",
    { { "ascending excitation removed: tonic ACh plus slow waves still push the bolus along, but without the reflex it is clearly slower",
          { { "AIN", "EMN" } }, 1,
          [ & ]( const Run& r ) { CHECK( part( r, "arrival_s" ) < 0 || part( r, "arrival_s" ) > ref_arrival + 1 ); }, false },
      { "descending inhibition removed: the segments ahead keep contracting against the bolus, it arrives late or not at all",
          { { "DIN", "IMN" } }, 1,
          [ & ]( const Run& r ) {
            CHECK( part( r, "no_spikes" ) < 1 );
            CHECK( part( r, "arrival_s" ) < 0 || part( r, "arrival_s" ) > ref_arrival + 6 );
          } },
      { "interstitial cells of Cajal silenced (no slow waves): the muscle never gates above its contraction threshold",
          { { "ICCP", "ICC" }, { "ICC", "ICC" } }, 1,
          [ & ]( const Run& r ) {
            CHECK( part( r, "slow_waves" ) < 1 );
            CHECK( part( r, "arrival_s" ) < 0 || part( r, "arrival_s" ) > ref_arrival + 6 );
          } },
      { "stretch sensors cut off from the interneurons: the bolus is not detected",
          { { "S", "AIN" }, { "S", "DIN" } }, 1,
          []( const Run& r ) { CHECK( part( r, "progress" ) < 0.5 ); } } } );
  unwired( "peristalsis", "dist3" );
  // N4: icc*/no* are optional. Without any of them the task still runs (the gut's own clock), but slower; with no0..7 but no icc it passes.
  {
    auto strip = [ & ]( std::initializer_list< const char* > prefixes ) {
      json d = load( "peristalsis" );
      json outs = json::array();
      for ( auto& o : d[ "outputs" ] )
      {
        bool drop = false;
        for ( const char* p : prefixes )
          if ( o[ "name" ].get< std::string >().rfind( p, 0 ) == 0 ) drop = true;
        if ( !drop ) outs.push_back( o );
      }
      d[ "outputs" ] = outs;
      return d;
    };
    Run a = run( "peristalsis", strip( { "icc", "no" } ), 1 );
    show( "ach only", 1, a );
    CHECK( a.loaded );
    CHECK( part( a, "progress" ) >= 0.99 );                                   // it works, the bolus gets there ...
    CHECK( !a.score.passed );                                                 // ... but too late without the inhibitory arm
    CHECK_MSG( a.score.verdict.find( "no0..7" ) != std::string::npos, a.score.verdict );
    CHECK( a.score.value < 50.0 && contains( a.score.verdict, "descending" ) );   // the simple build is the start of the level, not the law of the intestine
    Run b = run( "peristalsis", strip( { "icc" } ), 1 );
    show( "ach + no", 1, b );
    CHECK_MSG( b.score.passed, b.score.verdict );
    no_numbers( "peristalsis simple build", a );
  }
  { json d = c; scale( d, "S", "DIN", 0 ); mechanism( "peristalsis", "descending chain loses its stretch input", d, "descending" ); }
  { json d = c; scale( d, "AIN", "EMN", 0 ); mechanism( "peristalsis", "no ascending chain", d, "ascending" ); }
}

static void test_fight_or_flight()
{
  std::printf( "fight_or_flight\n" );
  json c = load( "fight_or_flight" );
  check_contract( "fight_or_flight", c, "autonomic", "dashboard", 420 );
  auto t = make_task( "fight_or_flight" );
  std::vector< std::string > m = t->modulators();
  CHECK( m.size() == 2 && m[ 0 ] == "NE" && m[ 1 ] == "ACh" );
  reference( "fight_or_flight", [ & ]( const Run& r, uint64_t ) {
    CHECK( part( r, "conditions_met" ) == 12 );
    // each system: rest in band, clear rise during the threat, back to rest afterwards
    CHECK( part( r, "hr_threat" ) - part( r, "hr_rest" ) >= 40 );
    CHECK( part( r, "map_threat" ) - part( r, "map_rest" ) >= 12 );
    CHECK( part( r, "pupil_threat" ) - part( r, "pupil_rest" ) >= 1.2 );
    CHECK( part( r, "ve_threat" ) >= 1.4 * part( r, "ve_rest" ) );
    CHECK( std::abs( part( r, "hr_recovery" ) - part( r, "hr_rest" ) ) <= 8 );
    CHECK( std::abs( part( r, "map_recovery" ) - part( r, "map_rest" ) ) <= 8 );
    CHECK( std::abs( part( r, "pupil_recovery" ) - part( r, "pupil_rest" ) ) <= 0.6 );
    CHECK( part( r, "hr_rest" ) >= 55 && part( r, "hr_rest" ) <= 80 );
    CHECK( part( r, "map_threat" ) < 150 );                              // a strong, not a pathological, pressor response
    CHECK( r.body.readouts.count( "hr_bpm" ) && r.body.readouts.count( "map_mmhg" ) && r.body.readouts.count( "pupil_mm" ) &&
           r.body.readouts.count( "NE" ) && r.body.readouts.count( "ACh" ) );
  } );
  lesions( "fight_or_flight",
    { { "hypothalamus cut from the RVLM: no sympathetic outflow at all; pressure barely moves and heart rate rises only by vagal withdrawal (to the intrinsic rate; the pupil still opens a little by parasympathetic withdrawal)",
          { { "HYP", "RVLM" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "hr_threat" ) - part( r, "hr_rest" ) < 40 );
            CHECK( part( r, "map_threat" ) - part( r, "map_rest" ) < 12 );
          } },
      { "stellate ganglion cut from the RVLM: no noradrenaline on the pacemaker, heart rate rises only by vagal withdrawal",
          { { "RVLM", "SG" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "hr_threat" ) - part( r, "hr_rest" ) < 40 );
            CHECK( part( r, "hr_threat" ) - part( r, "hr_rest" ) > 2 );     // the vagal withdrawal alone does speed it up a little
          } },
      { "respiratory group cut from the hypothalamus: no ventilatory response",
          { { "HYP", "VRG" } }, 1, []( const Run& r ) { CHECK( part( r, "ve_threat" ) < 1.4 * part( r, "ve_rest" ) ); } },
      { "superior cervical ganglion and Edinger-Westphal drive cut: pupil fixed while the rest of the response is intact",
          { { "RVLM", "SCG" }, { "HYP", "EW" } }, 1,
          []( const Run& r ) {
            CHECK( part( r, "pupil_threat" ) - part( r, "pupil_rest" ) < 1.2 );
            CHECK( part( r, "hr_threat" ) - part( r, "hr_rest" ) >= 40 );
          } } } );
  unwired( "fight_or_flight", "threat" );
  { json d = c; scale( d, "RVLM", "SCG", 0 ); mechanism( "fight_or_flight", "the command does not reach the pupil", d, "pupil" ); }
  { json d = c; scale( d, "HYP", "VRG", 0 ); mechanism( "fight_or_flight", "the command does not reach the breathing group", d, "breathing" ); }
  // a novice's 'sympathetic' / 'vagal' declared as plain rate outputs: the verdict says to make them modulators
  {
    json d = load( "fight_or_flight" );
    for ( auto& o : d[ "outputs" ] )
      if ( o[ "kind" ] == "modulator" )
      {
        o[ "kind" ] = "rate";
        o.erase( "modulator" ), o.erase( "gain" ), o.erase( "tau_ms" );
      }
    Run r = run( "fight_or_flight", d, 1 );
    show( "rate outputs", 1, r );
    CHECK( !r.score.passed );
    CHECK_MSG( r.score.verdict.find( "modulator" ) != std::string::npos, r.score.verdict );
  }
  auto kinds = t->channel_kinds();
  CHECK( kinds.count( "sympathetic" ) && kinds[ "sympathetic" ].kind == "modulator" && kinds[ "sympathetic" ].modulator == "NE" );
  CHECK( kinds.count( "vagal" ) && kinds[ "vagal" ].kind == "modulator" && kinds[ "vagal" ].modulator == "ACh" );
  CHECK( kinds.count( "threat" ) && kinds[ "threat" ].kind == "poisson" );
}

int main( int argc, char** argv )
{
  std::string only = argc > 1 ? argv[ 1 ] : "";
  if ( only.empty() || only == "pupil" ) test_pupil();
  if ( only.empty() || only == "peristalsis" ) test_peristalsis();
  if ( only.empty() || only == "fight_or_flight" ) test_fight_or_flight();
  if ( only.empty() || only == "breathing" ) test_breathing();
  return chk::finish( "tasks_autonomic_test" );
}
