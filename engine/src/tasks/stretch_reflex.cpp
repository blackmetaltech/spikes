// Task "stretch_reflex": the monosynaptic stretch reflex with reciprocal Ia inhibition.
//
// WORLD (deterministic; times in ms, torques in N m, angles in rad). The closed loop is in spinal_limb.h: a hinge joint (elbow-like,
// supported, no gravity, inertia 0.1 kg m^2, viscosity 0.3 N m s/rad) with a flexor and an extensor Hill muscle, spindles with
// 10 ms afferent and 10 ms efferent conduction delay, pool rate (Hz per neuron) smoothed with tau 15 ms; muscle excitation = the pool's
// total rate (cells x mean rate) / (20 motor units x 50 Hz), so a pool of 20 cells at 50 Hz is full excitation and one cell can only
// drive its share of one unit (the Muscle recruits its motor units by size).
//   Sensors  "ag_Ia", "ant_Ia": spindle Ia rate (Hz, a dc input: each afferent neuron the channel feeds is a regular integrator, see
//            circuits/tools/gen_spinal.py) of the flexor (agonist) and the extensor (antagonist);
//            "drive": tonic descending drive (dc, 1.0 every slice: dc current = value x weight, not a spike rate). Optional (driven when the circuit declares them): "ag_Ib", "ant_Ib"
//            (Golgi tendon organs), "ag_II", "ant_II" (spindle secondaries).
//   Motors   "ag_motor", "ant_motor": alpha motoneuron pools (mean Hz per neuron).
//   Protocol 1) TENDON TAPS, t = 300 .. ~3200 ms: 24 taps (12 per muscle), 120 ms apart (+ U(0,30) ms), alternately on the flexor and the extensor:
//               a 3 ms Ia volley (+1000 Hz on the channel) delivered at the spinal cord 10 ms after the tap. Used only to time the reflex.
//            2) FOUR IMPOSED STRETCHES (cosine ramps 20 ms, hold 350 ms, 2.5 N m), alternating direction: the flexor is stretched by
//               -2.5 N m (limb pushed toward extension), then the extensor by +2.5 N m, again twice. Stretch k starts at
//               3600 + 2400 k + U(0, 300) ms (seeded splitmix64). Episode 13 500 ms.
//   Open-loop reference: at each stretch onset an exact copy of the limb is started with the drive frozen at its tonic level
//            (mean of the last 400 ms). It sees the same torque, so  resistance = 1 - dev / dev_open  is how much the circuit's
//            reaction reduced the peak deflection (0 = no better than muscle stiffness alone, 1 = no deflection). dev_open is the push's own effect on
//            that copy: its excursion minus that of a second copy held the same way WITHOUT the push. (A copy held at a lumpy tonic level drifts by itself:
//            before this correction dev_open ranged from 0.03 to 0.67 rad between seeds with the real deflection steady at 0.05-0.10, so the ratio, not the
//            circuit, decided the pass on about 1 seed in 35 of the quick check.)
//
// MEASURES
//   latency_ms  : T-reflex latency. Spikes of the tapped muscle's motoneuron pool are averaged over its twelve taps in 2 ms bins (PSTH);
//                 the response starts at the first bin (from 8 ms on) above baseline + max(4 sqrt(baseline / 12 taps), 1.2 spikes), with spikes above baseline from at least 20 % of the pool's cells in the next 8 ms (baseline = the 40 ms before the tap).
//                 Latency = bin start + 10 ms efferent conduction (so it is the delay from the tendon hit to the muscle's EMG;
//                 the spinal range is 25-50 ms for long human arm and leg nerves, Matthews 1991). Mean of the two pools.
//   resistance  : see above (deflection = peak |theta - theta_pre| until 100 ms after the release), mean of the four stretches.
//   recip       : antagonist suppression: 1 - (mean antagonist pool rate from 20 to 140 ms after the stretch started) / (its mean in the
//                 450 ms before), floored at 0, mean of the stretches. Reciprocal inhibition makes it large; a circuit without it
//                 leaves the antagonist firing (co-contraction).
//   osc_rad     : quiet window 1500-2200 ms after each stretch onset: if the angle swings more than 0.08 rad peak to peak with
//                 >= 6 reversals, it is a sustained oscillation (clonus).
// PASS: both pools respond to taps (latency found), mean latency <= 50 ms, resistance >= 0.30, recip >= 0.20, no sustained oscillation,
//       motor pools wired, episode finished.
// VALUE: 100 * ( 0.35 min(1, resistance / 0.5) + 0.25 latency score (1 at <= 35 ms, 0 at >= 80 ms)
//               + 0.25 min(1, recip / 0.5) + 0.15 (no oscillation) ), capped at 49 unless every pool has a reflex and no ringing.
//
// Biology: Sherrington 1906; Liddell & Sherrington 1924 (myotatic reflex); Lloyd 1943 (monosynaptic Ia reflex); Eccles, Eccles &
// Lundberg 1957 (reciprocal Ia inhibition); Matthews 1991 (human stretch reflex latencies); Henneman 1957 (size principle).
#include <iomanip>
#include "spinal_limb.h"
#include "tasks.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_stretch_reflex();
namespace
{

constexpr int kN = 4, kTaps = 24;
constexpr double kDuration = 13500, kTorque = 2.5, kRamp = 20, kHold = 350, kLead = 3600, kSpacing = 2400, kJitter = 300;
constexpr double kEnd = 2 * kRamp + kHold + 100, kQuiet0 = 1500, kQuiet1 = 2200;
constexpr double kRecBase = 450, kRecEnd = 140;   // the antagonist's baseline is its mean rate over the 450 ms before the onset (after the last tendon tap has died away), its response the mean from 20 to 140 ms after it: the pools fire in lumpy waves about 110 ms apart, and a 150 ms baseline caught a random phase of one (that estimator's spread, not the circuit, made the quick check miss 1 seed in 35)
constexpr int kTonicWin = 400;   // ms of drive averaged for the open-loop copy's tonic level
constexpr double kTapStart = 300, kTapSpacing = 120, kTapRate = 1000, kTapLen = 3;

class StretchReflexTask : public LimbTaskBase
{
public:
  StretchReflexTask() { reset( 1 ); }
  std::string id() const override { return "stretch_reflex"; }
  std::string title() const override { return "Stretch reflex"; }
  std::string brief() const override
  {
    return "Tap a tendon and the muscle twitches: a stretched muscle's spindle fires Ia afferents that excite the same muscle's "
           "alpha motoneurons through a single synapse (Sherrington's myotatic reflex). The same Ia fibre also feeds an "
           "inhibitory interneuron that silences the opposing muscle (reciprocal inhibition, Eccles 1957), so the two do not "
           "fight. First the tendons are tapped, one muscle at a time, to time the reflex; then the forearm is pushed by four "
           "sudden loads, alternately toward extension and flexion. Wire ag_Ia and ant_Ia (the flexor and extensor spindles) to "
           "motoneuron pools ag_motor and ant_motor, give both pools tonic drive, and add the inhibitory crossing. Score: the motor "
           "reply to a tap must come within 50 ms (a spinal reflex, not a decision), the reflex must cut the deflection to well "
           "below what the muscles' own stiffness would allow, the opposing muscle must be switched off, and the limb must "
           "settle without ringing (too much reflex gain oscillates: clonus).";
  }
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A recipe that works (iaf_psc_alpha groups; weights pA; the Ia sensors are dc inputs, so their weight is pA of current per Hz of "
           "afferent rate, and the drive weight is pA of tonic current): ag_Ia and ant_Ia at weight 3 onto relay groups IaF and IaE (6 cells, "
           "ADVANCED tau m 60 ms so they fire regularly at the afferent rate); motoneuron pools MNF and MNE of 20 cells with drive at weight 320 "
           "onto both; IaF > MNF and IaE > MNE at 240 with a weight spread (CV) of 0.5, so the pool is recruited gradually instead of all at once; "
           "IaF > INF and IaE > INE (6 cells, inhibitory) at 240; INF > MNE and INE > MNF at -320. A much weaker Ia > MN link (80) misses tendon taps; "
           "much stronger gains, or more drive, ring or make both pools fight.";
  }
  std::vector< std::string > sensor_channels() const override { return { "ag_Ia", "ant_Ia", "drive" }; }
  std::vector< std::string > motor_channels() const override { return { "ag_motor", "ant_motor" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    return { { "ag_Ia", in_kind( "dc", 40, 3 ) }, { "ant_Ia", in_kind( "dc", 40, 3 ) }, { "drive", in_kind( "dc", 1, 320 ) } };
  }
  double duration_ms() const override { return kDuration; }
  /// Quick check: the taps and the first two stretches (one per muscle, the second with its ringing window): the same rules as the full episode on
  /// a shorter run. A full run judges all four stretches.
  double quick_duration_ms() const override { return kLead + kSpacing + kJitter + kQuiet1; }
  int par_neurons() const override { return variant_par( variant_, 54 ); }

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }
  std::vector< std::string > concepts_used() const override { return { "excitation", "inhibition", "convergence", "recruitment", "reciprocal_inhibition" }; }
  bool on_spine() const override { return true; }
  /// Pass limits: guided latency <= 62 ms, resistance >= 0.24, antagonist suppression >= 0.16; standard 50 ms, 0.30, 0.20;
  /// researcher 42.5 ms, 0.35, 0.24. The pushes are 2.5 N m in every mode (the loop has no noise of its own).
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 27 instead of 54" },
             { "heavy_load", "pushes of 3.5 N m instead of 2.5 N m: the reflex must work harder" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    const double rec = s.parts[ "antagonist_suppression" ], res = s.parts[ "resistance" ], osc = s.parts[ "osc_rad" ];
    std::vector< Hint > h;
    if ( wiring_.ok() && mech_hints( h, "The stretch reflex is built of two things: one excitatory synapse from the muscle's Ia afferent onto its own motoneurons, and a branch of the same afferent that inhibits the opposing muscle through an interneuron." ) )
      return h;
    if ( s.parts[ "latency_ms" ] <= 0 )
    {
      h.push_back( make_hint( 1, "A tendon tap sends one brief volley down the Ia fibres: one synapse onto the motoneurons has to be strong enough to fire several cells at once.", "" ) );
      h.push_back( make_hint( 2, "The motoneuron pool did not answer the tap.", "ag_motor" ) );
      h.push_back( make_hint( 3, "Make the Ia > motoneuron synapse stronger, give the pools tonic drive so they sit near threshold, and let the relay cells fire regularly at the afferent rate (a slower membrane helps).", "" ) );
    }
    else if ( osc > 0.08 )
    {
      h.push_back( make_hint( 1, "Negative feedback with a delay rings when its gain is too high.", "" ) );
      h.push_back( make_hint( 2, "The reflex loop is oscillating: lower the Ia to motoneuron gain.", "ag_Ia" ) );
      h.push_back( make_hint( 3, "Lower the Ia > motoneuron gain a little, and make the reciprocal inhibition no stronger than the excitation.", "" ) );
    }
    else if ( rec < tol_min( mode_, 0.20, 1.25, 0.85 ) )
    {
      h.push_back( make_hint( 1, "Reciprocal inhibition: when a muscle is stretched the opposing muscle must be told to relax, or they fight.", "" ) );
      h.push_back( make_hint( 2, "The antagonist pool keeps firing: it needs an inhibitory interneuron from the opposite Ia.", "ant_motor" ) );
      h.push_back( make_hint( 3, "Let each Ia group excite a small inhibitory interneuron group, and let that group project onto the opposite pool; strengthen that inhibitory link until the opposite pool quietens.", "" ) );
    }
    else if ( res < tol_min( mode_, 0.30, 1.25, 0.85 ) )
    {
      h.push_back( make_hint( 1, "Recruitment: the more the pool is driven, the more motor units fire, so a bigger pool gives more force.", "" ) );
      h.push_back( make_hint( 2, "The reflex is too weak to cut the deflection: strengthen the Ia link or enlarge the pool.", "ag_motor" ) );
      h.push_back( make_hint( 3, "Use larger motoneuron pools, and make the Ia > motoneuron link stronger with some spread in the weights so the pool is recruited gradually.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "A spinal reflex starts very soon after the tap, because it is only a few synapses long.", "" ) );
      h.push_back( make_hint( 2, "Check the path from the Ia input to the motor pool for extra relay steps or slow synapses.", "ag_Ia" ) );
      h.push_back( make_hint( 3, "Ia group > motoneuron pool, with no more than one relay on the way; a relay with a slower membrane fires regularly at the afferent rate.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override { return { { "latency_ms", make_metric( score().parts[ "latency_ms" ], "ms", true ) } }; }
  Note notebook() const override
  {
    return { "The knee-jerk is one synapse",
             "The stretch reflex is monosynaptic: a spindle Ia afferent excites the alpha motoneurons of the same muscle directly, and "
             "branches onto an interneuron that inhibits the antagonist, which is why the tap on the patellar tendon gives a twitch in "
             "about 20-30 ms in the leg.",
             "Lloyd DPC (1943) J Neurophysiol 6:111; Eccles JC, Eccles RM, Lundberg A (1957) J Physiol 137:22" };
  }

  std::vector< Event > timeline() const override
  {
    std::vector< Event > ev;
    ev.push_back( { tap_[ 0 ], tap_[ kTaps - 1 ] - tap_[ 0 ] + 50, "tendon taps" } );
    for ( int k = 0; k < kN; ++k )
      ev.push_back( { onset_[ k ], 2 * kRamp + kHold, k % 2 == 0 ? "stretch flexor" : "stretch extensor" } );
    return ev;
  }
  std::vector< Band > bands() const override
  {
    // the open-loop limb is pushed about 0.3 rad; the reflex must hold it inside +-0.17 rad of its rest angle
    return { { "theta_dev_rad", "rad", -0.17, 0.17 } };
  }

  void reset( uint64_t seed ) override
  {
    Rng rng( seed ^ 0x73747265746368ULL );
    for ( int k = 0; k < kTaps; ++k )
      tap_[ k ] = kTapStart + kTapSpacing * k + rng.uniform( 0, 30 );
    for ( int k = 0; k < kN; ++k )
      onset_[ k ] = kLead + kSpacing * k + rng.uniform( 0, kJitter );
    LimbRigConfig c;
    base_reset( c, 0.2 );
    th_.clear();
    sh_.clear();
    sc_.clear();
    hist_ag_.clear();
    hist_ant_.clear();
    hist_gam_.clear();
    ctrl_on_ = ctrl_fresh_ = false;
    shadow_on_ = -1;
    theta_pre_ = 0.2;
  }

  Score score() const override
  {
    Score s;
    Metrics m = analyse();
    s.parts[ "latency_ms" ] = m.latency;
    s.parts[ "latency_flexor_ms" ] = m.lat_f;
    s.parts[ "latency_extensor_ms" ] = m.lat_e;
    s.parts[ "resistance" ] = m.resistance;
    s.parts[ "antagonist_suppression" ] = m.recip;
    s.parts[ "osc_rad" ] = m.osc;
    s.parts[ "deflection_rad" ] = m.dev;
    s.parts[ "deflection_open_loop_rad" ] = m.dev_open;
    s.parts[ "stretches_done" ] = m.done;
    const bool done = t_ >= quick_duration_ms() - 1e-6;  // the quick check's length: two stretches (a full run has all four)
    const bool tapped = m.lat_f >= 0 && m.lat_e >= 0;
    const double lat_s = tapped ? clampd( ( 80.0 - m.latency ) / 45.0, 0, 1 ) : 0.0;
    s.value = 100.0 * ( 0.35 * std::min( 1.0, m.resistance / 0.5 ) + 0.25 * lat_s + 0.25 * std::min( 1.0, m.recip / 0.5 ) +
                        0.15 * ( m.osc_flag ? 0.0 : 1.0 ) );
    if ( !tapped || m.osc_flag )
      s.value = std::min( s.value, 49.0 );
    if ( !wiring_.ok() )
      s.value = 0;  // an empty bench scores nothing ("no ringing" is not an achievement when nothing is wired)
    const mechb::Issue* mech = mech_bad();
    if ( mech )
      s.value = std::min( s.value, 40.0 );  // behaviour without the mechanism the level teaches is not a solution (docs/REQ-0.4.md rule 1)
    s.passed = done && m.done >= 2 && !mech && wiring_.ok() && tapped && m.latency <= tol_max( mode_, 50.0 ) && m.resistance >= tol_min( mode_, 0.30 ) && m.recip >= tol_min( mode_, 0.20 ) && !m.osc_flag;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 0 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( mech )
      o << mech->fail;
    else if ( spikes_ag_ < 1 && spikes_ant_ < 1 )
      o << "Both motor pools are silent" << yet << " Give the motoneurons tonic drive and a way to hear the Ia afferents.";
    else if ( t_ < kTapStart + kTapSpacing * kTaps )
      o << "Tapping the tendons" << yet;
    else if ( !tapped )
    {
      // Never blame the wiring: the channels are connected (checked above). Say what was measured instead.
      const bool nof = m.lat_f < 0, noe = m.lat_e < 0;
      const char* which = nof && noe ? "either pool" : nof ? "the flexor pool" : "the extensor pool";
      o << "No reflex to the tendon taps in " << which << ": a brief Ia volley must recruit a good part of the motoneuron pool at once, and it did not.";
      if ( m.stretch_resp )
        o << " The pool does respond to a sustained stretch, so strengthen the Ia -> motoneuron synapses (or give the pool more Ia fibres) until one volley recruits more cells.";
      else if ( m.done > 0 )
        o << " Nor does it respond to a stretch: check that the Ia fibres excite their own motoneurons and that the motoneurons sit close enough to threshold.";
    }
    else if ( m.done == 0 )
      o << "Waiting for the first push" << yet;
    else if ( m.osc_flag )
      o << "Reflex rings: the limb oscillates after the push. Gain is too high (clonus).";
    else if ( m.latency > tol_max( mode_, 50.0 ) )
      o << "The reflex starts too long after a tap to be a spinal reflex: the path from the afferent to the motoneurons is too slow (extra relays, or a link too weak to fire the pool at once).";
    else if ( m.recip < tol_min( mode_, 0.20 ) && inhib_.armed() && inhib_.some_silent() )
      o << "The reflex is fast enough, but the opposing muscle keeps firing. " << inhib_.say( "interneurons that should inhibit the opposing muscle" )
        << " (one Ia volley may be too weak to fire them: give them the steady drive too, or more Ia fibres).";
    else if ( m.recip < tol_min( mode_, 0.20 ) )
      o << "The reflex is fast enough, but the opposing muscle keeps firing: co-contraction makes the limb stiff and the reflex weak. Strengthen the reciprocal Ia inhibition.";
    else if ( m.resistance < tol_min( mode_, 0.30 ) )
      o << "The reflex is fast enough but cuts the deflection too little: it is too weak. Strengthen the Ia > motoneuron link or enlarge the pools.";
    else
      o << ( done ? "Stretch reflex: it starts promptly after the tap, cuts the deflection, suppresses the antagonist and does not ring." : "Stretch reflex so far is on track." );
    s.verdict = o.str();
    return s;
  }

protected:
  void mechanism( const mechb::Graph& g, std::vector< mechb::Issue >& out ) const override { mechb::limb_stretch_reflex( g, out ); }
  double torque( double t ) const override
  {
    double tq = 0;
    for ( int k = 0; k < kN; ++k )
      tq += ( k % 2 == 0 ? -1.0 : 1.0 ) * ( variant_ == "heavy_load" ? 3.5 : kTorque ) * push_profile( t, onset_[ k ], kHold, kRamp );
    return tq;
  }
  void cues( Engine& e, double t ) override
  {
    put_cue( e, "drive", 1.0 );
    // tendon taps: a short Ia volley on the tapped muscle's channel, arriving at the cord one conduction delay after the tap
    for ( int k = 0; k < kTaps; ++k )
    {
      const double a = tap_[ k ] + rig_.cfg.aff_delay_ms;
      if ( t >= a && t < a + kTapLen )
        put( e, k % 2 == 0 ? "ag_Ia" : "ant_Ia", ( k % 2 == 0 ? rig_.ia_ag : rig_.ia_ant ) + kTapRate );
    }
  }
  void after_tick( double h ) override
  {
    // start / stop the open-loop copy
    for ( int k = 0; k < kN; ++k )
    {
      if ( shadow_on_ < 0 && t_ >= onset_[ k ] - 1 && t_ < onset_[ k ] + 5 )
      {
        // the tonic level the open-loop copy holds is the mean drive over the last kTonicWin ms: the pools fire in lumpy ~100 ms waves, so the rig's 120 ms
        // average is a noisy estimate of the tonic level, and a copy held at a lumpy level drifts on its own (see sc_)
        double a = 0, b = 0, g = 0;
        const size_t n = std::min< size_t >( hist_ag_.size(), static_cast< size_t >( kTonicWin ) );
        for ( size_t i = 0; i < n; ++i )
        {
          a += hist_ag_[ hist_ag_.size() - 1 - i ];
          b += hist_ant_[ hist_ant_.size() - 1 - i ];
          g += hist_gam_[ hist_gam_.size() - 1 - i ];
        }
        if ( n > 0 ) { a /= n; b /= n; g /= n; }
        ctrl_on_ = true;
        sh_levels_[ 0 ] = a, sh_levels_[ 1 ] = b, sh_levels_[ 2 ] = g;
        rig_.freeze_shadow_at( a, b, g );
        ctrl_ = rig_.shadow;   // the control: the same copy without the push
        shadow_on_ = k;
      }
      if ( shadow_on_ == k && t_ >= onset_[ k ] + 800 )
      {
        rig_.release_shadow();
        ctrl_on_ = false;
        shadow_on_ = -1;
      }
    }
    if ( std::fabs( tq_ ) < 1e-9 )
      theta_pre_ += ( rig_.joint.theta - theta_pre_ ) / 150.0;
    hist_ag_.push_back( rig_.exc_ag );
    hist_ant_.push_back( rig_.exc_ant );
    hist_gam_.push_back( rig_.gam );
    if ( ctrl_on_ && ctrl_fresh_ )
    {
      body::JointInput in;
      in.ag.excitation = sh_levels_[ 0 ];
      in.ant.excitation = sh_levels_[ 1 ];
      in.gamma_dyn = in.gamma_sta = sh_levels_[ 2 ];
      in.ext_torque = 0;
      ctrl_.step( h, in );
    }
    ctrl_fresh_ = ctrl_on_;   // the control is stepped from the tick after the freeze, like the copy
    th_.push_back( rig_.joint.theta );
    sh_.push_back( rig_.shadow_active() ? rig_.shadow.theta : rig_.joint.theta );
    sc_.push_back( rig_.shadow_active() ? ctrl_.theta : rig_.joint.theta );
  }
  bool watch_inhib_now() const override { return t_ >= kLead; }   // the interneurons must fire during the imposed stretches (a steady Ia rate), not only at the 3 ms tap volleys
  double target_rad() const override { return theta_pre_; }  // the angle the reflex must hold: the one before each push
  bool has_target() const override { return true; }
  void extra_readouts( std::map< std::string, double >& r ) const override
  {
    r[ "theta_dev_rad" ] = rig_.joint.theta - theta_pre_;
    r[ "open_loop_theta_rad" ] = rig_.shadow_active() ? rig_.shadow.theta : rig_.joint.theta;
  }

private:
  struct Metrics
  {
    double latency = 0, lat_f = -1, lat_e = -1, resistance = 0, recip = 0, osc = 0, dev = 0, dev_open = 0;
    int done = 0, osc_flag = 0, stretch_resp = 0;  // stretch_resp: the stretched muscle's pool fires faster during a stretch
  };

  Metrics analyse() const
  {
    Metrics m;
    // T-reflex latency per pool
    std::vector< double > tf, te;
    for ( int k = 0; k < kTaps; ++k )
      if ( t_ >= tap_[ k ] + 150 )
        ( k % 2 == 0 ? tf : te ).push_back( tap_[ k ] );
    const double eff = rig_.cfg.eff_delay_ms;
    if ( tf.size() == kTaps / 2 )
    {
      const double l = psth_latency( cnt_ag_, tf, pool_ag_ );
      m.lat_f = l >= 0 ? l + eff : -1;
    }
    if ( te.size() == kTaps / 2 )
    {
      const double l = psth_latency( cnt_ant_, te, pool_ant_ );
      m.lat_e = l >= 0 ? l + eff : -1;
    }
    if ( m.lat_f >= 0 && m.lat_e >= 0 )
      m.latency = 0.5 * ( m.lat_f + m.lat_e );
    else
      m.latency = std::max( m.lat_f, m.lat_e );
    // stretches
    double res = 0, rec = 0, dev = 0, devo = 0;
    int nres = 0, nrec = 0, sresp[ 2 ] = { 0, 0 };
    for ( int k = 0; k < kN; ++k )
    {
      const double t0 = onset_[ k ];
      if ( t_ < t0 + kEnd )
        break;
      ++m.done;
      const bool flexor_stretched = k % 2 == 0;
      const auto& A = flexor_stretched ? cnt_ant_ : cnt_ag_;
      const double nA = flexor_stretched ? pool_ant_ : pool_ag_;
      const double th0 = window_mean( th_, t0 - 100, t0 );
      double d = 0, d_open = 0;
      const long end = static_cast< long >( t0 + 2 * kRamp + kHold + 100 );
      for ( long i = static_cast< long >( t0 ); i < end && i < static_cast< long >( th_.size() ); ++i )
      {
        d = std::max( d, std::fabs( th_[ i ] - th0 ) );
        d_open = std::max( d_open, std::fabs( sh_[ i ] - sc_[ i ] ) );   // what the push alone does to the open-loop copy: its drift without the push (sc_) is taken out
      }
      dev += d;
      devo += d_open;
      res += d_open > 1e-6 ? clampd( 1.0 - d / d_open, 0, 1 ) : 0.0;
      ++nres;
      const double abase = window_mean( A, t0 - kRecBase, t0 ) * 1000.0 / nA;
      const double a_mean = window_mean( A, t0 + 20, t0 + kRecEnd ) * 1000.0 / nA;
      {
        // the muscle being stretched (flexor on even stretches) should speed up
        const std::vector< double >& S = k % 2 == 0 ? cnt_ag_ : cnt_ant_;
        const double nS = k % 2 == 0 ? std::max( 1.0, pool_ag_ ) : std::max( 1.0, pool_ant_ );
        const double sb = window_mean( S, t0 - 150, t0 ) * 1000.0 / nS, sm = window_mean( S, t0 + 20, t0 + 120 ) * 1000.0 / nS;
        if ( sm > sb + 3.0 && sm > 1.15 * sb )
          sresp[ k % 2 ] = 1;
      }
      if ( abase > 1.0 )
      {
        rec += clampd( 1.0 - a_mean / abase, 0, 1 );
        ++nrec;
      }
      if ( t_ >= t0 + kQuiet1 )
      {
        double p2p = 0;
        if ( oscillating( th_, static_cast< long >( t0 + kQuiet0 ), static_cast< long >( t0 + kQuiet1 ), p2p ) )
          m.osc_flag = 1;
        m.osc = std::max( m.osc, p2p );
      }
    }
    m.stretch_resp = ( sresp[ 0 ] || sresp[ 1 ] ) ? 1 : 0;
    if ( nrec > 0 )
      m.recip = rec / nrec;
    if ( nres > 0 )
    {
      m.resistance = res / nres;
      m.dev = dev / nres;
      m.dev_open = devo / nres;
    }
    return m;
  }

  double tap_[ kTaps ] = {}, onset_[ kN ] = {};
  std::vector< double > th_, sh_, sc_, hist_ag_, hist_ant_, hist_gam_;
  body::Joint ctrl_;
  bool ctrl_on_ = false, ctrl_fresh_ = false;
  double sh_levels_[ 3 ] = { 0, 0, 0 };
  int shadow_on_ = -1;
  double theta_pre_ = 0.2;
};

} // namespace

std::unique_ptr< Task > make_stretch_reflex() { return std::make_unique< StretchReflexTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( stretch_reflex, 100, "stretch_reflex.json", make_stretch_reflex ) }
