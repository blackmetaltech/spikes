// Micro-puzzle "m_neuromodulation" (concept 10: neuromodulation). Slow a pacemaker with acetylcholine released by a nerve.
//
// WORLD (5000 ms; dc sensor "vagus", level 2.0 x sensor weight in pA, on from 1700 ms to the end; the task holds the external ACh and NE at 0)
//   Motor "beat": spikes of the pacemaker cell(s) wired to it. Motor "vagal": a modulator output (ACh, gain 0.012 per spike, tau 500 ms) that
//   releases acetylcholine from the spikes of whatever is wired to it: at steady state the level is gain * rate * tau (about 0.5 for one
//   iaf cell fed at the lab's default 300 pA/unit, which fires at 84 Hz). The engine makes every sa_node_pacemaker cell follow the ACh level
//   (circuit-wide, docs/CIRCUIT-FORMAT.md "Default modulation"): at ACh 0.5 its beat interval is 895 ms instead of 580 ms.
// SCORING
//   base_iv : mean beat interval in (100, 1700]  (needs >= 2 beats, i.e. the cell beats on its own)
//   eff_iv  : mean beat interval in (3000, 5000] (needs >= 2 beats: not stopped)
//   ratio   = eff_iv / base_iv      needs 1.25 <= ratio <= 3 (guided 1.15, researcher 1.35)
//   ach_peak: highest ACh level of the run      needs >= 0.15 (the slowing must come through the modulator, not through a synapse)
//   value 100 when passed, else 85 * (criteria met / 3).
// Biology: Loewi's 1921 experiment: stimulating the vagus of a frog heart releases a substance ('Vagusstoff', acetylcholine) that, carried
// in the perfusate, slows a second heart. ACh opens muscarinic K+ channels (I_KACh) and shifts the funny current in the node cell.
#include <algorithm>
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;

constexpr double kDur = 5000, kOn = 1700, kLevel = 2.0, kB0 = 100, kE0 = 3000;

class MNeuromodulation : public MicroBase
{
public:
  MNeuromodulation() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_neuromodulation";
    i.title = "Neuromodulation";
    i.brief = "Not every nerve works through a synapse. The vagus nerve releases acetylcholine (ACh) near the heart, and the chemical, not a "
              "spike, slows the pacemaker: it changes how the node cell behaves rather than telling it to fire. Here a 'vagus' signal "
              "comes on at 1.7 s. Build a cell the vagus drives, wire it to the 'vagal' output (its spikes release ACh) and put a "
              "pacemaker cell on 'beat', and make the heart slow by at least 25 % (but not stop). The pacemaker needs no wiring "
              "from your vagus cell: it follows the ACh level by itself. Score: the beat interval at the end, compared with the one "
              "before the vagus came on, and the ACh level that got there.";
    i.chapter = "heart";
    i.sensors = { "vagus" };
    i.motors = { "beat", "vagal" };
    i.duration_ms = kDur;
    i.concepts_new = { "neuromodulation" };
    i.concepts_used = { "pacemaking", "excitation" };
    i.par_neurons = 2;
    i.kinds = { { "vagus", micro::dc( kLevel ) }, { "beat", micro::rate() }, { "vagal", micro::modulator( "ACh", 0.012, 500.0 ) } };
    i.bands = { { "slowing", "x", 1.25, 3.0 }, { "ACh", "a.u.", 0.15, 1.5 } };
    i.timeline = { { kOn, kDur - kOn, "vagus on" } };
    i.modulators = { "ACh", "NE" };
    i.note = { "Vagusstoff",
      "In 1921 Otto Loewi stimulated the vagus nerve of an isolated frog heart, which slowed it, then collected the fluid around it and "
      "poured it on a second heart, which slowed too. Something released by the nerve, not an electrical signal, carried the message: "
      "he called it Vagusstoff, and it was later identified as acetylcholine. This was the first proof of chemical neurotransmission, "
      "and the beginning of the idea that chemicals can change how a whole organ behaves.",
      "Loewi O (1921) Uber humorale Ubertragbarkeit der Herznervenwirkung. Pflugers Arch 189:239-242" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    started_ = false;
    beats_.clear();
    ach_peak_ = ach_ = 0;
    lvl_ = 0;
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A neuromodulator is a chemical that changes how cells behave, slowly and over a whole region, instead of delivering one "
                  "spike to one cell. The vagus releases acetylcholine, and the pacemaker responds to its level, not to a synapse.", "" },
      { 2, "Wire a cell that the vagus drives to the 'vagal' output: that output turns its spikes into ACh. Put the pacemaker cell on 'beat'. "
           "Do not connect the two cells with a synapse.", "vagal" },
      { 3, "vagus > cell V (an ordinary cell, the default sensor weight is right) > 'vagal'; SA node cell > 'beat'. V fires about 85 times a "
           "second, the vagal output (gain 0.012, tau 500 ms) turns that into an ACh level near 0.5, and the heart slows from about 103 to 67 "
           "per minute. More cells on 'vagal' means more ACh: too many stop the node.", "vagal" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    Stats st = stats();
    return { { "slowing_ratio", { st.ok ? st.ratio : 0.0, "x", false } } };
  }

  Score score() const override
  {
    Score s;
    Stats st = stats();
    const double need = mode_ == "guided" ? 1.15 : ( mode_ == "researcher" ? 1.35 : 1.25 );
    s.parts[ "base_iv_ms" ] = st.base_iv;
    s.parts[ "eff_iv_ms" ] = st.eff_iv;
    s.parts[ "ratio" ] = st.ok ? st.ratio : 0.0;
    s.parts[ "ach_peak" ] = ach_peak_;
    s.parts[ "beats_before" ] = st.nb;
    s.parts[ "beats_after" ] = st.ne;
    const bool a = st.nb >= 2, b = ach_peak_ >= 0.15, c = st.ok && st.ratio >= need && st.ratio <= 3.0 && st.ne >= 2;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ) + ( c ? 1 : 0 ), 3 );
    if ( !done() )
      s.passed = false;
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running: ACh " << fmt( ach_, 2 ) << ", " << static_cast< int >( beats_.size() ) << " beats so far.";
    else if ( !a )
      o << "The pacemaker is not beating on its own before the vagus comes on (" << static_cast< int >( st.nb ) << " beats). Put a sinoatrial-node cell on 'beat'.";
    else if ( !b && st.ok && st.ratio >= need )
      o << "The beat slowed, but the ACh level stayed near 0 (peak " << fmt( ach_peak_, 2 ) << "): you slowed it some other way. The vagus acts through "
           "the 'vagal' output, which releases ACh; wire your vagus cell to it.";
    else if ( !b )
      o << "No acetylcholine was released (peak " << fmt( ach_peak_, 2 ) << "), so the heart is not slowed. Make a cell fire when the vagus comes on and wire it "
           "to the 'vagal' output.";
    else if ( st.ne < 2 || st.ratio > 3.0 )
      o << "Too much: ACh reached " << fmt( ach_peak_, 2 ) << " and the node nearly stopped (" << static_cast< int >( st.ne ) << " beats at the end, interval x"
        << fmt( st.ratio, 1 ) << "). Fewer cells on 'vagal', or a smaller gain on the output, for a gentler slowing.";
    else if ( st.ratio < need )
      o << "The beat slowed only from " << fmt( st.base_iv ) << " to " << fmt( st.eff_iv ) << " ms between beats (x" << fmt( st.ratio, 2 ) << ", needs x"
        << fmt( need, 2 ) << "). ACh peaked at " << fmt( ach_peak_, 2 ) << ": raise the gain of the 'vagal' output or have more cells fire into it.";
    else
      o << "The vagus slows the heart: " << fmt( 60000.0 / st.base_iv ) << " to " << fmt( 60000.0 / st.eff_iv ) << " beats per minute, through an ACh level of "
        << fmt( ach_peak_, 2 ) << ".";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    if ( !started_ )
    {
      started_ = true;
      e.set_modulator( "ACh", 0.0 );
      e.set_modulator( "NE", 0.0 );
    }
    lvl_ = t_ >= kOn ? kLevel : 0.0;
    e.set_input( "vagus", lvl_ > 0 ? lvl_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
  }

  void observe( Engine& e, double dt ) override
  {
    if ( motor_.spike_count( e, "beat", dt ) > 0.5 ) beats_.push_back( t_ );
    ach_ = e.modulator( "ACh" );
    ach_peak_ = std::max( ach_peak_, ach_ );
    Stats st = stats();
    double iv = 0;
    if ( beats_.size() >= 2 ) iv = beats_.back() - beats_[ beats_.size() - 2 ];
    readouts_[ "vagus" ] = lvl_;
    readouts_[ "ACh" ] = ach_;
    readouts_[ "beat_interval_ms" ] = iv;
    readouts_[ "slowing" ] = st.ok ? st.ratio : ( t_ < kE0 ? 1.0 : 0.0 );
    readouts_[ "bpm" ] = iv > 0 ? 60000.0 / iv : 0.0;
  }

private:
  struct Stats
  {
    double base_iv = 0, eff_iv = 0, ratio = 0, nb = 0, ne = 0;
    bool ok = false;
  };

  Stats stats() const
  {
    Stats st;
    auto mean_iv = [ & ]( double t0, double t1, double& n ) {
      double prev = -1, sum = 0, k = 0;
      n = 0;
      for ( double t : beats_ )
      {
        if ( t <= t0 || t > t1 ) continue;
        n += 1;
        if ( prev >= 0 )
        {
          sum += t - prev;
          k += 1;
        }
        prev = t;
      }
      return k > 0 ? sum / k : 0.0;
    };
    st.base_iv = mean_iv( kB0, kOn, st.nb );
    st.eff_iv = mean_iv( kE0, kDur, st.ne );
    st.ok = st.base_iv > 0 && st.eff_iv > 0;
    st.ratio = st.ok ? st.eff_iv / st.base_iv : 0.0;
    return st;
  }

  std::vector< double > beats_;
  double ach_peak_ = 0, ach_ = 0, lvl_ = 0, gain_ = 1;
  bool started_ = false;
};

} // namespace

std::unique_ptr< Task > make_m_neuromodulation() { return std::make_unique< MNeuromodulation >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_neuromodulation, 209, "m_neuromodulation.json", make_m_neuromodulation ) }
