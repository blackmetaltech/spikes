// Diagnosis case "dx_a_worm" (chapter a_worm, Atlas wing): a model worm that does not back up when its head is touched. The patient circuit (circuits/dx_a_worm.json, made by
// circuits/tools/gen_atlas_worm.py) is the touch circuit of the worm chapter with every cell and every connection, but the OUTPUTS of one interneuron class (AVA, AVB, AVD or PVC) carry
// nothing. The player finds which by recording. The world is the ordinary world of a_path (atlas_worm.h): head touch, the two motor groups.
//
// THE SIGNATURE (tools/connectome/worm_dx.py, research/atlas-a.md 4b): in all four lesions the touch cells fire and the backward motor neurons stay silent. What differs is which interneuron
// classes still fire after the touch. With the answer AVD silenced, AVD itself fires strongly (a lesion of AVA, AVB or PVC leaves it nearly silent): one read of the AVD class tells the answer
// from the other three, and the classes AVA, AVB and PVC fire too, so a read of the backward motor neurons shows the break is after the interneurons. The case is fictional.
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_a_worm();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_a_worm";
  s.title = "Diagnosis: the model worm that does not back up";
  s.inner = "a_path";
  s.chapter = "a_worm";
  s.vignette = "(A model worm, not an animal.) Its touch circuit has all its cells and all its connections, as in the wiring data. When its head is touched it does not back up. "
               "In this model one of the interneuron classes AVA, AVB, AVD and PVC has been silenced: it still receives input, but its output carries nothing.";
  s.symptoms = "The head touch cells fire as usual. The backward motor neurons stay silent during the touch and after it, and so do the forward ones. "
               "Which interneuron classes still fire after the touch is what you can record.";
  s.method = "Run the circuit and record. Open the scope of a cell class to see whether it fires after the head touch. The silenced class still fires when its input is strong enough, "
             "and the classes after it lose what it carried. You can also silence a class yourself in the lesion lab. Then name the silenced class. The case is scored on what it took: "
             "each wrong guess and each probe beyond the first few lowers the score.";
  s.candidates = { "pop:AVA", "pop:AVB", "pop:AVD", "pop:PVC" };
  s.answer = "pop:AVD";
  s.labels = { { "pop:AVA", "the interneuron class AVA carries nothing" }, { "pop:AVB", "the interneuron class AVB carries nothing" },
    { "pop:AVD", "the interneuron class AVD carries nothing" }, { "pop:PVC", "the interneuron class PVC carries nothing" } };
  s.explain = {
    { "pop:AVA", "With AVA silent, AVD would stay nearly silent after a head touch in this model. Here AVD fires strongly, so the fault is not in AVA." },
    { "pop:AVB", "With AVB silent, AVA and AVD would stay nearly silent after a head touch in this model. Here they fire strongly, so the fault is not in AVB." },
    { "pop:AVD", "AVD fires strongly after the head touch and the backward motor neurons stay silent: in this model the class that fails is the one that fires and passes nothing on." },
    { "pop:PVC", "With PVC silent, AVA, AVB and AVD would all stay nearly silent after a head touch in this model. Here they fire, so the fault is not in PVC." } };
  s.probes = { { "scope", "AVD", "separates the silenced class from the other three: it fires strongly only when its own output is the one that is cut" },
    { "cut", "pop:AVD", "silencing a class that is already silent changes nothing downstream" } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "pathway" };
  s.hint1 = "All four lesions give the same symptom, no backing up. They differ in which cells still fire after the touch. Record the classes between the touch cells and the backward motor neurons.";
  s.hint2 = "Open the scope of one of the interneuron classes and compare it with what it would do in a healthy worm.";
  s.hint2_target = "AVD";
  s.hint3 = "The silenced class is among the interneuron classes AVA, AVB, AVD and PVC. Compare how strongly each of them fires after the touch.";
  s.hint3_target = "AVA";
  s.note = { "Finding a circuit by killing cells",
             "Chalfie and colleagues worked out the touch circuit of C. elegans by reconstructing its wiring from electron micrographs and by killing cells with a laser and watching how the animal "
             "moved after touch. This case works the other way round on a model: one cell class has been silenced, and the recordings tell which. The case is fictional.",
             "Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) J Neurosci 5:956-964, doi:10.1523/JNEUROSCI.05-04-00956.1985" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_a_worm() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_a_worm, 994, "dx_a_worm.json", make_dx_a_worm ) }
