// Task "posture": hold a limb up against gravity and random pushes with the stretch reflex, fusimotor (gamma) drive and tonic
// descending input.
//
// WORLD (deterministic; limb and loop as in spinal_limb.h: elbow-like joint with flexor and extensor Hill muscles, 10 + 10 ms
// conduction, pool rate / 50 Hz = excitation). Unlike the reflex tasks the limb is NOT supported: the forearm weighs 1.5 kg
// (centre of mass 17 cm), gravity pulls with m g c cos(theta) = 2.5 N m at horizontal, and theta = 0 is horizontal, positive = raised.
//   Target   theta* = 0.5 rad (29 degrees above horizontal). The limb starts there.
//   Spindles Here the resting spindle bias is mostly fusimotor: Ia and II rest at 10 Hz and gamma drive adds 80 Hz (and 50 % length gain)
//            per unit of gamma (the real relaxed, gamma-quiet muscle spindle is nearly silent: Hulliger 1984; Prochazka & Gorassini 1998). Without
//            gamma drive the reflex has nothing to listen to. gamma = gamma pool mean rate / 40 Hz, applied to both spindles' static and
//            dynamic gains.
//   Sensors  "ag_Ia", "ant_Ia", "ag_II", "ant_II" (spindle primary / secondary rate, Hz, dc inputs; II reports muscle length, with
//            little velocity sensitivity, which is what a position servo needs), "drive" (tonic descending drive, dc, 1.0).
//            Optional "ag_Ib", "ant_Ib" (Golgi tendon organs).
//   Motors   "ag_motor", "ant_motor" (alpha pools), "gamma_motor" (gamma motoneuron pool).
//   Pushes   Six random pushes, 250 ms (cosine ramps 20 ms) of 1.0-1.8 N m, random direction: onset 2200 + 1800 k + U(0, 300) ms.
//            Episode 13 500 ms; the limb is scored from 1500 ms on.
//
// MEASURES   (err = theta - theta*)
//   rms_err_rad  : root-mean-square of err over 1500 ms to the end (pushes included).
//   max_err_rad  : the largest |err| over the same window.
//   offset_rad   : |mean err| over the 600 ms before each push (steady-state sag or lift, gravity included).
//   osc_rad      : in the window 900-1600 ms after each push: peak-to-peak angle; a swing > 0.08 rad with >= 6 reversals is a sustained
//                  oscillation.
// PASS: rms_err <= 0.12 rad, max_err <= 0.30 rad, offset <= 0.12 rad, no sustained oscillation, all channels wired, finished.
// VALUE: 100 * ( 0.45 clamp(1 - rms / 0.2) + 0.30 clamp(1 - max / 0.5) + 0.15 clamp(1 - offset / 0.2) + 0.10 (no oscillation) ).
//
// Biology: Matthews 1972 (gamma loop and the servo); Merton 1953 (follow-up length servo); Houk 1979 (regulation of stiffness);
// Hulliger 1984 (fusimotor action); Nichols & Houk 1976 (reflex compensation for variations in muscle force).
#include <iomanip>
#include "spinal_limb.h"
#include "tasks.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_posture();
namespace
{

constexpr int kN = 6;
constexpr double kDuration = 13500, kTarget = 0.5, kRamp = 20, kHold = 250, kLead = 2200, kSpacing = 1800, kJitter = 300;
constexpr double kScoreFrom = 1500, kQuiet0 = 900, kQuiet1 = 1600;

class PostureTask : public LimbTaskBase
{
public:
  PostureTask() { reset( 1 ); }
  std::string id() const override { return "posture"; }
  std::string title() const override { return "Posture"; }
  std::string brief() const override
  {
    return "Holding an arm up is a servo: gravity and bumps stretch the muscles, the spindles report it, and the reflex pulls back. "
           "The loop only works if the spindles are tuned: gamma motoneurons set their sensitivity (a spindle in a slack, "
           "gamma-quiet muscle is nearly silent), and a tonic descending drive sets how hard the antigravity muscle works. The "
           "forearm is NOT supported: it weighs 1.5 kg and must be held 29 degrees above horizontal while six random pushes knock it "
           "either way. Wire the spindle afferents (ag_Ia, ant_Ia, ag_II, ant_II) to the motoneuron pools, drive a gamma pool "
           "(gamma_motor) from the descending tone, and give the flexor more tone than the extensor so that gravity is balanced "
           "at the target. Score: root-mean-square angle error, the worst error, the sag between pushes, and no ringing.";
  }
  std::vector< std::string > sensor_channels() const override { return { "ag_Ia", "ant_Ia", "ag_II", "ant_II", "drive" }; }
  std::vector< std::string > motor_channels() const override { return { "ag_motor", "ant_motor", "gamma_motor" }; }
  double duration_ms() const override { return kDuration; }
  int par_neurons() const override { return 54; }

  std::vector< Event > timeline() const override
  {
    std::vector< Event > ev;
    for ( int k = 0; k < kN; ++k )
      ev.push_back( { onset_[ k ], 2 * kRamp + kHold, amp_[ k ] > 0 ? "push up" : "push down" } );
    return ev;
  }
  std::vector< Band > bands() const override { return { { "theta_rad", "rad", kTarget - 0.15, kTarget + 0.15 } }; }

  void reset( uint64_t seed ) override
  {
    Rng rng( seed ^ 0x706F73747572ULL );
    for ( int k = 0; k < kN; ++k )
    {
      onset_[ k ] = kLead + kSpacing * k + rng.uniform( 0, kJitter );
      const double a = rng.uniform( 1.0, 1.8 );
      amp_[ k ] = rng.uniform() < 0.5 ? a : -a;
    }
    LimbRigConfig c;
    c.joint.mass_kg = 1.5;
    c.joint.spindle.ia_bias = 10;
    c.joint.spindle.ii_bias = 10;
    c.joint.spindle.gamma_sta_bias = 80;
    base_reset( c, kTarget );
    th_.clear();
  }

  Score score() const override
  {
    Score s;
    Metrics m = analyse();
    s.parts[ "rms_err_rad" ] = m.rms;
    s.parts[ "max_err_rad" ] = m.mx;
    s.parts[ "offset_rad" ] = m.offset;
    s.parts[ "mean_err_rad" ] = m.signed_offset;
    s.parts[ "osc_rad" ] = m.osc;
    s.parts[ "gamma" ] = rig_.gam;
    s.parts[ "pushes_done" ] = m.done;
    const bool done = t_ >= kDuration;
    s.value = 100.0 * ( 0.45 * clampd( 1.0 - m.rms / 0.2, 0, 1 ) + 0.30 * clampd( 1.0 - m.mx / 0.5, 0, 1 ) +
                        0.15 * clampd( 1.0 - m.offset / 0.2, 0, 1 ) + 0.10 * ( m.osc_flag ? 0.0 : 1.0 ) );
    if ( !wiring_.ok() )
      s.value = std::min( s.value, 20.0 );
    s.passed = done && m.done >= kN && wiring_.ok() && m.rms <= 0.12 && m.mx <= 0.30 && m.offset <= 0.12 && !m.osc_flag;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 2 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( t_ < kScoreFrom )
      o << "Settling" << yet;
    else if ( m.osc_flag )
      o << "The limb rings after a push (" << m.osc << " rad p-p): the reflex gain is too high for its delays.";
    else if ( m.offset > 0.12 )
      o << "The limb " << ( m.signed_offset < 0 ? "sags " : "rides up " ) << std::fabs( m.signed_offset ) << " rad from the target between pushes: "
        << ( m.signed_offset < 0 ? "more flexor tone (or more reflex stiffness) is needed to balance gravity." : "less flexor tone, or more extensor tone, is needed." );
    else if ( m.mx > 0.30 )
      o << "Pushes move the limb up to " << m.mx << " rad from the target (limit 0.30): too compliant. Raise gamma drive or reflex gain.";
    else if ( m.rms > 0.12 )
      o << "RMS angle error " << m.rms << " rad (limit 0.12).";
    else
      o << ( done ? "Holding" : "Holding so far" ) << ": rms error " << m.rms << " rad, worst " << m.mx << " rad, sag " << m.offset << " rad.";
    s.verdict = o.str();
    return s;
  }

protected:
  double torque( double t ) const override
  {
    double tq = 0;
    for ( int k = 0; k < kN; ++k )
      tq += amp_[ k ] * push_profile( t, onset_[ k ], kHold, kRamp );
    return tq;
  }
  void cues( Engine& e, double ) override { put_cue( e, "drive", 1.0 ); }
  void after_tick( double ) override { th_.push_back( rig_.joint.theta ); }
  double target_rad() const override { return kTarget; }
  void extra_readouts( std::map< std::string, double >& r ) const override { r[ "err_rad" ] = rig_.joint.theta - kTarget; }

private:
  struct Metrics
  {
    double rms = 0, mx = 0, offset = 0, signed_offset = 0, osc = 0;
    int done = 0, osc_flag = 0;
  };

  Metrics analyse() const
  {
    Metrics m;
    const long a = static_cast< long >( kScoreFrom ), b = static_cast< long >( th_.size() );
    if ( b - a < 100 )
      return m;
    double ss = 0;
    for ( long i = a; i < b; ++i )
    {
      const double e = th_[ i ] - kTarget;
      ss += e * e;
      m.mx = std::max( m.mx, std::fabs( e ) );
    }
    m.rms = std::sqrt( ss / ( b - a ) );
    // steady-state offset before each push (the first window ends at the first push)
    double off = 0, soff = 0;
    int n = 0;
    for ( int k = 0; k < kN; ++k )
    {
      if ( t_ < onset_[ k ] )
        break;
      const double e = window_mean( th_, std::max( kScoreFrom, onset_[ k ] - 600 ), onset_[ k ] ) - kTarget;
      off += std::fabs( e );
      soff += e;
      ++n;
      if ( t_ >= onset_[ k ] + kQuiet1 )
      {
        ++m.done;
        double p2p = 0;
        if ( oscillating( th_, static_cast< long >( onset_[ k ] + kQuiet0 ), static_cast< long >( onset_[ k ] + kQuiet1 ), p2p ) )
          m.osc_flag = 1;
        m.osc = std::max( m.osc, p2p );
      }
    }
    if ( n > 0 )
    {
      m.offset = off / n;
      m.signed_offset = soff / n;
    }
    return m;
  }

  double onset_[ kN ] = {}, amp_[ kN ] = {};
  std::vector< double > th_;
};

} // namespace

std::unique_ptr< Task > make_posture() { return std::make_unique< PostureTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( posture, 120, "posture.json", make_posture ) }
