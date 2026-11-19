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
  /// `truth`: the fly's heading change (degrees) since the start at a time (ms) and for the quick (true) or full episode, for the compass gauge (readouts bump_deg, true_deg, bump_strength); empty = the heading stays at the landmark.
  explicit Landmark( Spec s, std::function< double( double, bool ) > truth = nullptr ) : Scripted( std::move( s ) ), truth_( std::move( truth ) ) {}
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
  void on_observe() override
  {
    const double lm = 360.0 * wedge_ / kWedges;
    readouts_[ "landmark_deg" ] = lm;
    readouts_[ "true_deg" ] = std::fmod( lm + ( truth_ ? truth_( t_, quick_ ) : 0.0 ) + 7200.0, 360.0 );
    readouts_[ "bump_deg" ] = meas_.vec_deg( "heading", std::max( 0.0, t_ - 100 ), t_ );
    readouts_[ "bump_strength" ] = meas_.vec_strength( "heading", std::max( 0.0, t_ - 100 ), t_ );
    readouts_[ "dark" ] = t_ > 500 ? 1.0 : 0.0;
  }
  int wedge_ = 0;
  std::function< double( double, bool ) > truth_;
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


// ---------------------------------------------------------------- the mushroom body: odours as patterns on PN sensors, a sparse Kenyon-cell code, a dopamine-gated KC > MBON synapse
/// The odour sensors: pn0 .. pn19 are the glomeruli (one Poisson input each); an odour is a pattern of six of them. A and B share one.
constexpr int kGlomeruli = 20;
inline const std::vector< int >& odour_a() { static const std::vector< int > a = { 1, 4, 7, 9, 12, 16 }; return a; }
inline const std::vector< int >& odour_b() { static const std::vector< int > b = { 0, 3, 7, 10, 13, 18 }; return b; }
inline std::vector< std::string > glomerulus_family()
{
  std::vector< std::string > v;
  for ( int k = 0; k < kGlomeruli; ++k ) v.push_back( "pn" + std::to_string( k ) );
  return v;
}
inline Task::ChannelKind glomerulus_kind() { return in_kind( "poisson", 100, 1000 ); }
/// Segments of one odour presentation on the glomeruli sensors at `hz`.
inline void odour_segments( const std::vector< int >& o, double t0, double t1, double hz, std::vector< Seg >& segs )
{
  for ( int k : o ) segs.push_back( { "pn" + std::to_string( k ), t0, t1, hz } );
}
/// "" when the Kenyon cells read the glomeruli with sparse, random-looking wiring (each cell listens to a few of them) and feed an inhibitory loop (the APL) that holds the code sparse.
inline std::string sparse_code_missing( const Graph& g, const std::string& kc_channel, bool need_apl = true )
{
  const auto kc = g.out_pops( kc_channel );
  if ( kc.empty() ) return "";
  std::vector< int > pn;
  for ( int k = 0; k < kGlomeruli; ++k )
    for ( int p : g.in_pops( "pn" + std::to_string( k ) ) )
      if ( std::find( pn.begin(), pn.end(), p ) == pn.end() ) pn.push_back( p );
  if ( pn.empty() ) return "";
  if ( Graph::overlap( pn, kc ) || !g.path( pn, kc, +1, 3 ) )
    return "The odour does not reach the Kenyon cells: the glomeruli sensors drive projection neurons, and those excite the Kenyon cells.";
  if ( g.cells( kc ) < 30 ) return "A sparse code needs many Kenyon cells, each answering to a few odours: use a large group of Kenyon cells.";
  for ( const auto& p : g.st.projections )
    if ( p.n_edges > 0 && p.sign > 0 && Graph::overlap( { p.pre_index }, pn ) && Graph::overlap( { p.post_index }, kc ) && p.indegree_max > 8 )
      return "Every Kenyon cell listens to many of the projection neurons: with that much overlap every odour excites the same cells. Each Kenyon cell should listen to just a few of them, picked at random (random sparse wiring: a few inputs per cell).";
  if ( need_apl && !g.path( kc, kc, -1, 3 ) )
    return "Nothing inhibits the Kenyon cells in proportion to their own activity: the code gets dense when the odour is strong. Add a cell that the Kenyon cells excite and that inhibits them all (the APL, feedback inhibition).";
  return "";
}
/// "" when the circuit has a dopamine-gated synapse from the odour code onto the avoidance cells, with a dopamine source that the punishment reaches.
inline std::string dopamine_gate_missing( const Graph& g, const std::string& punish, const std::string& motor, const std::string& first_sensor )
{
  const auto m = g.out_pops( motor ), pu = g.in_pops( punish ), od = g.in_pops( first_sensor );
  if ( m.empty() || pu.empty() ) return "";
  bool any_gate = false;
  for ( const auto& p : g.st.projections )
  {
    if ( p.n_edges <= 0 || p.sign <= 0 || p.synapse_model.find( "stdp_dopamine" ) == std::string::npos || !Graph::overlap( { p.post_index }, m ) ) continue;
    any_gate = true;
    if ( p.dopamine_index < 0 ) continue;
    if ( !od.empty() && !g.path( od, { p.pre_index }, +1, 3, true ) ) continue;
    if ( g.path( pu, { p.dopamine_index }, +1, 3, true ) && p.dopamine_index != p.pre_index && p.dopamine_index != p.post_index ) return "";
    return "The punishment does not reach the dopamine cells of this synapse: the dopamine source must be a cell group that the punishment sensor drives (directly or through a relay), and it must not be the odour cells or the avoidance cells themselves.";
  }
  (void)any_gate;
  return "Nothing in the circuit can learn: the synapse from the odour cells onto the avoidance cells is an ordinary one, so it answers every odour as before. It must be a dopamine-gated synapse, whose strength changes only when its input was active and a dopamine cell fires (select the synapse type, set its dopamine source).";
}


/// The training protocol of the mushroom-body levels (ms; 4400 in all). Odour A and B tests before and after; two pairings of odour A with punishment in between.
///   base [0, 400)  quiet                  a0 [550, 900)  odour A    b0 [1150, 1500)  odour B     (naive responses)
///   pairing 1 [1900, 2300), pairing 2 [2500, 2900): odour A and punishment together
///   a1 [3350, 3700)  odour A    b1 [3950, 4300)  odour B     (trained responses)
constexpr double kMbEnd = 4400;
constexpr double kMbHz = 150;   // the rate of an odour on its glomeruli
inline void mb_windows( Schedule& c )
{
  c.wins = { { "base", 0, 400 }, { "a0", 550, 900 }, { "b0", 1150, 1500 }, { "a1", 3350, 3700 }, { "b1", 3950, 4300 } };
}
/// Learning criteria on the avoidance channel `ch` (response = Hz per cell above the baseline of the quiet window).
inline void mb_judge( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p, const std::string& ch )
{
  const double base = m.hz( ch, "base" );
  auto r = [ & ]( const char* k ) { return std::max( 0.0, m.hz( ch, k ) - base ); };
  const double a0 = r( "a0" ), b0 = r( "b0" ), a1 = r( "a1" ), b1 = r( "b1" );
  p[ "response_a_before_hz" ] = a0;
  p[ "response_b_before_hz" ] = b0;
  p[ "response_a_after_hz" ] = a1;
  p[ "response_b_after_hz" ] = b1;
  p[ "a_after_over_before" ] = a0 > 0 ? a1 / a0 : 0.0;
  p[ "b_after_over_before" ] = b0 > 0 ? b1 / b0 : 0.0;
  p[ "baseline_hz" ] = base;
  const double need = 8.0 / m.tol;
  c.push_back( { "answers", a0 >= need && b0 >= need, ramp( std::min( a0, b0 ), 0, need ),
    "The avoidance cells hardly answer the odours before any training: the odour cells must drive them (the synapse onto them too weak, or the odour does not reach it). Without a response there is nothing to learn." } );
  const double lim = m.tol > 1.2 ? 0.8 : ( m.tol < 0.9 ? 0.6 : 0.65 );
  c.push_back( { "learns", a0 > 0 && a1 <= lim * a0, a0 > 0 ? 1.0 - ramp( a1 / a0, lim, 1.0 ) : 0.0,
    a1 > 1.15 * a0 ? "After the pairing the avoidance cells answer odour A MORE: the dopamine-gated synapse potentiated instead of depressing. In the mushroom body odour followed by punishment weakens the synapse."
                   : "After odour A was paired with punishment the avoidance cells still answer it as before: nothing learned. The dopamine cells must fire during the punishment, and the synapse must weaken when its input was active and dopamine is released: strengthen the learning of the synapse." } );
  const double sp = m.tol > 1.2 ? 0.45 : ( m.tol < 0.9 ? 0.6 : 0.55 );
  c.push_back( { "specific", b0 > 0 && b1 >= sp * b0, b0 > 0 ? ramp( b1 / b0, 0, sp ) : 0.0,
    "The avoidance cells also answer odour B less after training: the memory is not specific to the punished odour. Only the synapses of the odour that was active may weaken: the odours' codes must not overlap (sparse code), and the dopamine alone must not weaken silent inputs." } );
}


// ---------------------------------------------------------------- the pyloric rhythm: bursts, cycles and phases
struct Burst
{
  double t0, t1;  // first and last spike (ms)
  int n;          // spikes
};
/// Bursts of a channel in [from, to) ms: runs of spikes separated by gaps of at most `gap` ms.
inline std::vector< Burst > burst_list( const std::vector< float >& c, double from, double to, double gap )
{
  std::vector< Burst > out;
  double last = -1e9;
  for ( long k = static_cast< long >( from ); k < static_cast< long >( to ) && k < static_cast< long >( c.size() ); ++k )
  {
    const int n = static_cast< int >( c[ static_cast< size_t >( k ) ] + 0.5f );
    if ( n <= 0 ) continue;
    if ( out.empty() || static_cast< double >( k ) - last > gap ) out.push_back( { static_cast< double >( k ), static_cast< double >( k ), 0 } );
    out.back().t1 = static_cast< double >( k );
    out.back().n += n;
    last = static_cast< double >( k );
  }
  return out;
}
/// Cycle statistics of the three-phase pattern: cycles are the intervals between successive onsets of the first channel's bursts; in each, the phases (onset and duty, as fractions of the cycle)
/// of the other channels' bursts. `ok` cycles hold exactly one burst of each follower.
struct Pyloric
{
  int cycles = 0, ok = 0;
  double period = 0, period_cv = 0;
  double duty_a = 0, on_b = 0, duty_b = 0, on_c = 0, duty_c = 0;
};
inline Pyloric pyloric_stats( const Meas& m, const std::string& a, const std::string& b, const std::string& c, double from, double to, double gap = 120 )
{
  Pyloric r;
  auto ia = m.cnt.find( a ), ib = m.cnt.find( b ), ic = m.cnt.find( c );
  if ( ia == m.cnt.end() || ib == m.cnt.end() || ic == m.cnt.end() ) return r;
  const auto A = burst_list( ia->second, from, to, gap ), B = burst_list( ib->second, from, to, gap ), C = burst_list( ic->second, from, to, gap );
  if ( A.size() < 3 ) return r;
  std::vector< double > per;
  for ( size_t i = 0; i + 1 < A.size(); ++i ) per.push_back( A[ i + 1 ].t0 - A[ i ].t0 );
  double sum = 0, sq = 0;
  for ( double x : per ) sum += x;
  r.period = sum / static_cast< double >( per.size() );
  for ( double x : per ) sq += ( x - r.period ) * ( x - r.period );
  r.period_cv = r.period > 0 ? std::sqrt( sq / static_cast< double >( per.size() ) ) / r.period : 0.0;
  r.cycles = static_cast< int >( per.size() );
  for ( size_t i = 0; i + 1 < A.size(); ++i )
  {
    const double t0 = A[ i ].t0, t1 = A[ i + 1 ].t0, T = t1 - t0;
    const Burst* pb = nullptr;
    const Burst* pc = nullptr;
    int nb = 0, nc = 0;
    for ( const auto& x : B ) if ( x.t0 >= t0 && x.t0 < t1 ) { pb = &x; ++nb; }
    for ( const auto& x : C ) if ( x.t0 >= t0 && x.t0 < t1 ) { pc = &x; ++nc; }
    if ( nb != 1 || nc != 1 ) continue;
    ++r.ok;
    r.duty_a += ( A[ i ].t1 - A[ i ].t0 + 10 ) / T;
    r.on_b += ( pb->t0 - t0 ) / T;
    r.duty_b += ( pb->t1 - pb->t0 + 10 ) / T;
    r.on_c += ( pc->t0 - t0 ) / T;
    r.duty_c += ( pc->t1 - pc->t0 + 10 ) / T;
  }
  if ( r.ok > 0 )
  {
    const double n = r.ok;
    r.duty_a /= n; r.on_b /= n; r.duty_b /= n; r.on_c /= n; r.duty_c /= n;
  }
  return r;
}
/// Models that cannot burst or rebound on their own (no slow currents).
inline bool plain_cell( const std::string& m ) { return is_one_of( m, { "iaf_psc_alpha", "iaf_psc_exp", "iaf_psc_delta", "iaf_cond_alpha", "iaf_cond_exp", "parrot_neuron" } ); }

}  // namespace spike::tasks::bestiary
