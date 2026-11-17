// Diagnosis case "dx_horner": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying
// task (see dx_common.h); the circuit given to the player is circuits/dx_horner.json (reference circuit minus the missing connection).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_horner();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_horner";
  s.title = "Diagnosis: the small pupil";
  s.inner = "pupil";
  s.chapter = "autonomic";
  s.vignette = "A 61-year-old smoker notices that one pupil is smaller than the other, and the upper lid droops a little on that side. You are given the "
               "circuit of that eye's pupil control (the light reflex and the sympathetic dilator pathway); the examiner flashes a bright light, "
               "darkens the room, and finally startles the patient.";
  s.symptoms = "The pupil is small (miosis): in the dark it opens only to about 5 mm instead of 7, and a sudden fright adds little. It still "
               "reacts briskly to a bright light (to about 2.6 mm in a quarter of a second): the light reflex itself is normal.";
  s.candidates = { "IML>SCG", "PON>EW", "EW>CG", "RGC>PON", "HYP>IML+HYP>EW" };
  s.answer = "IML>SCG";
  s.labels = {
    { "IML>SCG", "the sympathetic chain (spinal preganglionic neurons) no longer drives the superior cervical ganglion, so the dilator is silent" },
    { "PON>EW", "the pretectum no longer excites the Edinger-Westphal nucleus" },
    { "EW>CG", "the Edinger-Westphal nucleus no longer excites the ciliary ganglion" },
    { "RGC>PON", "the retina no longer reaches the pretectal nucleus" },
    { "HYP>IML+HYP>EW", "the hypothalamic (alarm and arousal) input no longer reaches the sympathetic or Edinger-Westphal neurons" } };
  s.explain = {
    { "IML>SCG", "Horner syndrome: the sphincter is intact (normal light reflex) but the dilator has no sympathetic drive, so the pupil is small and reopens only as far as parasympathetic withdrawal allows." },
    { "PON>EW", "That cripples the light reflex (the pupil only gets to about 5 mm in bright light, by sympathetic withdrawal alone) while the pupil opens fully in the dark (7 mm); here it is the other way round." },
    { "EW>CG", "Same result as the pretectal cut one synapse later: a poor light reflex (5 mm in bright light) and a normal 7 mm in the dark; the opposite of this patient." },
    { "RGC>PON", "The pupil would not react to light at all and sit wide at 7 mm (afferent defect); this one reacts briskly and is small." },
    { "HYP>IML+HYP>EW", "That removes the response to fright (+0.3 mm) but the pupil would still open fully in the dark (7 mm) and react to light normally; this pupil does not open in the dark." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "negative_feedback", "inhibition" };
  s.hint1 = "A small pupil that still reacts to light: which of the two iris muscles has lost its drive?";
  s.hint2 = "Find the pool that should be firing in the dark and compare it with the cells that feed it.";
  s.hint2_target = "SCG";
  s.note = { "Horner syndrome",
             "Interrupting the three-neuron sympathetic pathway to the eye (hypothalamus to the spinal cord, then the sympathetic chain, then the superior cervical ganglion) "
             "gives a small pupil, a slightly drooping lid and dilation lag in the dark, while the light reflex, which is parasympathetic, stays normal. "
             "Common causes are carotid dissection and apical lung tumours.",
             "Kanagalingam S, Miller NR (2015) Eye Brain 7:35; Loewenfeld IE (1993) The Pupil; McDougal DH, Gamlin PD (2015) Compr Physiol 5:439" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_horner() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_horner, 440, "dx_horner.json", make_dx_horner ) }
