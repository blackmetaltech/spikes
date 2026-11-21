// Shared plumbing for the diagnosis cases (level_kind "diagnosis"): a patient circuit with a hidden lesion runs inside the
// ORDINARY world of the task it was made from (baroreflex, pupil, ...); the player probes it with the scopes and the lesion lab,
// then names the lesion from a short multiple-choice list.
//
// ANSWER CONVENTION: the player's answer is submitted with  task->set_variant( candidate_id )  (the v0.3 contract slot that is
// free here: diagnosis cases have no challenge variants, variants() is empty). variant() then returns the submitted id, "" means
// "nothing submitted". The submission may carry the probes the player has used so far: "candidate_id@N" or "candidate_id@N:W" (N lesion re-runs and scopes
// opened; W wrong guesses made before this one; no suffix = none charged), and "candidate_id@revealed" is the player giving up (the answer is shown, nothing is scored).
// score() can be read at any time (it does not wait for the episode): passed == ( answer is right ).
// SCORING (0.5): value = 100 - 15 per wrong guess - 3 per probe beyond the first three, never below 25; the verdict says what was used. A wrong
// pick only says "not that one": the explanations (Diagnosis::explain) are for the screen that follows a solved or revealed case.
// Wrong submissions are counted (metric "wrong_guesses"), the probes too (metric "probes").
// The episode itself is only there for probing: its world result is reported as parts "run_value" / "run_passed".
//
// Candidate ids are lesion ids of the lesion lab: "pre>post" (every projection pre -> post), several joined with "+" for a
// bilateral lesion ("MN_F>RC_F+MN_E>RC_E"). The patient circuit file is the reference circuit of the underlying task with the
// answer's projections kept in the file with their weight set to 0 (circuits/tools/gen_boss_dx.py): every candidate is drawn, selectable and
// cuttable in the lesion lab, and the dead wire is told apart by activity (a source that fires into a target that is silent), not by a missing arrow.
#pragma once
#include <algorithm>
#include <cstdlib>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "spike/task.h"
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks
{

struct DxSpec
{
  std::string id, title, inner, chapter;
  std::string vignette;                          // who the patient is and what the examiner found (plain language)
  std::string symptoms;                          // clinical, plain-language findings
  std::vector< std::string > candidates;         // 4-6 lesion ids
  std::string answer;                            // one of candidates
  std::map< std::string, std::string > labels;   // candidate id -> plain-language name of the lesion (shown in the list)
  std::map< std::string, std::string > explain;  // candidate id -> one line: why it does or does not fit the findings
  std::vector< std::string > concepts_new, concepts_used;
  std::string hint1, hint2, hint2_target;        // tier 1: the concept; tier 2: the place (population id to highlight)
  std::string hint3, hint3_target;               // tier 3: a REGION of the circuit (empty: the connections of the group of tier 2); never a pair of candidates
  Task::Note note;
};

class DxTask : public Task
{
public:
  explicit DxTask( DxSpec s ) : s_( std::move( s ) ), inner_( make_task( s_.inner ) ) {}

  std::string id() const override { return s_.id; }
  std::string title() const override { return s_.title; }
  std::string brief() const override
  {
    std::ostringstream o;
    o << s_.vignette << "\n\nFindings: " << s_.symptoms
      << "\n\nThis circuit is a working one with ONE connection dead: every candidate is drawn, but the dead one carries nothing. Run it and "
         "watch which groups fire and which do not answer their input: the dead connection leads from an active group into one that fails to "
         "respond to it. To probe, cut a suspected connection in the lesion lab and see whether the symptoms or the activity downstream "
         "change: cutting a connection that is working changes something, while cutting the dead one changes nothing (and neither does "
         "cutting anything after a silent group). Then name the lesion. The case is scored on what it took: each wrong guess and each probe (a scope opened, a cut tried, a lesion re-run) beyond the first few lowers the score. Candidates:";
    for ( auto& c : s_.candidates )
      o << "\n - " << c << ": " << s_.labels.at( c );
    return o.str();
  }
  std::vector< std::string > sensor_channels() const override { return inner_->sensor_channels(); }
  std::vector< std::string > motor_channels() const override { return inner_->motor_channels(); }
  double duration_ms() const override { return inner_->duration_ms(); }
  // The diagnosis is decided by the submitted answer, never by the run, so a long run adds nothing to the CHECK: it is the inner task's quick episode
  // (forwarded, so that the inner task picks its quick schedule), cut at kDxQuickMs. The run only feeds the "run_*" parts and the patient's findings.
  static constexpr double kDxQuickMs = 8000;
  double quick_duration_ms() const override { return std::min( inner_->quick_duration_ms(), kDxQuickMs ); }
  void set_quick( bool q ) override
  {
    Task::set_quick( q );
    inner_->set_quick( q );
  }
  void reset( uint64_t seed ) override
  {
    inner_->set_mode( mode() );
    inner_->reset( seed );
  }
  void step( Engine& e, double dt_ms ) override { inner_->step( e, dt_ms ); }
  BodyState body() const override { return inner_->body(); }
  std::array< double, 4 > world_bounds() const override { return inner_->world_bounds(); }
  double field( double x, double y ) const override { return inner_->field( x, y ); }
  std::string chapter() const override { return s_.chapter; }
  std::string view() const override { return inner_->view(); }
  int par_neurons() const override { return 0; }
  std::vector< Event > timeline() const override { return inner_->timeline(); }
  std::vector< Band > bands() const override { return inner_->bands(); }
  std::vector< std::string > modulators() const override { return inner_->modulators(); }
  std::map< std::string, ChannelKind > channel_kinds() const override
  {
    auto k = inner_->channel_kinds();
    for ( auto& c : inner_->sensor_channels() ) k.emplace( c, ChannelKind{} );  // 0.1 defaults: inputs "poisson", outputs "rate"
    for ( auto& c : inner_->motor_channels() ) k.emplace( c, ChannelKind{ "rate" } );
    return k;
  }
  double next_event_ms( double t ) const override { return inner_->next_event_ms( t ); }

  std::string level_kind() const override { return "diagnosis"; }
  std::vector< std::string > concepts_new() const override { return s_.concepts_new; }
  std::vector< std::string > concepts_used() const override { return s_.concepts_used; }
  bool on_spine() const override { return false; }
  // no challenge variants: the variant slot carries the submitted answer (see the header comment)
  std::map< std::string, std::string > variants() const override { return {}; }
  void set_variant( const std::string& v ) override
  {
    std::string cand = v;
    probes_ = 0;
    charged_wrong_ = 0;
    revealed_ = false;
    if ( const auto at = v.rfind( '@' ); at != std::string::npos )
    {
      cand = v.substr( 0, at );
      const std::string tail = v.substr( at + 1 );
      if ( tail == "revealed" )
        revealed_ = true;
      else
      {
        probes_ = std::max( 0, std::atoi( tail.c_str() ) );
        if ( const auto colon = tail.find( ':' ); colon != std::string::npos )   // "@N:W": W wrong guesses were made before this one (the game keeps the count; only this is charged to the score)
          charged_wrong_ = std::max( 0, std::atoi( tail.c_str() + colon + 1 ) );
      }
    }
    variant_ = cand;
    if ( !cand.empty() && !revealed_ && cand != s_.answer )
      ++wrong_;
  }

  Diagnosis diagnosis() const override { return { s_.id + ".json", s_.symptoms, s_.candidates, s_.answer, s_.labels, s_.explain }; }
  // the patient is watched on the cell type of the underlying task's reference circuit; a diagnosis has no recipe
  std::string default_model() const override { return inner_->default_model(); }
  std::string recipe() const override { return ""; }
  Note notebook() const override { return s_.note; }

  std::vector< Hint > hints() const override
  {
    std::vector< Hint > h;
    h.push_back( { 1, s_.hint1, "" } );
    h.push_back( { 2, s_.hint2, s_.hint2_target } );
    // tier 3 narrows to a region of the circuit, never to a pair of candidates that contains the answer
    const std::string region = s_.hint3.empty() ? "The dead connection touches the group that the previous hint pointed at: it either leaves that group or arrives on it." : s_.hint3;
    h.push_back( { 3, region + " Trace the groups at both ends of each connection in that region: the dead one leads from an active group into one that does not answer. Then cut them one at a time in the lesion lab: a working one changes the activity or the symptoms, the dead one changes nothing (so does one after a silent group). Every scope and every cut costs a little of the score.", s_.hint3_target.empty() ? s_.hint2_target : s_.hint3_target } );
    return h;
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "wrong_guesses", Metric{ static_cast< double >( wrong_ ), "guesses", true } }, { "probes", Metric{ static_cast< double >( probes_ ), "probes", true } } };
  }

  // 100 - 15 per wrong guess - 3 per probe beyond the first three, never below 25
  static constexpr int kFreeProbes = 3;
  static constexpr double kWrongCost = 15, kProbeCost = 3, kFloor = 25;
  static double case_value( int wrong, int probes )
  {
    return std::max( kFloor, 100.0 - kWrongCost * wrong - kProbeCost * std::max( 0, probes - kFreeProbes ) );
  }

  Score score() const override
  {
    Score s;
    const bool sub = !variant_.empty();
    const bool known = std::find( s_.candidates.begin(), s_.candidates.end(), variant_ ) != s_.candidates.end();
    const bool ok = sub && !revealed_ && variant_ == s_.answer;
    Score in = inner_->score();
    s.parts[ "submitted" ] = sub ? ( known ? 1.0 : -1.0 ) : 0.0;
    s.parts[ "correct" ] = ok ? 1.0 : 0.0;
    s.parts[ "wrong_guesses" ] = wrong_;
    s.parts[ "probes" ] = probes_;
    s.parts[ "run_value" ] = in.value;
    s.parts[ "run_passed" ] = in.passed ? 1.0 : 0.0;
    for ( auto& [ k, v ] : in.parts )
      s.parts[ "run_" + k ] = v;
    s.value = ok ? case_value( charged_wrong_, probes_ ) : 0.0;
    s.passed = ok;
    const int extra = std::max( 0, probes_ - kFreeProbes );
    // a verdict carries no digits (the honesty contract): what was used is told in words; the lab shows the score and the counts as numbers
    auto word = []( int n ) -> std::string {
      static const char* w[] = { "no", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve" };
      return n >= 0 && n <= 12 ? w[ n ] : "many";
    };
    auto used = [ & ]() {
      std::ostringstream o;
      o << word( charged_wrong_ ) << ( charged_wrong_ == 1 ? " wrong guess" : " wrong guesses" ) << " and " << word( probes_ ) << ( probes_ == 1 ? " probe" : " probes" ) << " (lesion runs and scopes opened; the first " << word( kFreeProbes ) << " are free)";
      return o.str();
    };
    if ( !sub )
      s.verdict = "No diagnosis submitted yet. Run the circuit, probe it, then pick a candidate from the list.";
    else if ( revealed_ )
      s.verdict = "The answer was revealed after " + used() + ": " + s_.answer + ". Nothing is scored for a revealed case.";
    else if ( !known )
      s.verdict = "'" + variant_ + "' is not one of the candidates.";
    else if ( ok )
    {
      std::ostringstream o;
      o << "Correct: " << variant_ << ". Solved with " << used();
      if ( charged_wrong_ > 0 || extra > 0 ) o << "; each wrong guess and each probe beyond the free ones lowers the score of the case";
      o << ". " << s_.explain.at( variant_ );
      s.verdict = o.str();
    }
    else
      s.verdict = "Not " + variant_ + ". That does not tell you why: probe again.";   // the reasons are shown once the case is solved or revealed
    return s;
  }

  const DxSpec& spec() const { return s_; }

private:
  DxSpec s_;
  std::unique_ptr< Task > inner_;
  int wrong_ = 0, probes_ = 0, charged_wrong_ = 0;   // wrong_: every wrong submission this task has seen (the metric); charged_wrong_: the count the game sent with the answer (the score)
  bool revealed_ = false;
};

}  // namespace spike::tasks
