// Task "reflex": a touch sensor drives a withdrawal motor pool. Detect real touches fast, ignore background noise.
//
// WORLD (deterministic given the seed; times in ms of model time)
//   Episode 20 000 ms. Six touches. Touch k starts at  1500 + 3000*k + U(0,1200)  (U from the seeded splitmix64 RNG)
//   and lasts 200 ms (a poke). So touches are always >= 1.6 s apart.
//   Sensor channel "touch" (poisson_generator, Hz) is set every slice to
//        rate = 40 Hz                      background (spontaneous afferent firing, "noise")
//        rate = 40 + 400 = 440 Hz          while a touch is on
//   Motor channel "withdraw": population rate (Hz per neuron) of the motor pool, only used to detect bursts.
//
// BURST DETECTION (the "motor burst" of the brief)
//   Spikes of the motor pool are summed in a trailing 25 ms window. A burst is declared the moment the window holds
//   >= 3 spikes; after a burst the detector is blind for 100 ms (so one burst is counted once).
//
// SCORING
//   hit        : a burst at t with  onset <= t <= onset + 150 ms   (the first one counts; latency = t - onset)
//   miss       : a touch whose 150 ms deadline passed with no burst
//   false      : a burst that is not inside any [onset, onset + 400 ms] window (after-discharge in that window is free)
//   value      : 100 * hit_fraction * (1 - 0.4*clamp((mean_latency-20)/80, 0, 1)) - 12*false_triggers, floored at 0
//   pass       : no misses, at most 1 false trigger, mean latency <= 60 ms
//
// Biology: the withdrawal reflex arc (sensory afferent -> interneuron -> motoneuron), e.g. the Aplysia siphon/gill
// withdrawal (Kandel) or the leech/vertebrate flexor reflex; the interesting part is noise rejection by convergence.
#include <cmath>
#include <deque>
#include <sstream>
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr int kTouches = 6;
constexpr double kDuration = 20000, kTouchLen = 200, kBg = 40, kTouchRate = 400;
constexpr double kDeadline = 150, kAfter = 400, kWindow = 25, kBlind = 100;
constexpr int kBurstSpikes = 3;

class ReflexTask : public Task
{
public:
  ReflexTask() { reset( 1 ); }
  std::string id() const override { return "reflex"; }
  std::string title() const override { return "Reflex arc"; }
  std::string brief() const override
  {
    return "A touch receptor fires a little in the background and a lot when something pokes the skin. Wire the "
           "receptor to a withdrawal motor pool so the animal pulls away within 60 ms of every poke, yet stays still "
           "when the receptor only chatters. Six pokes arrive at random times in 20 s. Score: how fast you respond, "
           "pokes you miss, and false withdrawals triggered by noise. Hint: one noise spike must not be enough; "
           "make the motor cells need several inputs at once.";
  }
  std::vector< std::string > sensor_channels() const override { return { "touch" }; }
  std::vector< std::string > motor_channels() const override { return { "withdraw" }; }
  double duration_ms() const override { return kDuration; }

  void reset( uint64_t seed ) override
  {
    Rng rng( seed ^ 0x7265666C6578ULL );
    for ( int k = 0; k < kTouches; ++k )
    {
      onset_[ k ] = 1500 + 3000.0 * k + rng.uniform( 0, 1200 );
      latency_[ k ] = -1;
      deadline_passed_[ k ] = false;
    }
    t_ = 0;
    window_.clear();
    window_sum_ = 0;
    last_burst_ = -1e9;
    false_triggers_ = 0;
    bursts_ = 0;
    motor_.invalidate();
    touching_ = false;
    recent_burst_t_ = -1e9;
  }

  void step( Engine& e, double dt ) override
  {
    touching_ = in_touch( t_ );
    e.set_input( "touch", kBg + ( touching_ ? kTouchRate : 0.0 ) );
    e.step( dt );
    t_ += dt;
    double n = motor_.spike_count( e, "withdraw", dt );
    window_.push_back( { t_, n } );
    window_sum_ += n;
    while ( !window_.empty() && window_.front().first <= t_ - kWindow )
    {
      window_sum_ -= window_.front().second;
      window_.pop_front();
    }
    if ( window_sum_ >= kBurstSpikes && t_ - last_burst_ >= kBlind )
    {
      last_burst_ = t_;
      ++bursts_;
      recent_burst_t_ = t_;
      classify_burst( t_ );
    }
    for ( int k = 0; k < kTouches; ++k )
      if ( t_ > onset_[ k ] + kDeadline )
        deadline_passed_[ k ] = true;
  }

  BodyState body() const override
  {
    // A stick-figure limb: it retracts (shortens) for 300 ms after a motor burst; the touch shows as a poke from the right.
    BodyState b;
    double since = t_ - recent_burst_t_;
    double retract = since >= 0 && since < 300 ? 1.0 - since / 300.0 : 0.0;
    double len = 1.0 - 0.6 * retract;
    b.points = { { 0.0, 0.0 }, { 0.5 * len, 0.0 }, { len, 0.0 } };
    b.heading = 0;
    b.readouts[ "touching" ] = touching_ ? 1.0 : 0.0;
    b.readouts[ "touch_rate_hz" ] = kBg + ( touching_ ? kTouchRate : 0.0 );
    b.readouts[ "withdrawn" ] = retract;
    b.readouts[ "bursts" ] = bursts_;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    int hits = 0, misses = 0;
    double lat_sum = 0;
    for ( int k = 0; k < kTouches; ++k )
    {
      if ( latency_[ k ] >= 0 )
      {
        ++hits;
        lat_sum += latency_[ k ];
      }
      else if ( deadline_passed_[ k ] )
        ++misses;
    }
    double lat = hits ? lat_sum / hits : 0;
    s.parts[ "hits" ] = hits;
    s.parts[ "misses" ] = misses;
    s.parts[ "false_triggers" ] = false_triggers_;
    s.parts[ "latency_ms" ] = lat;
    s.parts[ "bursts" ] = bursts_;
    double hf = static_cast< double >( hits ) / kTouches;
    double v = 100.0 * hf * ( 1.0 - 0.4 * clampd( ( lat - 20.0 ) / 80.0, 0, 1 ) ) - 12.0 * false_triggers_;
    s.value = std::max( 0.0, v );
    bool done = t_ >= kDuration;
    s.passed = done && misses == 0 && hits == kTouches && false_triggers_ <= 1 && lat <= 60.0;
    std::ostringstream o;
    if ( !done )
      o << "In progress: " << hits << " of " << kTouches << " pokes answered, " << false_triggers_ << " false withdrawals.";
    else if ( s.passed )
      o << "Clean reflex: all " << kTouches << " pokes answered in " << static_cast< int >( lat ) << " ms on average, "
        << false_triggers_ << " false withdrawals.";
    else if ( misses )
      o << "Missed " << misses << " of " << kTouches << " pokes (answered " << hits << ").";
    else if ( false_triggers_ > 1 )
      o << "Too jumpy: " << false_triggers_ << " withdrawals with no poke.";
    else
      o << "Too slow: mean latency " << static_cast< int >( lat ) << " ms (needs <= 60 ms).";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { -0.2, -0.5, 1.4, 0.5 }; }

private:
  bool in_touch( double t ) const
  {
    for ( int k = 0; k < kTouches; ++k )
      if ( t >= onset_[ k ] && t < onset_[ k ] + kTouchLen )
        return true;
    return false;
  }
  void classify_burst( double t )
  {
    for ( int k = 0; k < kTouches; ++k )
    {
      if ( t >= onset_[ k ] && t <= onset_[ k ] + kAfter )
      {
        if ( latency_[ k ] < 0 && t - onset_[ k ] <= kDeadline )
          latency_[ k ] = t - onset_[ k ];
        return;  // late or repeated burst inside a touch window: neither a hit nor a false trigger
      }
    }
    ++false_triggers_;
  }

  double onset_[ kTouches ] = {};
  double latency_[ kTouches ] = {};
  bool deadline_passed_[ kTouches ] = {};
  double t_ = 0, window_sum_ = 0, last_burst_ = -1e9, recent_burst_t_ = -1e9;
  std::deque< std::pair< double, double > > window_;
  int false_triggers_ = 0, bursts_ = 0;
  bool touching_ = false;
  MotorReader motor_;
};

} // namespace

std::unique_ptr< Task > make_reflex() { return std::make_unique< ReflexTask >(); }

} // namespace spike::tasks
