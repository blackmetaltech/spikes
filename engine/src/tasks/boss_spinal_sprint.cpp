// Task "boss_spinal_sprint" (chapter finale of the spinal chapter): one spinal motor system, four jobs at once. Hold the arm up against gravity,
// stand firm when it is pushed, reach to a new target on a go cue, and hold THERE under more pushes, without ringing or creeping.
//
// WORLD (deterministic; limb and loop as in spinal_limb.h and posture.cpp: elbow-like joint, flexor and extensor Hill muscles, spindle Ia / II
// feedback with 10 + 10 ms conduction, pool total rate / (20 units x rmax) = excitation; gravity ON: forearm 1.0 kg, centre of mass 17 cm,
// 1.7 N m at horizontal; theta = 0 is horizontal, positive = raised). Episode 40 s.
//   Base posture  theta_0 = 0.5 rad. The limb starts there.
//   Sensors  "ag_Ia", "ant_Ia", "ag_II", "ant_II" (spindle primary / secondary, Hz, dc inputs), "drive" (tonic descending drive, dc, 1.0),
//            "go" (dc: 100 for 40 ms at the go cue) and "target" (dc: 100 x reach distance in rad while the target is displayed, 2300 ms).
//   Motors   "ag_motor", "ant_motor" (alpha pools), "gamma_motor" (gamma motoneurons: fusimotor drive, as in posture).
//   Schedule (+ U(0, 150) ms seeded jitter on every onset): pushes of 1.0-1.6 N m, 250 ms, random sign, at 2200 and 4200 ms (posture holding);
//            REACH 1 go at 6500 ms (+0.30 rad), push at 7700 ms (holding the new target); REACH 2 go at 11000 ms (+0.40 rad), push at 12500 ms;
//            push at 15000 ms (back at base posture); REACH 3 go at 17500 ms (+0.25 rad), pushes at 19000 and 20800 ms; two more pushes at
//            24000 and 28000 ms (calm, base); the episode ends at 40 s with a last quiet hold (scoring ends at 38 s).
//            After the display (2300 ms from the go cue) the target input falls to 0 and the limb must come back to the base posture.
//
// MEASURES (err = theta - the target that is displayed now, or theta_0 when none)
//   hold_rms_rad, hold_max_rad : rms and the largest |err| over the whole run from 1500 ms, except 900 ms after each go cue and after each display ends
//                                (the movement itself, judged by the reach measures).
//   reach_ms     : as in the reach task: time from the go cue until the angle enters +-0.10 rad of the target and stays within +-0.16 rad for 200 ms.
//   overshoot    : (peak - target) / distance, floored 0, within 900 ms of the cue.
//   final_err    : |mean angle over the last 300 ms of the display - target|.
//   ring_rad     : peak-to-peak angle in the 900-1600 ms window after each push; a swing above 0.08 rad with >= 6 reversals is a sustained oscillation (clonus).
//   creep_rad    : peak-to-peak angle in the 300 ms before each go cue (the arm must not move before 'go').
// A reach is good when reach_ms <= 800, overshoot <= 0.65 and final_err <= 0.20 (overshoot is relative to the distance; the base posture itself
// wanders +-0.15 rad between seeds, which is why these limits are wider than Reach's).
// PASS: hold_rms <= 0.23, hold_max <= 0.40, all three reaches good, no sustained oscillation, creep <= 0.10, wired, finished.
// MODES: the limits above are the standard ones; guided multiplies the tolerances by 1.25, researcher by 0.85 (structure never changes).
// VALUE: 100 x ( 0.30 posture (rms, max) + 0.40 reach (good, time, overshoot, final error) + 0.15 steadiness (no ringing, no creep) + 0.15 reflex
//                resistance to the pushes during the reach holds ) , posture and reach parts as in posture.cpp / reach.cpp.
//
// Biology: stretch reflex and spindle fusimotor drive hold posture (Matthews 1972; Nichols & Houk 1976; Houk 1979), Renshaw cells stabilise the
// motor pool (Hultborn 2006), the triphasic burst generates fast movement (Hallett, Shahani & Young 1975; Berardelli et al. 1996) and the
// equilibrium-point command shifts the length at which the reflex holds the limb (Feldman 1986). A fast reach has to work with the reflex, not
// against it: the stretch reflex that holds the arm against the push also resists the reach, so a circuit that only holds, or only reaches, fails.
#include <iomanip>
#include "spinal_limb.h"
#include "tasks.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_boss_spinal_sprint();
namespace
{

constexpr double kDuration = 40000, kBase = 0.5, kRamp = 20, kHold = 250, kGoLen = 40, kShow = 2300, kJitter = 150;
constexpr double kScoreFrom = 1500, kScoreTo = 38000, kQuiet0 = 900, kQuiet1 = 1600, kMoveGrace = 900;
constexpr int kNP = 9, kNR = 3;
constexpr double kPushT[ kNP ] = { 2200, 4200, 7700, 12500, 15000, 19000, 20800, 24000, 28000 };
constexpr double kGoT[ kNR ] = { 6500, 11000, 17500 };
constexpr double kAmp[ kNR ] = { 0.30, 0.40, 0.25 };

class SprintTask : public LimbTaskBase
{
public:
  SprintTask() { reset( 1 ); }
  std::string id() const override { return "boss_spinal_sprint"; }
  std::string title() const override { return "Spinal sprint"; }
  std::string brief() const override
  {
    return "The spinal motor system does four jobs with the same few neurons. It holds the arm up against gravity (spindle loop, gamma drive and "
           "tonic descending drive), it stands firm when someone pushes (stretch reflex with reciprocal inhibition, Renshaw cells to keep the "
           "pool from running away), it reaches to a new target fast (a triphasic agonist - antagonist - agonist burst started by 'go', scaled by "
           "'target'), and it holds the new angle under more pushes. Nothing in the world is new: the arm is the one from Posture and the "
           "reach is the one from Reach. What is new is that every part must work with the others: the reflex that holds the arm against a push "
           "also resists the reach, and the burst must not set the arm ringing. Wire the posture circuit (spindles, gamma, tone), add the "
           "burst generator for 'go' and 'target', and add Renshaw cells to both pools. Score: how well the arm is held (rms and worst error "
           "against the target that is displayed), how well it reaches (time, overshoot, final error, three reaches of 0.30, 0.40 and 0.25 rad), "
           "no ringing after a push, and no creeping before 'go'. 40 s long: pushes arrive before, during and after the reaches.";
  }
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A starting point (the numbers are those of the Posture, Reach and Renshaw recipes): the Posture circuit with the Reach generator added and "
           "Renshaw cells on both pools; expect to re-tune the hold command.";
  }
  std::vector< std::string > sensor_channels() const override
  {
    return { "ag_Ia", "ant_Ia", "ag_II", "ant_II", "drive", "go", "target" };
  }
  std::vector< std::string > motor_channels() const override { return { "ag_motor", "ant_motor", "gamma_motor" }; }
  double duration_ms() const override { return kDuration; }
  int par_neurons() const override { return 80; }
  std::string level_kind() const override { return "boss"; }
  bool on_spine() const override { return true; }
  std::vector< std::string > concepts_used() const override
  {
    return { "recruitment", "reciprocal_inhibition", "recurrent_inhibition", "negative_feedback", "timed_bursts" };
  }

  std::vector< Event > timeline() const override
  {
    std::vector< Event > ev;
    for ( int k = 0; k < kNP; ++k )
      ev.push_back( { push_t_[ k ], 2 * kRamp + kHold, push_amp_[ k ] > 0 ? "push up" : "push down" } );
    for ( int k = 0; k < kNR; ++k )
      ev.push_back( { go_[ k ], kShow, "reach to target" } );
    std::sort( ev.begin(), ev.end(), []( const Event& a, const Event& b ) { return a.t_ms < b.t_ms; } );
    return ev;
  }
  // theta band: the whole range the arm should stay in (the base posture +-0.15 rad and the highest target +0.40)
  std::vector< Band > bands() const override
  {
    return { { "theta_rad", "rad", kBase - 0.15, kBase + 0.55 }, { "err_rad", "rad", -0.15, 0.15 } };
  }

  void reset( uint64_t seed ) override
  {
    Rng rng( seed ^ 0x737072696E74ULL );
    for ( int k = 0; k < kNP; ++k )
    {
      push_t_[ k ] = kPushT[ k ] + rng.uniform( 0, kJitter );
      const double a = rng.uniform( 1.0, 1.6 );
      push_amp_[ k ] = rng.uniform() < 0.5 ? a : -a;
    }
    for ( int k = 0; k < kNR; ++k )
      go_[ k ] = kGoT[ k ] + rng.uniform( 0, kJitter );
    LimbRigConfig c;
    c.rmax_hz = 100;
    c.joint.mass_kg = kMass;
    c.joint.spindle.ia_bias = 10;
    c.joint.spindle.ii_bias = 10;
    c.joint.spindle.gamma_sta_bias = 80;
    base_reset( c, kBase );
    th_.clear();
  }

  Score score() const override
  {
    Score s;
    Metrics m = analyse();
    const bool done = t_ >= kDuration;
    s.parts[ "hold_rms_rad" ] = m.rms;
    s.parts[ "hold_max_rad" ] = m.mx;
    s.parts[ "reach_ms" ] = m.reach_ms;
    s.parts[ "worst_reach_ms" ] = m.worst_reach_ms;
    s.parts[ "overshoot" ] = m.worst_over;
    s.parts[ "worst_final_err_rad" ] = m.worst_final;
    s.parts[ "good_reaches" ] = m.good;
    s.parts[ "reaches_done" ] = m.done;
    s.parts[ "final_signed_rad" ] = m.done > 0 ? m.signed_final / m.done : 0.0;
    s.parts[ "ring_rad" ] = m.osc;
    s.parts[ "creep_rad" ] = m.creep;
    s.parts[ "push_hold_max_rad" ] = m.push_hold_max;
    s.parts[ "gamma" ] = rig_.gam;
    const double post = 0.6 * clampd( 1.0 - m.rms / 0.35, 0, 1 ) + 0.4 * clampd( 1.0 - m.mx / 0.6, 0, 1 );
    const double reach = m.done > 0 ? m.reach_value / kNR : 0.0;
    const double steady = ( m.osc_flag ? 0.0 : 0.6 ) + 0.4 * clampd( 1.0 - m.creep / 0.2, 0, 1 );
    const double resist = clampd( 1.0 - m.push_hold_max / 0.6, 0, 1 );
    s.value = 100.0 * ( 0.30 * post + 0.40 * reach + 0.15 * steady + 0.15 * resist );
    if ( !wiring_.ok() )
      s.value = std::min( s.value, 20.0 );
    s.passed = done && wiring_.ok() && m.done >= kNR && m.good >= kNR && m.rms <= 0.23 * tol() && m.mx <= 0.40 * tol() && !m.osc_flag && m.creep <= 0.10;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 2 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( t_ < kScoreFrom )
      o << "Settling" << yet;
    else if ( m.osc_flag )
      o << "The arm rings after a push (" << m.osc << " rad p-p): the loop gain is too high for its delays (more Renshaw damping, or less reflex gain).";
    else if ( m.creep > 0.10 )
      o << "The arm creeps " << m.creep << " rad before the go cue (limit 0.10): the tonic drive or the target input moves it early.";
    else if ( m.done > 0 && m.good < m.done && m.never > 0 )
      o << "The arm does not reach the target: " << m.never << " of " << m.done << " reaches never settle within 0.10 rad (the reflex or the hold command fights the burst).";
    else if ( m.done > 0 && m.good < m.done && m.worst_over > 0.65 * tol() )
      o << "Worst overshoot " << std::setprecision( 0 ) << m.worst_over * 100 << "% of the distance (limit " << 65 * tol() << "%): the braking (antagonist) burst is too weak or too late.";
    else if ( m.done > 0 && m.good < m.done && m.worst_reach_ms > 800 * tol() )
      o << "The slowest reach takes " << std::setprecision( 0 ) << m.worst_reach_ms << " ms (limit " << 800 * tol() << "): the first burst is too weak, or the stretch reflex resists it.";
    else if ( m.done > 0 && m.good < m.done )
      o << "The worst reach ends " << m.worst_final << " rad from the target (limit " << 0.20 * tol() << "): the hold command does not match the target under gravity.";
    else if ( m.mx > 0.40 * tol() )
      o << "Pushes move the arm up to " << m.mx << " rad from its target (limit " << 0.40 * tol() << "): too compliant. Raise gamma drive or reflex gain.";
    else if ( m.rms > 0.23 * tol() )
      o << "RMS error from the target " << m.rms << " rad (limit " << 0.23 * tol() << "): the arm sags or drifts between pushes.";
    else
      o << ( done ? "Held and reached: " : "Good so far: " ) << "rms " << m.rms << " rad, worst " << m.mx << " rad, reaches " << std::setprecision( 0 )
        << m.reach_ms << " ms.";
    s.verdict = o.str();
    return s;
  }

  std::map< std::string, Metric > metrics() const override
  {
    Metrics m = analyse();
    return { { "hold_error", Metric{ m.rms, "rad", true } }, { "reach_time", Metric{ m.reach_ms, "ms", true } } };
  }

  std::vector< Hint > hints() const override
  {
    Metrics m = analyse();
    std::vector< Hint > h;
    if ( m.osc_flag )
    {
      h.push_back( { 1, "A ringing arm: a feedback loop with too much gain for its delays. What damps a motor pool from within?", "" } );
      h.push_back( { 2, "Look at the Renshaw cells of both pools.", "RC_F" } );
      h.push_back( { 3, "Give each pool a few Renshaw cells: pool > RC excitatory, RC > pool inhibitory, RC > the Ia interneuron of the same side.", "RC_F" } );
    }
    else if ( m.done > 0 && m.good < m.done )
    {
      h.push_back( { 1, "A movement needs a burst pattern (accelerate, brake, clamp) AND a hold command that matches the target.", "" } );
      h.push_back( { 2, "Look at the burst generator and the target relay.", "B1" } );
      h.push_back( { 3, "'go' starts B1 > B2 > B3 with delays; 'target' scales every burst and shifts the tonic balance (flexor up, extensor down).", "TGT" } );
    }
    else
    {
      h.push_back( { 1, "Holding a limb up needs spindles that are tuned (gamma drive) and a tonic drive that balances gravity.", "" } );
      h.push_back( { 2, "Look at the gamma pool and the tonic cells.", "GAM" } );
      h.push_back( { 3, "Drive TONIC > MN_F stronger than TONIC > MN_E, and TONIC > GAM so the spindles listen.", "TONIC" } );
    }
    return h;
  }

  Note notebook() const override
  {
    return { "The reflex does not know about your plans",
             "A stretch reflex resists any lengthening of a muscle, including the lengthening of the antagonist that a voluntary movement causes. Fast "
             "movements are launched by a pre-planned burst and then caught by the reflex at the new length: the command sets where the reflex holds "
             "the limb (the equilibrium point), it does not fight it.",
             "Feldman AG (1986) J Mot Behav 18:17; Hallett M, Shahani BT, Young RR (1975) J Neurol Neurosurg Psychiatry 38:1154; Hultborn H (2006) Prog Neurobiol 78:215" };
  }

protected:
  double torque( double t ) const override
  {
    double tq = 0;
    for ( int k = 0; k < kNP; ++k )
      tq += push_amp_[ k ] * push_profile( t, push_t_[ k ], kHold, kRamp );
    return tq;
  }
  void cues( Engine& e, double t ) override
  {
    put_cue( e, "drive", 1.0 );
    double go = 0, target = 0;
    for ( int k = 0; k < kNR; ++k )
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
  bool has_target() const override { return true; }
  double target_rad() const override { return target_at( t_ ); }
  void extra_readouts( std::map< std::string, double >& r ) const override { r[ "err_rad" ] = rig_.joint.theta - target_at( t_ ); }

private:
  static constexpr double kMass = 1.0;
  double tol() const { return mode_ == "guided" ? 1.25 : ( mode_ == "researcher" ? 0.85 : 1.0 ); }
  struct Metrics
  {
    double rms = 0, mx = 0, signed_final = 0, osc = 0, creep = 0, reach_ms = -1, worst_reach_ms = -1, worst_over = 0, worst_final = 0, reach_value = 0, push_hold_max = 0;
    int good = 0, done = 0, never = 0, osc_flag = 0;
  };

  double target_at( double t ) const
  {
    for ( int k = 0; k < kNR; ++k )
      if ( t >= go_[ k ] && t < go_[ k ] + kShow )
        return kBase + kAmp[ k ];
    return kBase;
  }
  // true while the arm is moving to a new target or coming back from one (not judged as a hold)
  bool in_move( double t ) const
  {
    for ( int k = 0; k < kNR; ++k )
      if ( ( t >= go_[ k ] && t < go_[ k ] + kMoveGrace ) || ( t >= go_[ k ] + kShow && t < go_[ k ] + kShow + kMoveGrace ) )
        return true;
    return false;
  }

  Metrics analyse() const
  {
    Metrics m;
    const long a = static_cast< long >( kScoreFrom ), b = std::min< long >( th_.size(), static_cast< long >( kScoreTo ) );
    if ( b - a < 100 )
      return m;
    double ss = 0;
    long n = 0;
    for ( long i = a; i < b; ++i )
    {
      if ( in_move( static_cast< double >( i ) ) )
        continue;
      const double e = th_[ i ] - target_at( static_cast< double >( i ) );
      ss += e * e;
      ++n;
      m.mx = std::max( m.mx, std::fabs( e ) );
    }
    if ( n > 0 )
      m.rms = std::sqrt( ss / n );
    // reaches
    double sr = 0;
    int nr = 0;
    for ( int k = 0; k < kNR; ++k )
    {
      const double t0 = go_[ k ];
      if ( t_ < t0 + kShow )
        break;
      ++m.done;
      const double tgt = kBase + kAmp[ k ], A = kAmp[ k ];
      double plo = 1e9, phi = -1e9;
      for ( long i = static_cast< long >( t0 - 300 ); i < static_cast< long >( t0 ); ++i )
        if ( i >= 0 )
          plo = std::min( plo, th_[ i ] ), phi = std::max( phi, th_[ i ] );
      if ( phi >= plo )
        m.creep = std::max( m.creep, phi - plo );
      double reach = -1;
      const long a0 = static_cast< long >( t0 ), b0 = std::min< long >( th_.size(), static_cast< long >( t0 + kShow ) - 200 );
      for ( long i = a0; i < b0; ++i )
        if ( std::fabs( th_[ i ] - tgt ) <= 0.10 )
        {
          bool stay = true;
          for ( long j = i; j < i + 200 && j < static_cast< long >( th_.size() ) && stay; ++j )
            stay = std::fabs( th_[ j ] - tgt ) <= 0.16;
          if ( stay )
          {
            reach = static_cast< double >( i - a0 );
            break;
          }
        }
      double peak = -1e9;
      for ( long i = a0; i < a0 + 900 && i < static_cast< long >( th_.size() ); ++i )
        peak = std::max( peak, th_[ i ] );
      const double over = std::max( 0.0, ( peak - tgt ) / A );
      const double fin = std::fabs( window_mean( th_, t0 + kShow - 300, t0 + kShow ) - tgt );
      const bool good = reach >= 0 && reach <= 800 * tol() && over <= 0.65 * tol() && fin <= 0.20 * tol();
      if ( good )
        ++m.good;
      const double rt_s = reach >= 0 ? clampd( ( 1100.0 - reach ) / 800.0, 0, 1 ) : 0.0;
      m.reach_value += 0.40 * good + 0.20 * rt_s + 0.20 * clampd( 1.0 - over / 0.5, 0, 1 ) + 0.20 * clampd( 1.0 - fin / 0.15, 0, 1 );
      m.signed_final += window_mean( th_, t0 + kShow - 300, t0 + kShow ) - tgt;
      m.worst_over = std::max( m.worst_over, over );
      m.worst_final = std::max( m.worst_final, fin );
      if ( reach < 0 )
        ++m.never;
      else
      {
        m.worst_reach_ms = std::max( m.worst_reach_ms, reach );
        sr += reach;
        ++nr;
      }
    }
    if ( nr > 0 )
      m.reach_ms = sr / nr;
    // ringing after each push whose quiet window lies in a hold; deflection of the pushes that arrive during a displayed target
    for ( int k = 0; k < kNP; ++k )
    {
      const double q0 = push_t_[ k ] + kQuiet0, q1 = push_t_[ k ] + kQuiet1;
      if ( t_ < q1 )
        continue;
      bool clean = true;
      for ( int r = 0; r < kNR; ++r )
        if ( ( q1 > go_[ r ] && q0 < go_[ r ] + kMoveGrace ) || ( q1 > go_[ r ] + kShow && q0 < go_[ r ] + kShow + kMoveGrace ) )
          clean = false;
      if ( clean )
      {
        double p2p = 0;
        if ( oscillating( th_, static_cast< long >( q0 ), static_cast< long >( q1 ), p2p ) )
          m.osc_flag = 1;
        m.osc = std::max( m.osc, p2p );
      }
      for ( int r = 0; r < kNR; ++r )
        if ( push_t_[ k ] > go_[ r ] + kMoveGrace && push_t_[ k ] < go_[ r ] + kShow )
          for ( long i = static_cast< long >( push_t_[ k ] ); i < static_cast< long >( push_t_[ k ] + 600 ) && i < static_cast< long >( th_.size() ); ++i )
            m.push_hold_max = std::max( m.push_hold_max, std::fabs( th_[ i ] - ( kBase + kAmp[ r ] ) ) );
    }
    return m;
  }

  double push_t_[ kNP ] = {}, push_amp_[ kNP ] = {}, go_[ kNR ] = {};
  std::vector< double > th_;
};

} // namespace

std::unique_ptr< Task > make_boss_spinal_sprint() { return std::make_unique< SprintTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( boss_spinal_sprint, 135, "boss_spinal_sprint.json", make_boss_spinal_sprint ) }
