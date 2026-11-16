// Task "heart_pacemaker": a sinoatrial-node cell (or a coupled cluster of them) must beat like a heart.
//
// WORLD (body: spike::body::Heart, docs/body/heart.md; deterministic, no randomness; times in ms)
//   Sensors: none. The SA cells fire on their own (funny current I_f, T- and L-type Ca, delayed rectifier K).
//   Motor "sa_beat": spikes of the cluster. Every spike is one beat request to the heart; the heart merges requests
//   closer than its 250 ms refractory period into the first one (the first cell to fire starts the beat, as in the node).
//   Autonomic drives are held at rest tone (sympathetic 0.2, vagal 0.6): this task is about the intrinsic rhythm.
//   The beat times are the heart's beat events (Heart::beat_now), so a cluster that does not agree on one rhythm gives
//   extra, irregular beats exactly as it would for the ventricle.
//
// SCORING over the analysis window t in [6000 ms, 30000 ms] (the first 6 s let the cells reach their limit cycle)
//   rr      : successive beat intervals; need >= 12 beats, else "too few beats"
//   hr_bpm  : 60000 / mean rr
//   cv      : sd(rr) / mean(rr), sample sd
//   sdnn, rmssd : time-domain heart-rate variability of the beat times (analysis::hrv_analyze)
//   synchrony : Golomb (2007) chi of the cluster's spike counts in 40 ms bins over the window, 0 = asynchronous, 1 = all
//           cells spike together; only defined for a motor pool of >= 2 cells (0 for a single cell)
//   rate_term = 1 for 60 <= hr <= 80, falling linearly to 0 at 20 bpm outside; cv_term = 1 for cv <= 2 %, 0 at 15 %
//   value = 100 * (0.5 rate_term + 0.5 cv_term) * (0.85 + 0.15 sync_term), sync_term = clamp((chi - 0.3) / 0.6, 0, 1)
//           for a pool of >= 2 cells (0 for one cell: a lone cell tops out at 85, the coupled cluster is the bonus)
//   pass  : 60 <= hr <= 80 and cv < 5 %, at the end of the episode
//   verdict: names the failure: not wired, no beats, too slow, too fast, irregular (and whether the cells disagree).
//
// Biology: the sinoatrial node paces the heart at 60-100/min at rest in humans (intrinsic ~100-110, slowed by vagal tone;
// Jose & Collison 1970 Cardiovasc Res 4:160); node cells differ in intrinsic rate and are coupled by gap junctions, which
// entrain them to one frequency (Michaels, Matyas & Jalife 1987 Circ Res 61:704 for the coupled-oscillator picture; Jalife 1984
// J Physiol 356:221). Beat-to-beat variability is small (CV 2-5 %, Task Force 1996 Circulation 93:1043).
#include <cmath>
#include <iomanip>
#include <sstream>
#include "spike/analysis.h"
#include "spike/body/heart.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr double kDuration = 30000, kSkip = 6000, kBinMs = 40;
constexpr int kMinBeats = 12;

class HeartPacemakerTask : public Task
{
public:
  HeartPacemakerTask() { reset( 1 ); }
  std::string id() const override { return "heart_pacemaker"; }
  std::string title() const override { return "Sinoatrial pacemaker"; }
  std::string brief() const override
  {
    return "The heart beats because a small patch of cells in the right atrium, the sinoatrial (SA) node, fires on its own: "
           "a slow inward 'funny' current (I_f) depolarises each cell until calcium channels open and it fires one beat. "
           "Place SA-node cells from the parts bin and read their spikes out as sa_beat; each spike is a beat of the heart. "
           "The beat must come at 60-80 per minute and be regular (beat-interval variation under 5 percent). "
           "Real node cells are not identical, so a cluster of different cells has to agree on one rhythm: couple them with "
           "excitatory synapses (the stand-in for gap junctions) and the fastest does not run away with the beat. "
           "Bonus: the more tightly the cluster fires together (synchrony), the higher the score.";
  }
  std::vector< std::string > sensor_channels() const override { return {}; }
  std::vector< std::string > motor_channels() const override { return { "sa_beat" }; }
  double duration_ms() const override { return kDuration; }
  std::string chapter() const override { return "heart"; }
  std::string view() const override { return "heart"; }
  int par_neurons() const override { return 7; }
  std::vector< Event > timeline() const override { return { { 0, kSkip, "settling" }, { kSkip, kDuration - kSkip, "scored window" } }; }
  std::vector< Band > bands() const override { return { { "hr_bpm", "bpm", 60, 80 } }; }
  std::vector< std::string > modulators() const override { return {}; }

  void reset( uint64_t ) override
  {
    heart_.reset();
    t_ = 0;
    beats_.clear();
    last_beat_ = -1;
    hr_inst_ = 0;
    bins_.clear();
    bin_acc_ = 0;
    bin_t_ = 0;
    spikes_ = 0;
    pool_ = 1;
    motor_.invalidate();
    wiring_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    e.step( dt );
    double cnt = motor_.spike_count( e, "sa_beat", dt );
    pool_ = motor_.pool_size( e, "sa_beat" );
    int n = static_cast< int >( std::lround( cnt ) );
    spikes_ += n;
    if ( t_ >= kSkip )
    {
      bin_acc_ += n;
      bin_t_ += dt;
      if ( bin_t_ >= kBinMs - 1e-9 )
      {
        bins_.push_back( bin_acc_ );
        bin_acc_ = 0;
        bin_t_ = 0;
      }
    }
    int ticks = std::max( 1, static_cast< int >( std::ceil( dt - 1e-9 ) ) );
    double h = dt / ticks;
    for ( int i = 0; i < ticks; ++i )
    {
      body::HeartInput in;
      in.sa_spikes = i == 0 ? n : 0;
      in.sa_rate_hz = 0;  // beats only from the SA cells
      in.sym_cardiac = in.sym_vascular = 0.2;
      in.vagal = 0.6;
      heart_.step( h, in );
      t_ += h;
      if ( heart_.beat_now )
      {
        beats_.push_back( t_ );
        last_beat_ = t_;
      }
    }
    double since = last_beat_ < 0 ? 1e9 : t_ - last_beat_;
    double rr = beats_.size() >= 2 ? beats_.back() - beats_[ beats_.size() - 2 ] : 1e9;
    double iv = std::max( rr, since );
    hr_inst_ = iv >= 1e8 ? 0.0 : 60000.0 / iv;
  }

  BodyState body() const override
  {
    BodyState b;
    b.points = { { 0, 0 } };
    b.readouts = heart_.readouts();
    auto v = heart_.view();
    b.readouts[ "hr_bpm" ] = hr_inst_;
    b.readouts[ "heart_phase" ] = v.phase;
    b.readouts[ "ejection" ] = v.ejection;
    b.readouts[ "beat" ] = v.beat ? 1.0 : 0.0;
    b.readouts[ "beats" ] = heart_.beats;
    b.readouts[ "t_ms" ] = t_;
    b.readouts[ "cells" ] = pool_;
    b.readouts[ "last_beat_ms" ] = last_beat_;
    return b;
  }

  Score score() const override
  {
    Score s;
    Stats a = stats();
    s.parts[ "hr_bpm" ] = a.hr;
    s.parts[ "cv" ] = a.cv;
    s.parts[ "beats" ] = a.n;
    s.parts[ "sdnn_ms" ] = a.sdnn;
    s.parts[ "rmssd_ms" ] = a.rmssd;
    s.parts[ "synchrony" ] = a.chi;
    s.parts[ "cells" ] = pool_;
    s.parts[ "spikes" ] = spikes_;
    bool enough = a.n >= kMinBeats;
    double rate_term = 0, cv_term = 0, sync_term = 0;
    if ( enough )
    {
      double dist = a.hr < 60 ? 60 - a.hr : ( a.hr > 80 ? a.hr - 80 : 0 );
      rate_term = clampd( 1.0 - dist / 20.0, 0, 1 );
      cv_term = clampd( 1.0 - ( a.cv - 0.02 ) / 0.13, 0, 1 );
      if ( pool_ >= 2 )
        sync_term = clampd( ( a.chi - 0.3 ) / 0.6, 0, 1 );
    }
    s.value = 100.0 * ( 0.5 * rate_term + 0.5 * cv_term ) * ( 0.85 + 0.15 * sync_term );
    bool done = t_ >= kDuration;
    bool rate_ok = enough && a.hr >= 60 && a.hr <= 80, cv_ok = enough && a.cv < 0.05;
    s.passed = done && rate_ok && cv_ok;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( spikes_ < 1 )
      o << "No beats: sa_beat has not fired" << yet << " Check that the cells are SA-node pacemaker cells and that sa_beat reads them.";
    else if ( !enough )
      o << "Too few beats (" << static_cast< int >( a.n ) << " in the scored window, 12 needed): the node is pausing or has stopped"
        << yet;
    else if ( a.hr < 60 )
      o << "Bradycardia: " << a.hr << " beats/min, below the 60-80 target.";
    else if ( a.hr > 80 )
      o << "Tachycardia: " << a.hr << " beats/min, above the 60-80 target"
        << ( pool_ >= 2 && a.chi < 0.6 ? " (the cells are not locked together, so extra beats get through)." : "." );
    else if ( !cv_ok )
      o << "Rate is fine (" << a.hr << "/min) but the rhythm is irregular: beat-to-beat variation " << 100 * a.cv
        << " % (needs under 5 %)" << ( pool_ >= 2 && a.chi < 0.6 ? "; the cells fire out of step." : "." );
    else
      o << ( done ? "Sinus rhythm" : "Sinus rhythm so far" ) << ": " << a.hr << " beats/min, variation " << 100 * a.cv << " %, "
        << ( pool_ >= 2 ? "cluster synchrony " : "single cell, no cluster bonus; synchrony " ) << a.chi << ".";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { -1, -1, 1, 1 }; }

private:
  struct Stats
  {
    double hr = 0, cv = 0, n = 0, sdnn = 0, rmssd = 0, chi = 0;
  };

  Stats stats() const
  {
    Stats r;
    std::vector< double > bt;
    for ( double t : beats_ )
      if ( t >= kSkip )
        bt.push_back( t );
    r.n = bt.size() >= 1 ? bt.size() - 1 : 0;  // intervals
    if ( bt.size() >= 3 )
    {
      std::vector< double > rr;
      for ( size_t i = 1; i < bt.size(); ++i )
        rr.push_back( bt[ i ] - bt[ i - 1 ] );
      double m = 0;
      for ( double x : rr )
        m += x;
      m /= rr.size();
      double v = 0;
      for ( double x : rr )
        v += ( x - m ) * ( x - m );
      v /= std::max< size_t >( 1, rr.size() - 1 );
      r.hr = 60000.0 / m;
      r.cv = std::sqrt( v ) / m;
      analysis::HrvResult h = analysis::hrv_analyze( bt );
      r.sdnn = std::isnan( h.sdnn_ms ) ? 0 : h.sdnn_ms;
      r.rmssd = std::isnan( h.rmssd_ms ) ? 0 : h.rmssd_ms;
    }
    // Golomb chi on the pooled counts: var_t(mean_i s_i) / mean_i var_t(s_i), with s_i in {0,1} per bin
    // so that var(s_i) = p (1 - p), p = spikes per cell per bin.
    if ( pool_ >= 2 && bins_.size() >= 10 )
    {
      double N = pool_, B = bins_.size(), tot = 0;
      for ( double c : bins_ )
        tot += c;
      double p = tot / ( N * B ), mean_c = tot / B, var_c = 0;
      for ( double c : bins_ )
        var_c += ( c - mean_c ) * ( c - mean_c );
      var_c /= B;
      double denom = p * ( 1 - p );
      r.chi = denom > 1e-12 ? std::min( 1.0, std::sqrt( ( var_c / ( N * N ) ) / denom ) ) : 0.0;
    }
    return r;
  }

  body::Heart heart_;
  double t_ = 0, last_beat_ = -1, hr_inst_ = 0, bin_acc_ = 0, bin_t_ = 0, spikes_ = 0, pool_ = 1;
  std::vector< double > beats_, bins_;
  MotorReader motor_;
  Wiring wiring_;
};

} // namespace

std::unique_ptr< Task > make_heart_pacemaker() { return std::make_unique< HeartPacemakerTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( heart_pacemaker, 200, "heart_pacemaker.json", make_heart_pacemaker ) }
