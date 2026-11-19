// Diagnosis case "dx_mb_apl": see DxSpec below. The world is the ordinary world of fly_mb (dx_common.h); the circuit given to the player is circuits/dx_mb_apl.json (the reference circuit of fly_mb minus the APL's
// output onto the Kenyon cells, which stays in the file at weight 0).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_mb_apl();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_mb_apl";
  s.title = "Diagnosis: the fly that cannot tell odours apart";
  s.inner = "fly_mb";
  s.chapter = "fly_mb";
  s.vignette = "(A laboratory animal.) A fruit fly from a mutant line is trained to avoid one odour. You are given the circuit model of its mushroom body: the glomeruli, the projection neurons, the Kenyon cells, the giant "
               "inhibitory neuron APL, the output neurons that drive avoidance, and the punishment pathway to the dopamine neurons.";
  s.symptoms = "The fly answers both odours before training, the punishment pathway works, and the output neurons are weakened for the punished odour. But the weakening is not specific: after training the fly avoids both odours. "
               "Recording the Kenyon cells, a large share of them fire for each odour, and the two odours excite many of the same cells. The APL fires whenever the Kenyon cells are active.";
  s.candidates = { "PN>KC", "KC>APL", "APL>KC", "KC>MBON", "TSN>DAN" };
  s.answer = "APL>KC";
  s.labels = {
    { "PN>KC", "the projection neurons no longer drive the Kenyon cells" },
    { "KC>APL", "the Kenyon cells no longer excite the APL" },
    { "APL>KC", "the APL no longer inhibits the Kenyon cells" },
    { "KC>MBON", "the Kenyon cells no longer drive the avoidance output neurons" },
    { "TSN>DAN", "the punishment cell no longer drives the dopamine neurons" } };
  s.explain = {
    { "PN>KC", "Then no Kenyon cell would fire for an odour, and nothing could be learned or answered. Here the Kenyon cells fire, in large numbers." },
    { "KC>APL", "That would also leave the Kenyon cells without feedback inhibition, but then the APL would be silent. Here the APL fires when the Kenyon cells do, so it still hears them; its output is what is missing." },
    { "APL>KC", "The APL is driven by the Kenyon cells but its inhibition no longer reaches them: the feedback loop is open, the code is dense, the odours overlap, and what is learned about one odour is learned about the other." },
    { "KC>MBON", "Then the output neurons would not answer the odours at all. Here they answer both odours and are weakened by the training." },
    { "TSN>DAN", "Then the dopamine neurons would be silent in the punishment and nothing would be learned. Here the output neurons are weakened by the pairing, only too widely." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "sparse_coding", "dopamine_gated_depression" };
  s.hint1 = "A memory that spreads to the wrong odour means the codes of the two odours overlap. What keeps a code sparse?";
  s.hint2 = "Watch the Kenyon cells and the APL while an odour is on: how many Kenyon cells fire, and does the APL's activity reach them?";
  s.hint2_target = "APL";
  s.note = { "A dense code cannot be specific",
             "In the mushroom body each odour excites a small subset of the Kenyon cells, and the giant APL neuron keeps it small by feedback inhibition. When that inhibition is removed the code becomes dense and the "
             "subsets of different odours overlap, so a synapse that is weakened by dopamine for one odour is shared by others: the memory generalises. In flies the overlap between the sparse representations of two odours "
             "predicts how much a memory generalises between them. This case is fictional; the lesion is one missing connection in the model.",
             "Hige T, Aso Y, Modi MN, Rubin GM, Turner GC (2015) Neuron 88:985-998, doi:10.1016/j.neuron.2015.11.003; Papadopoulou M, Cassenaer S, Nowotny T, Laurent G (2011) Science 332:721-725, doi:10.1126/science.1201835" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_mb_apl() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_mb_apl, 543, "dx_mb_apl.json", make_dx_mb_apl ) }
