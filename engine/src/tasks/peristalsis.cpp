// Task "peristalsis": an enteric nervous system circuit moves a bolus down a chain of 8 gut segments (the law of the intestine).
//
// WORLD (spike::body::Gut, docs/body/gut.md; 1 ms slices, game time = real time)
//   Segments 0 (oral) .. 7 (aboral). The bolus (length 1.2 segments) starts at position 1.0 (segment units) and must reach 7.0 (the end of the
//   tube is at 7.4, the centre can go no further). It moves only by the luminal pressure difference between its oral and aboral sides.
//   Sensors "dist0".."dist7": distension of segment i = Poisson rate kDistHz (500 Hz) x overlap(bolus, segment i), 0..1 (an intrinsic primary
//                     afferent neuron fires when the bolus stretches its segment).
//   Motors  "ach0".."ach7": excitatory motoneuron pool of segment i, mean Hz / kExRef (45 Hz), lowpass 300 ms (ACh / substance P) -> Gut ex_i. REQUIRED.
//           "no0".."no7": inhibitory motoneuron pool of segment i, mean Hz / kInhRef (50 Hz), lowpass 300 ms (NO / VIP) -> Gut inh_i. OPTIONAL: without
//                     them there is no relaxation ahead of the bolus (a simpler, slower gut).
//           "icc0".."icc7": interstitial cells of Cajal of segment i (icc_slow_wave: one spike = one slow-wave onset). OPTIONAL: if the circuit declares
//                     none of them, the gut runs its own built-in 12 /min slow wave (so a first circuit needs only dist -> ach). If it declares them,
//                     they ARE the clock (silence them and nothing contracts). Each spike starts a
//           depolarising slow-wave plateau, half a sine of 2.5 s (the depolarising half of a 12 /min wave), amplitude 0.4, which the gut adds to the
//           membrane drive (m = ex - inh + 0.4 sw; the body's own built-in slow wave is switched off, the ICC cells of the circuit ARE the clock).
//   Contraction needs m above 0.6 (threshold), so slow waves alone and weak excitation alone do nothing: ICC timing gates propulsion.
//
// MECHANISM (Engine::structure(), checked at the start and every 0.5 s): the law of the intestine needs both chains, an ascending EXCITATORY one (an
//   excitatory path from the dist sensors to the cells ach0..7 read) and a descending INHIBITORY one (an excitatory path from the dist sensors to
//   cells no0..7 read, which relax the gut ahead of the bolus), on separate cells. The simple build with ach0..7 alone is how the level starts
//   but is not the mechanism: it scores below the pass bar, and the verdict names the missing descending limb.
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
#include <map>
#include <sstream>
#include "spike/body/gut.h"
#include "mech_util.h"
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
           "contract. The stretch sensors are 'dist0..7' and the excitatory motor pools 'ach0..7'; the inhibitory pools 'no0..7' (ahead of the bolus) "
           "are read only if you declare them, and so are 'icc0..7' outputs (slow-wave pacemakers of your own, "
           "which then replace the built-in clock). Score: the bolus must reach the end of the tube within 25 s. ";
  }
  /// The cell type the reference circuit and the verdicts assume; the numbers of the build live in recipe() (shown by mode in the lab).
  std::string default_model() const override { return "iaf_psc_alpha"; }
  std::string recipe() const override
  {
    return "A recipe that works (iaf_psc_alpha groups of 8; "
           "pA; bias = ADVANCED I_e; drag a sensor or a group onto a numbered channel and the whole family is wired, neuron k to channel k, "
           "and in a link's inspector choose wiring 'neuron k to k' with a shift): dist0..7 onto S; S > AIN (bias 340) segment shift 0, weight "
           "100, delay 3 ms; AIN > EMN (bias 380, tau syn I 10 ms) segment shift -1 (each AIN cell to the pool just oral of it), weight 36; "
           "EMN onto ach0..7. That alone takes about 32 s, too slow: add the descending chain S > DIN (bias 340) segment shift 0, weight 100, "
           "delay 3 ms; DIN > IMN (bias 365) segment shift +1 (aboral), weight 100; IMN onto no0..7. The bolus then arrives in about 11 s.";
  }
  std::vector< std::string > sensor_channels() const override { return dist_ch_; }
  // Required: the eight excitatory pools. "no0..7" and "icc0..7" are optional: they are read only if the circuit declares them.
  std::vector< std::string > motor_channels() const override { return ach_ch_; }
  // sensors dist0..7 are Poisson trains (500 Hz when the bolus fills the segment); outputs are rate pools; icc*/no* are optional
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    std::map< std::string, ChannelKind > k;
    for ( int i = 0; i < kSeg; ++i )
    {
      k[ dist_ch_[ i ] ].kind = "poisson";
      k[ dist_ch_[ i ] ].value = kDistHz;
      k[ ach_ch_[ i ] ].kind = "rate";
    }
    return k;
  }
  double duration_ms() const override { return kDuration; }
  /// Quick check: the run up to just past this mode's deadline. The pass rule is "the bolus arrived by the deadline", so every circuit that arrives
  /// in time gets the same arrival time and the same score, and every circuit that does not is judged on its progress at the deadline (the full
  /// episode keeps going to 40 s, which can only change the partial score of a circuit that already failed).
  double quick_duration_ms() const override { return std::min( kDuration, deadline() + 1000.0 ); }
  std::string chapter() const override { return "autonomic"; }
  std::string view() const override { return "gut"; }
  int par_neurons() const override { return variant_par( variant_, 49 ); }

  // ---- v0.3 ----
  std::string level_kind() const override { return "task"; }
  std::vector< std::string > concepts_new() const override { return {}; }
  std::vector< std::string > concepts_used() const override { return { "travelling_wave", "excitation", "inhibition", "timed_bursts" }; }
  bool on_spine() const override { return true; }
  /// Pass bar: the bolus must arrive within 32 s (guided), 25 s (standard) or 18 s (researcher); the fast_transit variant asks for 15 s.
  /// The distension afferents are Poisson trains (500 Hz at full distension): their sampling noise is not scaled.
  std::map< std::string, std::string > variants() const override
  {
    return { { "half_neurons", "half the neurons: 25 instead of 49" },
             { "fast_transit", "the bolus must arrive within 15 s" } };
  }
  std::vector< Hint > hints() const override
  {
    Score s = score();
    std::vector< Hint > h;
    if ( s.parts[ "ach_spikes" ] < 1 || s.parts[ "net_distance" ] < -0.5 )
    {
      h.push_back( make_hint( 1, "Travelling wave: each segment's stretch must excite the muscle just behind the bolus, so the squeeze moves the food on.", "" ) );
      h.push_back( make_hint( 2, "The excitatory motor pools are silent or push the wrong way.", "ach0" ) );
      h.push_back( make_hint( 3, "Relay the stretch sensors through excitatory interneurons to motoneurons shifted one segment toward the mouth, with standing drive on the relays so they fire.", "" ) );
    }
    else if ( !mech_.ok() || s.parts[ "no_spikes" ] < 1 )
    {
      h.push_back( make_hint( 1, "Polarity: squeeze behind, open ahead. The segments ahead of the bolus must relax, or they resist it.", "" ) );
      h.push_back( make_hint( 2, "The inhibitory motor pools never fire: nothing relaxes the segments ahead.", "no0" ) );
      h.push_back( make_hint( 3, "Build a second chain from the stretch sensors through its own interneurons to motoneurons one segment toward the anus, read out as the no channels.", "" ) );
    }
    else
    {
      h.push_back( make_hint( 1, "The wave is there but slow. Timing between the ascending and descending chains sets the speed.", "" ) );
      h.push_back( make_hint( 2, "Shorten the delay between the stretch and the contraction behind it.", "ach0" ) );
      h.push_back( make_hint( 3, "Shorten the synaptic delay on the first links of both chains, and strengthen the link onto the excitatory motoneurons so they contract earlier.", "" ) );
    }
    return h;
  }
  std::map< std::string, Metric > metrics() const override
  {
    Score s = score();
    return { { "arrival_s", make_metric( s.parts[ "arrival_s" ] >= 0 ? s.parts[ "arrival_s" ] : kDuration / 1000.0, "s", true ) } };
  }
  Note notebook() const override
  {
    return { "The law of the intestine",
             "A bolus stretching the gut wall makes the muscle contract on the oral side and relax on the aboral side, so the "
             "contents move on. The enteric nervous system does this with ascending excitatory and descending inhibitory interneuron chains.",
             "Bayliss WM, Starling EH (1899) J Physiol 24:99; Furness JB (2012) Nat Rev Gastroenterol Hepatol 9:286" };
  }
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
    probed_ = has_no_ = has_icc_ = false;
    spikes_ex_ = spikes_inh_ = 0;
    motor_.invalidate();
    wiring_.invalidate();
    mech_.invalidate();
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, sensor_channels(), motor_channels() );
    mech_.refresh( e, t_, [ this ]( const Structure& st ) { return limbs_missing( st ); } );
    if ( !probed_ )
    {
      probed_ = true;
      has_no_ = has_icc_ = false;
      for ( const auto& o : e.outputs() )
      {
        if ( o.name == no_ch_[ 0 ] ) has_no_ = true;
        if ( o.name == icc_ch_[ 0 ] ) has_icc_ = true;
      }
    }
    const double half = 0.5 * gut_.params().bolus_len;
    for ( int i = 0; i < kSeg; ++i )
    {
      double lo = std::max( gut_.bolus_pos - half, double( i ) ), hi = std::min( gut_.bolus_pos + half, double( i + 1 ) );
      e.set_input( dist_ch_[ i ], kDistHz * clampd( hi - lo, 0, 1 ) );
    }
    e.step( dt );
    for ( int i = 0; i < kSeg; ++i )
    {
      double hx = motor_.hz_per_neuron( e, ach_ch_[ i ] ), hi = has_no_ ? motor_.hz_per_neuron( e, no_ch_[ i ] ) : 0.0;
      spikes_ex_ += motor_.spike_count( e, ach_ch_[ i ], dt );
      if ( has_no_ ) spikes_inh_ += motor_.spike_count( e, no_ch_[ i ], dt );
      ex_[ i ] = clampd( ex_lp_[ i ].step( hx / kExRef, dt ), 0, 1 );
      inh_[ i ] = clampd( inh_lp_[ i ].step( hi / kInhRef, dt ), 0, 1 );
      if ( has_icc_ && motor_.spike_count( e, icc_ch_[ i ], dt ) > 0.5 )
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
    in.slow_waves = !has_icc_;  // no ICC channels declared: the gut's own clock
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
    s.passed = arrived && arrival_ms_ <= deadline() && mech_.ok();
    if ( !mech_.ok() )
      s.value = std::min( s.value, mech::kNoMechanismCap );
    std::ostringstream o;
    o << std::fixed << std::setprecision( 1 );
    const char* yet = done ? "." : " yet.";
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !mech_.ok() )
      o << mech_.missing();
    else if ( s.passed )
      o << "The bolus reached the end in time.";
    else if ( arrived )
      o << "The bolus arrived, but too late."
        << ( !has_no_ ? " Add the 'no' inhibitory outputs (descending interneurons -> motoneurons ahead of the bolus): relaxing the segments ahead makes it much faster." : "" );
    else if ( spikes_ex_ < 1 )
      o << "The excitatory motoneurons (the 'ach' outputs) never fire" << yet << " Nothing squeezes behind the bolus.";
    else if ( has_icc_ && cycles < 1 )
      o << "No slow waves: the 'icc' cells never fire" << yet << " Without the ICC clock the muscle never reaches contraction threshold.";
    else if ( gut_.distance < -0.5 )
      o << "The bolus was pushed back toward the mouth: the wrong side is contracting.";
    else if ( !done )
      o << "The bolus is still on its way" << yet;
    else
      o << "The bolus did not get to the end."
        << ( !has_no_ ? " No 'no' outputs: nothing relaxes the segments ahead of the bolus (add inhibitory motoneurons from the descending interneurons)."
                   : ( spikes_inh_ < 1 ? " The inhibitory motoneurons (the 'no' outputs) never fire: nothing relaxes the segments ahead." : "" ) );
    s.verdict = o.str();
    return s;
  }

  std::array< double, 4 > world_bounds() const override { return { 0, 0, kSeg, 1 }; }

private:
  // the mechanism: ascending excitatory chain (dist -> ach) and descending chain (dist -> no), on separate cells
  std::string limbs_missing( const Structure& st ) const
  {
    mech::PopSet dist, ach, no;
    for ( int i = 0; i < kSeg; ++i )
    {
      dist = mech::unite( dist, mech::in_pops( st, dist_ch_[ i ] ) );
      ach = mech::unite( ach, mech::out_pops( st, ach_ch_[ i ] ) );
      no = mech::unite( no, mech::out_pops( st, no_ch_[ i ] ) );
    }
    if ( !mech::path( st, dist, ach, +1 ) )
      return "No ascending excitatory chain: the stretch sensors (the 'dist' inputs) must excite, through interneurons, the motoneurons that squeeze behind the bolus (the 'ach' outputs).";
    if ( no.empty() )
      return "No descending inhibitory chain: nothing relaxes the gut ahead of the bolus. Add a second chain from the stretch sensors to inhibitory motoneurons, read out as the 'no' outputs: squeeze behind, open ahead.";
    if ( !mech::path( st, dist, no, +1 ) )
      return "The inhibitory motoneurons (the 'no' outputs) are not driven by the stretch sensors: the descending chain must carry the distension signal to them.";
    if ( mech::overlap( ach, no ) )
      return "The excitatory and the inhibitory motor outputs read the same cells: the ascending and the descending chain need separate motoneurons.";
    return "";
  }

  double deadline() const { return variant_ == "fast_transit" ? 15000 : ( mode_ == "guided" ? 32000 : ( mode_ == "researcher" ? 18000 : kDeadline ) ); }
  double t_ = 0, arrival_ms_ = -1, spikes_ex_ = 0, spikes_inh_ = 0;
  bool probed_ = false, has_no_ = false, has_icc_ = false;
  body::Gut gut_;
  std::vector< std::string > dist_ch_, ach_ch_, no_ch_, icc_ch_;
  std::vector< double > ex_, inh_, sw_start_, sw_;
  std::vector< Lowpass > ex_lp_, inh_lp_;
  std::vector< int > icc_spikes_;
  MotorReader motor_;
  Wiring wiring_;
  mech::Gate mech_;
};

} // namespace

std::unique_ptr< Task > make_peristalsis() { return std::make_unique< PeristalsisTask >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( peristalsis, 410, "peristalsis.json", make_peristalsis ) }
