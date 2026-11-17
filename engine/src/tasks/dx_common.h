// Shared plumbing for the diagnosis cases (level_kind "diagnosis"): a patient circuit with a hidden lesion runs inside the
// ORDINARY world of the task it was made from (baroreflex, pupil, ...); the player probes it with the scopes and the lesion lab,
// then names the lesion from a short multiple-choice list.
//
// ANSWER CONVENTION: the player's answer is submitted with  task->set_variant( candidate_id )  (the v0.3 contract slot that is
// free here: diagnosis cases have no challenge variants, variants() is empty). variant() then returns the submitted id, "" means
// "nothing submitted". score() can be read at any time (it does not wait for the episode): passed == ( answer is right ),
// value 100 or 0, verdict explains why the choice does or does not fit. Wrong submissions are counted (metric "wrong_guesses").
// The episode itself is only there for probing: its world result is reported as parts "run_value" / "run_passed".
//
// Candidate ids are lesion ids of the lesion lab: "pre>post" (every projection pre -> post), several joined with "+" for a
// bilateral lesion ("MN_F>RC_F+MN_E>RC_E"). The patient circuit file is the reference circuit of the underlying task with the
// answer's projections DELETED (circuits/tools/gen_boss_dx.py), so a lesion lab "restore the wire" test is possible.
#pragma once
#include <algorithm>
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
      << "\n\nThis circuit is a working one with ONE connection missing. Run it, watch the scopes, use the lesion lab to test your "
         "ideas (a cut you add that makes no difference was not the missing one), then name the lesion. Candidates:";
    for ( auto& c : s_.candidates )
      o << "\n - " << c << ": " << s_.labels.at( c );
    return o.str();
  }
  std::vector< std::string > sensor_channels() const override { return inner_->sensor_channels(); }
  std::vector< std::string > motor_channels() const override { return inner_->motor_channels(); }
  double duration_ms() const override { return inner_->duration_ms(); }
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
    variant_ = v;
    if ( !v.empty() && v != s_.answer )
      ++wrong_;
  }

  Diagnosis diagnosis() const override { return { s_.id + ".json", s_.symptoms, s_.candidates, s_.answer }; }
  Note notebook() const override { return s_.note; }

  std::vector< Hint > hints() const override
  {
    std::vector< Hint > h;
    h.push_back( { 1, s_.hint1, "" } );
    h.push_back( { 2, s_.hint2, s_.hint2_target } );
    std::string other;  // tier 3: a pair (the answer and the first other candidate): half the list is out
    for ( auto& c : s_.candidates )
      if ( c != s_.answer && other.empty() )
        other = c;
    h.push_back( { 3, "It is either " + s_.answer + " or " + other + ". Cut the one you suspect in the lesion lab: if the symptom does not change, it was not the missing wire.", s_.hint2_target } );
    return h;
  }

  std::map< std::string, Metric > metrics() const override
  {
    return { { "wrong_guesses", Metric{ static_cast< double >( wrong_ ), "guesses", true } } };
  }

  Score score() const override
  {
    Score s;
    const bool sub = !variant_.empty();
    const bool known = std::find( s_.candidates.begin(), s_.candidates.end(), variant_ ) != s_.candidates.end();
    const bool ok = sub && variant_ == s_.answer;
    Score in = inner_->score();
    s.parts[ "submitted" ] = sub ? ( known ? 1.0 : -1.0 ) : 0.0;
    s.parts[ "correct" ] = ok ? 1.0 : 0.0;
    s.parts[ "wrong_guesses" ] = wrong_;
    s.parts[ "run_value" ] = in.value;
    s.parts[ "run_passed" ] = in.passed ? 1.0 : 0.0;
    for ( auto& [ k, v ] : in.parts )
      s.parts[ "run_" + k ] = v;
    s.value = ok ? 100.0 : 0.0;
    s.passed = ok;
    if ( !sub )
      s.verdict = "No diagnosis submitted yet. Run the circuit, probe it, then name the missing connection. (The patient's own run: " + in.verdict + ")";
    else if ( !known )
      s.verdict = "'" + variant_ + "' is not one of the candidates.";
    else if ( ok )
      s.verdict = "Correct: " + variant_ + ". " + s_.explain.at( variant_ );
    else
      s.verdict = "Not " + variant_ + ": " + s_.explain.at( variant_ );
    return s;
  }

  const DxSpec& spec() const { return s_; }

private:
  DxSpec s_;
  std::unique_ptr< Task > inner_;
  int wrong_ = 0;
};

}  // namespace spike::tasks
