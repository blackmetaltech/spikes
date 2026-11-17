// Diagnosis case "dx_orthostatic": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying
// task (see dx_common.h); the circuit given to the player is circuits/dx_orthostatic.json (reference circuit minus the missing connection).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_orthostatic();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_orthostatic";
  s.title = "Diagnosis: faint on standing";
  s.inner = "baroreflex";
  s.chapter = "heart";
  s.vignette = "A 68-year-old man sees stars and nearly faints every time he stands up, and has to sit down within a minute. You are given the circuit of "
               "his arterial baroreflex; the examiner stands him up at 15 s, lays him down at 60 s and later takes blood.";
  s.symptoms = "Blood pressure is low even lying down (about 68 mmHg mean) and falls further, to the mid-50s, on standing and after blood loss. "
               "His heart and arteries are healthy, the baroreceptors report the fall normally, and the brainstem cells that should answer are firing.";
  s.candidates = { "RVLM>IML", "AFF>NTS", "NTS>CVLM", "CVLM>RVLM", "NA>VG" };
  s.answer = "RVLM>IML";
  s.labels = {
    { "RVLM>IML", "the brainstem's sympathetic vasomotor neurons no longer reach the spinal sympathetic neurons" },
    { "AFF>NTS", "baroreceptor afferents no longer reach the solitary nucleus (sino-aortic denervation)" },
    { "NTS>CVLM", "the solitary nucleus no longer excites the depressor area (CVLM)" },
    { "CVLM>RVLM", "the depressor area no longer inhibits the vasomotor neurons" },
    { "NA>VG", "the cardiac vagal neurons no longer excite their ganglion cells" } };
  s.explain = {
    { "RVLM>IML", "The reflex senses the fall and the brainstem answers, but the command never reaches the vessels: no sympathetic tone, low pressure supine and a fall on standing (neurogenic orthostatic hypotension)." },
    { "AFF>NTS", "Denervation removes the brake on the sympathetic neurons: pressure runs HIGH and unregulated, not low." },
    { "NTS>CVLM", "A depressor-area lesion also releases the sympathetic neurons: pressure is high, not low." },
    { "CVLM>RVLM", "Without the inhibition the vasomotor neurons run unopposed: pressure is high, not low." },
    { "NA>VG", "A vagal cut speeds the heart (mean about 109/min) while pressure stays regulated (84-100 mmHg): this patient's pressure is low." },
    };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "negative_feedback", "inhibition" };
  s.hint1 = "Low pressure with a reflex that still 'sees' the fall: where could the command to the vessels be lost?";
  s.hint2 = "Compare the rate of the vasomotor neurons (RVLM) with the rate of the cells downstream of them, and with the NE trace.";
  s.hint2_target = "RVLM";
  s.note = { "Neurogenic orthostatic hypotension",
             "Standing pools about half a litre of blood in the legs; a healthy baroreflex raises sympathetic tone within seconds and holds the pressure. When the sympathetic "
             "outflow is interrupted (autonomic failure, spinal cord lesions) pressure falls on standing by 20 mmHg systolic or more without the usual rise in pulse.",
             "Freeman R et al. (2011) Clin Auton Res 21:69; Guyenet PG (2006) Nat Rev Neurosci 7:335; Dampney RAL (1994) Physiol Rev 74:323" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_orthostatic() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_orthostatic, 241, "dx_orthostatic.json", make_dx_orthostatic ) }
