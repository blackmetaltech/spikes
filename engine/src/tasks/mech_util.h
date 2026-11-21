// Mechanism checks for the heart / breathing / autonomic tasks (docs/REQ-0.4.md, "honesty contract" rule 1).
// A task reads the circuit through Engine::structure() (docs/ENGINE-STRUCTURE.md) and asks name-independent questions: which populations
// does this input channel feed, which does this output read, is there a path from one to the other with a given net sign.
// Nothing here knows population ids: the player names the groups.
//
// Net sign of a path: every projection with weights (n_edges > 0, sign != 0) is an edge; a path is net excitatory when it holds an even number
// of inhibitory projections, net inhibitory when it holds an odd number (disinhibition counts as excitation). Zero hops are allowed when a
// population is both a start and a target (a sensor and a motor channel on the same group).
#pragma once
#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <vector>
#include "spike/engine.h"
#include "task_util.h"

namespace spike::tasks::mech
{

using PopSet = std::set< int >;

inline PopSet channel_pops( const Structure& st, const std::vector< ChannelStructure >& list, const std::string& name )
{
  PopSet r;
  for ( const auto& c : list )
    if ( c.name == name )
      for ( const auto& id : c.pops )
      {
        int k = st.population_index( id );
        if ( k >= 0 )
          r.insert( k );
      }
  return r;
}
/// Populations the input channel drives / the output channel reads.
inline PopSet in_pops( const Structure& st, const std::string& ch ) { return channel_pops( st, st.inputs, ch ); }
inline PopSet out_pops( const Structure& st, const std::string& ch ) { return channel_pops( st, st.outputs, ch ); }
inline PopSet unite( PopSet a, const PopSet& b )
{
  a.insert( b.begin(), b.end() );
  return a;
}
inline bool overlap( const PopSet& a, const PopSet& b )
{
  for ( int x : a )
    if ( b.count( x ) )
      return true;
  return false;
}

/// Populations reached from `from` along projections; `parity` +1 = by a net-excitatory path, -1 = net-inhibitory, 0 = any.
/// `from` itself counts as reached (zero hops) for parity +1 / 0. max_hops <= 0 = unlimited. edge_sign +1 / -1 uses only excitatory / inhibitory projections.
inline PopSet reach( const Structure& st, const PopSet& from, int parity = 0, int max_hops = 0, int edge_sign = 0 )
{
  const int n = static_cast< int >( st.populations.size() );
  std::vector< std::array< bool, 2 > > seen( n, { false, false } );  // [pop][0] even #inhibitory, [1] odd
  std::vector< std::pair< int, int > > frontier;
  for ( int p : from )
    if ( p >= 0 and p < n )
    {
      seen[ p ][ 0 ] = true;
      frontier.push_back( { p, 0 } );
    }
  for ( int hop = 0; ( max_hops <= 0 or hop < max_hops ) and not frontier.empty(); ++hop )
  {
    std::vector< std::pair< int, int > > next;
    for ( const auto& [ p, par ] : frontier )
      for ( const auto& pr : st.projections )
      {
        if ( not functional( st, pr ) or pr.pre_index != p or pr.post_index >= n or ( edge_sign != 0 and pr.sign != edge_sign ) )
          continue;
        const int np = par ^ ( pr.sign < 0 ? 1 : 0 );
        if ( not seen[ pr.post_index ][ np ] )
        {
          seen[ pr.post_index ][ np ] = true;
          next.push_back( { pr.post_index, np } );
        }
      }
    frontier.swap( next );
  }
  PopSet r;
  for ( int p = 0; p < n; ++p )
    if ( parity == 0 ? ( seen[ p ][ 0 ] or seen[ p ][ 1 ] ) : ( parity > 0 ? seen[ p ][ 0 ] : seen[ p ][ 1 ] ) )
      r.insert( p );
  return r;
}
/// True when some population of `to` is reached from some population of `from`.
inline bool path( const Structure& st, const PopSet& from, const PopSet& to, int parity = 0, int max_hops = 0 )
{
  if ( from.empty() or to.empty() )
    return false;
  return overlap( reach( st, from, parity, max_hops ), to );
}

/// Recurrent excitation: some population of `pops` lies on a cycle of excitatory projections. By default a cycle needs at least two cells (a group
/// projecting to itself needs >= 2 cells and a rule other than one_to_one, which would only be autapses; a loop through other groups counts as it is).
/// `autapse_ok`: a cell that excites ITSELF (a self-loop, one cell or one_to_one) is a latch too, as in a real command neuron, and counts.
inline bool excitatory_loop( const Structure& st, const PopSet& pops, bool autapse_ok = false )
{
  for ( int p : pops )
  {
    PopSet first;
    for ( const auto& pr : st.projections )
      if ( functional( st, pr ) and pr.sign > 0 and pr.pre_index == p )
      {
        if ( not autapse_ok and pr.post_index == p and ( st.populations[ p ].n < 2 or pr.rule == "one_to_one" ) )
          continue;
        first.insert( pr.post_index );
      }
    if ( first.count( p ) or overlap( reach( st, first, 0, 0, +1 ), { p } ) )
      return true;
  }
  return false;
}

/// Caches a task's mechanism verdict: the structure is read at the first step and again every `every_ms` of model time (a lesion applied with
/// set_weight() during a run is seen within that time). `missing()` is the empty string while the mechanism is complete (or not yet checked).
class Gate
{
public:
  void invalidate()
  {
    missing_.clear();
    next_ = 0;
  }
  template < class F >
  void refresh( const Engine& e, double t_ms, F&& check, double every_ms = 500 )
  {
    if ( t_ms + 1e-9 < next_ )
      return;
    next_ = t_ms + every_ms;
    missing_ = check( e.structure() );
  }
  const std::string& missing() const { return missing_; }
  bool ok() const { return missing_.empty(); }

private:
  std::string missing_;
  double next_ = 0;
};

/// The baroreflex: baroreceptor -> relay -> {vagal limb, inhibitory sympathetic limb}; "" when complete, else a plain sentence naming what is missing.
inline std::string baroreflex_limbs_missing( const Structure& st )
{
  auto kind = [ & ]( const char* ch ) {
    for ( const auto& o : st.outputs )
      if ( o.name == ch )
        return o.kind;
    return std::string();
  };
  const auto vag = mech::out_pops( st, "vagal" ), sym = mech::out_pops( st, "sympathetic" ), baro = mech::in_pops( st, "baroreceptor" );
  if ( baro.empty() )
    return "The baroreceptor input does not reach any cell: the reflex has no afferent limb. Connect it to a relay group in the brainstem.";
  if ( vag.empty() || sym.empty() || kind( "vagal" ) != "modulator" || kind( "sympathetic" ) != "modulator" )
    return "The reflex needs a vagal (ACh) and a sympathetic (NE) modulator output, each read from its own ganglion cells.";
  if ( mech::overlap( vag, sym ) )
    return "The vagal and the sympathetic output read the same cells: the two limbs of the reflex need separate cells.";
  // a relay the afferent reaches that feeds both limbs
  const auto relays = mech::reach( st, baro, +1 );
  bool vagal_limb = false, sym_limb = false;
  for ( int r : relays )
  {
    const bool v = mech::path( st, { r }, vag, +1 ), y = mech::path( st, { r }, sym, -1 );
    vagal_limb = vagal_limb || v;
    sym_limb = sym_limb || y;
    if ( v && y )
      return "";
  }
  if ( !vagal_limb && !sym_limb )
    return "Neither limb of the baroreflex is wired: the baroreceptor signal must be relayed to the vagal output (excitation) and, through an inhibitory relay, to the sympathetic output.";
  if ( !vagal_limb )
    return "The vagal limb is missing: nothing carries the baroreceptor signal (relay, excitatory) to the cells behind the vagal output, so high pressure cannot slow the heart.";
  if ( !sym_limb )
    return "The sympathetic limb is missing: the baroreceptor signal must reach the sympathetic output through an inhibitory relay (CVLM inhibiting the tonic vasomotor cells), so that high pressure turns sympathetic drive DOWN.";
  return "The two limbs start from different relays: one relay population that the baroreceptor input reaches (the NTS) must feed both the vagal and the sympathetic limb.";
}


/// A score below the pass bar for a circuit that lacks the mechanism the level teaches.
constexpr double kNoMechanismCap = 49.0;

}  // namespace spike::tasks::mech
