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
// Biology: lamprey spinal locomotor CPG (Grillner 2003 Nat Rev Neurosci; Buchanan 2001), swim frequency 0.3-10 Hz
// (Wallen & Williams 1984), anti-phase left/right bursts from reciprocal commissural inhibition, one cycle of wave per
// body length. Thrust is inspired by Lighthill M.J. (1960) J Fluid Mech 9:305, but simplified to a toy.
#include <cmath>
#include <iomanip>
#include <sstream>
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
  std::vector< std::string > sensor_channels() const override { return { "drive" }; }
  std::vector< std::string > motor_channels() const override { return { "left_motor", "right_motor" }; }
  double duration_ms() const override { return kDuration; }

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
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
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
    s.passed = done && a.valid && f_ok > 0 && std::fabs( a.phase - 180.0 ) <= 40.0 && a.speed >= 0.25;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( spikes_l_ < 1 && spikes_r_ < 1 )
      o << "Both motor channels are silent: neither left_motor nor right_motor has fired" << yet
        << " Check that drive reaches your neurons and that the weights are strong enough.";
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
      o << "Rhythm at " << a.f << " Hz is outside the 1-10 Hz swimming range.";
    else if ( std::fabs( a.phase - 180.0 ) > 40.0 )
      o << "Left and right are " << static_cast< int >( a.phase ) << " degrees apart, not alternating (180).";
    else if ( a.speed < 0.25 )
      o << "Alternating at " << a.f << " Hz but only " << a.speed << " BL/s: too slow.";
    else
      o << ( done ? "Swimming" : "Swimming so far" ) << " at " << a.f << " Hz, " << static_cast< int >( a.phase ) << " degrees L/R, "
        << a.speed << " BL/s.";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { -2, -4, 18, 4 }; }

private:
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
  double spikes_l_ = 0, spikes_r_ = 0;
};

} // namespace

std::unique_ptr< Task > make_lamprey() { return std::make_unique< LampreyTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( lamprey, 30, "lamprey_cpg.json", make_lamprey ) }
