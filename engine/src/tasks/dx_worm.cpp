// Diagnosis case "dx_worm": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying task (worm_touch, see dx_common.h);
// the circuit given to the player is circuits/dx_worm.json (the reference circuit of worm_touch minus the AVA > motor-neuron connection, which stays in the file at weight 0).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_worm();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_worm";
  s.title = "Diagnosis: the worm that cannot back up";
  s.inner = "worm_touch";
  s.chapter = "worm";
  s.vignette = "(A laboratory animal.) A C. elegans from a mutant strain crawls and feeds normally, but when its head is touched with an eyelash it does not back away. "
               "You are given the circuit of its touch response: the touch cells, the backward and forward command interneurons and the two groups of motor neurons.";
  s.symptoms = "A touch on the tail makes it run forward as usual. A touch on the head makes it stop and nothing else: the backward motor neurons do not fire, and the forward motor "
               "neurons are held silent as well. The touch cells and the command interneurons of the head pathway are active.";
  s.candidates = { "AVA>MA", "AVD>AVA", "PVC>AVB", "AVB>MB", "PLM>PVC", "ALM>AVD+AVM>AVD" };
  s.answer = "AVA>MA";
  s.labels = {
    { "AVA>MA", "the backward command cells (AVA) no longer drive the backward motor neurons" },
    { "AVD>AVA", "the head-touch interneurons (AVD) no longer drive the backward command cells (AVA)" },
    { "PVC>AVB", "the tail-touch interneurons (PVC) no longer drive the forward command cells (AVB)" },
    { "AVB>MB", "the forward command cells (AVB) no longer drive the forward motor neurons" },
    { "PLM>PVC", "the tail touch cells (PLM) no longer drive the tail-touch interneurons (PVC)" },
    { "ALM>AVD+AVM>AVD", "the head touch cells (ALM and AVM) no longer drive the head-touch interneurons (AVD)" } };
  s.explain = {
    { "AVA>MA", "The head touch reaches the backward command cells and they fire, and they still silence the forward command, but the backward motor neurons stay silent: the broken link is the one that carries the command to them." },
    { "AVD>AVA", "With that link cut the backward command cells would be silent after a head touch and could not hold the forward command back. In this patient they are active and the forward motors stay silent, so the break is further down the chain." },
    { "PVC>AVB", "That is a break in the tail pathway: a tail touch would no longer make the worm go forward. This worm runs forward normally, so the tail pathway is intact." },
    { "AVB>MB", "That would silence the forward motor neurons after a tail touch as well. This worm runs forward normally, so the forward command reaches its motors." },
    { "PLM>PVC", "That is a break in the tail pathway: a tail touch would no longer make the worm go forward. This worm runs forward normally." },
    { "ALM>AVD+AVM>AVD", "With the head touch cells cut off, the head-touch interneurons would be silent after a head touch. In this patient they fire, so the signal gets past the touch cells." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "command_switch", "persistence" };
  s.hint1 = "A reversal needs a chain of cells: touch cell, command interneuron, motor neuron. Which cell is active while the cell after it stays silent?";
  s.hint2 = "Watch the backward command cells and the backward motor neurons while the head is touched.";
  s.hint2_target = "MA";
  s.note = { "Finding a circuit by killing cells",
             "Chalfie and colleagues worked out the touch circuit of C. elegans by killing single cells with a laser and testing how the animal responded to touch. Killing the "
             "interneurons on one pathway abolished the response that pathway carried and left the other intact. This case does the same on the model: the lesion is one missing "
             "connection, and the test is which cell fires and which cell after it does not. The case is fictional; the circuit is the reference of the touch level.",
             "Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) J Neurosci 5:956-964, doi:10.1523/JNEUROSCI.05-04-00956.1985" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_worm() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_worm, 503, "dx_worm.json", make_dx_worm ) }
