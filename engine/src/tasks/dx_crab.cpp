// Diagnosis case "dx_crab": see DxSpec below. The world is the ordinary world of crab_pyloric (dx_common.h); the circuit given to the player is circuits/dx_crab.json: the reference circuit of crab_pyloric with its SECOND
// hand-picked parameter set (crab_pyloric_b.json, the same pattern from different conductances) minus the LP > PY inhibition, which stays in the file at weight 0. Cutting AB > PY or PD > PY (live connections here) hardly
// changes the pattern, because PY is inhibited by three sources: the three lesions look alike, and probing has to tell them apart.
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_crab();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_crab";
  s.title = "Diagnosis: the follower that does not wait";
  s.inner = "crab_pyloric";
  s.chapter = "crab";
  s.vignette = "(A laboratory animal.) A crab stomatogastric ganglion from a second animal, whose cells have different conductances from the textbook ones, is recorded. You are given the circuit model of its pyloric network: the pacemaker "
               "kernel (AB and PD), the lateral pyloric cell LP and the pyloric cells PY, with their inhibitory synapses.";
  s.symptoms = "The rhythm recorded from the motor nerves is regular, about one cycle a second, but the pattern is no longer triphasic. The last burst of the cycle begins early, at the same time as the middle one instead of after it, and lasts about as long as the whole quiet half-cycle. "
               "It is still held off during the first burst of the cycle.";
  s.candidates = { "LP>PY", "AB>PY", "PD>PY", "PY>LP", "LP>PD" };
  s.answer = "LP>PY";
  s.labels = {
    { "LP>PY", "LP no longer inhibits PY" },
    { "AB>PY", "the anterior burster no longer inhibits PY" },
    { "PD>PY", "the pyloric dilator cells no longer inhibit PY" },
    { "PY>LP", "PY no longer inhibits LP" },
    { "LP>PD", "LP no longer inhibits the pacemaker's PD cells" } };
  s.explain = {
    { "LP>PY", "PY is held back by the pacemaker and, in the second half of the quiet period, by LP. Without LP's inhibition PY fires as soon as the pacemaker lets go, together with LP: the order PD, LP, PY is lost, as here." },
    { "AB>PY", "PY has three sources of inhibition (AB, PD and LP) and the network has overlapping routes: losing the one from AB changes the pattern very little, and PY would still wait for LP. Here PY does not wait." },
    { "PD>PY", "As with AB, losing PD's inhibition of PY leaves the other two and the order intact. Here PY does not wait for LP." },
    { "PY>LP", "Without PY's inhibition of LP the alternation of the two followers breaks down (in this model one of them then fires twice or not at all in a cycle). Here every cycle is clean and only the last burst is early." },
    { "LP>PD", "LP's inhibition of the PD cells shortens the cycle slightly when it is lost, but it does not change when PY starts. Here PY fires early." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "pacemaker_kernel", "rebound" };   // degeneracy is not taught by any level, so the case does not use it
  s.hint1 = "PY has several sources of inhibition and the network can still work when some of them are lost. Which one is it that puts PY after LP?";
  s.hint2 = "Watch PY while LP fires: does LP's burst do anything to PY, as it should? Compare with what the pacemaker's burst does to it.";
  s.hint2_target = "PY";
  s.hint3 = "The break is in the part of the circuit that sets the timing of the follower cells at the end of the cycle: it touches the last follower, one of the connections that arrive on it or leave it.";
  s.hint3_target = "PY";
  s.note = { "Degeneracy makes lesions look alike",
             "Because many different combinations of synaptic strengths and membrane conductances give the same pyloric pattern, the loss of one of several parallel inputs may leave the pattern almost unchanged, while the loss of the one input that "
             "carries a particular timing role breaks it. Telling which connection is missing therefore takes recording from the cells on both sides of it, not just looking at the output. This case is fictional; the lesion is one missing connection in the model.",
             "Prinz AA, Bucher D, Marder E (2004) Nat Neurosci 7:1345-1352, doi:10.1038/nn1352; Marder E, Bucher D (2001) Curr Biol 11:R986-R996, doi:10.1016/s0960-9822(01)00581-4" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_crab() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_crab, 553, "dx_crab.json", make_dx_crab ) }
