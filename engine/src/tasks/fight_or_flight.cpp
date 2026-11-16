// Task "fight_or_flight": one threat cue, four organs. Build the autonomic outflow that raises heart rate, blood pressure, pupil size
// and ventilation while the threat lasts, and hands everything back to the parasympathetic side afterwards.
//
// WORLD (times in ms, 1 ms slices; bodies: spike::body::Heart, Eye, Lungs; docs/body/*.md)
//   Threat cue: Sensor "threat" = Poisson rate 250 Hz from 20 s to 50 s (0 otherwise), ramped over 0.5 s. (A predator, an alarm: amygdala and
//   hypothalamic defence area input.)
//   Motors (the circuit declares them; mean rates are per neuron):
//     "sa_beat"      spikes of the SA-node cell (sa_node_pacemaker) = beats; every spike is one heartbeat (heart merges requests <250 ms apart)
//     "sympathetic"  modulator output (NE) of the stellate-ganglion cells. The circuit's "modulation" table sets the SA cell's ne from it
//                    (Engine::modulator("NE")); contractility sym_cardiac = 0.2 + 0.2 NE/(NE + 0.3)
//     "vagal"        modulator output (ACh) of the cardiac vagal ganglion cells: SA cell ach (slows the heart)
//     "sym_vascular" sympathetic vasomotor pool, mean Hz / kVasoRef (100 Hz) = vessel drive 0..1 (resting 0.2; resistance R = R0 (1 + 0.8 (x - 0.2)))
//     "dilator"      superior-cervical-ganglion pool, Hz / kDilRef (40 Hz); "sphincter" ciliary-ganglion pool, Hz / kSphRef (40 Hz). Fixed
//                    luminance 30 cd/m2; pupil D = 2 + 6 clamp(0.5 + 0.5 (dilator - sphincter)) mm (Eye model)
//     "vent"         ventral respiratory group pool, d = Hz / kVentRef (40 Hz), smoothed 1 s. The task runs a simple respiratory pattern generator
//                    (the CPG itself is the 'breathing' task): breath rate 10 + 12 d per minute, inspiratory drive amplitude 0.18 + 0.2 d
//                    for the first 35 % of each cycle (half-sine) into the Lungs model.
//   All motor rates are smoothed (20 ms for the muscles, 1 s for ventilation) BEFORE any clamp.
//
// SCORING  (each trace is sampled every 100 ms: HR = last beat interval, MAP = 2 s-smoothed pressure, pupil, VE = last tidal volume x last breath rate)
//   rest = mean over 12-20 s, threat = mean over 40-50 s, recovery = mean over 90-100 s.
//   per system: (1) rest inside its band (HR 55-80 bpm, MAP 80-105 mmHg, pupil 3-6 mm, VE 4-9 L/min),
//               (2) threat response: HR +40 bpm, MAP +12 mmHg, pupil +1.2 mm, VE x1.4 over rest,
//               (3) recovery: back within HR 8 bpm, MAP 8 mmHg, pupil 0.6 mm, VE x1.25 of rest.
//   value = 100 * mean over the four systems of (0.2 rest + 0.6 response + 0.2 recovery), each a soft 0..1 score.  pass: all 12 conditions hold at the end.
//
// Biology: the defence reaction (Cannon 1915 fight-or-flight; Hilton 1982 defence area; Dampney 1994 Physiol Rev 74:323 for the central pathways):
// hypothalamus -> RVLM -> sympathetic preganglionic neurons (heart, vessels, iris dilator), simultaneous withdrawal of cardiac vagal and
// Edinger-Westphal drive and increased ventilation. Heart rate rises first by vagal withdrawal, then by NE (Robinson et al. 1966 J Appl Physiol
// 21:1655), recovery is vagal reactivation and sympathetic washout. All gains are game values.
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include "spike/body/eye.h"
#include "spike/body/heart.h"
#include "spike/body/lungs.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr double kDuration = 100000, kThreatOn = 20000, kThreatOff = 50000, kRamp = 500, kThreatHz = 250;
constexpr double kVasoRef = 100, kDilRef = 40, kSphRef = 40, kVentRef = 40, kLum = 30;
constexpr double kRestA = 12, kRestB = 20, kThrA = 40, kThrB = 50, kRecA = 90, kRecB = 100;  // s

enum Sys { HR = 0, MAP = 1, PUPIL = 2, VENT = 3 };
const char* kName[ 4 ] = { "hr", "map", "pupil", "ve" };
const char* kLabel[ 4 ] = { "heart rate", "blood pressure", "pupil", "ventilation" };
constexpr double kBandLo[ 4 ] = { 55, 80, 3.0, 4.0 }, kBandHi[ 4 ] = { 80, 105, 6.0, 9.0 };
constexpr double kRise[ 4 ] = { 40, 12, 1.2, 1.4 };  // HR, MAP, pupil: additive; VE: ratio
constexpr double kTol[ 4 ] = { 8, 8, 0.6, 1.25 };    // additive; VE: ratio

class FightOrFlightTask : public Task
{
public:
  FightOrFlightTask() { reset( 1 ); }
  std::string id() const override { return "fight_or_flight"; }
  std::string title() const override { return "Fight or flight"; }
  std::string brief() const override
  {
    return "A threat turns the body from rest-and-digest to fight-or-flight. The hypothalamus drives the sympathetic premotor cells of the "
           "brainstem (RVLM); they excite the sympathetic ganglia that release noradrenaline on the heart's pacemaker (faster beats, "
           "stronger beats), tighten the vessels (pressure up), open the pupil and, with the respiratory group, raise ventilation. At the "
           "same time the threat switches off the cardiac vagus and the Edinger-Westphal nucleus, the parasympathetic side. When the threat "
           "ends the parasympathetic side must take over again and everything must settle back to rest. Wire 'threat' into the circuit and "
           "read out the pacemaker ('sa_beat'), the two heart modulators ('sympathetic' = NE, 'vagal' = ACh: set these two outputs to kind modulator, and let the SA cell respond to NE and ACh), 'sym_vascular', 'dilator', "
           "'sphincter' and 'vent'. Score: for each of heart rate, pressure, pupil and ventilation: resting value in its band, a clear rise "
           "during the 30 s threat, and a return to rest within 50 s after it.";
  }
  std::vector< std::string > sensor_channels() const override { return { "threat" }; }
  std::vector< std::string > motor_channels() const override
  {
    return { "sa_beat", "sympathetic", "vagal", "sym_vascular", "dilator", "sphincter", "vent" };
  }
  double duration_ms() const override { return kDuration; }
  std::string chapter() const override { return "autonomic"; }
  std::string view() const override { return "dashboard"; }
  int par_neurons() const override { return 45; }
  std::vector< Event > timeline() const override
  {
    return { { kThreatOn, kThreatOff - kThreatOn, "threat cue" }, { kThreatOff, kDuration - kThreatOff, "safe again" } };
  }
  std::vector< Band > bands() const override
  {
    return { { "hr_bpm", "bpm", kBandLo[ HR ], kBandHi[ HR ] },
             { "map_mmhg", "mmHg", kBandLo[ MAP ], kBandHi[ MAP ] },
             { "pupil_mm", "mm", kBandLo[ PUPIL ], kBandHi[ PUPIL ] },
             { "ventilation", "L/min", kBandLo[ VENT ], kBandHi[ VENT ] } };
  }
  std::vector< std::string > modulators() const override { return { "NE", "ACh" }; }
  // 'sympathetic' and 'vagal' must be MODULATOR outputs (NE / ACh release pools); everything else is a rate pool. Gains/taus are those of the
  // reference (per spike of a pool of about 10 cells); the lab starts from them and the player tunes the release.
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    std::map< std::string, ChannelKind > k;
    ChannelKind thr;
    thr.kind = "poisson";
    thr.value = kThreatHz;
    k[ "threat" ] = thr;
    ChannelKind ne;
    ne.kind = "modulator";
    ne.modulator = "NE";
    ne.gain = 0.0015;
    ne.tau_ms = 1500;
    k[ "sympathetic" ] = ne;
    ChannelKind ach;
    ach.kind = "modulator";
    ach.modulator = "ACh";
    ach.gain = 0.007;
    ach.tau_ms = 500;
    k[ "vagal" ] = ach;
    for ( const char* r : { "sa_beat", "sym_vascular", "dilator", "sphincter", "vent" } )
      k[ r ].kind = "rate";
    return k;
  }

  void reset( uint64_t ) override
  {
    t_ = 0;
    heart_ = body::Heart();
    eye_.reset( 0.0, 0.5 );
    body::LungParams lp;
    lp.time_scale = 3;
    lungs_ = body::Lungs( lp );
    lungs_.reset();
    for ( auto* l : { &l_sv_, &l_dil_, &l_sph_ } ) { l->tau_ms = 20; l->reset( 0 ); }
    l_sa_.tau_ms = 200;
    l_vg_.tau_ms = 200;
    l_sa_.reset( 0 );
    l_vg_.reset( 0 );
    sg_hz_ = vg_hz_ = 0;
    l_vent_.tau_ms = 1000;
    l_vent_.reset( 0.4 );
    sv_hz_ = dil_hz_ = sph_hz_ = vent_hz_ = 0;
    phase_ = 0;
    last_breaths_ = 0;
    breath_log_.clear();
    for ( auto& v : tr_ ) v.clear();
    ne_ = ach_ = ne_max_ = ach_max_ = 0;
    ve_hud_ = rr_hud_ = 0;
    spikes_.fill( 0 );
    motor_.invalidate();
    wiring_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    double thr = kThreatHz * clampd( ( t_ - kThreatOn ) / kRamp, 0, 1 ) * ( t_ < kThreatOff ? 1.0 : 0.0 );
    e.set_input( "threat", thr );
    e.step( dt );
    static const char* kCh[ 7 ] = { "sa_beat", "sympathetic", "vagal", "sym_vascular", "dilator", "sphincter", "vent" };
    for ( int i = 0; i < 7; ++i )
      spikes_[ i ] += motor_.spike_count( e, kCh[ i ], dt );
    double beats = motor_.spike_count( e, "sa_beat", dt );
    sv_hz_ = l_sv_.step( motor_.hz_per_neuron( e, "sym_vascular" ), dt );
    sg_hz_ = l_sa_.step( motor_.hz_per_neuron( e, "sympathetic" ), dt );
    vg_hz_ = l_vg_.step( motor_.hz_per_neuron( e, "vagal" ), dt );
    dil_hz_ = l_dil_.step( motor_.hz_per_neuron( e, "dilator" ), dt );
    sph_hz_ = l_sph_.step( motor_.hz_per_neuron( e, "sphincter" ), dt );
    vent_hz_ = l_vent_.step( motor_.hz_per_neuron( e, "vent" ), dt );
    ne_ = e.modulator( "NE" );
    ach_ = e.modulator( "ACh" );
    ne_max_ = std::max( ne_max_, ne_ );
    ach_max_ = std::max( ach_max_, ach_ );

    body::HeartInput hi;
    hi.sa_spikes = beats > 0.5 ? 1 : 0;
    hi.sa_rate_hz = 0;  // beats only from the SA cell's spikes
    hi.sym_cardiac = clampd( 0.2 + 0.2 * ne_ / ( ne_ + 0.3 ), 0, 1 );
    hi.sym_vascular = clampd( sv_hz_ / kVasoRef, 0, 1 );
    hi.vagal = clampd( ach_, 0, 1 );
    heart_.step( dt, hi );

    body::EyeInput ei;
    ei.sphincter = clampd( sph_hz_ / kSphRef, 0, 1 );
    ei.dilator = clampd( dil_hz_ / kDilRef, 0, 1 );
    ei.luminance = kLum;
    eye_.step( dt, ei );

    // respiratory pattern generator (rate and depth from the ventral respiratory group's drive d)
    double d = clampd( vent_hz_ / kVentRef, 0, 1.5 );
    double f_per_min = 10.0 + 12.0 * d, amp = 0.18 + 0.2 * d;
    phase_ += f_per_min / 60000.0 * dt;
    if ( phase_ >= 1.0 ) phase_ -= 1.0;
    body::LungInput li;
    li.drive = phase_ < 0.35 ? amp * std::sin( kPi * phase_ / 0.35 ) : 0.0;
    lungs_.step( dt, li );
    t_ += dt;
    if ( lungs_.breaths != last_breaths_ )
    {
      last_breaths_ = lungs_.breaths;
      breath_log_.push_back( { t_ / 1000.0, lungs_.tidal_l } );
    }
    // ventilation = last tidal volume x last breath-to-breath rate (updates every breath)
    ve_hud_ = lungs_.tidal_l * lungs_.rate_bpm;
    rr_hud_ = lungs_.rate_bpm;
    while ( tr_[ HR ].size() < static_cast< size_t >( t_ / 100.0 + 1e-9 ) )
    {
      tr_[ HR ].push_back( heart_.hr_bpm );
      tr_[ MAP ].push_back( heart_.map_smooth );
      tr_[ PUPIL ].push_back( eye_.diameter_mm );
      tr_[ VENT ].push_back( ve_hud_ );
    }
  }

  BodyState body() const override
  {
    BodyState b;
    auto hv = heart_.view();
    b.readouts[ "hr_bpm" ] = heart_.hr_bpm;
    b.readouts[ "beat" ] = hv.phase;
    b.readouts[ "map_mmhg" ] = heart_.map_smooth;
    b.readouts[ "sys_mmhg" ] = heart_.systolic;
    b.readouts[ "dia_mmhg" ] = heart_.diastolic;
    b.readouts[ "stroke_ml" ] = heart_.stroke_ml;
    b.readouts[ "pupil_mm" ] = eye_.diameter_mm;
    b.readouts[ "ventilation" ] = ve_hud_;
    b.readouts[ "rr_per_min" ] = rr_hud_;
    b.readouts[ "volume" ] = lungs_.view().volume_frac;
    b.readouts[ "co2_mmhg" ] = lungs_.paco2;
    b.readouts[ "NE" ] = ne_;
    b.readouts[ "ACh" ] = ach_;
    b.readouts[ "DA" ] = 0;
    b.readouts[ "sympathetic_hz" ] = sg_hz_;
    b.readouts[ "parasympathetic_hz" ] = vg_hz_;
    b.readouts[ "threat" ] = ( t_ >= kThreatOn && t_ < kThreatOff ) ? 1.0 : 0.0;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    double rest[ 4 ], thr[ 4 ], rec[ 4 ];
    double total = 0;
    int ok_all = 0;
    for ( int k = 0; k < 4; ++k )
    {
      rest[ k ] = mean( tr_[ k ], kRestA, kRestB, k );
      thr[ k ] = mean( tr_[ k ], kThrA, kThrB, k );
      rec[ k ] = mean( tr_[ k ], kRecA, kRecB, k );
      bool ratio = ( k == VENT );
      double rise = ratio ? ( rest[ k ] > 0.1 ? thr[ k ] / rest[ k ] : 0.0 ) : thr[ k ] - rest[ k ];
      double back = ratio ? ( rest[ k ] > 0.1 && rec[ k ] > 0.1 ? std::max( rec[ k ] / rest[ k ], rest[ k ] / rec[ k ] ) : 99.0 ) : std::fabs( rec[ k ] - rest[ k ] );
      bool c1 = rest[ k ] >= kBandLo[ k ] && rest[ k ] <= kBandHi[ k ];
      bool c2 = ratio ? rise >= kRise[ k ] : rise >= kRise[ k ];
      bool c3 = back <= kTol[ k ];
      ok_[ k ] = ( c1 ? 1 : 0 ) + ( c2 ? 2 : 0 ) + ( c3 ? 4 : 0 );
      ok_all += c1 + c2 + c3;
      double s1 = soft_band( rest[ k ], kBandLo[ k ], kBandHi[ k ], 0.5 * ( kBandHi[ k ] - kBandLo[ k ] ) );
      double s2 = ratio ? clampd( ( rise - 1.0 ) / ( kRise[ k ] - 1.0 ), 0, 1 ) : clampd( rise / kRise[ k ], 0, 1 );
      double s3 = ratio ? clampd( 1.0 - ( back - 1.0 ) / ( 2.0 * ( kTol[ k ] - 1.0 ) ), 0, 1 ) : clampd( 1.0 - ( back - 0.0 ) / ( 2.0 * kTol[ k ] ), 0, 1 );
      total += 0.2 * s1 + 0.6 * s2 + 0.2 * s3;
      std::string n = kName[ k ];
      s.parts[ n + "_rest" ] = rest[ k ];
      s.parts[ n + "_threat" ] = thr[ k ];
      s.parts[ n + "_recovery" ] = rec[ k ];
    }
    s.value = 100.0 * total / 4.0;
    bool done = t_ >= kDuration;
    s.passed = done && ok_all == 12;
    s.parts[ "conditions_met" ] = ok_all;
    s.parts[ "sa_spikes" ] = spikes_[ 0 ];
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( spikes_[ 0 ] < 1 )
      o << "The pacemaker never fired: no heartbeat. Check 'sa_beat'.";
    else if ( spikes_[ 1 ] >= 1 && ne_max_ < 1e-6 )
      o << "'sympathetic' fires but releases no noradrenaline: make it a modulator output (NE), not a rate output. The heart reads NE and ACh, not rates.";
    else if ( spikes_[ 2 ] >= 1 && ach_max_ < 1e-6 )
      o << "'vagal' fires but releases no acetylcholine: make it a modulator output (ACh), not a rate output.";
    else if ( t_ < kThrB * 1000.0 )
      o << "At rest so far: HR " << heart_.hr_bpm << " bpm, MAP " << heart_.map_smooth << " mmHg, pupil " << eye_.diameter_mm << " mm.";
    else
    {
      std::string bad;
      for ( int k = 0; k < 4 && bad.empty(); ++k )
        if ( ( ok_[ k ] & 1 ) == 0 )
          bad = std::string( "Resting " ) + kLabel[ k ] + " is " + num( rest[ k ] ) + ", outside its band " + num( kBandLo[ k ] ) + "-" + num( kBandHi[ k ] ) + ".";
      for ( int k = 0; k < 4 && bad.empty(); ++k )
        if ( ( ok_[ k ] & 2 ) == 0 )
          bad = std::string( "The threat did not raise the " ) + kLabel[ k ] + " enough (" + num( rest[ k ] ) + " -> " + num( thr[ k ] ) + "; needs " +
                ( k == VENT ? "x" + num( kRise[ k ] ) : "+" + num( kRise[ k ] ) ) + ").";
      for ( int k = 0; k < 4 && bad.empty() && done; ++k )
        if ( ( ok_[ k ] & 4 ) == 0 )
          bad = std::string( "The " ) + kLabel[ k ] + " did not recover after the threat (rest " + num( rest[ k ] ) + ", end " + num( rec[ k ] ) + ").";
      if ( !bad.empty() )
        o << bad;
      else if ( !done )
        o << "Threat response is right (HR " << rest[ HR ] << " -> " << thr[ HR ] << " bpm, MAP " << rest[ MAP ] << " -> " << thr[ MAP ] << " mmHg); recovery still to be judged.";
      else
        o << "HR " << rest[ HR ] << " -> " << thr[ HR ] << " -> " << rec[ HR ] << " bpm, MAP " << rest[ MAP ] << " -> " << thr[ MAP ] << " -> " << rec[ MAP ]
          << " mmHg, pupil " << rest[ PUPIL ] << " -> " << thr[ PUPIL ] << " -> " << rec[ PUPIL ] << " mm, ventilation x" << thr[ VENT ] / std::max( 0.1, rest[ VENT ] ) << ".";
    }
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { 0, 0, 1, 1 }; }

private:
  struct Breath { double t, vt; };
  static std::string num( double v )
  {
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 ) << v;
    return o.str();
  }
  static double soft_band( double v, double lo, double hi, double w )
  {
    double d = v < lo ? lo - v : ( v > hi ? v - hi : 0.0 );
    return clampd( 1.0 - d / w, 0, 1 );
  }
  // mean of a 100 ms-sampled trace over [a, b] seconds (the current value if the window has not started)
  double mean( const std::vector< double >& v, double a, double b, int k ) const
  {
    double s = 0;
    int n = 0;
    for ( size_t i = 0; i < v.size(); ++i )
    {
      double t = ( i + 1 ) * 0.1;
      if ( t > a && t <= b ) s += v[ i ], ++n;
    }
    if ( n ) return s / n;
    return k == HR ? heart_.hr_bpm : ( k == MAP ? heart_.map_smooth : ( k == PUPIL ? eye_.diameter_mm : ve_hud_ ) );
  }

  double t_ = 0, sg_hz_ = 0, vg_hz_ = 0, sv_hz_ = 0, dil_hz_ = 0, sph_hz_ = 0, vent_hz_ = 0, phase_ = 0, ne_ = 0, ach_ = 0, ne_max_ = 0, ach_max_ = 0, ve_hud_ = 0, rr_hud_ = 0;
  int last_breaths_ = 0;
  mutable int ok_[ 4 ] = { 0, 0, 0, 0 };
  body::Heart heart_;
  body::Eye eye_;
  body::Lungs lungs_;
  Lowpass l_sv_, l_dil_, l_sph_, l_sa_, l_vg_, l_vent_;
  std::vector< Breath > breath_log_;
  std::vector< double > tr_[ 4 ];
  std::array< double, 7 > spikes_{};
  MotorReader motor_;
  Wiring wiring_;
};

} // namespace

std::unique_ptr< Task > make_fight_or_flight() { return std::make_unique< FightOrFlightTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( fight_or_flight, 420, "fight_or_flight.json", make_fight_or_flight ) }
