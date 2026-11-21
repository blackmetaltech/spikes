// Diagnosis case "dx_aplysia": see DxSpec below for the patient, the findings and the biology. The world is the ordinary world of the underlying task (aplysia_gill, see dx_common.h);
// the circuit given to the player is circuits/dx_aplysia.json (the reference circuit of aplysia_gill minus the tail-to-facilitator connection, which stays in the file at weight 0).
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_aplysia();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_aplysia";
  s.title = "Diagnosis: the reflex that does not sensitise";
  s.inner = "aplysia_gill";
  s.chapter = "aplysia";
  s.vignette = "(A laboratory animal.) A sea slug from a breeding line is tested for the two simple kinds of learning of its gill-withdrawal reflex. You are given the circuit of the reflex: the "
               "siphon sensory cells and the gill motor neuron, and the tail pathway with its facilitating interneuron.";
  s.symptoms = "The reflex is normal: a siphon touch gives a clear withdrawal, repeated touches make it smaller, and after a rest it is back. A shock to the tail still makes the gill twitch by itself. "
               "But the touch after a tail shock is no stronger than the touch before it: the reflex does not sensitise.";
  s.candidates = { "SN>MN", "TSN>MN", "TSN>FN", "TSN>MN+TSN>FN" };
  s.answer = "TSN>FN";
  s.labels = {
    { "SN>MN", "the siphon sensory cells no longer drive the gill motor neuron" },
    { "TSN>MN", "the tail sensory cells no longer drive the gill motor neuron directly" },
    { "TSN>FN", "the tail sensory cells no longer drive the facilitating interneuron" },
    { "TSN>MN+TSN>FN", "the tail sensory cells no longer drive either the gill motor neuron or the facilitating interneuron" } };
  s.explain = {
    { "SN>MN", "That would remove the reflex altogether: even the first touch would give no withdrawal. Here the reflex is normal and habituates." },
    { "TSN>MN", "That would stop the shock from moving the gill by itself, but the facilitating interneuron would still fire and the touches after the shock would be stronger. Here the shock still twitches the gill and sensitisation fails." },
    { "TSN>FN", "The facilitating interneuron is silent during the shock although the tail sensory cells fire: no modulator is released, so the sensory synapse is not strengthened, while the direct effect of the shock on the gill is intact." },
    { "TSN>MN+TSN>FN", "That would also take away the twitch of the gill during the shock. In this patient the shock still moves the gill, so the tail cells still drive the motor neuron." } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "synaptic_depression", "heterosynaptic_facilitation" };
  s.hint1 = "Sensitisation needs a pathway that releases a modulator when the tail is shocked. Which cell must fire for that, and does it?";
  s.hint2 = "Watch the facilitating interneuron during the tail shock: it should fire when the tail sensory cells do.";
  s.hint2_target = "FN";
  s.hint3 = "The break is on the tail pathway, the connections that start from the tail sensory cells. The siphon pathway is not the problem.";
  s.hint3_target = "TSN";
  s.note = { "Learning that fails in one place",
             "Habituation and sensitisation of the Aplysia gill-withdrawal reflex are two separate processes that share one synapse: depression of the sensory-to-motor synapse is "
             "intrinsic to it, while sensitisation needs facilitating interneurons that release serotonin. An animal can therefore habituate perfectly and still fail to "
             "sensitise if the facilitating pathway is lost. This case is fictional; the lesion is one missing connection in the model.",
             "Castellucci V, Pinsker H, Kupfermann I, Kandel ER (1970) Science 167:1745-1748, doi:10.1126/science.167.3926.1745; Hawkins RD, Kandel ER, Bailey CH (2006) Biol Bull 210:174-191, doi:10.2307/4134556" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_aplysia() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_aplysia, 523, "dx_aplysia.json", make_dx_aplysia ) }
