// Diagnosis case "dx_achalasia": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying
// task (see dx_common.h); the circuit given to the player is circuits/dx_achalasia.json (reference circuit minus the missing connection).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_achalasia();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_achalasia";
  s.title = "Diagnosis: food that will not pass";
  s.inner = "peristalsis";
  s.chapter = "autonomic";
  s.vignette = "A 45-year-old man says swallowed food 'sticks' and has to be washed down, and sometimes comes back up. You are given the circuit of a gut "
               "segment chain (here, a bolus is placed in the tube and the enteric nervous system must move it along).";
  s.symptoms = "The bolus does move, but very slowly: the muscle ahead of it tightens instead of relaxing, so the passage takes more than 30 s where "
               "the normal gut needs 15. The segments do sense the bolus, and the slow waves are present.";
  s.candidates = { "DIN>IMN", "S>AIN+S>DIN", "AIN>EMN", "ICCP>ICC+ICC>ICC" };
  s.answer = "DIN>IMN";
  s.labels = {
    { "DIN>IMN", "the descending inhibitory interneurons no longer drive the inhibitory (NO/VIP) motoneurons" },
    { "S>AIN+S>DIN", "the sensory neurons no longer drive either interneuron chain (the bolus is not sensed)" },
    { "AIN>EMN", "the ascending interneurons no longer drive the excitatory (ACh) motoneurons" },
    { "ICCP>ICC+ICC>ICC", "the slow-wave pacemaker cells no longer drive each other (no slow waves)" } };
  s.explain = {
    { "DIN>IMN", "Without descending inhibition the segments ahead of the bolus contract against it instead of relaxing (the achalasia-like pattern): passage takes over 30 s." },
    { "S>AIN+S>DIN", "Then the reflex would not start at all: the bolus stalls about 2.5 segments in (and the inhibitory motoneurons never fire), instead of creeping through." },
    { "AIN>EMN", "The tone and slow waves still carry the bolus, only a little slower than normal (arrival at about 16 s); the passage here is far slower." },
    { "ICCP>ICC+ICC>ICC", "Without slow waves the bolus does not finish the passage in time (about 6.4 of 7 segments) but the inhibitory motoneurons are still active, so the muscle ahead of it does relax; look for the slow-wave cells that never fire." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "inhibition", "traveling_waves" };
  s.hint1 = "Propulsion needs the gut to contract behind the bolus and RELAX ahead of it. Which half is failing?";
  s.hint2 = "Compare the inhibitory motoneuron pool with the interneurons that should drive it.";
  s.hint2_target = "IMN";
  s.note = { "Achalasia",
             "In achalasia the inhibitory (nitric oxide / VIP) neurons of the oesophageal myenteric plexus are lost, so the lower sphincter and the segments ahead of a swallowed bolus "
             "do not relax. Food stalls, the oesophagus dilates, and propulsion fails although the excitatory neurons and the muscle are intact.",
             "Goyal RK, Chaudhury A (2010) Gastroenterology 139:1086; Pandolfino JE, Gawron AJ (2015) JAMA 313:1841; Bayliss WM, Starling EH (1899) J Physiol 24:99" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_achalasia() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_achalasia, 441, "dx_achalasia.json", make_dx_achalasia ) }
