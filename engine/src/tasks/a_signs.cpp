// "a_signs" (chapter a_compass, Atlas wing, level_kind "atlas"): the wiring of the fly compass says who connects to whom, not whether a connection excites or inhibits. Set the sign of three cell types.
//
// The wiring of the compass (148 cells, 9,487 measured connections, circuits/a_signs_given.json, made by circuits/tools/gen_atlas_compass.py from data/connectome/hemibrain/compass.json) is given and locked, with
// the fitted gains in the stored weights. Four cell-type groups: E-PG, P-EG and Delta7 each have a sign the player sets (given excitatory); P-EN has nothing free (its sign barely changes the result in this
// model, research/atlas-a.md 4c). The world is the cue-and-hold world of atlas_compass.h, at the cues signs_cues().
// WHAT PASSES: the bump holds where it was put at every one of those cues (atlas_compass.h: within 30 degrees, compact, 2 to 60 Hz). Measured, not stored: tasks_atlas_compass_test holds the run to
// "E-PG excitatory, P-EG excitatory, Delta7 inhibitory" (the reference) and asserts that each of the seven other assignments fails, on 40 circuit seeds (research/atlas-a.md 4d).
#include "atlas_compass.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_signs();
namespace
{
using namespace atlas_compass;

class ASigns : public CompassRun
{
public:
  ASigns() : CompassRun( info(), signs_cues() ) { reset( 1 ); }

  static Info info()
  {
    Info i = world( "a_signs", "Set the signs", signs_cues() );
    i.brief = "The wiring of the fly's compass was measured in an electron-microscope volume: which cell connects to which, and with how many synapses. It does not say whether a connection excites or inhibits. "
              "That has to be assumed, and the wrong assumption gives a circuit that does not work. The wiring and its gains are given and cannot be edited. Three cell types have a card with a sign: the compass cells "
              "(E-PG), the P-EG cells and the Delta7 cells. A fourth card, the P-EN cells, is fixed: in this model its sign barely matters. Set each sign so that the compass holds a bump of activity where a cue puts it. "
              "A cue is shown at four places in turn, with a reset between them, and the bump has to stay at each one until the end. The level checks that it does.";
    i.concepts_new = { "synaptic_sign" };
    i.concepts_used = { "ring_attractor", "inhibition" };
    i.recipe = "Three cards, E-PG, P-EG and Delta7, each excitatory or inhibitory. A ring that holds a bump needs cells that excite their neighbours and cells that inhibit the whole ring. "
               "Run the cues and watch the ring: if the activity dies out, or spreads over the whole ring, at least one sign is wrong. E-PG and P-EG excite; Delta7 inhibits.";
    i.note = { "Wiring is not sign",
               "An electron-microscope reconstruction gives the connections of a circuit and the number of synapses at each, but not whether a synapse excites or inhibits the cell it ends on: that is a "
               "property of the transmitter and the receptor, and it is measured with physiology (Franconville et al. 2018 tested 70 pairs of central-complex cell types this way). Here the signs are "
               "assumptions that the level lets you test: in this model only one assignment of the three signs holds a bump. The model is made of simple point cells, and is not the fly.",
               "Scheffer LK et al. (2020) eLife 9:e57443, doi:10.7554/eLife.57443; Franconville R, Beron C, Jayaraman V (2018) eLife 7:e37017, doi:10.7554/eLife.37017; "
               "Turner-Evans DB et al. (2020) Neuron 108:145-163, doi:10.1016/j.neuron.2020.08.006" };
    return i;
  }

  std::string level_kind() const override { return "atlas"; }

  Given given() const override
  {
    Given g;
    g.circuit = "a_signs_given.json";
    // printed by `spike-run --locked-digest circuits/a_signs_given.json`; tasks_atlas_compass_test checks that the given and the reference circuits both have exactly these
    g.digest.pops = "fe6963039bfc82301f8be725d63ea5dc";
    g.digest.wiring = "17f3c5dc11cc7c156bc681a04919aafc";
    g.digest.types = "c266b127e5a48b5c047f5d964a1cc82a";
    g.digest.channels = "ad2768fd684cc647b82ae1abd7d16b11";
    return g;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A ring that holds one bump of activity needs two things: cells that excite their neighbours, so the bump keeps itself going, and cells that inhibit the ring as a whole, so there is only one bump. The wiring does not say which cells do which.", "" },
      { 2, "Look at the cell type that connects to the whole ring, the Delta seven cells, and at the cells that make up the ring itself. Each card has a sign. Run a cue and watch whether the activity dies out or spreads.", "DELTA" },
      { 3, "Compass cells and P-EG cells excite the cells they connect to. The Delta seven cells inhibit them. Set the three signs and run the cues again.", "DELTA" } };
  }

  std::map< std::string, Metric > metrics() const override { return { { "cues_held", { static_cast< double >( held_count() ), "cues", false } } }; }

  Score score() const override
  {
    Score s;
    fill_parts( s );
    score_holds( *this, done(), wiring_.ok(), wiring_.message(), mech_, s,
                 "The bump stays where it was put, at every cue. In this model the compass holds a bump with these signs.",
                 " In this model at least one of the three signs is wrong: each cell type excites all the cells it connects to, or inhibits all of them." );
    apply_mech( s );
    return s;
  }

protected:
  void inspect( Engine& ) override
  {
    if ( sign_of( "EPG" ) == 0 || sign_of( "PEG" ) == 0 || sign_of( "DELTA" ) == 0 ) mech_ = "The cell-type cards are missing: load the given circuit again.";
  }

private:
  int held_count() const
  {
    int n = 0;
    for ( const auto& b : bumps() ) n += b.held ? 1 : 0;
    return n;
  }
};
}  // namespace

std::unique_ptr< Task > make_a_signs() { return std::make_unique< ASigns >(); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_signs, 995, "a_signs.json", make_a_signs ) }
