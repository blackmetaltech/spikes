// Micro-puzzle "m_subtraction" (concept 5: subtraction, ON minus OFF). out should fire only when 'plus' is stronger than 'minus'.
//
// WORLD (3800 ms; two dc sensors "plus" and "minus", level x sensor weight in pA)
//   Seven segments of 500 ms from t = 300 ms, (plus level, minus level) and the required answer:
//     0: (0, 0) silent   1: (3, 0) FIRE   2: (3, 3) silent   3: (0, 3) silent   4: (3, 2) FIRE   5: (2, 2) silent   6: (3, 0) FIRE
//   Motor "diff": spikes of the cell(s) wired to it. A segment is judged on (t0 + 100, t0 + 500]: FIRE needs >= 5 spikes, silent needs <= 1
//   (guided <= 3). Noise: 4 % white per ms slice and 2 % per-episode gain, x0 guided, x2 researcher.
// SCORING  fire_ok = FIRE segments answered (needs 3 of 3), silent_ok = silent segments kept quiet (needs 4 of 4).
//   value 100 when passed, else 85 * (criteria met / 2).
// The only way to pass is a real difference: plus must excite the out cell, minus must reach it through an inhibitory cell whose strength
// cancels a minus of 3 against a plus of 3 (fully) but leaves a plus of 3 against a minus of 2 firing. With the lab's defaults the plus
// level 3 gives 900 pA on the out cell; an inhibitory link of about -1000 to -1500 pA (see docs/circuits/m_subtraction.md) satisfies all seven segments.
//
// Biology: ON-minus-OFF is the basic computation of a receptive field: C. elegans AWC/ASE sensory neurons and the retinal ganglion
// cell's centre-surround difference; the worm's ASEL (ON) and ASER (OFF) drive opposite turns (Suzuki et al. 2008 Nature 454:114).
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;
using micro::Graph;

constexpr int kSeg = 7;
constexpr double kT0 = 300, kSlot = 500, kDur = 3800;
const double kPlus[ kSeg ] = { 0, 3, 3, 0, 3, 2, 3 };
const double kMinus[ kSeg ] = { 0, 0, 3, 3, 2, 2, 0 };
const bool kFire[ kSeg ] = { false, true, false, false, true, false, true };

class MSubtraction : public MicroBase
{
public:
  MSubtraction() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_subtraction";
    i.title = "Subtraction";
    i.brief = "Many sensory circuits compute a difference: ON minus OFF. Two inputs, 'plus' and 'minus', take levels from 0 to 3. Build a "
              "circuit whose output 'diff' fires only when plus is stronger than minus: not when both are 0, not when minus is on "
              "alone, and not when the two are equal (3 against 3, or 2 against 2), but yes for 3 against 0 and for 3 against 2. "
              "Excitation from plus and inhibition from minus meet on the output cell, and the inhibition has to be as strong as "
              "the excitation: a plus of 3 must be cancelled by a minus of 3 and still win against a minus of 2. Score: all three "
              "firing segments answered, all four quiet ones kept silent.";
    i.chapter = "invertebrates";
    i.sensors = { "plus", "minus" };
    i.motors = { "diff" };
    i.duration_ms = kDur;
    i.concepts_new = { "subtraction" };
    i.concepts_used = { "inhibition", "convergence" };
    i.par_neurons = 2;
    i.default_model = "iaf_psc_alpha";
    i.recipe = "plus > out cell > diff. minus > cell B (inhibitory) > out cell, with the B > out weight at about -1100 pA. Too weak "
               "and equal inputs still fire; too strong and plus 3 against minus 2 goes quiet.";
    i.kinds = { { "plus", micro::dc( 3.0 ) }, { "minus", micro::dc( 3.0 ) }, { "diff", micro::rate() } };
    i.bands = { { "fire_ok", "of 3", 3, 3 }, { "silent_ok", "of 4", 4, 4 } };
    for ( int s = 0; s < kSeg; ++s )
      i.timeline.push_back( { kT0 + kSlot * s, kSlot, std::string( "plus " ) + fmt( kPlus[ s ] ) + " / minus " + fmt( kMinus[ s ] ) } );
    i.note = { "Opposite signs, opposite turns",
      "The nematode C. elegans has two salt-sensing neurons, ASEL and ASER, that respond in opposite ways: ASEL is excited when salt "
      "rises and ASER when it falls. Each drives one of two behaviours, so the animal's turning reflects the difference between the "
      "two, and it climbs the salt gradient. Neurons need no arithmetic unit: excitation and inhibition meeting on one cell is a "
      "subtraction.",
      "Suzuki H, Thiele TR, Faumont S, Ezcurra M, Lockery SR, Schafer WR (2008) Functional asymmetry in Caenorhabditis elegans taste "
      "neurons and its computational role in chemotaxis. Nature 454:114-117" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    gain_ = 1.0 + 0.02 * noise() * rng_.normal();
    for ( int s = 0; s < kSeg; ++s ) n_[ s ] = 0;
    p_ = m_ = 0;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "Subtraction is excitation plus inhibition on one cell: plus excites it, minus inhibits it. A sensor can only excite, so "
                  "minus must go through a neuron you set to inhibitory. The inhibition has to be about as strong as the excitation it cancels.", "" },
      { 2, "plus drives the output cell directly; minus drives a second, inhibitory neuron that connects to the same output cell. Look at "
           "the weight of that last link: with the default it barely dents the drive.", "diff" },
      { 3, "plus > out cell > diff. minus > cell B (inhibitory) > the same out cell. Make the inhibitory link several times the default: "
           "too weak and equal inputs still fire, too strong and a plus that is only a little stronger than minus goes quiet. The verdict says which way you are off.", "diff" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    double fire = 0, nf = 0;
    for ( int s = 0; s < kSeg; ++s )
      if ( kFire[ s ] ) { fire += n_[ s ]; nf += 1; }
    return { { "fire_rate_hz", { nf > 0 ? fire / nf / 0.4 : 0.0, "Hz", true } } };
  }

  void inspect( Engine& ) override
  {
    const auto plus = g_.in_pops( "plus" ), minus = g_.in_pops( "minus" ), out = g_.out_pops( "diff" );
    if ( out.empty() || plus.empty() || minus.empty() ) return;
    if ( !g_.path( plus, out, +1, 3, true ) )
      mech_ = "'plus' does not excite the cell that is read on 'diff': the output needs excitation from plus.";
    else if ( !g_.path( minus, out, 0, 3, true ) )
      mech_ = "'minus' does not reach the cell that is read on 'diff': excitation and inhibition have to meet on the same output cell.";
    else if ( !g_.path( minus, out, -1, 3 ) )
      mech_ = "'minus' reaches the output only by exciting it, so it adds to plus instead of subtracting. A sensor can only excite: send minus through an "
              "inhibitory neuron onto the output cell.";
  }

  Score score() const override
  {
    Score s;
    const double max_silent = mode_ == "guided" ? 3 : 1;
    double fire_ok = 0, silent_ok = 0;
    int bad_fire = -1, bad_silent = -1;
    for ( int k = 0; k < kSeg; ++k )
    {
      if ( kFire[ k ] )
      {
        if ( n_[ k ] >= 5 ) fire_ok += 1;
        else if ( bad_fire < 0 ) bad_fire = k;
      }
      else
      {
        if ( n_[ k ] <= max_silent ) silent_ok += 1;
        else if ( bad_silent < 0 ) bad_silent = k;
      }
    }
    s.parts[ "fire_ok" ] = fire_ok;
    s.parts[ "silent_ok" ] = silent_ok;
    for ( int k = 0; k < kSeg; ++k ) s.parts[ "seg" + std::to_string( k ) ] = n_[ k ];
    const bool a = fire_ok >= 3, b = silent_ok >= 4;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ), 2 );
    if ( !done() )
      s.passed = false;
    apply_mech( s );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( fire_ok == 0 && n_[ 1 ] == 0 )
      o << "Silent even when only plus is on. plus must excite the output cell, and its current must be over the rheobase.";
    else if ( !b && a )
      o << "Fired in a segment where minus should have cancelled plus"
        << ": the minus input does not cancel the plus. Make the inhibitory link from the minus cell stronger.";
    else if ( !a && b )
      o << "Silent in a segment where plus was stronger than minus"
        << ": the inhibition is too strong. It should cancel equal inputs, not subtract more than minus carries. Weaken the inhibitory link a little.";
    else if ( !a && !b )
      o << "Neither right: some segments that should fire do not and some that should be quiet fire. Check that plus reaches the output cell and that "
           "minus goes through an inhibitory cell onto the same output cell.";
    else
      o << "Output = plus minus minus: it fires when plus clearly beats minus, and stays quiet whenever minus is as strong as plus.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    p_ = m_ = 0;
    const int k = seg_at( t_ );
    if ( k >= 0 )
    {
      p_ = kPlus[ k ];
      m_ = kMinus[ k ];
    }
    e.set_input( "plus", p_ > 0 ? p_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
    e.set_input( "minus", m_ > 0 ? m_ * gain_ * ( 1.0 + 0.04 * noise() * rng_.normal() ) : 0.0 );
  }

  void observe( Engine& e, double dt ) override
  {
    const double n = motor_.spike_count( e, "diff", dt );
    for ( int k = 0; k < kSeg; ++k )
    {
      const double t0 = kT0 + kSlot * k;
      if ( t_ > t0 + 100 && t_ <= t0 + kSlot ) n_[ k ] += n;
    }
    double fire_ok = 0, silent_ok = 0;
    for ( int k = 0; k < kSeg; ++k )
    {
      if ( kFire[ k ] && n_[ k ] >= 5 ) fire_ok += 1;
      if ( !kFire[ k ] && n_[ k ] <= ( mode_ == "guided" ? 3 : 1 ) && t_ > kT0 + kSlot * k + kSlot ) silent_ok += 1;
    }
    readouts_[ "plus" ] = p_;
    readouts_[ "minus" ] = m_;
    readouts_[ "difference" ] = p_ - m_;
    readouts_[ "fire_ok" ] = fire_ok;
    readouts_[ "silent_ok" ] = silent_ok;
  }

private:
  static int seg_at( double t )
  {
    for ( int k = 0; k < kSeg; ++k )
      if ( t >= kT0 + kSlot * k && t < kT0 + kSlot * ( k + 1 ) ) return k;
    return -1;
  }

  double gain_ = 1, n_[ kSeg ] = {}, p_ = 0, m_ = 0;
};

} // namespace

std::unique_ptr< Task > make_m_subtraction() { return std::make_unique< MSubtraction >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_subtraction, 18, "m_subtraction.json", make_m_subtraction ) }
