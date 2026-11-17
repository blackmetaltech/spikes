// Scaffolding shared by the micro-puzzles of concepts 1-10 (m_threshold ... m_neuromodulation; docs/DESIGN-0.3.md section 3).
// A micro-puzzle is a fish-tank task: 1-4 neurons, a 2-5 s episode, one concept, a dashboard with a few clear readouts.
// Nothing here touches NEST beyond Engine; the worlds are plain C++ and deterministic given the seed.
//
// DIFFICULTY (set_mode, docs/DESIGN-0.3.md section 5): a mode scales only the tolerance of the acceptance bands (tol()) and the noise
// of the stimulus (noise()); the structure of the puzzle never changes. Guided: wide bands, no noise. Standard: the bands as written
// in each task. Researcher: tight bands and physiological noise (the game asks for 5 seeds there).
#pragma once
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "task_util.h"
#include "tasks.h"

namespace spike::tasks::micro
{

/// ChannelKind builders (aggregate initialisation of ChannelKind trips -Wmissing-field-initializers).
inline Task::ChannelKind dc( double level, double weight = 300.0 )
{
  Task::ChannelKind k;
  k.kind = "dc";
  k.value = level;
  k.weight = weight;  // pA per unit: the first-connect weight in the lab; 300 is the lab's own dc start, kept where the weight is the puzzle itself
  return k;
}
inline Task::ChannelKind poisson( double hz )
{
  Task::ChannelKind k;
  k.kind = "poisson";
  k.value = hz;
  return k;
}
inline Task::ChannelKind rate() { return Task::ChannelKind{ "rate", 0, "", 0.05, 1000.0, 0.0 }; }
inline Task::ChannelKind modulator( const std::string& which, double gain, double tau_ms )
{
  return Task::ChannelKind{ "modulator", 0, which, gain, tau_ms, 0.0 };
}

/// Counts spikes of one motor channel inside a time window [t0, t1) of model time.
struct Window
{
  double t0 = 0, t1 = 0, n = 0;
  bool has( double t ) const { return t > t0 && t <= t1; }  // a step ending at t covers (t - dt, t]
  double len_s() const { return ( t1 - t0 ) / 1000.0; }
  double rate_hz( double pool = 1 ) const { return len_s() > 0 ? n / len_s() / std::max( pool, 1.0 ) : 0.0; }
};

class MicroBase : public Task
{
public:
  struct Info
  {
    std::string id, title, brief, chapter, view = "dashboard";
    std::vector< std::string > sensors, motors, concepts_new, concepts_used;
    double duration_ms = 4000;
    int par_neurons = 1;
    std::map< std::string, ChannelKind > kinds;
    std::vector< Band > bands;
    std::vector< Event > timeline;
    std::vector< std::string > modulators;
    Note note;
    std::string default_model = "iaf_psc_alpha";  // catalog card the lab opens the level on (the cell the reference and verdicts assume)
    std::string recipe;                            // concrete numbers, kept out of the brief (shown by mode in the lab)
  };

  explicit MicroBase( Info i ) : info_( std::move( i ) ) {}

  std::string id() const override { return info_.id; }
  std::string title() const override { return info_.title; }
  std::string brief() const override { return info_.brief; }
  std::vector< std::string > sensor_channels() const override { return info_.sensors; }
  std::vector< std::string > motor_channels() const override { return info_.motors; }
  double duration_ms() const override { return info_.duration_ms; }
  std::string chapter() const override { return info_.chapter; }
  std::string view() const override { return info_.view; }
  int par_neurons() const override { return info_.par_neurons; }
  std::vector< Event > timeline() const override { return info_.timeline; }
  std::vector< Band > bands() const override { return info_.bands; }
  std::vector< std::string > modulators() const override { return info_.modulators; }
  std::map< std::string, ChannelKind > channel_kinds() const override { return info_.kinds; }
  std::string level_kind() const override { return "micro"; }
  std::vector< std::string > concepts_new() const override { return info_.concepts_new; }
  std::vector< std::string > concepts_used() const override { return info_.concepts_used; }
  Note notebook() const override { return info_.note; }
  std::string default_model() const override { return info_.default_model; }
  std::string recipe() const override { return info_.recipe; }
  std::array< double, 4 > world_bounds() const override { return { 0, 0, 1, 1 }; }
  BodyState body() const override
  {
    BodyState b;
    b.points = { { 0, 0 }, { 1, 0 } };
    b.readouts = readouts_;
    b.readouts[ "t_ms" ] = t_;
    return b;
  }

  void step( Engine& e, double dt ) override
  {
    wiring_.probe( e, info_.sensors, info_.motors );
    drive( e, dt );
    e.step( dt );
    t_ += dt;
    observe( e, dt );
  }

protected:
  /// Mode scaling: > 1 means wider bands (guided), < 1 tighter (researcher).
  double tol() const { return mode_ == "guided" ? 1.5 : ( mode_ == "researcher" ? 0.7 : 1.0 ); }
  /// Stimulus noise multiplier: 0 guided, 1 standard, 2 researcher.
  double noise() const { return mode_ == "guided" ? 0.0 : ( mode_ == "researcher" ? 2.0 : 1.0 ); }

  void begin( uint64_t seed )
  {
    rng_ = Rng( seed ^ hash_id() );
    t_ = 0;
    wiring_.invalidate();
    motor_.invalidate();
    readouts_.clear();
  }

  /// Set inputs for the slice about to run (t_ is its start).
  virtual void drive( Engine& e, double dt ) = 0;
  /// Read the motor channels after the slice (t_ is now its end).
  virtual void observe( Engine& e, double dt ) = 0;

  /// "sensor 'x' is not connected to any neuron" etc., or "".
  std::string unwired() const { return wiring_.ok() ? std::string() : wiring_.message(); }
  bool done() const { return t_ >= info_.duration_ms - 1e-6; }

  /// Fills value / passed from the number of acceptance criteria met (a failed puzzle shows partial credit, a passed one 100).
  static void finish( Score& s, int met, int total )
  {
    s.passed = met == total;
    s.value = s.passed ? 100.0 : 100.0 * 0.85 * met / std::max( total, 1 );
  }

  static std::string fmt( double x, int prec = 0 )
  {
    std::ostringstream o;
    o << std::fixed << std::setprecision( prec ) << x;
    return o.str();
  }

  Info info_;
  Wiring wiring_;
  MotorReader motor_;
  Rng rng_;
  double t_ = 0;
  std::map< std::string, double > readouts_;

private:
  uint64_t hash_id() const
  {
    uint64_t h = 1469598103934665603ULL;
    for ( char c : info_.id ) h = ( h ^ static_cast< unsigned char >( c ) ) * 1099511628211ULL;
    return h;
  }
};

} // namespace spike::tasks::micro
