// Diagnosis case "dx_a_worm" (chapter a_worm, Atlas wing): a model worm that does not back up when its head is touched. The patient circuit (circuits/dx_a_worm.json, made by
// circuits/tools/gen_atlas_worm.py) is the touch circuit of the worm chapter with every cell and every connection, but the OUTPUTS of one interneuron class (AVA, AVB, AVD or PVC) carry
// nothing. The player finds which by recording. The world is the ordinary world of a_path (atlas_worm.h): head touch, the two motor groups.
//
// THE SIGNATURE (tasks_atlas_worm_test prints it, research/atlas-a.md 4b): in all four lesions the touch cells fire. After the touch AVB fires clearly (20 Hz and more) when AVA, AVB or AVD is the
// silenced class, and stays nearly silent (6 Hz and less) only when PVC is: AVB depends on PVC for its drive in this model. So one read of the AVB class tells the answer (PVC) from the other
// three, on every circuit seed tried. In the answer's patient the backward and forward motor neurons are silent and AVA and AVD are silent too. The case is fictional.
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
  s.answer = "pop:PVC";
  s.labels = { { "pop:AVA", "the interneuron class AVA carries nothing" }, { "pop:AVB", "the interneuron class AVB carries nothing" },
    { "pop:AVD", "the interneuron class AVD carries nothing" }, { "pop:PVC", "the interneuron class PVC carries nothing" } };
  s.explain = {
    { "pop:AVA", "With AVA silent, AVB would still fire strongly after a head touch in this model. Here AVB is nearly silent, so the fault is not in AVA." },
    { "pop:AVB", "With the output of AVB cut, AVB itself would still fire strongly after a head touch in this model. Here it is nearly silent, so the fault is further back." },
    { "pop:AVD", "With AVD silent, AVB would still fire strongly after a head touch in this model. Here AVB is nearly silent, so the fault is not in AVD." },
    { "pop:PVC", "In this model AVB depends on PVC for its drive. With the output of PVC cut, AVB, AVA and AVD stay nearly silent after a head touch while PVC itself fires: the class that fails is the one that fires and passes nothing on." } };
  s.probes = { { "scope", "AVB", "separates the silenced class from the other three: AVB stays nearly silent only when its drive from the silenced class is gone" },
    { "cut", "pop:PVC", "silencing a class that is already silent changes nothing downstream" } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "pathway" };
  s.hint1 = "All four lesions give the same symptom, no backing up. They differ in which cells still fire after the touch. Record the classes between the touch cells and the backward motor neurons.";
  s.hint2 = "Open the scope of one of the interneuron classes and compare it with what it would do in a healthy worm.";
  s.hint2_target = "AVB";
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
