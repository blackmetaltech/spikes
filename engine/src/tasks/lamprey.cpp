// Task "lamprey": a half-centre swimming rhythm generator drives a 10-segment body; score = rhythm + speed.
//
// WORLD (deterministic, no randomness; lengths in body lengths BL, times in ms)
//   Sensor "drive": set every slice to 1.0 (the brainstem / MLR "swim" command at 100 %; the circuit's input weight
//   turns it into current, see Grillner 2003). Motors "left_motor", "right_motor": mean rate per neuron in Hz.
//
//   Muscle command  u(t) = clamp( (rL - rR) / 40 Hz, -1, 1 ),  rL, rR low-passed with tau = 40 ms (muscle activation).
//   Body: 10 segments (11 points, head first), each BL/10 long. Segment i (i = 0..9, head to tail) bends by
//        theta_i(t) = 0.12 rad * u(t - i * 36 ms)
//   i.e. a travelling wave with a fixed intersegmental delay (wave speed c = 0.1 BL / 36 ms = 2.8 BL/s).
//   Segment directions are phi_i = sum_{k<=i} theta_k (relative to the body axis). Lateral offset of the tail point
//   y_tail = sum_i 0.1 * sin(phi_i); v_tail = d y_tail / dt (BL/s).
//
//   Thrust (a toy reactive-thrust model in the spirit of Lighthill 1960, NOT his formula): added-mass momentum flux off the tail,
//        T = 1/2 * m * v_tail^2 * (1 - U/c)+          m = 0.03 (added mass per BL, normalised)
//   Drag and inertia:  M dU/dt = T - k U^2,  M = 1, k = 1, U >= 0 is the forward speed in BL/s.
//   Steering: heading psi changes by  dpsi/dt = 2.0 /s * mean_i(theta_i)  (a left/right bias bends the body).
//   Position integrates U along psi. Arena for drawing: x in [-2, 18], y in [-4, 4].
//
// SCORING over the analysis window t in [2500 ms, 10 000 ms]
//   The analysis works on the motor rates smoothed once more (tau = 40 ms, on top of the muscle low-pass), so spike ripple
//   of a tonically firing cell cannot be mistaken for a rhythm.
//   swing_L, swing_R : peak-to-peak of the smoothed left / right rate (Hz). BOTH sides must modulate:
//           min(swing_L, swing_R) >= 2 Hz and >= 0.3 * max(swing_L, swing_R), else "no rhythm" and no phase is reported
//   freq  : upward zero crossings of (rL - rR) with +-25 % hysteresis (of the half swing), per second; needs >= 3 crossings
//   amp_L, amp_R : amplitude of each side at freq (complex demodulation); both must be >= 1 Hz and >= 0.3 * the larger
//   phase : phase between left and right at freq, folded to [0,180] deg; only defined when both sides are coherent
//   speed : mean U over the window (BL/s)
//   value = 100 * (0.25 * f_ok + 0.35 * phase_ok + 0.40 * min(1, speed/0.8)), where f_ok = 1 if 1 <= f <= 10,
//           phase_ok = max(0, 1 - |phase - 180|/90)  (0 when there is no valid phase)
//   pass  : valid two-sided rhythm, 1 <= f <= 10 Hz, |phase - 180| <= 40 deg, speed >= 0.25 BL/s
//   verdict: says plainly when a motor channel is not wired or never fired (left_motor / right_motor), or both are silent.
//
// MECHANISM (Engine::structure(), checked at the first slice): crossed inhibition. The left motor pool must be able to inhibit the right one and the
// right the left (a path whose net sign is inhibitory, directly or through a commissural interneuron, in at most 3 projections each way). Without it
// the run fails with a verdict naming the missing crossing, whatever the motor rates do. Verdicts and hint tiers 1-2 carry no numbers.
//
// Biology: lamprey spinal locomotor CPG (Grillner 2003 Nat Rev Neurosci; Buchanan 2001), swim frequency 0.3-10 Hz
// (Wallen & Williams 1984), anti-phase left/right bursts from reciprocal commissural inhibition, one cycle of wave per
// body length. Thrust is inspired by Lighthill M.J. (1960) J Fluid Mech 9:305, but simplified to a toy.
#include <cmath>
#include <iomanip>
#include <sstream>
#include "micro_a.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr int kSeg = 10;
constexpr double kDuration = 10000, kSkip = 2500;
constexpr double kThetaMax = 0.12, kSegDelayMs = 36, kSegLen = 0.1, kRref = 40;
constexpr double kAddedMass = 0.03, kInertia = 1.0, kDragK = 1.0, kYaw = 2.0;
constexpr double kWaveSpeed = kSegLen / ( kSegDelayMs / 1000.0 );  // BL/s

class LampreyTask : public Task
{
public:
  LampreyTask() { reset( 1 ); }
  std::string id() const override { return "lamprey"; }
  std::string title() const override { return "Lamprey swimmer"; }
  std::string brief() const override
  {
    return "A lamprey swims by alternating left and right muscle bursts, produced by a half-centre oscillator in its "
           "spinal cord: two groups of neurons that inhibit each other and tire (spike-frequency adaptation) so that "
           "the winner periodically hands over. A steady descending drive switches swimming on. Build left and right "
           "pools with mutual inhibition, give them the drive, and read out left_motor and right_motor. Both sides must "
           "burst (a rhythm on one side only does not count). Score: "
           "swim frequency inside 1-10 Hz, left and right about 180 degrees apart, and forward speed (the body "
           "makes a travelling wave and a simple reactive-thrust model pushes it along).";
  }
  std::string default_model() const override { return "aeif_cond_alpha"; }
  std::string recipe() const override
  {
    return "A recipe that works: two groups of 8 aeif_cond_alpha cells (preset Adapting), each set inhibitory (E / I) and wired to the other "
           "with the default weight (-8 nS), drive (a dc input: the weight is pA of current per unit of drive) onto both with about 600 pA, "
           "each group onto its own motor channel. Below about 550 pA nothing fires; above about 700 the rhythm is lost on some seeds.";
  }
  std::vector< std::string > sensor_channels() const override { return { "drive" }; }
  std::vector< std::string > motor_channels() const override { return { "left_motor", "right_motor" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    return { { "drive", in_kind( "dc", 1, 600 ) } };
  }
  double duration_ms() const override { return kDuration; }
  int par_neurons() const override { return variant_par( variant_, 16 ); }  // L and R: 8 cells each, as in the recipe

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }  // the micros teach adaptation and the half-centre; this level builds the swimmer
  std::vector< std::string > concepts_used() const override { return { "adaptation", "half_centre", "inhibition" }; }
  bool on_spine() const override { return true; }
  /// The world has no noise (deterministic), so the modes move the pass bars only: guided phase within 50 deg and speed >= 0.2 BL/s,
  /// standard 40 deg and 0.25, researcher 34 deg and 0.29 (reference: 180 deg +- a few, 0.8+ BL/s, seeds 1-5).
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 8 instead of 16" },
             { "fast_swim", "a sprint: swim at 0.5 BL/s or more" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    const double sl = s.parts[ "spikes_left" ], sr = s.parts[ "spikes_right" ];
    const double swing = std::min( s.parts[ "swing_left_hz" ], s.parts[ "swing_right_hz" ] );
    std::vector< Hint > h;
    if ( sl < 1 || sr < 1 )
    {
      h.push_back( make_hint( 1, "A swimming rhythm needs both sides to fire in turn. One side is silent.", "" ) );
      h.push_back( make_hint( 2, sl < 1 ? "left_motor never fired." : "right_motor never fired.", sl < 1 ? "left_motor" : "right_motor" ) );
      h.push_back( make_hint( 3, "Give drive to a left and a right pool of adapting cells (a handful each), each driving its own motor channel. If nothing fires, strengthen the drive.", "" ) );
    }
    else if ( swing < 2.0 || std::fabs( s.parts[ "phase_deg" ] - 180.0 ) > 40.0 )
    {
      h.push_back( make_hint( 1, "Half-centre: two pools that inhibit each other take turns, because the winner tires (adaptation) and lets go.", "" ) );
      h.push_back( make_hint( 2, "The sides are not alternating: add or strengthen the inhibitory crossing between them.", "left_motor" ) );
      h.push_back( make_hint( 3, "Left pool inhibits right and right inhibits left, with an adapting cell model so each side tires and lets go. The inhibition must be strong enough to silence the other side, not so strong that nothing is left to fire.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "The rhythm is there; the body needs a stronger, faster command to swim.", "" ) );
      h.push_back( make_hint( 2, "Raise the burst size of both motor pools.", "right_motor" ) );
      h.push_back( make_hint( 3, "More drive to both pools lifts the rate difference that bends the body; stay below the strength at which the rhythm stops.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override { return { { "speed_bl_s", make_metric( score().parts[ "speed" ], "BL/s", false ) } }; }
  Note notebook() const override
  {
    return { "Two half-centres",
             "The lamprey spinal cord makes its swimming rhythm with a left and a right pool of excitatory neurons that "
             "inhibit each other through crossing interneurons; adaptation ends each burst so the sides alternate.",
             "Grillner S (2003) Nat Rev Neurosci 4:573" };
  }

  void reset( uint64_t ) override
  {
    t_ = 0;
    tick_ = 0;
    u_hist_.clear();
    rl_.tau_ms = rr_.tau_ms = 40;
    rl_.reset( 0 );
    rr_.reset( 0 );
    U_ = 0;
    x_ = y_ = psi_ = 0;
    y_tail_prev_ = 0;
    have_prev_ = false;
    dl_.clear();
    dr_.clear();
    sp_.clear();
    theta_.assign( kSeg, 0.0 );
    tail_amp_ = 0;
    motor_.invalidate();
    wiring_.invalidate();
    spikes_l_ = spikes_r_ = 0;
    inspected_ = false;
    mech_.clear();
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    if ( !inspected_ )
    {
      inspected_ = true;
      inspect( e );
      break_symmetry( e );
    }
    e.set_input( "drive", 1.0 );
    e.step( dt );
    double l = motor_.hz_per_neuron( e, "left_motor" ), r = motor_.hz_per_neuron( e, "right_motor" );
    spikes_l_ += motor_.spike_count( e, "left_motor", dt );
    spikes_r_ += motor_.spike_count( e, "right_motor", dt );
    // world ticks of at most 1 ms (a 5 ms call integrates 5 ticks with the same motor rates)
    int n = std::max( 1, static_cast< int >( std::ceil( dt - 1e-9 ) ) );
    double h = dt / n;
    for ( int i = 0; i < n; ++i )
      tick( l, r, h );
  }

  BodyState body() const override
  {
    BodyState b;
    double px = x_, py = y_, a = psi_;
    b.points.push_back( { px, py } );
    // head at the front: draw from head backwards (head first) along the travelling wave
    double cum = 0;
    for ( int i = 0; i < kSeg; ++i )
    {
      cum += theta_[ i ];
      // the body trails behind the head: head points along +heading, each segment continues tail-ward
      px -= kSegLen * std::cos( a + cum );
      py -= kSegLen * std::sin( a + cum );
      b.points.push_back( { px, py } );
    }
    b.heading = psi_;
    b.readouts[ "speed" ] = U_;
    b.readouts[ "u" ] = u_hist_.empty() ? 0.0 : u_hist_.back();
    b.readouts[ "rate_left" ] = rl_.y;
    b.readouts[ "rate_right" ] = rr_.y;
    b.readouts[ "tail_amp" ] = tail_amp_;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    Analysis a = analyse();
    s.parts[ "freq_hz" ] = a.f;
    s.parts[ "phase_deg" ] = a.valid ? a.phase : 0.0;
    s.parts[ "speed" ] = a.speed;
    s.parts[ "swing_left_hz" ] = a.swing_l;
    s.parts[ "swing_right_hz" ] = a.swing_r;
    s.parts[ "spikes_left" ] = spikes_l_;
    s.parts[ "spikes_right" ] = spikes_r_;
    double f_ok = ( a.valid && a.f >= 1.0 && a.f <= 10.0 ) ? 1.0 : 0.0;
    double p_ok = a.valid ? std::max( 0.0, 1.0 - std::fabs( a.phase - 180.0 ) / 90.0 ) : 0.0;
    s.value = 100.0 * ( 0.25 * f_ok + 0.35 * p_ok + 0.40 * std::min( 1.0, a.speed / 0.8 ) );
    bool done = t_ >= kDuration;
    s.passed = done && a.valid && f_ok > 0 && std::fabs( a.phase - 180.0 ) <= phase_tol() && a.speed >= speed_min() && mech_.empty();
    if ( !mech_.empty() ) s.value = std::min( s.value, 40.0 );
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( spikes_l_ < 1 && spikes_r_ < 1 )
      o << "Both motor channels are silent: neither left_motor nor right_motor has fired" << yet
        << " Check that drive reaches your neurons and that the drive weight (current per unit of drive) is over the cells' rheobase: "
           "the adaptive cells need much more than a plain cell.";
    else if ( spikes_l_ < 1 )
      o << "left_motor never fired" << yet << " Both sides must burst in turn; only right_motor is active.";
    else if ( spikes_r_ < 1 )
      o << "right_motor never fired" << yet << " Both sides must burst in turn; only left_motor is active.";
    else if ( !a.valid )
    {
      if ( a.swing_l < 2.0 && a.swing_r < 2.0 )
        o << "No rhythm: neither motor rate rises and falls (it fires steadily or in a blur)" << yet;
      else if ( std::min( a.swing_l, a.swing_r ) < 2.0 || std::min( a.swing_l, a.swing_r ) < 0.3 * std::max( a.swing_l, a.swing_r ) )
        o << "No rhythm: only " << ( a.swing_l > a.swing_r ? "left_motor" : "right_motor" )
          << " rises and falls; the other side stays flat. Both sides must burst in turn" << yet;
      else
        o << "No rhythm: left and right motor rates never alternate cleanly" << yet;
    }
    else if ( f_ok == 0 )
      o << "The rhythm is outside the swimming range (1-10 Hz): " << ( a.f < 1.0 ? "too slow." : "too fast." );
    else if ( std::fabs( a.phase - 180.0 ) > phase_tol() )
      o << "Left and right are not alternating: the bursts of the two sides overlap too much.";
    else if ( a.speed < speed_min() )
      o << "Alternating, but the body goes too slowly: the rate difference between the sides is too small to bend it.";
    else
      o << ( done ? "Swimming" : "Swimming so far" ) << ": the two sides alternate and the body goes forward.";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { -2, -4, 18, 4 }; }

private:
  /// Crossed inhibition (docs/REQ-0.4.md rule 1): left inhibits right and right inhibits left, directly or through a commissural interneuron.
  void inspect( Engine& e )
  {
    micro::Graph g( e.structure() );
    const auto l = g.out_pops( "left_motor" ), r = g.out_pops( "right_motor" );
    if ( l.empty() || r.empty() ) return;
    if ( micro::Graph::overlap( l, r ) )
      mech_ = "left_motor and right_motor are read from the same cells: a swimming rhythm needs a left side and a right side.";
    else if ( !g.crossed( l, r ) && !g.crossed( r, l ) )
      mech_ = "The two sides do not inhibit each other: there is no crossed inhibition. A half-centre is built on it: each side's activity must silence the other.";
    else if ( !g.crossed( l, r ) || !g.crossed( r, l ) )
      mech_ = "The crossed inhibition runs one way only: one side silences the other, which cannot answer back, so nothing hands the burst over. "
              "Each side must inhibit the other.";
  }

  /// The descending command reaches the left side a moment before the right (as in m_half_centre): the cells that drive the left motor pool start just
  /// over threshold (V_m -50 mV) and those that drive the right one at -75 mV. Identical cells (and pools of them) can otherwise start in perfect step,
  /// and a half-centre has no way to choose which side goes first. Only cells that clearly belong to one side are touched.
  void break_symmetry( Engine& e )
  {
    micro::Graph g( e.structure() );
    const auto l = g.out_pops( "left_motor" ), r = g.out_pops( "right_motor" );
    if ( l.empty() || r.empty() || micro::Graph::overlap( l, r ) ) return;
    std::vector< int > side( g.st.populations.size(), 0 );
    for ( size_t p = 0; p < side.size(); ++p )
    {
      const std::vector< int > P = { static_cast< int >( p ) };
      const bool in_l = g.path( P, l, +1, 3, true ), in_r = g.path( P, r, +1, 3, true );
      side[ p ] = in_l && !in_r ? 1 : ( in_r && !in_l ? -1 : 0 );
    }
    for ( int n = 0; n < e.neuron_count(); ++n )
    {
      const std::string id = e.neuron_population( n );
      for ( size_t p = 0; p < side.size(); ++p )
        if ( g.st.populations[ p ].id == id && side[ p ] != 0 ) e.set_neuron_param( n, "V_m", side[ p ] > 0 ? -50.0 : -75.0 );
    }
  }

  void tick( double rl_hz, double rr_hz, double h )
  {
    double l = rl_.step( rl_hz, h ), r = rr_.step( rr_hz, h );
    double u = clampd( ( l - r ) / kRref, -1, 1 );
    u_hist_.push_back( u );
    ++tick_;
    t_ += h;
    // segment bends with delay
    double mean_theta = 0, phi = 0, y_tail = 0;
    for ( int i = 0; i < kSeg; ++i )
    {
      long back = std::lround( i * kSegDelayMs / h );
      long idx = static_cast< long >( u_hist_.size() ) - 1 - back;
      double ui = idx >= 0 ? u_hist_[ idx ] : 0.0;
      theta_[ i ] = kThetaMax * ui;
      mean_theta += theta_[ i ];
      phi += theta_[ i ];
      y_tail += kSegLen * std::sin( phi );
    }
    mean_theta /= kSeg;
    double v_tail = have_prev_ ? ( y_tail - y_tail_prev_ ) / ( h / 1000.0 ) : 0.0;
    y_tail_prev_ = y_tail;
    have_prev_ = true;
    tail_amp_ = std::max( tail_amp_ * 0.9995, std::fabs( y_tail ) );
    double thrust = 0.5 * kAddedMass * v_tail * v_tail * std::max( 0.0, 1.0 - U_ / kWaveSpeed );
    U_ = std::max( 0.0, U_ + ( thrust - kDragK * U_ * U_ ) / kInertia * ( h / 1000.0 ) );
    psi_ += kYaw * mean_theta * ( h / 1000.0 );
    x_ += U_ * std::cos( psi_ ) * ( h / 1000.0 );
    y_ += U_ * std::sin( psi_ ) * ( h / 1000.0 );
    if ( t_ >= kSkip )
    {
      dl_.push_back( l );
      dr_.push_back( r );
      sp_.push_back( U_ );
    }
  }

  double phase_tol() const { return tol_max( mode_, 40.0, 1.25, 0.85 ); }
  double speed_min() const { return variant_ == "fast_swim" ? 0.5 : tol_min( mode_, 0.25, 1.25, 0.85 ); }

  struct Analysis
  {
    double f = 0, phase = 0, speed = 0, swing_l = 0, swing_r = 0, amp_l = 0, amp_r = 0;
    bool valid = false;  // both sides modulate coherently at f
  };

  // Samples are one per world tick (about 1 ms). Extra smoothing (tau 40 ms) removes spike ripple.
  static std::vector< double > smooth( const std::vector< double >& x )
  {
    std::vector< double > y( x.size() );
    double a = 1.0 - std::exp( -1.0 / 40.0 ), v = x.empty() ? 0.0 : x[ 0 ];
    for ( size_t i = 0; i < x.size(); ++i )
      y[ i ] = v += a * ( x[ i ] - v );
    return y;
  }

  Analysis analyse() const
  {
    Analysis r;
    size_t n = dl_.size();
    if ( n < 100 )
      return r;
    std::vector< double > l = smooth( dl_ ), rr = smooth( dr_ );
    double ml = 0, mr = 0, ms = 0;
    for ( size_t i = 0; i < n; ++i )
    {
      ml += l[ i ];
      mr += rr[ i ];
      ms += sp_[ i ];
    }
    ml /= n; mr /= n; r.speed = ms / n;
    size_t skip = std::min< size_t >( n / 10, 200 );  // let the extra filter settle
    double lmax = -1e18, lmin = 1e18, rmax = -1e18, rmin = 1e18, dmax = -1e18, dmin = 1e18;
    for ( size_t i = skip; i < n; ++i )
    {
      double d = ( l[ i ] - ml ) - ( rr[ i ] - mr );
      lmax = std::max( lmax, l[ i ] ); lmin = std::min( lmin, l[ i ] );
      rmax = std::max( rmax, rr[ i ] ); rmin = std::min( rmin, rr[ i ] );
      dmax = std::max( dmax, d ); dmin = std::min( dmin, d );
    }
    r.swing_l = lmax - lmin;
    r.swing_r = rmax - rmin;
    double smax = std::max( r.swing_l, r.swing_r ), smin = std::min( r.swing_l, r.swing_r );
    if ( smin < 2.0 || smin < 0.3 * smax )
      return r;  // one side (or both) does not modulate: no rhythm, no frequency, no phase
    // zero crossings of L - R with 25 % hysteresis of the half swing
    double hy = 0.25 * 0.5 * ( dmax - dmin ), first = -1, last = -1;
    int state = 0, crossings = 0;
    for ( size_t i = skip; i < n; ++i )
    {
      double d = ( l[ i ] - ml ) - ( rr[ i ] - mr );
      if ( d > hy && state <= 0 )
      {
        if ( state == -1 )
        {
          ++crossings;
          if ( first < 0 ) first = i;
          last = i;
        }
        state = 1;
      }
      else if ( d < -hy && state >= 0 )
        state = -1;
    }
    if ( crossings < 3 )
      return r;
    r.f = 1000.0 / ( ( last - first ) / ( crossings - 1 ) );  // samples are ~1 ms
    // amplitude and phase of each side at f (complex demodulation)
    double w = 2 * kPi * r.f / 1000.0, cl = 0, sl = 0, cr = 0, sr = 0;
    size_t m = n - skip;
    for ( size_t i = skip; i < n; ++i )
    {
      double c = std::cos( w * i ), s = std::sin( w * i );
      cl += ( l[ i ] - ml ) * c; sl += ( l[ i ] - ml ) * s;
      cr += ( rr[ i ] - mr ) * c; sr += ( rr[ i ] - mr ) * s;
    }
    r.amp_l = 2.0 * std::hypot( cl, sl ) / m;
    r.amp_r = 2.0 * std::hypot( cr, sr ) / m;
    double amax = std::max( r.amp_l, r.amp_r ), amin = std::min( r.amp_l, r.amp_r );
    if ( amin < 1.0 || amin < 0.3 * amax )
      return r;
    double d = std::atan2( sl, cl ) - std::atan2( sr, cr );
    r.phase = std::fabs( std::remainder( d, 2 * kPi ) ) * 180.0 / kPi;
    r.valid = true;
    return r;
  }

  double t_ = 0, U_ = 0, x_ = 0, y_ = 0, psi_ = 0, y_tail_prev_ = 0, tail_amp_ = 0;
  long tick_ = 0;
  bool have_prev_ = false;
  std::vector< double > u_hist_, theta_, dl_, dr_, sp_;
  Lowpass rl_, rr_;
  MotorReader motor_;
  Wiring wiring_;
  bool inspected_ = false;
  std::string mech_;
  double spikes_l_ = 0, spikes_r_ = 0;
};

} // namespace

std::unique_ptr< Task > make_lamprey() { return std::make_unique< LampreyTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( lamprey, 30, "lamprey_cpg.json", make_lamprey ) }
