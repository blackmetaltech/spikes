// Task "baroreflex": hold arterial pressure. Build the brainstem baroreflex: baroreceptor afferents -> NTS -> nucleus ambiguus
// (cardiac vagal) and CVLM -| RVLM (sympathetic), so that mean arterial pressure (MAP) stays at 90 +- 10 mmHg when the body is
// stood up (orthostatic stress) and after a haemorrhage.
//
// WORLD (body: spike::body::Heart, docs/body/heart.md; deterministic, no randomness; times in ms)
//   Scripted events: 0-15 s supine; STAND UP at 15 s (blood pools in the legs, tau 6 s, up to 220 ml); LIE DOWN at 60 s (the
//   pooled blood returns); HAEMORRHAGE from 80 s to 110 s at 10 ml/s (300 ml lost, not replaced); episode ends at 140 s.
//   Sensor "baroreceptor": Poisson rate in Hz, set every slice to the heart's baroreceptor afferent rate (the 1 s average of
//     100 Hz * sigmoid((P + 0.04 dP/dt - 100) / 20), Kirchheim 1976; Heart::baro_mean_hz). About 40 Hz at MAP 93.
//   Motors: "sa_beat" (SA-node spikes: beats), "vagal" (modulator output, ACh, from the cardiac vagal ganglion cells) and
//     "sympathetic" (modulator output, NE, from the sympathetic ganglion cells). The task reads Engine::modulator("ACh") and
//     ("NE"); the circuit's "modulation" table maps them onto the SA cells.
//   Effects: heart rate comes from the SA cells (so vagal and sympathetic drive both change it). The sympathetic level sets
//   contractility and vascular tone: sym_cardiac = sym_vascular = clamp( 0.2 + 0.5 (NE - 0.2), 0, 1 ), 0.2 at NE = 0.2 (the
//   heart module's resting tone): resistance R = R0 (1 + 0.8 (sym - 0.2)), venoconstriction, contractility 1 + (sym - 0.2). A silent sympathetic
//   output (NE = 0) therefore lowers resting pressure too (sympathetic tone is part of normal pressure).
//
// SCORING (MAP = Heart::map_smooth, sampled every 100 ms from 10 s on)
//   in_band      : fraction of samples with 80 <= MAP <= 100 mmHg
//   settle_stand : time from the stand-up (15 s) until MAP has stayed inside the band for 3 s running (0 if it never left);
//                  settle_lie and settle_haem likewise for the lie-down (segment 60-80 s) and the haemorrhage (80 s to the end);
//                  a segment that never gets 3 s in band scores its whole length
//   tail_stand, tail_lie, tail_haem : fraction of the last 10 s of each segment spent in the band
//   map_min, map_max : extremes after 10 s
//   value = 100 * in_band
//   pass  : in_band >= 0.85, every settle time <= 15 s, and every tail fraction >= 0.8
//   verdict: names the failure (no sympathetic output: pressure collapses; pressure runs high; falls on standing; ...).
//
// Biology: arterial baroreceptors (carotid sinus, aortic arch) fire with pressure and drive the NTS; NTS excites the nucleus
// ambiguus (vagal slowing of the heart) and the caudal ventrolateral medulla (CVLM), whose GABAergic neurons inhibit the tonically
// active rostral ventrolateral medulla (RVLM), the source of sympathetic vasomotor tone (Guyenet 2006 Nat Rev Neurosci 7:335;
// Dampney 1994 Physiol Rev 74:323; Kirchheim 1976 Physiol Rev 56:100). Without the reflex, standing causes orthostatic
// hypotension; sino-aortic denervation removes the inhibition and makes pressure unregulated (Cowley, Liard & Guyton 1973
// Circ Res 32:564).
#include <cmath>
#include <iomanip>
#include <sstream>
#include "spike/body/heart.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr double kDuration = 140000, kSkip = 10000;
constexpr double kStand = 15000, kLie = 60000, kHaem0 = 80000, kHaem1 = 110000;
constexpr double kHaemRate = 10.0;  // ml/s
constexpr double kLo = 80, kHi = 100, kRecoverHold = 3000;  // ms in band to count as recovered
constexpr double kSymGain = 0.5;  // sympathetic drive change per unit of NE change (NE 0.2 = resting tone = Heart's sym_rest 0.2)

class BaroreflexTask : public Task
{
public:
  BaroreflexTask() { reset( 1 ); }
  std::string id() const override { return "baroreflex"; }
  std::string title() const override { return "Baroreflex"; }
  std::string brief() const override
  {
    return "Standing up pools blood in the legs and pressure falls; a haemorrhage lowers it further. Arterial baroreceptors "
           "fire faster the more their wall is stretched; the brainstem turns that into a counter-move within seconds. "
           "The afferents (the 'baroreceptor' sensor) excite the nucleus of the solitary tract (NTS). The NTS excites the "
           "nucleus ambiguus, whose cardiac vagal neurons slow the heart (a second cell in the cardiac ganglion releases "
           "ACh: the 'vagal' output), and the caudal ventrolateral medulla (CVLM), whose inhibitory cells silence the "
           "tonically active rostral ventrolateral medulla (RVLM). RVLM drives the sympathetic preganglionic neurons of the "
           "spinal cord, and a ganglion cell releases noradrenaline: the 'sympathetic' output, which speeds the heart and "
           "raises its force and the tone of the vessels. High pressure must therefore reduce sympathetic output and raise "
           "vagal output, low pressure the reverse. Include the SA node so that the heart beats (sa_beat). "
           "Score: keep mean arterial pressure at 90 +- 10 mmHg through standing, lying down and a blood loss of 300 ml.";
  }
  std::vector< std::string > sensor_channels() const override { return { "baroreceptor" }; }
  std::vector< std::string > motor_channels() const override { return { "sa_beat", "vagal", "sympathetic" }; }
  double duration_ms() const override { return kDuration; }
  std::string chapter() const override { return "heart"; }
  std::string view() const override { return "dashboard"; }
  int par_neurons() const override { return 48; }
  std::vector< Event > timeline() const override
  {
    return { { kStand, 2000, "stand up" }, { kStand, kLie - kStand, "upright" }, { kLie, 2000, "lie down" },
      { kHaem0, kHaem1 - kHaem0, "haemorrhage 300 ml" } };
  }
  std::vector< Band > bands() const override { return { { "map_mmHg", "mmHg", kLo, kHi } }; }
  std::vector< std::string > modulators() const override { return { "NE", "ACh" }; }

  void reset( uint64_t ) override
  {
    heart_.reset();
    t_ = 0;
    beats_.clear();
    last_beat_ = -1;
    hr_inst_ = 0;
    ne_ = ach_ = 0;
    spikes_ = 0;
    n_ = in_ = 0;
    map_min_ = 1e9;
    map_max_ = -1e9;
    run_start_ = -1;
    cur_seg_ = -1;
    recov_ = { -1, -1, -1 };
    tail_in_ = { 0, 0, 0 };
    tail_n_ = { 0, 0, 0 };
    next_sample_ = 0;
    hr_sum_ = hr_n_ = 0;
    motor_.invalidate();
    wiring_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    e.set_input( "baroreceptor", heart_.baro_mean_hz );
    e.step( dt );
    ne_ = e.modulator( "NE" );
    ach_ = e.modulator( "ACh" );
    int n = static_cast< int >( std::lround( motor_.spike_count( e, "sa_beat", dt ) ) );
    spikes_ += n;
    int ticks = std::max( 1, static_cast< int >( std::ceil( dt - 1e-9 ) ) );
    double h = dt / ticks;
    for ( int i = 0; i < ticks; ++i )
    {
      body::HeartInput in;
      in.sa_spikes = i == 0 ? n : 0;
      in.sa_rate_hz = 0;
      double sym = clampd( 0.2 + kSymGain * ( ne_ - 0.2 ), 0, 1 );
      in.sym_cardiac = in.sym_vascular = sym;
      in.vagal = 0.6;
      in.standing = ( t_ >= kStand && t_ < kLie ) ? 1.0 : 0.0;
      in.haemorrhage_ml_s = ( t_ >= kHaem0 && t_ < kHaem1 ) ? kHaemRate : 0.0;
      heart_.step( h, in );
      t_ += h;
      if ( heart_.beat_now )
      {
        beats_.push_back( t_ );
        last_beat_ = t_;
      }
      double since = last_beat_ < 0 ? 1e9 : t_ - last_beat_;
      double rr = beats_.size() >= 2 ? beats_.back() - beats_[ beats_.size() - 2 ] : 1e9;
      double iv = std::max( rr, since );
      hr_inst_ = iv >= 1e8 ? 0.0 : 60000.0 / iv;
    }
    if ( t_ >= next_sample_ )
    {
      next_sample_ += 100;
      sample();
    }
  }

  BodyState body() const override
  {
    BodyState b;
    b.points = { { 0, 0 } };
    b.readouts = heart_.readouts();
    auto v = heart_.view();
    b.readouts[ "hr_bpm" ] = hr_inst_;
    b.readouts[ "NE" ] = ne_;
    b.readouts[ "ACh" ] = ach_;
    b.readouts[ "standing" ] = ( t_ >= kStand && t_ < kLie ) ? 1.0 : 0.0;
    b.readouts[ "haemorrhage_ml_s" ] = ( t_ >= kHaem0 && t_ < kHaem1 ) ? kHaemRate : 0.0;
    b.readouts[ "heart_phase" ] = v.phase;
    b.readouts[ "ejection" ] = v.ejection;
    b.readouts[ "beat" ] = v.beat ? 1.0 : 0.0;
    b.readouts[ "beats" ] = heart_.beats;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    double frac = n_ > 0 ? in_ / n_ : 0.0;
    s.parts[ "in_band" ] = frac;
    s.parts[ "map_min_mmHg" ] = n_ > 0 ? map_min_ : 0.0;
    s.parts[ "map_max_mmHg" ] = n_ > 0 ? map_max_ : 0.0;
    const char* nm[ 3 ] = { "settle_stand_s", "settle_lie_s", "settle_haem_s" };
    const char* tn[ 3 ] = { "tail_stand", "tail_lie", "tail_haem" };
    const double start[ 3 ] = { kStand, kLie, kHaem0 }, end[ 3 ] = { kLie, kHaem0, kDuration };
    double worst_settle = 0, worst_tail = 1;
    for ( int i = 0; i < 3; ++i )
    {
      bool reached = t_ >= start[ i ];
      double st = recov_[ i ] >= 0 ? std::max( 0.0, recov_[ i ] - kRecoverHold - start[ i ] ) / 1000.0
                                   : ( reached ? ( std::min( t_, end[ i ] ) - start[ i ] ) / 1000.0 : 0.0 );
      double tl = tail_n_[ i ] > 0 ? tail_in_[ i ] / tail_n_[ i ] : ( reached ? 0.0 : 1.0 );
      s.parts[ nm[ i ] ] = st;
      s.parts[ tn[ i ] ] = tl;
      worst_settle = std::max( worst_settle, st );
      worst_tail = std::min( worst_tail, tl );
    }
    s.parts[ "hr_mean_bpm" ] = hr_n_ > 0 ? hr_sum_ / hr_n_ : 0.0;
    s.parts[ "spikes" ] = spikes_;
    s.value = 100.0 * frac;
    bool done = t_ >= kDuration;
    bool tails = worst_tail >= 0.8;
    s.passed = done && frac >= 0.85 && worst_settle <= 15.0 && tails;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 0 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( spikes_ < 1 )
      o << "No beats: sa_beat has not fired" << yet << " Add the SA-node cell and read it out as sa_beat.";
    else if ( !done )
      o << "MAP " << heart_.map_smooth << " mmHg so far, " << 100 * frac << " % of the time in 80-100.";
    else if ( passed_all_but( frac, worst_settle, tails ) )
      o << "Pressure held: MAP " << s.parts[ "map_min_mmHg" ] << "-" << s.parts[ "map_max_mmHg" ] << " mmHg, " << 100 * frac
        << " % of the time in 80-100.";
    else if ( ne_peak_ < 0.05 )
      o << "No sympathetic output: pressure sits near " << map_mean() << " mmHg (sympathetic tone is part of normal pressure) and falls "
        << "when upright; MAP reached " << s.parts[ "map_min_mmHg" ] << ".";
    else if ( map_mean() > kHi )
      o << "Pressure runs high: mean " << map_mean() << " mmHg, up to " << s.parts[ "map_max_mmHg" ]
        << ". Sympathetic output is not being turned down by pressure (is the baroreceptor input reaching CVLM, and CVLM inhibiting RVLM?).";
    else if ( s.parts[ "map_min_mmHg" ] < kLo - 10 )
      o << "Pressure falls to " << s.parts[ "map_min_mmHg" ] << " mmHg and the reflex does not catch it ("
        << 100 * frac << " % in band): the response is too weak or too slow.";
    else if ( s.parts[ "map_max_mmHg" ] > kHi + 15 )
      o << "Pressure overshoots to " << s.parts[ "map_max_mmHg" ] << " mmHg: the reflex over-corrects.";
    else
      o << "Pressure leaves 80-100 mmHg too often: " << 100 * frac << " % in band, settling takes up to " << worst_settle
        << " s (needs 85 % and 15 s or less).";
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { -1, -1, 1, 1 }; }

private:
  static bool passed_all_but( double frac, double worst, bool tails ) { return frac >= 0.85 && worst <= 15.0 && tails; }

  double map_mean() const { return n_ > 0 ? map_sum_ / n_ : 0.0; }

  void sample()
  {
    ne_peak_ = std::max( ne_peak_, ne_ );
    if ( t_ < kSkip )
      return;
    double m = heart_.map_smooth;
    bool ok = m >= kLo && m <= kHi;
    n_ += 1;
    in_ += ok ? 1 : 0;
    map_sum_ += m;
    map_min_ = std::min( map_min_, m );
    map_max_ = std::max( map_max_, m );
    hr_sum_ += hr_inst_;
    hr_n_ += 1;
    int seg = t_ >= kHaem0 ? 2 : ( t_ >= kLie ? 1 : ( t_ >= kStand ? 0 : -1 ) );
    if ( seg != cur_seg_ )
    {
      cur_seg_ = seg;
      run_start_ = -1;  // a new segment restarts the in-band run
    }
    if ( seg >= 0 )
    {
      if ( ok )
      {
        if ( run_start_ < 0 )
          run_start_ = t_;
        if ( recov_[ seg ] < 0 && t_ - run_start_ >= kRecoverHold )
          recov_[ seg ] = t_;
      }
      else
        run_start_ = -1;
      double seg_end = seg == 0 ? kLie : ( seg == 1 ? kHaem0 : kDuration );
      if ( t_ >= seg_end - 10000 )
      {
        tail_n_[ seg ] += 1;
        tail_in_[ seg ] += ok ? 1 : 0;
      }
    }
  }

  body::Heart heart_;
  double t_ = 0, last_beat_ = -1, hr_inst_ = 0, ne_ = 0, ach_ = 0, spikes_ = 0, n_ = 0, in_ = 0, map_min_ = 1e9, map_max_ = -1e9,
         map_sum_ = 0, next_sample_ = 0, hr_sum_ = 0, hr_n_ = 0, ne_peak_ = 0;
  double run_start_ = -1;
  int cur_seg_ = -1;
  std::array< double, 3 > recov_{ -1, -1, -1 }, tail_in_{ 0, 0, 0 }, tail_n_{ 0, 0, 0 };
  std::vector< double > beats_;
  MotorReader motor_;
  Wiring wiring_;
};

} // namespace

std::unique_ptr< Task > make_baroreflex() { return std::make_unique< BaroreflexTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( baroreflex, 220, "baroreflex.json", make_baroreflex ) }
