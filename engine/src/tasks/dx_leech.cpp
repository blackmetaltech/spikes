// Diagnosis case "dx_leech": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying task (leech_heart, see dx_common.h);
// the circuit given to the player is circuits/dx_leech.json (the reference circuit of leech_heart minus the left-to-right inhibition, which stays in the file at weight 0).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_leech();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_leech";
  s.title = "Diagnosis: the one-sided heartbeat";
  s.inner = "leech_heart";
  s.chapter = "leech";
  s.vignette = "(A laboratory animal.) A medicinal leech has been dissected so that the heartbeat timing network and its two heart motor neurons can be recorded. The usual picture is "
               "a rhythm in which the two sides take turns. You are given the circuit of this preparation: two heart interneurons that should inhibit each other, and the heart motor neuron each of them inhibits.";
  s.symptoms = "The two sides of the heart no longer take turns: the left heart tube is driven steadily and the right one is almost still. Recordings from the two heart motor neurons show no bursts that give way to the other side. A small extra current into the left side, as in the lab's test, changes nothing about this: still no turns.";
  s.candidates = { "HN_L>HN_R", "HN_R>HN_L", "HN_L>HE_L", "HN_R>HE_R" };
  s.answer = "HN_L>HN_R";
  s.labels = {
    { "HN_L>HN_R", "the left heart interneuron no longer inhibits the right one" },
    { "HN_R>HN_L", "the right heart interneuron no longer inhibits the left one" },
    { "HN_L>HE_L", "the left heart interneuron no longer inhibits its own motor neuron" },
    { "HN_R>HE_R", "the right heart interneuron no longer inhibits its own motor neuron" } };
  s.explain = {
    { "HN_L>HN_R", "With that inhibition missing the right interneuron is never silenced by the left one, so it wins and keeps the left interneuron down; its own motor neuron is held silent and the left motor neuron, freed from inhibition, fires steadily." },
    { "HN_R>HN_L", "That would make the left interneuron the winner: the left motor neuron would be the silent one and the right one would fire steadily. Here it is the right interneuron that fires strongly and the left motor neuron that fires steadily." },
    { "HN_L>HE_L", "That would leave the left motor neuron firing steadily, but the two interneurons would still alternate and the right motor neuron would still take turns. Here the right motor neuron is nearly silent and the interneurons do not alternate." },
    { "HN_R>HE_R", "That would leave the right motor neuron firing steadily. Here it is the right motor neuron that is silent, and the interneurons do not take turns, which a cut after them cannot cause." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "half_centre", "inhibition" };
  s.hint1 = "In a half-centre both sides must be able to silence each other. If one side cannot, which side wins, and what happens to the other?";
  s.hint2 = "Compare the two heart interneurons: one is strong, one is held down. Which inhibition would have to be missing for that?";
  s.hint2_target = "HN_R";
  s.hint3 = "The break involves the left side of the preparation: it is a connection that starts from or ends on the left heart interneuron.";
  s.hint3_target = "HN_L";
  s.note = { "A half-centre with one inhibition missing",
             "The leech heartbeat oscillator is a pair of reciprocally inhibitory interneurons; each fires a burst while the other is silent. If one direction of inhibition is lost, the "
             "side that is not inhibited wins and the rhythm stops: the oscillator then behaves like a switch that is stuck. Real preparations in which one HN is silenced or removed "
             "also lose the pattern. This case is fictional and the lesion is a model of the loss of one inhibitory link.",
             "Hill AA, Lu J, Masino MA, Olsen OH, Calabrese RL (2001) J Comput Neurosci 10:281-302, doi:10.1023/a:1011216131638; Hill AAV, Masino MA, Calabrese RL (2002) J Neurophysiol 87:1586-1602, doi:10.1152/jn.00337.2001" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_leech() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_leech, 513, "dx_leech.json", make_dx_leech ) }
