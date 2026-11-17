// Diagnosis case "dx_vagal": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying
// task (see dx_common.h); the circuit given to the player is circuits/dx_vagal.json (reference circuit minus the missing connection).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_vagal();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_vagal";
  s.title = "Diagnosis: the racing resting pulse";
  s.inner = "heart_autonomic";
  s.chapter = "heart";
  s.vignette = "A 47-year-old woman has a resting pulse of about 100 beats per minute that nothing seems to slow. She is otherwise well. You are given the "
               "circuit that controls her heart rate through the autonomic nerves (rest, exercise, recovery).";
  s.symptoms = "Regular pulse near 100/min at rest with almost no beat-to-beat variability. Exercise raises it to about 130, but afterwards it does not "
               "fall back below 100. An injection of atropine, which blocks the vagus, changes nothing.";
  s.candidates = { "NA>VG", "IML>SG", "CC>IML", "CC>NA", "HIGH>NA" };
  s.answer = "NA>VG";
  s.labels = {
    { "NA>VG", "the vagal motor neurons no longer excite the cardiac ganglion cells (vagotomy-like)" },
    { "IML>SG", "the sympathetic preganglionic neurons no longer excite the stellate ganglion" },
    { "CC>IML", "central command no longer reaches the sympathetic preganglionic neurons" },
    { "CC>NA", "central command no longer withdraws vagal tone at the start of exercise" },
    { "HIGH>NA", "the 'rate too high' signal no longer drives the vagal motor neurons" } };
  s.explain = {
    { "NA>VG", "With the vagus cut there is no brake on the pacemaker: it fires at its intrinsic rate (~100/min), variability is lost and atropine can do nothing more." },
    { "IML>SG", "A sympathetic cut leaves the resting rate normal (65/min: rest is under vagal control) and limits only the exercise rate (about 99/min)." },
    { "CC>IML", "Without central command the exercise rate is a little low (about 107/min) but the resting rate stays normal (65/min)." },
    { "CC>NA", "Central command withdraws vagal tone at the start of exercise; without it the exercise rate lags (about 102/min), but the resting rate is normal (65/min)." },
    { "HIGH>NA", "That only loses the vagal 'brake' when the rate overshoots: rest (65/min) and exercise (about 128/min) are still tracked within a few beats." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "neuromodulation", "negative_feedback" };
  s.hint1 = "Which branch of the autonomic system sets the resting heart rate, and what happens to the pacemaker without it?";
  s.hint2 = "Compare the vagal ganglion cells' rate (and the ACh level) with the heart rate.";
  s.hint2_target = "VG";
  s.note = { "The vagus holds the heart back",
             "The sinus node's own rate is about 100 beats per minute; vagal tone slows a resting heart to about 60-70. Blocking or cutting the vagus (atropine, vagotomy) "
             "raises the resting rate towards the intrinsic rate, which is why a healthy heart-transplant recipient, with no vagal supply, rests near 90-100.",
             "Jose AD, Collison D (1970) Cardiovasc Res 4:160; Robinson BF et al. (1966) Circ Res 19:400; Task Force (1996) Circulation 93:1043" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_vagal() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_vagal, 240, "dx_vagal.json", make_dx_vagal ) }
