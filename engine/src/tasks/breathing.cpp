// Task "breathing": a preBotzinger burster network drives the phrenic pool and the lungs; a CO2 chemoreflex keeps PaCO2 in band.
//
// WORLD (spike::body::Lungs, docs/body/lungs.md; times in s of game time, 1 ms slices)
//   Sensor "drive":  constant brainstem / wakefulness drive, a Poisson rate of kDriveHz (200 Hz) into whichever cells the circuit wires.
//   Sensor "chemo":  chemoreceptor afferent drive as a graded current (pA, dc input) = kChemoPA * (peripheral_hz + central_hz) of the lung
//                    model: carotid body (PaCO2, fast) plus central (tissue/brain PCO2, slow). Rest is about 13 Hz -> 51 pA, 5 % CO2 about 3x that.
//                    (A graded current rather than spikes: the afferent population code is summarised as one smooth drive.)
//   Motor "phrenic": mean rate per phrenic motoneuron (Hz). Inspiratory drive u = clamp(rate / kPhrenicRef, 0, 1) goes to the lung's
//                    diaphragm (Pmus = 20 cmH2O * u, lowpass 80 ms); expiration is passive.
//   Gas exchange runs kTimeScale times faster than real physiology (game pacing; steady states unchanged).
//
// PERTURBATION  (timeline): 5 % CO2 added to the inspired air (inspired PCO2 kInspired = 30 mmHg) from 50 s to 110 s, ramped over 5 s.
//   Without a chemoreflex, PaCO2 climbs toward PI + 863 VCO2 / VA = 30 + 41 = 71 mmHg.
//
// MECHANISM (Engine::structure(), checked at the start and every 0.5 s): a rhythm population, that is a group on a cycle of excitatory projections
//   (recurrent coupling among >= 2 cells) that drives the cells 'phrenic' reads (or is read by it), and the 'chemo' input must reach that rhythm
//   group (an excitatory path from the chemoreceptor input to it): the CO2 reflex acts on the rhythm generator itself, not only on the motoneurons.
//   Missing either caps the score below the pass bar, and the verdict names it.
//
// SCORING (windows are game time)
//   rest      12-48 s : breaths per minute from breath completions in the window, VE, mean PaCO2
//   challenge 85-110 s: VE (sum of tidal volumes / time), peak PaCO2 over 50-110 s
//   recovery  135-150 s: mean PaCO2
//   pass  : rest rate 12-20 /min with >= 5 breaths, VE(challenge) >= 1.6 x VE(rest), rest PaCO2 in 35-45, peak PaCO2 <= 52 mmHg and
//           recovery PaCO2 <= 46 mmHg
//   value = 100 * (0.25 rate + 0.30 ventilation response + 0.30 PaCO2 control + 0.15 recovery), each a 0..1 soft score.
//
// Biology: preBotC bursting pacemakers (Smith et al. 1991 Science 254:726; Butera, Rinzel & Smith 1999 J Neurophysiol 82:382) sit inside a
// recurrently excitatory network (Feldman & Del Negro 2006 Nat Rev Neurosci 7:232); chemosensory input from retrotrapezoid neurons
// (Guyenet & Mulkey 2010 Respir Physiol Neurobiol 173:244; Guyenet, Stornetta & Bayliss 2010 J Comp Neurol 518:3883) and the carotid body sets the drive. Eupnoea 12-20 /min (West,
// Respiratory Physiology). The chemoreflex gain here is a game value (a few L/min per mmHg PaCO2 for real subjects).
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include "spike/body/lungs.h"
#include "mech_util.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr double kDuration = 150000;
constexpr double kDriveHz = 200, kChemoPA = 4, kPhrenicRef = 225;
constexpr double kInspired = 30;
// The scripted run: the CO2 breath (ms; on, off, ramp), the end, the gas exchange clock (x faster than physiology) and the scoring windows (s). The quick
// check runs the same test with a shorter rest, a 23 s CO2 breath and shorter windows, on the SAME body clock (gas exchange scale 5, ramps 5 s) and scored by the
// same rules: a quick check must not change the physiology it judges. The rest window cannot shrink below the time that holds the 5 breaths the rule asks for at
// 12 per minute, so this level cannot be made much shorter than this.
struct Sched
{
  double co2_on, co2_off, ramp, end, ts, rest_a, rest_b, ch_a, ch_b, peak_a, rec_a, rec_b;
};
constexpr Sched kFull{ 50000, 110000, 5000, kDuration, 5, 12, 48, 85, 110, 50, 135, 150 };
constexpr Sched kQuick{ 33000, 56000, 5000, 70000, 5, 8, 33, 46, 56, 33, 62, 70 };

class BreathingTask : public Task
{
public:
  BreathingTask() { reset( 1 ); }
  std::string id() const override { return "breathing"; }
  std::string title() const override { return "Breathing rhythm"; }
  std::string brief() const override
  {
    return "Breathing is rhythmic because a small network in the brainstem, the preBotzinger complex, contains neurons that burst "
           "on their own (persistent sodium current) and excite each other into a shared burst every few seconds. The bursts excite "
           "phrenic motoneurons, the phrenic nerve drives the diaphragm, and the lungs fill. Blood CO2 closes the loop: chemoreceptors "
           "report PaCO2, and the brainstem answers with faster and deeper breaths. Score: 12-20 breaths per minute at rest; when the air "
           "gets 5% CO2 (50-110 s) ventilation must rise at least 1.6x and PaCO2 must stay below 52 mmHg; it must come back afterwards. "
           "";
  }
  /// The cell type the reference circuit and the verdicts assume; the numbers of the build live in recipe() (shown by mode in the lab).
  std::string default_model() const override { return "butera_burster"; }
  std::string recipe() const override
  {
    return "A recipe that works, in steps (units: weights of burster synapses are nS, of iaf cells pA; 'drive' is a Poisson train, "
           "'chemo' is a current in pA): (1) about 10 butera_burster cells (PBC), g_NaP 2.8 nS and g_tonic 0.29 nS, each excited by every "
           "other with a WEAK weight of about 0.03 nS (not 1: strong coupling gives 100+ breaths a minute); 'drive' onto PBC at weight "
           "0.005. Alone a burster fires a burst every 4-5 s. (2) 8 iaf_psc_alpha phrenic cells (PHR) with I_e about 380 pA, excited by "
           "PBC at about 2 pA, read out as 'phrenic'. (3) A few tonic iaf 'expiratory' cells (I_e about 430 pA) that PBC inhibits "
           "(weight about -60) and that inhibit PHR (about -30): they keep the phrenic pool quiet between breaths. (4) Route 'chemo' "
           "through relay cells (iaf, I_e about 335 pA, weight 1) to PBC (about 0.09 nS: breathing gets faster, the 'frequency' path) and "
           "to PHR (about 8 pA: breaths get deeper). If the rate is wrong, change the burster drive, not the coupling: g_tonic 0.30 gives a "
           "burst every 4.9 s, 0.35 every 2.6 s. The phrenic cells need slow synapses to smooth the bursts into breaths: ADVANCED tau syn E and tau "
           "syn I 20 ms on PHR, tau syn I 10 ms on the expiratory cells. Sensor weights: drive 0.005, chemo 1 (pA per unit of its value).";
  }
  std::vector< std::string > sensor_channels() const override { return { "drive", "chemo" }; }
  std::vector< std::string > motor_channels() const override { return { "phrenic" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    ChannelKind d, c;
    d.kind = "poisson";
    d.value = kDriveHz;
    c.kind = "dc";  // graded current (pA)
    c.value = 51;
    d.weight = 0.005;
    c.weight = 1;
    return { { "drive", d }, { "chemo", c }, { "phrenic", ChannelKind{ "rate" } } };
  }
  double duration_ms() const override { return kDuration; }
  /// Quick check: the same test, compressed (see Sched). The episode is chosen by Task::set_quick() (before reset()); it is the one the task scripts and
  /// scores (spike-run and the test harness call set_quick() before reset()).
  double quick_duration_ms() const override
  {
    return kQuick.end;
  }
  std::string chapter() const override { return "breathing"; }
  std::string view() const override { return "lungs"; }
  int par_neurons() const override { return variant_par( variant_, 26 ); }

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }
  std::vector< std::string > concepts_used() const override { return { "rhythm_gating", "pacemaking", "coupling", "excitation", "negative_feedback" }; }
  bool on_spine() const override { return true; }
  /// Pass bars (rest rate, resting PaCO2, ventilation ratio, PaCO2 peak, recovery): guided 10-22 /min, 33-47, 1.4x, 55, 48; standard 12-20,
  /// 35-45, 1.6x, 52, 46; researcher 13-19, 36-44, 1.8x, 52, 45. Inspired CO2 x0.8 guided, x1.1 researcher.
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 13 instead of 26" },
             { "strong_co2", "the CO2 challenge is 1.15 times as strong" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    const double rate = s.parts[ "rest_rate_bpm" ];
    std::vector< Hint > h;
    if ( !mech_.ok() )
    {
      h.push_back( make_hint( 1, "Rhythm: breathing comes from a network whose cells excite one another into a shared burst, and CO2 must act on that network, not only on the motoneurons.", "" ) );
      h.push_back( make_hint( 2, "Either the rhythm group is not coupled to itself, or the chemo input does not reach it.", "chemo" ) );
      h.push_back( make_hint( 3, "Connect a group of several bursting cells to itself with weak excitatory synapses, drive the phrenic pool from it, and route the chemo input (through a relay if you like) onto that group.", "" ) );
    }
    else if ( s.parts[ "phrenic_spikes" ] < 1 || s.parts[ "rest_breaths" ] < 5 )
    {
      h.push_back( make_hint( 1, "Rhythm: bursting pacemaker cells fire a burst, rest, and burst again by themselves. A network of coupled bursters shares one burst.", "" ) );
      h.push_back( make_hint( 2, "No regular breaths reach the phrenic pool.", "phrenic" ) );
      h.push_back( make_hint( 3, "Use burster cells in the rhythm group, give the phrenic cells a standing drive and a link from the rhythm group strong enough to fire them, and drive the bursters.", "" ) );
    }
    else if ( rate < rlo() || rate > rhi() )
    {
      h.push_back( make_hint( 1, "The burst rate is set by the drive to the bursters, not by the coupling.", "" ) );
      h.push_back( make_hint( 2, rate > rhi() ? "Too fast: lower the drive onto the bursters." : "Too slow: raise the drive onto the bursters.", "drive" ) );
      h.push_back( make_hint( 3, rate > rhi() ? "Lower the burster drive or weaken the coupling; strong coupling makes the rhythm run away." : "Raise the burster drive a little; keep the coupling weak.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "Sensory gating: CO2 must make breathing faster and deeper. The chemosensor reports PaCO2.", "" ) );
      h.push_back( make_hint( 2, "Ventilation does not rise enough with CO2: the chemo input is not reaching the rhythm and the motor cells.", "chemo" ) );
      h.push_back( make_hint( 3, "Relay the chemo input onto the bursters (faster breaths) and onto the phrenic cells (deeper breaths), and make those links strong enough to change the ventilation.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override { return { { "paco2_peak", make_metric( score().parts[ "paco2_peak" ], "mmHg", true ) } }; }
  Note notebook() const override
  {
    return { "Where the breathing rhythm comes from",
             "The preBotzinger complex of the medulla generates the inspiratory rhythm. Some of its neurons burst on their own through a persistent "
             "sodium current, and in the pacemaker hypothesis excitatory coupling among them synchronises the bursts into the rhythm; in the "
             "group-pacemaker hypothesis the rhythm emerges from the recurrent excitation of the network itself, and it can survive when the persistent "
             "sodium current is blocked. This level builds the pacemaker version; the question is still open. CO2 sensed by retrotrapezoid neurons "
             "raises the frequency and the drive to the motoneurons.",
             "Smith JC et al. (1991) Science 254:726; Feldman JL, Del Negro CA (2006) Nat Rev Neurosci 7:232 (review of both hypotheses)" };
  }
  std::vector< Event > timeline() const override { return { { kFull.co2_on, kFull.co2_off - kFull.co2_on, "5% CO2 inhaled" } }; }
  std::vector< Band > bands() const override { return { { "rr_per_min", "/min", rlo(), rhi() }, { "co2_mmhg", "mmHg", palo(), pahi() } }; }

  void reset( uint64_t ) override
  {
    t_ = 0;
    sch_ = kFull;
    lungs_ = body::Lungs( params( sch_.ts ) );
    lungs_.reset();
    u_ = 0;
    drive_lp_.tau_ms = 30;
    drive_lp_.reset( 0 );
    log_.clear();
    last_breaths_ = 0;
    pa_.clear();
    ve_hud_ = rr_hud_ = 0;
    spikes_ = 0;
    motor_.invalidate();
    wiring_.invalidate();
    mech_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    if ( t_ == 0 && ( quick_ ? kQuick.end : kFull.end ) != sch_.end )  // the schedule follows the episode asked for
    {
      sch_ = quick_ ? kQuick : kFull;
      lungs_ = body::Lungs( params( sch_.ts ) );
      lungs_.reset();
    }
    wiring_.probe( e, sensor_channels(), motor_channels() );
    mech_.refresh( e, t_, []( const Structure& st ) { return rhythm_missing( st ); } );
    e.set_input( "drive", kDriveHz );
    e.set_input( "chemo", kChemoPA * ( lungs_.peripheral_hz + lungs_.central_hz ) );
    e.step( dt );
    double hz = motor_.hz_per_neuron( e, "phrenic" );
    spikes_ += motor_.spike_count( e, "phrenic", dt );
    u_ = clampd( drive_lp_.step( hz / kPhrenicRef, dt ), 0, 1 );
    body::LungInput in;
    in.drive = u_;
    in.inspired_pco2 = inspired() * clampd( ( t_ - sch_.co2_on ) / sch_.ramp, 0, 1 ) * clampd( ( sch_.co2_off + sch_.ramp - t_ ) / sch_.ramp, 0, 1 );
    lungs_.step( dt, in );
    t_ += dt;
    if ( lungs_.breaths != last_breaths_ )
    {
      last_breaths_ = lungs_.breaths;
      log_.push_back( { t_ / 1000.0, lungs_.tidal_l } );
    }
    while ( pa_.size() < static_cast< size_t >( t_ / 100.0 + 1e-9 ) )
      pa_.push_back( lungs_.paco2 );
    // HUD: rate and VE over the last 20 s
    double span = std::min( 20.0, std::max( t_ / 1000.0, 2.0 ) );
    int n = 0;
    double vt = 0;
    for ( auto& b : log_ )
      if ( b.t > t_ / 1000.0 - span )
        ++n, vt += b.vt;
    rr_hud_ = n * 60.0 / span;
    ve_hud_ = vt * 60.0 / span;
  }

  BodyState body() const override
  {
    BodyState b;
    auto v = lungs_.view();
    b.readouts[ "volume" ] = v.volume_frac;
    b.readouts[ "rr_per_min" ] = rr_hud_;
    b.readouts[ "co2_mmhg" ] = lungs_.paco2;
    b.readouts[ "ventilation" ] = ve_hud_;
    b.readouts[ "tidal_l" ] = lungs_.tidal_l;
    b.readouts[ "flow_l_s" ] = lungs_.flow_l_s;
    b.readouts[ "phrenic_drive" ] = u_;
    b.readouts[ "tissue_pco2_mmhg" ] = lungs_.tissue_pco2;
    b.readouts[ "chemo_hz" ] = lungs_.peripheral_hz + lungs_.central_hz;
    b.readouts[ "inspiring" ] = lungs_.inspiring ? 1.0 : 0.0;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    Win rest = window( sch_.rest_a, sch_.rest_b ), ch = window( sch_.ch_a, sch_.ch_b );
    double pa_rest = mean_pa( sch_.rest_a, sch_.rest_b ), pa_peak = max_pa( sch_.peak_a, sch_.ch_b ), pa_rec = mean_pa( sch_.rec_a, sch_.rec_b );
    double ratio = rest.ve > 0.05 ? ch.ve / rest.ve : ( ch.ve > 0.05 ? 99.0 : 0.0 );
    s.parts[ "rest_rate_bpm" ] = rest.rate;
    s.parts[ "rest_breaths" ] = rest.n;
    s.parts[ "ve_rest_l_min" ] = rest.ve;
    s.parts[ "ve_challenge_l_min" ] = ch.ve;
    s.parts[ "ve_ratio" ] = ratio;
    s.parts[ "paco2_rest" ] = pa_rest;
    s.parts[ "paco2_peak" ] = pa_peak;
    s.parts[ "paco2_recovery" ] = pa_rec;
    s.parts[ "phrenic_spikes" ] = spikes_;
    double rate_ok = rest.n >= 5 ? soft_band( rest.rate, rlo(), rhi(), 6 ) : 0.0;
    double resp = clampd( ( ratio - 1.0 ) / 0.6, 0, 1 );
    double ctrl = ( ( pa_rest >= palo() && pa_rest <= pahi() ) ? 0.4 : 0.0 ) + 0.6 * clampd( ( 70.0 - pa_peak ) / 18.0, 0, 1 );
    double rec = soft_band( pa_rec, 33, 46, 8 );
    s.value = 100.0 * ( 0.25 * rate_ok + 0.30 * resp + 0.30 * ctrl + 0.15 * rec );
    bool done = t_ >= sch_.end;
    bool pass = done && rest.n >= 5 && rest.rate >= rlo() && rest.rate <= rhi() && ratio >= rmin() && pa_rest >= palo() && pa_rest <= pahi() &&
                pa_peak <= pmax() && pa_rec <= recmax();
    s.passed = pass && mech_.ok();
    if ( !mech_.ok() )
      s.value = std::min( s.value, mech::kNoMechanismCap );
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !mech_.ok() )
      o << mech_.missing();
    else if ( spikes_ < 1 )
      o << "The phrenic pool never fired" << yet << " Do the bursters burst? The phrenic cells need a standing drive, and the link from the rhythm group must be strong enough to fire them.";
    else if ( rest.n < 5 )
      o << "Too few breaths during the rest" << yet << " Need a regular rhythm: raise the burster drive "
        << "and make sure the bursters are coupled so they burst together; if the phrenic pool is "
        << "held silent, check the expiratory inhibition.";
    else if ( rest.rate < rlo() || rest.rate > rhi() )
      o << "The resting rate is out of range: "
        << ( rest.rate > rhi() ? "too fast. Weaken the burster coupling, or lower the drive (the cells' own drive or the chemo weight to the bursters)."
                            : "too slow. Raise the drive to the bursters (their own drive or more chemo weight) or strengthen the coupling a little." );
    else if ( pa_rest < palo() || pa_rest > pahi() )
      o << "Resting blood CO2 is out of range: the breaths are too " << ( pa_rest > pahi() ? "shallow" : "deep" ) << ".";
    else if ( !done && t_ < sch_.ch_b * 1000.0 )
      o << "Resting breathing is in range; the CO2 test is still running.";
    else if ( ratio < rmin() )
      o << "Ventilation did not rise enough under the CO2 challenge: no chemoreflex.";
    else if ( pa_peak > pmax() )
      o << "Ventilation rose, but blood CO2 still climbed too high: the response is too weak or too slow.";
    else if ( pa_rec > recmax() && done )
      o << "Blood CO2 had not recovered afterwards.";
    else
      o << ( done ? "Breathing held: a steady resting rhythm, faster and deeper under CO2, and blood CO2 recovered." : "Breathing so far is on track." );
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { 0, 0, 1, 1 }; }

private:
  // the mechanism: a recurrently excitatory rhythm group that drives 'phrenic' and is reached by the 'chemo' input
  static std::string rhythm_missing( const Structure& st )
  {
    const auto phr = mech::out_pops( st, "phrenic" ), chemo = mech::in_pops( st, "chemo" );
    mech::PopSet rhythm;
    for ( int p = 0; p < static_cast< int >( st.populations.size() ); ++p )
      if ( mech::excitatory_loop( st, { p } ) && mech::path( st, { p }, phr, +1 ) )
        rhythm.insert( p );
    if ( rhythm.empty() )
      return "No rhythm generator: nothing here is a group of cells that excite one another (recurrent coupling) and drive the phrenic pool. "
             "Breathing needs a coupled burst network, not a tonic cell.";
    if ( !mech::path( st, chemo, rhythm, +1 ) )
      return "The chemoreceptor input does not reach the rhythm group: CO2 must speed up the rhythm generator itself, not only drive the motoneurons. "
             "Route the chemo input onto the coupled burst network.";
    return "";
  }

  double rlo() const { return mode_ == "guided" ? 10 : ( mode_ == "researcher" ? 13 : 12 ); }
  double rhi() const { return mode_ == "guided" ? 22 : ( mode_ == "researcher" ? 19 : 20 ); }
  double palo() const { return mode_ == "guided" ? 33 : ( mode_ == "researcher" ? 36 : 35 ); }
  double pahi() const { return mode_ == "guided" ? 47 : ( mode_ == "researcher" ? 44 : 45 ); }
  double rmin() const { return mode_ == "guided" ? 1.4 : ( mode_ == "researcher" ? 1.8 : 1.6 ); }
  double pmax() const { return mode_ == "guided" ? 55 : 52; }
  double recmax() const { return mode_ == "guided" ? 48 : ( mode_ == "researcher" ? 45 : 46 ); }
  double inspired() const { return kInspired * mode_factor( mode_, 0.8, 1.1 ) * ( variant_ == "strong_co2" ? 1.15 : 1.0 ); }
  struct Breath { double t, vt; };
  struct Win { double rate = 0, ve = 0, n = 0; };

  static body::LungParams params( double time_scale )
  {
    body::LungParams p;
    p.time_scale = time_scale;
    return p;
  }
  static double soft_band( double v, double lo, double hi, double width )
  {
    double d = v < lo ? lo - v : ( v > hi ? v - hi : 0.0 );
    return clampd( 1.0 - d / width, 0, 1 );
  }
  Win window( double a, double b ) const
  {
    Win w;
    double vt = 0;
    for ( auto& x : log_ )
      if ( x.t > a && x.t <= b )
        ++w.n, vt += x.vt;
    w.rate = w.n * 60.0 / ( b - a );
    w.ve = vt * 60.0 / ( b - a );
    return w;
  }
  // pa_ holds PaCO2 every 100 ms of game time
  double mean_pa( double a, double b ) const
  {
    double s = 0;
    int n = 0;
    for ( size_t i = 0; i < pa_.size(); ++i )
    {
      double t = ( i + 1 ) * 0.1;
      if ( t > a && t <= b )
        s += pa_[ i ], ++n;
    }
    return n ? s / n : lungs_.paco2;
  }
  double max_pa( double a, double b ) const
  {
    double m = 0;
    for ( size_t i = 0; i < pa_.size(); ++i )
    {
      double t = ( i + 1 ) * 0.1;
      if ( t > a && t <= b )
        m = std::max( m, pa_[ i ] );
    }
    return m > 0 ? m : lungs_.paco2;
  }

  double t_ = 0, u_ = 0, ve_hud_ = 0, rr_hud_ = 0, spikes_ = 0;
  int last_breaths_ = 0;
  body::Lungs lungs_;
  Lowpass drive_lp_;
  std::vector< Breath > log_;
  std::vector< double > pa_;
  MotorReader motor_;
  Wiring wiring_;
  mech::Gate mech_;
  Sched sch_ = kFull;
};

} // namespace

std::unique_ptr< Task > make_breathing() { return std::make_unique< BreathingTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( breathing, 300, "breathing.json", make_breathing ) }
