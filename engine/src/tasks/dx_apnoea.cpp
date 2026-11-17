// Diagnosis case "dx_apnoea": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying
// task (see dx_common.h); the circuit given to the player is circuits/dx_apnoea.json (reference circuit minus the missing connection).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_apnoea();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_apnoea";
  s.title = "Diagnosis: stops breathing in sleep";
  s.inner = "breathing";
  s.chapter = "breathing";
  s.vignette = "A newborn breathes well when awake and crying but becomes blue and stops breathing when she falls asleep. The monitor shows CO2 rising "
               "without any attempt to breathe harder. You are given the circuit of her brainstem breathing rhythm in sleep, driven by a steady "
               "background drive and by a chemoreceptor signal; the examiner raises the CO2 in the air she breathes from 50 s to 110 s.";
  s.symptoms = "In this sleep recording: slow, shallow breathing (about 8-10 per minute) with a high CO2. Breathing CO2-enriched air hardly increases her ventilation. "
               "The rhythm itself is regular and each breath has normal shape.";
  s.candidates = { "RTN>PBC+RTN>PHR", "PBC>PBC", "PBC>EXP", "EXP>PHR" };
  s.answer = "RTN>PBC+RTN>PHR";
  s.labels = {
    { "RTN>PBC+RTN>PHR", "the chemoreceptor relay (RTN) no longer reaches the rhythm generator or the phrenic motoneurons" },
    { "PBC>PBC", "the pre-Botzinger bursting cells are no longer coupled to each other" },
    { "PBC>EXP", "the rhythm generator no longer drives the expiratory cells that silence the phrenic pool between breaths" },
    { "EXP>PHR", "the expiratory cells no longer inhibit the phrenic motoneurons" } };
  s.explain = {
    { "RTN>PBC+RTN>PHR", "The rhythm survives on the steady drive but the CO2 signal cannot raise it: slow breathing and no CO2 response (congenital central hypoventilation syndrome, CCHS)." },
    { "PBC>PBC", "Uncoupled bursters (look at the PBC cells: they fire out of step) also lose the CO2 response, but resting breathing and CO2 stay normal (13/min, PaCO2 39); this patient already retains CO2 at rest (about 54 mmHg)." },
    { "PBC>EXP", "Without the expiratory cells being driven the phrenic pool shows no breaths at all (apnoea, PaCO2 above 120), not a slow regular rhythm." },
    { "EXP>PHR", "The phrenic pool then fires with nothing to silence it: rapid shallow breaths (about 47/min), not slow ones." },
    };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "rhythm_gating", "negative_feedback" };
  s.hint1 = "The rhythm is fine but the CO2 signal is not being used: where does the chemoreceptor information enter the circuit?";
  s.hint2 = "Watch the chemoreceptor relay cells and see whether they change the other cells' rates when CO2 is raised.";
  s.hint2_target = "RTN";
  s.note = { "Congenital central hypoventilation syndrome",
             "In congenital central hypoventilation syndrome, mutations of the PHOX2B gene remove the CO2-sensitive drive to the brainstem respiratory network. "
             "Patients may breathe adequately when awake, because cortical drive covers the deficit, but in sleep they hypoventilate or stop breathing, and they do not respond to rising CO2 (formerly called Ondine's curse).",
             "Amiel J et al. (2003) Nat Genet 33:459; Weese-Mayer DE et al. (2010) Am J Respir Crit Care Med 181:626; Guyenet PG, Stornetta RL, Bayliss DA (2010) Respir Physiol Neurobiol 173:244" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_apnoea() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_apnoea, 340, "dx_apnoea.json", make_dx_apnoea ) }
