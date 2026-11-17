// Micro-puzzle "m_recurrent" (concept 13: recurrent inhibition, the Renshaw loop). A motoneuron's own output must limit how far extra input can push it.
//
// WORLD (3100 ms; both sensors are dc, multiplied by the sensor weight (pA per unit) in the circuit)
//   "drive": level 1.5 all episode (450 pA at the lab's 300 pA per unit: a motoneuron wired to it alone fires at about 50 spikes/s).
//   "kick" : extra excitation of the same motoneurons (a descending volley): level 0 until b = 1000 + J (J = U(0,100) ms seeded), 1.0 on [b, b + 1000),
//            2.0 on [b + 1000, b + 2000). 300 pA and then 600 pA more, which would carry an unprotected motoneuron to about 110 and 155 spikes/s.
//   Standard noise: 3 % white per ms slice on both levels (x0 guided, x2 researcher).
//   Motor "mn": the motoneurons' pooled rate (Hz per neuron).
// SCORING (windows in model time)
//   A  [600, b]              rate rA >= 15 spikes/s                the cells are alive at the base drive
//   B  [b + 500, b + 1000]   rise rB - rA <= 27 spikes/s  (45 % of the 60 of an unprotected cell; guided x1.35, researcher x0.8)
//   C  [b + 1500, b + 2000]  rise rC - rA <= 42 spikes/s  (40 % of the 105 of an unprotected cell)
//   value 100 when passed, else 85 * credit of the three criteria.
// Why it works out: an unprotected iaf cell goes 50 -> 110 -> 155 spikes/s (+60, +105). A Renshaw cell driven by the motoneurons' own spikes
// and inhibiting them (negative feedback in the loop, gain control) flattens that (the rise is less than half) ; it does not distinguish where the
// extra input came from, which is the point of recurrent inhibition.
//
// Biology: Renshaw B (1941) J Neurophysiol 4:167-183 found interneurons excited by motor axon collaterals that inhibit the motoneurons;
// Eccles JC, Fatt P, Koketsu K (1954) J Physiol 126:524-562 showed the pathway is cholinergic (motor-axon collaterals onto Renshaw cells) and
// inhibitory onto motoneurons.
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 3100, kDrive = 1.5, kKick1 = 1.0, kKick2 = 2.0, kB0 = 1000;
constexpr double kUnprotB = 60, kUnprotC = 105;  // rate rise (spikes/s) of an unprotected cell for the first and the double kick

class MRecurrent : public MicroB
{
public:
  MRecurrent() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_recurrent";
    i.title = "Recurrent inhibition";
    i.brief = "A motoneuron that is pushed harder should not simply fire faster and faster. Its axon sends a collateral to a small inhibitory "
              "interneuron, the Renshaw cell, which inhibits the motoneurons back: the harder they fire, the harder they are held in, a "
              "negative feedback loop in one synapse pair. Here a steady 'drive' makes the motoneurons fire at about 50 spikes/s. At 1 s an extra "
              "volley 'kick' adds to their input, and 1 s later it doubles. Without a brake the rate would climb by about 60 and 105 spikes/s. "
              "Keep the extra rate the kicks add under 27 spikes/s for the first and under 42 for the second, while still firing at "
              "least 15 spikes/s at the base drive. Wire drive and kick onto the motoneuron group and read it out on 'mn'. Then let the "
              "motoneurons excite a few cells set to inhibitory (the Renshaw cells) that inhibit the motoneurons back. The Renshaw cells only "
              "fire if the loop is strong: the default weights are far too weak (about 600 pA both ways works for 2 Renshaw cells).";
    i.chapter = "spinal";
    i.sensors = { "drive", "kick" };
    i.motors = { "mn" };
    i.duration_ms = kDur;
    i.concepts_new = { "recurrent_inhibition" };
    i.concepts_used = { "inhibition", "excitation" };
    i.par_neurons = 5;
    i.kinds = { { "drive", dc( kDrive ) }, { "kick", dc( kKick1 ) }, { "mn", rate() } };
    i.bands = { { "rate_rise_hz", "spikes/s", -25, 42 } };
    i.timeline = { { kB0, 1000, "kick" }, { kB0 + 1000, 1000, "double kick" } };
    i.concept_hint = "Recurrent inhibition: motoneurons excite a Renshaw cell that inhibits them back. It is negative feedback on the output "
                     "itself, so extra input from anywhere is held down, like a gain control.";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "Close a loop: the motoneuron group should feed a small inhibitory group and that group should project back onto the motoneurons.", "mn" },
      { 3, "mn group of 4 cells (drive and kick onto it). An inhibitory group of 2 cells (Renshaw) receives the motoneurons at about 300 pA and returns "
           "-300 pA (about -450 if the rate still rises too much).", "mn" } };
    i.note = { "Renshaw cells",
      "In 1941 Birdsey Renshaw found interneurons in the spinal cord that fire after the motoneurons do, driven by the motoneurons' own "
      "axon collaterals. Later work showed they inhibit the same motoneurons and their neighbours. The loop turns each motoneuron's activity "
      "into its own brake: it limits firing rate, helps synchronise the pool and, as in clonus when it fails, keeps the output stable.",
      "Renshaw B (1941) Influence of discharge of motoneurons upon excitation of neighboring motoneurons. J Neurophysiol 4:167-183; "
      "Eccles JC, Fatt P, Koketsu K (1954) J Physiol 126:524-562" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    j_ = rng_.uniform( 0, 100 );
    base_ = 50;
  }

  std::map< std::string, Metric > metrics() const override
  {
    std::map< std::string, double > p;
    std::vector< Crit > c;
    std::string m;
    measure( p, c, m );
    return { { "rise_double_kick_hz", { p.count( "rise_double_kick_hz" ) ? p[ "rise_double_kick_hz" ] : 999.0, "spikes/s", true } } };
  }

protected:
  double kick_at( double t ) const
  {
    const double b = kB0 + j_;
    if ( t >= b && t < b + 1000 ) return kKick1;
    if ( t >= b + 1000 && t < b + 2000 ) return kKick2;
    return 0;
  }

  void drive( Engine& e, double ) override
  {
    const double k = kick_at( t_ );
    e.set_input( "drive", kDrive * ( 1.0 + 0.03 * noise() * rng_.normal() ) );
    e.set_input( "kick", k > 0 ? k * ( 1.0 + 0.03 * noise() * rng_.normal() ) : 0.0 );
  }

  void update_readouts() override
  {
    if ( t_ >= kB0 + j_ - 20 && t_ < kB0 + j_ - 10 )
      base_ = std::max( 5.0, hz( "mn", 600, kB0 + j_ - 20 ) );
    readouts_[ "mn_hz" ] = hz_now( "mn" );
    readouts_[ "kick" ] = kick_at( t_ );
    readouts_[ "rate_rise_hz" ] = t_ >= kB0 + j_ ? hz_now( "mn" ) - base_ : 0.0;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol();
    const double b = kB0 + j_;
    const double ra = hz( "mn", 600, b ), rb = hz( "mn", b + 500, b + 1000 ), rc = hz( "mn", b + 1500, b + 2000 );
    const double db = rb - ra, dc = rc - ra;
    parts[ "rate_base_hz" ] = ra;
    parts[ "rate_kick_hz" ] = rb;
    parts[ "rate_double_kick_hz" ] = rc;
    parts[ "rise_kick_hz" ] = db;
    parts[ "rise_double_kick_hz" ] = dc;
    parts[ "ratio_c" ] = rc / std::max( ra, 1.0 );
    const double lim_b = kUnprotB * 0.45 * tl, lim_c = kUnprotC * 0.40 * tl;

    Crit base;
    base.key = "alive";
    base.ok = ra >= 15;
    base.soft = ramp( ra, 0, 15 );
    base.fail = "At the base drive the motoneurons fire only " + fmt( ra ) + " spikes/s (needs 15): too much inhibition, or 'drive' does not reach them.";
    base.place = "The motoneuron group is silenced at rest: its brake is too strong, or it gets no 'drive'.";
    base.target = "mn";
    base.recipe = "Wire 'drive' and 'kick' straight onto the motoneurons (weight 300 pA per unit); keep the feedback weights at about 600 pA.";
    crits.push_back( base );

    Crit kb;
    kb.key = "kick";
    kb.ok = db <= lim_b;
    kb.soft = 1.0 - ramp( db, lim_b, kUnprotB );
    kb.fail = "The first kick lifts the rate by " + fmt( db ) + " spikes/s, from " + fmt( ra ) + " to " + fmt( rb ) + " (limit " + fmt( lim_b ) +
              "; with no brake it is about " + fmt( kUnprotB ) + "): nothing holds the motoneurons back. Their own output must excite an inhibitory cell that inhibits them in return.";
    kb.place = "The loop around the motoneurons: they should drive an inhibitory group that projects back onto them.";
    kb.target = "mn";
    kb.recipe = "mn (4 cells) onto an inhibitory group of 2 cells at about 600 pA, and that group back onto mn at about -600 pA.";
    crits.push_back( kb );

    Crit kc;
    kc.key = "double_kick";
    kc.ok = dc <= lim_c;
    kc.soft = 1.0 - ramp( dc, lim_c, kUnprotC );
    kc.fail = "The double kick lifts the rate by " + fmt( dc ) + " spikes/s, to " + fmt( rc ) + " (limit " + fmt( lim_c ) + ", unbraked about " + fmt( kUnprotC ) + "): the feedback is too weak. "
              "Strengthen the Renshaw loop (both weights, or more Renshaw cells).";
    kc.place = "The strength of the loop: the weights motoneuron -> Renshaw cell and Renshaw cell -> motoneuron.";
    kc.target = "mn";
    kc.recipe = "Try about 600 pA from the motoneurons onto the Renshaw cells and -600 pA back (2 Renshaw cells).";
    crits.push_back( kc );

    pass_msg = "The rate went " + fmt( ra ) + " -> " + fmt( rb ) + " -> " + fmt( rc ) + " spikes/s: the kicks added only " + fmt( db ) + " and " + fmt( dc ) +
               " spikes/s where an unbraked cell adds about " + fmt( kUnprotB ) + " and " + fmt( kUnprotC ) + ": the Renshaw loop is a gain control.";
  }

private:
  double j_ = 0, base_ = 50;
};

} // namespace

std::unique_ptr< Task > make_m_recurrent() { return std::make_unique< MRecurrent >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_recurrent, 109, "m_recurrent.json", make_m_recurrent ) }
