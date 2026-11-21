// Scaffolding shared by the micro-puzzles of concepts 1-10 (m_threshold ... m_neuromodulation; docs/DESIGN-0.3.md section 3).
// A micro-puzzle is a fish-tank task: 1-4 neurons, a 2-5 s episode, one concept, a dashboard with a few clear readouts.
// Nothing here touches NEST beyond Engine; the worlds are plain C++ and deterministic given the seed.
//
// DIFFICULTY (set_mode, docs/DESIGN-0.3.md section 5): a mode scales only the tolerance of the acceptance bands (tol()) and the noise
// of the stimulus (noise()); the structure of the puzzle never changes. Guided: wide bands, no noise. Standard: the bands as written
// in each task. Researcher: tight bands and physiological noise (the game asks for 5 seeds there).
#pragma once
#include <algorithm>
#include <array>
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

/// Read-only view of Engine::structure() for the mechanism checks of the micro-puzzles (docs/ENGINE-STRUCTURE.md). Populations are addressed by
/// index into Structure::populations; a channel's populations are the ones it is wired to (inputs: target pops, outputs: source pops).
/// Only projections with cells (n_edges > 0) and a non-zero mean weight count: a cut synapse (weight 0) is no wiring.
class Graph
{
public:
  Graph() = default;
  explicit Graph( Structure s ) : st( std::move( s ) ) {}
  Structure st;

  std::vector< int > in_pops( const std::string& ch ) const { return pops_of( st.inputs, ch ); }
  std::vector< int > out_pops( const std::string& ch ) const { return pops_of( st.outputs, ch ); }
  const PopulationInfo& pop( int i ) const { return st.populations[ static_cast< size_t >( i ) ]; }
  bool empty() const { return st.populations.empty(); }
  /// Number of cells of the populations in `s`.
  int cells( const std::vector< int >& s ) const
  {
    int n = 0;
    for ( int i : s ) n += pop( i ).n;
    return n;
  }
  static bool overlap( const std::vector< int >& a, const std::vector< int >& b )
  {
    for ( int x : a )
      for ( int y : b )
        if ( x == y ) return true;
    return false;
  }
  /// A direct projection from a population of `from` to one of `to` whose sign is `sign` (0 = any non-zero).
  bool edge( const std::vector< int >& from, const std::vector< int >& to, int sign = 0 ) const
  {
    for ( const auto& p : st.projections )
    {
      if ( !functional( st, p ) || ( sign != 0 && p.sign != sign ) ) continue;
      if ( has( from, p.pre_index ) && has( to, p.post_index ) ) return true;
    }
    return false;
  }
  /// A chain of 1..max_hops projections (0 hops allowed when `allow_zero` and the sets overlap) from `from` to `to` whose net sign is
  /// `net` (+1: an even number of inhibitory projections, -1: an odd number, 0: any).
  bool path( const std::vector< int >& from, const std::vector< int >& to, int net = 0, int max_hops = 4, bool allow_zero = false ) const
  {
    const int n = static_cast< int >( st.populations.size() );
    if ( n == 0 ) return false;
    std::vector< std::array< bool, 2 > > seen( static_cast< size_t >( n ), { false, false } );
    std::vector< std::pair< int, int > > frontier;
    for ( int f : from )
      if ( !seen[ f ][ 0 ] ) { seen[ f ][ 0 ] = true; frontier.push_back( { f, 0 } ); }
    auto hit = [ & ]( int node, int par ) { return has( to, node ) && ( net == 0 || ( net > 0 ? par == 0 : par == 1 ) ); };
    if ( allow_zero )
      for ( auto& s : frontier )
        if ( hit( s.first, s.second ) ) return true;
    for ( int hop = 1; hop <= max_hops && !frontier.empty(); ++hop )
    {
      std::vector< std::pair< int, int > > next;
      for ( auto& s : frontier )
        for ( const auto& p : st.projections )
        {
          if ( !functional( st, p ) || p.pre_index != s.first ) continue;
          const int par = s.second ^ ( p.sign < 0 ? 1 : 0 );
          if ( hit( p.post_index, par ) ) return true;
          if ( !seen[ p.post_index ][ par ] ) { seen[ p.post_index ][ par ] = true; next.push_back( { p.post_index, par } ); }
        }
      frontier = next;
    }
    return false;
  }
  /// `from` inhibits `to` across the midline: some population P is excited by (or is) `from`'s side, i.e. reaches `from` with net excitation, and reaches
  /// `to` with net inhibition (directly, or through an interneuron: at most max_hops projections). Reads the same for a half-centre of two cells (P =
  /// the cell itself) and for a spinal CPG whose commissural interneurons sit between the excitatory pools and the motor pools.
  bool crossed( const std::vector< int >& from, const std::vector< int >& to, int max_hops = 3 ) const
  {
    for ( size_t p = 0; p < st.populations.size(); ++p )
    {
      const std::vector< int > P = { static_cast< int >( p ) };
      if ( path( P, from, +1, max_hops, true ) && path( P, to, -1, max_hops ) ) return true;
    }
    return false;
  }
  /// Largest number of presynaptic cells one cell of `to` receives over the projections from populations anywhere (sum of indegree_max).
  int indegree( const std::vector< int >& to ) const
  {
    int best = 0;
    for ( int t : to )
    {
      int s = 0;
      for ( const auto& p : st.projections )
        if ( functional( st, p ) && p.post_index == t ) s += p.indegree_max;
      best = std::max( best, s );
    }
    return best;
  }
  /// Value of a numeric parameter the player set on a population (nan when the card's own value is used).
  double param( int i, const std::string& name ) const
  {
    auto it = pop( i ).params.find( name );
    return it == pop( i ).params.end() ? std::nan( "" ) : it->second;
  }

private:
  static bool has( const std::vector< int >& v, int x )
  {
    for ( int y : v )
      if ( y == x ) return true;
    return false;
  }
  std::vector< int > pops_of( const std::vector< ChannelStructure >& chans, const std::string& ch ) const
  {
    std::vector< int > r;
    for ( const auto& c : chans )
      if ( c.name == ch )
        for ( const auto& id : c.pops )
        {
          int i = -1;  // (a local lookup: Structure::population_index lives in engine.cpp, which the stub-engine physics test must not pull in)
          for ( size_t k = 0; k < st.populations.size(); ++k )
            if ( st.populations[ k ].id == id ) i = static_cast< int >( k );
          if ( i >= 0 && !has( r, i ) ) r.push_back( i );
        }
    return r;
  }
};

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
    inspect_once( e );
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
    inspected_ = false;
    mech_.clear();
    g_ = Graph();
    wiring_.invalidate();
    motor_.invalidate();
    readouts_.clear();
  }

  /// Mechanism check (v0.4 honesty contract): called once, before the first slice, with the live circuit. Read the structure through g_ and
  /// put a sentence naming the missing part of the mechanism (never a number) into mech_; leave mech_ empty when the mechanism is there.
  virtual void inspect( Engine& ) {}
  void inspect_once( Engine& e )
  {
    if ( inspected_ ) return;
    inspected_ = true;
    g_ = Graph( e.structure() );
    inspect( e );
  }
  /// A circuit that lacks the mechanism never passes and never scores more than 60.
  void apply_mech( Score& s ) const
  {
    if ( mech_.empty() ) return;
    s.passed = false;
    s.value = std::min( s.value, 60.0 );
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
  Graph g_;
  std::string mech_;
  Wiring wiring_;
  MotorReader motor_;
  Rng rng_;
  double t_ = 0;
  std::map< std::string, double > readouts_;

private:
  bool inspected_ = false;
  uint64_t hash_id() const
  {
    uint64_t h = 1469598103934665603ULL;
    for ( char c : info_.id ) h = ( h ^ static_cast< unsigned char >( c ) ) * 1099511628211ULL;
    return h;
  }
};

} // namespace spike::tasks::micro
