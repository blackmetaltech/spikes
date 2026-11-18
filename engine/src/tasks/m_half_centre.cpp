// Micro-puzzle "m_half_centre" (concept 7: mutual inhibition and the half-centre). Two cells that take turns: a flip-flop.
//
// WORLD (5000 ms; dc sensor "drive", level 2.0 x sensor weight in pA, on for the whole episode)
//   At t = 0 the task starts the left cells at V_m = -50 mV and the right cells at -75 mV (the command reaches the left side a moment
//   earlier), so that the symmetry between identical cells is broken the same way on every seed.
//   Motors "left" and "right": spikes of the cell(s) wired to each. The drive is a tonic, unpatterned command: the rhythm has to come from the circuit.
// SCORING over the analysis window (1000, 5000] ms, in 25 ms bins
//   a bin is "left" when left spikes > right spikes in it, "right" when right > left, empty when both are 0 (empty bins are skipped);
//   switches   : changes between left and right bins               needs >= 6 (3 full cycles; guided >= 4, researcher >= 8)
//   overlap    : bins in which both sides fire / bins in which either fires          needs <= 0.15 (guided 0.25, researcher 0.10)
//   balance    : smaller side's share of all spikes                  needs >= 0.25 (neither side may win for good)
//   period_ms  : window length / (switches / 2) (quality metric: the cycle period, reported)
//   value 100 when passed, else 85 * (criteria met / 3).
// With plain integrate-and-fire cells and mutual inhibition the circuit is a flip-flop that gets stuck: the side that fires first silences the
// other for good (winner takes all). A slow adaptation current in each cell is what ends a burst and releases the other side (concept 6).
//
// Biology: the lamprey spinal swimming CPG: excitatory interneurons drive ipsilateral motoneurons and commissural inhibitory interneurons that
// silence the other side (Grillner 2003 Nat Rev Neurosci 4:573); the half-centre idea is Brown's (1911): two mutually inhibiting centres, each
// tiring (adapting) while active, release the other.
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;

constexpr double kDur = 5000, kOn = 0, kLevel = 2.0, kW0 = 1000, kBin = 25;
constexpr int kBins = static_cast< int >( ( kDur - kW0 ) / kBin );

class MHalfCentre : public MicroBase
{
public:
  MHalfCentre() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_half_centre";
    i.title = "Half-centre";
    i.brief = "Swimming, walking and breathing are rhythms, and the simplest rhythm generator is two cells that inhibit each other and take "
              "turns: a half-centre. A steady 'drive' excites both sides equally. Build a left and a right cell, each inhibiting the "
              "other, and read them on 'left' and 'right': the output must switch from one side to the other at least 6 times in "
              "4 seconds, with both sides almost never firing together and neither side winning for good. Be careful: two plain cells "
              "that inhibit each other do not alternate, the first one to fire silences the other for ever. Something has to wear "
              "the active side out so the other can take over.";
    i.chapter = "invertebrates";
    i.sensors = { "drive" };
    i.motors = { "left", "right" };
    i.duration_ms = kDur;
    i.concepts_new = { "half_centre" };
    i.concepts_used = { "inhibition", "adaptation" };
    i.par_neurons = 2;
    i.default_model = "aeif_cond_alpha";
    i.recipe = "drive > L and drive > R (the default sensor weight 300 is right for the Adapting preset). L and R: one aeif cell each, "
               "Adapting preset, both inhibitory. L > R and R > L at the default weight (-8 nS), L > 'left' and R > 'right'. Each side then "
               "bursts for a few hundred milliseconds and gives way to the other. With the NEST-default aeif cell the drive has to be raised "
               "to about 400 pA of weight (its rheobase is about 620 pA).";
    i.kinds = { { "drive", micro::dc( kLevel ) }, { "left", micro::rate() }, { "right", micro::rate() } };
    i.bands = { { "switches", "switches", 6, 100 }, { "overlap_pct", "%", 0, 25 } };
    i.timeline = { { 0, kDur, "tonic drive" } };
    i.note = { "A rhythm from two cells that inhibit each other",
      "In 1911 T. Graham Brown proposed that the stepping rhythm of the cat spinal cord comes from two 'half-centres', one for flexion and "
      "one for extension, that inhibit each other and alternate. In the lamprey the same layout was found in the spinal cord: on each side "
      "excitatory interneurons drive motoneurons and inhibitory commissural interneurons that silence the opposite side, and a tonic "
      "command from the brainstem is enough to make the two sides alternate and the animal swim.",
      "Brown TG (1911) The intrinsic factors in the act of progression in the mammal. Proc R Soc Lond B 84:308-319; "
      "Grillner S (2003) The motor infrastructure: from ion channels to neuronal networks. Nat Rev Neurosci 4:573-586" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    for ( int b = 0; b < kBins; ++b ) l_[ b ] = r_[ b ] = 0;
    cur_l_ = cur_r_ = 0;
    total_l_ = total_r_ = 0;
    lvl_ = 0;
    kicked_ = false;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A half-centre needs two things: mutual inhibition, so that when one side fires the other is silenced, and a way for the "
                  "active side to tire (adapt) so that it lets go and the other side takes over. Without the second, one side wins for ever.", "" },
      { 2, "Each side is a cell wired to the other with an inhibitory link (set both cells to inhibitory). Use the adaptive exponential cell "
           "for both, with the Adapting preset: its adaptation is what ends the burst. Both cells get the drive.", "left" },
      { 3, "drive > L and drive > R (the default sensor weight 300 is right). L and R: one aeif cell each, Adapting preset, both inhibitory. "
           "L > R and R > L at the default weight (-8 nS), L > 'left' and R > 'right'. Each side then bursts for a few hundred milliseconds "
           "and gives way to the other.", "left" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    Stats st = stats();
    return { { "period_ms", { st.switches >= 2 ? ( kDur - kW0 ) / ( st.switches / 2.0 ) : 9999.0, "ms", true } } };
  }

  Score score() const override
  {
    Score s;
    Stats st = stats();
    const double sw_need = mode_ == "guided" ? 4 : ( mode_ == "researcher" ? 8 : 6 );
    const double ov_max = mode_ == "guided" ? 0.25 : ( mode_ == "researcher" ? 0.10 : 0.15 );
    s.parts[ "switches" ] = st.switches;
    s.parts[ "overlap_pct" ] = 100.0 * st.overlap;
    s.parts[ "balance" ] = st.balance;
    s.parts[ "left_spikes" ] = total_l_;
    s.parts[ "right_spikes" ] = total_r_;
    s.parts[ "period_ms" ] = st.switches >= 2 ? ( kDur - kW0 ) / ( st.switches / 2.0 ) : 0.0;
    const bool a = st.switches >= sw_need, b = st.overlap <= ov_max && total_l_ + total_r_ > 0, c = st.balance >= 0.25;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ) + ( c ? 1 : 0 ), 3 );
    if ( !done() )
      s.passed = false;
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running: " << static_cast< int >( st.switches ) << " switches so far.";
    else if ( total_l_ + total_r_ < 1 )
      o << "Nothing fired. The drive (2 x the sensor weight, in pA) must reach both cells and be over their rheobase: the NEST-default adaptive "
           "cell needs about 620 pA, which is more than the lab's 300 pA sensor weight gives. Raise the drive weight, or choose the Adapting preset.";
    else if ( total_l_ < 1 || total_r_ < 1 )
      o << "Only the " << ( total_l_ >= 1 ? "left" : "right" ) << " side fires: the other side is silenced for good (or never got the drive). A half-centre "
           "needs the drive on both cells and a way for the active side to let go: use adapting cells.";
    else if ( !c )
      o << "One side wins and keeps the other silent (" << fmt( 100.0 * st.balance ) << " % of the spikes on the weaker side). Each cell needs adaptation (the "
           "Adapting preset) so that the active side tires and lets the other go.";
    else if ( !b )
      o << "Both sides fire together " << fmt( 100.0 * st.overlap ) << " % of the time: they are not inhibiting each other enough. Check that both "
           "cells are inhibitory and connected to each other, and strengthen the links.";
    else if ( !a )
      o << "Only " << static_cast< int >( st.switches ) << " switches in 4 s (needs " << static_cast< int >( sw_need ) << "). The turns are too long: more adaptation "
           "makes the sides tire faster, or weaker inhibition lets the other side in sooner.";
    else
      o << "A half-centre: " << static_cast< int >( st.switches ) << " switches in 4 s (a cycle of about " << fmt( ( kDur - kW0 ) / ( st.switches / 2.0 ) )
        << " ms), " << fmt( 100.0 * st.overlap ) << " % overlap.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    if ( !kicked_ )
    {
      // The command arrives slightly earlier on the left: left cells start just over threshold (V_m -50 mV), right cells at -75 mV. Without
      // this, identical cells (and groups of them) can start in perfect step and a half-centre has no way to choose which side goes first.
      kicked_ = true;
      for ( const auto& o : e.outputs() )
      {
        if ( o.name != "left" && o.name != "right" ) continue;
        for ( int n : o.neurons ) e.set_neuron_param( n, "V_m", o.name == "left" ? -50.0 : -75.0 );
      }
    }
    lvl_ = t_ >= kOn ? kLevel : 0.0;
    e.set_input( "drive", lvl_ > 0 ? lvl_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
  }

  void observe( Engine& e, double dt ) override
  {
    const double nl = motor_.spike_count( e, "left", dt ), nr = motor_.spike_count( e, "right", dt );
    cur_l_ += nl;
    cur_r_ += nr;
    if ( t_ > kW0 )
    {
      const int b = std::min( kBins - 1, static_cast< int >( ( t_ - kW0 - 1e-9 ) / kBin ) );
      l_[ b ] += nl;
      r_[ b ] += nr;
      total_l_ += nl;
      total_r_ += nr;
    }
    // live readouts: the rates of the two sides over the last 100 ms
    ll_.tau_ms = lr_.tau_ms = 60;
    readouts_[ "left_hz" ] = ll_.step( nl / motor_.pool_size( e, "left" ) * 1000.0 / dt, dt );
    readouts_[ "right_hz" ] = lr_.step( nr / motor_.pool_size( e, "right" ) * 1000.0 / dt, dt );
    Stats st = stats();
    readouts_[ "switches" ] = st.switches;
    readouts_[ "overlap_pct" ] = 100.0 * st.overlap;
  }

private:
  struct Stats
  {
    double switches = 0, overlap = 0, balance = 0;
  };

  Stats stats() const
  {
    Stats st;
    int last = 0, active = 0, both = 0;
    for ( int b = 0; b < kBins; ++b )
    {
      if ( l_[ b ] > 0 || r_[ b ] > 0 ) ++active;
      if ( l_[ b ] > 0 && r_[ b ] > 0 ) ++both;
      int d = l_[ b ] > r_[ b ] ? 1 : ( r_[ b ] > l_[ b ] ? -1 : 0 );
      if ( d == 0 ) continue;
      if ( last != 0 && d != last ) st.switches += 1;
      last = d;
    }
    st.overlap = active > 0 ? static_cast< double >( both ) / active : 1.0;
    const double tot = total_l_ + total_r_;
    st.balance = tot > 0 ? std::min( total_l_, total_r_ ) / tot : 0.0;
    return st;
  }

  double gain_ = 1, l_[ kBins ] = {}, r_[ kBins ] = {}, cur_l_ = 0, cur_r_ = 0, total_l_ = 0, total_r_ = 0, lvl_ = 0;
  bool kicked_ = false;
  Lowpass ll_, lr_;
};

} // namespace

std::unique_ptr< Task > make_m_half_centre() { return std::make_unique< MHalfCentre >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_half_centre, 29, "m_half_centre.json", make_m_half_centre ) }
