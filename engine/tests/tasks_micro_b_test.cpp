// Micro-puzzles for concepts 11-19 (m_recruitment, m_reciprocal, m_recurrent, m_negative_feedback, m_feedforward, m_timed_bursts, m_rhythm, m_wave,
// m_divergent), through the real engine:
//   metadata    level kind, one new concept, 1-2 earlier ones, channels declared, notebook entry, three-tier hints, episode length
//   reference   the reference circuit passes on 5 seeds in standard mode and on 3 seeds in guided and researcher modes; deterministic
//   recipe      the circuit the tier-3 hint describes, built the way the lab builds one from an empty bench (skeleton, default weight 80 pA
//               and delay 2 ms unless the hint sets them), passes on 5 seeds: the "solvable from an empty bench with the hints" gate
//   unwired     a bench with the sensors connected to nothing scores "not wired" and does not pass
//   lesions     each reference circuit with its key mechanism removed must fail, and a failed run's hints carry a target (hint tier 2)
//   tuning aid:  tasks_micro_b_test run <task> <circuit.json> [seeds=1,2,3] [mode]
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include "check.h"
#include "nlohmann/json.hpp"
#include "micro_lab.h"
#include "spike/task.h"

using nlohmann::json;
using namespace spike;

struct Run
{
  Score score;
  std::unique_ptr< Task > task;
  bool loaded = false;
  std::string error;
};

static Run run_circuit( const std::string& id, const json& circuit_in, uint64_t seed, const std::string& mode = "standard" )
{
  Run r;
  Engine e;
  json c = circuit_in;
  c[ "seed" ] = 1000 * seed + circuit_in.value( "seed", 1 );  // as the lab's TEST x5 and spike-run do
  r.loaded = e.load( c.dump() );
  if ( !r.loaded )
  {
    r.error = e.error();
    return r;
  }
  r.task = make_task( id );
  r.task->set_mode( mode );
  r.task->reset( seed );
  const long steps = static_cast< long >( r.task->duration_ms() );
  for ( long s = 0; s < steps; ++s )
    r.task->step( e, 1.0 );
  r.score = r.task->score();
  return r;
}

static std::string circuits_dir() { return std::string( SPIKE_SOURCE_DIR ) + "/circuits/"; }
static json load_circuit( const std::string& f ) { return json::parse( chk::slurp( f ) ); }

// ---- the lab's bench, in a few lines -----------------------------------------------------------------------------------------
// skeleton: every sensor with its declared kind and the lab's first-connect weight (microlab::sensor_weight) and every motor as a rate readout.
static json skeleton( const std::string& id )
{
  auto t = make_task( id );
  json c = { { "schema_version", 1 }, { "name", "bench" }, { "resolution_ms", 0.1 }, { "seed", 1 }, { "populations", json::array() },
    { "projections", json::array() }, { "inputs", json::array() }, { "outputs", json::array() }, { "record", { { "spikes", "all" } } } };
  auto kinds = t->channel_kinds();
  for ( auto& s : t->sensor_channels() )
  {
    const std::string k = kinds.count( s ) ? kinds[ s ].kind : "poisson";
    c[ "inputs" ].push_back( { { "name", s }, { "kind", k }, { "targets", json::array() },
      { "weight", microlab::sensor_weight( *t, s, microlab::part_of( t->default_model() ) ) }, { "value", 0.0 } } );
  }
  for ( auto& m : t->motor_channels() )
    c[ "outputs" ].push_back( { { "name", m }, { "kind", "rate" }, { "neurons", json::array() } } );
  return c;
}
static void grp( json& c, const std::string& id, int n, const char* model = "iaf_psc_alpha", json params = json::object() )
{
  json pos = json::array();
  for ( int k = 0; k < n; ++k ) pos.push_back( { 0, 30 * k } );
  c[ "populations" ].push_back( { { "id", id }, { "model", model }, { "n", n }, { "params", params }, { "pos", pos }, { "role", "neuron" } } );
}
// a group of cells of the part the lab opens the level on (Task::default_model()), with parameters the player changed
static void dgrp( json& c, const std::string& task, const std::string& id, int n, json extra = json::object() )
{
  auto p = microlab::part_of( make_task( task )->default_model() );
  for ( auto it = extra.begin(); it != extra.end(); ++it ) p.params[ it.key() ] = it.value();
  grp( c, id, n, p.model.c_str(), p.params );
}
static void link( json& c, const std::string& pre, const std::string& post, double w, double delay = 2.0 )
{
  c[ "projections" ].push_back( { { "pre", pre }, { "post", post }, { "rule", "all_to_all" },
    { "synapse", { { "synapse_model", "static_synapse" }, { "weight", w }, { "delay", delay } } } } );
}
static void sens( json& c, const std::string& name, std::vector< std::string > pops, double weight = -1 )
{
  for ( auto& i : c[ "inputs" ] )
    if ( i[ "name" ] == name )
    {
      for ( auto& p : pops ) i[ "targets" ].push_back( p );
      if ( weight >= 0 ) i[ "weight" ] = weight;
    }
}
static void motor( json& c, const std::string& name, const std::string& pop )
{
  for ( auto& o : c[ "outputs" ] )
    if ( o[ "name" ] == name ) o[ "neurons" ].push_back( { { "pop", pop } } );
}

// ---- lesions ---------------------------------------------------------------------------------------------------------------------
static void scale_proj( json& c, const std::string& pre, const std::string& post, double f )
{
  bool hit = false;
  for ( auto& p : c[ "projections" ] )
    if ( p[ "pre" ] == pre && p[ "post" ] == post )
    {
      hit = true;
      p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * f;
    }
  CHECK_MSG( hit, "lesion " + pre + ">" + post + " matched no projection" );
}
static void cut_sensor( json& c, const std::string& name )
{
  for ( auto& i : c[ "inputs" ] )
    if ( i[ "name" ] == name ) i[ "targets" ] = json::array();
}
static void scale_sensor( json& c, const std::string& name, double f )
{
  for ( auto& i : c[ "inputs" ] )
    if ( i[ "name" ] == name ) i[ "weight" ] = i[ "weight" ].get< double >() * f;
}
static void set_param( json& c, const std::string& pop, const std::string& key, double v )
{
  for ( auto& p : c[ "populations" ] )
    if ( p[ "id" ] == pop ) p[ "params" ][ key ] = v;
}

struct Puzzle
{
  std::string id, concept_id;
  std::string chapter;
  std::function< json() > recipe;  // the tier-3 hint, built from an empty bench
  struct Lesion { const char* what; std::function< void( json& ) > apply; };
  std::vector< Lesion > lesions;
};


// ---- the novice path ---------------------------------------------------------------------------------------------------------------
// The first build of each path is made of the lab's own defaults: the part the parts picker opens on (default_model()), the first-connect weight
// of each sensor (the task's declared weight, else 300 pA per unit) and the default synapse weight onto the part (80 pA for the integrate-and-fire
// cell, delay 2 ms; a link set to inhibitory carries the same weight, negative). The following builds change what the verdict says, one thing at a
// time. The gate of docs/DESIGN-0.3.md section 10: every path ends in a pass (on 5 seeds) within 8 runs, with no recipe and no hint.
struct NStep { const char* what; std::function< json() > build; bool pass; const char* verdict_has; };
struct NPath { std::string task; std::vector< NStep > steps; };

static std::vector< NPath > novice_paths()
{
  std::vector< NPath > v;
  // m_recruitment: three single cells, the command onto each; the bias (ADVANCED, I_e) is the knob
  auto recruit = []( double bs, double bm, double bl ) {
    return [ = ] {
      json c = skeleton( "m_recruitment" );
      dgrp( c, "m_recruitment", "S", 1, { { "I_e", bs } } ), dgrp( c, "m_recruitment", "M", 1, { { "I_e", bm } } ), dgrp( c, "m_recruitment", "L", 1, { { "I_e", bl } } );
      sens( c, "command", { "S", "M", "L" } );
      motor( c, "mu_small", "S" ), motor( c, "mu_mid", "M" ), motor( c, "mu_large", "L" );
      return c;
    };
  };
  v.push_back( { "m_recruitment", {
    { "three identical cells (every bias 0)", recruit( 0, 0, 0 ), false, "stayed silent" },
    { "small pool +100 pA", recruit( 100, 0, 0 ), false, "too early" },
    { "large pool -100 pA", recruit( 100, 0, -100 ), false, "too early" },
    { "large pool -250 pA", recruit( 100, 0, -250 ), false, "too early" },
    { "large pool -350 pA", recruit( 100, 0, -350 ), true, "clean staircase" } } } );
  auto recip = []( double w ) {
    return [ = ] {
      json c = skeleton( "m_reciprocal" );
      dgrp( c, "m_reciprocal", "F", 2 ), dgrp( c, "m_reciprocal", "E", 2 ), dgrp( c, "m_reciprocal", "IN", 2 );
      sens( c, "tone", { "F", "E" } ), sens( c, "ia_flexor", { "F", "IN" } );
      link( c, "IN", "E", w );
      motor( c, "flexor", "F" ), motor( c, "extensor", "E" );
      return c;
    };
  };
  v.push_back( { "m_reciprocal", {
    { "Ia through an interneuron left excitatory (the default +80)", recip( 80 ), false, "did not relax" },
    { "the interneuron set inhibitory (-80)", recip( -80 ), true, "reciprocal inhibition" } } } );
  auto recur = []( double w ) {
    return [ = ] {
      json c = skeleton( "m_recurrent" );
      dgrp( c, "m_recurrent", "MN", 4 ), dgrp( c, "m_recurrent", "RC", 2 );
      sens( c, "drive", { "MN" } ), sens( c, "kick", { "MN" } );
      link( c, "MN", "RC", w ), link( c, "RC", "MN", -w );
      motor( c, "mn", "MN" );
      return c;
    };
  };
  v.push_back( { "m_recurrent", {
    { "4 motoneurons, 2 Renshaw cells, both links at the default 80 pA", recur( 80 ), false, "nothing holds" },
    { "both links 300 pA", recur( 300 ), false, "too little" },
    { "both links 450 pA", recur( 450 ), false, "too weak" },
    { "both links 600 pA", recur( 600 ), true, "gain control" } } } );
  auto negfb = []( double w ) {
    return [ = ] {
      json c = skeleton( "m_negative_feedback" );
      dgrp( c, "m_negative_feedback", "EFF", 2 ), dgrp( c, "m_negative_feedback", "INH", 3 );
      sens( c, "tone", { "EFF" } ), sens( c, "level", { "INH" } );
      link( c, "INH", "EFF", w );
      motor( c, "effort", "EFF" );
      return c;
    };
  };
  v.push_back( { "m_negative_feedback", {
    { "level through cells left excitatory (+80)", negfb( 80 ), false, "inhibitory" },
    { "the cells set inhibitory (-80)", negfb( -80 ), false, "" },
    { "-160", negfb( -160 ), false, "" },
    { "-300", negfb( -300 ), true, "negative feedback" } } } );
  auto feedfwd = []( double w ) {
    return [ = ] {
      json c = skeleton( "m_feedforward" );
      dgrp( c, "m_feedforward", "EFF", 2 ), dgrp( c, "m_feedforward", "INH", 3 );
      sens( c, "tone", { "EFF" } ), sens( c, "level", { "INH" } ), sens( c, "cue", { "EFF" } );
      link( c, "INH", "EFF", w );
      motor( c, "effort", "EFF" );
      return c;
    };
  };
  v.push_back( { "m_feedforward", {
    { "feedback cells at the default inhibitory weight (-80), cue onto the effort cells", feedfwd( -80 ), false, "inhibitory" },
    { "feedback weight -300", feedfwd( -300 ), true, "Feed-forward" } } } );
  auto bursts = []( double w, double delay ) {
    return [ = ] {
      json c = skeleton( "m_timed_bursts" );
      dgrp( c, "m_timed_bursts", "G1", 1 ), dgrp( c, "m_timed_bursts", "G2", 1 ), dgrp( c, "m_timed_bursts", "G3", 1 );
      sens( c, "go", { "G1" } );
      link( c, "G1", "G2", w, delay ), link( c, "G2", "G3", w, delay );
      motor( c, "b1", "G1" ), motor( c, "b2", "G2" ), motor( c, "b3", "G3" );
      return c;
    };
  };
  v.push_back( { "m_timed_bursts", {
    { "a chain of three single cells at the default 80 pA, 2 ms", bursts( 80, 2 ), false, "never burst" },
    { "300 pA", bursts( 300, 2 ), false, "never burst" },
    { "1200 pA (the verdict: weight x cells in the previous group is about 1200)", bursts( 1200, 2 ), false, "delay" },
    { "1200 pA with a delay of 120 ms", bursts( 1200, 120 ), true, "pattern generator" } } } );
  auto rhythm = []( double w ) {
    return [ = ] {
      json c = skeleton( "m_rhythm" );
      dgrp( c, "m_rhythm", "B", 1 );
      sens( c, "chemo", { "B" }, w );
      motor( c, "phrenic", "B" );
      return c;
    };
  };
  v.push_back( { "m_rhythm", {
    { "the part the lab opens on (chattering) with the lab's first-connect weight", [] {
        json c = skeleton( "m_rhythm" );
        dgrp( c, "m_rhythm", "B", 1 );
        sens( c, "chemo", { "B" } );
        motor( c, "phrenic", "B" );
        return c; }, true, "rhythm tuned by a sensor" } } } );
  v.push_back( { "m_rhythm", {
    { "a plain dc weight of 300 pA per unit", rhythm( 300 ), false, "steady train" },
    { "weight 5", rhythm( 5 ), true, "rhythm tuned by a sensor" } } } );
  auto wave = []( double w ) {
    return [ = ] {
      json c = skeleton( "m_wave" );
      dgrp( c, "m_wave", "S0", 2 ), dgrp( c, "m_wave", "S1", 2 ), dgrp( c, "m_wave", "S2", 2 ), dgrp( c, "m_wave", "IN", 2 );
      sens( c, "tone", { "S0", "S1", "S2" } );
      sens( c, "dist1", { "S0", "IN" } ), sens( c, "dist2", { "S1" } );
      link( c, "IN", "S2", w );
      motor( c, "seg0", "S0" ), motor( c, "seg1", "S1" ), motor( c, "seg2", "S2" );
      return c;
    };
  };
  v.push_back( { "m_wave", {
    { "dist1 through an interneuron left excitatory (+80)", wave( 80 ), false, "inhibitory" },
    { "the interneuron set inhibitory (-80)", wave( -80 ), true, "wave has a direction" } } } );
  auto diverge = []( double wh, double wp ) {
    return [ = ] {
      json c = skeleton( "m_divergent" );
      dgrp( c, "m_divergent", "C", 2 ), dgrp( c, "m_divergent", "H", 1 ), dgrp( c, "m_divergent", "P", 1 ), dgrp( c, "m_divergent", "S", 1 ), dgrp( c, "m_divergent", "G", 1 ), dgrp( c, "m_divergent", "IN", 2 );
      sens( c, "alarm", { "C", "IN" } ), sens( c, "tone", { "G" } );
      link( c, "C", "H", wh ), link( c, "C", "P", wp ), link( c, "C", "S", wp ), link( c, "IN", "G", -80 );
      motor( c, "heart", "H" ), motor( c, "pupil", "P" ), motor( c, "sweat", "S" ), motor( c, "gut", "G" );
      return c;
    };
  };
  v.push_back( { "m_divergent", {
    { "a command group of 2 cells, every link at the default 80 pA, inhibitory route -80 to the gut", diverge( 80, 80 ), false, "heart reaches only" },
    { "the links 300 pA", diverge( 300, 300 ), false, "heart reaches only" },
    { "the links 600 pA", diverge( 600, 600 ), false, "pupil fires" },
    { "the heart's link 600, pupil and sweat 280", diverge( 600, 280 ), true, "one command, many effectors" } } } );
  return v;
}

static std::vector< Puzzle > puzzles()
{
  std::vector< Puzzle > v;
  v.push_back( { "m_recruitment", "recruitment", "spinal",
    [] {
      json c = skeleton( "m_recruitment" );
      grp( c, "S", 3, "iaf_psc_alpha", { { "I_e", 100.0 } } );
      grp( c, "M", 3 );
      grp( c, "L", 3, "iaf_psc_alpha", { { "I_e", -350.0 } } );
      sens( c, "command", { "S", "M", "L" } );
      motor( c, "mu_small", "S" ), motor( c, "mu_mid", "M" ), motor( c, "mu_large", "L" );
      return c;
    },
    { { "identical thresholds (every bias 0): all pools switch on together", []( json& c ) { for ( auto p : { "MU_S", "MU_L" } ) set_param( c, p, "I_e", 0.0 ); } },
      { "command cut from the large pool", []( json& c ) { for ( auto& i : c[ "inputs" ] ) if ( i[ "name" ] == "command" ) i[ "targets" ] = json::array( { "MU_S", "MU_M" } ); } } } } );
  v.push_back( { "m_reciprocal", "reciprocal_inhibition", "spinal",
    [] {
      json c = skeleton( "m_reciprocal" );
      grp( c, "F", 2 ), grp( c, "E", 2 ), grp( c, "IN", 2 );
      sens( c, "tone", { "F", "E" } );
      sens( c, "ia_flexor", { "F", "IN" } );
      link( c, "IN", "E", -160 );
      motor( c, "flexor", "F" ), motor( c, "extensor", "E" );
      return c;
    },
    { { "Ia inhibitory interneuron cut from the antagonist", []( json& c ) { scale_proj( c, "IN", "MN_E", 0.0 ); } },
      { "Ia afferent cut from its own motoneurons", []( json& c ) { for ( auto& i : c[ "inputs" ] ) if ( i[ "name" ] == "ia_flexor" ) i[ "targets" ] = json::array( { "IN" } ); } } } } );
  v.push_back( { "m_recurrent", "recurrent_inhibition", "spinal",
    [] {
      json c = skeleton( "m_recurrent" );
      grp( c, "MN", 4 ), grp( c, "RC", 2 );
      sens( c, "drive", { "MN" } ), sens( c, "kick", { "MN" } );
      link( c, "MN", "RC", 600 ), link( c, "RC", "MN", -600 );
      motor( c, "mn", "MN" );
      return c;
    },
    { { "Renshaw cells no longer inhibit the motoneurons", []( json& c ) { scale_proj( c, "RC", "MN", 0.0 ); } },
      { "motoneurons no longer drive the Renshaw cells", []( json& c ) { scale_proj( c, "MN", "RC", 0.0 ); } },
      { "Renshaw loop at the lab's default weight (80 pA)", []( json& c ) { scale_proj( c, "MN", "RC", 0.1 ), scale_proj( c, "RC", "MN", 0.1 ); } } } } );
  v.push_back( { "m_negative_feedback", "negative_feedback", "spinal",
    [] {
      json c = skeleton( "m_negative_feedback" );
      grp( c, "EFF", 2 ), grp( c, "INH", 3 );
      sens( c, "tone", { "EFF" } ), sens( c, "level", { "INH" } );
      link( c, "INH", "EFF", -300 );
      motor( c, "effort", "EFF" );
      return c;
    },
    { { "feedback cut: the level no longer reaches the effort cells", []( json& c ) { scale_proj( c, "INH", "EFF", 0.0 ); } },
      { "feedback with the wrong sign (excitatory)", []( json& c ) { scale_proj( c, "INH", "EFF", -1.0 ); } } } } );
  v.push_back( { "m_feedforward", "feedforward_feedback", "heart",
    [] {
      json c = skeleton( "m_feedforward" );
      grp( c, "EFF", 2 ), grp( c, "INH", 3 );
      sens( c, "tone", { "EFF" } ), sens( c, "level", { "INH" } ), sens( c, "cue", { "EFF" } );
      link( c, "INH", "EFF", -300 );
      motor( c, "effort", "EFF" );
      return c;
    },
    { { "feedback cut (feed-forward only): the unannounced workload is not corrected", []( json& c ) { scale_proj( c, "INH", "EFF", 0.0 ); } },
      { "cue cut (feedback only): the announced workload dips too far", []( json& c ) { cut_sensor( c, "cue" ); } } } } );
  v.push_back( { "m_timed_bursts", "timed_bursts", "spinal",
    [] {
      json c = skeleton( "m_timed_bursts" );
      grp( c, "G1", 4 ), grp( c, "G2", 4 ), grp( c, "G3", 4 );
      sens( c, "go", { "G1" } );
      link( c, "G1", "G2", 300, 120 ), link( c, "G2", "G3", 300, 120 );
      motor( c, "b1", "G1" ), motor( c, "b2", "G2" ), motor( c, "b3", "G3" );
      return c;
    },
    { { "no delays on the links: the three bursts coincide", []( json& c ) { for ( auto& p : c[ "projections" ] ) p[ "synapse" ][ "delay" ] = 2.0; } },
      { "the second link deleted: the third burst never comes", []( json& c ) { scale_proj( c, "G2", "G3", 0.0 ); } } } } );
  v.push_back( { "m_rhythm", "rhythm_gating", "breathing",
    [] {
      json c = skeleton( "m_rhythm" );
      grp( c, "B", 1, "izhikevich", { { "a", 0.02 }, { "b", 0.2 }, { "c", -50.0 }, { "d", 2.0 } } );
      sens( c, "chemo", { "B" }, 5.0 );
      motor( c, "phrenic", "B" );
      return c;
    },
    { { "sensor weight left at the lab's 300 pA per unit: the cell fires steadily", []( json& c ) { scale_sensor( c, "chemo", 60.0 ); } },
      { "sensor weight far too low: the cell is silent", []( json& c ) { scale_sensor( c, "chemo", 0.2 ); } } } } );
  v.push_back( { "m_wave", "travelling_wave", "autonomic",
    [] {
      json c = skeleton( "m_wave" );
      grp( c, "S0", 2 ), grp( c, "S1", 2 ), grp( c, "S2", 2 ), grp( c, "IN", 2 );
      sens( c, "tone", { "S0", "S1", "S2" } );
      sens( c, "dist1", { "S0", "IN" } ), sens( c, "dist2", { "S1" } );
      link( c, "IN", "S2", -160 );
      motor( c, "seg0", "S0" ), motor( c, "seg1", "S1" ), motor( c, "seg2", "S2" );
      return c;
    },
    { { "descending inhibition cut: the segment below the bolus does not relax", []( json& c ) { scale_proj( c, "IN", "SEG2", 0.0 ); } },
      { "ascending excitation cut: the segment above the bolus does not contract", []( json& c ) { for ( auto& i : c[ "inputs" ] ) if ( i[ "name" ] == "dist1" ) i[ "targets" ] = json::array( { "IN" } ); } } } } );
  v.push_back( { "m_divergent", "divergent_command", "autonomic",
    [] {
      json c = skeleton( "m_divergent" );
      grp( c, "C", 2 ), grp( c, "H", 1 ), grp( c, "P", 1 ), grp( c, "S", 1 ), grp( c, "G", 1 ), grp( c, "IN", 2 );
      sens( c, "alarm", { "C", "IN" } ), sens( c, "tone", { "G" } );
      link( c, "C", "H", 600 ), link( c, "C", "P", 280 ), link( c, "C", "S", 280 ), link( c, "IN", "G", -160 );
      motor( c, "heart", "H" ), motor( c, "pupil", "P" ), motor( c, "sweat", "S" ), motor( c, "gut", "G" );
      return c;
    },
    { { "inhibitory route to the gut cut: digestion goes on", []( json& c ) { scale_proj( c, "INH", "GUT", 0.0 ); } },
      { "command group no longer reaches the heart", []( json& c ) { scale_proj( c, "CMD", "HEART", 0.0 ); } },
      { "every effector at the heart's weight (no per-target gain)", []( json& c ) { scale_proj( c, "CMD", "PUPIL", 600.0 / 280.0 ), scale_proj( c, "CMD", "SWEAT", 600.0 / 280.0 ); } } } } );
  return v;
}

int main( int argc, char** argv )
{
  if ( argc >= 4 && std::string( argv[ 1 ] ) == "run" )
  {
    json c = load_circuit( argv[ 3 ] );
    std::vector< uint64_t > seeds;
    std::stringstream ss( argc > 4 ? argv[ 4 ] : "1,2,3" );
    for ( std::string tok; std::getline( ss, tok, ',' ); ) seeds.push_back( std::stoull( tok ) );
    for ( auto s : seeds )
    {
      Run r = run_circuit( argv[ 2 ], c, s, argc > 5 ? argv[ 5 ] : "standard" );
      std::printf( "seed %llu value %.1f %s\n  %s\n  ", static_cast< unsigned long long >( s ), r.score.value, r.score.passed ? "PASS" : "fail", r.score.verdict.c_str() );
      for ( auto& [ k, x ] : r.score.parts ) std::printf( "%s=%.3g ", k.c_str(), x );
      std::printf( "\n" );
    }
    return 0;
  }

  const std::set< std::string > chapters = { "invertebrates", "spinal", "heart", "breathing", "autonomic" };
  std::set< std::string > ids = { "m_recruitment", "m_reciprocal", "m_recurrent", "m_negative_feedback", "m_feedforward", "m_timed_bursts", "m_rhythm", "m_wave", "m_divergent" };
  std::set< std::string > registered;
  for ( auto& id : task_ids() ) registered.insert( id );
  for ( auto& id : ids ) CHECK_MSG( registered.count( id ), id + " is not registered" );

  for ( const auto& p : puzzles() )
  {
    std::printf( "%s\n", p.id.c_str() );
    auto t = make_task( p.id );
    if ( !t )
    {
      CHECK_MSG( false, p.id + " cannot be made" );
      continue;
    }
    // ---- metadata ----
    CHECK( t->id() == p.id );
    CHECK_MSG( reference_circuit( p.id ) == p.id + ".json", reference_circuit( p.id ) );
    CHECK( t->level_kind() == "micro" );
    CHECK( t->on_spine() );
    CHECK_MSG( t->concepts_new().size() == 1 && t->concepts_new()[ 0 ] == p.concept_id, "one new concept: " + p.concept_id );
    CHECK_MSG( !t->concepts_used().empty() && t->concepts_used().size() <= 2, "1-2 earlier concepts" );
    for ( auto& u : t->concepts_used() ) CHECK_MSG( u != p.concept_id, "a concept is not its own prerequisite" );
    CHECK( t->chapter() == p.chapter );
    CHECK( chapters.count( t->chapter() ) );
    CHECK( t->duration_ms() >= 2000 && t->duration_ms() <= 5000 );
    CHECK( t->par_neurons() >= 1 && t->par_neurons() <= 12 );
    CHECK( t->title().size() >= 5 && t->brief().size() > 400 );
    // the lab opens the level on a declared part (the reference is built of the same cell type); the recipe lives apart from the brief
    CHECK_MSG( !t->default_model().empty(), p.id + " declares default_model" );
    CHECK_MSG( t->recipe().size() > 60, p.id + " has a recipe (concrete numbers, kept out of the brief)" );
    CHECK_MSG( t->brief().find( " pA" ) == std::string::npos && t->brief().find( " nS" ) == std::string::npos, p.id + ": the brief carries numbers that belong to recipe()" );
    CHECK( t->view() == "dashboard" );
    auto kinds = t->channel_kinds();
    for ( auto& s : t->sensor_channels() ) CHECK_MSG( kinds.count( s ) && ( kinds[ s ].kind == "dc" || kinds[ s ].kind == "poisson" ), p.id + " sensor kind " + s );
    for ( auto& m : t->motor_channels() ) CHECK_MSG( kinds.count( m ) && kinds[ m ].kind == "rate", p.id + " motor kind " + m );
    auto note = t->notebook();
    CHECK( note.title.size() > 3 && note.text.size() > 150 && note.citation.size() > 20 );
    for ( auto& b : t->bands() ) CHECK( b.lo < b.hi && !b.name.empty() );
    for ( auto& ev : t->timeline() ) CHECK( ev.t_ms >= 0 && ev.t_ms + ev.dur_ms <= t->duration_ms() + 1 && !ev.label.empty() );
    auto h = t->hints();
    CHECK_MSG( h.size() == 3 && h[ 0 ].tier == 1 && h[ 1 ].tier == 2 && h[ 2 ].tier == 3, "hints() lists three tiers" );
    for ( auto& x : h ) CHECK( x.text.size() > 30 );
    CHECK( !h.empty() && !h[ 1 ].target.empty() );

    json ref = load_circuit( circuits_dir() + reference_circuit( p.id ) );
    CHECK_MSG( ref[ "populations" ][ 0 ][ "model" ] == microlab::part_of( t->default_model() ).model, p.id + ": default_model " + t->default_model() + " is the reference's first cell type" );
    for ( auto& s : t->sensor_channels() )
    {
      bool have = false;
      for ( auto& i : ref[ "inputs" ] ) have = have || i[ "name" ] == s;
      CHECK_MSG( have, p.id + " reference declares sensor " + s );
    }
    int neurons = 0;
    for ( auto& pp : ref[ "populations" ] ) neurons += pp[ "n" ].get< int >();
    CHECK_MSG( neurons <= 20, p.id + " reference has " + std::to_string( neurons ) + " neurons" );
    CHECK_MSG( ref[ "populations" ].size() <= 6, p.id + " reference groups" );

    // ---- reference: five seeds standard, three seeds in the other modes, deterministic ----
    double first = -1;
    for ( uint64_t seed : { 1, 2, 3, 4, 5 } )
    {
      Run r = run_circuit( p.id, ref, seed );
      CHECK_MSG( r.loaded, r.error );
      CHECK_MSG( r.score.passed, p.id + " reference seed " + std::to_string( seed ) + ": " + r.score.verdict );
      CHECK( r.score.value >= 99.0 );
      if ( seed == 1 ) first = r.score.value;
      if ( seed == 1 )
      {
        Run again = run_circuit( p.id, ref, seed );
        CHECK_MSG( again.score.value == first && again.score.verdict == r.score.verdict, p.id + " is not deterministic" );
        auto m = r.task->metrics();
        CHECK_MSG( !m.empty(), p.id + " has a quality metric" );
        for ( auto& [ k, x ] : m ) CHECK_MSG( std::isfinite( x.value ), p.id + " metric " + k );
      }
    }
    for ( const char* mode : { "guided", "researcher" } )
      for ( uint64_t seed : { 1, 2, 3 } )
      {
        Run r = run_circuit( p.id, ref, seed, mode );
        CHECK_MSG( r.score.passed, p.id + " reference in " + mode + " mode, seed " + std::to_string( seed ) + ": " + r.score.verdict );
      }

    // ---- the tier-3 recipe, from an empty bench, passes ----
    json rec = p.recipe();
    for ( uint64_t seed : { 1, 2, 3, 4, 5 } )
    {
      Run r = run_circuit( p.id, rec, seed );
      CHECK_MSG( r.score.passed, p.id + " hint-3 recipe seed " + std::to_string( seed ) + ": " + r.score.verdict );
    }

    // ---- a bench with nothing wired (one idle cell behind every motor channel) is "not wired" and does not pass ----
    {
      json c = skeleton( p.id );
      grp( c, "X", 1 );
      for ( auto& m : t->motor_channels() ) motor( c, m, "X" );
      Run r = run_circuit( p.id, c, 1 );
      CHECK_MSG( r.loaded && !r.score.passed && r.score.verdict.rfind( "Not wired", 0 ) == 0, p.id + " unwired bench: " + r.score.verdict );
      CHECK( r.score.value <= 40.0 );
    }

    // ---- lesions fail, and a failed run's hints point somewhere ----
    for ( const auto& l : p.lesions )
    {
      json c = ref;
      l.apply( c );
      int passes = 0;
      for ( uint64_t seed : { 1, 2, 3 } )
      {
        Run r = run_circuit( p.id, c, seed );
        passes += r.score.passed ? 1 : 0;
        if ( seed == 1 )
        {
          std::printf( "  lesion: %s -> %.0f  %s\n", l.what, r.score.value, r.score.verdict.c_str() );
          auto hh = r.task->hints();
          CHECK_MSG( hh.size() == 3 && hh[ 0 ].tier == 1 && hh[ 1 ].tier == 2 && hh[ 2 ].tier == 3, p.id + " lesion hints" );
          CHECK_MSG( hh.size() == 3 && !hh[ 1 ].target.empty() && hh[ 2 ].text.size() > 30, p.id + " lesion hint targets" );
          CHECK_MSG( r.score.value < 100.0, p.id + " lesion value" );
        }
      }
      CHECK_MSG( passes == 0, p.id + " still passes with lesion: " + l.what );
    }
  }
  // ---- the novice path: lab defaults first, then what each verdict says; at most 8 runs, on 5 seeds ----
  std::printf( "\nnovice paths (lab default part and weights):\n" );
  std::set< std::string > first_seen;
  std::map< std::string, int > runs_to_pass;
  for ( const auto& p : novice_paths() )
  {
    auto t = make_task( p.task );
    const bool primary = first_seen.insert( p.task ).second;
    int runs = 0;
    bool passed = false;
    for ( const auto& st : p.steps )
    {
      ++runs;
      json c = st.build();
      if ( runs == 1 && primary )
        CHECK_MSG( c[ "populations" ][ 0 ][ "model" ] == microlab::part_of( t->default_model() ).model, p.task + ": the first build of the primary path uses the default part " + t->default_model() );
      Run r = run_circuit( p.task, c, 1 );
      CHECK_MSG( r.loaded, std::string( st.what ) + ": " + r.error );
      std::printf( "  %s run %d (%s): %.1f %s\n    %s\n", p.task.c_str(), runs, st.what, r.score.value, r.score.passed ? "PASS" : "fail", r.score.verdict.c_str() );
      CHECK_MSG( r.score.passed == st.pass, p.task + " step '" + st.what + "' expected " + ( st.pass ? "pass" : "fail" ) + ": " + r.score.verdict );
      if ( *st.verdict_has )
        CHECK_MSG( r.score.verdict.find( st.verdict_has ) != std::string::npos, p.task + " step '" + st.what + "' verdict should mention '" + st.verdict_has + "': " + r.score.verdict );
      if ( st.pass )
      {
        for ( uint64_t seed = 2; seed <= 5; ++seed )
          CHECK_MSG( run_circuit( p.task, c, seed ).score.passed, p.task + " step '" + st.what + "' seed " + std::to_string( seed ) );
        passed = true;
        break;
      }
      else if ( r.score.value > 0 )
        CHECK_MSG( r.score.value < 100.0, p.task + " a failing step shows 100" );
    }
    CHECK_MSG( passed, p.task + ": the novice path never passes" );
    CHECK_MSG( runs <= 8, p.task + ": needs " + std::to_string( runs ) + " runs" );
    if ( primary ) runs_to_pass[ p.task ] = runs;
  }
  // an empty bench (every channel wired to one idle cell) scores 0, not partial credit
  for ( const auto& p : puzzles() )
  {
    auto t = make_task( p.id );
    json c = skeleton( p.id );
    dgrp( c, p.id, "X", 1 );
    for ( auto& m : t->motor_channels() ) motor( c, m, "X" );
    for ( auto& sname : t->sensor_channels() ) sens( c, sname, { "X" }, 0.0 );
    Run r = run_circuit( p.id, c, 1 );
    CHECK_MSG( r.loaded && !r.score.passed && r.score.value == 0.0, p.id + ": nothing fires -> value 0, got " + std::to_string( r.score.value ) + " (" + r.score.verdict + ")" );
  }
  std::printf( "\nnovice runs to pass with the lab's default part (primary path):\n" );
  for ( auto& [ k, n ] : runs_to_pass ) std::printf( "  %-20s %d\n", k.c_str(), n );
  return chk::finish( "tasks_micro_b_test" );
}
