// Task "renshaw": recurrent (Renshaw) inhibition steadies a motoneuron pool and lowers force tremor.
//
// WORLD (deterministic; see spinal_limb.h and stretch_reflex.cpp for the limb: supported elbow-like joint, flexor and extensor Hill
// muscles with size-principle recruitment, spindle Ia feedback with 10 + 10 ms conduction, pool rate / 50 Hz = excitation).
//   Sensors  "ag_Ia", "ant_Ia" (spindle Ia rate, Hz, dc input), "drive" (tonic descending drive, dc, 1.0) and "noise" (Poisson, set to
//            400 Hz every slice): the background synaptic input of the cord. A circuit that sends "noise" through shared fibres to
//            both motoneuron pools gives the pools COMMON input, which is what makes real motor pools fluctuate together and the
//            force wobble (tremor, 2-20 Hz; Farmer 1993). Independent noise on every cell averages out over the pool and does nothing.
//   Motors   "ag_motor", "ant_motor": alpha motoneuron pools (mean Hz per neuron).
//   Protocol HOLD 1500-7500 ms: tonic drive and noise, limb free; 6 s are scored for tremor.
//            Then two imposed stretches (flexor at 8200 + U(0,300) ms, extensor at 10 600 + U(0,300) ms; 2.5 N m, cosine ramps 20 ms,
//            hold 350 ms) check that the stretch reflex still works with the recurrent loop in place. Episode 13 500 ms.
//
// MEASURES
//   tremor_pct  : force tremor = lock-in amplitude of the summed muscle force r (F_flexor + F_extensor) at the three drive frequencies
//                 (8.5, 10 and 11.5 Hz: each fits a whole number of cycles in the 6 s hold window, so the independent spike noise of the
//                 pool drops out), root-sum-square, divided by the mean force, in percent. (The sum, not the difference: common input
//                 moves both muscles together, and in the net joint torque that would cancel.)
//   Common drive: the "noise" rate is 400 Hz x (1 + 0.6 (sin(8.5 Hz) + sin(10 Hz) + sin(11.5 Hz)) / sqrt 3), phases seeded: the 8-12 Hz
//                 central drive that physiological tremor is made of (Elble & Koller 1990).
//   rate_cv     : motoneuron-rate instability: sd / mean of the pool rate (Hz per neuron, smoothed with tau 15 ms) in the hold window.
//   improvement : 1 - tremor_pct / tremor_ref(rate), floored 0: relative reduction against an unbraked cord (the same reference circuit
//                 with Renshaw cells cut) firing at the same mean rate; tremor_ref is a measured control curve (table below).
//                 0.5 means the tremor is halved.
//   resistance  : as in stretch_reflex (the recurrent loop must not abolish the reflex), mean of the two stretches.
// PASS: improvement >= 0.25 (tremor cut by a quarter or more), resistance >= 0.30, no sustained oscillation, pools fire (>= 5 Hz mean
//       in the hold window), wired, finished.
// VALUE: 100 * ( 0.55 min(1, improvement / 0.5) + 0.25 min(1, resistance / 0.5) + 0.2 (no oscillation) ), capped at 49 unless the
//        pools are firing.
//
// Biology: Renshaw 1941 (recurrent inhibition); Eccles, Fatt & Koketsu 1954 (cholinergic collaterals); Hultborn, Jankowska &
// Lindstrom 1971 (Renshaw cells inhibit Ia inhibitory interneurons); Hultborn 2006 review; Farmer 1993 (common drive to motor
// units); Elble & Koller 1990 (physiological tremor).
#include <iomanip>
#include "spinal_limb.h"
#include "tasks.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_renshaw();
namespace
{

constexpr int kN = 2;
constexpr double kDuration = 13500, kTorque = 2.5, kRamp = 20, kHold = 350, kNoiseHz = 400;
constexpr double kHold0 = 1500, kHold1 = 7500, kStretch0 = 8200, kStretchGap = 2400, kJitter = 300;
constexpr double kEnd = 2 * kRamp + kHold + 100, kQuiet0 = 1500, kQuiet1 = 2200;
constexpr double kBandHz[ 3 ] = { 8.5, 10.0, 11.5 }, kNoiseDepth = 0.6;

class RenshawTask : public LimbTaskBase
{
public:
  RenshawTask() { reset( 1 ); }
  std::string id() const override { return "renshaw"; }
  std::string title() const override { return "Renshaw cell"; }
  std::string brief() const override
  {
    return "Every alpha motoneuron sends a collateral to a Renshaw cell, which inhibits the motoneurons back (Renshaw 1941): "
           "negative feedback, so the harder the pool fires, the harder it is held back. Add Renshaw cells to your stretch reflex: "
           "motoneurons excite them, they inhibit their own motoneurons (and, as in the real cord, the Ia inhibitory interneurons and "
           "the opposite Renshaw cells). The cord is bathed in background synaptic noise that reaches both pools through shared "
           "fibres, which makes the pools fire in waves and the muscle force wobble (tremor). Hold the limb for six seconds, then "
           "take two sudden stretches. Score: how much your recurrent inhibition cuts the force tremor compared with a cord "
           "without Renshaw cells (at least a quarter less to pass), while the stretch reflex still resists the pushes and the "
           "limb does not ring.";
  }
  std::vector< std::string > sensor_channels() const override { return { "ag_Ia", "ant_Ia", "drive", "noise" }; }
  std::vector< std::string > motor_channels() const override { return { "ag_motor", "ant_motor" }; }
  double duration_ms() const override { return kDuration; }
  int par_neurons() const override { return 56; }

  std::vector< Event > timeline() const override
  {
    std::vector< Event > ev;
    ev.push_back( { kHold0, kHold1 - kHold0, "hold (tremor scored)" } );
    for ( int k = 0; k < kN; ++k )
      ev.push_back( { onset_[ k ], 2 * kRamp + kHold, k % 2 == 0 ? "stretch flexor" : "stretch extensor" } );
    return ev;
  }
  std::vector< Band > bands() const override { return { { "tremor_pct", "%", 0.0, 4.5 } }; }

  void reset( uint64_t seed ) override
  {
    Rng rng( seed ^ 0x72656E73686177ULL );
    for ( int k = 0; k < kN; ++k )
      onset_[ k ] = kStretch0 + kStretchGap * k + rng.uniform( 0, kJitter );
    for ( int k = 0; k < 3; ++k )
      phase_[ k ] = rng.uniform( 0, 2 * kPi );
    LimbRigConfig c;
    c.rmax_hz = 70;  // a pool at 70 Hz per cell is fully excited: keeps 25-45 Hz on the steep part of the force curve, so tremor does not depend on saturation
    base_reset( c, 0.2 );
    th_.clear();
    sh_.clear();
    tq_hp_.clear();
    ft_.clear();
    rate_.clear();
    lp_tq_ = lp_rate_ = 0;
    lp_tq_set_ = false;
    shadow_on_ = -1;
    theta_pre_ = 0.2;
    tremor_now_ = 0;
  }

  Score score() const override
  {
    Score s;
    Metrics m = analyse();
    s.parts[ "tremor_pct" ] = m.tremor;
    s.parts[ "tremor_reference_pct" ] = tremor_ref( m.rate_mean );
    s.parts[ "improvement" ] = m.improvement;
    s.parts[ "rate_cv" ] = m.rate_cv;
    s.parts[ "pool_rate_hz" ] = m.rate_mean;
    s.parts[ "resistance" ] = m.resistance;
    s.parts[ "osc_rad" ] = m.osc;
    s.parts[ "stretches_done" ] = m.done;
    const bool done = t_ >= kDuration;
    const bool firing = m.rate_mean >= 5.0;
    s.value = 100.0 * ( 0.55 * std::min( 1.0, m.improvement / 0.5 ) + 0.25 * std::min( 1.0, m.resistance / 0.5 ) + 0.2 * ( m.osc_flag ? 0.0 : 1.0 ) );
    if ( !firing || m.osc_flag )
      s.value = std::min( s.value, 49.0 );
    s.passed = done && m.done >= kN && wiring_.ok() && firing && m.improvement >= 0.25 && m.resistance >= 0.30 && !m.osc_flag;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( t_ < kHold1 )
      o << "Holding" << yet;
    else if ( !firing )
      o << "The motor pool is nearly silent (" << m.rate_mean << " Hz): give it tonic drive.";
    else if ( m.done == 0 )
      o << "Tremor " << m.tremor << "%, waiting for the pushes" << yet;
    else if ( m.osc_flag )
      o << "The limb rings after the push (" << std::setprecision( 2 ) << m.osc << " rad p-p): too much loop gain.";
    else if ( m.improvement < 0.25 )
      o << "Force tremor " << m.tremor << "% is only " << std::setprecision( 0 ) << m.improvement * 100 << "% below a cord without Renshaw cells ("
        << std::setprecision( 1 ) << tremor_ref( m.rate_mean ) << "% at this rate): the recurrent loop is too weak, or missing.";
    else if ( m.resistance < 0.30 )
      o << "The stretch reflex only cuts the deflection by " << std::setprecision( 0 ) << m.resistance * 100
        << "%: the recurrent inhibition is smothering the reflex.";
    else
      o << ( done ? "Tremor " : "Tremor so far " ) << m.tremor << "%, " << std::setprecision( 0 ) << m.improvement * 100 << "% below an unbraked cord, reflex cuts deflection by "
        << m.resistance * 100 << "%.";
    s.verdict = o.str();
    return s;
  }

protected:
  double torque( double t ) const override
  {
    double tq = 0;
    for ( int k = 0; k < kN; ++k )
      tq += ( k % 2 == 0 ? -kTorque : kTorque ) * push_profile( t, onset_[ k ], kHold, kRamp );
    return tq;
  }
  void cues( Engine& e, double t ) override
  {
    put_cue( e, "drive", 1.0 );
    put_cue( e, "noise", noise_rate( t ) );
  }
  double noise_rate( double t ) const
  {
    double m = 0;
    for ( int k = 0; k < 3; ++k )
      m += std::sin( 2 * kPi * kBandHz[ k ] * t / 1000.0 + phase_[ k ] );
    return std::max( 0.0, kNoiseHz * ( 1.0 + kNoiseDepth * m / std::sqrt( 3.0 ) ) );
  }
  void after_tick( double h ) override
  {
    for ( int k = 0; k < kN; ++k )
    {
      if ( shadow_on_ < 0 && t_ >= onset_[ k ] - 1 && t_ < onset_[ k ] + 5 )
      {
        rig_.freeze_shadow();
        shadow_on_ = k;
      }
      if ( shadow_on_ == k && t_ >= onset_[ k ] + 800 )
      {
        rig_.release_shadow();
        shadow_on_ = -1;
      }
    }
    if ( std::fabs( tq_ ) < 1e-9 )
      theta_pre_ += ( rig_.joint.theta - theta_pre_ ) / 150.0;
    th_.push_back( rig_.joint.theta );
    sh_.push_back( rig_.shadow_active() ? rig_.shadow.theta : rig_.joint.theta );
    // total muscle force torque above its own 100 ms low-pass (the common input moves both muscles together; in the difference it would cancel)
    const double r = rig_.joint.params().moment_arm_m;
    const double net = r * ( rig_.joint.agonist.force_N + rig_.joint.antagonist.force_N );
    if ( !lp_tq_set_ )
    {
      lp_tq_ = net;
      lp_rate_ = rig_.emg_ag;
      lp_tq_set_ = true;
    }
    lp_tq_ += ( 1.0 - std::exp( -h / 100.0 ) ) * ( net - lp_tq_ );
    lp_rate_ += ( 1.0 - std::exp( -h / 30.0 ) ) * ( 0.5 * ( rig_.rate_ag + rig_.rate_ant ) - lp_rate_ );
    tq_hp_.push_back( net - lp_tq_ );
    ft_.push_back( r * ( rig_.joint.agonist.force_N + rig_.joint.antagonist.force_N ) );
    rate_.push_back( 0.5 * ( rig_.rate_ag + rig_.rate_ant ) );
    if ( t_ >= kHold0 )
    {
      const double a = 1.0 - std::exp( -h / 500.0 );
      tremor_now_ += a * ( std::fabs( tq_hp_.back() ) / std::max( 1e-6, ft_.back() ) * 100.0 - tremor_now_ );
    }
  }
  void extra_readouts( std::map< std::string, double >& r ) const override
  {
    r[ "theta_dev_rad" ] = rig_.joint.theta - theta_pre_;
    r[ "tremor_pct" ] = tremor_now_;
    r[ "pool_rate_hz" ] = rig_.rate_ag;
  }

private:
  struct Metrics
  {
    double tremor = 0, improvement = 0, rate_cv = 0, rate_mean = 0, resistance = 0, osc = 0;
    int done = 0, osc_flag = 0;
  };

  /// Control curve: force tremor (%) of the same reference circuit WITHOUT Renshaw cells (MN -> RC cut) at tonic drives 450..1000 pA,
  /// mean of seeds 1-3, against the pool's mean rate. Tremor falls with rate (the muscle's force curve flattens, the pool's spike
  /// noise shrinks), so "less tremor" is judged against an unbraked cord firing at the SAME rate, not against a fixed number.
  static double tremor_ref( double rate_hz )
  {
    static const double r[] = { 16.6, 18.5, 21.8, 24.1, 27.0, 30.1, 33.1, 36.5, 39.6, 42.6, 45.5, 48.0 };
    static const double t[] = { 10.0, 9.1, 6.1, 5.8, 5.7, 5.1, 4.3, 3.5, 2.9, 2.2, 1.5, 1.1 };
    constexpr int n = 12;
    if ( rate_hz <= r[ 0 ] )
      return t[ 0 ];
    for ( int i = 1; i < n; ++i )
      if ( rate_hz <= r[ i ] )
        return t[ i - 1 ] + ( t[ i ] - t[ i - 1 ] ) * ( rate_hz - r[ i - 1 ] ) / ( r[ i ] - r[ i - 1 ] );
    return t[ n - 1 ];
  }

  Metrics analyse() const
  {
    Metrics m;
    const double t1 = std::min( t_, kHold1 );
    if ( t1 > kHold0 + 500 )
    {
      const long a = static_cast< long >( kHold0 ), b = std::min< long >( static_cast< long >( t1 ), ft_.size() );
      const double n = static_cast< double >( b - a );
      // lock-in amplitude of the summed muscle force at the three drive frequencies (8.5, 10, 11.5 Hz: every one fits a whole number of
      // cycles into 6 s), so the independent spike noise of the pool drops out and only the common-drive response (tremor) remains
      double mean = 0, rss = 0;
      for ( long i = a; i < b; ++i )
        mean += ft_[ i ];
      mean /= n;
      for ( int k = 0; k < 3; ++k )
      {
        double c = 0, sn = 0;
        for ( long i = a; i < b; ++i )
        {
          const double ph = 2 * kPi * kBandHz[ k ] * ( i + 0.5 ) / 1000.0;
          c += ( ft_[ i ] - mean ) * std::cos( ph );
          sn += ( ft_[ i ] - mean ) * std::sin( ph );
        }
        const double amp = 2.0 / n * std::sqrt( c * c + sn * sn );
        rss += amp * amp;
      }
      m.tremor = mean > 1e-9 ? 100.0 * std::sqrt( rss ) / mean : 0;
      m.rate_mean = window_mean( rate_, kHold0, t1 );
      m.rate_cv = m.rate_mean > 1e-6 ? window_sd( rate_, kHold0, t1 ) / m.rate_mean : 0;
      m.improvement = clampd( 1.0 - m.tremor / tremor_ref( m.rate_mean ), 0, 1 );
    }
    double res = 0;
    int nres = 0;
    for ( int k = 0; k < kN; ++k )
    {
      const double t0 = onset_[ k ];
      if ( t_ < t0 + kEnd )
        break;
      ++m.done;
      const double th0 = window_mean( th_, t0 - 100, t0 );
      double d = 0, d_open = 0;
      const long end = static_cast< long >( t0 + kEnd );
      for ( long i = static_cast< long >( t0 ); i < end && i < static_cast< long >( th_.size() ); ++i )
      {
        d = std::max( d, std::fabs( th_[ i ] - th0 ) );
        d_open = std::max( d_open, std::fabs( sh_[ i ] - th0 ) );
      }
      res += d_open > 1e-6 ? clampd( 1.0 - d / d_open, 0, 1 ) : 0.0;
      ++nres;
      if ( t_ >= t0 + kQuiet1 )
      {
        double p2p = 0;
        if ( oscillating( th_, static_cast< long >( t0 + kQuiet0 ), static_cast< long >( t0 + kQuiet1 ), p2p ) )
          m.osc_flag = 1;
        m.osc = std::max( m.osc, p2p );
      }
    }
    if ( nres > 0 )
      m.resistance = res / nres;
    return m;
  }

  double onset_[ kN ] = {}, phase_[ 3 ] = {};
  std::vector< double > th_, sh_, tq_hp_, ft_, rate_;
  double lp_tq_ = 0, lp_rate_ = 0, tremor_now_ = 0, theta_pre_ = 0.2;
  bool lp_tq_set_ = false;
  int shadow_on_ = -1;
};

} // namespace

std::unique_ptr< Task > make_renshaw() { return std::make_unique< RenshawTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( renshaw, 110, "renshaw.json", make_renshaw ) }
