// Mechanism checks for the spinal and micro-b levels (docs/REQ-0.4.md, honesty contract rule 1): "does the circuit use the mechanism the level teaches?",
// answered from Engine::structure() (populations, projections with live weights and signs, channels), never from the output alone.
//
// A task cannot call the engine from score() (it has no Engine there), so it captures a Graph in its first step() (before any slice has run, so a
// weight lesion applied after loading is seen) and asks it questions from then on. Everything here works on population indices of that snapshot.
//
// Sign of a path: the product of the signs of its projections (an excitatory chain is +1, one inhibitory projection makes it -1, two make it +1 again).
// A projection counts only when it has connections and a non-zero mean weight (a weight-0 projection is "cut"), exactly as Engine::has_path.
#pragma once
#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "spike/engine.h"

namespace spike::tasks::mechb
{

using PopSet = std::set< int >;

/// One mechanism criterion of a level (the spinal tasks list them; the micro-puzzles use their own Crit with the same fields).
/// `fail` is the verdict when !ok (words only, no numbers); `place` is hint tier 2 (where to look); `target` the id the lab highlights; `direction` is hint tier 3.
struct Issue
{
  std::string key;
  bool ok = true;
  std::string fail, place, target, direction;
};
inline Issue issue( const std::string& key, bool ok, const std::string& fail, const std::string& place, const std::string& target, const std::string& direction )
{
  Issue i;
  i.key = key;
  i.ok = ok;
  i.fail = fail;
  i.place = place;
  i.target = target;
  i.direction = direction;
  return i;
}

class Graph
{
public:
  void clear()
  {
    st_ = Structure();
    valid_ = false;
  }
  void capture( const Engine& e )
  {
    st_ = e.structure();
    valid_ = !st_.populations.empty();
  }
  bool valid() const { return valid_; }
  const Structure& st() const { return st_; }

  /// Populations an input channel is connected to (empty when it is connected to nothing or unknown).
  PopSet in_pops( const std::string& ch ) const { return chan_pops( st_.inputs, ch ); }
  /// Populations an output channel reads.
  PopSet out_pops( const std::string& ch ) const { return chan_pops( st_.outputs, ch ); }
  PopSet out_pops( const std::vector< std::string >& chs ) const
  {
    PopSet s;
    for ( const auto& c : chs )
    {
      auto p = out_pops( c );
      s.insert( p.begin(), p.end() );
    }
    return s;
  }
  PopSet in_pops( const std::vector< std::string >& chs ) const
  {
    PopSet s;
    for ( const auto& c : chs )
    {
      auto p = in_pops( c );
      s.insert( p.begin(), p.end() );
    }
    return s;
  }
  const PopulationInfo& pop( int i ) const { return st_.populations[ static_cast< size_t >( i ) ]; }
  int n_pops() const { return static_cast< int >( st_.populations.size() ); }

  /// Fewest projections in a path from any population of `from` to any of `to` whose sign is `sign` (+1 or -1; 0 = any sign), at most max_hops of them
  /// (a path of 0 projections exists when the sets overlap, sign +1 or 0). Populations of `avoid` may be neither the start nor an intermediate stop of the
  /// path (e.g. "an inhibitory interneuron that is not itself a motor pool"). -1 when there is none.
  int hops( const PopSet& from, const PopSet& to, int sign = 0, int max_hops = 6, const PopSet& avoid = {}, bool only_excitatory = false ) const
  {
    if ( !valid_ || from.empty() || to.empty() )
      return -1;
    const int n = n_pops();
    // state = (population, parity of negative projections so far); BFS by number of projections
    std::vector< int > dist( static_cast< size_t >( n ) * 2, -1 );
    std::deque< std::pair< int, int > > q;
    for ( int p : from )
      if ( p >= 0 && p < n && !avoid.count( p ) )
      {
        dist[ static_cast< size_t >( p ) * 2 ] = 0;
        q.push_back( { p, 0 } );
      }
    int best = -1;
    auto arrived = [ & ]( int p, int par, int d ) {
      if ( !to.count( p ) )
        return;
      if ( sign == 0 || ( sign > 0 && par == 0 ) || ( sign < 0 && par == 1 ) )
        if ( best < 0 || d < best )
          best = d;
    };
    for ( auto& s : q )
      arrived( s.first, s.second, 0 );
    while ( !q.empty() )
    {
      auto [ p, par ] = q.front();
      q.pop_front();
      const int d = dist[ static_cast< size_t >( p ) * 2 + static_cast< size_t >( par ) ];
      if ( d >= max_hops || ( avoid.count( p ) ) )
        continue;
      for ( const auto& pr : st_.projections )
      {
        if ( pr.pre_index != p || pr.n_edges <= 0 || pr.sign == 0 || pr.post_index < 0 || ( only_excitatory && pr.sign < 0 ) )
          continue;
        const int np = pr.post_index, npar = par ^ ( pr.sign < 0 ? 1 : 0 );
        auto& dd = dist[ static_cast< size_t >( np ) * 2 + static_cast< size_t >( npar ) ];
        if ( dd >= 0 )
          continue;
        dd = d + 1;
        arrived( np, npar, d + 1 );
        q.push_back( { np, npar } );
      }
    }
    return best;
  }
  bool path( const PopSet& from, const PopSet& to, int sign = 0, int max_hops = 6, const PopSet& avoid = {} ) const
  {
    return hops( from, to, sign, max_hops, avoid ) >= 0;
  }
  /// Fewest projections in a chain of EXCITATORY projections only (a double inhibition, i.e. disinhibition, does not count as excitation); 0 hops when the sets overlap.
  int exc_hops( const PopSet& from, const PopSet& to, int max_hops = 6, const PopSet& avoid = {} ) const { return hops( from, to, +1, max_hops, avoid, true ); }
  bool exc( const PopSet& from, const PopSet& to, int max_hops = 6, const PopSet& avoid = {} ) const { return exc_hops( from, to, max_hops, avoid ) >= 0; }
  /// Is there a single projection pre -> post (population sets) of the given sign (0 = any)?
  bool edge( const PopSet& pre, const PopSet& post, int sign = 0 ) const
  {
    for ( const auto& pr : st_.projections )
      if ( pr.n_edges > 0 && pr.sign != 0 && pre.count( pr.pre_index ) && post.count( pr.post_index ) && ( sign == 0 || pr.sign == sign ) )
        return true;
    return false;
  }

  /// Populations X (not in `exclude`) with an edge of sign `sign` from `from` into X and an edge of sign `sign2` from X into `to`.
  PopSet relays( const PopSet& from, const PopSet& to, int sign, int sign2, const PopSet& exclude = {} ) const
  {
    PopSet r;
    for ( int x = 0; x < n_pops(); ++x )
      if ( !exclude.count( x ) && edge( from, PopSet{ x }, sign ) && edge( PopSet{ x }, to, sign2 ) )
        r.insert( x );
    return r;
  }

  /// A cycle a -> x -> a with a projection of sign +1 a -> x and of sign -1 x -> a, x not in `exclude` and x != a (the Renshaw loop). Returns the x's.
  PopSet ei_loop( int a, const PopSet& exclude = {} ) const
  {
    PopSet ex = exclude;
    ex.insert( a );
    return relays( PopSet{ a }, PopSet{ a }, +1, -1, ex );
  }

  /// Populations whose model can burst on its own (the part has an intrinsic burst mechanism: an Izhikevich cell, an adaptive exponential
  /// cell, a Hodgkin-Huxley cell). Integrate-and-fire cells cannot.
  static bool can_burst( const std::string& model )
  {
    static const char* const ok[] = { "izhikevich", "aeif_cond_alpha", "aeif_cond_exp", "aeif_psc_alpha", "aeif_psc_exp", "aeif_cond_beta_multisynapse",
                                      "hh_psc_alpha", "hh_cond_exp_traub", "hh_cond_beta_gap_traub", "gif_psc_exp", "gif_cond_exp", "pp_psc_delta" };
    for ( const char* m : ok )
      if ( model == m )
        return true;
    return false;
  }

  /// Fingerprint of what sets a population's firing threshold: model, numeric parameters, and the mean weights of its incoming projections from
  /// other populations (a relay with a different weight is a different threshold too). Two pools with the same fingerprint switch on together.
  std::string threshold_signature( int p ) const
  {
    std::string s = pop( p ).model;
    for ( const auto& [ k, v ] : pop( p ).params )
      if ( !( k == "I_e" && v == 0.0 ) )  // a bias of zero is the default: a cell with I_e 0 and one without the entry are the same cell
        s += "|" + k + "=" + std::to_string( v );
    std::vector< std::string > w;
    for ( const auto& pr : st_.projections )
      if ( pr.post_index == p && pr.n_edges > 0 && pr.sign != 0 )
        w.push_back( pr.pre_pop + ":" + std::to_string( pr.w_mean ) );
    std::sort( w.begin(), w.end() );
    for ( const auto& x : w )
      s += "<" + x;
    return s;
  }

private:
  PopSet chan_pops( const std::vector< ChannelStructure >& v, const std::string& ch ) const
  {
    PopSet s;
    for ( const auto& c : v )
      if ( c.name == ch )
        for ( const auto& id : c.pops )
        {
          const int i = st_.population_index( id );
          if ( i >= 0 )
            s.insert( i );
        }
    return s;
  }
  Structure st_;
  bool valid_ = false;
};


// ---- the limb rig's mechanisms (stretch_reflex, renshaw, posture, reach, boss_spinal_sprint): sensors "ag_Ia" ..., motors "ag_motor", "ant_motor", "gamma_motor" ----

/// Monosynaptic stretch reflex: each muscle's Ia afferent group excites its own alpha pool through at most one projection (a path of one synapse),
/// and reciprocal Ia inhibition: the Ia afferent of one muscle reaches the OTHER muscle's alpha pool through an inhibitory interneuron (a cell that is neither pool).
inline void limb_stretch_reflex( const Graph& g, std::vector< Issue >& out )
{
  const auto ia_f = g.in_pops( "ag_Ia" ), ia_e = g.in_pops( "ant_Ia" ), mn_f = g.out_pops( "ag_motor" ), mn_e = g.out_pops( "ant_motor" );
  PopSet pools = mn_f;
  pools.insert( mn_e.begin(), mn_e.end() );
  const bool f = g.exc_hops( ia_f, mn_f, 1 ) >= 0, e = g.exc_hops( ia_e, mn_e, 1 ) >= 0;
  out.push_back( issue( "monosynaptic", f && e,
                        std::string( "No monosynaptic stretch reflex: the Ia afferents of " ) + ( !f && !e ? "both muscles are" : ( !f ? "the flexor is" : "the extensor is" ) ) +
                        " not connected to " + ( !f && !e ? "their" : "its" ) + " own motoneuron pool by a single excitatory synapse (a relay chain adds delay and is not the reflex).",
                        "The path from each Ia afferent group onto its own motoneuron pool: it should be one excitatory synapse.", f ? "ant_Ia" : "ag_Ia",
                        "Connect each Ia afferent group directly onto the motoneuron pool of the same muscle with an excitatory projection." ) );
  const bool rf = g.hops( ia_f, mn_e, -1, 4, pools ) >= 1, re = g.hops( ia_e, mn_f, -1, 4, pools ) >= 1;
  out.push_back( issue( "reciprocal_inhibition", rf && re,
                        std::string( "No reciprocal inhibition: the Ia afferent of " ) + ( !rf && !re ? "either muscle" : ( !rf ? "the flexor" : "the extensor" ) ) +
                        " does not reach the opposing motoneuron pool through an inhibitory interneuron, so the two muscles can fight.",
                        "Between each Ia afferent group and the opposite pool there should be an interneuron set to inhibitory.", rf ? "ant_Ia" : "ag_Ia",
                        "Let each Ia afferent group excite an inhibitory interneuron group that projects onto the opposite motoneuron pool." ) );
}

/// Recurrent (Renshaw) inhibition: a motor pool excites an inhibitory cell group that inhibits it back (a loop through a cell that is not the pool).
inline void limb_renshaw( const Graph& g, std::vector< Issue >& out )
{
  bool loop = false;
  for ( const char* ch : { "ag_motor", "ant_motor" } )
    for ( int a : g.out_pops( ch ) )
      loop = loop || !g.ei_loop( a ).empty();
  out.push_back( issue( "renshaw_loop", loop,
                        "No recurrent inhibition: no motoneuron pool excites an inhibitory cell group that inhibits it back, so nothing turns the pool's own output down.",
                        "A loop motor pool -> inhibitory cells -> the same motor pool is missing.", "ag_motor",
                        "Add inhibitory (Renshaw) cells: each motoneuron pool excites them, and they project back onto the same pool." ) );
}

/// Spindle feedback: an afferent (Ia or II) of a muscle drives that muscle's alpha pool, closing the loop through the limb; and fusimotor drive: the tonic
/// 'drive' reaches the gamma pool.
inline void limb_spindle( const Graph& g, std::vector< Issue >& out, bool gamma = true )
{
  bool fb = false;
  for ( const char* a : { "ag_Ia", "ag_II" } )
    fb = fb || g.exc( g.in_pops( a ), g.out_pops( "ag_motor" ), 3 );
  for ( const char* a : { "ant_Ia", "ant_II" } )
    fb = fb || g.exc( g.in_pops( a ), g.out_pops( "ant_motor" ), 3 );
  out.push_back( issue( "spindle_feedback", fb,
                        "No spindle feedback: no muscle's spindle afferent drives its own motoneuron pool, so the limb has no servo and nothing pushes back when it is displaced.",
                        "The loop through the limb: a spindle afferent (Ia or II) must reach the motor pool of its own muscle.", "ag_Ia",
                        "Connect the spindle afferent groups onto the motoneuron pool of the same muscle with excitatory projections." ) );
  if ( gamma )
    out.push_back( issue( "fusimotor_drive", g.exc( g.in_pops( "drive" ), g.out_pops( "gamma_motor" ), 4 ),
                          "No fusimotor drive: the gamma pool is not driven by the descending tone, so the spindles stay slack and the loop cannot listen.",
                          "The gamma pool: the tonic 'drive' must reach the cells read out on gamma_motor.", "gamma_motor",
                          "Send the tonic drive (through a tonic group) onto the gamma motoneuron group." ) );
}

/// Triphasic burst: 'go' excites the agonist pool (the first burst) AND the antagonist pool through a longer chain of groups (the delayed braking burst).
inline void limb_burst( const Graph& g, std::vector< Issue >& out )
{
  const auto go = g.in_pops( "go" );
  const int ha = g.exc_hops( go, g.out_pops( "ag_motor" ), 6 ), hn = g.exc_hops( go, g.out_pops( "ant_motor" ), 6 );
  out.push_back( issue( "agonist_burst", ha >= 0, "No agonist burst: 'go' does not reach the flexor pool through an excitatory path, so nothing accelerates the limb.",
                        "The first burst: 'go' must drive a burst group that excites the agonist (flexor) pool.", "go", "Connect 'go' to a burst group and that group onto the flexor pool." ) );
  out.push_back( issue( "antagonist_burst", hn >= 0 && ha >= 0 && hn > ha,
                        hn < 0 ? "No antagonist burst: nothing excites the extensor pool after the go, so nothing brakes the movement."
                               : "The antagonist burst is not delayed: it must come through a longer chain of groups than the agonist burst, or it fires together with it.",
                        "The braking burst: a later group in the chain should excite the antagonist (extensor) pool.", "ant_motor",
                        "Chain a second burst group behind the first, with a delay on the link, and let it excite the extensor pool." ) );
}

}  // namespace spike::tasks::mechb
