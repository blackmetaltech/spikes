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
           "less than 30% of the distance, finish within 0.15 rad, and do not creep before 'go'. Real motoneurons differ in size and in how strongly each is driven, so a pool is recruited gradually; identical cells all start together and the creep and the reach time then depend on the seed.";
  }
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A starting point (iaf_psc_alpha groups; pA; the sensors are dc inputs; it reaches on about half the seeds, so expect to tune it, and the "
           "braking and clamping bursts are yours to add): the stretch-reflex loop (Ia sensors at weight 3 onto relay groups of 6 with tau m 60 ms; "
           "pools of 20 cells; Ia > pool 160; Ia > inhibitory groups of 4 at 80; those > opposite pool -160) with the drive weight at 250; target "
           "(weight 3) onto a relay group of 3 (tau m 60) > ag_motor pool +1500 and > ant_motor pool -450 (the hold command: the target shifts the balance, "
           "Feldman); go (weight 3) onto a relay group of 3 > burst group B1 (4 cells, tau syn 50 ms) 600 > ag_motor 800 and ant_motor -400. A delayed "
           "second group B2 (B1 > B2 with a delay of 100-250 ms, tau syn 50 ms) > ant_motor is the brake against overshoot, and B2 > B3 > ag_motor the "
           "final clamp. Delays go up to 250 ms and tau syn up to 50 ms. The bursts are thresholded: a go weight of 650 instead of 600 doubles their size.";
  }
  std::vector< std::string > sensor_channels() const override { return { "go", "target", "ag_Ia", "ant_Ia", "drive" }; }
  std::vector< std::string > motor_channels() const override { return { "ag_motor", "ant_motor" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    return { { "go", in_kind( "dc", 100, 3 ) }, { "target", in_kind( "dc", 40, 3 ) }, { "ag_Ia", in_kind( "dc", 40, 3 ) }, { "ant_Ia", in_kind( "dc", 40, 3 ) }, { "drive", in_kind( "dc", 1, 250 ) } };
  }
  double duration_ms() const override { return kDuration; }
  /// Quick check: the first two reaches (0.4 and 0.7 rad), each shown for its whole display; the same rules as the full episode (a full run judges all three).
  double quick_duration_ms() const override { return kLead + kSpacing + kJitter + kShow; }
  int par_neurons() const override { return variant_par( variant_, 55 ); }

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }
  std::vector< std::string > concepts_used() const override { return { "excitation", "inhibition", "reciprocal_inhibition", "negative_feedback", "timed_bursts" }; }
  /// Off the spine until a novice bench passes it (lead decision, docs/CRITIQUE-0.3.md): the best recipe from an empty bench passes Standard on
  /// about half the seeds (docs/circuits/reach.md), the reference circuit on all.
  bool on_spine() const override { return false; }
  /// Pass limits (reach time / overshoot / final error / creep before the cue) scale by x1.4 (guided: 980 ms, 0.42, 0.21 rad, 0.14 rad),
  /// 1 (standard: 700 ms, 0.30, 0.15 rad, 0.10 rad) and x0.85 (researcher: 595 ms, 0.255, 0.1275 rad, 0.085 rad). The quick_reach variant asks 550 ms. (researcher factor x0.95: 665 ms, 0.285, 0.1425 rad, 0.095 rad)
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 28 instead of 55" },
             { "quick_reach", "reach each target within 550 ms of the cue" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    std::vector< Hint > h;
    if ( wiring_.ok() && mech_hints( h, "A fast reach is a triphasic burst: the agonist burst accelerates the limb, a later antagonist burst brakes it, and the reflex holds it on the target." ) )
      return h;
    if ( s.parts[ "premature_rad" ] > lim_creep() )
    {
      h.push_back( make_hint( 1, "A pattern generator must stay quiet until it is triggered: the pools should be nearly silent at rest.", "" ) );
      h.push_back( make_hint( 2, "The arm creeps before 'go': lower the tonic drive onto the pools.", "ag_motor" ) );
      h.push_back( make_hint( 3, "Lower the tonic drive weight until the pools are nearly silent at rest.", "" ) );
    }
    else if ( s.parts[ "worst_overshoot" ] > lim_over() )
    {
      h.push_back( make_hint( 1, "Timed bursts: the agonist burst accelerates, then a delayed antagonist burst brakes. Without the brake the limb overshoots.", "" ) );
      h.push_back( make_hint( 2, "The braking burst is too weak or too late.", "ant_motor" ) );
      h.push_back( make_hint( 3, "Delay the second burst group behind the first with a long link delay, make it excite the antagonist pool strongly, and let a third delayed group give the final clamp.", "" ) );
    }
    else if ( s.parts[ "good_reaches" ] < s.parts[ "reaches_done" ] && s.parts[ "worst_reach_ms" ] > lim_reach() )
    {
      h.push_back( make_hint( 1, "The first agonist burst sets the speed. It must be strong and start at the 'go' cue.", "" ) );
      h.push_back( make_hint( 2, "The first burst is too weak or late.", "ag_motor" ) );
      h.push_back( make_hint( 3, "Strengthen go > relay > B1 and B1 > ag_motor; the bursts are thresholded, so a small change of the go weight changes their size a lot.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "The target input must set where the arm holds after the bursts: equilibrium-point control.", "" ) );
      h.push_back( make_hint( 2, "The final angle does not match the target.", "target" ) );
      h.push_back( make_hint( 3, "Send target through a relay with a strong excitatory link onto ag_motor and a weaker inhibitory link onto ant_motor, then trim the balance until the final angle matches.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override { return { { "reach_ms", make_metric( score().parts[ "reach_ms" ], "ms", true ) } }; }
  Note notebook() const override
  {
    return { "Agonist, antagonist, agonist",
             "Fast, accurate arm movements are made by a triphasic burst: an agonist burst accelerates the limb, an antagonist burst "
             "brakes it, and a second agonist burst clamps it on the target. Bigger movements get bigger bursts.",
             "Hallett M, Shahani BT, Young RR (1975) J Neurol Neurosurg Psychiatry 38:1154; Berardelli A et al. (1996) Brain 119:661" };
  }

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
    const bool done = t_ >= quick_duration_ms() - 1e-6;  // the quick check's length: two reaches (a full run has three)
    const mechb::Issue* mech = mech_bad();
    s.value = m.value;
    if ( mech )
      s.value = std::min( s.value, 40.0 );  // behaviour without the mechanism the level teaches is not a solution (docs/REQ-0.4.md rule 1)
    s.passed = done && m.done >= 2 && m.good >= m.done && !mech && m.premature <= lim_creep() && wiring_.ok();
    std::ostringstream o;
    o << std::fixed << std::setprecision( 0 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( mech )
      o << mech->fail;
    else if ( m.done == 0 )
      o << "Waiting for the first target" << yet;
    else if ( m.premature > lim_creep() )
      o << "The arm creeps before the go cue: the target input and the tonic drive must not move it until 'go'.";
    else if ( m.good < m.done && m.never > 0 )
      o << "The arm does not reach the target: some reaches never settle close to it.";
    else if ( m.good < m.done && m.worst_over > lim_over() )
      o << "The arm overshoots the target too far: the braking (antagonist) burst is too weak or too late.";
    else if ( m.good < m.done && m.worst_reach_ms > lim_reach() )
      o << "The slowest reach is too slow: the first (agonist) burst is too weak or too late.";
    else if ( m.good < m.done )
      o << "The worst reach ends too far from the target: the hold command does not match the target.";
    else
      o << ( done ? "All reaches good: fast, with little overshoot, and settling on the target." : "Good so far." );
    s.verdict = o.str();
    return s;
  }

protected:
  void mechanism( const mechb::Graph& g, std::vector< mechb::Issue >& out ) const override { mechb::limb_burst( g, out ); }
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
  double tf() const { return mode_factor( mode_, 1.4, 0.95 ); }
  double lim_reach() const { return variant_ == "quick_reach" ? 550.0 : kReachLimit * tf(); }
  double lim_over() const { return 0.30 * tf(); }
  double lim_final() const { return 0.15 * tf(); }
  double lim_creep() const { return 0.10 * tf(); }
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
      const bool good = reach >= 0 && reach <= lim_reach() && over <= lim_over() && fin <= lim_final();
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
      m.value = 100.0 * val / std::max( m.done, 2 ) - 10.0 * ( m.premature / 0.05 );
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
