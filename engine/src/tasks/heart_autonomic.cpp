// Task "heart_autonomic": build the autonomic controller of the heart. An SA-node pacemaker (intrinsic ~100/min with no autonomic input) is slowed by
// vagal acetylcholine and sped up by sympathetic noradrenaline; the circuit must make the heart rate follow a target schedule
// (rest, exercise, recovery).
//
// WORLD (body: spike::body::Heart, docs/body/heart.md; deterministic, no randomness; times in ms)
//   Target heart rate T(t), bpm:  65 until 20 s (rest, vagal tone), linear ramp to 120 over 20-30 s (exercise starts),
//   120 until 55 s, linear ramp back to 65 over 55-70 s (recovery), 65 until 85 s.
//   Workload w(t) = (T - 65) / 55, 0..1.
//   The two error channels and the cue are STAND-INS, not sensors: hr_too_low / hr_too_high hand the circuit the sign of its own
//   error (a controller given its error, in place of baroreceptor and arterial-stretch afferents), exercise_cue stands for central
//   command and muscle afferents. The brief says so.
//   Sensors (Poisson rates in Hz, set every slice; the circuit's input weight turns them into current):
//     "exercise_cue" : 100 Hz * w(t). Central command from the motor cortex and muscle afferents: feed-forward, no feedback.
//     "hr_too_low"   : 100 Hz * clamp( (T - HR_s) / 40 bpm, 0, 1 )   (the target is above the heart rate)
//     "hr_too_high"  : 100 Hz * clamp( (HR_s - T) / 40 bpm, 0, 1 )   (the heart rate is above the target)
//     HR_s is the beat-based heart rate, low-passed with tau = 1 s (a baroreceptor / stretch-receptor stand-in). Two
//     channels, because a firing rate cannot be negative: one signals each sign of the error.
//   Motors:
//     "sa_beat"      : spikes of the SA cells = beats (every spike requests a beat; the heart merges requests < 250 ms apart)
//     "vagal"        : a modulator output (kind "modulator", ACh) from the cardiac vagal ganglion cells
//     "sympathetic"  : a modulator output (kind "modulator", NE) from the sympathetic (stellate) ganglion cells
//   The task does not set the modulators itself; it reads Engine::modulator("ACh") and ("NE") (the circuit's release pools)
//   and the circuit's "modulation" table maps them onto the SA cells (param ach / ne), see docs/CIRCUIT-FORMAT.md.
//   Heart effects of NE beyond the rate (the rate comes from the SA cells): contractility drive sym_cardiac =
//   0.2 + 0.25 (NE - 0.2); vessels: sym_vascular = 0.2 - 0.1 w (active muscle vasodilates).
//
// SCORING (HR sampled every 100 ms, HR = 60000 / max(last beat interval, time since the last beat); asystole counts as a miss)
//   mae       : mean |HR - T| over [8 s, 85 s]
//   rest_hr   : mean HR over [12 s, 20 s];  plateau_hr: mean over [40 s, 55 s];  rec_hr: mean over [75 s, 85 s]
//   value = 100 * clamp( 1 - mae / 15 bpm, 0, 1 )
//   pass  : mae <= 6, |rest_hr - 65| <= 4, |plateau_hr - 120| <= 8, |rec_hr - 65| <= 4 (checked at the end of the episode)
//   verdict: names the phase that failed (resting tachycardia, exercise HR too low or too high, slow recovery).
//   Extra parts: peak_hr, rest_rmssd_ms (beat-to-beat variability at rest, a vagal-tone index), ach_rest, ne_peak, map_peak.
//
// Biology: vagal tone dominates the resting heart rate (the cell's intrinsic ~100/min is slowed to 65 by ACh 0.5-0.6); at the start of exercise the heart rate rises first by vagal
// withdrawal (within a beat or two) and then by sympathetic activation (seconds), and recovery is the reverse: fast vagal
// reactivation, slower sympathetic washout (Robinson et al. 1966 Circ Res 19:400 (autonomic blockade in exercise); Rowell &
// O'Leary 1990 J Appl Physiol 69:407; Imai et al. 1994 J Am Coll Cardiol 24:1529; Levy 1971 Circ Res 29:437 for the nonlinear
// vagal-sympathetic interaction; Task Force 1996 Circulation 93:1043 for HRV).
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include "spike/analysis.h"
#include "spike/body/heart.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr double kDuration = 85000;
constexpr double kRest = 65, kPeak = 120;
constexpr double kT1 = 20000, kT2 = 30000, kT3 = 55000, kT4 = 70000;  // ramp up start/end, ramp down start/end

double target_bpm( double t )
{
  if ( t < kT1 ) return kRest;
  if ( t < kT2 ) return kRest + ( kPeak - kRest ) * ( t - kT1 ) / ( kT2 - kT1 );
  if ( t < kT3 ) return kPeak;
  if ( t < kT4 ) return kPeak - ( kPeak - kRest ) * ( t - kT3 ) / ( kT4 - kT3 );
  return kRest;
}

class HeartAutonomicTask : public Task
{
public:
  HeartAutonomicTask() { reset( 1 ); }
  std::string id() const override { return "heart_autonomic"; }
  std::string title() const override { return "Autonomic heart rate"; }
  std::string brief() const override
  {
    return "The SA node has its own rate, about 100 per minute with no autonomic input (as after drug blockade); the autonomic "
           "nervous system moves it. Vagal (parasympathetic) neurons release acetylcholine and slow the node, sympathetic neurons "
           "release noradrenaline and speed it up. At rest it is the vagal tone that holds the heart near 65. Each branch is a "
           "two-neuron chain: a preganglionic cell in the brainstem or spinal cord drives a postganglionic ganglion cell that "
           "releases the transmitter (the 'vagal' and 'sympathetic' modulator outputs). Build both branches and the SA node, and "
           "make the heart follow the target: about 65 at rest (vagal tone), up to 120 in exercise, and back to 65 in recovery. "
           "The three inputs are stand-ins, not real sensors: exercise_cue is central command from the motor cortex and muscle "
           "afferents (feed-forward), and hr_too_low and hr_too_high hand you the sign of the heart-rate error directly, in place "
           "of the baroreceptors and arterial stretch afferents that would report it in the body. Cutting the vagus should show up "
           "as resting tachycardia (about 100), or cutting the sympathetic chain as a heart that cannot speed up. Too much ACh "
           "slows the node a lot, so keep the vagal drive moderate. Score: mean tracking error, and the average rate in each phase. ";
  }
  /// The cell type the reference circuit and the verdicts assume; the numbers of the build live in recipe() (shown by mode in the lab).
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A recipe that "
           "works (iaf_psc_alpha groups; pA; bias = ADVANCED I_e): the three sensors at a weight of about 800 onto relay groups CC, LOW and HIGH "
           "(4 cells each; the sensor weight is set on the sensor); vagal chain NA (6 cells, bias 380: it fires on its own, that is the vagal tone) "
           "> VG (6, segment wiring neuron k to k, weight 1500) > vagal output, set to release ACh with gain 0.009 and tau 500 ms; "
           "sympathetic chain IML (6, bias 370) > SG (6, segment, 1500) > sympathetic output, NE, gain 0.003, tau 1500 ms (the defaults). "
           "Links onto NA: CC -24 (exercise withdraws the vagus), HIGH +10, LOW -6; onto IML: CC +12, LOW +10, HIGH -25. The SA node "
           "needs no wiring but the sa_beat output (the engine makes it follow ACh and NE). Bias values matter: 5 pA on NA changes the "
           "resting rate by 10 beats/min.";
  }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    // ChannelKind: kind, value (typical input level), modulator, gain per spike, tau_ms
    return { { "exercise_cue", in_kind( "poisson", 0, 800 ) }, { "hr_too_low", in_kind( "poisson", 0, 800 ) }, { "hr_too_high", in_kind( "poisson", 0, 800 ) },
      { "sa_beat", { "rate" } }, { "vagal", { "modulator", 0, "ACh", 0.012, 500 } }, { "sympathetic", { "modulator", 0, "NE", 0.0031, 1500 } } };
  }
  std::vector< std::string > sensor_channels() const override { return { "exercise_cue", "hr_too_low", "hr_too_high" }; }
  std::vector< std::string > motor_channels() const override { return { "sa_beat", "vagal", "sympathetic" }; }
  double duration_ms() const override { return kDuration; }
  std::string chapter() const override { return "heart"; }
  std::string view() const override { return "heart"; }
  int par_neurons() const override { return variant_par( variant_, 45 ); }

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }
  std::vector< std::string > concepts_used() const override { return { "neuromodulation", "feedforward_feedback", "pacemaking", "excitation", "inhibition" }; }
  bool on_spine() const override { return true; }
  /// Pass bars (mean error, rest, exercise, recovery): guided 7.5 / 5 / 10 / 5 bpm, standard 6 / 4 / 8 / 4, researcher 5.1 / 3.4 / 6.8 / 3.4.
  /// The tight_tracking variant asks for 4 / 3 / 6 / 3.
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 23 instead of 45" },
             { "tight_tracking", "tighter tracking: mean error 4 bpm, rest and recovery within 3, exercise within 6" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    const double rest = s.parts[ "rest_hr" ], plat = s.parts[ "exercise_hr" ];
    std::vector< Hint > h;
    if ( rest > kRest + lim_rest() )
    {
      h.push_back( make_hint( 1, "Neuromodulation: the heart is held at its resting rate by acetylcholine released continuously by the vagus. Without it the node runs at about 100.", "" ) );
      h.push_back( make_hint( 2, "The vagal output is not releasing enough ACh at rest.", "vagal" ) );
      h.push_back( make_hint( 3, "NA (6 cells, bias 380) > VG (6, segment, 1500) > vagal, ACh, gain 0.009, tau 500 ms.", "" ) );
    }
    else if ( rest < kRest - lim_rest() )
    {
      h.push_back( make_hint( 1, "Too much ACh slows the node too far. Keep the vagal tone moderate.", "" ) );
      h.push_back( make_hint( 2, "Lower the vagal release.", "vagal" ) );
      h.push_back( make_hint( 3, "Vagal gain about 0.009; bias on the NA group a few pA lower.", "" ) );
    }
    else if ( plat < kPeak - lim_plat() )
    {
      h.push_back( make_hint( 1, "Feed-forward: exercise_cue announces the effort before the heart rate has changed; the sympathetic branch should answer it and the vagus should withdraw.", "" ) );
      h.push_back( make_hint( 2, "The sympathetic output is too weak.", "sympathetic" ) );
      h.push_back( make_hint( 3, "exercise_cue > CC > IML at +12; IML (6, bias 370) > SG > sympathetic, NE gain 0.003, tau 1500 ms; CC > NA at -24.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "Feedback corrects what feed-forward misses: the sign of the error (hr_too_low / hr_too_high) trims both branches.", "" ) );
      h.push_back( make_hint( 2, "The error inputs are not trimming the rate: check their links onto both autonomic branches.", "hr_too_low" ) );
      h.push_back( make_hint( 3, "Onto NA: HIGH +10, LOW -6; onto IML: LOW +10, HIGH -25.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override { return { { "mae_bpm", make_metric( score().parts[ "mae_bpm" ], "bpm", true ) } }; }
  Note notebook() const override
  {
    return { "Two chains, one node",
             "The sinoatrial node is slowed by acetylcholine from vagal postganglionic neurons and sped up by noradrenaline from "
             "sympathetic ones. At rest the vagal tone dominates: the intrinsic rate is about 100 beats per minute, but the resting "
             "heart beats near 65.",
             "Jose AD, Collison D (1970) Cardiovasc Res 4:160; Levy MN (1971) Circ Res 29:437" };
  }
  std::vector< Event > timeline() const override
  {
    return { { 0, kT1, "rest" }, { kT1, kT2 - kT1, "exercise starts" }, { kT2, kT3 - kT2, "exercise" },
      { kT3, kT4 - kT3, "recovery" }, { kT4, kDuration - kT4, "rest" } };
  }
  std::vector< Band > bands() const override { return { { "hr_error_bpm", "bpm", -lim_mae(), lim_mae() } }; }
  std::vector< std::string > modulators() const override { return { "NE", "ACh" }; }

  void reset( uint64_t ) override
  {
    heart_.reset();
    t_ = 0;
    beats_.clear();
    last_beat_ = -1;
    hr_inst_ = 0;
    hr_s_.tau_ms = 1000;
    hr_s_.reset( 80 );
    ne_ = ach_ = 0;
    spikes_ = 0;
    n_ = mae_sum_ = 0;
    rest_.reset();
    plat_.reset();
    rec_.reset();
    peak_hr_ = ach_rest_ = ne_peak_ = map_peak_ = 0;
    next_sample_ = 0;
    max_err_ = 0;
    ach_rest_n_ = 0;
    motor_.invalidate();
    wiring_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    double T = target_bpm( t_ );
    double w = ( T - kRest ) / ( kPeak - kRest );
    e.set_input( "exercise_cue", 100.0 * w );
    double err = T - hr_s_.y;
    e.set_input( "hr_too_low", 100.0 * clampd( err / 40.0, 0, 1 ) );
    e.set_input( "hr_too_high", 100.0 * clampd( -err / 40.0, 0, 1 ) );
    e.step( dt );
    ne_ = e.modulator( "NE" );
    ach_ = e.modulator( "ACh" );
    int n = static_cast< int >( std::lround( motor_.spike_count( e, "sa_beat", dt ) ) );
    spikes_ += n;
    int ticks = std::max( 1, static_cast< int >( std::ceil( dt - 1e-9 ) ) );
    double h = dt / ticks;
    for ( int i = 0; i < ticks; ++i )
    {
      body::HeartInput in;
      in.sa_spikes = i == 0 ? n : 0;
      in.sa_rate_hz = 0;
      in.sym_cardiac = 0.2 + 0.25 * ( ne_ - 0.2 );
      in.sym_vascular = 0.2 - 0.1 * w;
      in.vagal = 0.6;
      heart_.step( h, in );
      t_ += h;
      if ( heart_.beat_now )
      {
        beats_.push_back( t_ );
        last_beat_ = t_;
      }
      double since = last_beat_ < 0 ? 1e9 : t_ - last_beat_;
      double rr = beats_.size() >= 2 ? beats_.back() - beats_[ beats_.size() - 2 ] : 1e9;
      double iv = std::max( rr, since );
      hr_inst_ = iv >= 1e8 ? 0.0 : 60000.0 / iv;
      hr_s_.step( hr_inst_, h );
    }
    if ( t_ >= next_sample_ )
    {
      next_sample_ += 100;
      sample();
    }
  }

  BodyState body() const override
  {
    BodyState b;
    b.points = { { 0, 0 } };
    b.readouts = heart_.readouts();
    auto v = heart_.view();
    double T = target_bpm( t_ );
    b.readouts[ "hr_bpm" ] = hr_inst_;
    b.readouts[ "hr_target" ] = T;
    b.readouts[ "hr_error_bpm" ] = hr_inst_ - T;
    b.readouts[ "workload" ] = ( T - kRest ) / ( kPeak - kRest );
    b.readouts[ "NE" ] = ne_;
    b.readouts[ "ACh" ] = ach_;
    b.readouts[ "heart_phase" ] = v.phase;
    b.readouts[ "ejection" ] = v.ejection;
    b.readouts[ "beat" ] = v.beat ? 1.0 : 0.0;
    b.readouts[ "beats" ] = heart_.beats;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    double mae = n_ > 0 ? mae_sum_ / n_ : 100.0;
    double rest = rest_.mean(), plat = plat_.mean(), rec = rec_.mean();
    s.parts[ "mae_bpm" ] = mae;
    s.parts[ "rest_hr" ] = rest;
    s.parts[ "exercise_hr" ] = plat;
    s.parts[ "recovery_hr" ] = rec;
    s.parts[ "peak_hr" ] = peak_hr_;
    s.parts[ "max_error_bpm" ] = max_err_;
    s.parts[ "rest_rmssd_ms" ] = rest_rmssd();
    s.parts[ "ach_rest" ] = ach_rest_n_ > 0 ? ach_rest_ / ach_rest_n_ : 0.0;
    s.parts[ "ne_peak" ] = ne_peak_;
    s.parts[ "map_peak_mmHg" ] = map_peak_;
    s.parts[ "spikes" ] = spikes_;
    s.value = 100.0 * clampd( 1.0 - mae / 15.0, 0, 1 );
    bool done = t_ >= kDuration;
    bool rest_ok = rest_.n > 0 && std::fabs( rest - kRest ) <= lim_rest(), plat_ok = plat_.n > 0 && std::fabs( plat - kPeak ) <= lim_plat(),
         rec_ok = rec_.n > 0 && std::fabs( rec - kRest ) <= lim_rest();
    s.passed = done && mae <= lim_mae() && rest_ok && plat_ok && rec_ok;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 0 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( spikes_ < 1 )
      o << "No beats: sa_beat has not fired" << yet << " Check the SA-node cells and the sa_beat output.";
    else if ( !done && t_ < 12000 )
      o << "Warming up (" << heart_.hr_bpm << " beats/min now, target " << target_bpm( t_ ) << ").";
    else if ( !done )
      o << "Tracking so far: error " << mae << " bpm on average, " << hr_inst_ << " beats/min now (target " << target_bpm( t_ ) << ").";
    else if ( !rest_ok )
      o << ( rest > kRest ? "Resting tachycardia: " : "Resting bradycardia: " ) << rest << " beats/min at rest, target " << kRest
        << ( rest > kRest ? ". The resting heart is held down by vagal tone; is the vagal branch working?" : ". Too much vagal drive at rest." );
    else if ( !plat_ok )
      o << ( plat < kPeak ? "The heart cannot speed up: " : "Heart rate overshoots: " ) << plat << " beats/min in exercise, target " << kPeak
        << ( plat < kPeak ? ". Sympathetic drive is too weak or cut." : "." );
    else if ( !rec_ok )
      o << ( rec > kRest ? "Slow recovery: " : "Recovery overshoots into bradycardia: " ) << rec << " beats/min at the end, target " << kRest
        << ( rec > kRest ? ". Vagal tone must return and sympathetic drive must fall." : ". Too much vagal drive after exercise (ACh near the arrest range?)." );
    else if ( mae > lim_mae() )
      o << "Phases are right on average but tracking is loose: " << mae << " bpm mean error (needs " << lim_mae() << " or less).";
    else
      o << "Tracked the schedule: " << mae << " bpm mean error, rest " << rest << ", exercise " << plat << ", recovery " << rec << ".";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { -1, -1, 1, 1 }; }

private:
  bool tight() const { return variant_ == "tight_tracking"; }
  double lim_mae() const { return tight() ? 4.0 : tol_max( mode_, 6.0 ); }
  double lim_rest() const { return tight() ? 3.0 : tol_max( mode_, 4.0 ); }
  double lim_plat() const { return tight() ? 6.0 : tol_max( mode_, 8.0 ); }
  struct Avg
  {
    double sum = 0, n = 0;
    void reset() { sum = n = 0; }
    void add( double x ) { sum += x; n += 1; }
    double mean() const { return n > 0 ? sum / n : 0.0; }
  };

  void sample()
  {
    double T = target_bpm( t_ );
    peak_hr_ = std::max( peak_hr_, hr_inst_ );
    ne_peak_ = std::max( ne_peak_, ne_ );
    map_peak_ = std::max( map_peak_, static_cast< double >( heart_.map_smooth ) );
    if ( t_ < 8000 )
      return;
    mae_sum_ += std::fabs( hr_inst_ - T );
    n_ += 1;
    max_err_ = std::max( max_err_, std::fabs( hr_inst_ - T ) );
    if ( t_ >= 12000 && t_ < kT1 )
    {
      rest_.add( hr_inst_ );
      ach_rest_ += ach_;
      ach_rest_n_ += 1;
    }
    if ( t_ >= 40000 && t_ < kT3 )
      plat_.add( hr_inst_ );
    if ( t_ >= 75000 )
      rec_.add( hr_inst_ );
  }

  double rest_rmssd() const
  {
    std::vector< double > bt;
    for ( double t : beats_ )
      if ( t >= 8000 && t < kT1 )
        bt.push_back( t );
    if ( bt.size() < 4 )
      return 0;
    auto h = analysis::hrv_analyze( bt );
    return std::isnan( h.rmssd_ms ) ? 0.0 : h.rmssd_ms;
  }

  body::Heart heart_;
  double t_ = 0, last_beat_ = -1, hr_inst_ = 0, ne_ = 0, ach_ = 0, spikes_ = 0, n_ = 0, mae_sum_ = 0, next_sample_ = 0;
  double peak_hr_ = 0, ach_rest_ = 0, ach_rest_n_ = 0, ne_peak_ = 0, map_peak_ = 0, max_err_ = 0;
  Avg rest_, plat_, rec_;
  Lowpass hr_s_;
  std::vector< double > beats_;
  MotorReader motor_;
  Wiring wiring_;
};

} // namespace

std::unique_ptr< Task > make_heart_autonomic() { return std::make_unique< HeartAutonomicTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( heart_autonomic, 210, "heart_autonomic.json", make_heart_autonomic ) }
