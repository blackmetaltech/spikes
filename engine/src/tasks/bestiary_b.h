// Scaffolding for the Bestiary chapters B of 0.5 (fly compass, fly mushroom body, crab pyloric rhythm; docs/DESIGN-0.5.md, research/bestiary-a.md sections 4-6).
// Same machinery as chapters A (bestiary_a.h: a scripted world, a mechanism check on Engine::structure(), a judge); this header adds the few things the new chapters need:
// ring readouts (population_vector outputs, see Spec::vec and Meas::vec_deg in bestiary_a.h), the angular-velocity world of the compass, and mechanism helpers.
#pragma once
#include <cmath>
#include "bestiary_a.h"

namespace spike::tasks::bestiary
{

/// Signed difference a - b in degrees, in (-180, 180].
inline double wrap180( double d )
{
  while ( d > 180.0 ) d -= 360.0;
  while ( d <= -180.0 ) d += 360.0;
  return d;
}

/// Output channel kind of a ring readout (population vector over the cells in ring order).
inline Task::ChannelKind ring_kind()
{
  Task::ChannelKind k;
  k.kind = "population_vector";
  return k;
}

// ---------------------------------------------------------------- the compass world: the fly turns, the circuit must follow in the dark
/// A turn of the fly: from t0 to t1 ms at `dps` degrees per second (positive = to the right, the direction of increasing ring index).
struct Turn
{
  double t0, t1, dps;
};
/// The fly's heading change since t = 0 (degrees), from its turns.
inline double heading_change( const std::vector< Turn >& turns, double t )
{
  double d = 0;
  for ( const auto& u : turns )
    if ( t > u.t0 ) d += u.dps * ( std::min( t, u.t1 ) - u.t0 ) / 1000.0;
  return d;
}
/// The two shifter inputs: every ms of the episode both sides get the tonic level `base` (Hz), and during a turn the side of the turn gets `step` Hz more and the other
/// side `step` Hz less (the sensor level that encodes the angular velocity: `hz_per_dps` Hz per degree per second, the same for both directions).
constexpr double kShiftBase = 40.0;
constexpr double kShiftHzPerDps = 0.25;
inline void shifter_segments( const std::vector< Turn >& turns, double dur, std::vector< Seg >& segs )
{
  double t = 0;
  auto flat = [ & ]( double a, double b ) {
    if ( b > a )
    {
      segs.push_back( { "turn_left", a, b, kShiftBase } );
      segs.push_back( { "turn_right", a, b, kShiftBase } );
    }
  };
  for ( const auto& u : turns )
  {
    flat( t, u.t0 );
    const double d = std::fabs( u.dps ) * kShiftHzPerDps;
    segs.push_back( { u.dps > 0 ? "turn_right" : "turn_left", u.t0, u.t1, kShiftBase + d } );
    segs.push_back( { u.dps > 0 ? "turn_left" : "turn_right", u.t0, u.t1, std::max( 0.0, kShiftBase - d ) } );
    t = u.t1;
  }
  flat( t, dur );
}

/// A ring task with a landmark: the landmark appears at one of kWedges bearings, chosen by the episode's seed (so a circuit that only works for one bearing fails); the sensor family
/// cue0 .. cue15 is one input per wedge, the landmark drives the one at its bearing. `landmark_deg` is the bearing (22.5 degrees per wedge); the segments named "cue" in the schedule are
/// re-addressed to that sensor. The heading of the fly starts at the landmark's bearing.
constexpr int kWedges = 16;
class Landmark : public Scripted
{
public:
  using Scripted::Scripted;
  void reset( uint64_t seed ) override
  {
    Scripted::reset( seed );
    Rng pick( seed * 0x9E3779B97F4A7C15ULL + 77 );
    pick.next();
    wedge_ = static_cast< int >( pick.next() % kWedges );
    for ( auto& g : cur_.segs )
      if ( g.ch == "cue" ) g.ch = "cue" + std::to_string( wedge_ );
    meas_.extra[ "landmark_deg" ] = 360.0 * wedge_ / kWedges;
  }

protected:
  int wedge_ = 0;
};
inline std::vector< std::string > cue_family()
{
  std::vector< std::string > v;
  for ( int k = 0; k < kWedges; ++k ) v.push_back( "cue" + std::to_string( k ) );
  return v;
}
/// "" when every landmark sensor reaches the ring.
inline std::string landmarks_missing( const Graph& g, const std::string& heading )
{
  const auto h = g.out_pops( heading );
  if ( h.empty() ) return "";
  for ( int k = 0; k < kWedges; ++k )
    if ( !Graph::overlap( g.in_pops( "cue" + std::to_string( k ) ), h ) )
      return "A landmark sensor is not wired to the ring: the landmark can appear at any bearing round the ring, and the cue sensors are one for each ring cell. Wire the ring to the whole cue family (cell k to sensor k).";
  if ( g.cells( h ) != kWedges ) return "The cue sensors are one for each cell of the ring: build the ring with exactly as many cells as there are cue sensors (sixteen).";
  return "";
}

// ---------------------------------------------------------------- mechanism helpers
/// "" when `heading` is a ring readout with enough cells, else the sentence.
inline std::string ring_readout_missing( const Graph& g, const std::string& ch )
{
  const auto h = g.out_pops( ch );
  if ( h.empty() ) return "";
  if ( chan_kind( g, g.st.outputs, ch ) != "population_vector" )
    return "'" + ch + "' is read as a plain rate. A compass needs a ring readout: set the channel to read the bump angle (population vector) over the ring cells, in ring order.";
  if ( g.cells( h ) < 4 ) return "The ring has too few cells to hold a bump anywhere but on top of itself: give the ring several cells (8 or more) in order around the circle.";
  return "";
}
/// "" when the ring cells excite each other in a loop and inhibit each other broadly, else the sentence.
inline std::string ring_dynamics_missing( const Graph& g, const std::string& ch, bool need_inhibition = true )
{
  const auto h = g.out_pops( ch );
  if ( h.empty() ) return "";
  if ( !g.path( h, h, +1, 3 ) )
    return "The ring cells do not excite each other: a bump is held by recurrent excitation, each cell driving its neighbours on the ring. Without it the bump dies when the landmark goes.";
  if ( need_inhibition && !g.path( h, h, -1, 3 ) )
    return "Nothing inhibits the ring broadly: with local excitation only, the activity spreads until the whole ring fires. Add a cell that the ring excites and that inhibits the ring cells (global inhibition), so that one bump wins.";
  return "";
}
/// "" when the turn inputs reach two separate shifter groups, each driven by the heading and returning to it.
inline std::string shifters_missing( const Graph& g, const std::string& heading, const std::string& left, const std::string& right )
{
  const auto h = g.out_pops( heading ), l = g.in_pops( left ), r = g.in_pops( right );
  if ( h.empty() ) return "";
  if ( l.empty() || r.empty() ) return "A turn sensor is not connected: the bump can only be moved if both 'turn_left' and 'turn_right' reach the shifting cells.";
  if ( Graph::overlap( l, h ) || Graph::overlap( r, h ) )
    return "The turn signal goes straight into the ring cells. Turning must not write on the bump directly: it has to act through shifting cells that also listen to the ring, so that the bump is moved from where it is.";
  if ( Graph::overlap( l, r ) )
    return "'turn_left' and 'turn_right' drive the same cells: with one shifting group the bump has no preferred direction. Each direction needs its own group of shifting cells, with its own wiring back to the ring.";
  if ( !g.path( h, l, +1, 2 ) || !g.path( h, r, +1, 2 ) )
    return "The shifting cells do not listen to the ring: a shifter should be excited by the heading cells (and by a turn), so that only the cells at the bump are pushed on.";
  if ( !g.path( l, h, +1, 2 ) || !g.path( r, h, +1, 2 ) )
    return "A group of shifting cells does not excite the ring: the shifters must feed back onto the ring cells, one step to one side for one group and one step to the other side for the other.";
  return "";
}

}  // namespace spike::tasks::bestiary
