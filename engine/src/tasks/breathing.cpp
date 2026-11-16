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
// (Guyenet, Stornetta & Bayliss 2010 Respir Physiol Neurobiol 173:244) and the carotid body sets the drive. Eupnoea 12-20 /min (West,
// Respiratory Physiology). The chemoreflex gain here is a game value (a few L/min per mmHg PaCO2 for real subjects).
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include "spike/body/lungs.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr double kDuration = 150000;
constexpr double kDriveHz = 200, kChemoPA = 4, kPhrenicRef = 225;
constexpr double kTimeScale = 5;
constexpr double kCo2On = 50000, kCo2Off = 110000, kRamp = 5000, kInspired = 30;
constexpr double kRestA = 12, kRestB = 48, kChA = 85, kChB = 110, kPeakA = 50, kRecA = 135, kRecB = 150;  // s

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
           "A recipe that works, in steps (units: weights of burster synapses are nS, of iaf cells pA; 'drive' is a Poisson train, "
           "'chemo' is a current in pA): (1) about 10 butera_burster cells (PBC), g_NaP 2.8 nS and g_tonic 0.29 nS, each excited by every "
           "other with a WEAK weight of about 0.03 nS (not 1: strong coupling gives 100+ breaths a minute); 'drive' onto PBC at weight "
           "0.005. Alone a burster fires a burst every 4-5 s. (2) 8 iaf_psc_alpha phrenic cells (PHR) with I_e about 380 pA, excited by "
           "PBC at about 2 pA, read out as 'phrenic'. (3) A few tonic iaf 'expiratory' cells (I_e about 430 pA) that PBC inhibits "
           "(weight about -60) and that inhibit PHR (about -30): they keep the phrenic pool quiet between breaths. (4) Route 'chemo' "
           "through relay cells (iaf, I_e about 335 pA, weight 1) to PBC (about 0.09 nS: breathing gets faster, the 'frequency' path) and "
           "to PHR (about 8 pA: breaths get deeper). If the rate is wrong, change the burster drive, not the coupling: g_tonic 0.30 gives a "
           "burst every 4.9 s, 0.35 every 2.6 s.";
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
    return { { "drive", d }, { "chemo", c }, { "phrenic", ChannelKind{ "rate" } } };
  }
  double duration_ms() const override { return kDuration; }
  std::string chapter() const override { return "breathing"; }
  std::string view() const override { return "lungs"; }
  int par_neurons() const override { return 26; }
  std::vector< Event > timeline() const override { return { { kCo2On, kCo2Off - kCo2On, "5% CO2 inhaled" } }; }
  std::vector< Band > bands() const override { return { { "rr_per_min", "/min", 12, 20 }, { "co2_mmhg", "mmHg", 35, 45 } }; }

  void reset( uint64_t ) override
  {
    t_ = 0;
    lungs_ = body::Lungs( params() );
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
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    e.set_input( "drive", kDriveHz );
    e.set_input( "chemo", kChemoPA * ( lungs_.peripheral_hz + lungs_.central_hz ) );
    e.step( dt );
    double hz = motor_.hz_per_neuron( e, "phrenic" );
    spikes_ += motor_.spike_count( e, "phrenic", dt );
    u_ = clampd( drive_lp_.step( hz / kPhrenicRef, dt ), 0, 1 );
    body::LungInput in;
    in.drive = u_;
    in.inspired_pco2 = kInspired * clampd( ( t_ - kCo2On ) / kRamp, 0, 1 ) * clampd( ( kCo2Off + kRamp - t_ ) / kRamp, 0, 1 );
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
    Win rest = window( kRestA, kRestB ), ch = window( kChA, kChB );
    double pa_rest = mean_pa( kRestA, kRestB ), pa_peak = max_pa( kPeakA, kChB ), pa_rec = mean_pa( kRecA, kRecB );
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
    double rate_ok = rest.n >= 5 ? soft_band( rest.rate, 12, 20, 6 ) : 0.0;
    double resp = clampd( ( ratio - 1.0 ) / 0.6, 0, 1 );
    double ctrl = ( ( pa_rest >= 35 && pa_rest <= 45 ) ? 0.4 : 0.0 ) + 0.6 * clampd( ( 70.0 - pa_peak ) / 18.0, 0, 1 );
    double rec = soft_band( pa_rec, 33, 46, 8 );
    s.value = 100.0 * ( 0.25 * rate_ok + 0.30 * resp + 0.30 * ctrl + 0.15 * rec );
    bool done = t_ >= kDuration;
    bool pass = done && rest.n >= 5 && rest.rate >= 12 && rest.rate <= 20 && ratio >= 1.6 && pa_rest >= 35 && pa_rest <= 45 && pa_peak <= 52 &&
                pa_rec <= 46;
    s.passed = pass;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( spikes_ < 1 )
      o << "The phrenic pool never fired" << yet << " Do the bursters burst (g_tonic about 0.3 nS)? Iaf phrenic cells need a standing current (I_e about 380 pA) and a PBC -> PHR weight of about 2.";
    else if ( rest.n < 5 )
      o << "Only " << static_cast< int >( rest.n ) << " breaths in the 36 s of rest" << yet << " Need a regular rhythm of 12-20 per minute: raise the burster drive "
        << "(g_tonic 0.30 nS = a burst every 4.9 s, 0.35 = 2.6 s) and make sure the bursters are coupled so they burst together; if the phrenic pool is "
        << "held silent, check the expiratory inhibition.";
    else if ( rest.rate < 12 || rest.rate > 20 )
      o << "Resting rate " << rest.rate << " /min is outside 12-20: "
        << ( rest.rate > 20 ? "too fast. Weaken the burster coupling (about 0.03 nS) or lower g_tonic / the chemo weight to the bursters (a burster bursts every 4.9 s at g_tonic 0.30)."
                            : "too slow. Raise the drive to the bursters (g_tonic about 0.3-0.35 nS, or more chemo weight) or strengthen the coupling a little." );
    else if ( pa_rest < 35 || pa_rest > 45 )
      o << "Resting PaCO2 " << pa_rest << " mmHg is outside 35-45: the breaths are too " << ( pa_rest > 45 ? "shallow" : "deep" ) << ".";
    else if ( !done && t_ < kChB * 1000.0 )
      o << "Breathing at " << rest.rate << " /min at rest, PaCO2 " << pa_rest << " mmHg; the CO2 test is still running.";
    else if ( ratio < 1.6 )
      o << "Ventilation rose only " << ratio << "x under 5% CO2 (need 1.6x): no chemoreflex. PaCO2 peaked at " << pa_peak << " mmHg.";
    else if ( pa_peak > 52 )
      o << "Ventilation rose " << ratio << "x but PaCO2 still reached " << pa_peak << " mmHg (limit 52).";
    else if ( pa_rec > 46 && done )
      o << "PaCO2 had not recovered afterwards (" << pa_rec << " mmHg).";
    else
      o << ( done ? "Breathing" : "Breathing so far" ) << " " << rest.rate << " /min at rest, ventilation " << ratio << "x with CO2, PaCO2 peak " << pa_peak
        << " mmHg.";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { 0, 0, 1, 1 }; }

private:
  struct Breath { double t, vt; };
  struct Win { double rate = 0, ve = 0, n = 0; };

  static body::LungParams params()
  {
    body::LungParams p;
    p.time_scale = kTimeScale;
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
};

} // namespace

std::unique_ptr< Task > make_breathing() { return std::make_unique< BreathingTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( breathing, 300, "breathing.json", make_breathing ) }
