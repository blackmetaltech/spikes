// "a_predict" (chapter a_worm, Atlas wing, level_kind "atlas"): a PREDICTION BET. Which one interneuron class can be silenced without losing the backward response?
//
// The circuit is the given touch circuit of the worm (circuits/a_predict_given.json: wiring locked, five cell-type groups AVA, AVB, AVD, AVE, PVC with a free scale from 0 to 1).
// The player COMMITS to a class first; the lesion run then silences that class (the game sets its scale to 0; the level reads the scales of the circuit that was run) and the
// verdict says what the run showed. The commit travels like a diagnosis answer: set_variant( "AVE" ), "AVE@N" (N instrument reads), "AVE@N:W" (W wrong commits charged),
// "AVE@revealed" (given up). Diagnosis::bet is true and Diagnosis::budget is the number of free instrument reads.
//
// WHAT PASSES: one class silenced, no class half on, the commit (if any) names that class, and the run shows that the worm still backs up (atlas_worm.h). The class for which the
// response survives is measured, not stored: tasks_atlas_worm_test holds the run to Diagnosis::answer (AVE) and asserts that exactly one of the five commits passes on the
// circuit seeds 1 to 3 and 101 to 105.
// SCORE: with a matching commit 100 minus 15 per wrong commit and 3 per instrument read beyond the budget (floor 25). With no commit at all the run is a control run: it can pass
// (so that a circuit that works is not failed for having no bet) and scores at most 60, because trial and error earns less than a prediction.
#include "atlas_worm.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_predict();
namespace
{
using namespace atlas_worm;

const char* kAnswer = "AVE";

class APredict : public WormRun
{
public:
  APredict() : WormRun( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i = world( "a_predict", "Atlas: bet on a silencing" );
    i.brief = "Five interneuron classes sit between the touch cells and the motor neurons of the worm. Silence any one of them and, in some cases, the worm no longer backs up when its head is touched. "
              "Which one class can be silenced without losing the backward response? Commit to one class first. Then the run silences it, touches the head, and tells you what happened. "
              "A committed prediction earns more than finding it by trial and error.";
    i.concepts_new = { "prediction" };
    i.concepts_used = { "pathway" };
    i.recipe = "Commit to one class on the bet panel. The lesion run sets that class's scale to zero and keeps the other four at one. The wiring is given and cannot be edited.";
    i.note = { "Hypothesis before experiment",
               "A prediction made before the experiment tells you more than a result looked up afterwards. In this model, with every synapse excitatory, silencing most of the interneuron "
               "classes abolishes the backward response and silencing one class leaves it. The animal is different from this model: its touch circuit was tested by killing cells with a "
               "laser and watching the behaviour, which found two pathways for touch to the head, through the AVD and AVB interneurons (Chalfie et al. 1985).",
               "Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) J Neurosci 5:956-964, doi:10.1523/JNEUROSCI.05-04-00956.1985" };
    return i;
  }

  std::string level_kind() const override { return "atlas"; }

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
    d.symptoms = "The touch circuit of the worm, with its wiring given. Five interneuron classes can each be silenced.";
    d.answer = kAnswer;
    d.candidates = ordered( "a_predict", commands(), kAnswer );
    for ( const auto& c : commands() )
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
    const Setting st = setting();
    const std::string silenced = st.silenced.size() == 1 ? st.silenced.front() : "";
    s.parts[ "committed" ] = commit_.has ? 1.0 : 0.0;
    s.parts[ "wrong_guesses" ] = wrong_;
    s.parts[ "probes" ] = commit_.probes;
    const auto& cands = commands();
    const bool known = commit_.has && std::find( cands.begin(), cands.end(), commit_.cand ) != cands.end();
    std::string why;   // what is wrong with the setting or the commit
    if ( !st.missing.empty() ) why = "The interneuron class cards are missing: load the given circuit again.";
    else if ( !st.partial.empty() ) why = "The class " + st.partial.front() + " is only partly on. A lesion silences a class completely, at exactly zero.";
    else if ( st.silenced.empty() ) why = "No class is silenced: this run is the intact worm. Silence the class you bet on.";
    else if ( st.silenced.size() > 1 ) why = "More than one class is silenced. The bet is about one class at a time.";
    else if ( commit_.has && !commit_.revealed && !known ) why = "'" + commit_.cand + "' is not one of the five classes.";
    else if ( commit_.has && !commit_.revealed && commit_.cand != silenced ) why = "You committed to " + commit_.cand + " but silenced " + silenced + ". The lesion run must silence the class you committed to.";
    s.passed = false;
    s.value = 0;
    if ( !wiring_.ok() )
      s.verdict = "Not wired: " + wiring_.message() + ".";
    else if ( !done() )
      s.verdict = "Running.";
    else if ( commit_.revealed )
      s.verdict = std::string( "The answer was revealed: " ) + kAnswer + ". Nothing is scored for a revealed bet.";
    else if ( !why.empty() )
      s.verdict = why;
    else if ( !mech_.empty() )
      s.verdict = mech_;
    else if ( backs_up() )
    {
      s.passed = true;
      if ( commit_.has )
      {
        s.value = bet_value( commit_.wrong, commit_.probes, 3 );
        s.verdict = "Correct: with " + silenced + " silenced the worm still backs up. " + outcome() + " In this model the response does not need this class.";
      }
      else
      {
        s.value = 60;
        s.verdict = "With " + silenced + " silenced the worm still backs up. " + outcome() + " No prediction was committed before the run, so this counts as a control run: commit first to earn the full score.";
      }
    }
    else
      s.verdict = "Not " + silenced + ". With it silenced the worm does not back up: " + outcome() + " In this model the response needs this class.";
    return s;
  }

private:
  Commit commit_;
  int wrong_ = 0;
};
}  // namespace

std::unique_ptr< Task > make_a_predict() { return std::make_unique< APredict >(); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_predict, 993, "a_predict.json", make_a_predict ) }
