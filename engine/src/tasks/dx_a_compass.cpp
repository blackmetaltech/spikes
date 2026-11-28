// Diagnosis case "dx_a_compass" (chapter a_compass, Atlas wing): a model fly whose compass holds no bump. The patient circuit (circuits/dx_a_compass.json, made by
// circuits/tools/gen_atlas_compass.py) is the compass of the chapter with every cell and every connection, but the OUTPUTS of one cell type carry nothing. The player finds which by
// recording. The world is the ordinary cue-and-hold world of a_gain (atlas_compass.h): two cues with a reset between them.
//
// THE SIGNATURE (tasks_atlas_compass_test, research/atlas-a.md 4d; mean Hz per cell, 40 circuit seeds): of the five lesions only two stop the ring holding a bump at both cues, E-PG and Delta7. With the Delta7 outputs
// cut the compass cells fire at 234 to 256 Hz all round the ring and the Delta7 cells themselves at 340 to 349 Hz; with the E-PG outputs cut the P-EG and Delta7 cells and the first kind of P-EN cells are silent
// (0.0 Hz) and the compass cells fire at 32 to 73 Hz. Silencing the P-EG cells or either kind of P-EN cells leaves a ring that holds its bump (compass cells 20 to 22 Hz late). So one scope on the compass cells
// and one on the Delta7 cells tell the answer from every other candidate, on all 40 seeds. The case is fictional.
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
  s.method = "Run the circuit and record. Open the scope of a cell type to see how strongly it fires after the cue. A silenced type still fires when its own input is strong enough, and the types after it "
             "lose what it carried. You can also silence a type yourself in the lesion lab. Then name the silenced type. The case is scored on what it took: each wrong guess and each probe beyond "
             "the first few lowers the score.";
  s.candidates = { "pop:EPG", "pop:PENa", "pop:PENb", "pop:PEG", "pop:DELTA" };
  s.answer = "pop:DELTA";
  s.labels = { { "pop:EPG", "the compass cells (E-PG) carry nothing" }, { "pop:PENa", "the P-EN cells of the first kind (P-EN a) carry nothing" },
    { "pop:PENb", "the P-EN cells of the second kind (P-EN b) carry nothing" }, { "pop:PEG", "the P-EG cells carry nothing" }, { "pop:DELTA", "the Delta7 cells carry nothing" } };
  s.explain = {
    { "pop:EPG", "With the output of the compass cells cut, the P-EG cells, the Delta seven cells and one kind of P-EN cell lose their drive and fall silent. Here they fire strongly, so the compass cells are not the silenced type." },
    { "pop:PENa", "With only the output of the first kind of P-EN cells cut, the ring still holds its bump in this model and the compass cells fire at an ordinary rate. Here they fire far too strongly, so the fault is elsewhere." },
    { "pop:PENb", "With only the output of the second kind of P-EN cells cut, the ring still holds its bump in this model and the compass cells fire at an ordinary rate. Here they fire far too strongly, so the fault is elsewhere." },
    { "pop:PEG", "With only the output of the P-EG cells cut, the ring still holds its bump in this model and the compass cells fire at an ordinary rate. Here they fire far too strongly, so the fault is elsewhere." },
    { "pop:DELTA", "With the output of the Delta seven cells cut, nothing holds the activity in check: the compass cells fire strongly all round the ring, while the Delta seven cells themselves fire hard and pass nothing on." } };
  s.probes = { { "scope", "EPG", "separates a ring that is over-active from one that is quiet: the compass cells fire strongly all round the ring only when the inhibition across the ring is cut" },
    { "cut", "pop:DELTA", "silencing a type that is already silent changes nothing downstream" } };
  s.concepts_new = { "diagnosis" };
  s.concepts_used = { "inhibition_balance" };
  s.hint1 = "A ring that holds one bump needs excitation among its cells and inhibition across the ring. Record the compass cells after a cue and ask which of the two is missing.";
  s.hint2 = "Open the scope of the compass cells and compare their rate with what one compact bump would show.";
  s.hint2_target = "EPG";
  s.hint3 = "The silenced type is one of the five cell types of the circuit. Compare how strongly each of them fires after the cue: a silenced type still fires when its own input is strong, and the types behind it lose what it carried.";
  s.hint3_target = "PEG";
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
