// Task "boss_autonomic_exam" (chapter finale of the autonomic chapter): a day in the life of the autonomic nervous system. One brainstem
// circuit must keep heart rate, blood pressure, pupil and breathing right through rest, standing up, exercise, a threat and recovery.
//
// WORLD (times in ms, 1 ms slices; bodies: spike::body::Heart, Eye, Lungs; docs/body/*.md; deterministic, no randomness)
//   Day (s): 0-15 rest, supine. 15: STAND UP (blood pools in the legs, tau 6 s) and stay upright. 50-70 EXERCISE (workload w ramps 0 -> 1 over
//   3 s, 1 until 67 s, back to 0 over 3 s). 85-100 THREAT (a predator cue, ramped over 0.5 s). Recovery 100-120; the episode ends at 120 s.
//   Sensors  "baroreceptor" (Poisson Hz = Heart::baro_mean_hz, ~41 Hz at MAP 93, as in the baroreflex task), "exercise_cue" (Poisson,
//            100 Hz x w: central command / muscle afferents, feed-forward), "threat" (Poisson, 250 Hz during the cue: hypothalamic defence area),
//            "chemo" (Poisson, 4 x (peripheral + central chemoreceptor Hz) of the Lungs model: ~50 Hz at rest, more as PaCO2 rises).
//   Motors   "sa_beat" (SA-node spikes = beats), "sympathetic" (modulator NE: the heart reads contractility and vascular tone from it exactly as in
//            the baroreflex task, sym = 0.2 + 0.5 (NE - 0.2)), "vagal" (modulator ACh: the circuit's SA cell slows with it), "dilator" (superior
//            cervical ganglion pool, Hz / 40) and "sphincter" (ciliary ganglion pool, Hz / 40): pupil D = 2 + 6 clamp(0.5 + 0.5 (dilator - sphincter))
//            mm at a fixed 30 cd/m2 (Eye model), "vent" (ventral respiratory group pool, d = Hz / 40, smoothed 1 s): a simple respiratory pattern
//            generator (breath rate 10 + 12 d per min, inspiratory amplitude 0.18 + 0.2 d) drives the Lungs model.
//   Body effects of the day: exercise multiplies CO2 production by 1 + 2 w (so PaCO2 rises unless ventilation follows: the chemoreflex loop is
//            closed) and dilates the working muscle's vessels (vascular drive - 0.1 w). Gas exchange runs 3x faster than real physiology (game time).
//
// MECHANISM (Engine::structure(), checked at the start and every 0.5 s): the chapter's parts must all be in the circuit: the baroreflex (both limbs, as
//   in the baroreflex task), central command (the exercise cue and the threat each reach, by excitation, the cells behind 'sympathetic'), the defence
//   reaction (the threat reaches the cells behind 'dilator' and behind 'vent'), the chemoreflex (the chemo input reaches the cells behind 'vent') and
//   separate pools for the dilator, the sphincter and the ventilation outputs. A missing component caps the score below the pass bar and the
//   verdict names it.
//
// SCORING (HR = last beat interval, MAP = 2 s-smoothed pressure, pupil, VE = last tidal volume x last breath rate; sampled every 100 ms)
//   windows (s): rest 8-15, stand 38-48, exercise 62-70, threat 92-100, recovery 110-120 (means).
//   REST      : HR 55-80, MAP 80-105, pupil 3-6 mm, VE 4-9 L/min.
//   STAND     : MAP 80-105 (the baroreflex holds pressure upright), HR 55-100, pupil within 0.8 mm of rest, VE within x1.3 of rest.
//   EXERCISE  : HR >= stand + 30 bpm, MAP >= stand + 5 mmHg and <= 140, VE >= x1.8 of stand.
//   THREAT    : HR >= stand + 30 bpm, MAP >= stand + 10 mmHg, pupil >= stand + 1.2 mm, VE >= x1.4 of stand.
//   RECOVERY  : within 10 bpm, 10 mmHg, 0.7 mm and x1.3 of the standing values.
//   value = 100 x mean over the five phases of the mean of that phase's soft 0..1 scores; pass: all 19 conditions hold at the end.
//   MODES: guided widens every tolerance by 1.25 (rises needed shrink to 0.8 of the above), researcher tightens by 0.85 (standard otherwise).
//
// Biology: arterial baroreflex (Kirchheim 1976; Guyenet 2006 Nat Rev Neurosci 7:335); during exercise and the defence reaction central command
// resets the baroreflex to a higher pressure instead of fighting the rise (Raven, Fadel & Ogoh 2006 Exp Physiol 91:37; Rowell & O'Leary 1990 J Appl
// Physiol 69:407); defence reaction (Cannon 1915; Hilton 1982 J Exp Biol 100:159; Dampney 1994 Physiol Rev 74:323); the pupil dilates by sympathetic drive and
// Edinger-Westphal withdrawal (Steinhauer & Hakerem 1992); ventilation follows CO2 production through the chemoreflex plus central command (Dempsey
// & Smith 2014 Eur Respir J 44:495; Guyenet, Stornetta & Bayliss 2010 J Comp Neurol 518:3883). All gains are game values.
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include "spike/body/eye.h"
#include "spike/body/heart.h"
#include "spike/body/lungs.h"
#include "mech_util.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_boss_autonomic_exam();
namespace
{

constexpr double kDuration = 120000, kExRamp = 3000, kThrRamp = 500;
// The scripted day (ms) and the scoring windows (s). The quick check runs the same day with shorter stretches (stand up at 12 s, 14 s of exercise,
// 12 s of threat, 6-8 s windows) and a gas exchange clock 1.5 times faster to match, scored by the same 19 conditions.
struct Sched
{
  double stand, ex0, ex1, thr0, thr1, end, lung_scale;
  double w[ 5 ][ 2 ];
};
constexpr Sched kFull{ 15000, 50000, 70000, 85000, 100000, kDuration, 3.0, { { 8, 15 }, { 38, 48 }, { 62, 70 }, { 92, 100 }, { 110, 120 } } };
constexpr Sched kQuick{ 12000, 36000, 50000, 56000, 68000, 82000, 4.5, { { 8, 12 }, { 28, 34 }, { 43, 50 }, { 62, 68 }, { 74, 82 } } };
constexpr double kThreatHz = 250, kExerciseHz = 100, kDilRef = 40, kSphRef = 40, kVentRef = 40, kLum = 30, kSymGain = 0.5, kChemoPerHz = 4.0;
// windows in s

enum Sys { HR = 0, MAP = 1, PUPIL = 2, VENT = 3 };
const char* kName[ 4 ] = { "hr", "map", "pupil", "ve" };
const char* kLabel[ 4 ] = { "heart rate", "blood pressure", "pupil", "ventilation" };
const char* kPhase[ 5 ] = { "rest", "stand", "exercise", "threat", "recovery" };

class ExamTask : public Task
{
public:
  ExamTask() { reset( 1 ); }
  std::string id() const override { return "boss_autonomic_exam"; }
  std::string title() const override { return "Autonomic exam"; }
  std::string brief() const override
  {
    return "A day in the life, in two minutes: lie at rest, stand up, exercise, get frightened, calm down. One brainstem circuit has to hold heart "
           "rate, blood pressure, pupil and breathing right through all of it, using only what you learned in this chapter. Standing pools blood in the "
           "legs: the baroreceptors ('baroreceptor') must raise sympathetic output and drop the vagus so pressure holds. Exercise brings central "
           "command ('exercise_cue') and a CO2 load: the heart must speed up, pressure rise a little, and ventilation follow the chemoreceptors "
           "('chemo') and the command. A threat ('threat') opens the pupil and raises heart rate, pressure and breathing at once. The same "
           "baroreflex that holds pressure when you stand would fight the rise in exercise and fright, so the central signals must also turn the "
           "reflex down (CVLM) while they push the sympathetic side up and the vagal side down. Wire all of it into one circuit and read out the pacemaker "
           "('sa_beat'), the two heart modulators ('sympathetic' = NE, 'vagal' = ACh: set both to kind modulator), 'dilator', 'sphincter' and 'vent'. Score: "
           "five phases (rest, stand, exercise, threat, recovery) x the four organs, 19 conditions; each is a soft score, the task passes when all hold. "
           "";
  }
  /// The cell type the reference circuit and the verdicts assume; the numbers of the build live in recipe() (shown by mode in the lab).
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A starting point: the Baroreflex circuit, plus the Fight-or-Flight pupil and breathing arms, with the hypothalamus and the exercise relay "
           "both inhibiting the vagal and depressor cells and exciting the vasomotor cells and the ventral respiratory group, and a chemoreceptor relay "
           "onto the ventral respiratory group.";
  }
  std::vector< std::string > sensor_channels() const override { return { "baroreceptor", "exercise_cue", "threat", "chemo" }; }
  std::vector< std::string > motor_channels() const override { return { "sa_beat", "sympathetic", "vagal", "dilator", "sphincter", "vent" }; }
  double duration_ms() const override
  {
    quick_ = false;
    return kDuration;
  }
  /// Quick check: the same day, compressed (see Sched). The episode asked for last, duration_ms() or quick_duration_ms(), is the one the task scripts and
  /// scores (spike-run and the test harness ask after reset(), before the first step).
  double quick_duration_ms() const override
  {
    quick_ = true;
    return kQuick.end;
  }
  std::string chapter() const override { return "autonomic"; }
  std::string view() const override { return "dashboard"; }
  int par_neurons() const override { return 75; }
  std::string level_kind() const override { return "boss"; }
  bool on_spine() const override { return true; }
  std::vector< std::string > concepts_used() const override { return { "negative_feedback", "feedforward", "neuromodulation", "divergent_command", "rhythm_gating" }; }

  std::vector< Event > timeline() const override
  {
    return { { kFull.stand, 3000, "stand up" }, { kFull.stand, kDuration - kFull.stand, "upright" }, { kFull.ex0, kFull.ex1 - kFull.ex0, "exercise" },
             { kFull.thr0, kFull.thr1 - kFull.thr0, "threat cue" } };
  }
  std::vector< Band > bands() const override
  {
    // envelopes over the whole day (the per-phase windows are in the brief)
    return { { "hr_bpm", "bpm", 55, 130 }, { "map_mmhg", "mmHg", 80, 140 }, { "pupil_mm", "mm", 3, 8 }, { "ventilation", "L/min", 4, 30 } };
  }
  std::vector< std::string > modulators() const override { return { "NE", "ACh" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    std::map< std::string, ChannelKind > k;
    k[ "baroreceptor" ] = ChannelKind{ "poisson", 41 };
    k[ "exercise_cue" ] = ChannelKind{ "poisson", kExerciseHz };
    k[ "threat" ] = ChannelKind{ "poisson", kThreatHz };
    k[ "chemo" ] = ChannelKind{ "poisson", 50 };
    ChannelKind ne;
    ne.kind = "modulator";
    ne.modulator = "NE";
    ne.gain = 0.0076;
    ne.tau_ms = 1000;
    k[ "sympathetic" ] = ne;
    ChannelKind ach;
    ach.kind = "modulator";
    ach.modulator = "ACh";
    ach.gain = 0.012;
    ach.tau_ms = 500;
    k[ "vagal" ] = ach;
    for ( const char* r : { "sa_beat", "dilator", "sphincter", "vent" } )
      k[ r ].kind = "rate";
    return k;
  }

  void reset( uint64_t ) override
  {
    t_ = 0;
    sch_ = kFull;
    heart_ = body::Heart();
    eye_.reset( 0.0, 0.5 );
    body::LungParams lp;
    lp.time_scale = sch_.lung_scale;
    lungs_ = body::Lungs( lp );
    lungs_.reset();
    for ( auto* l : { &l_dil_, &l_sph_ } ) { l->tau_ms = 20; l->reset( 0 ); }
    l_vent_.tau_ms = 1000;
    l_vent_.reset( 0.4 );
    dil_hz_ = sph_hz_ = vent_hz_ = 0;
    phase_ = 0;
    ne_ = ach_ = ne_max_ = ach_max_ = 0;
    ve_hud_ = rr_hud_ = 0;
    spikes_.fill( 0 );
    for ( auto& v : tr_ ) v.clear();
    motor_.invalidate();
    wiring_.invalidate();
    mech_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    if ( t_ == 0 && ( quick_ ? kQuick.end : kFull.end ) != sch_.end )  // the schedule follows the episode asked for
    {
      sch_ = quick_ ? kQuick : kFull;
      body::LungParams lp;
      lp.time_scale = sch_.lung_scale;
      lungs_ = body::Lungs( lp );
      lungs_.reset();
    }
    wiring_.probe( e, sensor_channels(), motor_channels() );
    mech_.refresh( e, t_, []( const Structure& st ) { return parts_missing( st ); } );
    const double w = workload( t_ );
    const double thr = kThreatHz * clampd( ( t_ - sch_.thr0 ) / kThrRamp, 0, 1 ) * ( t_ < sch_.thr1 ? 1.0 : 0.0 );
    e.set_input( "baroreceptor", heart_.baro_mean_hz );
    e.set_input( "exercise_cue", kExerciseHz * w );
    e.set_input( "threat", thr );
    e.set_input( "chemo", kChemoPerHz * ( lungs_.peripheral_hz + lungs_.central_hz ) );
    e.step( dt );
    static const char* kCh[ 6 ] = { "sa_beat", "sympathetic", "vagal", "dilator", "sphincter", "vent" };
    for ( int i = 0; i < 6; ++i )
      spikes_[ i ] += motor_.spike_count( e, kCh[ i ], dt );
    const double beats = motor_.spike_count( e, "sa_beat", dt );
    dil_hz_ = l_dil_.step( motor_.hz_per_neuron( e, "dilator" ), dt );
    sph_hz_ = l_sph_.step( motor_.hz_per_neuron( e, "sphincter" ), dt );
    vent_hz_ = l_vent_.step( motor_.hz_per_neuron( e, "vent" ), dt );
    ne_ = e.modulator( "NE" );
    ach_ = e.modulator( "ACh" );
    ne_max_ = std::max( ne_max_, ne_ );
    ach_max_ = std::max( ach_max_, ach_ );

    body::HeartInput hi;
    hi.sa_spikes = beats > 0.5 ? 1 : 0;
    hi.sa_rate_hz = 0;
    const double sym = clampd( 0.2 + kSymGain * ( ne_ - 0.2 ), 0, 1 );
    hi.sym_cardiac = sym;
    hi.sym_vascular = clampd( sym - 0.1 * w, 0, 1 );
    hi.vagal = 0.6;
    hi.standing = t_ >= sch_.stand ? 1.0 : 0.0;
    heart_.step( dt, hi );

    body::EyeInput ei;
    ei.sphincter = clampd( sph_hz_ / kSphRef, 0, 1 );
    ei.dilator = clampd( dil_hz_ / kDilRef, 0, 1 );
    ei.luminance = kLum;
    eye_.step( dt, ei );

    const double d = clampd( vent_hz_ / kVentRef, 0, 1.5 );
    const double f_per_min = 10.0 + 12.0 * d, amp = 0.18 + 0.2 * d;
    phase_ += f_per_min / 60000.0 * dt;
    if ( phase_ >= 1.0 ) phase_ -= 1.0;
    body::LungInput li;
    li.drive = phase_ < 0.35 ? amp * std::sin( kPi * phase_ / 0.35 ) : 0.0;
    li.metabolic = 1.0 + 2.0 * w;
    lungs_.step( dt, li );
    t_ += dt;
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
    b.readouts[ "workload" ] = workload( t_ );
    b.readouts[ "standing" ] = t_ >= sch_.stand ? 1.0 : 0.0;
    b.readouts[ "threat" ] = ( t_ >= sch_.thr0 && t_ < sch_.thr1 ) ? 1.0 : 0.0;
    b.readouts[ "baro_hz" ] = heart_.baro_mean_hz;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    const double f = tol();
    double m[ 5 ][ 4 ];
    for ( int p = 0; p < 5; ++p )
      for ( int k = 0; k < 4; ++k )
        m[ p ][ k ] = mean( tr_[ k ], sch_.w[ p ][ 0 ], sch_.w[ p ][ 1 ], k );
    // conditions: c[phase][i] = pass, sc = soft score
    std::vector< Cond > cs;
    // REST
    cs.push_back( band( 0, HR, m[ 0 ][ HR ], 55, 80, 12 ) );
    cs.push_back( band( 0, MAP, m[ 0 ][ MAP ], 80, 105, 12 ) );
    cs.push_back( band( 0, PUPIL, m[ 0 ][ PUPIL ], 3, 6, 1.5 ) );
    cs.push_back( band( 0, VENT, m[ 0 ][ VENT ], 4, 9, 3 ) );
    // STAND: held
    cs.push_back( band( 1, MAP, m[ 1 ][ MAP ], 80 - 0 * f, 105, 12 / f ) );
    cs.push_back( band( 1, HR, m[ 1 ][ HR ], 55, 100, 15 ) );
    cs.push_back( within( 1, PUPIL, m[ 1 ][ PUPIL ], m[ 0 ][ PUPIL ], 0.8 * f, 1.0 ) );
    cs.push_back( ratio_within( 1, VENT, m[ 1 ][ VENT ], m[ 0 ][ VENT ], 1.3 * f ) );
    // EXERCISE
    cs.push_back( rise( 2, HR, m[ 2 ][ HR ] - m[ 1 ][ HR ], 30 / f ) );
    cs.push_back( rise( 2, MAP, m[ 2 ][ MAP ] - m[ 1 ][ MAP ], 5 / f ) );
    if ( m[ 2 ][ MAP ] > 140 )
      cs.back().ok = false;
    cs.push_back( ratio_rise( 2, VENT, m[ 2 ][ VENT ], m[ 1 ][ VENT ], 1.8 / ( f > 1 ? 1.15 : ( f < 1 ? 0.95 : 1.0 ) ) ) );
    // THREAT
    cs.push_back( rise( 3, HR, m[ 3 ][ HR ] - m[ 1 ][ HR ], 30 / f ) );
    cs.push_back( rise( 3, MAP, m[ 3 ][ MAP ] - m[ 1 ][ MAP ], 10 / f ) );
    cs.push_back( rise( 3, PUPIL, m[ 3 ][ PUPIL ] - m[ 1 ][ PUPIL ], 1.2 / f ) );
    cs.push_back( ratio_rise( 3, VENT, m[ 3 ][ VENT ], m[ 1 ][ VENT ], 1.4 / ( f > 1 ? 1.1 : ( f < 1 ? 0.97 : 1.0 ) ) ) );
    // RECOVERY
    cs.push_back( within( 4, HR, m[ 4 ][ HR ], m[ 1 ][ HR ], 10 * f, 20 ) );
    cs.push_back( within( 4, MAP, m[ 4 ][ MAP ], m[ 1 ][ MAP ], 10 * f, 20 ) );
    cs.push_back( within( 4, PUPIL, m[ 4 ][ PUPIL ], m[ 1 ][ PUPIL ], 0.7 * f, 1.4 ) );
    cs.push_back( ratio_within( 4, VENT, m[ 4 ][ VENT ], m[ 1 ][ VENT ], 1.3 * f ) );

    double phase_sum[ 5 ] = {}, phase_n[ 5 ] = {};
    int ok_n = 0;
    for ( auto& c : cs )
    {
      phase_sum[ c.phase ] += c.soft;
      phase_n[ c.phase ] += 1;
      ok_n += c.ok ? 1 : 0;
    }
    double total = 0;
    for ( int p = 0; p < 5; ++p )
      total += phase_sum[ p ] / phase_n[ p ] / 5.0;
    for ( int p = 0; p < 5; ++p )
      for ( int k = 0; k < 4; ++k )
        s.parts[ std::string( kName[ k ] ) + "_" + kPhase[ p ] ] = m[ p ][ k ];
    s.parts[ "conditions_met" ] = ok_n;
    s.parts[ "conditions_total" ] = static_cast< double >( cs.size() );
    s.parts[ "sa_spikes" ] = spikes_[ 0 ];
    s.parts[ "ne_peak" ] = ne_max_;
    s.parts[ "ach_peak" ] = ach_max_;
    s.value = 100.0 * total;
    const bool done = t_ >= sch_.end;
    s.passed = done && ok_n == static_cast< int >( cs.size() ) && wiring_.ok() && mech_.ok();
    if ( !wiring_.ok() )
      s.value = std::min( s.value, 20.0 );
    else if ( !mech_.ok() )
      s.value = std::min( s.value, mech::kNoMechanismCap );
    // verdict: first failing condition in the order of the day (phases not yet reached are not judged)
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( spikes_[ 0 ] < 1 )
      o << "The pacemaker never fired: no heartbeat. Check 'sa_beat'.";
    else if ( !mech_.ok() )
      o << mech_.missing() << " (The run itself: " << ok_n << " of " << cs.size() << " conditions met.)";
    else if ( spikes_[ 1 ] >= 1 && ne_max_ < 1e-6 )
      o << "'sympathetic' fires but releases no noradrenaline: make it a modulator output (NE).";
    else if ( spikes_[ 2 ] >= 1 && ach_max_ < 1e-6 )
      o << "'vagal' fires but releases no acetylcholine: make it a modulator output (ACh).";
    else
    {
      std::string bad;
      for ( auto& c : cs )
        if ( !c.ok && t_ >= sch_.w[ c.phase ][ 1 ] * 1000.0 && bad.empty() )
          bad = c.text( kPhase[ c.phase ], kLabel[ c.sys ], m[ c.phase ][ c.sys ], c.sys == VENT ? m[ 1 ][ VENT ] : m[ 1 ][ c.sys ] );
      if ( !bad.empty() )
        o << bad;
      else if ( !done )
        o << "So far so good (" << heart_.hr_bpm << " bpm, MAP " << heart_.map_smooth << " mmHg, pupil " << eye_.diameter_mm << " mm).";
      else
        o << "The whole day held: HR " << m[ 0 ][ HR ] << " rest, " << m[ 1 ][ HR ] << " standing, " << m[ 2 ][ HR ] << " exercise, " << m[ 3 ][ HR ]
          << " threat; MAP " << m[ 1 ][ MAP ] << " standing, " << m[ 3 ][ MAP ] << " in the threat.";
    }
    s.verdict = o.str();
    return s;
  }

  std::map< std::string, Metric > metrics() const override
  {
    Score s = score();
    return { { "conditions_met", Metric{ s.parts[ "conditions_met" ], "of 19", false } },
             { "map_standing_error", Metric{ std::fabs( s.parts[ "map_stand" ] - 92.5 ), "mmHg", true } } };
  }

  std::vector< Hint > hints() const override
  {
    Score s = score();
    std::vector< Hint > h;
    const double ne = s.parts[ "ne_peak" ];
    if ( !mech_.ok() )
    {
      h.push_back( { 1, "The exam needs every part of the chapter in one circuit: the baroreflex, the central command, the defence reaction and the chemoreflex.", "" } );
      h.push_back( { 2, "One of those parts is not wired through to its output; read the verdict for which.", "" } );
      h.push_back( { 3, "Reuse the earlier levels' circuits as blocks and connect each input to the organs it should move.", "" } );
    }
    else if ( s.parts[ "map_stand" ] < 80 )
    {
      h.push_back( { 1, "Standing is a pressure drop: negative feedback from the baroreceptors must raise sympathetic output.", "" } );
      h.push_back( { 2, "Follow the baroreceptor signal through NTS to the depressor cells and see what they do to the vasomotor neurons.", "CVLM" } );
      h.push_back( { 3, "The baroreceptor relay excites an inhibitory depressor group that silences the tonic vasomotor cells, which drive the sympathetic output; the same relay also excites the vagal cells.", "RVLM" } );
    }
    else if ( s.parts[ "hr_exercise" ] - s.parts[ "hr_stand" ] < 30 || s.parts[ "hr_threat" ] - s.parts[ "hr_stand" ] < 30 )
    {
      h.push_back( { 1, "Exercise and fright do not wait for the baroreflex: a central command must override it, speeding the heart and raising pressure.", "" } );
      h.push_back( { 2, "Look at what the exercise relay and the hypothalamus do to the vagal cells and the depressor cells.", "CC" } );
      h.push_back( { 3, "Let the exercise relay and the hypothalamus inhibit the vagal cells and the depressor group, and excite the vasomotor cells.", "HYP" } );
    }
    else if ( s.parts[ "pupil_threat" ] - s.parts[ "pupil_stand" ] < 1.2 )
    {
      h.push_back( { 1, "The pupil has its own two arms: the sympathetic dilator and the parasympathetic sphincter, and a threat acts on both.", "" } );
      h.push_back( { 2, "Look at the hypothalamus connections to the superior cervical ganglion and the Edinger-Westphal cells.", "SCG" } );
      h.push_back( { 3, "The hypothalamus should excite the dilator ganglion and inhibit the parasympathetic relay of the sphincter.", "EW" } );
    }
    else
    {
      h.push_back( { 1, "Ventilation follows two signals: CO2 through the chemoreceptors, and the command from above.", "" } );
      h.push_back( { 2, "Look at the ventral respiratory group's inputs.", "VRG" } );
      h.push_back( { 3, "Feed the chemoreceptor relay (feedback) and the exercise and threat relays (feed-forward) onto the ventral respiratory group that drives the vent output.", "VRG" } );
    }
    (void)ne;
    return h;
  }

  Note notebook() const override
  {
    return { "Central command resets the baroreflex",
             "During exercise the arterial pressure rises, yet the baroreflex keeps working: central command resets it to operate at the higher pressure "
             "(here by turning down the depressor arm, CVLM) instead of pulling the pressure back down. The same happens in the defence reaction. A circuit with a fixed baroreflex set point fights every "
             "rise; real brainstems do not.",
             "Raven PB, Fadel PJ, Ogoh S (2006) Arterial baroreflex resetting during exercise: a current perspective. Exp Physiol 91:37-49, doi:10.1113/expphysiol.2005.032250; Rowell LB, O'Leary DS (1990) J Appl Physiol 69:407; Dampney RAL (1994) Physiol Rev 74:323" };
  }

  std::array< double, 4 > world_bounds() const override { return { 0, 0, 1, 1 }; }

private:
  // the mechanism: every component of the chapter is in the circuit
  static std::string parts_missing( const Structure& st )
  {
    const auto sym = mech::out_pops( st, "sympathetic" ), dil = mech::out_pops( st, "dilator" ), sph = mech::out_pops( st, "sphincter" ),
               vent = mech::out_pops( st, "vent" ), cue = mech::in_pops( st, "exercise_cue" ), thr = mech::in_pops( st, "threat" ),
               chemo = mech::in_pops( st, "chemo" );
    std::string base = mech::baroreflex_limbs_missing( st );
    if ( !base.empty() )
      return "The baroreflex part of the exam is incomplete. " + base;
    if ( !mech::path( st, cue, sym, +1 ) )
      return "Central command is missing: the exercise cue must excite the cells behind the sympathetic output, so that the heart and vessels follow the effort.";
    if ( !mech::path( st, thr, sym, +1 ) )
      return "The defence reaction is missing its sympathetic part: the threat must excite the cells behind the sympathetic output.";
    if ( dil.empty() || sph.empty() || vent.empty() || mech::overlap( dil, sph ) || mech::overlap( dil, vent ) || mech::overlap( sph, vent ) )
      return "The pupil and breathing outputs need their own pools: the dilator, the sphincter and the vent output must each read separate cells.";
    if ( !mech::path( st, thr, dil, +1 ) )
      return "The defence reaction does not reach the pupil: the threat must excite the cells behind the dilator output.";
    if ( !mech::path( st, thr, vent, +1 ) && !mech::path( st, cue, vent, +1 ) )
      return "The central signals (threat, exercise cue) do not reach the breathing group: ventilation cannot follow the command.";
    if ( !mech::path( st, chemo, vent, +1 ) )
      return "The chemoreflex is missing: the chemo input must excite the cells behind the vent output, so that ventilation follows CO2.";
    return "";
  }

  struct Cond
  {
    int phase, sys;
    bool ok;
    double soft;
    int kind;  // 0 band, 1 within, 2 rise, 3 ratio rise, 4 ratio within
    double a, b;
    std::string text( const char* ph, const char* what, double v, double ref ) const
    {
      std::ostringstream o;
      o << std::fixed << std::setprecision( 1 );
      o << "In the " << ph << " phase the " << what << " is " << v;
      if ( kind == 0 )
        o << ", outside " << a << "-" << b << ".";
      else if ( kind == 1 )
        o << ", more than " << a << " from the standing value " << ref << ".";
      else if ( kind == 2 )
        o << " (standing " << ref << "): it needs to rise by " << a << (a > 0 && ph[ 0 ] != 'e' ? "" : "") << ".";
      else if ( kind == 3 )
        o << " (standing " << ref << "): it needs to be x" << a << " the standing value.";
      else
        o << ", more than x" << a << " from the standing value " << ref << ".";
      return o.str();
    }
  };
  double tol() const { return mode_ == "guided" ? 1.25 : ( mode_ == "researcher" ? 0.85 : 1.0 ); }
  static Cond band( int ph, int sys, double v, double lo, double hi, double w )
  {
    double d = v < lo ? lo - v : ( v > hi ? v - hi : 0.0 );
    return { ph, sys, d == 0.0, clampd( 1.0 - d / w, 0, 1 ), 0, lo, hi };
  }
  static Cond within( int ph, int sys, double v, double ref, double tol, double w )
  {
    double d = std::fabs( v - ref );
    return { ph, sys, d <= tol, clampd( 1.0 - std::max( 0.0, d - tol ) / w, 0, 1 ), 1, tol, 0 };
  }
  static Cond rise( int ph, int sys, double dv, double need )
  {
    return { ph, sys, dv >= need, clampd( dv / need, 0, 1 ), 2, need, 0 };
  }
  static Cond ratio_rise( int ph, int sys, double v, double ref, double need )
  {
    double r = ref > 0.1 ? v / ref : 0.0;
    return { ph, sys, r >= need, clampd( ( r - 1.0 ) / ( need - 1.0 ), 0, 1 ), 3, need, 0 };
  }
  static Cond ratio_within( int ph, int sys, double v, double ref, double tol )
  {
    double r = ( ref > 0.1 && v > 0.1 ) ? std::max( v / ref, ref / v ) : 99.0;
    return { ph, sys, r <= tol, clampd( 1.0 - ( r - tol ) / ( 2.0 * ( tol - 1.0 ) ), 0, 1 ), 4, tol, 0 };
  }

  double workload( double t ) const
  {
    if ( t < sch_.ex0 || t >= sch_.ex1 + kExRamp ) return 0.0;
    if ( t < sch_.ex0 + kExRamp ) return ( t - sch_.ex0 ) / kExRamp;
    if ( t < sch_.ex1 - kExRamp ) return 1.0;
    return clampd( ( sch_.ex1 - t ) / kExRamp, 0, 1 );
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

  double t_ = 0, dil_hz_ = 0, sph_hz_ = 0, vent_hz_ = 0, phase_ = 0, ne_ = 0, ach_ = 0, ne_max_ = 0, ach_max_ = 0, ve_hud_ = 0, rr_hud_ = 0;
  body::Heart heart_;
  body::Eye eye_;
  body::Lungs lungs_;
  Lowpass l_dil_, l_sph_, l_vent_;
  std::vector< double > tr_[ 4 ];
  std::array< double, 6 > spikes_{};
  MotorReader motor_;
  Wiring wiring_;
  mech::Gate mech_;
  Sched sch_ = kFull;
  mutable bool quick_ = false;
};

} // namespace

std::unique_ptr< Task > make_boss_autonomic_exam() { return std::make_unique< ExamTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( boss_autonomic_exam, 435, "boss_autonomic_exam.json", make_boss_autonomic_exam ) }
