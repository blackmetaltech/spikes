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
//   Thrust (simplified Lighthill 1960 reactive model): added-mass momentum flux off the tail,
//        T = 1/2 * m * v_tail^2 * (1 - U/c)+          m = 0.03 (added mass per BL, normalised)
//   Drag and inertia:  M dU/dt = T - k U^2,  M = 1, k = 1, U >= 0 is the forward speed in BL/s.
//   Steering: heading psi changes by  dpsi/dt = 2.0 /s * mean_i(theta_i)  (a left/right bias bends the body).
//   Position integrates U along psi. Arena for drawing: x in [-2, 18], y in [-4, 4].
//
// SCORING over the analysis window t in [2500 ms, 10 000 ms]
//   freq  : upward zero crossings of (rL - rR) with +-10 % hysteresis, per second; 0 if the swing is < 2 Hz
//   phase : phase between the left and right motor rate (complex demodulation at freq), folded to [0,180] deg
//   speed : mean U over the window (BL/s)
//   value = 100 * (0.25 * f_ok + 0.35 * phase_ok + 0.40 * min(1, speed/0.8)), where f_ok = 1 if 1 <= f <= 10,
//           phase_ok = max(0, 1 - |phase - 180|/90)
//   pass  : 1 <= f <= 10 Hz, |phase - 180| <= 40 deg, speed >= 0.25 BL/s
//
// Biology: lamprey spinal locomotor CPG (Grillner 2003 Nat Rev Neurosci; Buchanan 2001), swim frequency 0.3-10 Hz
// (Wallen & Williams 1984), anti-phase left/right bursts from reciprocal commissural inhibition, one cycle of wave per
// body length. Lighthill M.J. (1960) J Fluid Mech 9:305 "Note on the swimming of slender fish".
#include <cmath>
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
           "pools with mutual inhibition, give them the drive, and read out left_motor and right_motor. Score: "
           "swim frequency inside 1-10 Hz, left and right about 180 degrees apart, and forward speed (the body "
           "makes a travelling wave and Lighthill's reactive thrust pushes it along).";
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
  }

  void step( Engine& e, double dt ) override
  {
    e.set_input( "drive", 1.0 );
    e.step( dt );
    double l = motor_.hz_per_neuron( e, "left_motor" ), r = motor_.hz_per_neuron( e, "right_motor" );
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
    double f = 0, ph = 0, speed = 0, swing = 0;
    analyse( f, ph, speed, swing );
    s.parts[ "freq_hz" ] = f;
    s.parts[ "phase_deg" ] = ph;
    s.parts[ "speed" ] = speed;
    s.parts[ "swing_hz" ] = swing;
    double f_ok = ( f >= 1.0 && f <= 10.0 ) ? 1.0 : 0.0;
    double p_ok = f > 0 ? std::max( 0.0, 1.0 - std::fabs( ph - 180.0 ) / 90.0 ) : 0.0;
    s.value = 100.0 * ( 0.25 * f_ok + 0.35 * p_ok + 0.40 * std::min( 1.0, speed / 0.8 ) );
    bool done = t_ >= kDuration;
    s.passed = done && f_ok > 0 && std::fabs( ph - 180.0 ) <= 40.0 && speed >= 0.25;
    std::ostringstream o;
    o.precision( 2 );
    if ( swing < 2.0 )
      o << "No rhythm: left and right motor rates never alternate" << ( done ? "." : " yet." );
    else if ( f_ok == 0 )
      o << "Rhythm at " << f << " Hz is outside the 1-10 Hz swimming range.";
    else if ( std::fabs( ph - 180.0 ) > 40.0 )
      o << "Left and right are " << static_cast< int >( ph ) << " degrees apart, not alternating (180).";
    else if ( speed < 0.25 )
      o << "Alternating at " << f << " Hz but only " << speed << " BL/s: too slow.";
    else
      o << ( done ? "Swimming" : "Swimming so far" ) << " at " << f << " Hz, " << static_cast< int >( ph ) << " degrees L/R, "
        << speed << " BL/s.";
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

  void analyse( double& f, double& phase_deg, double& speed, double& swing ) const
  {
    f = phase_deg = speed = swing = 0;
    size_t n = dl_.size();
    if ( n < 100 )
      return;
    double ml = 0, mr = 0, ms = 0;
    for ( size_t i = 0; i < n; ++i )
    {
      ml += dl_[ i ];
      mr += dr_[ i ];
      ms += sp_[ i ];
    }
    ml /= n; mr /= n; speed = ms / n;
    double dmax = 0, dmin = 0;
    for ( size_t i = 0; i < n; ++i )
    {
      double d = ( dl_[ i ] - ml ) - ( dr_[ i ] - mr );
      dmax = std::max( dmax, d );
      dmin = std::min( dmin, d );
    }
    swing = dmax - dmin;
    if ( swing < 2.0 )
      return;
    // zero crossings with hysteresis (10 % of the swing)
    double hy = 0.1 * 0.5 * swing, first = -1, last = -1;
    int state = 0, crossings = 0;
    for ( size_t i = 0; i < n; ++i )
    {
      double d = ( dl_[ i ] - ml ) - ( dr_[ i ] - mr );
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
    if ( crossings < 2 )
      return;
    double period_ms = ( last - first ) / ( crossings - 1 );  // samples are ~1 ms
    f = 1000.0 / period_ms;
    // phase by complex demodulation at f
    double w = 2 * kPi * f / 1000.0, cl = 0, sl = 0, cr = 0, sr = 0;
    for ( size_t i = 0; i < n; ++i )
    {
      double c = std::cos( w * i ), s = std::sin( w * i );
      cl += ( dl_[ i ] - ml ) * c; sl += ( dl_[ i ] - ml ) * s;
      cr += ( dr_[ i ] - mr ) * c; sr += ( dr_[ i ] - mr ) * s;
    }
    double d = std::atan2( sl, cl ) - std::atan2( sr, cr );
    d = std::fabs( std::remainder( d, 2 * kPi ) );  // 0..pi
    phase_deg = d * 180.0 / kPi;
  }

  double t_ = 0, U_ = 0, x_ = 0, y_ = 0, psi_ = 0, y_tail_prev_ = 0, tail_amp_ = 0;
  long tick_ = 0;
  bool have_prev_ = false;
  std::vector< double > u_hist_, theta_, dl_, dr_, sp_;
  Lowpass rl_, rr_;
  MotorReader motor_;
};

} // namespace

std::unique_ptr< Task > make_lamprey() { return std::make_unique< LampreyTask >(); }

} // namespace spike::tasks
