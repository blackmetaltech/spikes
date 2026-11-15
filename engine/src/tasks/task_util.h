// Shared helpers for the task worlds: a tiny deterministic RNG, a first-order filter and a motor-rate reader.
// Nothing here touches NEST; the worlds are plain C++ and are the same on every platform.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include "spike/engine.h"

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

} // namespace spike::tasks
