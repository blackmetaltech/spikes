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
};

std::vector< std::string > task_ids();
std::unique_ptr< Task > make_task( const std::string& id );
/// Path (relative to the repo's circuits/ dir) of the reference circuit that passes the task.
std::string reference_circuit( const std::string& id );

} // namespace spike
