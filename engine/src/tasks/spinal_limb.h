// Shared limb rig for the "spinal" chapter tasks (stretch_reflex, renshaw, posture, reach).
//
// One hinge joint with a flexor (agonist) / extensor (antagonist) pair (body/joint.h) in a closed loop with the circuit:
//
//   joint --(spindle Ia / II, Golgi Ib, 8 ms afferent conduction)--> sensor channels --> circuit
//   circuit --> alpha motor pools (mean Hz per neuron, low-pass 15 ms, 10 ms efferent conduction) --> Muscle excitation
//
// Sensor channels are dc generators: the task writes the receptor's firing rate (Hz) and the circuit's input weight turns it into a
// current (4 pA per Hz) for the afferent neurons it targets, which then fire regularly at about that rate (real primary endings are
// very regular, CV ~ 0.1). Poisson trains were tried first: their noise, multiplied by the spindle's velocity derivative, made the loop tremble. Motor channels read alpha motoneuron pools: the muscle's pool excitation (0..1) is the pool's
// total rate (cells x mean rate) / (20 units x rmax); the Muscle then recruits its motor units by size (Henneman). An optional gamma pool sets the
// fusimotor drive of both spindles.
//
// Required channels differ per task; "optional" afferent channels (Ib, II, ...) are driven only if the circuit declares them,
// so a player can add Golgi feedback without the task complaining about a missing wire.
#pragma once
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "spike/body/joint.h"
#include "spike/task.h"
#include "mech_b.h"
#include "task_util.h"

namespace spike::tasks
{

struct LimbRigConfig
{
  double aff_delay_ms = 10;  // spindle -> spinal cord conduction (about 0.6 m at 60 m/s)
  double eff_delay_ms = 10;  // alpha motoneuron -> muscle conduction
  double rate_tau_ms = 15;   // pool rate -> excitation smoothing (neuromuscular transmission, EPSP-to-twitch lag)
  double emg_tau_ms = 8;     // faster smoothing used only for the EMG-like readouts and latency detection
  double rmax_hz = 50;       // rate of ONE motoneuron when all `units_ref` units of the muscle fire at it: excitation 1.0 = units_ref * rmax_hz spikes/s in total
  double units_ref = 20;     // motor units of a muscle (Muscle::n_units): a pool of N cells can drive at most N / units_ref of the muscle
  double gamma_rmax_hz = 40; // mean gamma motoneuron rate that means fusimotor drive 1.0
  double gamma_default = 0.3;// fusimotor drive when the circuit has no gamma pool
  double gamma_tau_ms = 100; // fusimotor action builds up slowly (intrafusal contraction)
  body::JointParams joint;
  LimbRigConfig()
  {
    joint.mass_kg = 0;     // arm supported by default (posture turns gravity on)
    joint.inertia = 0.1;   // forearm and hand in a light manipulandum
    joint.damping = 0.3;   // N m s/rad: soft-tissue viscosity of the elbow is about 0.1-0.5
    joint.flexor.fatigue = joint.extensor.fatigue = false;  // seconds-long trials: fatigue would only drift the baseline
  }
};

class LimbRig
{
public:
  void reset( const LimbRigConfig& c, double theta0 )
  {
    cfg = c;
    joint.configure( c.joint );
    joint.reset( theta0 );
    shadow = joint;
    frozen_ = false;
    for ( auto* d : { &d_ag_, &d_ant_, &d_gam_ } )
      d->delay_ms = c.eff_delay_ms, d->reset( 0 );
    for ( auto* d : { &a_ag_ia_, &a_ant_ia_, &a_ag_ii_, &a_ant_ii_, &a_ag_ib_, &a_ant_ib_ } )
      d->delay_ms = c.aff_delay_ms;
    a_ag_ia_.reset( joint.params().spindle.ia_bias );
    a_ant_ia_.reset( joint.params().spindle.ia_bias );
    a_ag_ii_.reset( joint.params().spindle.ii_bias );
    a_ant_ii_.reset( joint.params().spindle.ii_bias );
    a_ag_ib_.reset( 0 );
    a_ant_ib_.reset( 0 );
    ia_ag = ia_ant = joint.params().spindle.ia_bias;
    ii_ag = ii_ant = joint.params().spindle.ii_bias;
    ib_ag = ib_ant = 0;
    lp_ag_.tau_up_ms = lp_ag_.tau_down_ms = c.rate_tau_ms;
    lp_ant_ = lp_gam_ = lp_ag_;
    lp_gam_.tau_up_ms = lp_gam_.tau_down_ms = c.gamma_tau_ms;
    lp_ag_.reset( 0 ); lp_ant_.reset( 0 ); lp_gam_.reset( 0 );
    emg_ag_f_ = body::Lowpass( c.emg_tau_ms );
    emg_ant_f_ = body::Lowpass( c.emg_tau_ms );
    emg_ag = emg_ant = 0;
    slow_ag_ = slow_ant_ = slow_gam_ = 0;
    exc_ag = exc_ant = gam = c.gamma_default;
    exc_ag = exc_ant = 0;
    sh_exc_ag_ = sh_exc_ant_ = sh_gam_ = 0;
  }

  /// Advance one tick of h ms (<= 1). ag_hz / ant_hz / gam_hz: pool mean rate per neuron (Hz) read from the circuit.
  void tick( double h, double ag_hz, double ant_hz, double gam_hz, bool has_gamma, double torque, double n_ag = 20, double n_ant = 20 )
  {
    const double lpa = lp_ag_.step( ag_hz, h ), lpn = lp_ant_.step( ant_hz, h ), lpg = lp_gam_.step( gam_hz, h );
    emg_ag = emg_ag_f_.step( ag_hz, h );
    emg_ant = emg_ant_f_.step( ant_hz, h );
    rate_ag = lpa;
    rate_ant = lpn;
    // Muscle drive = total pool rate (cells x mean rate) over the rate of the whole muscle, so it scales with the number of recruited motor
    // units and their rates: one motoneuron at rmax can excite one unit of 20, not the muscle. Pool size is therefore part of the design.
    const double full = cfg.units_ref * cfg.rmax_hz;
    exc_ag = clampd( d_ag_.step( lpa * n_ag, h ) / full, 0, 1 );
    exc_ant = clampd( d_ant_.step( lpn * n_ant, h ) / full, 0, 1 );
    gam = has_gamma ? clampd( d_gam_.step( lpg, h ) / cfg.gamma_rmax_hz, 0, 1 ) : cfg.gamma_default;
    body::JointInput in;
    in.ag.excitation = exc_ag;
    in.ant.excitation = exc_ant;
    in.ext_torque = torque;
    in.gamma_dyn = in.gamma_sta = gam;
    joint.step( h, in );
    // slow "tonic" level of the drive (tau 120 ms): the open-loop reference holds this when a stretch starts
    const double a = 1.0 - std::exp( -h / 120.0 );
    slow_ag_ += a * ( exc_ag - slow_ag_ );
    slow_ant_ += a * ( exc_ant - slow_ant_ );
    slow_gam_ += a * ( gam - slow_gam_ );
    if ( frozen_ )
    {
      body::JointInput s;
      s.ag.excitation = sh_exc_ag_;
      s.ant.excitation = sh_exc_ant_;
      s.ext_torque = torque;
      s.gamma_dyn = s.gamma_sta = sh_gam_;
      shadow.step( h, s );
    }
    ia_ag = a_ag_ia_.step( joint.ag_aff.ia, h );
    ia_ant = a_ant_ia_.step( joint.ant_aff.ia, h );
    ii_ag = a_ag_ii_.step( joint.ag_aff.ii, h );
    ii_ant = a_ant_ii_.step( joint.ant_aff.ii, h );
    ib_ag = a_ag_ib_.step( joint.ag_aff.ib, h );
    ib_ant = a_ant_ib_.step( joint.ant_aff.ib, h );
  }

  /// Open-loop reference: from now on, an exact copy of the limb receives the same external torque but the drive held at the
  /// tonic level of the last ~120 ms (no reflex change). Used to measure how much the circuit's reaction reduced the deflection.
  void freeze_shadow()
  {
    shadow = joint;
    sh_exc_ag_ = slow_ag_;
    sh_exc_ant_ = slow_ant_;
    sh_gam_ = slow_gam_;
    frozen_ = true;
  }
  void release_shadow() { frozen_ = false; }
  bool shadow_active() const { return frozen_; }

  LimbRigConfig cfg;
  body::Joint joint, shadow;
  // delayed afferent rates, to be written to the circuit's sensor channels (Hz)
  double ia_ag = 0, ia_ant = 0, ii_ag = 0, ii_ant = 0, ib_ag = 0, ib_ant = 0;
  double emg_ag = 0, emg_ant = 0;      // fast-smoothed pool rates (Hz per neuron)
  double rate_ag = 0, rate_ant = 0;    // pool rates smoothed with rate_tau_ms, before the efferent delay (Hz per neuron)
  double exc_ag = 0, exc_ant = 0, gam = 0;

private:
  class Delayed
  {
  public:
    double delay_ms = 0;
    void reset( double v ) { dl_.delay_ms = delay_ms; dl_.reset( v ); }
    double step( double x, double h ) { dl_.delay_ms = delay_ms; return dl_.step( x, h ); }
  private:
    body::DelayLine dl_;
  };
  Delayed d_ag_, d_ant_, d_gam_, a_ag_ia_, a_ant_ia_, a_ag_ii_, a_ant_ii_, a_ag_ib_, a_ant_ib_;
  body::Lowpass lp_ag_, lp_ant_, lp_gam_, emg_ag_f_, emg_ant_f_;
  double slow_ag_ = 0, slow_ant_ = 0, slow_gam_ = 0, sh_exc_ag_ = 0, sh_exc_ant_ = 0, sh_gam_ = 0;
  bool frozen_ = false;
};

/// Smooth push profile: 0 -> 1 over `ramp` ms, hold, 1 -> 0 over `ramp` (cosine ramps, so the limb sees a finite jerk).
inline double push_profile( double t, double t0, double hold_ms, double ramp_ms )
{
  const double u = t - t0;
  if ( u <= 0 || u >= hold_ms + 2 * ramp_ms )
    return 0;
  auto s = []( double x ) { return 0.5 - 0.5 * std::cos( kPi * clampd( x, 0, 1 ) ); };
  if ( u < ramp_ms )
    return s( u / ramp_ms );
  if ( u < ramp_ms + hold_ms )
    return 1;
  return s( 1.0 - ( u - ramp_ms - hold_ms ) / ramp_ms );
}

/// Shared Task plumbing: sensors, motors, delays, limb drawing, readouts. A derived task supplies the protocol (torque, cues)
/// and the scoring.
class LimbTaskBase : public Task
{
public:
  std::string chapter() const override { return "spinal"; }
  std::string view() const override { return "limb"; }

  void step( Engine& e, double dt ) override
  {
    if ( !probed_inputs_ )
    {
      graph_.capture( e );  // the circuit as the run starts: mechanism criteria are judged on its structure (docs/REQ-0.4.md rule 1)
      mech_.clear();
      if ( graph_.valid() )
        mechanism( graph_, mech_ );
      for ( const auto& c : e.inputs() )
        inputs_.insert( c.name );
      for ( const auto& c : e.outputs() )
        if ( c.name == "gamma_motor" && !c.neurons.empty() )
          gamma_present_ = true;
      // the interneurons that inhibit the motor pools: a verdict that says "strengthen the inhibition" must first know whether they fire at all
      mechb::PopSet motors;
      for ( const char* ch : { "ag_motor", "ant_motor", "gamma_motor" } )
      {
        const auto p = graph_.out_pops( ch );
        motors.insert( p.begin(), p.end() );
      }
      inhib_.arm( e.structure(), std::set< int >( motors.begin(), motors.end() ), -1 );
      probed_inputs_ = true;
    }
    wiring_.probe( e, sensor_channels(), motor_channels() );
    // afferents (only channels the circuit declares)
    put( e, "ag_Ia", rig_.ia_ag );
    put( e, "ant_Ia", rig_.ia_ant );
    put( e, "ag_II", rig_.ii_ag );
    put( e, "ant_II", rig_.ii_ant );
    put( e, "ag_Ib", rig_.ib_ag );
    put( e, "ant_Ib", rig_.ib_ant );
    cues( e, t_ );
    e.step( dt );
    if ( watch_inhib_now() ) inhib_.tick( e );
    const double ag = motor_.hz_per_neuron( e, "ag_motor" ), ant = motor_.hz_per_neuron( e, "ant_motor" );
    const bool hg = gamma_present_;
    const double gm = hg ? motor_.hz_per_neuron( e, "gamma_motor" ) : 0.0;
    const double sc_ag = motor_.spike_count( e, "ag_motor", dt ), sc_ant = motor_.spike_count( e, "ant_motor", dt );
    spikes_ag_ += sc_ag;
    spikes_ant_ += sc_ant;
    pool_ag_ = motor_.pool_size( e, "ag_motor" );
    pool_ant_ = motor_.pool_size( e, "ant_motor" );
    const int n = std::max( 1, static_cast< int >( std::ceil( dt - 1e-9 ) ) );
    const double h = dt / n;
    for ( int i = 0; i < n; ++i )
    {
      cnt_ag_.push_back( sc_ag / n );  // pool spikes in this tick (one sample per ms when the harness steps 1 ms)
      cnt_ant_.push_back( sc_ant / n );
      const double tq = torque( t_ );
      rig_.tick( h, ag, ant, gm, hg, tq, pool_ag_, pool_ant_ );
      t_ += h;
      tq_ = tq;
      after_tick( h );
    }
  }

  BodyState body() const override
  {
    BodyState b;
    const double th = rig_.joint.theta, L = 0.30;
    b.points.push_back( { L * std::cos( th ), L * std::sin( th ) } );  // hand
    b.points.push_back( { 0.0, 0.0 } );                               // elbow
    b.points.push_back( { -0.12, -0.28 } );                           // shoulder
    b.heading = th;
    auto& r = b.readouts;
    r[ "t_ms" ] = t_;
    r[ "theta_rad" ] = th;
    r[ "theta_deg" ] = th * 180.0 / kPi;
    r[ "target_rad" ] = target_rad();
    r[ "has_target" ] = has_target() ? 1.0 : 0.0;  // 0: no angle target at the moment, the view must not print "off by ..."
    r[ "ag_force_N" ] = rig_.joint.agonist.force_N;
    r[ "ant_force_N" ] = rig_.joint.antagonist.force_N;
    r[ "ag_Ia_hz" ] = rig_.joint.ag_aff.ia;
    r[ "ant_Ia_hz" ] = rig_.joint.ant_aff.ia;
    r[ "ag_II_hz" ] = rig_.joint.ag_aff.ii;
    r[ "ant_II_hz" ] = rig_.joint.ant_aff.ii;
    r[ "ag_Ib_hz" ] = rig_.joint.ag_aff.ib;
    r[ "ant_Ib_hz" ] = rig_.joint.ant_aff.ib;
    r[ "ag_emg_hz" ] = rig_.emg_ag;
    r[ "ant_emg_hz" ] = rig_.emg_ant;
    r[ "ext_torque_Nm" ] = tq_;
    r[ "gamma" ] = rig_.gam;
    extra_readouts( r );
    return b;
  }

  std::array< double, 4 > world_bounds() const override { return { -0.45, -0.40, 0.45, 0.45 }; }

  /// Every spinal sensor is a dc source whose value the task writes each slice (receptor rate in Hz, or a cue level); the weight
  /// turns it into a current. Only the background "noise" of the Renshaw task is a Poisson train (value = Hz). Motors are rates.
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    std::map< std::string, ChannelKind > m;
    for ( const auto& s : sensor_channels() )
    {
      ChannelKind k;
      k.kind = s == "noise" ? "poisson" : "dc";
      k.value = s == "noise" ? 400 : ( s == "drive" ? 1 : ( s == "go" || s == "target" ) ? 0 : 40 );
      m[ s ] = k;
    }
    for ( const auto& s : motor_channels() )
    {
      ChannelKind k;
      k.kind = "rate";
      m[ s ] = k;
    }
    return m;
  }

protected:
  /// Mechanism criteria of the level, listed in the order they speak (see mech_b.h limb_* helpers). Default: none.
  virtual void mechanism( const mechb::Graph&, std::vector< mechb::Issue >& ) const {}
  /// The first mechanism criterion that fails (nullptr when the circuit has them all).
  const mechb::Issue* mech_bad() const
  {
    for ( const auto& m : mech_ )
      if ( !m.ok )
        return &m;
    return nullptr;
  }
  /// Hint ladder of a missing mechanism: tier 1 names the concept, tier 2 the place, tier 3 the direction (words only). False when nothing is missing.
  bool mech_hints( std::vector< Task::Hint >& h, const std::string& concept_text ) const
  {
    const auto* m = mech_bad();
    if ( !m )
      return false;
    h.push_back( { 1, concept_text, "" } );
    h.push_back( { 2, m->place, m->target } );
    h.push_back( { 3, m->direction, m->target } );
    return true;
  }
  virtual double torque( double t_ms ) const = 0;
  virtual void cues( Engine&, double /*t_ms*/ ) {}
  /// Is the model time one in which the inhibitory interneurons are expected to fire on their own (a brief volley at a tap is not a steady stretch)? Default: always.
  virtual bool watch_inhib_now() const { return true; }
  virtual void after_tick( double /*h*/ ) {}
  /// Angle the limb is asked to hold (rad). Meaningful only while has_target(); otherwise it follows the limb, so a view that prints
  /// "off by theta - target" shows 0 rather than the angle itself.
  virtual double target_rad() const { return rig_.joint.theta; }
  virtual bool has_target() const { return false; }
  virtual void extra_readouts( std::map< std::string, double >& ) const {}

  void put( Engine& e, const char* ch, double v )
  {
    if ( inputs_.count( ch ) )
      e.set_input( ch, std::max( 0.0, v ) );
  }
  void put_cue( Engine& e, const char* ch, double v )
  {
    if ( inputs_.count( ch ) )
      e.set_input( ch, v );
  }
  void base_reset( const LimbRigConfig& c, double theta0 )
  {
    rig_.reset( c, theta0 );
    t_ = 0;
    tq_ = 0;
    spikes_ag_ = spikes_ant_ = 0;
    cnt_ag_.clear();
    cnt_ant_.clear();
    motor_.invalidate();
    wiring_.invalidate();
    probed_inputs_ = false;
    gamma_present_ = false;
    inputs_.clear();
    mech_.clear();
    graph_.clear();
    inhib_.reset();
  }

  LimbRig rig_;
  MotorReader motor_;
  Wiring wiring_;
  std::set< std::string > inputs_;
  mechb::Graph graph_;
  SourceWatch inhib_;   // the cells that inhibit the motor pools (reciprocal Ia inhibition, Renshaw cells)
  std::vector< mechb::Issue > mech_;
  bool probed_inputs_ = false, gamma_present_ = false;
  double t_ = 0, tq_ = 0, spikes_ag_ = 0, spikes_ant_ = 0, pool_ag_ = 1, pool_ant_ = 1;
  std::vector< double > cnt_ag_, cnt_ant_;  // per-tick pool spike counts
};

/// PSTH latency of a motor pool to events at `onsets` (ms): the pool's per-ms spike counts are averaged over the events (aligned at
/// their onsets) in 2 ms bins. Baseline = mean and sd of the 100 ms before the events; the response starts at the first bin (within
/// search_ms) whose count exceeds baseline + max(4 sqrt(baseline / events), 1.2 spikes), looking from 8 ms on and needing a second such bin within 4 ms (or a single bin twice as far above the baseline) and, in the 8 ms from the onset, spikes above baseline from at least 20 % of the pool's cells, a response that at least two thirds of the events reproduce (15 % of the cells each); the latency is the first bin's start. The baseline is the 40 ms before the events. -1 when the pool never responds.
inline double psth_latency( const std::vector< double >& cnt, const std::vector< double >& onsets, double n_pool, double search_ms = 120, int min_lat_ms = 8,
                            double* peak_gain = nullptr )
{
  const int pre = 40, post = static_cast< int >( search_ms ), bin = 2;
  const int nb_pre = pre / bin, nb_post = post / bin;
  std::vector< double > avg( nb_pre + nb_post, 0.0 );
  int used = 0;
  for ( double t0 : onsets )
  {
    const long o = static_cast< long >( std::floor( t0 ) );
    if ( o - pre < 0 || o + post > static_cast< long >( cnt.size() ) )
      continue;
    for ( int i = 0; i < nb_pre + nb_post; ++i )
      for ( int j = 0; j < bin; ++j )
        avg[ i ] += cnt[ o - pre + i * bin + j ];
    ++used;
  }
  if ( used == 0 )
    return -1;
  for ( auto& v : avg )
    v /= used;
  double mu = 0, sd = 0;
  for ( int i = 0; i < nb_pre; ++i )
    mu += avg[ i ];
  mu /= nb_pre;
  for ( int i = 0; i < nb_pre; ++i )
    sd += ( avg[ i ] - mu ) * ( avg[ i ] - mu );
  sd = std::sqrt( sd / nb_pre );
  const double thr = mu + std::max( 4 * std::sqrt( mu / used ), 1.2 );  // shot-noise criterion: the baseline of a pool can be structured (tremor), its sd is only printed
  double peak = 0;
  for ( int i = nb_pre; i < nb_pre + nb_post; ++i )
    peak = std::max( peak, avg[ i ] );
  if ( peak_gain )
    *peak_gain = mu > 1e-9 ? peak / mu : peak;
  for ( int i = nb_pre + min_lat_ms / bin; i + 2 < nb_pre + nb_post; ++i )
    if ( avg[ i ] > thr && ( avg[ i + 1 ] > thr || avg[ i + 2 ] > thr || avg[ i ] > mu + 2 * ( thr - mu ) ) )  // a lone bin counts only if it is twice as high as the criterion
    {
      // a reflex recruits a good part of the pool: the spikes above baseline in the 8 ms from the onset must reach 20 % of the cells
      double excess = 0;
      for ( int j = i; j < i + 4 && j < nb_pre + nb_post; ++j )
        excess += std::max( 0.0, avg[ j ] - mu );
      // ... and it is reproducible: at least two thirds of the taps recruit 15 % of the cells in those 8 ms
      int consistent = 0;
      for ( double t0 : onsets )
      {
        const long o = static_cast< long >( std::floor( t0 ) );
        if ( o - pre < 0 || o + post > static_cast< long >( cnt.size() ) )
          continue;
        double sum = 0;
        for ( int j = 0; j < 4 * bin; ++j )
          sum += cnt[ o - pre + i * bin + j ];
        if ( sum >= 0.15 * n_pool )
          ++consistent;
      }
      if ( excess >= 0.2 * n_pool && 3 * consistent >= 2 * used )
        return ( i - nb_pre ) * bin;
    }
  return -1;
}

/// Sustained-oscillation test on the angle samples th[q0, q1): peak-to-peak and the number of reversals (each swing of more than
/// 0.01 rad counts). Clonus = p2p > 0.08 rad with >= 6 reversals.
inline bool oscillating( const std::vector< double >& th, long q0, long q1, double& p2p )
{
  q1 = std::min< long >( q1, th.size() );
  if ( q1 - q0 < 10 )
  {
    p2p = 0;
    return false;
  }
  double lo = 1e9, hi = -1e9;
  for ( long i = q0; i < q1; ++i )
    lo = std::min( lo, th[ i ] ), hi = std::max( hi, th[ i ] );
  p2p = hi - lo;
  int rev = 0, dir = 0;
  double last_ext = th[ q0 ];
  for ( long i = q0; i < q1; ++i )
  {
    const double v = th[ i ];
    if ( dir >= 0 && v < last_ext - 0.01 ) { if ( dir > 0 ) ++rev; dir = -1; last_ext = v; }
    else if ( dir <= 0 && v > last_ext + 0.01 ) { if ( dir < 0 ) ++rev; dir = 1; last_ext = v; }
    else if ( dir > 0 ) last_ext = std::max( last_ext, v );
    else if ( dir < 0 ) last_ext = std::min( last_ext, v );
  }
  return p2p > 0.08 && rev >= 6;
}

/// Little helpers for window statistics over per-tick sample vectors (1 sample per ms).
inline double window_mean( const std::vector< double >& x, double t0, double t1 )
{
  long a = std::max( 0L, static_cast< long >( std::floor( t0 ) ) ), b = std::min< long >( x.size(), static_cast< long >( std::floor( t1 ) ) );
  if ( b <= a )
    return 0;
  double s = 0;
  for ( long i = a; i < b; ++i )
    s += x[ i ];
  return s / ( b - a );
}
inline double window_sd( const std::vector< double >& x, double t0, double t1 )
{
  long a = std::max( 0L, static_cast< long >( std::floor( t0 ) ) ), b = std::min< long >( x.size(), static_cast< long >( std::floor( t1 ) ) );
  if ( b - a < 2 )
    return 0;
  double m = window_mean( x, t0, t1 ), s = 0;
  for ( long i = a; i < b; ++i )
    s += ( x[ i ] - m ) * ( x[ i ] - m );
  return std::sqrt( s / ( b - a ) );
}

} // namespace spike::tasks
