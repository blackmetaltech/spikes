// Diagnosis case "dx_a_compass" (chapter a_compass, Atlas wing): a model fly whose compass holds no bump. The patient circuit (circuits/dx_a_compass.json, made by
// circuits/tools/gen_atlas_compass.py) is the compass of the chapter with every cell and every connection, but the OUTPUTS of one cell type carry nothing. The player finds which by
// recording. The world is the ordinary cue-and-hold world of a_gain (atlas_compass.h): two cues with a reset between them.
//
// THE SIGNATURE (tasks_atlas_compass_test prints it, research/atlas-a.md 4d): see the table there. The case is fictional.
#include "dx_common.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_dx_a_compass();
namespace
{

DxSpec spec()
{
  DxSpec s;
  s.id = "dx_a_compass";
  s.title = "Diagnosis: the compass that holds no bump";
  s.inner = "a_gain";
  s.chapter = "a_compass";
  s.vignette = "(A model fly, not an animal.) Its compass circuit has all its cells and all its connections, as in the wiring data. When a cue is shown, the ring does not hold one bump of activity where "
               "the cue put it. In this model one of the cell types has been silenced: it still receives input, but its output carries nothing.";
  s.symptoms = "The cue reaches the compass cells. After it the ring does not settle into one bump at the cue. Which cell types still fire, and how strongly, is what you can record.";
  s.method = "Run the circuit and record. Open the scope of a cell type to see how strongly it fires after the cue. A silenced type still fires when its input is strong enough, and the types after it "
             "lose what it carried. You can also silence a type yourself in the lesion lab. Then name the silenced type. The case is scored on what it took: each wrong guess and each probe beyond "
             "the first few lowers the score.";
  s.candidates = { "pop:EPG", "pop:PEG", "pop:D7" };
  s.answer = "pop:D7";
  s.labels = { { "pop:EPG", "the compass cells (E-PG) carry nothing" }, { "pop:PEG", "the P-EG cells carry nothing" }, { "pop:D7", "the Delta7 cells carry nothing" } };
  s.explain = {
    { "pop:EPG", "With the compass cells silent the ring cannot hold a bump and the cells that listen to it fall quiet. Here the compass cells fire strongly and the ring is not quiet, so the fault is elsewhere." },
    { "pop:PEG", "With the P-EG cells silent the bump would still hold in this model. Here the activity is not held, so the fault is elsewhere." },
    { "pop:D7", "With the Delta7 cells silent nothing holds the activity in check: the compass cells fire strongly all round the ring while the Delta7 cells themselves fire and pass nothing on." } };
  s.probes = { { "scope", "EPG", "separates a ring that is quiet from a ring that is over-active: the compass cells fire strongly all round the ring only when the ring-wide inhibition is cut" },
    { "cut", "pop:D7", "silencing a type that is already silent changes nothing downstream" } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "inhibition_balance" };
  s.hint1 = "All three lesions give the same symptom, no single bump. They differ in how strongly the compass cells fire. Record the compass cells after a cue.";
  s.hint2 = "Open the scope of the compass cells and compare their rate with what a ring that holds one bump would show.";
  s.hint2_target = "EPG";
  s.hint3 = "The silenced type is among the compass cells, the P-EG cells and the Delta seven cells. Compare how strongly each of them fires after the cue.";
  s.hint3_target = "D7";
  s.note = { "Finding a lesion from the activity",
             "Without a ring-wide inhibition, a ring of cells that excite their neighbours spreads its activity all round instead of keeping one bump. This case works on a model: one cell type has been "
             "silenced, and the recordings tell which. The case is fictional.",
             "Turner-Evans DB et al. (2020) Neuron 108:145-163, doi:10.1016/j.neuron.2020.08.006" };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_dx_a_compass() { return std::make_unique< DxTask >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( dx_a_compass, 998, "dx_a_compass.json", make_dx_a_compass ) }
