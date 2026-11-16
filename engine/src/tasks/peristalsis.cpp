// Task "peristalsis": an enteric nervous system circuit moves a bolus down a chain of 8 gut segments (the law of the intestine).
//
// WORLD (spike::body::Gut, docs/body/gut.md; 1 ms slices, game time = real time)
//   Segments 0 (oral) .. 7 (aboral). The bolus (length 1.2 segments) starts at position 1.0 (segment units) and must reach 7.0 (the end of the
//   tube is at 7.4, the centre can go no further). It moves only by the luminal pressure difference between its oral and aboral sides.
//   Sensors "dist0".."dist7": distension of segment i = Poisson rate kDistHz (500 Hz) x overlap(bolus, segment i), 0..1 (an intrinsic primary
//                     afferent neuron fires when the bolus stretches its segment).
//   Motors  "ach0".."ach7": excitatory motoneuron pool of segment i, mean Hz / kExRef (45 Hz), lowpass 300 ms (ACh / substance P) -> Gut ex_i.
//           "no0".."no7": inhibitory motoneuron pool of segment i, mean Hz / kInhRef (50 Hz), lowpass 300 ms (NO / VIP) -> Gut inh_i.
//           "icc0".."icc7": interstitial cells of Cajal of segment i (icc_slow_wave: one spike = one slow-wave onset). Each spike starts a
//           depolarising slow-wave plateau, half a sine of 2.5 s (the depolarising half of a 12 /min wave), amplitude 0.4, which the gut adds to the
//           membrane drive (m = ex - inh + 0.4 sw; the body's own built-in slow wave is switched off, the ICC cells of the circuit ARE the clock).
//   Contraction needs m above 0.6 (threshold), so slow waves alone and weak excitation alone do nothing: ICC timing gates propulsion.
//
// SCORING
//   progress = (bolus - 1) / 6, capped 0..1.  The bolus "arrives" the first time its centre passes 7.0.
//   value = 100 * (0.8 * progress + 0.2 * time bonus), time bonus = 1 if arrival by 25 s, falling to 0 at 40 s.
//   pass  : the bolus has arrived by 25 s (episode is 40 s).
//
// Biology: Bayliss & Starling 1899 (law of the intestine: contraction oral and relaxation aboral to a distension), reviewed by Costa, Brookes &
// Hennig 2000 Gut 47:iv15 and Furness 2012 Nat Rev Gastroenterol Hepatol 9:286 (peristaltic reflex: IPANs, ascending excitatory interneurons -> ACh/SP motoneurons
// oral, descending inhibitory interneurons -> NO/VIP motoneurons aboral); slow waves of the interstitial cells of Cajal gate phasic contractions
// (Sanders 1996 Gastroenterology 111:492; Huizinga & Lammers 2009 Am J Physiol 296:G1). Rates and gains here are game values.
#include <cmath>
#include <iomanip>
#include <sstream>
#include "spike/body/gut.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{
namespace
{

constexpr int kSeg = 8;
constexpr double kDuration = 40000, kDeadline = 25000, kStart = 1.0, kGoal = 7.0;
constexpr double kDistHz = 500, kExRef = 45, kInhRef = 50, kChemTau = 300, kSwMs = 2500, kSwAmp = 0.4;

class PeristalsisTask : public Task
{
public:
  PeristalsisTask()
  {
    for ( int i = 0; i < kSeg; ++i )
    {
      dist_ch_.push_back( "dist" + std::to_string( i ) );
      ach_ch_.push_back( "ach" + std::to_string( i ) );
      no_ch_.push_back( "no" + std::to_string( i ) );
      icc_ch_.push_back( "icc" + std::to_string( i ) );
    }
    reset( 1 );
  }
  std::string id() const override { return "peristalsis"; }
  std::string title() const override { return "Peristalsis"; }
  std::string brief() const override
  {
    return "The gut moves food by itself. When a bolus stretches the wall, sensory neurons of the enteric nervous system fire, and two "
           "interneuron chains answer: an ascending one excites the motoneurons just oral (behind) the bolus, which release ACh and contract, "
           "and a descending one excites inhibitory motoneurons aboral (ahead), which release NO and VIP and relax. That is the law of the "
           "intestine: squeeze behind, open ahead. Interstitial cells of Cajal run a slow wave (12 per minute) that decides when the muscle can "
           "contract. Build the chain for 8 segments: 'dist0..7' (stretch sensors), 'ach0..7' (excitatory motor pools), 'no0..7' (inhibitory "
           "motor pools) and 'icc0..7' (slow-wave pacemakers). Score: the bolus must reach the end of the tube within 25 s.";
  }
  std::vector< std::string > sensor_channels() const override { return dist_ch_; }
  std::vector< std::string > motor_channels() const override
  {
    std::vector< std::string > m;
    for ( int i = 0; i < kSeg; ++i ) m.push_back( ach_ch_[ i ] );
    for ( int i = 0; i < kSeg; ++i ) m.push_back( no_ch_[ i ] );
    for ( int i = 0; i < kSeg; ++i ) m.push_back( icc_ch_[ i ] );
    return m;
  }
  double duration_ms() const override { return kDuration; }
  std::string chapter() const override { return "autonomic"; }
  std::string view() const override { return "gut"; }
  int par_neurons() const override { return 49; }
  std::vector< Event > timeline() const override { return { { 0, 2000, "bolus enters" } }; }
  std::vector< Band > bands() const override { return { { "bolus", "frac", 0.0, 1.0 } }; }
  std::vector< std::string > modulators() const override { return { "ACh" }; }

  void reset( uint64_t ) override
  {
    t_ = 0;
    body::GutParams p;
    p.n_segments = kSeg;
    gut_ = body::Gut( p );
    gut_.reset( kStart );
    ex_.assign( kSeg, 0.0 );
    inh_.assign( kSeg, 0.0 );
    ex_lp_.assign( kSeg, Lowpass{} );
    inh_lp_.assign( kSeg, Lowpass{} );
    for ( auto& l : ex_lp_ ) l.tau_ms = kChemTau;
    for ( auto& l : inh_lp_ ) l.tau_ms = kChemTau;
    sw_start_.assign( kSeg, -1e9 );
    sw_.assign( kSeg, 0.0 );
    icc_spikes_.assign( kSeg, 0 );
    arrival_ms_ = -1;
    spikes_ex_ = spikes_inh_ = 0;
    motor_.invalidate();
    wiring_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    const double half = 0.5 * gut_.params().bolus_len;
    for ( int i = 0; i < kSeg; ++i )
    {
      double lo = std::max( gut_.bolus_pos - half, double( i ) ), hi = std::min( gut_.bolus_pos + half, double( i + 1 ) );
      e.set_input( dist_ch_[ i ], kDistHz * clampd( hi - lo, 0, 1 ) );
    }
    e.step( dt );
    for ( int i = 0; i < kSeg; ++i )
    {
      double hx = motor_.hz_per_neuron( e, ach_ch_[ i ] ), hi = motor_.hz_per_neuron( e, no_ch_[ i ] );
      spikes_ex_ += motor_.spike_count( e, ach_ch_[ i ], dt );
      spikes_inh_ += motor_.spike_count( e, no_ch_[ i ], dt );
      ex_[ i ] = clampd( ex_lp_[ i ].step( hx / kExRef, dt ), 0, 1 );
      inh_[ i ] = clampd( inh_lp_[ i ].step( hi / kInhRef, dt ), 0, 1 );
      if ( motor_.spike_count( e, icc_ch_[ i ], dt ) > 0.5 )
      {
        sw_start_[ i ] = t_;
        ++icc_spikes_[ i ];
      }
      double ph = ( t_ - sw_start_[ i ] ) / kSwMs;
      sw_[ i ] = ( ph >= 0 && ph <= 1 ) ? std::sin( kPi * ph ) : 0.0;
    }
    // the slow wave enters the gut as extra excitatory membrane drive (the gut's own clock is off)
    std::vector< double > ex_total( kSeg );
    for ( int i = 0; i < kSeg; ++i )
      ex_total[ i ] = ex_[ i ] + kSwAmp * sw_[ i ];
    body::GutInput in;
    in.excitatory = &ex_total;
    in.inhibitory = &inh_;
    in.slow_waves = false;
    gut_.step( dt, in );
    t_ += dt;
    if ( arrival_ms_ < 0 && gut_.bolus_pos >= kGoal )
      arrival_ms_ = t_;
  }

  BodyState body() const override
  {
    BodyState b;
    b.readouts[ "bolus" ] = gut_.bolus_pos / kSeg;
    b.readouts[ "bolus_pos" ] = gut_.bolus_pos;
    b.readouts[ "bolus_vel" ] = gut_.bolus_vel;
    for ( int i = 0; i < kSeg; ++i )
    {
      b.readouts[ "seg" + std::to_string( i ) ] = gut_.contraction[ i ];
      b.readouts[ "ex" + std::to_string( i ) ] = ex_[ i ];
      b.readouts[ "inh" + std::to_string( i ) ] = inh_[ i ];
      b.readouts[ "sw" + std::to_string( i ) ] = sw_[ i ];
    }
    double m = 0, mex = 0;
    for ( int i = 0; i < kSeg; ++i ) m += gut_.contraction[ i ], mex += ex_[ i ];
    b.readouts[ "gut_motility" ] = 100.0 * m / kSeg;
    b.readouts[ "ACh" ] = mex / kSeg;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  Score score() const override
  {
    Score s;
    double prog = clampd( ( gut_.bolus_pos - kStart ) / ( kGoal - kStart ), 0, 1 );
    bool arrived = arrival_ms_ >= 0;
    double bonus = arrived ? clampd( ( kDuration - arrival_ms_ ) / ( kDuration - kDeadline ), 0, 1 ) : 0.0;
    int cycles = 0;
    for ( int c : icc_spikes_ ) cycles = std::max( cycles, c );
    s.parts[ "bolus_pos" ] = gut_.bolus_pos;
    s.parts[ "progress" ] = prog;
    s.parts[ "arrival_s" ] = arrived ? arrival_ms_ / 1000.0 : -1.0;
    s.parts[ "net_distance" ] = gut_.distance;
    s.parts[ "ach_spikes" ] = spikes_ex_;
    s.parts[ "no_spikes" ] = spikes_inh_;
    s.parts[ "slow_waves" ] = cycles;
    s.value = 100.0 * ( 0.8 * prog + 0.2 * bonus );
    bool done = t_ >= kDuration;
    s.passed = arrived && arrival_ms_ <= kDeadline;
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( s.passed )
      o << "The bolus reached the end in " << arrival_ms_ / 1000.0 << " s.";
    else if ( arrived )
      o << "The bolus arrived, but after " << arrival_ms_ / 1000.0 << " s (limit 25 s).";
    else if ( spikes_ex_ < 1 )
      o << "The excitatory motoneurons (ach0..7) never fire" << yet << " Nothing squeezes behind the bolus.";
    else if ( cycles < 1 )
      o << "No slow waves: the icc0..7 cells never fire" << yet << " Without the ICC clock the muscle never reaches contraction threshold.";
    else if ( gut_.distance < -0.5 )
      o << "The bolus was pushed back toward the mouth (" << gut_.distance << " segments): the wrong side is contracting.";
    else if ( !done )
      o << "The bolus is at segment " << gut_.bolus_pos << " of 7" << yet;
    else
      o << "The bolus only got to segment " << gut_.bolus_pos << " of 7 (net " << gut_.distance << ")."
        << ( spikes_inh_ < 1 ? " The inhibitory motoneurons (no0..7) never fire: nothing relaxes the segments ahead." : "" );
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { 0, 0, kSeg, 1 }; }

private:
  double t_ = 0, arrival_ms_ = -1, spikes_ex_ = 0, spikes_inh_ = 0;
  body::Gut gut_;
  std::vector< std::string > dist_ch_, ach_ch_, no_ch_, icc_ch_;
  std::vector< double > ex_, inh_, sw_start_, sw_;
  std::vector< Lowpass > ex_lp_, inh_lp_;
  std::vector< int > icc_spikes_;
  MotorReader motor_;
  Wiring wiring_;
};

} // namespace

std::unique_ptr< Task > make_peristalsis() { return std::make_unique< PeristalsisTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( peristalsis, 410, "peristalsis.json", make_peristalsis ) }
