// Micro-puzzle "m_convergence" (concept 3: convergence and coincidence). Fire only when all three inputs arrive together.
//
// WORLD (3800 ms; three dc sensors "in_a", "in_b", "in_c", each driving level 1.0 x its weight (pA) while it is on)
//   Seven patterns, pattern p starting at 300 + 500 p ms:
//     0: a      1: b + c      2: a + b + c (TARGET)      3: a, then b, then c, 100 ms each, never together (staggered)
//     4: a + c  5: a + b + c (TARGET)                    6: c
//   Every pattern lasts 300 ms (the staggered one 100 ms per input).
//   Motor "out": spikes of the cell(s) wired to it.
// SCORING
//   target_hits : target patterns with >= 3 spikes in (t0, t0 + 330]            needs 2 of 2
//   false_spikes: spikes anywhere else (in the other five windows and the gaps)  needs 0 (guided <= 2)
//   noise: 4 % white per ms slice and 2 % per-episode gain (x0 guided, x2 researcher).
//   value 100 when passed, else 85 * (criteria met / 2).
// With the default part (iaf, rheobase 375 pA) and the lab's 300 pA sensor weight a single input is under threshold but two are over it, so
// the first build fires on pairs. With a weight x per input, one gives x, two 2x, three 3x: only 125 < x < 187 pA separates the target from
// the pairs. (A higher threshold on the cell, V_th about -45 mV, does it at the default weight too.)
//
// Biology: a cat spinal motoneuron needs about 10 mV of depolarisation to fire, while one spindle afferent gives it a 0.1 mV EPSP on
// average (Mendell & Henneman 1971): a motoneuron fires only when many inputs converge within the membrane time constant.
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;

constexpr int kPatterns = 7;
constexpr double kT0 = 300, kSlot = 500, kLen = 300, kAfter = 330, kDur = 3800;
const char* const kName[ kPatterns ] = { "a alone", "b + c", "a + b + c", "a, b, c one after the other", "a + c", "a + b + c", "c alone" };
const bool kTarget[ kPatterns ] = { false, false, true, false, false, true, false };
// bit 0 = a, bit 1 = b, bit 2 = c
const int kMask[ kPatterns ] = { 1, 6, 7, 0, 5, 7, 4 };

class MConvergence : public MicroBase
{
public:
  MConvergence() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_convergence";
    i.title = "Convergence";
    i.brief = "A neuron adds up what converges on it, and fires only when the sum is big enough. Three inputs, in_a, in_b and in_c, switch "
              "on in different combinations: one alone, two together, all three together, and all three one after the other. Build a "
              "neuron that fires only when all three are on at the same time (at least 3 spikes, both times it happens) and stays "
              "silent for every other combination. Each input is a current of its sensor's weight; the cell fires when the total goes "
              "over its rheobase. Make one input too small to fire the cell, and even two not enough, so that it takes all three. "
              "Score: both 'all three' patterns answered, no other spikes.";
    i.chapter = "invertebrates";
    i.sensors = { "in_a", "in_b", "in_c" };
    i.motors = { "out" };
    i.duration_ms = kDur;
    i.concepts_new = { "convergence" };
    i.concepts_used = { "threshold", "excitation" };
    i.par_neurons = 1;
    i.kinds = { { "in_a", micro::dc( 1.0 ) }, { "in_b", micro::dc( 1.0 ) }, { "in_c", micro::dc( 1.0 ) }, { "out", micro::rate() } };
    i.bands = { { "target_hits", "of 2", 2, 2 }, { "false_spikes", "spikes", 0, 0.5 } };
    for ( int p = 0; p < kPatterns; ++p ) i.timeline.push_back( { kT0 + kSlot * p, kLen, kName[ p ] } );
    i.note = { "A motoneuron is a coincidence detector",
      "One spindle afferent produces an excitatory potential of only about a tenth of a millivolt (0.1 mV on average) in a cat spinal "
      "motoneuron, while the motoneuron has to be depolarised by roughly 10 mV to fire. So a motoneuron fires only when many afferents "
      "converge on it within a few milliseconds: convergence plus a threshold turns a crowd of weak inputs into one decision.",
      "Mendell LM, Henneman E (1971) Terminals of single Ia fibers: location, density, and distribution within a pool of 300 homonymous "
      "motoneurons. J Neurophysiol 34:171-187" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    for ( int p = 0; p < kPatterns; ++p ) spikes_[ p ] = 0;
    stray_ = 0;
    first_ = -1;
    lat_sum_ = lat_n_ = 0;
    for ( int k = 0; k < 3; ++k ) on_[ k ] = false;
    t_first_[ 0 ] = t_first_[ 1 ] = -1;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A neuron sums the inputs that converge on it and fires only when the total crosses threshold. So make each input too weak "
                  "to matter alone, and even two together too weak: it takes all three at once (coincidence).", "" },
      { 2, "Look at the weights of the three sensors in_a, in_b and in_c: they set how much current each input adds. Two inputs together "
           "must stay under the cell's rheobase.", "in_a" },
      { 3, "Wire in_a, in_b and in_c to one neuron and the neuron to 'out'. Set each of the three sensor weights to about 150 pA: one input "
           "gives 150, two give 300 (under the 375 pA rheobase), three give 450 (over it). The lab starts them at 300, where two already fire.",
        "in_a" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "latency_ms", { lat_n_ > 0 ? lat_sum_ / lat_n_ : 999.0, "ms", true } } };
  }

  Score score() const override
  {
    Score s;
    double hits = 0, other = stray_;
    int first_false = -1;
    for ( int p = 0; p < kPatterns; ++p )
    {
      if ( kTarget[ p ] )
      {
        if ( spikes_[ p ] >= 3 ) hits += 1;
      }
      else
      {
        other += spikes_[ p ];
        if ( spikes_[ p ] > 0 && first_false < 0 ) first_false = p;
      }
    }
    const double max_false = mode_ == "guided" ? 2 : 0;
    s.parts[ "target_hits" ] = hits;
    s.parts[ "false_spikes" ] = other;
    s.parts[ "latency_ms" ] = lat_n_ > 0 ? lat_sum_ / lat_n_ : -1.0;
    const bool a = hits >= 2, b = other <= max_false;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ), 2 );
    if ( !done() )
      s.passed = false;
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running: " << static_cast< int >( hits ) << " of 2 target patterns answered so far.";
    else if ( !a && other == 0 )
      o << "The cell never fired, not even with all three inputs on. The three together are still under its rheobase: raise the weights.";
    else if ( !b && first_false >= 0 && !a )
      o << "Wrong way round: the cell fired for '" << kName[ first_false ] << "' but missed " << fmt( 2 - hits ) << " of the 'all three' patterns.";
    else if ( !b && first_false >= 0 )
      o << "It answered both 'all three' patterns, but also fired for '" << kName[ first_false ] << "' (" << static_cast< int >( other )
        << " spikes in all outside the targets). Lower the sensor weights until two inputs together are no longer enough.";
    else if ( !b )
      o << static_cast< int >( other ) << " stray spikes outside the patterns. Check for a bias current.";
    else if ( !a )
      o << "Silent for the wrong patterns, but only " << static_cast< int >( hits ) << " of the 2 'all three' patterns made it fire: raise the weights a little.";
    else
      o << "Only all three inputs together fire the cell: both targets answered, nothing else.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    for ( int k = 0; k < 3; ++k )
    {
      bool on = active( k, t_ );
      on_[ k ] = on;
      e.set_input( info_.sensors[ k ], on ? gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
    }
  }

  void observe( Engine& e, double dt ) override
  {
    double n = motor_.spike_count( e, "out", dt );
    if ( n > 0 )
    {
      int p = pattern_at( t_ );
      if ( p >= 0 )
      {
        spikes_[ p ] += n;
        if ( kTarget[ p ] )
        {
          const int slot = p == 2 ? 0 : 1;
          if ( t_first_[ slot ] < 0 )
          {
            t_first_[ slot ] = t_;
            lat_sum_ += t_ - ( kT0 + kSlot * p );
            lat_n_ += 1;
          }
        }
      }
      else
        stray_ += n;
    }
    readouts_[ "inputs_on" ] = static_cast< double >( on_[ 0 ] + on_[ 1 ] + on_[ 2 ] );
    double hits = 0, other = stray_;
    for ( int p = 0; p < kPatterns; ++p )
    {
      if ( kTarget[ p ] ) hits += spikes_[ p ] >= 3 ? 1 : 0;
      else other += spikes_[ p ];
    }
    readouts_[ "target_hits" ] = hits;
    readouts_[ "false_spikes" ] = other;
  }

private:
  bool active( int k, double t ) const
  {
    for ( int p = 0; p < kPatterns; ++p )
    {
      const double t0 = kT0 + kSlot * p;
      if ( kMask[ p ] == 0 )  // staggered
      {
        if ( t >= t0 + 100 * k && t < t0 + 100 * k + 100 ) return true;
      }
      else if ( ( kMask[ p ] >> k ) & 1 )
      {
        if ( t >= t0 && t < t0 + kLen ) return true;
      }
    }
    return false;
  }
  static int pattern_at( double t )
  {
    for ( int p = 0; p < kPatterns; ++p )
    {
      const double t0 = kT0 + kSlot * p;
      if ( t > t0 && t <= t0 + kAfter ) return p;
    }
    return -1;
  }

  double gain_ = 1, spikes_[ kPatterns ] = {}, stray_ = 0, first_ = -1, lat_sum_ = 0, lat_n_ = 0, t_first_[ 2 ] = { -1, -1 };
  bool on_[ 3 ] = {};
};

} // namespace

std::unique_ptr< Task > make_m_convergence() { return std::make_unique< MConvergence >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_convergence, 9, "m_convergence.json", make_m_convergence ) }
