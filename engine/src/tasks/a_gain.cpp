// "a_gain" (chapter a_compass, Atlas wing, level_kind "atlas"): one number to set, the strength of the ring-wide inhibition.
//
// The wiring of the compass with the signs that hold a bump (circuits/a_gain_given.json, made by circuits/tools/gen_atlas_compass.py) is given and locked. One cell-type card has a free number: the scale of
// the Delta7 group, which multiplies every connection out of the Delta7 cells (given too weak; range 0.1 to 1.8). Too little inhibition and the activity spreads over the ring. In this model more inhibition
// never kills the bump (the rates fall, the bump stays), and above a scale of about 2 whether it holds differs from seed to seed, so the range stops at 1.8: the window is one-sided (research/atlas-a.md 4d).
// The world is the cue-and-hold world of atlas_compass.h, at the cues gain_cues() (4 and 12: their resting places do not move with the gain, those of most other cues do).
// WHAT PASSES: the bump holds where it was put at both cues. Measured, not stored: tasks_atlas_compass_test steps the scale over the whole range (0.05 steps) on circuit seeds 1 to 40: it holds from 0.30 up on
// all of them and from 0.25 down on none.
#include "atlas_compass.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_gain();
namespace
{
using namespace atlas_compass;

class AGain : public CompassRun
{
public:
  AGain() : CompassRun( info(), gain_cues() ) { reset( 1 ); }

  static Info info()
  {
    Info i = world( "a_gain", "Balance the inhibition", gain_cues() );
    i.brief = "The signs of the compass are settled: the compass cells excite, the Delta7 cells inhibit the whole ring. What the wiring does not give is how strong one synapse is. One card, the Delta7 cells, "
              "has a scale that multiplies the strength of every connection they make. Too little inhibition and the activity spreads over the ring instead of staying a bump. "
              "Set the scale so that the bump holds where a cue puts it. A cue is shown at two places in turn, with a reset between them.";
    i.concepts_new = { "inhibition_balance" };
    i.concepts_used = { "ring_attractor", "inhibition", "synaptic_sign" };
    i.recipe = "One scale, on the Delta7 card, from 0.1 to 1.8. Run the cues and read the ring: if the activity covers the ring, the inhibition is too weak for the excitation, so raise the scale. "
               "The bump holds from a scale of about 0.3 up to the top of the range.";
    i.note = { "A window, and its edge",
               "In a ring attractor the inhibition has to be strong enough to keep a single bump, and a ring that is too strongly inhibited can lose it. In this model only the first edge is clean: below a scale of "
               "about 0.3 the activity spreads, and from there up the bump holds. Stronger inhibition lowers the rates and, above a scale of about 2, makes the result differ from run to run, so the card stops "
               "before it. This is a property of this model, a point-cell circuit with fitted gains, and says nothing about the strength of the fly's synapses.",
               "Turner-Evans DB et al. (2020) Neuron 108:145-163, doi:10.1016/j.neuron.2020.08.006; Hulse BK et al. (2021) eLife 10:e66039, doi:10.7554/eLife.66039" };
    return i;
  }

  std::string level_kind() const override { return "atlas"; }

  Given given() const override
  {
    Given g;
    g.circuit = "a_gain_given.json";
    // printed by `spike-run --locked-digest circuits/a_gain_given.json`
    g.digest.pops = "fe6963039bfc82301f8be725d63ea5dc";
    g.digest.wiring = "17f3c5dc11cc7c156bc681a04919aafc";
    g.digest.types = "5d72f27991f4fad7b3825dc7ea03d250";
    g.digest.channels = "ad2768fd684cc647b82ae1abd7d16b11";
    return g;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "Inhibition that spans the ring has to balance the excitation of the ring: with too little of it every cell is pulled along and the activity covers the ring.", "" },
      { 2, "Run the cues and look at the ring. If the activity covers it, the Delta seven scale is too low.", "DELTA" },
      { 3, "Raise the Delta seven scale in small steps and run the cues again after each step. The bump holds from about a third of the reference value up to the top of the range.", "DELTA" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    int n = 0;
    for ( const auto& b : bumps() ) n += b.held ? 1 : 0;
    return { { "cues_held", { static_cast< double >( n ), "cues", false } } };
  }

  Score score() const override
  {
    Score s;
    fill_parts( s );
    int kind = 0;
    for ( const auto& b : bumps() )
      if ( !b.held ) { kind = b.kind; break; }
    score_holds( *this, done(), wiring_.ok(), wiring_.message(), mech_, s, "The bump stays where it was put, at every cue. In this model that strength of inhibition balances the excitation of the ring.",
                 kind == 2 ? " In this model the inhibition is too weak to keep the activity to one bump."
                 : kind == 1 ? " In this model the inhibition is too strong for the bump to start."
                             : " In this model the strength of the inhibition is outside the range that holds a bump at this cue." );
    apply_mech( s );
    return s;
  }

protected:
  void inspect( Engine& ) override
  {
    if ( sign_of( "DELTA" ) == 0 ) mech_ = "The cell-type card of the Delta seven cells is missing: load the given circuit again.";
  }
};
}  // namespace

std::unique_ptr< Task > make_a_gain() { return std::make_unique< AGain >(); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_gain, 997, "a_gain.json", make_a_gain ) }
