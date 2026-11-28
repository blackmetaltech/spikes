// "a_places" (chapter a_compass, Atlas wing, level_kind "diagnosis"): a PREDICTION BET. How many directions can this wiring hold?
//
// The circuit is the compass with the signs and gains that hold a bump (circuits/a_places_given.json = a_places.json: wiring locked, nothing to edit). The level runs the sixteen cues
// one after the other, a reset between them (atlas_compass.h: 16 trials of 3700 ms), and counts the headings at which the bump came to rest (a resting place: the heading nearest to the angle of the
// bump of an active trial). The player COMMITS to a count first; the commit travels like a diagnosis answer (set_variant: "about6", "about6@N", "about6@N:W", "about6@revealed", see atlas_worm.h Commit).
// CANDIDATES (what the count must be): all16 = 13 or more places, about6 = 4 to 9, two = 1 to 3, none = no active trial. A count of 10 to 12 matches none of them (the test asserts it does not occur).
// WHAT PASSES: a commit that names a candidate, and the run's count falls in its range. Measured, not stored: tasks_atlas_compass_test holds the run to Diagnosis::answer (about6) on 40 circuit seeds.
// NO COMMIT: not passed, and the verdict asks for the bet; the run itself can be watched before the bet (instrument reads are free up to the budget). SCORE: 100 minus 15 per wrong commit and 3 per
// instrument read beyond the budget (floor 25), as a case.
#include "atlas_compass.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_places();
namespace
{
using namespace atlas_compass;
using atlas_worm::Commit;

const char* kAnswer = "about6";
const std::vector< std::string >& bets()
{
  static const std::vector< std::string > c = { "all16", "about6", "two", "none" };
  return c;
}
/// The candidate a count of resting places matches ("" for a count that matches none).
std::string match_of( int places )
{
  if ( places >= 13 ) return "all16";
  if ( places >= 4 && places <= 9 ) return "about6";
  if ( places >= 1 && places <= 3 ) return "two";
  if ( places == 0 ) return "none";
  return "";
}

class APlaces : public CompassRun
{
public:
  APlaces() : CompassRun( info(), places_cues() ) { reset( 1 ); }

  static Info info()
  {
    Info i = world( "a_places", "How many directions can it hold?", places_cues() );
    i.brief = "A compass has a direction for every heading, sixteen of them round the ring. Does this wiring? The level puts a cue at each of the sixteen headings in turn, with a reset between them, and "
              "watches where the bump of activity comes to rest. The wiring and its signs are given. Bet on how many different headings the bump rests at: all sixteen, about six, two, or none. "
              "Commit to one first. You can watch the cues run and read the ring before you bet; each instrument you open beyond the free ones lowers the score. Then the level says what the run counted.";
    i.concepts_new = { "resting_places" };
    i.concepts_used = { "ring_attractor", "prediction" };
    i.recipe = "Commit to one count on the bet panel. The run puts the sixteen cues in turn and counts the headings where the bump rested at the end of each trial. The wiring is given and cannot be edited.";
    i.note = { "Rings that rest at places",
               "A ring attractor in theory holds a bump at any heading. This wiring, with point cells and fitted gains, holds a bump at about six headings and pulls a cue between them to the nearest one. "
               "Whether the fly's compass has such places, or a smooth ring that this model only approximates, this level does not settle: the model is not the fly.",
               "Turner-Evans DB et al. (2020) Neuron 108:145-163, doi:10.1016/j.neuron.2020.08.006; Hulse BK et al. (2021) eLife 10:e66039, doi:10.7554/eLife.66039" };
    return i;
  }

  std::string level_kind() const override { return "diagnosis"; }
  bool on_spine() const override { return false; }
  std::string parent() const override { return "a_signs"; }

  Given given() const override
  {
    Given g;
    g.circuit = "a_places_given.json";
    // printed by `spike-run --locked-digest circuits/a_places_given.json`
    g.digest.pops = "fe6963039bfc82301f8be725d63ea5dc";
    g.digest.wiring = "c6ab5e9f78e440253471a4d981a6024c";
    g.digest.types = "4f53cda18c2baa0c0354bb5f9a3ecbe5";
    g.digest.channels = "ad2768fd684cc647b82ae1abd7d16b11";
    return g;
  }

  // the commit travels in the variant slot (Diagnosis::bet); registry.cpp lets a bet carry it
  void set_variant( const std::string& v ) override
  {
    Task::set_variant( v );
    commit_ = atlas_worm::parse_commit( v );
    wrong_ = commit_.has && !commit_.revealed && commit_.cand != kAnswer ? wrong_ + 1 : wrong_;
  }
  std::map< std::string, std::string > variants() const override { return {}; }

  Diagnosis diagnosis() const override
  {
    Diagnosis d;
    d.circuit = "a_places_given.json";
    d.symptoms = "The compass circuit of the fly, with its wiring and signs given. Sixteen cues, one at each heading, are run in turn with a reset between them.";
    d.answer = kAnswer;
    d.candidates = atlas_worm::ordered( "a_places", bets(), kAnswer );
    d.labels = { { "all16", "all sixteen headings" }, { "about6", "about six headings" }, { "two", "two headings" }, { "none", "none: no bump rests anywhere" } };
    d.explain = { { "all16", "In this model the bump comes to rest at far fewer than sixteen headings: cues between two resting places are pulled to one of them." },
      { "about6", "In this model the bump rests at about six of the sixteen headings, and a cue between two of them is pulled to the nearest." },
      { "two", "In this model the bump rests at more than two headings: the cues end at about six different places." },
      { "none", "In this model the bump does not die out: it rests at several headings." } };
    d.probes = { { "scope", "EPG", "the compass cells show where the bump rests after each cue" } };
    d.bet = true;
    d.budget = 3;
    return d;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "A cue puts the bump at a heading, but what the circuit then holds is decided by its own wiring: the bump may stay where it was put, or slide to a place the wiring prefers.", "" },
      { 2, "Watch the compass cells over several cues. Ask whether cues that differ a little end at different places.", "EPG" },
      { 3, "Open the activity of the compass cells and follow the bump over the cues. Count the different headings where it stays. Then commit before the run: a prediction earns more than trial and error.", "EPG" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "wrong_guesses", { static_cast< double >( wrong_ ), "guesses", true } }, { "probes", { static_cast< double >( commit_.probes ), "probes", true } },
      { "resting_places", { static_cast< double >( places() ), "headings", false } } };
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
    const int n = places();
    const std::string matched = match_of( n );
    s.parts[ "match" ] = matched.empty() ? -1.0 : static_cast< double >( std::find( cands.begin(), cands.end(), matched ) - cands.begin() );
    s.passed = false;
    s.value = 0;
    const std::string counted = "After the sixteen cues the bump had come to rest at " + number_word( n ) + ( n == 1 ? " heading." : " headings." );
    if ( !wiring_.ok() )
      s.verdict = "Not wired: " + wiring_.message() + ".";
    else if ( !commit_.has )
      s.verdict = "No prediction committed yet. Commit to one count first: the level then says how many headings the bump rested at.";
    else if ( commit_.revealed )
      s.verdict = std::string( "The answer was revealed: about six headings. Nothing is scored for a revealed bet." );
    else if ( !known )
      s.verdict = "'" + commit_.cand + "' is not one of the counts offered.";
    else if ( !done() )
      s.verdict = "Running.";
    else if ( !mech_.empty() )
      s.verdict = mech_;
    else if ( matched == commit_.cand )
    {
      s.passed = true;
      s.value = atlas_worm::bet_value( commit_.wrong, commit_.probes, 3 );
      s.verdict = "Correct. " + counted + " In this model this wiring holds about six directions, not all sixteen.";
    }
    else
      s.verdict = "Not that count. " + counted + " In this model this wiring holds about six directions, not all sixteen.";
    return s;
  }

private:
  Commit commit_;
  int wrong_ = 0;
};
}  // namespace

std::unique_ptr< Task > make_a_places() { return std::make_unique< APlaces >(); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_places, 996, "a_places.json", make_a_places ) }
