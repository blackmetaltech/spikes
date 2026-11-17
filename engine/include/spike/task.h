// Tasks: a body in a world. Sensors write engine inputs, motor outputs read engine output rates,
// the world advances in the same 1 ms slices as the network. Everything is deterministic given the seed.
#pragma once
#include <array>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "spike/engine.h"

namespace spike
{

/// What the game draws. Kept generic so the renderer doesn't need per-task code for the basics.
struct BodyState
{
  std::vector< std::array< double, 2 > > points;  // body polyline in world units (head first)
  double heading = 0;                             // radians
  std::map< std::string, double > readouts;       // named values for HUD gauges (sensor levels, speed, phase…)
};

struct Score
{
  double value = 0;          // 0..100, the number shown to the player
  bool passed = false;       // meets the task's pass bar
  std::map< std::string, double > parts;  // e.g. latency_ms, false_triggers, freq_hz, phase_deg, speed
  std::string verdict;       // one plain sentence
};

class Task
{
public:
  virtual ~Task() = default;
  virtual std::string id() const = 0;
  virtual std::string title() const = 0;
  /// Plain-language brief: the biology, what to build, how it's scored.
  virtual std::string brief() const = 0;
  /// Input channel names the task drives and output channel names it reads. A circuit must declare them.
  virtual std::vector< std::string > sensor_channels() const = 0;
  virtual std::vector< std::string > motor_channels() const = 0;
  /// Episode length in ms.
  virtual double duration_ms() const = 0;

  virtual void reset( uint64_t seed ) = 0;
  /// One slice: read the world into engine inputs, step the engine `dt_ms`, read motor rates, advance the world.
  virtual void step( Engine& engine, double dt_ms ) = 0;
  virtual BodyState body() const = 0;
  virtual Score score() const = 0;
  /// World extent for the renderer (min x, min y, max x, max y) and an optional scalar field to draw
  /// (e.g. the salt gradient), sampled on demand.
  virtual std::array< double, 4 > world_bounds() const = 0;
  virtual double field( double x, double y ) const { return 0; }

  // ---- v0.2 additions (add-only, with defaults so 0.1 tasks keep compiling) ----

  /// Campaign grouping: "invertebrates", "spinal", "heart", "breathing", "autonomic".
  virtual std::string chapter() const { return "invertebrates"; }
  /// Renderer hint: which body view the game draws ("polyline" = 0.1 generic, "limb", "heart", "lungs", "eye", "gut", "dashboard").
  virtual std::string view() const { return "polyline"; }
  /// Neuron budget for the "under par" star (the reference circuit's size is a good default).
  virtual int par_neurons() const { return 0; }
  /// Scripted perturbations shown on the timeline under the world (a push, standing up, exercise, light flash, CO2 rise…).
  struct Event { double t_ms; double dur_ms; std::string label; };
  virtual std::vector< Event > timeline() const { return {}; }
  /// Traces the score is judged on, for the timeline plot: name -> (target band lo, hi), and the current value via body().readouts[name].
  struct Band { std::string name; std::string unit; double lo; double hi; };
  virtual std::vector< Band > bands() const { return {}; }
  /// Neuromodulator channels the task reads from the circuit ("NE", "ACh", "DA") or writes (task-driven modulators).
  virtual std::vector< std::string > modulators() const { return {}; }
  /// How the circuit should declare each channel (keyed by sensor_channels() / motor_channels() names), so the lab can build a correct
  /// skeleton. Channels missing from the map get the 0.1 defaults (inputs "poisson", outputs "rate").
  ///   inputs : kind "dc" (value = pA, scaled by the weight) | "poisson" (value = Hz); `value` is a typical level, informational.
  ///   outputs: kind "rate" | "modulator" (`modulator` = "NE" | "ACh" | "DA", `gain` per spike, `tau_ms` decay).
  struct ChannelKind
  {
    std::string kind = "poisson";
    double value = 0;            // inputs: default level (pA or Hz)
    std::string modulator;       // outputs of kind "modulator": "NE" | "ACh" | "DA"
    double gain = 0.05;          // outputs of kind "modulator": concentration added per spike
    double tau_ms = 1000.0;      // outputs of kind "modulator": decay time constant
  };
  virtual std::map< std::string, ChannelKind > channel_kinds() const { return {}; }

  // ---- v0.3 additions (add-only, defaults keep 0.2 tasks compiling). See docs/DESIGN-0.3.md ----

  /// Level kind on the campaign map: "micro" (fish-tank puzzle), "task" (0.1/0.2 build), "boss" (chapter finale),
  /// "diagnosis" (find the hidden lesion in a given circuit).
  virtual std::string level_kind() const { return "task"; }
  /// Concept ids this level introduces (usually one) and reuses (docs/DESIGN-0.3.md section 3, e.g. "threshold", "inhibition").
  virtual std::vector< std::string > concepts_new() const { return {}; }
  virtual std::vector< std::string > concepts_used() const { return {}; }
  /// Spine levels gate chapter progress; branches (variants, diagnosis, optional) never do.
  virtual bool on_spine() const { return true; }

  /// Difficulty mode, set before reset(): "guided" | "standard" | "researcher". Tasks scale tolerance (bands, seeds, noise)
  /// and perturbation strength from it; structure never changes. Default: standard.
  virtual void set_mode( const std::string& mode ) { mode_ = mode; }
  std::string mode() const { return mode_; }
  /// Challenge variants: id -> one-line description ("half_neurons", "vagus_cut", "noise_x2", "izhikevich_only").
  /// set_variant("") restores the base task. Variants are optional branches, never on the spine.
  virtual std::map< std::string, std::string > variants() const { return {}; }
  virtual void set_variant( const std::string& v ) { variant_ = v; }
  std::string variant() const { return variant_; }

  /// Hint ladder for the current score state: tier 1 names the concept, tier 2 names the place (population/channel id
  /// to highlight, in `target`), tier 3 a partial recipe. Called after a failed run; may depend on score().parts.
  struct Hint { int tier; std::string text; std::string target; };
  virtual std::vector< Hint > hints() const { return {}; }

  /// Mastery metrics for post-solve histograms (Zachtronics style): name -> {value, unit, lower_is_better}.
  /// "neurons" and "energy_hz" are filled by the game; tasks add one quality metric (latency, error, synchrony…).
  struct Metric { double value; std::string unit; bool lower_is_better; };
  virtual std::map< std::string, Metric > metrics() const { return {}; }

  /// Pacing: model time (ms) of the next scripted event after `t_ms`, or < 0 if none (for "skip to next event").
  virtual double next_event_ms( double t_ms ) const
  {
    double best = -1;
    for ( auto& e : timeline() )
      if ( e.t_ms > t_ms + 1 && ( best < 0 || e.t_ms < best ) ) best = e.t_ms;
    return best;
  }

  /// Diagnosis levels: the circuit the player is given (path relative to circuits/), the patient's symptoms, the multiple-choice
  /// candidate lesions (ids like "pre>post" or "pop:ID") and which one is correct.
  struct Diagnosis { std::string circuit; std::string symptoms; std::vector< std::string > candidates; std::string answer; };
  virtual Diagnosis diagnosis() const { return {}; }

  /// Lab-notebook entry unlocked on first pass: a short true fact with its citation.
  struct Note { std::string title; std::string text; std::string citation; };
  virtual Note notebook() const { return {}; }

protected:
  std::string mode_ = "standard";
  std::string variant_;
};

std::vector< std::string > task_ids();
std::unique_ptr< Task > make_task( const std::string& id );
/// Path (relative to the repo's circuits/ dir) of the reference circuit that passes the task.
std::string reference_circuit( const std::string& id );

} // namespace spike
