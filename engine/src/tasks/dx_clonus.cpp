// Diagnosis case "dx_clonus": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying
// task (see dx_common.h); the circuit given to the player is circuits/dx_clonus.json (reference circuit minus the missing connection).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_clonus();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_clonus";
  s.title = "Diagnosis: the shaky arm";
  s.inner = "renshaw";
  s.chapter = "spinal";
  s.vignette = "(A fictional patient.) A 54-year-old man, a year after a cervical spinal cord injury, says his arm trembles whenever he holds it out, and his "
               "physiotherapist notes that the muscles 'run away' with small stretches. You are given the circuit of his motor pool: the "
               "stretch reflex and its Renshaw cells.";
  s.symptoms = "Postural tremor at about 10 Hz that nothing damps: the force wobble is not reduced the way a healthy motor pool reduces it, and the "
               "motor units fire very fast (about 48 impulses/s per cell against about 30 normally) for the same effort. The stretch reflex "
               "itself is present and of nearly normal strength. Both the flexor and the extensor pools are involved.";
  s.candidates = { "MN_F>RC_F+MN_E>RC_E", "RC_F>MN_F+RC_E>MN_E", "Ia_F>MN_F+Ia_E>MN_E", "Ia_F>IN_F+Ia_E>IN_E", "DESC>MN_F+DESC>MN_E", "RC_F>IN_F+RC_E>IN_E" };
  s.answer = "MN_F>RC_F+MN_E>RC_E";
  s.labels = {
    { "MN_F>RC_F+MN_E>RC_E", "motoneurons no longer excite their Renshaw cells (recurrent inhibition is never recruited)" },
    { "RC_F>MN_F+RC_E>MN_E", "Renshaw cells fire but no longer inhibit the motoneurons" },
    { "Ia_F>MN_F+Ia_E>MN_E", "spindle afferents no longer excite the motoneurons (stretch reflex lost)" },
    { "Ia_F>IN_F+Ia_E>IN_E", "spindle afferents no longer excite the Ia inhibitory interneurons (reciprocal inhibition lost)" },
    { "DESC>MN_F+DESC>MN_E", "the common descending input no longer reaches the motoneurons" },
    { "RC_F>IN_F+RC_E>IN_E", "Renshaw cells no longer inhibit the Ia interneurons" } };
  s.explain = {
    { "MN_F>RC_F+MN_E>RC_E", "The Renshaw cells are silent although the motoneurons fire hard: with no recurrent brake the pool rate rises and the 10 Hz common drive passes straight into force." },
    { "RC_F>MN_F+RC_E>MN_E", "The Renshaw cells would then be firing in step with the motoneurons (look at their scope); here they are silent, so the break is one synapse earlier." },
    { "Ia_F>MN_F+Ia_E>MN_E", "That would weaken the stretch reflex and lower the firing rate (about 20 Hz), but the Renshaw cells would still be firing hard; here they are silent and the pool fires at 48 Hz." },
    { "Ia_F>IN_F+Ia_E>IN_E", "That removes reciprocal inhibition (co-contraction on stretch), not the Renshaw brake: the Renshaw cells would still fire (about 77 Hz) and still damp the tremor." },
    { "DESC>MN_F+DESC>MN_E", "Removing the common drive would make the tremor smaller, not larger (the 10 Hz comes from that shared input)." },
    { "RC_F>IN_F+RC_E>IN_E", "That is a minor Renshaw output: the Renshaw cells would still be firing (about 65 Hz) and still braking the motoneurons themselves." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "recurrent_inhibition", "negative_feedback" };
  s.hint1 = "Tremor that grows when a feedback brake is missing: which cells provide the brake on a motor pool, and what drives them?";
  s.hint2 = "Look at the Renshaw cells' scope while the motor pool is firing hard.";
  s.hint2_target = "RC_F";
  s.note = { "Renshaw cells damp tremor",
             "Recurrent inhibition by Renshaw cells reduces the 8-12 Hz coupling between the motor cortex and the muscle, so the physiological tremor is smaller "
             "than it would be with the brake off. This case is a model, not a clinical entity: an isolated loss of Renshaw excitation is not a recognised cause of any "
             "disease, and the game's patient shows too much tremor, not clonus. Clinical clonus is a 5-8 Hz stretch-evoked oscillation of a hyperexcitable stretch "
             "reflex loop after upper motor neuron injury (Hidler & Rymer 1999 model it as reflex instability); reduced recurrent inhibition has only been proposed "
             "as one possible contributor to spasticity. In the game the missing wire is the one that recruits the Renshaw cells, because that is what the puzzle can show.",
             "Williams ER, Baker SN (2009) J Neurosci 29:6616-6624, doi:10.1523/JNEUROSCI.0272-09.2009; Hultborn H (2006) Prog Neurobiol 78:215-232, doi:10.1016/j.pneurobio.2006.04.001; "
             "Hidler JM, Rymer WZ (1999) IEEE Trans Rehabil Eng 7:327-340; Pierrot-Deseilligny E, Burke D (2005) The Circuitry of the Human Spinal Cord" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_clonus() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_clonus, 140, "dx_clonus.json", make_dx_clonus ) }
