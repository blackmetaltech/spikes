// Micro-puzzle "m_excitation" (concept 2: excitation, weight and delay). Relay every spike of a source cell to an echo cell, 8 ms later.
//
// WORLD (3000 ms; the engine is stepped in 0.1 ms slices inside every 1 ms task step so latencies are measured to 0.1 ms)
//   Sensor "tap" (dc, level x sensor weight in pA): 5 taps of level 2.0 and 14 ms length; tap k starts at 300 + 500 k + U(0, 100) ms.
//   At the lab's default sensor weight (300 pA) a tap gives 600 pA: an iaf cell fires one or two spikes per tap.
//   Motors "source" (the cell the tap drives) and "echo" (the cell that must repeat its spikes). Both are read as spike times.
// SCORING (every echo spike is paired with the nearest earlier source spike; an echo needs 0.5-25 ms latency)
//   echo_frac : fraction of source spikes that got an echo                                 needs >= 0.9 (guided 0.8)
//   latency_ms: mean echo latency                                                         needs 8 +- 1.5 ms (guided +- 2.25, researcher +- 1.05)
//   in_band   : fraction of source spikes whose echo is inside that band                   needs >= 0.9 (guided 0.8)
//   extra     : echo spikes with no source spike in the 25 ms before                       needs <= 1 (guided <= 3)
//   source_spikes >= 5 (the tap must drive the source). value 100 when passed, else 85 * (criteria met / 4).
// A one-to-one relay needs a strong synapse: the default 80 pA gives about 1.3 mV, the next cell needs 15 mV, so about 1400 pA or more.
// With 1500 pA the echo crosses threshold about 3.7 ms after the source spike with the lab's 2 ms synaptic delay (alpha-shaped current,
// 2 ms rise), so the delay slider has to add about 4.3 ms to reach 8 ms.
//
// Biology: a chemical synapse delays a spike by 0.5-4 ms (synaptic delay: vesicle release, diffusion, receptor opening; Katz & Miledi
// 1965 Proc R Soc B 161:483), and the postsynaptic cell fires only if one or more EPSPs bring it to threshold: at the neuromuscular
// junction one spike does (safety factor), in the cortex it takes many.
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;
using micro::Graph;

constexpr double kDur = 3000, kTapLen = 14, kLevel = 2.0, kTarget = 8.0, kMaxLat = 25.0, kSub = 0.1;
constexpr int kTaps = 5;

class MExcitation : public MicroBase
{
public:
  MExcitation() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_excitation";
    i.title = "Excitation: weight and delay";
    i.brief = "An excitatory synapse does two things: its weight decides whether a spike gets through, and its delay decides when. "
              "Five taps arrive on the 'tap' input; each makes a first cell fire a spike or two, which you read on 'source'. Build a "
              "second cell that repeats every one of those spikes, 8 ms later (within 1.5 ms), and read it on 'echo'. A single spike "
              "through a weak synapse is a tiny nudge, far from enough to make a cell fire; the delay of the link adds to the time the "
              "second cell needs to reach threshold. Score: echoes for at least 90 % of the source spikes, mean latency 8 +- 1.5 ms, "
              "at most one echo with no source spike before it.";
    i.chapter = "invertebrates";
    i.sensors = { "tap" };
    i.motors = { "source", "echo" };
    i.duration_ms = kDur;
    i.concepts_new = { "excitation" };
    i.concepts_used = { "threshold" };
    i.par_neurons = 2;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "tap > cell A (the default weights work) > cell B. A is wired to 'source', B to 'echo'. Raise the A > B weight to about "
               "1500 pA so that one spike fires B, then lengthen the delay to about 4 or 5 ms (it starts at 2) until the echo latency "
               "reads 8 ms.";
    i.kinds = { { "tap", micro::dc( 2.0 ) }, { "source", micro::rate() }, { "echo", micro::rate() } };
    i.bands = { { "latency_ms", "ms", kTarget - 1.5, kTarget + 1.5 }, { "echo_pct", "%", 90, 100 } };
    for ( int k = 0; k < kTaps; ++k ) i.timeline.push_back( { 300.0 + 500.0 * k, 100.0, "tap" } );
    i.note = { "Synaptic delay",
      "A chemical synapse is not instant: from the arrival of the spike in the terminal to the first current in the next cell takes "
      "about half a millisecond at the frog neuromuscular junction at room temperature, and 1-4 ms at many central synapses. Katz and "
      "Miledi showed that most of it is the time for calcium to trigger release, not the diffusion of the transmitter. The delay "
      "between two neurons in a chain is this delay plus the time the second cell needs to rise to threshold.",
      "Katz B, Miledi R (1965) The measurement of synaptic delay, and the time course of acetylcholine release at the neuromuscular "
      "junction. Proc R Soc Lond B 161:483-495" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    for ( int k = 0; k < kTaps; ++k ) onset_[ k ] = 300.0 + 500.0 * k + rng_.uniform( 0, 100 );
    src_.clear();
    ech_.clear();
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    level_ = 0;
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, info_.sensors, info_.motors );
    inspect_once( e );
    const int n = std::max( 1, static_cast< int >( std::lround( dt / kSub ) ) );
    const double h = dt / n;
    for ( int i = 0; i < n; ++i )
    {
      drive( e, h );
      e.step( h );
      t_ += h;
      observe( e, h );
    }
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A synapse has two numbers. The weight decides whether one spike is enough to make the next cell fire (a lone spike through "
                  "a weak synapse is a tiny nudge); the delay decides when the next cell answers.", "" },
      { 2, "Look at the link from the first cell to the second: its weight and its delay. The second cell is the one wired to 'echo', and "
           "it must be driven by the first cell's spikes, not by the tap.", "echo" },
      { 3, "tap > cell A > cell B, with A wired to 'source' and B to 'echo'. Make the A > B weight many times the default, until every "
           "spike of A fires B. Then use the delay of that link to move the echo: a longer delay moves it later, and the verdict tells you "
           "whether it is early or late.", "echo" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    Stats st = stats();
    return { { "latency_error_ms", { st.echoes > 0 ? std::fabs( st.mean_lat - kTarget ) : 99.0, "ms", true } } };
  }

  void inspect( Engine& ) override
  {
    const auto src = g_.out_pops( "source" ), ech = g_.out_pops( "echo" );
    if ( src.empty() || ech.empty() ) return;
    if ( Graph::overlap( src, ech ) )
      mech_ = "'source' and 'echo' are read from the same cell: an echo is a second cell repeating the first.";
    else if ( !g_.path( src, ech, +1, 3 ) )
      mech_ = "No excitatory synapse carries the source cell's spikes to the echo cell: an echo is a second cell fired by the first, through a link "
              "that excites. (Driving both from the tap is not an echo of the source.)";
  }

  Score score() const override
  {
    Score s;
    Stats st = stats();
    const double tw = 1.5 * tol(), need = mode_ == "guided" ? 0.8 : 0.9, max_extra = mode_ == "guided" ? 3 : 1;
    s.parts[ "source_spikes" ] = st.src;
    s.parts[ "echo_spikes" ] = st.echoes;
    s.parts[ "echo_frac" ] = st.src > 0 ? st.echoes / st.src : 0.0;
    s.parts[ "latency_ms" ] = st.echoes > 0 ? st.mean_lat : -1.0;
    s.parts[ "in_band" ] = st.src > 0 ? st.in_band / st.src : 0.0;
    s.parts[ "extra" ] = st.extra;
    const bool a = st.src >= 5, b = st.src > 0 && st.echoes / st.src >= need, c = st.src > 0 && st.in_band / st.src >= need, d = st.extra <= max_extra;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ) + ( c ? 1 : 0 ) + ( d ? 1 : 0 ), 4 );
    if ( !done() )
      s.passed = false;
    apply_mech( s );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running.";
    else if ( !a )
      o << "The source cell hardly fired: the taps are too weak to drive it. Strengthen the tap input.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( st.echoes == 0 )
      o << "The echo cell never fired. One spike through a weak synapse is only a nudge, far from enough to bring this cell to threshold: "
           "strengthen the link from the source cell to the echo cell a great deal.";
    else if ( !b )
      o << "The echo answers only some of the source spikes. Strengthen the link so every spike gets through.";
    else if ( !c && st.mean_lat < kTarget - tw )
      o << "Echo too early. Lengthen the delay of the link.";
    else if ( !c )
      o << "Echo too late. Shorten the delay of the link.";
    else if ( !d )
      o << "Echo spikes with no source spike before them: the echo cell must only repeat the source.";
    else
      o << "Every source spike was echoed, on time.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    level_ = 0;
    for ( int k = 0; k < kTaps; ++k )
      if ( t_ >= onset_[ k ] && t_ < onset_[ k ] + kTapLen ) level_ = kLevel * gain_;
    e.set_input( "tap", level_ );
  }

  void observe( Engine& e, double dt ) override
  {
    if ( motor_.spike_count( e, "source", dt ) > 0.5 ) src_.push_back( t_ );
    if ( motor_.spike_count( e, "echo", dt ) > 0.5 ) ech_.push_back( t_ );
    Stats st = stats();
    readouts_[ "tap" ] = level_;
    readouts_[ "source_spikes" ] = st.src;
    readouts_[ "echo_spikes" ] = st.echoes;
    readouts_[ "latency_ms" ] = st.echoes > 0 ? st.mean_lat : 0.0;
    readouts_[ "echo_pct" ] = st.src > 0 ? 100.0 * st.echoes / st.src : 0.0;
  }

private:
  struct Stats
  {
    double src = 0, echoes = 0, in_band = 0, extra = 0, coincident = 0, mean_lat = 0;
  };

  Stats stats() const
  {
    Stats st;
    st.src = static_cast< double >( src_.size() );
    const double tw = 1.5 * tol();
    std::vector< double > first( src_.size(), -1.0 );
    for ( double b : ech_ )
    {
      bool same = false;
      int idx = -1;
      for ( int i = static_cast< int >( src_.size() ) - 1; i >= 0; --i )
      {
        if ( std::fabs( src_[ i ] - b ) < 0.15 ) same = true;
        if ( idx < 0 && src_[ i ] < b - 0.15 ) idx = i;
      }
      if ( same )
      {
        st.coincident += 1;  // the same cell is wired to both channels
        continue;
      }
      if ( idx < 0 || b - src_[ idx ] > kMaxLat )
      {
        st.extra += 1;
        continue;
      }
      if ( first[ idx ] < 0 ) first[ idx ] = b - src_[ idx ];
    }
    double sum = 0;
    for ( double l : first )
      if ( l >= 0 )
      {
        st.echoes += 1;
        sum += l;
        if ( std::fabs( l - kTarget ) <= tw + 1e-9 ) st.in_band += 1;
      }
    st.mean_lat = st.echoes > 0 ? sum / st.echoes : 0.0;
    return st;
  }

  double onset_[ kTaps ] = {};
  std::vector< double > src_, ech_;
  double gain_ = 1, level_ = 0;
};

} // namespace

std::unique_ptr< Task > make_m_excitation() { return std::make_unique< MExcitation >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_excitation, 8, "m_excitation.json", make_m_excitation ) }
