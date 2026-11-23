// Shared plumbing of the worm chapter of the Atlas wing (chapter "a_worm"; docs/ROADMAP-CONNECTOME.md, iteration A2; research/atlas-a.md 4b):
// a_path, a_predict and dx_a_worm run the head-touch world below on the measured touch wiring of C. elegans (data/connectome/worm/touch.json, Cook et al. 2019),
// a_motif reads the whole chemical wiring (data/connectome/worm/whole.json) and needs only the helpers at the end.
//
// THE MODEL (what the circuits of godot/circuits/tools/gen_atlas_worm.py hold; "in this model", never "in the animal"): leaky integrate-and-fire cells, every synapse
// excitatory, weight = synapse count x one scale, no electrical coupling, a synapse floor, one background current. Cells of the five interneuron classes AVA, AVB, AVD, AVE
// and PVC form five cell-type groups whose scale the player may set from 0 (the class is silenced) to 1 (it is on); the sign is fixed.
//
// WORLD (1500 ms): the sensor "touch_head" is a Poisson train, 150 Hz per touch cell, on the head touch cells (ALM, AVM) from 500 to 900 ms. Motors: "reverse" (the A-type motor
// neurons DA, VA) and "forward" (the B-type motor neurons DB, VB), mean Hz per cell.
// WHAT THE RUN SHOWED ("the worm backs up") = A-type at least 5 Hz and B-type below 1 Hz over [500, 1200) ms, both motor groups below 1 Hz before the touch ([100, 500)) and
// again at the end ([1000, 1500)). The verdicts say what the measured rates show, in words, never a number.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <set>
#include "dx_common.h"
#include "micro_a.h"
#include "spike/analysis.h"

namespace spike::tasks::atlas_worm
{
inline const std::vector< std::string >& commands()
{
  static const std::vector< std::string > c = { "AVA", "AVB", "AVD", "AVE", "PVC" };
  return c;
}

/// The answer string of a diagnosis case or a prediction bet (set_variant): "candidate", "candidate@N", "candidate@N:W", "candidate@revealed" (see dx_common.h).
struct Commit
{
  bool has = false, revealed = false;
  std::string cand;
  int probes = 0, wrong = 0;
};
inline Commit parse_commit( const std::string& v )
{
  Commit c;
  if ( v.empty() ) return c;
  c.has = true;
  c.cand = v;
  if ( const auto at = v.rfind( '@' ); at != std::string::npos )
  {
    c.cand = v.substr( 0, at );
    const std::string tail = v.substr( at + 1 );
    if ( tail == "revealed" )
      c.revealed = true;
    else
    {
      c.probes = std::max( 0, std::atoi( tail.c_str() ) );
      if ( const auto colon = tail.find( ':' ); colon != std::string::npos ) c.wrong = std::max( 0, std::atoi( tail.c_str() + colon + 1 ) );
    }
  }
  return c;
}
/// 100 - 15 per wrong commit - 3 per instrument read beyond the free budget, never below 25 (the diagnosis rule, with the level's own budget).
inline double bet_value( int wrong, int probes, int budget ) { return std::max( 25.0, 100.0 - 15.0 * wrong - 3.0 * std::max( 0, probes - budget ) ); }
inline std::string word( int n )
{
  static const char* w[] = { "no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten" };
  return n >= 0 && n <= 10 ? w[ n ] : "many";
}
/// Candidates in a stable hash order, the answer never first (as the diagnosis cases).
inline std::vector< std::string > ordered( const std::string& id, std::vector< std::string > c, const std::string& answer )
{
  std::stable_sort( c.begin(), c.end(), [ & ]( const std::string& a, const std::string& b ) { return DxTask::hash_of( id, a ) < DxTask::hash_of( id, b ); } );
  if ( c.size() > 1 && c.front() == answer ) std::swap( c.front(), c[ 1 + DxTask::hash_of( id, "first" ) % ( c.size() - 1 ) ] );
  return c;
}

/// The head-touch world on the touch circuit and what it measures.
class WormRun : public micro::MicroBase
{
public:
  explicit WormRun( Info i ) : MicroBase( std::move( i ) ) {}

  static constexpr double kDur = 1500, kOn = 500, kOff = 900, kHz = 150;
  static constexpr double kBackUpHz = 5, kQuietHz = 1;

  static Info world( const std::string& id, const std::string& title )
  {
    Info i;
    i.id = id;
    i.title = title;
    i.chapter = "a_worm";
    i.sensors = { "touch_head" };
    i.motors = { "reverse", "forward" };
    i.duration_ms = kDur;
    i.par_neurons = 54;
    i.default_model = "iaf_psc_alpha";
    Task::ChannelKind touch;
    touch.kind = "poisson";
    touch.value = kHz;
    touch.weight = 1000;
    i.kinds = { { "touch_head", touch }, { "reverse", micro::rate() }, { "forward", micro::rate() } };
    i.bands = { { "reverse_hz", "Hz", kBackUpHz, 600 }, { "forward_hz", "Hz", 0, kQuietHz } };
    i.timeline.push_back( { kOn, kOff - kOn, "touch on the head" } );
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    for ( double& x : n_ ) x = 0;
  }

  struct Rates
  {
    double a_base = 0, b_base = 0, a_resp = 0, b_resp = 0, a_after = 0, b_after = 0;
  };
  Rates rates() const
  {
    auto r = [ & ]( int k, double pool, double len_ms ) { return pool > 0 ? n_[ k ] / pool / ( len_ms / 1000.0 ) : 0.0; };
    Rates x;
    x.a_base = r( 0, pool_[ 0 ], 400 );
    x.b_base = r( 1, pool_[ 1 ], 400 );
    x.a_resp = r( 2, pool_[ 0 ], 700 );
    x.b_resp = r( 3, pool_[ 1 ], 700 );
    x.a_after = r( 4, pool_[ 0 ], 500 );
    x.b_after = r( 5, pool_[ 1 ], 500 );
    return x;
  }
  /// The run showed the backward response: the worm backs up.
  bool backs_up() const
  {
    const Rates r = rates();
    return r.a_resp >= kBackUpHz && r.b_resp < kQuietHz && std::max( r.a_base, r.b_base ) < kQuietHz && std::max( r.a_after, r.b_after ) < kQuietHz;
  }
  /// What the measured rates show, in words (no number).
  std::string outcome() const
  {
    const Rates r = rates();
    if ( std::max( r.a_base, r.b_base ) >= kQuietHz ) return "The motor neurons fire before any touch, so nothing can be read from the touch.";
    if ( r.a_resp < kQuietHz && r.b_resp < kQuietHz ) return "After the touch the motor neurons stay silent: the worm does not back up.";
    if ( r.a_resp < kBackUpHz && r.b_resp < kQuietHz ) return "After the touch the backward motor neurons fire only weakly: the worm hardly backs up.";
    if ( r.b_resp >= kQuietHz ) return "After the touch the forward motor neurons fire as well as, or instead of, the backward ones: the worm does not back up cleanly.";
    if ( std::max( r.a_after, r.b_after ) >= kQuietHz ) return "The backward motor neurons fire but do not stop after the touch: the response does not end.";
    return "After the touch the backward motor neurons fire and the forward ones stay silent: the worm backs up.";
  }

  /// The setting of the five cell-type groups: scale 0 = silenced, scale 1 = on, anything between = partial.
  struct Setting
  {
    std::vector< std::string > silenced, kept, partial, missing;
  };
  Setting setting() const
  {
    Setting s;
    for ( const auto& c : commands() )
    {
      const GroupInfo* gr = nullptr;
      for ( const auto& x : g_.st.groups )
        if ( x.id == c ) gr = &x;
      if ( !gr ) s.missing.push_back( c );
      else if ( gr->scale <= 1e-9 ) s.silenced.push_back( c );
      else if ( gr->scale >= 1.0 - 1e-9 ) s.kept.push_back( c );
      else s.partial.push_back( c );
    }
    return s;
  }

protected:
  void drive( Engine& e, double ) override { e.set_input( "touch_head", t_ >= kOn && t_ < kOff ? kHz : 0.0 ); }
  void observe( Engine& e, double dt ) override
  {
    pool_[ 0 ] = motor_.pool_size( e, "reverse" );
    pool_[ 1 ] = motor_.pool_size( e, "forward" );
    const double a = motor_.spike_count( e, "reverse", dt ), b = motor_.spike_count( e, "forward", dt );
    auto in = [ & ]( double t0, double t1 ) { return t_ > t0 && t_ <= t1; };
    if ( in( 100, 500 ) ) { n_[ 0 ] += a; n_[ 1 ] += b; }
    if ( in( 500, 1200 ) ) { n_[ 2 ] += a; n_[ 3 ] += b; }
    if ( in( 1000, 1500 ) ) { n_[ 4 ] += a; n_[ 5 ] += b; }
    readouts_[ "reverse_hz" ] = motor_.hz_per_neuron( e, "reverse" );
    readouts_[ "forward_hz" ] = motor_.hz_per_neuron( e, "forward" );
  }
  void fill_parts( Score& s ) const
  {
    const Rates r = rates();
    s.parts[ "a_base_hz" ] = r.a_base;
    s.parts[ "b_base_hz" ] = r.b_base;
    s.parts[ "a_hz" ] = r.a_resp;
    s.parts[ "b_hz" ] = r.b_resp;
    s.parts[ "a_after_hz" ] = r.a_after;
    s.parts[ "b_after_hz" ] = r.b_after;
    s.parts[ "backs_up" ] = backs_up() ? 1.0 : 0.0;
    const Setting st = setting();
    s.parts[ "silenced" ] = static_cast< double >( st.silenced.size() );
    s.parts[ "kept" ] = static_cast< double >( st.kept.size() );
  }

  double n_[ 6 ] = {}, pool_[ 2 ] = { 0, 0 };
};

}  // namespace spike::tasks::atlas_worm
