// Shared helpers for the body modules (muscle, joint, heart, lungs, eye, gut).
// Pure C++, no NEST. Everything is deterministic: fixed step (callers use 1 ms slices), no std::distribution.
//
// Common module API (each module follows it):
//   struct XParams   constants, with a default that gives the textbook resting state
//   struct XInput    what the controller / task writes each slice
//   class  X         reset(), step(dt_ms, input), outputs as public members / accessors,
//                    readouts() -> map of named values for HUD gauges (BodyState::readouts),
//                    view()     -> small render-friendly struct for the game
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace spike::body
{

constexpr double kPi = 3.14159265358979323846;

inline double clamp( double x, double lo, double hi ) { return x < lo ? lo : ( x > hi ? hi : x ); }
inline double sigmoid( double x ) { return 1.0 / ( 1.0 + std::exp( -x ) ); }

/// Named values for HUD gauges (same type as spike::BodyState::readouts).
using Readouts = std::map< std::string, double >;

/// splitmix64 (platform independent).
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
};

/// First-order low-pass, exact for piecewise-constant input; separate rise and fall time constants.
struct Lowpass
{
  double tau_up_ms = 20, tau_down_ms = 20, y = 0;
  Lowpass() = default;
  Lowpass( double tau_ms ) : tau_up_ms( tau_ms ), tau_down_ms( tau_ms ) {}
  Lowpass( double up, double down ) : tau_up_ms( up ), tau_down_ms( down ) {}
  void reset( double v = 0 ) { y = v; }
  double step( double x, double dt_ms )
  {
    const double tau = x > y ? tau_up_ms : tau_down_ms;
    y += ( 1.0 - std::exp( -dt_ms / tau ) ) * ( x - y );
    return y;
  }
};

/// Fixed pure delay (ring buffer). dt must be constant between calls; the delay is rounded to whole steps.
struct DelayLine
{
  double delay_ms = 0;
  void reset( double v = 0 ) { buf_.clear(); fill_ = v; }
  double step( double x, double dt_ms )
  {
    const size_t n = static_cast< size_t >( std::lround( delay_ms / dt_ms ) );
    if ( buf_.size() != n + 1 )
      buf_.assign( n + 1, fill_ ), pos_ = 0;
    const double out = buf_[ pos_ ];
    buf_[ pos_ ] = x;
    pos_ = ( pos_ + 1 ) % buf_.size();
    return out;
  }

private:
  std::vector< double > buf_;
  size_t pos_ = 0;
  double fill_ = 0;
};

} // namespace spike::body
