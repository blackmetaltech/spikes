// Shared helpers for the task worlds: a tiny deterministic RNG, a first-order filter and a motor-rate reader.
// Nothing here touches NEST; the worlds are plain C++ and are the same on every platform.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
#include "spike/engine.h"
#include "spike/task.h"

namespace spike::tasks
{

constexpr double kPi = 3.14159265358979323846;

inline double clampd( double x, double lo, double hi ) { return x < lo ? lo : ( x > hi ? hi : x ); }

/// splitmix64: deterministic, platform independent (no std::distribution, whose output differs between standard libraries).
struct Rng
{
  uint64_t s = 0;
  explicit Rng( uint64_t seed = 0 ) : s( seed * 0x9E3779B97F4A7C15ULL + 0x1234567ULL ) {}
  uint64_t next()
  {
    uint64_t z = ( s += 0x9E3779B97F4A7C15ULL );
    z = ( z ^ ( z >> 30 ) ) * 0xBF58476D1CE4E5B9ULL;
    z = ( z ^ ( z >> 27 ) ) * 0x94D049BB133111EBULL;
    return z ^ ( z >> 31 );
  }
  double uniform() { return ( next() >> 11 ) * ( 1.0 / 9007199254740992.0 ); }  // [0,1)
  double uniform( double a, double b ) { return a + ( b - a ) * uniform(); }
  double normal()  // Box-Muller, both values used
  {
    if ( have_spare )
    {
      have_spare = false;
      return spare;
    }
    double u1 = std::max( uniform(), 1e-12 ), u2 = uniform();
    double r = std::sqrt( -2.0 * std::log( u1 ) );
    spare = r * std::sin( 2 * kPi * u2 );
    have_spare = true;
    return r * std::cos( 2 * kPi * u2 );
  }
  bool have_spare = false;
  double spare = 0;
};

/// First-order low-pass, exact for piecewise-constant input.
struct Lowpass
{
  double tau_ms = 20, y = 0;
  void reset( double v = 0 ) { y = v; }
  double step( double x, double dt_ms )
  {
    double a = 1.0 - std::exp( -dt_ms / tau_ms );
    y += a * ( x - y );
    return y;
  }
};

/// Reads motor channels from the engine. Engine::output_rate() is the spike count of the channel's neurons during the
/// last step divided by the step length, i.e. the summed rate of the population in Hz. Tasks use the mean rate per
/// neuron (summed / number of neurons) so that a motor pool of 3 or 30 cells has the same scale.
class MotorReader
{
public:
  void invalidate() { counts_.clear(); }
  /// Mean Hz per neuron during the last step.
  double hz_per_neuron( const Engine& e, const std::string& ch )
  {
    return sum_hz( e, ch ) / n_of( e, ch );
  }
  /// Number of neurons the channel reads (1 if unknown).
  double pool_size( const Engine& e, const std::string& ch ) { return n_of( e, ch ); }
  /// Spike count of the channel during a step of dt_ms.
  double spike_count( const Engine& e, const std::string& ch, double dt_ms ) { return sum_hz( e, ch ) * dt_ms / 1000.0; }

private:
  double sum_hz( const Engine& e, const std::string& ch ) const { return e.output_rate( ch ); }
  double n_of( const Engine& e, const std::string& ch )
  {
    auto it = counts_.find( ch );
    if ( it != counts_.end() )
      return it->second;
    double n = 1;
    for ( const auto& o : e.outputs() )
      if ( o.name == ch && !o.neurons.empty() )
        n = static_cast< double >( o.neurons.size() );
    counts_[ ch ] = n;
    return n;
  }
  std::map< std::string, double > counts_;
};

/// Checks, once per episode, which of a task's channels are connected to at least one neuron. Tasks use it to say plainly
/// "sensor X is not wired" / "motor Y has no neurons" instead of an unexplained zero score.
class Wiring
{
public:
  void invalidate() { probed_ = false; unwired_.clear(); }
  void probe( const Engine& e, const std::vector< std::string >& sensors, const std::vector< std::string >& motors )
  {
    if ( probed_ )
      return;
    probed_ = true;
    unwired_.clear();
    auto check = [ & ]( const std::vector< ChannelInfo >& have, const std::vector< std::string >& want, const char* kind ) {
      for ( const auto& w : want )
      {
        bool ok = false;
        for ( const auto& c : have )
          if ( c.name == w && !c.neurons.empty() )
            ok = true;
        if ( !ok )
          unwired_.push_back( std::string( kind ) + " '" + w + "'" );
      }
    };
    check( e.inputs(), sensors, "sensor" );
    check( e.outputs(), motors, "motor" );
  }
  bool ok() const { return unwired_.empty(); }
  /// "sensor 'touch' is not connected to any neuron" etc. (empty when everything is wired)
  std::string message() const
  {
    std::string m;
    for ( size_t i = 0; i < unwired_.size(); ++i )
    {
      if ( i ) m += "; ";
      m += unwired_[ i ];
    }
    if ( m.empty() ) return m;
    return m + ( unwired_.size() > 1 ? " are" : " is" ) + " not connected to any neuron";
  }

private:
  bool probed_ = false;
  std::vector< std::string > unwired_;
};

// ---- v0.3 helpers (add-only) -------------------------------------------------------------------------------------------
// Difficulty modes (docs/DESIGN-0.3.md section 5). A task reads mode_ / variant_ (set before reset()) through these.
//   guided      wide bands, low noise            standard  the pass bars as documented       researcher  tight bands, physiological noise
// Structure (the pass criteria's form, the circuit contract) never changes with the mode; only the numbers do.

/// Scale of a pass limit: guided widens it, researcher tightens it. `tol_max( m, x )` for a limit the value must stay below
/// (guided x * 1.25, researcher x * 0.85), `tol_min( m, x )` for a limit it must reach (guided x / 1.25, researcher x / 0.85).
inline double mode_factor( const std::string& mode, double guided = 1.25, double researcher = 0.85 )
{
  return mode == "guided" ? guided : ( mode == "researcher" ? researcher : 1.0 );
}
inline double tol_max( const std::string& mode, double x, double guided = 1.25, double researcher = 0.85 )
{
  return x * mode_factor( mode, guided, researcher );
}
inline double tol_min( const std::string& mode, double x, double guided = 1.25, double researcher = 0.85 )
{
  return x / mode_factor( mode, guided, researcher );
}
/// Multiplier on a task's noise source: guided 0.6, standard 1, researcher 1.25 (physiological spread); the "noise_x2" variant doubles it.
inline double mode_noise( const std::string& mode, const std::string& variant, double guided = 0.6, double researcher = 1.25 )
{
  return mode_factor( mode, guided, researcher ) * ( variant == "noise_x2" ? 2.0 : 1.0 );
}
/// "half_neurons" challenge variant: the par (neuron budget) is halved, rounded up.
inline int variant_par( const std::string& variant, int par ) { return variant == "half_neurons" ? ( par + 1 ) / 2 : par; }

inline Task::Hint make_hint( int tier, const std::string& text, const std::string& target = "" ) { return { tier, text, target }; }
/// Input channel kind with the lab's default first-connect weight (Task::ChannelKind::weight, v0.3).
inline Task::ChannelKind in_kind( const std::string& kind, double value, double weight )
{
  Task::ChannelKind k;
  k.kind = kind;
  k.value = value;
  k.weight = weight;
  return k;
}
inline Task::Metric make_metric( double value, const std::string& unit, bool lower_is_better ) { return { value, unit, lower_is_better }; }

} // namespace spike::tasks
