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
//   (log: Weber-like, ASE cells respond to relative changes, Suzuki et al. 2008 Neuron 57:1).
//     ASEL (ON : fires when c rises)   rate = 5 + 400 * clamp(  s / 0.05 /s, 0, 1 )  Hz
//     ASER (OFF: fires when c falls)   rate = 5 + 400 * clamp( -s / 0.05 /s, 0, 1 )  Hz
//   i.e. a 5 Hz background plus a response that saturates at |d ln c/dt| = 0.05 per second.
//   Motors "turn_left", "turn_right": mean Hz per neuron, low-passed with tau = 150 ms;
//     omega = 1.5 rad/s * clamp( (rate_left - rate_right) / 40 Hz, -1, 1 )   (positive = counter-clockwise = left)
//
// SCORING
//   t_reach  : first time the head-centre is within 0.1 R = 1 mm of the peak (else not reached)
//   mean_dist: mean distance to the peak over the episode (mm)
//   value    : 100 * ( 0.6 * (reached ? 1 - t_reach/duration : 0) + 0.4 * clamp(1 - mean_dist / start_dist, 0, 1) )
//   pass     : reached the 1 mm disc within the 120 s episode
//
// Biology: ASEL/ASER on/off salt sensing (Suzuki et al. 2008; Thiele, Faumont, Lockery 2009 J Neurosci); klinotaxis
// and pirouettes (Pierce-Shimomura, Morse, Lockery 1999 J Neurosci; Iino & Yoshida 2009 J Neurosci); AIY promotes
// runs, turning pathway via AIB/RIA/SMB (Gray, Hill, Bargmann 2005 PNAS; Kunitomo et al. 2013 Nat Commun).
#include <cmath>
#include <deque>
#include <sstream>
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

class WormTask : public Task
{
public:
  WormTask() { reset( 1 ); }
  std::string id() const override { return "worm"; }
  std::string title() const override { return "Worm chemotaxis"; }
  std::string brief() const override
  {
    return "A worm crawls at constant speed across a plate with a hill of salt it loves. At its head, ASEL fires when "
           "the salt level rises and ASER fires when it falls. Two motor pools, turn_left and turn_right, set how fast "
           "it steers. It starts facing away from the peak. Wire the two sensors so it turns when things get worse "
           "and runs straight when they improve. Score: get within 10 % of the plate radius of the peak before 120 s "
           "(faster is better), and how close you stay on average.";
  }
  std::vector< std::string > sensor_channels() const override { return { "ASEL", "ASER" }; }
  std::vector< std::string > motor_channels() const override { return { "turn_left", "turn_right" }; }
  double duration_ms() const override { return kDuration; }

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
    start_dist_ = std::hypot( sx - peak_[ 0 ], sy - peak_[ 1 ] );
    t_ = 0;
    trail_.clear();
    trail_.push_back( pos_ );
    t_reach_ = -1;
    dist_sum_ = 0;
    nsteps_ = 0;
    ml_.tau_ms = mr_.tau_ms = kMotorTau;
    ml_.reset( 0 );
    mr_.reset( 0 );
    sens_.tau_ms = kSensTau;
    sens_.reset( 0 );
    prev_lnc_ = ln_c_head();
    omega_ = 0;
    motor_.invalidate();
    rate_l_ = rate_r_ = kBase;
  }

  void step( Engine& e, double dt ) override
  {
    e.set_input( "ASEL", rate_l_ );
    e.set_input( "ASER", rate_r_ );
    e.step( dt );
    double l = motor_.hz_per_neuron( e, "turn_left" ), r = motor_.hz_per_neuron( e, "turn_right" );
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
    s.value = 100.0 * ( 0.6 * ( reached ? 1.0 - t_reach_ / kDuration : 0.0 ) + 0.4 * clampd( 1.0 - mean_d / start_dist_, 0, 1 ) );
    s.passed = reached;
    std::ostringstream o;
    o.precision( 3 );
    if ( reached )
      o << "Reached the salt peak after " << t_reach_ / 1000.0 << " s; mean distance " << mean_d << " mm.";
    else if ( t_ >= kDuration )
      o << "Never got within " << kReach << " mm of the peak (closest approach measured as mean distance " << mean_d << " mm).";
    else
      o << "Still searching: " << s.parts[ "final_dist_mm" ] << " mm from the peak.";
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
    rate_l_ = kBase + kGain * clampd( sens_.y / kSensSat, 0, 1 );
    rate_r_ = kBase + kGain * clampd( -sens_.y / kSensSat, 0, 1 );
    // bookkeeping
    double d = std::hypot( pos_[ 0 ] - peak_[ 0 ], pos_[ 1 ] - peak_[ 1 ] );
    dist_sum_ += d;
    ++nsteps_;
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
  std::deque< std::array< double, 2 > > trail_;
  Lowpass ml_, mr_, sens_;
  Rng noise_rng_;
  MotorReader motor_;
};

} // namespace

std::unique_ptr< Task > make_worm() { return std::make_unique< WormTask >(); }

} // namespace spike::tasks
