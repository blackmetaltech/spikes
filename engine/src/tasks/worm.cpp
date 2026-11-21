// Task "worm": C. elegans chemotaxis on a salt plate, with a head sensor made of one ON and one OFF derivative cell.
//
// WORLD (deterministic given the seed; mm, s, ms of model time)
//   Plate: disc, radius R = 10 mm, centre (0,0). Walls: the worm cannot leave (position is projected back to the rim).
//   Salt field (relative concentration, peak = 1):  c(x,y) = exp( -|p - p_peak|^2 / (2 sigma^2) ),  sigma = 2.5 mm,
//   p_peak at radius U(0,0.3 R) from the centre. field() returns c.
//   Worm: constant forward speed 0.3 mm/s (typical crawling speed 0.1-0.3 mm/s). Start at distance 0.55 R = 5.5 mm
//   from the peak (angle from the seeded RNG, resampled until the start lies inside 0.9 R), heading pointing AWAY
//   from the peak +- 0.5 rad (so a worm that never turns fails). Heading noise: dpsi = 0.15 rad/sqrt(s) * dW.
//   Body drawn as 12 points over 1 mm following the path of the head.
//   Head sensor 0.3 mm ahead of the body centre. s(t) = d ln c / dt at the head, low-passed with tau = 200 ms
//   (log: Weber-like, ASE cells respond to relative changes, Suzuki et al. 2008 Nature 454:114).
//     ASEL (ON : fires when c rises)   rate = 5 + 400 * clamp(  s / 0.05 /s, 0, 1 )  Hz
//     ASER (OFF: fires when c falls)   rate = 5 + 400 * clamp( -s / 0.05 /s, 0, 1 )  Hz
//   i.e. a 5 Hz background plus a response that saturates at |d ln c/dt| = 0.05 per second.
//   COMMON CHATTER (the reason one cell is not enough). The plate is stirred: from time to time BOTH cells fire extra
//   spikes together, in bursts about a second long. Chatter c(t) = 600 Hz * clamp(x, 0, 1.5) / 1.5, x an
//   Ornstein-Uhlenbeck process (unit variance, tau = 1 s, seeded). It is added equally to ASEL and ASER:
//     ASEL rate = 5 + c(t) + 400 * clamp(  s / 0.05 /s, 0, 1 ) Hz,   ASER rate = 5 + c(t) + 400 * clamp( -s / 0.05 /s, 0, 1 ) Hz
//   Chatter carries no information. Only the DIFFERENCE ASER - ASEL says "worse" (positive) or "better" (negative). A
//   circuit that listens to one cell alone is thrown off by the chatter; one that subtracts the two is not.
//   Motors "turn_left", "turn_right": mean Hz per neuron, low-passed with tau = 150 ms;
//     omega = 1.5 rad/s * clamp( (rate_left - rate_right) / 40 Hz, -1, 1 )   (positive = counter-clockwise = left)
//
// SCORING
//   t_reach  : first time the head-centre is within 0.1 R = 1 mm of the peak (else not reached)
//   mean_dist: mean distance to the peak over the episode (mm)
//   value    : 100 * ( 0.6 * (reached ? 1 - t_reach/duration : 0) + 0.4 * clamp(1 - mean_dist / start_dist, 0, 1) )
//   pass     : reached the 1 mm disc within 45 s (the episode runs 120 s so the score can keep counting mean distance)
//   Verdicts say plainly when a sensor is not wired, when no motor ever fired (the worm just walks straight), what the
//   closest approach was and, when it got there too late, how late.
//   Reference circuit: 33-37 s. Best one-cell-per-side circuits seen: 42-110 s depending on seed, never within 45 s on all.
//
// MECHANISM (Engine::structure(), checked at the first slice): the ON/OFF difference. Both salt cells must reach the turning command with OPPOSITE
// effects on the turning speed (turn_left minus turn_right): one pushes it (excitation onto turn_left, or onto turn_right through an inhibitory
// cell...), the other holds it back (inhibition through a cell set inhibitory, or excitation onto the opposite turn pool). A circuit that listens to one
// salt cell, or to both the same way, fails with a verdict naming the missing difference, however well its weights happen to steer. Verdicts and hint
// tiers 1-2 carry no numbers; recipe() has them.
//
// Biology: ASEL/ASER on/off salt sensing (Suzuki et al. 2008; Thiele, Faumont, Lockery 2009 J Neurosci); klinotaxis
// and pirouettes (Pierce-Shimomura, Morse, Lockery 1999 J Neurosci; Iino & Yoshida 2009 J Neurosci); AIY promotes
// runs, turning pathway via AIB/RIA/SMB (Gray, Hill, Bargmann 2005 PNAS; Kunitomo et al. 2013 Nat Commun).
#include <cmath>
#include <deque>
#include <iomanip>
#include <sstream>
#include "micro_a.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr double kR = 10.0, kSigma = 2.5, kSpeed = 0.3, kDuration = 120000;
constexpr double kHeadOffset = 0.3, kBodyLen = 1.0, kReach = 0.1 * kR;
constexpr double kSensTau = 200, kSensSat = 0.05, kBase = 5, kGain = 400;
constexpr double kOmegaMax = 1.5, kRref = 40, kMotorTau = 150, kNoise = 0.15;
constexpr double kTurbTau = 1000;  // ms, correlation time of the common chatter
constexpr double kTurb = 600;      // Hz, peak extra rate of the common chatter
constexpr double kPassTime = 45000;  // ms, the pass bar

class WormTask : public Task
{
public:
  WormTask() { reset( 1 ); }
  std::string id() const override { return "worm"; }
  std::string title() const override { return "Worm chemotaxis"; }
  std::string brief() const override
  {
    return "A worm crawls at constant speed across a plate with a hill of salt it loves, and starts out facing away "
           "from it. Two sensor cells sit in its head: ASEL (ON) fires when the salt level is rising, which means "
           "things are getting better; ASER (OFF) fires when it is falling, which means worse. Two motor pools steer "
           "it: the turning speed is (turn_left rate minus turn_right rate), so turn_left spins it counter-clockwise "
           "and turn_right clockwise. Turning one way is enough, so you may leave the other turn channel unconnected: a worm that keeps turning eventually points "
           "uphill. The rule to build: turn while things get worse, go straight while they get better. Catch: the plate "
           "is stirred, and now and then BOTH cells chatter together for a second or so. That chatter means nothing; "
           "only ASER minus ASEL does. So wire ASER to excite turn_left and ASEL to inhibit it (inhibition does the "
           "subtraction). Pass: get within 10 % of the plate radius of the peak within 45 s "
           "(one cell listening to ASER alone is too slow and gets thrown off by the chatter). Score: how fast, and "
           "how close you stay on average.";
  }
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A recipe that works (8 integrate-and-fire cells in every group; weights in pA): ASER onto a group A and ASEL onto a group B "
           "with a sensor weight of about 1000 (each sensor cell then fires once per input spike); A onto the turn_left group with +1000; "
           "B set inhibitory (E / I) and onto turn_left with -1000; turn_right can be left unconnected (one turning direction is enough). With 4 cells per group the worm reaches the peak on some seeds only.";
  }
  std::vector< std::string > sensor_channels() const override { return { "ASEL", "ASER" }; }
  std::vector< std::string > motor_channels() const override { return { "turn_left", "turn_right" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    return { { "ASEL", in_kind( "poisson", 5, 1000 ) }, { "ASER", in_kind( "poisson", 5, 1000 ) } };
  }
  double duration_ms() const override { return kDuration; }
  /// The pass rule only asks whether the head got within 1 mm of the peak by the bar (45 s standard, 56 s guided, 40.5 s researcher): nothing after
  /// the bar can change `passed`, so CHECK stops there. (The value and mean_dist_mm of a quick run cover the shorter episode.)
  double quick_duration_ms() const override { return pass_time(); }
  int par_neurons() const override { return variant_par( variant_, 28 ); }  // A 8 + B 8 + TL 8 + idle 4 of the recipe

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }  // the micros teach inhibition and subtraction; this level uses them
  std::vector< std::string > concepts_used() const override { return { "excitation", "inhibition", "subtraction" }; }
  bool on_spine() const override { return true; }
  /// guided: chatter x0.6, pass time 56 s; researcher: x1.25, pass time 40.5 s (reference 33-37 s, seeds 1-5).
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 14 instead of 28" },
             { "noise_x2", "twice as much common salt chatter (up to 1200 Hz on both cells)" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    const bool silent = s.parts[ "spikes_turn_left" ] < 1 && s.parts[ "spikes_turn_right" ] < 1;
    std::vector< Hint > h;
    if ( silent )
    {
      h.push_back( make_hint( 1, "A sensor that only counts spikes cannot steer: the worm needs a motor pool that turns it.", "" ) );
      h.push_back( make_hint( 2, "Neither turn channel ever fired: connect the sensors through a cell that drives it.", "turn_left" ) );
      h.push_back( make_hint( 3, "ASER (concentration falling) onto a group of cells that drives turn_left; ASEL (rising) onto an inhibitory group that silences it, with links strong enough that one sensor spike moves the cell.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "Both salt cells fire together in the stirring chatter. Only the difference carries the message: use inhibition to subtract ASEL from ASER.", "" ) );
      h.push_back( make_hint( 2, "The turning command listens to one sensor only. Let the ON cell inhibit it.", "ASEL" ) );
      h.push_back( make_hint( 3, "ASER > A > turn_left (excitatory), ASEL > B (inhibitory) > the same turn pool. Turn only when ASER exceeds ASEL: the two links must be about equally strong, so that the chatter they share cancels.", "turn_left" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override
  {
    Score s = score();
    return { { "mean_dist_mm", make_metric( s.parts[ "mean_dist_mm" ], "mm", true ) } };
  }
  Note notebook() const override
  {
    return { "ON minus OFF",
             "C. elegans senses salt with two neurons, ASEL and ASER. ASEL is excited by a rise in salt and ASER by a fall; "
             "the worm steers by comparing their activity, which cancels what they share.",
             "Suzuki H, Thiele TR, Faumont S, Ezcurra M, Lockery SR, Schafer WR (2008) Nature 454:114-117; "
             "Thiele TR, Faumont S, Lockery SR (2009) J Neurosci 29:11904-11911" };
  }

  void reset( uint64_t seed ) override
  {
    Rng rng( seed ^ 0x776F726DULL );
    double pr = rng.uniform( 0, 0.3 * kR ), pa = rng.uniform( 0, 2 * kPi );
    peak_ = { pr * std::cos( pa ), pr * std::sin( pa ) };
    double sx = 0, sy = 0;
    for ( int tries = 0; tries < 100; ++tries )
    {
      double a = rng.uniform( 0, 2 * kPi );
      sx = peak_[ 0 ] + 0.55 * kR * std::cos( a );
      sy = peak_[ 1 ] + 0.55 * kR * std::sin( a );
      if ( std::hypot( sx, sy ) < 0.9 * kR )
        break;
    }
    pos_ = { sx, sy };
    double to_peak = std::atan2( peak_[ 1 ] - sy, peak_[ 0 ] - sx );
    psi_ = to_peak + kPi + rng.uniform( -0.5, 0.5 );
    noise_rng_ = Rng( seed ^ 0x6E6F697365ULL );
    turb_rng_ = Rng( seed ^ 0x7475726260ULL );
    turb_x_ = 0;
    turb_ = 0;
    start_dist_ = std::hypot( sx - peak_[ 0 ], sy - peak_[ 1 ] );
    t_ = 0;
    trail_.clear();
    trail_.push_back( pos_ );
    t_reach_ = -1;
    dist_sum_ = 0;
    nsteps_ = 0;
    turn_src_.reset();
    ml_.tau_ms = mr_.tau_ms = kMotorTau;
    ml_.reset( 0 );
    mr_.reset( 0 );
    sens_.tau_ms = kSensTau;
    sens_.reset( 0 );
    prev_lnc_ = ln_c_head();
    omega_ = 0;
    motor_.invalidate();
    wiring_.invalidate();
    spikes_l_ = spikes_r_ = 0;
    inspected_ = false;
    mech_.clear();
    min_dist_ = start_dist_;
    rate_l_ = rate_r_ = kBase;
  }

  void step( Engine& e, double dt ) override
  {
    if ( !inspected_ )
    {
      // Turning one way is enough (the brief says so): one wired turn channel satisfies the wiring check, the other may stay unconnected (it then never fires).
      std::vector< std::string > motors = motor_channels();
      int wired = 0;
      for ( const auto& c : e.outputs() )
        if ( ( c.name == "turn_left" || c.name == "turn_right" ) && !c.neurons.empty() ) ++wired;
      if ( wired >= 1 ) motors.clear();
      wiring_.probe( e, sensor_channels(), motors );
    }
    if ( !inspected_ )
    {
      inspected_ = true;
      inspect( e );
      micro::Graph gs( e.structure() );
      std::set< int > tp;
      for ( const char* ch : { "turn_left", "turn_right" } ) for ( int p : gs.out_pops( ch ) ) tp.insert( p );
      turn_src_.arm( gs.st, tp, 0 );
    }
    e.set_input( "ASEL", rate_l_ );
    e.set_input( "ASER", rate_r_ );
    e.step( dt );
    turn_src_.tick( e );
    double l = motor_.hz_per_neuron( e, "turn_left" ), r = motor_.hz_per_neuron( e, "turn_right" );
    spikes_l_ += motor_.spike_count( e, "turn_left", dt );
    spikes_r_ += motor_.spike_count( e, "turn_right", dt );
    int n = std::max( 1, static_cast< int >( std::ceil( dt - 1e-9 ) ) );
    double h = dt / n;
    for ( int i = 0; i < n; ++i )
      tick( l, r, h );
  }

  BodyState body() const override
  {
    BodyState b;
    // follow-the-leader: walk back along the recorded head path
    double need = kBodyLen / 11.0, acc = 0;
    std::array< double, 2 > cur = pos_;
    b.points.push_back( cur );
    for ( size_t k = trail_.size(); k-- > 0 && b.points.size() < 12; )
    {
      std::array< double, 2 > q = trail_[ k ];
      double d = std::hypot( q[ 0 ] - cur[ 0 ], q[ 1 ] - cur[ 1 ] );
      while ( d > 1e-12 && acc + d >= need && b.points.size() < 12 )
      {
        double f = ( need - acc ) / d;
        cur = { cur[ 0 ] + f * ( q[ 0 ] - cur[ 0 ] ), cur[ 1 ] + f * ( q[ 1 ] - cur[ 1 ] ) };
        b.points.push_back( cur );
        d = std::hypot( q[ 0 ] - cur[ 0 ], q[ 1 ] - cur[ 1 ] );
        acc = 0;
      }
      acc += d;
      cur = q;
    }
    while ( b.points.size() < 12 )  // still at the start: straight tail behind the heading
    {
      auto last = b.points.back();
      b.points.push_back( { last[ 0 ] - need * std::cos( psi_ ), last[ 1 ] - need * std::sin( psi_ ) } );
    }
    b.heading = psi_;
    b.readouts[ "conc" ] = field( pos_[ 0 ], pos_[ 1 ] );
    b.readouts[ "dlnc_dt" ] = sens_.y;
    b.readouts[ "ASEL_hz" ] = rate_l_;
    b.readouts[ "ASER_hz" ] = rate_r_;
    b.readouts[ "omega" ] = omega_;
    b.readouts[ "dist_mm" ] = std::hypot( pos_[ 0 ] - peak_[ 0 ], pos_[ 1 ] - peak_[ 1 ] );
    b.readouts[ "peak_x" ] = peak_[ 0 ];
    b.readouts[ "peak_y" ] = peak_[ 1 ];
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    double mean_d = nsteps_ ? dist_sum_ / nsteps_ : start_dist_;
    bool reached = t_reach_ >= 0;
    s.parts[ "t_reach_s" ] = reached ? t_reach_ / 1000.0 : -1.0;
    s.parts[ "mean_dist_mm" ] = mean_d;
    s.parts[ "final_dist_mm" ] = std::hypot( pos_[ 0 ] - peak_[ 0 ], pos_[ 1 ] - peak_[ 1 ] );
    s.parts[ "start_dist_mm" ] = start_dist_;
    s.parts[ "closest_mm" ] = min_dist_;
    s.parts[ "spikes_turn_left" ] = spikes_l_;
    s.parts[ "spikes_turn_right" ] = spikes_r_;
    s.value = 100.0 * ( 0.6 * ( reached ? 1.0 - t_reach_ / kDuration : 0.0 ) + 0.4 * clampd( 1.0 - mean_d / start_dist_, 0, 1 ) );
    s.passed = reached && t_reach_ <= pass_time() && mech_.empty();
    if ( !mech_.empty() ) s.value = std::min( s.value, 40.0 );
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( s.passed )
      o << "Reached the salt peak in time; the worm turns on ASER minus ASEL and the shared chatter cancels.";
    else if ( reached )
      o << "Reached the peak, but too late: steer faster. Turn on ASER minus ASEL so the chatter does not fool you, and make the turning command stronger.";
    else if ( t_ < pass_time() )
      o << "Still searching.";
    else if ( spikes_l_ < 1 && spikes_r_ < 1 )
      o << "Neither turn_left nor turn_right ever fired, so the worm walks straight and leaves. "
           "Check ASEL/ASER reach a cell that drives a motor pool, and that the weights are strong enough.";
    else if ( turn_src_.armed() && turn_src_.some_silent() )
      o << "The worm did not get to the peak in time. " << turn_src_.say( "cells that carry the salt signal to the turning command" )
        << " (the salt cells' own weights decide whether they fire: strengthen the sensors, or give the cells a bias current).";
    else
      o << "The worm did not get to the peak in time. Turn when "
           "ASER fires and ASEL does not; stay straight when ASEL fires.";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { -kR, -kR, kR, kR }; }
  double field( double x, double y ) const override
  {
    double dx = x - peak_[ 0 ], dy = y - peak_[ 1 ];
    return std::exp( -( dx * dx + dy * dy ) / ( 2 * kSigma * kSigma ) );
  }

private:
  /// The ON/OFF difference (docs/REQ-0.4.md rule 1): the two salt cells must act on the turning speed (turn_left - turn_right) in opposite ways.
  void inspect( Engine& e )
  {
    micro::Graph g( e.structure() );
    const auto left = g.out_pops( "turn_left" ), right = g.out_pops( "turn_right" );
    const std::vector< int > sa[ 2 ] = { g.in_pops( "ASEL" ), g.in_pops( "ASER" ) };
    if ( left.empty() && right.empty() ) return;
    bool pushes[ 2 ] = { false, false }, holds[ 2 ] = { false, false };
    for ( int k = 0; k < 2; ++k )
    {
      if ( sa[ k ].empty() ) continue;
      // effect on omega = rate_left - rate_right: positive through (left, net +) or (right, net -)
      pushes[ k ] = g.path( sa[ k ], left, +1, 4, true ) || g.path( sa[ k ], right, -1, 4 );
      holds[ k ] = g.path( sa[ k ], left, -1, 4 ) || g.path( sa[ k ], right, +1, 4, true );
    }
    const bool reaches = pushes[ 0 ] || holds[ 0 ] || pushes[ 1 ] || holds[ 1 ];
    const bool diff = ( pushes[ 0 ] && holds[ 1 ] ) || ( pushes[ 1 ] && holds[ 0 ] );
    if ( !reaches )
      mech_ = "Neither salt cell reaches the turning command: nothing connects ASEL or ASER to the turn pools.";
    else if ( !diff )
      mech_ = "The turning command does not take the DIFFERENCE of the two salt cells: only one of them reaches it, or both push it the same way. The chatter "
              "hits both cells together, so only 'ASER minus ASEL' is a signal: let one salt cell drive the turn and the other hold it back (inhibition "
              "does the subtraction).";
  }

  double pass_time() const { return tol_max( mode_, kPassTime, 1.25, 0.9 ); }
  double ln_c_head() const
  {
    double hx = pos_[ 0 ] + kHeadOffset * std::cos( psi_ ), hy = pos_[ 1 ] + kHeadOffset * std::sin( psi_ );
    double dx = hx - peak_[ 0 ], dy = hy - peak_[ 1 ];
    return -( dx * dx + dy * dy ) / ( 2 * kSigma * kSigma );
  }

  void tick( double l_hz, double r_hz, double h )
  {
    double ml = ml_.step( l_hz, h ), mr = mr_.step( r_hz, h );
    omega_ = kOmegaMax * clampd( ( ml - mr ) / kRref, -1, 1 );
    double dts = h / 1000.0;
    psi_ += omega_ * dts + kNoise * std::sqrt( dts ) * noise_rng_.normal();
    pos_[ 0 ] += kSpeed * std::cos( psi_ ) * dts;
    pos_[ 1 ] += kSpeed * std::sin( psi_ ) * dts;
    double rr = std::hypot( pos_[ 0 ], pos_[ 1 ] );
    if ( rr > kR )
    {
      pos_[ 0 ] *= kR / rr;
      pos_[ 1 ] *= kR / rr;
    }
    t_ += h;
    // sensing
    double lnc = ln_c_head();
    sens_.step( ( lnc - prev_lnc_ ) / dts, h );
    prev_lnc_ = lnc;
    // common chatter: an Ornstein-Uhlenbeck process (unit variance), only its positive part, raises BOTH cells together
    turb_x_ += -turb_x_ * ( h / kTurbTau ) + std::sqrt( 2.0 * h / kTurbTau ) * turb_rng_.normal();
    turb_ = kTurb * mode_noise( mode_, variant_, 0.6, 1.25 ) * clampd( turb_x_, 0, 1.5 ) / 1.5;
    rate_l_ = kBase + turb_ + kGain * clampd( sens_.y / kSensSat, 0, 1 );
    rate_r_ = kBase + turb_ + kGain * clampd( -sens_.y / kSensSat, 0, 1 );
    // bookkeeping
    double d = std::hypot( pos_[ 0 ] - peak_[ 0 ], pos_[ 1 ] - peak_[ 1 ] );
    dist_sum_ += d;
    ++nsteps_;
    min_dist_ = std::min( min_dist_, d );
    if ( t_reach_ < 0 && d <= kReach )
      t_reach_ = t_;
    auto& back = trail_.back();
    if ( std::hypot( pos_[ 0 ] - back[ 0 ], pos_[ 1 ] - back[ 1 ] ) >= 0.005 )
    {
      trail_.push_back( pos_ );
      if ( trail_.size() > 400 )
        trail_.pop_front();
    }
  }

  std::array< double, 2 > peak_{}, pos_{};
  double psi_ = 0, t_ = 0, start_dist_ = 1, t_reach_ = -1, dist_sum_ = 0, prev_lnc_ = 0, omega_ = 0;
  double rate_l_ = kBase, rate_r_ = kBase;
  long nsteps_ = 0;
  SourceWatch turn_src_;   // the groups that feed the turn pools: a silent one is said in the verdict
  double min_dist_ = 1, spikes_l_ = 0, spikes_r_ = 0;
  Wiring wiring_;
  bool inspected_ = false;
  std::string mech_;
  std::deque< std::array< double, 2 > > trail_;
  Lowpass ml_, mr_, sens_;
  Rng noise_rng_, turb_rng_;
  double turb_x_ = 0, turb_ = 0;
  MotorReader motor_;
};

} // namespace

std::unique_ptr< Task > make_worm() { return std::make_unique< WormTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( worm, 20, "worm_chemotaxis.json", make_worm ) }
