// Diagnosis case "dx_compass": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying task (fly_compass, see dx_common.h);
// the circuit given to the player is circuits/dx_compass.json (the reference circuit of fly_compass minus the left shifters' output onto the ring, which stays in the file at weight 0).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_compass();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_compass";
  s.title = "Diagnosis: the compass that walks off";
  s.inner = "fly_compass";
  s.chapter = "fly_compass";
  s.vignette = "(A laboratory animal.) A fruit fly from a mutant line is tested on a heading task: a landmark is shown, the lights go out, and the fly is made to turn left and right in the dark. "
               "You are given the circuit model of its compass: the ring of compass cells, the global inhibition, and the two groups of shifting cells, one for each direction of turn.";
  s.symptoms = "With the lights out and the fly still, the bump holds its place: the ring itself is fine. A turn to the right is followed correctly. A turn to the left is not: the compass hardly registers it, so after "
               "a right turn and a left turn the compass reports a heading that is off in one direction, and every further back-and-forth adds to the error.";
  s.candidates = { "PENL>EPG", "PENR>EPG", "EPG>PENL", "DELTA>EPG", "EPG>DELTA" };
  s.answer = "PENL>EPG";
  s.labels = {
    { "PENL>EPG", "the left shifting cells no longer excite the ring" },
    { "PENR>EPG", "the right shifting cells no longer excite the ring" },
    { "EPG>PENL", "the ring cells no longer excite the left shifting cells" },
    { "DELTA>EPG", "the global inhibition no longer acts on the ring" },
    { "EPG>DELTA", "the ring no longer excites the global inhibitory cells" } };
  s.explain = {
    { "PENL>EPG", "The left shifting cells are active (they receive the ring and the turn signal) but the bump they should push does not move: their output onto the ring is missing, so left turns are not integrated while right turns are." },
    { "PENR>EPG", "That would break the right turns, not the left ones. Here right turns are followed correctly." },
    { "EPG>PENL", "That would silence the left shifting cells, because they need the ring as well as the turn signal. Here they are active during a left turn, so the ring still reaches them." },
    { "DELTA>EPG", "Without the global inhibition the bump would spread over the ring and tracking would fail in both directions. Here the bump is a single clean bump that holds and follows right turns." },
    { "EPG>DELTA", "Without the inhibitory cells being driven the ring would lose its single bump and fire everywhere. Here the bump is clean and right turns are followed." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "ring_attractor", "path_integration" };
  s.hint1 = "Only one direction of turn fails. Which part of the circuit is used only for that direction?";
  s.hint2 = "Watch the left shifting cells during a left turn: do they fire? And does the bump move when they do?";
  s.hint2_target = "PENL";
  s.hint3 = "The break is in the part of the circuit that moves the bump (the shifting cells and their connections with the ring), not in the ring's own inhibition.";
  s.hint3_target = "PENL";
  s.note = { "A compass that tracks one way",
             "In the fly the left and right P-EN cells each move the E-PG bump one way, and the angular velocity of the animal is carried by the difference of their activity. Silencing one group of "
             "shifting cells breaks integration for turns in that direction, so the compass drifts: it keeps following turns one way and loses the other. This case is fictional; the lesion is one missing connection in the model.",
             "Turner-Evans D et al. (2017) eLife 6:e23496, doi:10.7554/eLife.23496; Green J et al. (2017) Nature 546:101-106, doi:10.1038/nature22343" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_compass() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_compass, 533, "dx_compass.json", make_dx_compass ) }
