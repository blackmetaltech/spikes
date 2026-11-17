// Task "reflex": a touch sensor drives a withdrawal motor pool. Detect real touches fast, ignore background noise.
//
// WORLD (deterministic given the seed; times in ms of model time)
//   Episode 20 000 ms. Six touches. Touch k starts at  1500 + 3000*k + U(0,1200)  (U from the seeded splitmix64 RNG)
//   and lasts 200 ms (a poke). So touches are always >= 1.6 s apart.
//   Sensor channel "touch" (poisson_generator, Hz; every neuron it targets gets its OWN independent Poisson train) is set
//   every slice to
//        rate = 150 Hz                     background (spontaneous afferent firing, "noise")
//        rate = 150 + 250 = 400 Hz         while a touch is on
//   Only 2.7x more than the background, so one cell sees a poke as barely more than a noisy fluctuation of the chatter:
//   a single touch->motor cell cannot pass at any weight (it either misses pokes or fires on noise). Averaging several
//   independent afferents (convergence) is what separates the two.
//   Motor channel "withdraw": population rate (Hz per neuron) of the motor pool, only used to detect bursts.
//
// BURST DETECTION (the "motor burst" of the brief)
//   Spikes of the motor pool are summed in a trailing 25 ms window. A burst is declared the moment the window holds
//   >= max(3, ceil(N/2)) spikes, N = number of neurons in the "withdraw" pool (so a 20-cell pool needs 10 spikes, not 3);
//   after a burst the detector is blind for 100 ms (so one burst is counted once).
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
constexpr double kDuration = 20000, kTouchLen = 200, kBg = 150, kTouchRate = 250;
constexpr double kDeadline = 150, kAfter = 400, kWindow = 25, kBlind = 100;
constexpr int kMinBurstSpikes = 3;

class ReflexTask : public Task
{
public:
  ReflexTask() { reset( 1 ); }
  std::string id() const override { return "reflex"; }
  std::string title() const override { return "Reflex arc"; }
  std::string brief() const override
  {
    return "A touch receptor chatters away in the background (about 150 spikes/s) and fires a little faster (about "
           "400/s) while something pokes the skin, so a poke is hard to tell from a noisy stretch of chatter. Wire "
           "touch to some neurons and on to a withdraw motor pool so the animal pulls back within 60 ms of every "
           "poke, yet stays still when the receptor only chatters. Six pokes arrive at random times in 20 s. Score: "
           "how fast you respond, pokes you miss, and false withdrawals triggered by noise. A \"withdrawal\" is a burst "
           "of motor spikes (at least 3 within 25 ms, or half the pool if it has more than 6 cells). Hint: one cell "
           "listening to one afferent cannot do it, because noise spikes bunch up by chance. Give the touch input "
           "several receptor cells, and let the motor cells need several of their spikes at the same time "
           "(convergence averages the noise away). Too much weight makes the motor cells fire on noise, too little "
           "and they stay silent: tune the weights between the two. A recipe that works (weights are pA): touch onto 10 cells (the first "
           "connection sets the sensor weight), those onto 6 motor cells with a link of about 140 (the default 80 misses pokes, above about "
           "200 the motor cells fire on noise), the motor cells onto withdraw.";
  }
  std::vector< std::string > sensor_channels() const override { return { "touch" }; }
  std::vector< std::string > motor_channels() const override { return { "withdraw" }; }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    return { { "touch", in_kind( "poisson", 150, 400 ) } };
  }
  double duration_ms() const override { return kDuration; }
  int par_neurons() const override { return variant_par( variant_, 16 ); }  // SN 10 + MN 6 of the recipe

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return { "threshold", "excitation", "convergence" }; }
  std::vector< std::string > concepts_used() const override { return {}; }
  bool on_spine() const override { return true; }
  /// guided: poke 1.67x stronger, <= 2 false withdrawals, latency <= 75 ms; standard: the brief; researcher: poke 0.8x as strong (200 Hz above the chatter), no false
  /// withdrawal, latency <= 51 ms (reference circuit: 23-25 ms, passes seeds 1-5).
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 8 cells instead of 16" },
             { "noise_x2", "the poke is half as strong against the chatter (125 Hz above 150 Hz instead of 250 Hz)" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    const bool jumpy = s.parts[ "false_triggers" ] > false_limit();
    std::vector< Hint > h;
    if ( !jumpy )
    {
      h.push_back( make_hint( 1, "A poke is only 2.7 times the background rate, so one afferent cannot tell it from noise. Convergence: a cell that needs several inputs at once.", "" ) );
      h.push_back( make_hint( 2, "Look at the link into the motor pool: it is too weak for the pokes to reach threshold.", "withdraw" ) );
      h.push_back( make_hint( 3, "Touch onto about 10 receptor cells, those onto 6 motor cells with a link near 140 pA, motor cells onto withdraw.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "Noise spikes bunch up by chance. Raise the threshold by asking for more coincident inputs.", "" ) );
      h.push_back( make_hint( 2, "The motor pool is firing on noise: the links into it are too strong.", "withdraw" ) );
      h.push_back( make_hint( 3, "Keep the motor link under about 200 pA, or give the touch input more receptor cells so their average is steadier.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override { return { { "latency_ms", make_metric( score().parts[ "latency_ms" ], "ms", true ) } }; }
  Note notebook() const override
  {
    return { "Convergence beats noise",
             "Withdrawal reflexes in Aplysia pass sensory input through interneurons that sum many weak afferents, so a brief touch "
             "stands out from the background chatter of individual sensory neurons.",
             "Castellucci V, Kandel ER (1974) PNAS 71:5004" };
  }

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
    wiring_.invalidate();
    spikes_ = 0;
    burst_spikes_ = kMinBurstSpikes;
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
    wiring_.probe( e, sensor_channels(), motor_channels() );
    burst_spikes_ = std::max( kMinBurstSpikes, static_cast< int >( std::ceil( 0.5 * motor_.pool_size( e, "withdraw" ) - 1e-9 ) ) );
    touching_ = in_touch( t_ );
    e.set_input( "touch", bg() + ( touching_ ? amp() : 0.0 ) );
    e.step( dt );
    t_ += dt;
    double n = motor_.spike_count( e, "withdraw", dt );
    spikes_ += n;
    window_.push_back( { t_, n } );
    window_sum_ += n;
    while ( !window_.empty() && window_.front().first <= t_ - kWindow )
    {
      window_sum_ -= window_.front().second;
      window_.pop_front();
    }
    if ( window_sum_ >= burst_spikes_ - 1e-6 && t_ - last_burst_ >= kBlind )
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
    b.readouts[ "touch_rate_hz" ] = bg() + ( touching_ ? amp() : 0.0 );
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
    s.parts[ "motor_spikes" ] = spikes_;
    s.parts[ "burst_threshold" ] = burst_spikes_;
    double hf = static_cast< double >( hits ) / kTouches;
    double v = 100.0 * hf * ( 1.0 - 0.4 * clampd( ( lat - 20.0 ) / 80.0, 0, 1 ) ) - 12.0 * false_triggers_;
    s.value = std::max( 0.0, v );
    bool done = t_ >= kDuration;
    s.passed = done && misses == 0 && hits == kTouches && false_triggers_ <= false_limit() && lat <= tol_max( mode_, 60.0, 1.25, 0.85 );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done )
      o << "In progress: " << hits << " of " << kTouches << " pokes answered, " << false_triggers_ << " false withdrawals.";
    else if ( s.passed )
      o << "Clean reflex: all " << kTouches << " pokes answered in " << static_cast< int >( lat ) << " ms on average, "
        << false_triggers_ << " false withdrawals.";
    else if ( spikes_ < 1 )
      o << "withdraw never fired: the motor cells got no spikes at all. Check the touch input reaches a neuron, "
           "that each link to the next cell exists, and increase the weights along the chain.";
    else if ( bursts_ == 0 )
      o << "withdraw fired " << static_cast< int >( spikes_ ) << " stray spikes but never a burst of " << burst_spikes_
        << " within 25 ms: strengthen the weights into the motor cells, or add more cells feeding them.";
    else if ( false_triggers_ > false_limit() && misses )
      o << "Missed " << misses << " of " << kTouches << " pokes and fired " << false_triggers_
        << " false withdrawals: the circuit reacts to noise but not reliably to pokes.";
    else if ( misses )
      o << "Missed " << misses << " of " << kTouches << " pokes (answered " << hits << "): the response is too weak or too late.";
    else if ( false_triggers_ > false_limit() )
      o << "Too jumpy: " << false_triggers_ << " withdrawals with no poke. Make the motor cells need several touch inputs at once.";
    else
      o << "Too slow: mean latency " << static_cast< int >( lat ) << " ms (needs <= " << static_cast< int >( tol_max( mode_, 60.0, 1.25, 0.85 ) ) << " ms).";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { -0.2, -0.5, 1.4, 0.5 }; }

private:
  // "noise" is the poke's contrast against the chatter: the background stays at 150 Hz, the poke adds 250 Hz / noise factor
  double bg() const { return kBg; }
  double amp() const { return kTouchRate / mode_noise( mode_, variant_, 0.6, 1.25 ); }
  int false_limit() const { return mode_ == "guided" ? 2 : ( mode_ == "researcher" ? 0 : 1 ); }
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
  int false_triggers_ = 0, bursts_ = 0, burst_spikes_ = kMinBurstSpikes;
  double spikes_ = 0;
  Wiring wiring_;
  bool touching_ = false;
  MotorReader motor_;
};

} // namespace

std::unique_ptr< Task > make_reflex() { return std::make_unique< ReflexTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( reflex, 10, "reflex_arc.json", make_reflex ) }
