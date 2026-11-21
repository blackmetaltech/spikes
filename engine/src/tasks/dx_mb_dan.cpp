// Diagnosis case "dx_mb_dan": see DxSpec below. The world is the ordinary world of fly_mb (dx_common.h); the circuit given to the player is circuits/dx_mb_dan.json (the reference circuit of fly_mb minus the punishment cell's
// output onto the dopamine neurons, which stays in the file at weight 0).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_mb_dan();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_mb_dan";
  s.title = "Diagnosis: the fly that never learns";
  s.inner = "fly_mb";
  s.chapter = "fly_mb";
  s.vignette = "(A laboratory animal.) A fruit fly from a mutant line is trained to avoid one odour, with the usual pairings of the odour and a punishment. You are given the circuit model of its mushroom body: the glomeruli, the "
               "projection neurons, the Kenyon cells, the APL, the output neurons that drive avoidance, and the punishment pathway to the dopamine neurons.";
  s.symptoms = "Before training the fly answers both odours, and the Kenyon-cell code is sparse and different for the two. After training nothing has changed: it answers the punished odour exactly as before. It does react to the shock itself, so it feels the punishment.";
  s.candidates = { "PN>KC", "KC>APL", "APL>KC", "KC>MBON", "TSN>DAN" };
  s.answer = "TSN>DAN";
  s.labels = {
    { "PN>KC", "the projection neurons no longer drive the Kenyon cells" },
    { "KC>APL", "the Kenyon cells no longer excite the APL" },
    { "APL>KC", "the APL no longer inhibits the Kenyon cells" },
    { "KC>MBON", "the Kenyon cells no longer drive the avoidance output neurons" },
    { "TSN>DAN", "the punishment cell no longer drives the dopamine neurons" } };
  s.explain = {
    { "PN>KC", "Then no Kenyon cell would fire for an odour, and the output neurons would not answer it. Here the odours are answered normally." },
    { "KC>APL", "That would make the code dense and the memory non-specific, but the memory would still form. Here nothing is learned." },
    { "APL>KC", "That would make the code dense, and what is learned would spread to the other odour. Here the code is sparse and nothing is learned at all." },
    { "KC>MBON", "Then the output neurons would not answer the odours at all, and here they do." },
    { "TSN>DAN", "The punishment cell fires, but the dopamine neurons it should excite are silent: the teaching signal never reaches the synapse, so the output neurons are never weakened, although everything else works." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "sparse_coding", "dopamine_gated_depression" };
  s.hint1 = "Learning needs a teaching signal. Which cells carry it, and do they fire when the fly is punished?";
  s.hint2 = "Watch the dopamine neurons during the punishment: do they fire when the punishment cell does?";
  s.hint2_target = "DAN";
  s.hint3 = "The break is in the learning stage of the circuit (the pathway of the punishment and the output neurons), not in the part that makes the odour code.";
  s.hint3_target = "DAN";
  s.note = { "No teaching signal, no memory",
             "In the mushroom body the synapses of the Kenyon cells onto an output neuron change only when dopamine neurons of that compartment fire at the right time after the odour. A fly whose dopamine neurons are silent "
             "or disconnected can smell and respond normally, yet it does not learn to avoid the odour: the memory needs the odour, the output cell and the dopamine signal together. This case is fictional; the lesion is one missing connection in the model.",
             "Hige T, Aso Y, Modi MN, Rubin GM, Turner GC (2015) Neuron 88:985-998, doi:10.1016/j.neuron.2015.11.003; Aso Y et al. (2014) eLife 3:e04577, doi:10.7554/eLife.04577" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_mb_dan() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_mb_dan, 544, "dx_mb_dan.json", make_dx_mb_dan ) }
