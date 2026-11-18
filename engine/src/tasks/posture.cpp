// Task "posture": hold a limb up against gravity and random pushes with the stretch reflex, fusimotor (gamma) drive and tonic
// descending input.
//
// WORLD (deterministic; limb and loop as in spinal_limb.h: elbow-like joint with flexor and extensor Hill muscles, 10 + 10 ms
// conduction, pool total rate / (20 units x rmax) = excitation). Unlike the reflex tasks the limb is NOT supported: the forearm weighs 1.5 kg
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
// PASS: rms_err <= 0.17 rad, max_err <= 0.30 rad, offset <= 0.17 rad, no sustained oscillation, all channels wired, finished.
// VALUE: 100 * ( 0.45 clamp(1 - rms / 0.35) + 0.30 clamp(1 - max / 0.6) + 0.15 clamp(1 - offset / 0.35) + 0.10 (no oscillation) ).
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
constexpr double kCeilingHz = 85;  // per-cell rate of the flexor pool beyond which more tone no longer lifts the limb (rmax 100 Hz)

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
           "at the target. Score: root-mean-square angle error, the worst error, the sag between pushes, and no ringing. Too much flexor "
           "tone is as bad as too little: a pool driven past its ceiling cannot lift the limb any further.";
  }
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A recipe that works (iaf_psc_alpha groups; weights pA; the spindle sensors are dc inputs, so their weight is pA per Hz): the four "
           "spindle sensors at weight 3 onto relay groups (Ia: 4 cells, II: 3 cells, ADVANCED tau m 60 ms); pools MNF and MNE of 20 cells; Ia and II "
           "relays > their pool at 150 (the lab's weight spread of 30 % is not needed here); Ia relays > inhibitory groups of 2 at 80, which inhibit the "
           "opposite pool at -320; drive (weight 300) onto a tonic group of 6 (tau m 60 ms): TONIC > MNF at 70 (the flexor tone that balances "
           "gravity; none onto MNE) and TONIC > GAM, the gamma group of 4 cells, at 960 (a silent gamma pool leaves the spindles quiet and the loop deaf). "
           "Raise the tone only while the limb sags and the flexor pool is below its ceiling; if the limb sags with the flexor pool already at "
           "its ceiling, lower the tone.";
  }
  std::vector< std::string > sensor_channels() const override { return { "ag_Ia", "ant_Ia", "ag_II", "ant_II", "drive" }; }
  std::vector< std::string > motor_channels() const override { return { "ag_motor", "ant_motor", "gamma_motor" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    return { { "ag_Ia", in_kind( "dc", 10, 3 ) }, { "ant_Ia", in_kind( "dc", 10, 3 ) }, { "ag_II", in_kind( "dc", 10, 3 ) }, { "ant_II", in_kind( "dc", 10, 3 ) }, { "drive", in_kind( "dc", 1, 300 ) } };
  }
  double duration_ms() const override { return kDuration; }
  /// Quick check: the first three pushes, each with its ringing window (the same rules as the full episode; a full run judges all six).
  double quick_duration_ms() const override { return kLead + 2 * kSpacing + kJitter + kQuiet1; }
  int par_neurons() const override { return variant_par( variant_, 54 ); }

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }
  std::vector< std::string > concepts_used() const override { return { "recruitment", "reciprocal_inhibition", "excitation", "negative_feedback" }; }
  bool on_spine() const override { return true; }
  /// Perturbation x0.8 (guided) / x1.05 (researcher) on the 1.0-1.8 N m pushes; pass limits rms 0.17 / worst 0.30 / sag 0.17 rad x1.25
  /// (guided) or x0.92 (researcher).
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 27 instead of 54" },
             { "strong_pushes", "pushes 1.4 times as hard (1.4-2.5 N m)" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    std::vector< Hint > h;
    if ( wiring_.ok() && mech_hints( h, "Posture is a servo: the spindle afferent of a muscle reports its length, the reflex pulls back, and the gamma motoneurons set how sensitive the spindles are." ) )
      return h;
    if ( s.parts[ "osc_rad" ] > 0.08 )
    {
      h.push_back( make_hint( 1, "Negative feedback with conduction delays rings when its gain is too high.", "" ) );
      h.push_back( make_hint( 2, "The limb oscillates after a push: lower the gain of the stretch-reflex loop.", "ag_motor" ) );
      h.push_back( make_hint( 3, "Lower the relay > pool weights a little, and keep the inhibition onto the opposite pool moderate.", "" ) );
    }
    else if ( s.parts[ "gamma" ] < 0.1 )
    {
      h.push_back( make_hint( 1, "The spindles are nearly silent when the gamma motoneurons are quiet: the feedback loop has nothing to listen to.", "" ) );
      h.push_back( make_hint( 2, "The gamma pool is not firing.", "gamma_motor" ) );
      h.push_back( make_hint( 3, "Let the tonic group excite the gamma group strongly enough that the gamma cells fire steadily.", "" ) );
    }
    else if ( s.parts[ "offset_rad" ] > tol_max( mode_, 0.17, 1.25, 0.92 ) )
    {
      h.push_back( make_hint( 1, "Gravity pulls the limb down all the time; the flexor needs a steady tone to balance it.", "" ) );
      h.push_back( make_hint( 2, s.parts[ "mean_err_rad" ] > 0 ? "The limb rides above the target between pushes: the flexor pool needs less tonic drive."
                                  : s.parts[ "flexor_rate_hz" ] >= kCeilingHz ? "The limb sags although the flexor pool is already at its ceiling: less tonic drive, not more."
                                                                              : "The limb sags between pushes: the flexor pool needs more tonic drive.", "ag_motor" ) );
      h.push_back( make_hint( 3, "Let the tonic group excite the flexor pool (and little or nothing the extensor), raising or lowering that weight until the sag between pushes goes.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "Negative feedback: the error (length from the target) is sensed by the spindle and fed back to oppose it.", "" ) );
      h.push_back( make_hint( 2, "The pushes move the limb too far: raise the gain from the spindle secondaries (II) and primaries (Ia) onto the pools.", "ag_II" ) );
      h.push_back( make_hint( 3, "Make the II and Ia relay links onto their pool stronger; the II group gives the position signal.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override { return { { "rms_err_rad", make_metric( score().parts[ "rms_err_rad" ], "rad", true ) } }; }
  Note notebook() const override
  {
    return { "Servo-assisted posture",
             "Muscle spindles and the stretch reflex form a length servo: the gamma motoneurons set how sensitive the spindles are, "
             "and the reflex changes alpha drive to hold the limb against loads. Gamma-quiet spindles are nearly silent.",
             "Merton PA (1953) Ciba Found Symp; Matthews PBC (1972) Mammalian Muscle Receptors and their Central Actions; Hulliger M (1984) Rev Physiol Biochem Pharmacol 101:1" };
  }

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
    c.rmax_hz = 100;  // excitation 1 = 20 units x 100 Hz in total: 14 cells at 30-45 Hz drive the muscle at 0.2-0.3, on the steep, recruiting part of its force curve (at 0.5+ the force saturates and the limb has no stiffness)
    c.joint.mass_kg = 1.5;
    c.joint.spindle.ia_bias = 10;
    c.joint.spindle.ii_bias = 10;
    c.joint.spindle.gamma_sta_bias = 80;
    base_reset( c, kTarget );
    th_.clear();
    rate_.clear();
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
    s.parts[ "flexor_rate_hz" ] = m.flexor_rate;
    s.parts[ "pushes_done" ] = m.done;
    const bool done = t_ >= quick_duration_ms() - 1e-6;  // the quick check's length: three pushes (a full run has six)
    const mechb::Issue* mech = mech_bad();
    s.value = 100.0 * ( 0.45 * clampd( 1.0 - m.rms / 0.35, 0, 1 ) + 0.30 * clampd( 1.0 - m.mx / 0.6, 0, 1 ) +
                        0.15 * clampd( 1.0 - m.offset / 0.35, 0, 1 ) + 0.10 * ( m.osc_flag ? 0.0 : 1.0 ) );
    if ( !wiring_.ok() )
      s.value = std::min( s.value, 20.0 );
    if ( mech )
      s.value = std::min( s.value, 40.0 );  // behaviour without the mechanism the level teaches is not a solution (docs/REQ-0.4.md rule 1)
    s.passed = done && m.done >= 3 && !mech && wiring_.ok() && m.rms <= tol_max( mode_, 0.17, 1.25, 0.92 ) && m.mx <= tol_max( mode_, 0.30, 1.25, 0.92 ) && m.offset <= tol_max( mode_, 0.17, 1.25, 0.92 ) && !m.osc_flag;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 2 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( mech )
      o << mech->fail;
    else if ( t_ < kScoreFrom )
      o << "Settling" << yet;
    else if ( m.osc_flag )
      o << "The limb rings after a push: the reflex gain is too high for its delays.";
    else if ( m.offset > tol_max( mode_, 0.17, 1.25, 0.92 ) )
    {
      o << "The limb " << ( m.signed_offset < 0 ? "sags" : "rides up" ) << " away from the target between pushes: ";
      if ( m.signed_offset > 0 )
        o << "less flexor tone (or more extensor tone) is needed.";
      else if ( m.flexor_rate >= kCeilingHz )
        o << "the flexor pool already fires at its ceiling, so more tone cannot lift the limb: lower the flexor tone (and check the extensor is not co-contracting).";
      else
        o << "more flexor tone (or more reflex stiffness) is needed to balance gravity.";
    }
    else if ( m.mx > tol_max( mode_, 0.30, 1.25, 0.92 ) )
      o << "Pushes move the limb too far from the target: too compliant. Raise gamma drive or reflex gain.";
    else if ( m.rms > tol_max( mode_, 0.17, 1.25, 0.92 ) )
      o << "The limb wanders too much from the target overall: more reflex stiffness is needed.";
    else
      o << ( done ? "Holding" : "Holding so far" ) << ": rms error " << m.rms << " rad, worst " << m.mx << " rad, sag " << m.offset << " rad.";
    s.verdict = o.str();
    return s;
  }

protected:
  void mechanism( const mechb::Graph& g, std::vector< mechb::Issue >& out ) const override { mechb::limb_spindle( g, out ); }
  double torque( double t ) const override
  {
    double tq = 0;
    for ( int k = 0; k < kN; ++k )
      tq += amp_[ k ] * push_scale() * push_profile( t, onset_[ k ], kHold, kRamp );
    return tq;
  }
  void cues( Engine& e, double ) override { put_cue( e, "drive", 1.0 ); }
  void after_tick( double ) override
  {
    th_.push_back( rig_.joint.theta );
    rate_.push_back( rig_.rate_ag );
  }
  double target_rad() const override { return kTarget; }
  bool has_target() const override { return true; }
  void extra_readouts( std::map< std::string, double >& r ) const override { r[ "err_rad" ] = rig_.joint.theta - kTarget; }

private:
  double push_scale() const { return mode_factor( mode_, 0.8, 1.05 ) * ( variant_ == "strong_pushes" ? 1.4 : 1.0 ); }
  struct Metrics
  {
    double rms = 0, mx = 0, offset = 0, signed_offset = 0, osc = 0, flexor_rate = 0;
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
    m.flexor_rate = window_mean( rate_, kScoreFrom, static_cast< double >( b ) );
    if ( n > 0 )
    {
      m.offset = off / n;
      m.signed_offset = soff / n;
    }
    return m;
  }

  double onset_[ kN ] = {}, amp_[ kN ] = {};
  std::vector< double > th_, rate_;
};

} // namespace

std::unique_ptr< Task > make_posture() { return std::make_unique< PostureTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( posture, 120, "posture.json", make_posture ) }
