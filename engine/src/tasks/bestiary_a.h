// Scaffolding for the Bestiary chapters of 0.5 (worm, leech, aplysia; docs/DESIGN-0.5.md, research/bestiary-a.md).
// Every level of these chapters is a SCRIPTED WORLD: a schedule of input levels on the sensors, a list of measurement windows on the motor channels,
// a mechanism check on the circuit structure, and a judge that turns the window counts into acceptance criteria. The base class does the rest (wiring
// check, spike counting at 1 ms, live readouts, the verdict: the first failing criterion speaks, partial credit, three hint tiers, modes).
//
// HONESTY CONTRACT (docs/REQ-0.4.md): a level's `mech` reads the circuit through Engine::structure() (Graph / mech:: helpers) and names the missing part of
// the mechanism; verdicts and hint tiers 1-3 never contain a number (recipe() has them); the criteria's thresholds live in the judge, never in a sentence.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "mech_util.h"
#include "micro_a.h"

namespace spike::tasks::bestiary
{
using micro::Graph;

/// Input level `v` on channel `ch` over [t0, t1) ms (0 elsewhere; Hz for a Poisson sensor, the unit of the channel kind otherwise).
struct Seg
{
  std::string ch;
  double t0, t1, v;
};
/// A measurement window [t0, t1) ms.
struct Win
{
  std::string key;
  double t0, t1;
};
struct Schedule
{
  double dur = 4000;
  std::vector< Seg > segs;
  std::vector< Win > wins;
};

/// What the judge reads: spike counts of the motor channels, per millisecond.
class Meas
{
public:
  std::map< std::string, std::vector< float > > cnt;  // channel -> spikes in [k, k+1) ms (summed over the pool)
  std::map< std::string, double > pool;               // channel -> number of neurons
  // 0.5 chapter B (add-only): population_vector outputs: per ms, count * strength * (cos, sin) of the bump angle of that step (Spec::vec lists the channels)
  std::map< std::string, std::vector< float > > vx, vy;
  std::map< std::string, std::vector< std::pair< int, int > > > spk;  // (ms, circuit-level neuron) of every spike of the channels Spec::cells lists (distinct active cells)
  std::map< std::string, double > extra;               // facts the task chose for this episode (e.g. which wedge the landmark was at), set in its reset()
  std::map< std::string, std::vector< float > > val;   // 0.6 (add-only): rate_value channel -> value at the end of every ms (Spec::vals)
  /// Mean of a rate_value channel over [t0, t1) ms; and its smallest / largest value there.
  double vmean( const std::string& ch, double t0, double t1 ) const
  {
    auto it = val.find( ch );
    if ( it == val.end() ) return 0;
    double s = 0;
    long n = 0;
    for ( long k = std::max( 0L, static_cast< long >( t0 ) ); k < static_cast< long >( t1 ) && k < static_cast< long >( it->second.size() ); ++k, ++n ) s += it->second[ static_cast< size_t >( k ) ];
    return n ? s / n : 0.0;
  }
  double vmin( const std::string& ch, double t0, double t1 ) const { return vextreme( ch, t0, t1, false ); }
  double vmax( const std::string& ch, double t0, double t1 ) const { return vextreme( ch, t0, t1, true ); }
  double vextreme( const std::string& ch, double t0, double t1, bool hi ) const
  {
    auto it = val.find( ch );
    if ( it == val.end() ) return 0;
    double r = hi ? -1e30 : 1e30;
    for ( long k = std::max( 0L, static_cast< long >( t0 ) ); k < static_cast< long >( t1 ) && k < static_cast< long >( it->second.size() ); ++k )
      r = hi ? std::max( r, static_cast< double >( it->second[ static_cast< size_t >( k ) ] ) ) : std::min( r, static_cast< double >( it->second[ static_cast< size_t >( k ) ] ) );
    return r;
  }
  std::map< std::string, std::set< int > > sets;       // 0.6 (add-only): named sets of circuit-level neurons the task worked out from the circuit (Spec::tick), e.g. "cued" and "uncued" cells of a pattern
  std::vector< Win > wins;
  double tol = 1.0;                                   // mode scale: 1.5 guided, 1 standard, 0.7 researcher (wider bands for larger)
  double dur = 0;

  double sum( const std::string& ch, double t0, double t1 ) const
  {
    auto it = cnt.find( ch );
    if ( it == cnt.end() ) return 0;
    double s = 0;
    for ( long k = std::max( 0L, static_cast< long >( t0 ) ); k < static_cast< long >( t1 ) && k < static_cast< long >( it->second.size() ); ++k ) s += it->second[ static_cast< size_t >( k ) ];
    return s;
  }
  const Win* win( const std::string& key ) const
  {
    for ( const auto& w : wins )
      if ( w.key == key ) return &w;
    return nullptr;
  }
  /// Total spikes of the channel's pool in the window.
  double n( const std::string& ch, const std::string& key ) const
  {
    const Win* w = win( key );
    return w ? sum( ch, w->t0, w->t1 ) : 0.0;
  }
  /// Bump direction (degrees in [0, 360)) of a population_vector channel over [t0, t1): the circular mean of its spikes; strength is the length of that mean vector per spike (0..1, 0 = no spike or no preference).
  double vec_deg( const std::string& ch, double t0, double t1 ) const
  {
    double x = 0, y = 0;
    vsum( ch, t0, t1, x, y );
    double a = std::atan2( y, x ) * 180.0 / 3.14159265358979323846;
    return a < 0 ? a + 360.0 : a;
  }
  double vec_strength( const std::string& ch, double t0, double t1 ) const
  {
    double x = 0, y = 0;
    vsum( ch, t0, t1, x, y );
    const double n = sum( ch, t0, t1 );
    return n > 0 ? std::sqrt( x * x + y * y ) / n : 0.0;
  }
  void vsum( const std::string& ch, double t0, double t1, double& x, double& y ) const
  {
    auto a = vx.find( ch ), b = vy.find( ch );
    if ( a == vx.end() || b == vy.end() ) return;
    for ( long k = std::max( 0L, static_cast< long >( t0 ) ); k < static_cast< long >( t1 ) && k < static_cast< long >( a->second.size() ); ++k )
    {
      x += a->second[ static_cast< size_t >( k ) ];
      y += b->second[ static_cast< size_t >( k ) ];
    }
  }
  /// Distinct cells of a Spec::cells channel that fired at least once in [t0, t1) ms; `both` counts the cells that fired in [t0, t1) and also in [u0, u1).
  int active( const std::string& ch, double t0, double t1 ) const { return both( ch, t0, t1, t0, t1 ); }
  int both( const std::string& ch, double t0, double t1, double u0, double u1 ) const
  {
    auto it = spk.find( ch );
    if ( it == spk.end() ) return 0;
    std::set< int > a, b;
    for ( const auto& [ t, n ] : it->second )
    {
      if ( t >= t0 && t < t1 ) a.insert( n );
      if ( t >= u0 && t < u1 ) b.insert( n );
    }
    int c = 0;
    for ( int n : a ) c += b.count( n ) ? 1 : 0;
    return c;
  }
  /// Distinct cells of the named set (Meas::sets) of the channel's tracked cells that fired in [t0, t1).
  int active_in( const std::string& ch, double t0, double t1, const std::string& set_name ) const
  {
    auto it = spk.find( ch );
    auto st = sets.find( set_name );
    if ( it == spk.end() || st == sets.end() ) return 0;
    std::set< int > a;
    for ( const auto& [ t, n ] : it->second )
      if ( t >= t0 && t < t1 && st->second.count( n ) ) a.insert( n );
    return static_cast< int >( a.size() );
  }
  int size_of( const std::string& set_name ) const
  {
    auto st = sets.find( set_name );
    return st == sets.end() ? 0 : static_cast< int >( st->second.size() );
  }
  /// Spikes per cell in the window (a response that does not depend on how many cells the player gave the motor pool).
  double c( const std::string& ch, const std::string& key ) const
  {
    auto p = pool.find( ch );
    return n( ch, key ) / std::max( 1.0, p == pool.end() ? 1.0 : p->second );
  }
  /// Mean rate per neuron (Hz) in the window.
  double hz( const std::string& ch, const std::string& key ) const
  {
    const Win* w = win( key );
    if ( !w || w->t1 <= w->t0 ) return 0.0;
    auto p = pool.find( ch );
    return sum( ch, w->t0, w->t1 ) / ( ( w->t1 - w->t0 ) / 1000.0 ) / std::max( 1.0, p == pool.end() ? 1.0 : p->second );
  }
  /// Share of `a` in the spikes of a and b in the window (0.5 when both are silent).
  double share( const std::string& a, const std::string& b, const std::string& key ) const
  {
    const double x = n( a, key ), y = n( b, key );
    return x + y > 0 ? x / ( x + y ) : 0.5;
  }
  /// Thresholds that scale with the mode: a floor a player must reach (lower in guided), a ceiling it must stay under (higher in guided).
  double lo( double x ) const { return x / tol; }
  double hi( double x ) const { return x * tol; }
};

/// One acceptance criterion: `ok`, the partial credit `soft` (0..1) of a failed run and the sentence (no number) said when it fails.
struct Crit
{
  std::string key;
  bool ok = true;
  double soft = 1.0;
  std::string fail;
};
inline double ramp( double x, double lo_x, double hi_x ) { return hi_x > lo_x ? clampd( ( x - lo_x ) / ( hi_x - lo_x ), 0.0, 1.0 ) : ( x >= hi_x ? 1.0 : 0.0 ); }

/// A live gauge fed from a score part: the readout `band` (a name of Info::bands) follows the part while the episode runs, so the gauge is never unfed. `scale` converts the part's unit
/// (ms -> s); `skip_zero`: a part of 0 means "not measurable yet" and sets no readout (the game then shows a dash); `from_ms`: nothing before this model time.
struct GaugeFeed
{
  std::string part;
  double scale = 1.0;
  bool skip_zero = true;
  double from_ms = 0;
};

struct Spec
{
  micro::MicroBase::Info info;
  std::map< std::string, GaugeFeed > gauges;           // band name -> the part that feeds it
  std::string kind = "micro";                          // level_kind: "micro" | "task" | "diagnosis"
  std::function< Schedule( bool quick ) > schedule;
  std::function< std::string( const Graph& ) > mech;   // "" when the mechanism is there, else the sentence naming the missing part
  std::function< std::string( const Graph&, Engine& ) > mech_e;  // 0.6 (add-only): the same, with the live engine (a cell's own parameters and phase plane); used instead of `mech` when set
  std::function< void( const Meas&, std::vector< Crit >&, std::map< std::string, double >& ) > judge;
  std::string ok_verdict;                              // said when everything passes
  std::string h1, h2, h2_target, h3, h3_target;        // hint tiers 1-3 (no numbers)
  std::string metric_name, metric_unit;
  bool metric_lower = true;
  std::function< double( const Meas& ) > metric;
  std::string concept_extra;
  std::vector< std::string > readouts;                 // motor channels shown live (default: every motor)
  bool spine = true;
  std::vector< std::string > vec;                      // motor channels read as a ring (population_vector): Meas::vec_deg / vec_strength
  std::vector< std::string > cells;                    // motor channels whose individual cells are tracked (Meas::active / Meas::both)
  std::function< void( Engine&, std::map< std::string, double >& ) > start;  // 0.6 (add-only): called once before the first slice with the live engine; may set the initial state of cells and fill the per-channel level scale
  std::set< std::string > raw;                           // 0.6 (add-only): sensor channels whose level is used as it is (a stimulus angle), not scaled by the episode's small gain noise
  std::function< void( Schedule&, uint64_t, std::map< std::string, double >& ) > reseed;  // 0.6 (add-only): called in reset() with the episode's seed: may redraw the levels of the schedule (random stimuli) and note them in Meas::extra
  std::function< void( Engine&, double, Meas& ) > tick;  // 0.6 (add-only): called after every slice with the live engine, the model time and the measurements (fill Meas::sets, Meas::extra)
  std::vector< std::string > vals;                     // 0.6 (add-only): rate_value motor channels recorded per ms in Meas::val (the mean `rate` of the units)
  std::function< std::vector< Task::Event >() > timeline;  // optional: the events shown under the world (default: one per input segment, labelled with its channel)
};

class Scripted : public micro::MicroBase
{
public:
  explicit Scripted( Spec s ) : MicroBase( s.info ), s_( std::move( s ) )
  {
    sched_ = s_.schedule( false );
    quick_sched_ = s_.schedule( true );
    reset( 1 );
  }
  std::string level_kind() const override { return s_.kind; }
  bool on_spine() const override { return s_.spine; }
  double duration_ms() const override { return sched_.dur; }
  double quick_duration_ms() const override { return quick_sched_.dur; }
  std::vector< Event > timeline() const override
  {
    if ( s_.timeline ) return s_.timeline();
    std::vector< Event > v;
    for ( const auto& g : sched_.segs ) v.push_back( { g.t0, g.t1 - g.t0, g.ch } );
    return v;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    cur_ = quick_ ? quick_sched_ : sched_;
    gain_ = 1.0 + 0.03 * noise() * rng_.normal();
    meas_ = Meas();
    meas_.dur = cur_.dur;
    meas_.wins = cur_.wins;
    if ( s_.reseed ) s_.reseed( cur_, seed, meas_.extra );
    for ( const auto& m : info_.motors ) meas_.cnt[ m ].assign( static_cast< size_t >( cur_.dur ) + 2, 0.0f );
    for ( const auto& m : info_.motors ) lp_[ m ].reset( 0 );
    meas_.spk.clear();
    for ( const auto& m : s_.vals ) meas_.val[ m ].assign( static_cast< size_t >( cur_.dur ) + 2, 0.0f );
    for ( const auto& m : s_.vec )
    {
      meas_.vx[ m ].assign( static_cast< size_t >( cur_.dur ) + 2, 0.0f );
      meas_.vy[ m ].assign( static_cast< size_t >( cur_.dur ) + 2, 0.0f );
    }
    kicked_ = false;
    scale_.clear();
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, s_.h1, "" }, { 2, s_.h2, s_.h2_target }, { 3, s_.h3, s_.h3_target.empty() ? s_.h2_target : s_.h3_target } };
  }
  std::map< std::string, Metric > metrics() const override
  {
    if ( s_.metric_name.empty() || !s_.metric ) return {};
    return { { s_.metric_name, Metric{ s_.metric( measure() ), s_.metric_unit, s_.metric_lower } } };
  }

  Score score() const override
  {
    Score s;
    Meas m = measure();
    std::vector< Crit > crits;
    std::map< std::string, double > parts;
    for ( const auto& w : m.wins )
      for ( const auto& ch : info_.motors )
        if ( std::find( s_.vals.begin(), s_.vals.end(), ch ) == s_.vals.end() ) parts[ ch + "_" + w.key + "_hz" ] = m.hz( ch, w.key );   // (a rate_value channel has no spikes to count)
    if ( s_.judge ) s_.judge( m, crits, parts );
    s.parts = parts;
    int met = 0;
    double soft = 0;
    for ( const auto& c : crits )
    {
      met += c.ok ? 1 : 0;
      soft += c.ok ? 1.0 : clampd( c.soft, 0.0, 0.95 );
    }
    const bool all = met == static_cast< int >( crits.size() );
    s.passed = all && finished();
    s.value = s.passed ? 100.0 : 85.0 * ( crits.empty() ? 0.0 : soft / static_cast< double >( crits.size() ) );
    if ( !finished() || !wiring_.ok() ) s.passed = false;   // a half-wired circuit never passes (chapters B: a sensor or motor that is not connected is no solution)
    apply_mech( s );
    s.parts[ "criteria_met" ] = met;
    s.parts[ "criteria" ] = static_cast< double >( crits.size() );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !finished() )
      o << "Running.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( !all )
    {
      for ( const auto& c : crits )
        if ( !c.ok )
        {
          o << c.fail;
          break;
        }
    }
    else
      o << s_.ok_verdict;
    s.verdict = o.str();
    return s;
  }

protected:
  void inspect( Engine& e ) override
  {
    if ( s_.mech_e && !g_.empty() ) mech_ = s_.mech_e( g_, e );
    else if ( s_.mech && !g_.empty() ) mech_ = s_.mech( g_ );
  }

  void drive( Engine& e, double ) override
  {
    if ( !kicked_ )
    {
      kicked_ = true;
      on_start( e );
    }
    for ( const auto& ch : info_.sensors )
    {
      double v = 0;
      for ( const auto& g : cur_.segs )
        if ( g.ch == ch && t_ >= g.t0 && t_ < g.t1 ) v = std::max( v, g.v );
      auto sc = scale_.find( ch );
      e.set_input( ch, v * ( s_.raw.count( ch ) ? 1.0 : gain_ ) * ( sc == scale_.end() ? 1.0 : sc->second ) );
    }
  }

  void observe( Engine& e, double dt ) override
  {
    const long k = static_cast< long >( t_ + 0.5 ) - 1;
    for ( const auto& m : info_.motors )
    {
      const double n = motor_.spike_count( e, m, dt );
      meas_.pool[ m ] = motor_.pool_size( e, m );
      auto& v = meas_.cnt[ m ];
      if ( k >= 0 && k < static_cast< long >( v.size() ) ) v[ static_cast< size_t >( k ) ] += static_cast< float >( n );
      lp_[ m ].tau_ms = 60;
      readouts_[ m + "_hz" ] = lp_[ m ].step( n / meas_.pool[ m ] * 1000.0 / dt, dt );
    }
    for ( const auto& m : s_.vals )
      if ( k >= 0 && k < static_cast< long >( meas_.val[ m ].size() ) ) meas_.val[ m ][ static_cast< size_t >( k ) ] = static_cast< float >( e.output_value( m ) );
    for ( const auto& m : s_.cells )
      for ( int32_t n : e.output_spiked( m ) )
        if ( k >= 0 ) meas_.spk[ m ].push_back( { static_cast< int >( k ), n } );
    for ( const auto& m : s_.vec )
    {
      const double n = e.output_count( m ), a = e.output_angle( m ), st = e.output_strength( m );
      if ( k < 0 || k >= static_cast< long >( meas_.vx[ m ].size() ) || n <= 0 ) continue;
      meas_.vx[ m ][ static_cast< size_t >( k ) ] += static_cast< float >( n * st * std::cos( a ) );
      meas_.vy[ m ][ static_cast< size_t >( k ) ] += static_cast< float >( n * st * std::sin( a ) );
    }
    if ( s_.tick ) s_.tick( e, t_, meas_ );
    on_observe();
    if ( !s_.gauges.empty() && s_.judge && static_cast< long >( t_ ) % 250 == 0 ) feed_gauges();
  }
  /// Evaluate the judge on what has been measured so far and copy the parts the gauges name into the readouts.
  void feed_gauges()
  {
    std::vector< Crit > cr;
    std::map< std::string, double > parts;
    Meas m = measure();
    m.dur = t_;  // so far
    s_.judge( m, cr, parts );
    for ( const auto& [ band, f ] : s_.gauges )
    {
      auto it = parts.find( f.part );
      if ( it == parts.end() || t_ < f.from_ms || ( f.skip_zero && std::fabs( it->second ) < 1e-9 ) ) continue;
      readouts_[ band ] = it->second * f.scale;
    }
  }

  /// Hooks for levels that need a start-up action (a task that sets V_m) or extra live readouts.
  virtual void on_start( Engine& e )
  {
    if ( s_.start ) s_.start( e, scale_ );
  }
  virtual void on_observe() {}

  bool finished() const { return t_ >= cur_.dur - 1e-6; }
  Meas measure() const
  {
    Meas m = meas_;
    m.tol = tol();
    return m;
  }

  Spec s_;
  Schedule sched_, quick_sched_, cur_;
  Meas meas_;
  double gain_ = 1;
  bool kicked_ = false;
  std::map< std::string, double > scale_;   // 0.6 (add-only): per sensor channel, a factor on the schedule's level (a level whose stimulus is scaled to the cell, set in on_start)
  std::map< std::string, Lowpass > lp_;
};

// ---------------------------------------------------------------- small helpers for the mechanism checks
inline std::string chan_kind( const Graph& g, const std::vector< ChannelStructure >& v, const std::string& ch )
{
  for ( const auto& c : v )
    if ( c.name == ch ) return c.kind;
  return "";
}
inline bool is_one_of( const std::string& m, std::initializer_list< const char* > l )
{
  for ( const char* x : l )
    if ( m == x ) return true;
  return false;
}
/// Spike trains: runs of spikes closer than `isi` ms, from per-ms counts of one channel in [t0, t1). Returns the number of spikes of each run.
inline std::vector< int > runs_of( const std::vector< float >& c, double t0, double t1, double isi )
{
  std::vector< int > out;
  double last = -1e9;
  int cur = 0;
  for ( long k = static_cast< long >( t0 ); k < static_cast< long >( t1 ) && k < static_cast< long >( c.size() ); ++k )
  {
    const int n = static_cast< int >( c[ static_cast< size_t >( k ) ] + 0.5f );
    if ( n <= 0 ) continue;
    if ( cur > 0 && static_cast< double >( k ) - last > isi )
    {
      out.push_back( cur );
      cur = 0;
    }
    cur += n;
    last = static_cast< double >( k );
  }
  if ( cur > 0 ) out.push_back( cur );
  return out;
}

// ---------------------------------------------------------------- Aplysia: depressing synapses and a modulated synapse
inline bool is_stp( const std::string& m ) { return m == "tsodyks2_synapse" || m == "tsodyks_synapse" || m == "quantal_stp_synapse"; }
/// Index of an excitatory projection that lies on a path from `from` to `to` (the projection itself is a hop of the path), or -1. With `stp_only` only projections
/// with a short-term-plasticity synapse model count.
inline int projection_on_path( const Graph& g, const std::vector< int >& from, const std::vector< int >& to, bool stp_only )
{
  for ( size_t k = 0; k < g.st.projections.size(); ++k )
  {
    const auto& p = g.st.projections[ k ];
    if ( p.n_edges <= 0 || p.sign <= 0 || ( stp_only && !is_stp( p.synapse_model ) ) ) continue;
    if ( g.path( from, { p.pre_index }, +1, 3, true ) && g.path( { p.post_index }, to, +1, 3, true ) ) return static_cast< int >( k );
  }
  return -1;
}
/// "" when the touch path carries a depressing synapse, else the sentence.
inline std::string depression_missing( const Graph& g, const std::vector< int >& touch, const std::vector< int >& gill )
{
  if ( projection_on_path( g, touch, gill, true ) >= 0 ) return "";
  return "Every synapse between the touch and the gill is a static one: a static synapse answers every touch with the same strength, so the reflex can never habituate. "
         "The synapse that carries the touch signal must be a depressing one (it runs out of transmitter when it is used and recovers slowly).";
}
/// "" when the circuit has a facilitator: the shock input reaches the cells behind a modulator output, and that modulator is wired (in the modulation table) to a
/// synapse on the touch path. `mod_out` is the name of the modulator output channel.
inline std::string facilitator_missing( const Graph& g, const std::vector< int >& touch, const std::vector< int >& gill, const std::vector< int >& shock, const std::string& mod_out )
{
  const ChannelStructure* mo = nullptr;
  for ( const auto& o : g.st.outputs )
    if ( o.name == mod_out ) mo = &o;
  if ( !mo || mo->kind != "modulator" || mo->pops.empty() )
    return "Nothing releases a modulator: the facilitating interneuron has to release serotonin (set the '" + mod_out + "' channel to release a modulator and read it from that cell).";
  std::vector< int > src;
  for ( const auto& id : mo->pops ) src.push_back( g.st.population_index( id ) );
  if ( !shock.empty() && !g.path( shock, src, +1, 3, true ) )
    return "The tail shock does not reach the cell that releases the modulator: connect the tail sensor to the facilitating interneuron.";
  for ( const auto& r : g.st.modulation )
  {
    if ( r.proj_index < 0 || r.modulator != mo->modulator ) continue;
    const auto& p = g.st.projections[ static_cast< size_t >( r.proj_index ) ];
    if ( g.path( touch, { p.pre_index }, +1, 3, true ) && g.path( { p.post_index }, gill, +1, 3, true ) ) return "";
  }
  return "The modulator does not act on the touch synapse: it has to change the strength of the synapse that carries the touch signal to the gill motor neuron "
         "(a synapse row of the modulation table: the released modulator raises its release or its weight). Released into nothing, serotonin changes nothing.";
}

}  // namespace spike::tasks::bestiary
