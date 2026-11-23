// "a_predict" (chapter a_worm, Atlas wing, level_kind "diagnosis"): a PREDICTION BET. Which one interneuron class can be silenced without losing the backward response?
//
// The circuit is the given touch circuit of the worm (circuits/a_predict_given.json = a_predict.json: wiring locked, five cell-type groups AVA, AVB, AVD, AVE, PVC at scale 1; nothing to edit).
// The player COMMITS to a class first. The commit travels like a diagnosis answer: set_variant( "AVE" ), "AVE@N" (N instrument reads), "AVE@N:W" (W wrong commits charged), "AVE@revealed".
// When the run starts the TASK silences the committed class itself (the weights of every connection out of that class are set to 0 before the first slice, as a lesion of the lesion lab),
// touches the head, and the verdict says what happened. The game edits nothing. Diagnosis::bet is true and Diagnosis::budget is the number of free instrument reads.
//
// WHAT PASSES: a commit, naming one of the four classes offered (AVA, AVD, AVE, PVC; see bets()), and the run shows that the worm still backs up (atlas_worm.h). The class for which the response survives is measured, not stored:
// tasks_atlas_worm_test holds the run to Diagnosis::answer (AVE) and asserts that exactly one of the four commits passes on the circuit seeds 1 to 3 and 101 to 105.
// NO COMMIT: not passed, and the verdict asks for the bet. SCORE: 100 minus 15 per wrong commit (the W of "cand@N:W") and 3 per instrument read beyond the budget (floor 25), as a case.
// The circuit must be the given one with every class at scale 1: the lesion is the level's, not the player's.
#include "atlas_worm.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_predict();
namespace
{
using namespace atlas_worm;

const char* kAnswer = "AVE";
/// The classes offered for the bet. AVB is not offered: with AVB silenced the backward response is weak and differs from seed to seed (the backward motor neurons fire
/// at 0 to 5.8 Hz over circuit seeds 1 to 60, against 0 to 0.3 Hz for AVA, AVD and PVC and 7.1 to 18.0 Hz for AVE; research/atlas-a.md 4b), so a bet on it has no clear outcome.
const std::vector< std::string >& bets()
{
  static const std::vector< std::string > c = { "AVA", "AVD", "AVE", "PVC" };
  return c;
}

class APredict : public WormRun
{
public:
  APredict() : WormRun( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i = world( "a_predict", "Atlas: bet on a silencing" );
    i.brief = "Five interneuron classes sit between the touch cells and the motor neurons of the worm. Silence any one of them and, in some cases, the worm no longer backs up when its head is touched. "
              "Which one class can be silenced without losing the backward response? Four are offered: AVA, AVD, AVE and PVC. Commit to one class first. Then the level silences it, touches the head, and tells you what happened. "
              "A committed prediction earns more than finding it by trial and error.";
    i.concepts_new = { "prediction" };
    i.concepts_used = { "pathway" };
    i.recipe = "Commit to one class on the bet panel. The run then silences that class itself and keeps the other four on. The wiring is given and cannot be edited, and there is nothing to set.";
    i.note = { "Hypothesis before experiment",
               "A prediction made before the experiment tells you more than a result looked up afterwards. In this model, with every synapse excitatory, silencing most of the interneuron "
               "classes abolishes the backward response and silencing one class leaves it. The animal is different from this model: its touch circuit was tested by killing cells with a "
               "laser and watching the behaviour, which found two pathways for touch to the head, through the AVD and AVB interneurons (Chalfie et al. 1985).",
               "Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) J Neurosci 5:956-964, doi:10.1523/JNEUROSCI.05-04-00956.1985" };
    return i;
  }

  std::string level_kind() const override { return "diagnosis"; }
  bool on_spine() const override { return false; }
  std::string parent() const override { return "a_path"; }

  Given given() const override
  {
    Given g;
    g.circuit = "a_predict_given.json";
    // printed by `spike-run --locked-digest circuits/a_predict_given.json`
    g.digest.pops = "738c6bdaeaf081583d69c8bfa5f74b51";
    g.digest.wiring = "7dc1ad137b57433fca1c823c254fc10a";
    g.digest.types = "7793bc66dc91308bfcea52245c731723";
    g.digest.channels = "bab362670876a5b510b31f294b1a2547";
    return g;
  }

  // the commit travels in the variant slot (Diagnosis::bet); registry.cpp lets a bet carry it
  void set_variant( const std::string& v ) override
  {
    Task::set_variant( v );
    commit_ = parse_commit( v );
    wrong_ = commit_.has && !commit_.revealed && commit_.cand != kAnswer ? wrong_ + 1 : wrong_;
  }
  std::map< std::string, std::string > variants() const override { return {}; }

  Diagnosis diagnosis() const override
  {
    Diagnosis d;
    d.circuit = "a_predict_given.json";
    d.symptoms = "The touch circuit of the worm, with its wiring given. Four of its interneuron classes are offered for silencing. AVB is left out: in this model its loss leaves a weak response that differs from run to run.";
    d.answer = kAnswer;
    d.candidates = ordered( "a_predict", bets(), kAnswer );
    for ( const auto& c : bets() )
    {
      d.labels[ c ] = "the interneuron class " + c;
      d.explain[ c ] = c == kAnswer ? "In this model the worm still backs up with " + c + " silenced."
                                    : "In this model the worm no longer backs up with " + c + " silenced: the backward response needs this class.";
    }
    d.probes = { { "scope", "DA", "the activity of the backward motor neurons shows whether the response survives" } };
    d.bet = true;
    d.budget = 3;
    return d;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "The wiring says which cells connect, not which of them the response depends on. Silencing a class removes everything that class carries, and the other classes may or may not make up for it.", "" },
      { 2, "Look at the classes between the touch cells and the backward motor neurons. Ask which of them the others could stand in for.", "DA" },
      { 3, "Open the activity of the backward motor neurons and of the classes in front of them, and compare. Then commit before the run: a prediction earns more than trial and error.", "DA" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "wrong_guesses", { static_cast< double >( wrong_ ), "guesses", true } }, { "probes", { static_cast< double >( commit_.probes ), "probes", true } },
      { "backward_hz", { rates().a_resp, "Hz", false } } };
  }

  Score score() const override
  {
    Score s;
    fill_parts( s );
    s.parts[ "committed" ] = commit_.has ? 1.0 : 0.0;
    s.parts[ "wrong_guesses" ] = wrong_;
    s.parts[ "probes" ] = commit_.probes;
    const auto& cands = bets();
    const bool known = commit_.has && std::find( cands.begin(), cands.end(), commit_.cand ) != cands.end();
    s.passed = false;
    s.value = 0;
    if ( !wiring_.ok() )
      s.verdict = "Not wired: " + wiring_.message() + ".";
    else if ( !commit_.has )
      s.verdict = "No prediction committed yet. Commit to one interneuron class first: the run then silences it and shows whether the worm still backs up.";
    else if ( commit_.revealed )
      s.verdict = std::string( "The answer was revealed: " ) + kAnswer + ". Nothing is scored for a revealed bet.";
    else if ( !known )
      s.verdict = "'" + commit_.cand + "' is not one of the classes offered.";
    else if ( !done() )
      s.verdict = "Running.";
    else if ( !mech_.empty() )
      s.verdict = mech_;
    else if ( backs_up() )
    {
      s.passed = true;
      s.value = bet_value( commit_.wrong, commit_.probes, 3 );
      s.verdict = "Correct: with " + commit_.cand + " silenced the worm still backs up. " + outcome() + " In this model the response does not need this class.";
    }
    else
      s.verdict = "Not " + commit_.cand + ". With it silenced the worm does not back up: " + outcome() + " In this model the response needs this class.";
    return s;
  }

protected:
  void inspect( Engine& e ) override
  {
    const Setting st = setting();
    if ( !st.missing.empty() ) { mech_ = "The interneuron class cards are missing: load the given circuit again."; return; }
    if ( !st.silenced.empty() || !st.partial.empty() ) { mech_ = "The given circuit has been edited. Leave every class at its given value: the level silences the class you committed to itself."; return; }
    const auto& cands = bets();
    if ( !commit_.has || commit_.revealed || std::find( cands.begin(), cands.end(), commit_.cand ) == cands.end() ) return;
    // the lesion: every connection out of the committed class is set to weight 0 (all edges of each of its projections, whatever their order)
    for ( const auto& pr : g_.st.projections )
      if ( pr.pre_pop == commit_.cand )
        for ( int32_t k = 0; k < pr.n_stored_edges; ++k )
          e.set_weight( pr.index, k, 0.0 );
  }

private:
  Commit commit_;
  int wrong_ = 0;
};
}  // namespace

std::unique_ptr< Task > make_a_predict() { return std::make_unique< APredict >(); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_predict, 993, "a_predict.json", make_a_predict ) }
