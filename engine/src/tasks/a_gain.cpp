// "a_gain" (chapter a_compass, Atlas wing, level_kind "atlas"): one number to set, the strength of the ring-wide inhibition.
//
// The wiring of the compass with the signs that hold a bump (circuits/a_gain_given.json, made by circuits/tools/gen_atlas_compass.py) is given and locked. One cell-type card has a free number: the scale of
// the Delta7 group, which multiplies every connection out of the Delta7 cells (given too weak; range in the file). Too little inhibition and the activity spreads over the ring, too much and the bump
// dies (the window is measured, research/atlas-a.md 4d). The world is the cue-and-hold world of atlas_compass.h, at the cues gain_cues().
// WHAT PASSES: the bump holds where it was put at every one of those cues. Measured, not stored: tasks_atlas_compass_test sweeps the scale and holds the level to a window on 40 circuit seeds.
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
              "has a scale that multiplies the strength of every connection they make. Too little inhibition and the activity spreads over the ring; too much and the bump cannot start or dies out. "
              "Set the scale so that the bump holds where a cue puts it. A cue is shown at two places in turn, with a reset between them.";
    i.concepts_new = { "inhibition_balance" };
    i.concepts_used = { "ring_attractor", "inhibition", "synaptic_sign" };
    i.recipe = "One scale, on the Delta7 card. Run the cues and read the ring: if the activity spreads, the inhibition is too weak for the excitation; if it dies out, it is too strong. "
               "The scale that holds is near the middle of the range.";
    i.note = { "A window, not a point",
               "In a ring attractor the inhibition has to be strong enough to keep a single bump and weak enough to let the bump exist: the working gains form a window. The window found here is a property "
               "of this model, a point-cell circuit with fitted gains, and says nothing about the strength of the fly's synapses.",
               "Turner-Evans DB et al. (2020) Neuron 108:145-163, doi:10.1016/j.neuron.2020.08.006; Hulse BK et al. (2021) eLife 10:e66039, doi:10.7554/eLife.66039" };
    return i;
  }

  std::string level_kind() const override { return "atlas"; }

  Given given() const override
  {
    Given g;
    g.circuit = "a_gain_given.json";
    // printed by `spike-run --locked-digest circuits/a_gain_given.json`
    g.digest.pops = "";
    g.digest.wiring = "";
    g.digest.types = "";
    g.digest.channels = "";
    return g;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "Inhibition that spans the ring has to balance the excitation of the ring: too little and every cell is pulled along, too much and nothing can start.", "" },
      { 2, "Run the cues and look at the ring. If the activity covers it, or dies out, the Delta seven scale is on the wrong side of the working range.", "D7" },
      { 3, "Change the Delta seven scale in small steps and run the cues again after each step. The working range lies well inside the allowed range, not at either end.", "D7" } };
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
    score_holds( *this, done(), wiring_.ok(), wiring_.message(), mech_, s, "The bump stays where it was put, at every cue. In this model that strength of inhibition balances the excitation of the ring.",
                 " In this model the strength of the inhibition is outside the range that holds a bump." );
    apply_mech( s );
    return s;
  }

protected:
  void inspect( Engine& ) override
  {
    if ( sign_of( "D7" ) == 0 ) mech_ = "The cell-type card of the Delta seven cells is missing: load the given circuit again.";
  }
};
}  // namespace

std::unique_ptr< Task > make_a_gain() { return std::make_unique< AGain >(); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_gain, 997, "a_gain.json", make_a_gain ) }
