// Shared plumbing of the compass chapter of the Atlas wing (chapter "a_compass"; docs/ROADMAP-CONNECTOME.md, iteration A3; research/atlas-a.md 4c and 4d):
// a_signs, a_places, a_gain and dx_a_compass run the cue-and-hold world below on the measured wiring of the fly compass (data/connectome/hemibrain/compass.json, hemibrain v1.2.1,
// Scheffer et al. 2020), circuits made by godot/circuits/tools/gen_atlas_compass.py.
//
// THE MODEL (what the circuits hold; "in this model", never "in the fly"): leaky integrate-and-fire cells, weight = sign x synapse count x one gain per cell-type pair (the fitted
// setting of data/connectome/hemibrain/compass_setting.json), a background current per type. The wiring holds a bump where it was put at most cues, rests at about six headings and
// does not turn reliably (research/atlas-a.md 4c): there is no turning level.
//
// WORLD: a run is a row of TRIALS. A trial is 3700 ms: a reset (the sensor "clear" holds a negative current on every cell, 0 to 200 ms), a cue (the sensor cue<k> is a Poisson
// train, 300 Hz, on the E-PG cells of heading k-1, k and k+1, 200 to 700 ms), then nothing. The motors h0 to h15 read the E-PG cells of each of the sixteen headings (22.5 degrees each).
// WHAT HOLDS, as in the study (tools/connectome/compass_trial.held): over the last 500 ms of the trial (3000 to 3500 ms after the cue starts) the rate per cell at each heading gives
// a population vector; the bump HOLDS where it was put when its angle is within 30 degrees of the cue, its strength is above 0.5, at least 0.7 of the spikes are within two headings of
// the peak and the mean rate per heading is between 2 and 60 Hz. A RESTING PLACE is the heading nearest to the angle of a trial's bump when the trial is active (mean rate 0.3 Hz or more).
// The verdicts say what the measured bumps show in words, never a number (a_places says its counts in words).
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <set>
#include "atlas_worm.h"

namespace spike::tasks::atlas_compass
{
constexpr int kHeads = 16;
constexpr double kClearMs = 200, kCueMs = 500, kWinLo = 3000, kWinHi = 3500, kTrialMs = kClearMs + kWinHi, kCueHz = 300;

inline std::string head_name( int k ) { return "h" + std::to_string( k ); }
inline std::string cue_name( int k ) { return "cue" + std::to_string( k ); }
inline double wrap180( double d )
{
  d = std::fmod( d + 180.0, 360.0 );
  if ( d < 0 ) d += 360.0;
  return d - 180.0;
}
/// A count in words, 0 to 20 (the verdicts carry no digit).
inline std::string number_word( int n )
{
  static const char* w[] = { "no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen", "fifteen", "sixteen", "seventeen", "eighteen", "nineteen", "twenty" };
  return n >= 0 && n <= 20 ? w[ n ] : "many";
}

/// The cues of the levels (headings; research/atlas-a.md 4d). a_signs and a_gain: cues at resting places that hold on every seed. a_places: all sixteen.
/// MEASUREMENT HOOK (removed before the merge): SPIKE_A_CUES="3,7,..." replaces the cues of every level of the chapter, so the study of research/atlas-a.md 4d can run any cue on the
/// level's own world.
inline std::vector< int > cues_or( std::vector< int > def )
{
  const char* v = std::getenv( "SPIKE_A_CUES" );
  if ( !v || !*v ) return def;
  std::vector< int > r;
  for ( const char* p = v; *p; )
  {
    r.push_back( std::atoi( p ) % kHeads );
    while ( *p && *p != ',' ) ++p;
    if ( *p == ',' ) ++p;
  }
  return r;
}
inline const std::vector< int >& signs_cues() { static const std::vector< int > c = cues_or( { 4, 11, 6, 15, 8 } ); return c; }
inline const std::vector< int >& gain_cues() { static const std::vector< int > c = cues_or( { 4, 11 } ); return c; }
inline const std::vector< int >& places_cues()
{
  static const std::vector< int > c = cues_or( { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } );
  return c;
}

/// What one finished trial showed.
struct Bump
{
  bool any = false, held = false, active = false;
  double angle = -1, strength = 0, near = 0, mean_hz = 0, err = 360;
  int rest = -1;
  int kind = 1;   // 0 holds, 1 died (the rate is 2 Hz or less), 2 spread (too strong a rate, or no compact bump), 3 moved (a compact bump, but not where the cue was)
};
inline Bump analyse( const std::array< double, kHeads >& prof, int cue )
{
  Bump b;
  double tot = 0;
  for ( double p : prof ) tot += p;
  b.mean_hz = tot / kHeads;
  b.active = b.mean_hz >= 0.3;
  if ( tot <= 0 ) return b;
  const double pi = std::acos( -1.0 );
  double x = 0, y = 0;
  for ( int k = 0; k < kHeads; ++k )
  {
    x += prof[ static_cast< size_t >( k ) ] * std::cos( 22.5 * k * pi / 180.0 );
    y += prof[ static_cast< size_t >( k ) ] * std::sin( 22.5 * k * pi / 180.0 );
  }
  b.any = true;
  b.angle = std::fmod( std::atan2( y, x ) * 180.0 / pi + 360.0, 360.0 );
  b.strength = std::hypot( x, y ) / tot;
  int pk = 0;
  for ( int k = 1; k < kHeads; ++k )
    if ( prof[ static_cast< size_t >( k ) ] > prof[ static_cast< size_t >( pk ) ] ) pk = k;
  double near = 0;
  for ( int d = -2; d <= 2; ++d ) near += prof[ static_cast< size_t >( ( pk + d + kHeads ) % kHeads ) ];
  b.near = near / tot;
  b.err = std::fabs( wrap180( b.angle - 22.5 * cue ) );
  b.rest = static_cast< int >( std::lround( b.angle / 22.5 ) ) % kHeads;
  b.held = b.err < 30 && b.strength > 0.5 && b.near > 0.7 && b.mean_hz > 2 && b.mean_hz < 60;
  if ( b.held ) b.kind = 0;
  else if ( b.mean_hz <= 2 ) b.kind = 1;
  else if ( b.mean_hz >= 60 || b.strength <= 0.5 || b.near <= 0.7 ) b.kind = 2;
  else b.kind = 3;
  return b;
}

/// The cue-and-hold world on the compass circuit and what it measures.
class CompassRun : public micro::MicroBase
{
public:
  CompassRun( Info i, std::vector< int > cues ) : MicroBase( std::move( i ) ), cues_( std::move( cues ) ) { zero(); }

  static Info world( const std::string& id, const std::string& title, const std::vector< int >& cues )
  {
    Info i;
    i.id = id;
    i.title = title;
    i.chapter = "a_compass";
    for ( int k = 0; k < kHeads; ++k ) i.sensors.push_back( cue_name( k ) );
    i.sensors.push_back( "clear" );
    for ( int k = 0; k < kHeads; ++k ) i.motors.push_back( head_name( k ) );
    i.duration_ms = kTrialMs * static_cast< double >( cues.size() );
    i.par_neurons = 148;
    i.default_model = "iaf_psc_alpha";
    for ( int k = 0; k < kHeads; ++k )
    {
      Task::ChannelKind cue;
      cue.kind = "poisson";
      cue.value = kCueHz;
      cue.weight = 400;
      i.kinds[ cue_name( k ) ] = cue;
      i.kinds[ head_name( k ) ] = micro::rate();
    }
    i.kinds[ "clear" ] = micro::dc( 1.0, -1000.0 );
    i.bands = { { "ring_hz", "Hz", 2, 60 } };
    for ( size_t t = 0; t < cues.size(); ++t )
    {
      const double t0 = kTrialMs * static_cast< double >( t );
      i.timeline.push_back( { t0, kClearMs, "reset" } );
      i.timeline.push_back( { t0 + kClearMs, kCueMs, "cue at " + std::to_string( static_cast< int >( 22.5 * cues[ t ] ) ) + " deg" } );
    }
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    zero();
  }

  const std::vector< int >& cues() const { return cues_; }
  bool trial_done( size_t i ) const { return t_ >= kTrialMs * static_cast< double >( i ) + kClearMs + kWinHi - 1e-6; }
  /// The finished trials in order (a trial that has not reached the end of its window is not listed).
  std::vector< Bump > bumps() const
  {
    std::vector< Bump > v;
    for ( size_t i = 0; i < cues_.size(); ++i )
    {
      if ( !trial_done( i ) ) break;
      std::array< double, kHeads > prof{};
      for ( int k = 0; k < kHeads; ++k )
      {
        const double pool = pool_[ static_cast< size_t >( k ) ];
        prof[ static_cast< size_t >( k ) ] = pool > 0 ? cnt_[ i ][ static_cast< size_t >( k ) ] / pool / ( ( kWinHi - kWinLo ) / 1000.0 ) : 0.0;
      }
      v.push_back( analyse( prof, cues_[ i ] ) );
    }
    return v;
  }
  /// Every finished trial holds its bump where the cue put it.
  bool all_hold() const
  {
    const auto v = bumps();
    if ( v.size() < cues_.size() ) return false;
    return std::all_of( v.begin(), v.end(), []( const Bump& b ) { return b.held; } );
  }
  /// The number of different resting headings over the active trials.
  int places() const
  {
    std::set< int > s;
    for ( const auto& b : bumps() )
      if ( b.active && b.rest >= 0 ) s.insert( b.rest );
    return static_cast< int >( s.size() );
  }
  /// What the first trial that did not hold showed, in words ("" when all hold).
  std::string failure() const
  {
    for ( const auto& b : bumps() )
    {
      if ( b.held ) continue;
      if ( b.kind == 1 ) return "After a cue the activity dies out: by the end of the trial the ring is quiet.";
      if ( b.kind == 2 ) return "After a cue the activity spreads over the ring instead of staying a compact bump.";
      return "After a cue the bump forms but settles at another heading than the one it was put at.";
    }
    return "";
  }
  /// The state of a cell-type group's sign (+1 / -1), 0 when the card is missing.
  int sign_of( const std::string& group ) const
  {
    for ( const auto& g : g_.st.groups )
      if ( g.id == group ) return g.sign;
    return 0;
  }

protected:
  void zero()
  {
    cnt_.assign( cues_.size(), std::array< double, kHeads >{} );
    pool_.fill( 0 );
    step_.fill( 0 );
    ring_ = 0;
  }
  void drive( Engine& e, double ) override
  {
    const double trial = std::floor( t_ / kTrialMs + 1e-9 );
    const size_t i = static_cast< size_t >( trial );
    int cue = -1;
    bool clear = false;
    if ( i < cues_.size() )
    {
      const double u = t_ - trial * kTrialMs;
      clear = u < kClearMs - 1e-9;
      if ( !clear && u < kClearMs + kCueMs - 1e-9 ) cue = cues_[ i ];
    }
    e.set_input( "clear", clear ? 1.0 : 0.0 );
    for ( int k = 0; k < kHeads; ++k ) e.set_input( cue_name( k ), k == cue ? kCueHz : 0.0 );
  }
  void observe( Engine& e, double dt ) override
  {
    double total = 0, cells = 0;
    for ( int k = 0; k < kHeads; ++k )
    {
      const size_t s = static_cast< size_t >( k );
      pool_[ s ] = motor_.pool_size( e, head_name( k ) );
      step_[ s ] = motor_.spike_count( e, head_name( k ), dt );
      total += step_[ s ];
      cells += pool_[ s ];
    }
    ring_ += ( ( cells > 0 ? total / cells / ( dt / 1000.0 ) : 0.0 ) - ring_ ) * std::min( 1.0, dt / 100.0 );
    readouts_[ "ring_hz" ] = ring_;
    for ( size_t i = 0; i < cues_.size(); ++i )
    {
      const double lo = kTrialMs * static_cast< double >( i ) + kClearMs + kWinLo, hi = kTrialMs * static_cast< double >( i ) + kClearMs + kWinHi;
      if ( t_ > lo + 1e-9 && t_ <= hi + 1e-9 )
        for ( size_t k = 0; k < static_cast< size_t >( kHeads ); ++k ) cnt_[ i ][ k ] += step_[ k ];
    }
  }
  void fill_parts( Score& s ) const
  {
    const auto v = bumps();
    int held = 0;
    for ( size_t i = 0; i < v.size(); ++i )
    {
      const std::string p = "t" + std::to_string( i ) + "_";
      s.parts[ p + "cue" ] = cues_[ i ];
      s.parts[ p + "held" ] = v[ i ].held ? 1.0 : 0.0;
      s.parts[ p + "angle" ] = v[ i ].angle;
      s.parts[ p + "strength" ] = v[ i ].strength;
      s.parts[ p + "near" ] = v[ i ].near;
      s.parts[ p + "hz" ] = v[ i ].mean_hz;
      s.parts[ p + "rest" ] = v[ i ].active ? v[ i ].rest : -1;
      s.parts[ p + "kind" ] = v[ i ].kind;
      held += v[ i ].held ? 1 : 0;
    }
    s.parts[ "n_trials" ] = static_cast< double >( cues_.size() );
    s.parts[ "n_done" ] = static_cast< double >( v.size() );
    s.parts[ "n_held" ] = held;
    s.parts[ "places" ] = places();
    s.parts[ "sign_EPG" ] = sign_of( "EPG" );
    s.parts[ "sign_PEG" ] = sign_of( "PEG" );
    s.parts[ "sign_D7" ] = sign_of( "D7" );
    for ( const auto& g : g_.st.groups )
      if ( g.id == "D7" ) s.parts[ "scale_D7" ] = g.scale;
  }

  std::vector< int > cues_;
  std::vector< std::array< double, kHeads > > cnt_;
  std::array< double, kHeads > pool_{}, step_{};
  double ring_ = 0;
};

/// The score of a level that asks the bump to hold at every cue (a_signs, a_gain).
inline void score_holds( const CompassRun& r, bool done, bool wired, const std::string& wiring_msg, const std::string& mech, Score& s, const std::string& pass_text, const std::string& tail )
{
  const auto v = r.bumps();
  int held = 0;
  for ( const auto& b : v ) held += b.held ? 1 : 0;
  const double frac = r.cues().empty() ? 0.0 : static_cast< double >( held ) / static_cast< double >( r.cues().size() );
  s.passed = done && mech.empty() && wired && r.all_hold();
  s.value = !done ? 0.0 : ( s.passed ? 100.0 : 30.0 + 50.0 * frac );
  if ( !wired ) s.verdict = "Not wired: " + wiring_msg + ".";
  else if ( !done ) s.verdict = "Running.";
  else if ( !mech.empty() ) s.verdict = mech;
  else if ( s.passed ) s.verdict = pass_text;
  else s.verdict = r.failure() + tail;
}

}  // namespace spike::tasks::atlas_compass
