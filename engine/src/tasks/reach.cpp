// Task "reach": a fast, accurate elbow movement from a triphasic agonist - antagonist - agonist burst pattern.
//
// WORLD (deterministic; limb and loop as in spinal_limb.h: supported elbow-like joint, flexor and extensor Hill muscles, spindle Ia
// feedback with 10 + 10 ms conduction, pool total rate / (20 units x rmax) = excitation; no gravity).
//   Rest     theta_rest = 0.2 rad (both muscles at their optimal length). After each display a light manipulandum (a spring-damper toward
//            the rest angle, 30 N m/rad and 3 N m s/rad, limited to 8 N m) brings the forearm back during 1500 ms, so every reach starts at rest.
//   Sensors  "go" (dc): 100 for 40 ms at the moment the target appears (the go cue), else 0.
//            "target" (dc): 100 x (target angle - rest) in units per radian, i.e. 40 for a 0.4 rad reach, while the target is displayed
//            (1800 ms from the go cue), else 0 (the arm may relax and return).
//            "ag_Ia", "ant_Ia" (spindle Ia, Hz, dc), "drive" (tonic descending drive, dc, 1.0).
//   Motors   "ag_motor" (flexor pool), "ant_motor" (extensor pool).
//   Protocol three reaches, flexion, targets +0.40, +0.70 and +0.50 rad from rest (absolute 0.60, 0.90, 0.70 rad), go cues at
//            1500, 5300 and 9100 ms (+ U(0, 200) ms, seeded). Episode 13 300 ms.
//
// MEASURES per reach (A = target - rest, evaluated from the cue)
//   reach_ms     : time from the go cue until the angle first enters +-0.10 rad of the target and stays within +-0.16 rad for 200 ms.
//   overshoot    : (peak angle - target) / A, floored at 0, within 900 ms of the cue.
//   final_err    : |mean angle over the last 300 ms of the display - target|.
//   premature    : peak-to-peak angle in the 300 ms before the cue (the arm must not creep before "go").
// A reach is "good" when reach_ms <= 700 ms, overshoot <= 0.30 and final_err <= 0.15 rad.
// PASS: all three reaches good, premature <= 0.10 rad, wired, finished.
// VALUE: 100 * mean over reaches of ( 0.40 good + 0.20 (reach time score: 1 at <= 300 ms, 0 at >= 1000 ms)
//                                   + 0.20 (1 - overshoot / 0.5)+ + 0.20 (1 - final_err / 0.15)+ ), minus 10 per 0.05 rad of premature creep. PASS limit on creep: 0.10 rad (limb tremor at rest reaches 0.03-0.08).
//
// Biology: Wachholder & Altenburger 1926; Hallett, Shahani & Young 1975 and Berardelli et al. 1996 (triphasic agonist - antagonist - agonist
// EMG burst of fast ballistic movements; burst size and duration scale with the amplitude of the movement); Feldman 1986 (equilibrium-point
// control: a tonic command shifts the length at which the reflex holds the limb); Gottlieb, Corcos & Agarwal 1989.
#include <iomanip>
#include "spinal_limb.h"
#include "tasks.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_reach();
namespace
{

constexpr int kN = 3;
constexpr double kDuration = 13300, kRest = 0.2, kGoLen = 40, kShow = 1800, kSpacing = 3800, kLead = 1500, kJitter = 200;
constexpr double kAmp[ kN ] = { 0.40, 0.70, 0.50 };
constexpr double kReachLimit = 700, kWindow = 900, kReturn = 1500;

class ReachTask : public LimbTaskBase
{
public:
  ReachTask() { reset( 1 ); }
  std::string id() const override { return "reach"; }
  std::string title() const override { return "Reach"; }
  std::string brief() const override
  {
    return "Fast movements are not steered by feedback: the nervous system fires a triphasic burst, agonist then antagonist then "
           "agonist again (Hallett 1975). The first burst accelerates the limb, the antagonist burst brakes it, and a small "
           "final agonist burst clamps it on the target. Bigger reaches get bigger bursts. A target appears with a go cue: 'go' "
           "pulses for 40 ms and 'target' stays on at 100 x the distance in radians for 1.8 s. Build a pattern generator "
           "that starts on 'go', scales with 'target', and times the three bursts with delays (neurons that inhibit and delay "
           "each other), drive the flexor (ag_motor) and extensor (ant_motor) pools, and keep the stretch reflex to hold the limb at "
           "the new angle. Three reaches of 0.4, 0.7 and 0.5 rad. Score: reach the target within 700 ms of the cue, overshoot by "
           "less than 30% of the distance, finish within 0.15 rad, and do not creep before 'go'. A starting point (iaf_psc_alpha groups; pA; the "
           "sensors are dc inputs; passes on some seeds only, so expect to tune it): the stretch-reflex loop (Ia sensors at weight 3 onto relay "
           "groups of 6 with tau m 60 ms; pools of 20 cells; Ia > pool 160; Ia > inhibitory groups of 4 at 80; those > opposite pool -160) with the "
           "drive weight lowered to 160 so the pools are nearly silent at rest (a busy resting pool creeps); target (weight 2) onto a relay group of 3 "
           "(tau m 60) > ag_motor pool +800 and > ant_motor pool -320 (the hold command: the target shifts the balance, Feldman); go (weight 3) onto a "
           "relay group of 3 > burst group B1 (4 cells, tau syn 50 ms) 800 > ag_motor 1000 and ant_motor -400; B1 > B2 (4 cells, tau syn 50) "
           "150 with delay 150 ms (the target relay also drives B2 at 160) > ant_motor 200 (the brake), B2 > B3 (4 cells, tau syn 50) 400 with delay 100 ms > ag_motor 900 (the final clamp), "
           "each stage inhibiting the one before at -400. Delays go up to 250 ms and tau syn up to 50 ms.";
  }
  std::vector< std::string > sensor_channels() const override { return { "go", "target", "ag_Ia", "ant_Ia", "drive" }; }
  std::vector< std::string > motor_channels() const override { return { "ag_motor", "ant_motor" }; }
  double duration_ms() const override { return kDuration; }
  int par_neurons() const override { return 55; }

  std::vector< Event > timeline() const override
  {
    std::vector< Event > ev;
    for ( int k = 0; k < kN; ++k )
      ev.push_back( { go_[ k ], kShow, "reach to target" } );
    return ev;
  }
  std::vector< Band > bands() const override { return { { "theta_rad", "rad", kRest - 0.1, kRest + 0.9 } }; }

  void reset( uint64_t seed ) override
  {
    Rng rng( seed ^ 0x7265616368ULL );
    for ( int k = 0; k < kN; ++k )
      go_[ k ] = kLead + kSpacing * k + rng.uniform( 0, kJitter );
    LimbRigConfig c;
    c.rmax_hz = 60;  // 12 motoneurons per pool: 12 cells at 60 Hz = 36 % of the muscle's 20 units at 60 Hz; the muscle is fully excited only when the pool's total rate reaches 20 x 60 Hz
    base_reset( c, kRest );
    th_.clear();
  }

  Score score() const override
  {
    Score s;
    Metrics m = analyse();
    s.parts[ "reach_ms" ] = m.reach_ms;
    s.parts[ "overshoot" ] = m.overshoot;
    s.parts[ "final_err_rad" ] = m.final_err;
    s.parts[ "premature_rad" ] = m.premature;
    s.parts[ "worst_overshoot" ] = m.worst_over;
    s.parts[ "worst_final_err_rad" ] = m.worst_final;
    s.parts[ "worst_reach_ms" ] = m.worst_reach_ms;
    s.parts[ "good_reaches" ] = m.good;
    s.parts[ "reaches_done" ] = m.done;
    const bool done = t_ >= kDuration;
    s.value = m.value;
    s.passed = done && m.done >= kN && m.good >= kN && m.premature <= 0.10 && wiring_.ok();
    std::ostringstream o;
    o << std::fixed << std::setprecision( 0 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( m.done == 0 )
      o << "Waiting for the first target" << yet;
    else if ( m.premature > 0.10 )
      o << "The arm creeps " << std::setprecision( 2 ) << m.premature << " rad before the go cue (limit 0.10): the target input must not move it until 'go'.";
    else if ( m.good < m.done && m.never > 0 )
      o << "The arm does not reach the target: " << m.never << " of " << m.done << " reaches never settle within 0.10 rad.";
    else if ( m.good < m.done && m.worst_over > 0.30 )
      o << "Worst overshoot " << m.worst_over * 100 << "% of the distance (limit 30%): the braking (antagonist) burst is too weak or too late.";
    else if ( m.good < m.done && m.worst_reach_ms > kReachLimit )
      o << "The slowest reach takes " << m.worst_reach_ms << " ms (limit 700): the first (agonist) burst is too weak or too late.";
    else if ( m.good < m.done )
      o << "The worst reach ends " << std::setprecision( 2 ) << m.worst_final << " rad from the target (limit 0.15): the hold command does not match the target.";
    else
      o << ( done ? "All reaches good: " : "Good so far: " ) << m.reach_ms << " ms, overshoot " << m.overshoot * 100 << "%.";
    s.verdict = o.str();
    return s;
  }

protected:
  // Between reaches a light manipulandum brings the forearm back to the start (a spring-damper toward the rest angle, 30 N m/rad and
  // 3 N m s/rad, at most 8 N m) during 1500 ms after each display, so that every reach starts from rest.
  double torque( double t ) const override
  {
    for ( int k = 0; k < kN; ++k )
    {
      const double a = go_[ k ] + kShow;
      if ( t >= a && t < a + kReturn )
        return clampd( -30.0 * ( rig_.joint.theta - kRest ) - 3.0 * rig_.joint.omega, -8.0, 8.0 );
    }
    return 0.0;
  }
  void cues( Engine& e, double t ) override
  {
    put_cue( e, "drive", 1.0 );
    double go = 0, target = 0;
    for ( int k = 0; k < kN; ++k )
      if ( t >= go_[ k ] && t < go_[ k ] + kShow )
      {
        target = 100.0 * kAmp[ k ];
        if ( t < go_[ k ] + kGoLen )
          go = 100.0;
      }
    put_cue( e, "go", go );
    put_cue( e, "target", target );
  }
  void after_tick( double ) override { th_.push_back( rig_.joint.theta ); }
  bool has_target() const override
  {
    for ( int k = 0; k < kN; ++k )
      if ( t_ >= go_[ k ] && t_ < go_[ k ] + kShow )
        return true;
    return false;
  }
  double target_rad() const override
  {
    for ( int k = 0; k < kN; ++k )
      if ( t_ >= go_[ k ] && t_ < go_[ k ] + kShow )
        return kRest + kAmp[ k ];
    return kRest;
  }
  void extra_readouts( std::map< std::string, double >& r ) const override { r[ "err_rad" ] = rig_.joint.theta - target_rad(); }

private:
  struct Metrics
  {
    double reach_ms = -1, overshoot = 0, final_err = 0, premature = 0, value = 0;
    double worst_reach_ms = -1, worst_over = 0, worst_final = 0;  // the verdict compares the WORST reach with the per-reach limits
    int good = 0, done = 0, never = 0;
  };

  Metrics analyse() const
  {
    Metrics m;
    double sr = 0, so = 0, sf = 0, val = 0;
    int nr = 0;
    for ( int k = 0; k < kN; ++k )
    {
      const double t0 = go_[ k ];
      if ( t_ < t0 + kShow )
        break;
      ++m.done;
      const double tgt = kRest + kAmp[ k ], A = kAmp[ k ];
      // premature creep: how far the arm moves in the 300 ms before the cue
      double pre = 0, plo = 1e9, phi = -1e9;
      for ( long i = static_cast< long >( t0 - 300 ); i < static_cast< long >( t0 ); ++i )
        if ( i >= 0 )
          plo = std::min( plo, th_[ i ] ), phi = std::max( phi, th_[ i ] );
      if ( phi >= plo )
        pre = phi - plo;
      m.premature = std::max( m.premature, pre );
      // reach time
      double reach = -1;
      const long a = static_cast< long >( t0 ), b = std::min< long >( th_.size(), static_cast< long >( t0 + kShow ) - 200 );
      for ( long i = a; i < b; ++i )
        if ( std::fabs( th_[ i ] - tgt ) <= 0.10 )
        {
          bool stay = true;
          for ( long j = i; j < i + 200 && j < static_cast< long >( th_.size() ) && stay; ++j )
            stay = std::fabs( th_[ j ] - tgt ) <= 0.16;
          if ( stay )
          {
            reach = static_cast< double >( i - a );
            break;
          }
        }
      double peak = -1e9;
      for ( long i = a; i < a + static_cast< long >( kWindow ) && i < static_cast< long >( th_.size() ); ++i )
        peak = std::max( peak, th_[ i ] );
      const double over = std::max( 0.0, ( peak - tgt ) / A );
      const double fin = std::fabs( window_mean( th_, t0 + kShow - 300, t0 + kShow ) - tgt );
      const bool good = reach >= 0 && reach <= kReachLimit && over <= 0.30 && fin <= 0.15;
      if ( good )
        ++m.good;
      const double rt_s = reach >= 0 ? clampd( ( 1000.0 - reach ) / 700.0, 0, 1 ) : 0.0;
      val += 0.40 * good + 0.20 * rt_s + 0.20 * clampd( 1.0 - over / 0.5, 0, 1 ) + 0.20 * clampd( 1.0 - fin / 0.15, 0, 1 );
      m.worst_over = std::max( m.worst_over, over );
      m.worst_final = std::max( m.worst_final, fin );
      if ( reach < 0 )
        ++m.never;
      else
        m.worst_reach_ms = std::max( m.worst_reach_ms, reach );
      if ( reach >= 0 )
      {
        sr += reach;
        ++nr;
      }
      so += over;
      sf += fin;
    }
    if ( m.done > 0 )
    {
      m.overshoot = so / m.done;
      m.final_err = sf / m.done;
      m.value = 100.0 * val / kN - 10.0 * ( m.premature / 0.05 );
      if ( m.value < 0 )
        m.value = 0;
    }
    if ( nr > 0 )
      m.reach_ms = sr / nr;
    return m;
  }

  double go_[ kN ] = {};
  std::vector< double > th_;
};

} // namespace

std::unique_ptr< Task > make_reach() { return std::make_unique< ReachTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( reach, 130, "reach.json", make_reach ) }
