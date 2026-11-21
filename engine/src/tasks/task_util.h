// Shared helpers for the task worlds: a tiny deterministic RNG, a first-order filter and a motor-rate reader.
// Nothing here touches NEST; the worlds are plain C++ and are the same on every platform.
#pragma once
#include <algorithm>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
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

// ---- functional weight floor (honesty contract: a wire that cannot move its target is no wiring) --------------------------------------------
/// The smallest weight that can change the state of a cell of this model, in the model's own unit: a connection weaker than this is "token wiring", not a
/// mechanism. Current cells (pA), delta and rate units (dimensionless) and conductance cells (nS, the NESTML models) each have their own scale; the limits sit
/// a factor of 20 or more under the weakest weight of any reference circuit and far above the 0.001 to 0.01 that the cheat circuits use. A parrot cell relays
/// whatever it hears, whatever the weight.
inline double weight_floor( const std::string& model, bool sensor = false, const std::string& kind = "" )
{
  auto has = [ & ]( const char* s ) { return model.find( s ) != std::string::npos; };
  if ( model == "parrot_neuron" ) return 0.0;
  if ( sensor && kind == "dc" ) return 0.001;   // a dc sensor's weight multiplies a level of any size (pA per unit): only a vanishing weight is token wiring
  if ( has( "rate" ) || has( "siegert" ) || has( "delta" ) ) return 0.05;
  if ( has( "psc" ) || model == "izhikevich" || has( "izhikevich" ) || has( "hh_" ) || has( "mat2" ) || has( "gif_" ) ) return 0.5;
  return sensor ? 0.002 : 0.01;  // (a sensor's weight multiplies a Poisson rate: the weakest reference sensor weight onto a conductance cell is 0.005)
}
/// A projection is functional when it has connections, a sign, and a weight (the largest one of its edges) that reaches the floor of its target's model.
inline bool functional( const Structure& st, const ProjectionInfo& pr )
{
  if ( pr.n_edges <= 0 || pr.sign == 0 || pr.post_index < 0 || pr.post_index >= static_cast< int >( st.populations.size() ) ) return false;
  // a mean-field (diffusion_connection) projection reports its drift factor, whose scale is J x K x tau, not a synaptic weight: only a vanishing one is token wiring
  if ( pr.synapse_model == "diffusion_connection" ) return std::max( std::fabs( pr.w_min ), std::fabs( pr.w_max ) ) >= 1e-4;
  return std::max( std::fabs( pr.w_min ), std::fabs( pr.w_max ) ) >= weight_floor( st.populations[ static_cast< size_t >( pr.post_index ) ].model );
}
/// The same for one live connection: the model of the target cell's population.
inline bool functional_edge( const Structure& st, const Engine& e, int32_t post, double weight, bool diffusion = false )
{
  if ( weight == 0.0 ) return false;
  if ( diffusion ) return std::fabs( weight ) >= 1e-4;
  const std::string id = e.neuron_population( post );
  for ( const auto& p : st.populations )  // (a local lookup: Structure::population lives in engine.cpp, which the stub-engine physics test does not link)
    if ( p.id == id ) return std::fabs( weight ) >= weight_floor( p.model );
  return true;
}

/// The level to set on a dc sensor that carries a disturbance the TASK owns (a kick, a push): the cells receive `nominal_w` pA per unit of level whatever weight the circuit gives the channel, so that
/// turning the weight down does not make the disturbance disappear (the weight is the player's knob only for sensors that carry information). The sign of the circuit's weight is kept; a weight of 0
/// gives no disturbance (and the sensor is reported as not wired).
inline double owned_level( const Engine& e, const std::string& ch, double level, double nominal_w )
{
  const double w = e.input_weight( ch );
  return w != 0.0 ? level * nominal_w / std::fabs( w ) : 0.0;
}

/// "Does the group that should do the work ever fire?" (gameplay review, finding 3). A verdict that says "strengthen X" is no help when X's input cells never fire: no weight of X changes anything.
/// A task arms a SourceWatch on the populations that feed a target (the opposing motoneurons, a dilator pool, a turn command) and calls tick() every slice; silent() / say() then tell, in words,
/// whether some feeding group never fired. Spikes are not drained from the engine (the lab's raster owns them): a few cells per group are sampled through their membrane state, and a cell counts as
/// having fired when its last-spike time is set, its voltage falls by 5 mV or more between two slices (a reset), or it reaches its own threshold.
class SourceWatch
{
public:
  void reset()
  {
    groups_.clear();
    armed_ = false;
  }
  /// The populations (indices into st.populations) with a functional projection of `sign` (0 = any) onto a population of `targets`, not themselves among the targets.
  void arm( const Structure& st, const std::set< int >& targets, int sign = 0 )
  {
    armed_ = true;
    for ( const auto& pr : st.projections )
    {
      if ( !functional( st, pr ) || !targets.count( pr.post_index ) || targets.count( pr.pre_index ) || ( sign != 0 && pr.sign != sign ) ) continue;
      bool have = false;
      for ( const auto& g : groups_ ) have = have || g.pop == pr.pre_index;
      if ( have ) continue;
      Group g;
      g.pop = pr.pre_index;
      const auto& p = st.populations[ static_cast< size_t >( pr.pre_index ) ];
      const int n = std::min( p.n, 4 );
      for ( int k = 0; k < n; ++k ) g.cells.push_back( p.first_index + ( p.n > n ? k * ( p.n / n ) : k ) );
      g.prev.assign( g.cells.size(), -1e9 );
      g.base.assign( g.cells.size(), -1e9 );
      groups_.push_back( g );
    }
  }
  bool armed() const { return armed_; }
  /// Call after every slice.
  void tick( const Engine& e )
  {
    for ( auto& g : groups_ )
    {
      if ( g.fired ) continue;
      for ( size_t i = 0; i < g.cells.size() && !g.fired; ++i )
      {
        const int32_t n = g.cells[ i ];
        const double ts = e.neuron_state( n, "t_spike" ), v = e.voltage( n ), vth = e.neuron_state( n, "V_th" );
        if ( g.base[ i ] < -1e8 ) g.base[ i ] = std::isfinite( ts ) ? ts : -1.0;   // the last-spike time at the first watched slice: only a LATER spike counts (a task may start watching late)
        if ( std::isfinite( ts ) && ts > g.base[ i ] + 0.05 ) g.fired = true;
        else if ( g.prev[ i ] > -1e8 && g.prev[ i ] - v >= 5.0 ) g.fired = true;
        else if ( std::isfinite( vth ) && v >= vth - 0.5 ) g.fired = true;
        g.prev[ i ] = v;
      }
    }
  }
  /// Some feeding group exists and none of its sampled cells has fired (so far).
  bool all_silent() const
  {
    if ( groups_.empty() ) return false;
    for ( const auto& g : groups_ ) if ( g.fired ) return false;
    return true;
  }
  bool some_silent() const
  {
    for ( const auto& g : groups_ ) if ( !g.fired ) return true;
    return false;
  }
  /// "The <what> never fires: ..." when a feeding group is silent, else "" (no digits, no population names).
  std::string say( const std::string& what ) const
  {
    if ( all_silent() ) return "The " + what + " never fire: give them input or a steady drive (a bias current) before changing any weight downstream of them.";
    if ( some_silent() ) return "One of the groups among the " + what + " never fires: give it input or a steady drive (a bias current).";
    return "";
  }

private:
  struct Group
  {
    int pop = -1;
    std::vector< int32_t > cells;
    std::vector< double > prev, base;
    bool fired = false;
  };
  std::vector< Group > groups_;
  bool armed_ = false;
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
  void invalidate() { probed_ = false; unwired_.clear(); weak_.clear(); bare_ = false; }
  /// True when the bench holds no cell at all (nothing built): a score of 0 with no criterion counted as met, whatever the vacuous criteria say.
  bool bare() const { return bare_; }
  void probe( const Engine& e, const std::vector< std::string >& sensors, const std::vector< std::string >& motors )
  {
    if ( probed_ )
      return;
    probed_ = true;
    unwired_.clear();
    weak_.clear();
    bare_ = e.structure().populations.empty();
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
    // a sensor wired with a token weight (under the functional floor of every cell it reaches) is wired to nothing
    const Structure st = e.structure();
    for ( const auto& w : sensors )
    {
      bool wired = false;
      for ( const auto& c : e.inputs() )
        if ( c.name == w && !c.neurons.empty() ) wired = true;
      if ( !wired ) continue;
      double floor = 1e9;
      for ( const auto& cs : st.inputs )
        if ( cs.name == w )
          for ( const auto& pid : cs.pops )
            for ( const auto& p : st.populations )
              if ( p.id == pid ) floor = std::min( floor, weight_floor( p.model, true, cs.kind ) );
      const double wt = std::fabs( e.input_weight( w ) );
      if ( floor < 1e9 && floor > 0 && wt > 0.0 && wt < floor ) weak_.push_back( w );
    }
  }
  bool ok() const { return unwired_.empty() && weak_.empty(); }
  /// "sensor 'touch' is not connected to any neuron" etc. (empty when everything is wired). A family of numbered channels ('cue0', 'cue1', ...) is named once
  /// as a family ("the 'cue' sensors"): the message carries no numbers.
  std::string message() const
  {
    std::vector< std::string > items;
    std::vector< std::string > seen_family;
    for ( const auto& u : unwired_ )
    {
      const size_t q = u.find( " '" );
      const std::string kind = u.substr( 0, q ), name = u.substr( q + 2, u.size() - q - 3 );
      size_t e = name.size();
      while ( e > 0 && std::isdigit( static_cast< unsigned char >( name[ e - 1 ] ) ) ) --e;
      if ( e == name.size() || e == 0 )
      {
        items.push_back( u );
        continue;
      }
      const std::string fam = kind + " '" + name.substr( 0, e ) + "'";
      if ( std::find( seen_family.begin(), seen_family.end(), fam ) != seen_family.end() ) continue;
      seen_family.push_back( fam );
      items.push_back( "the '" + name.substr( 0, e ) + "' " + kind + "s" );
    }
    std::string m;
    bool plural = false;
    for ( size_t i = 0; i < items.size(); ++i )
    {
      if ( i ) m += "; ";
      m += items[ i ];
      if ( items[ i ].rfind( "the '", 0 ) == 0 ) plural = true;
    }
    if ( !m.empty() ) m += ( items.size() > 1 || plural ? " are" : " is" ) + std::string( " not connected to any neuron" );
    if ( !weak_.empty() )
    {
      std::string w;
      for ( size_t i = 0; i < weak_.size(); ++i ) w += ( i ? "; sensor '" : "sensor '" ) + weak_[ i ] + "'";
      m += ( m.empty() ? "" : "; " ) + w + ( weak_.size() > 1 ? " are" : " is" ) + " wired with a weight too small to drive any cell";
    }
    return m;
  }

private:
  bool probed_ = false;
  bool bare_ = false;
  std::vector< std::string > unwired_, weak_;
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
