// Micro-puzzle "h_two" (memory chapter, concept "spurious_states"): two patterns in one network. Each has its own recurrent excitation; between them there is inhibition, symmetric too, so the network
// settles into one pattern or the other and never into a blend of both. A cue for one pattern calls it up, a cue for the other overwrites it, and an ambiguous cue, equal for both, still ends in one
// pattern, not in a mixture (a spurious state).
//
// WORLD (4600 ms; Poisson sensors 'cue_a' and 'cue_b', 300 Hz for 300 ms, weights = the player's): cue_a at 300, cue_b at 1700, both together at 3100 ms. The cells 'a' and 'b' read are the two patterns.
//   Windows: quiet [100, 300), A [900, 1500), B [2300, 2900), mixed cue [3700, 4300).
// SCORING
//   quiet    no more than 2 spikes before the first cue
//   a_on     after cue_a at least 80 % of A's cells fire and B is silent (at most 3 spikes)
//   b_on     after cue_b at least 80 % of B's cells fire and A is silent                       the other pattern overwrites
//   one      after both cues together exactly one pattern is lit (at least 80 % of its cells), the other silent     no blend
// MECHANISM (Engine::connections(), the edge list between the two sets of cells, within one population or across populations): 'a' and 'b' read different cells, each pattern excites itself in a loop, the two inhibit each other (directly or through an interneuron), the connections between
// groups are roughly symmetric, and each cue reaches its own pattern.
// Biology: Hopfield JJ (1982) PNAS 79:2554 (stored patterns are stable states; mixtures of patterns are spurious states that a good network avoids); Amit DJ, Brunel N (1997) Cereb Cortex 7:237. The
// patterns here are two disjoint groups, not overlapping +1 / -1 vectors, and the inhibition between them is built in, not the outcome of an outer-product rule.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_h_two();
namespace
{
using namespace bestiary;
using namespace tl;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "h_two";
  i.title = "Two memories, no blends";
  i.brief = "Store two patterns in one network. Each pattern is a group of cells that excite each other. If nothing separated the groups, the network could fall into a blend of both: a spurious state, "
            "a memory nobody stored. Inhibition between the patterns, as symmetric as the excitation inside them, makes each a state the network can fall into on its own and a blend not one. "
            "'cue_a' calls up pattern A and 'cue_b' pattern B; 'a' and 'b' read the cells of each. After 'cue_a' only A must be lit; after 'cue_b' only B, overwriting A; and when both cues come at "
            "once the network must pick exactly one pattern, not light both. A pattern's cells must stay lit after their cue is gone.";
  i.chapter = "memory";
  i.view = "memory";
  i.sensors = { "cue_a", "cue_b" };
  i.motors = { "a", "b" };
  i.duration_ms = 4600;
  i.concepts_new = { "spurious_states" };
  i.concepts_used = { "attractor_memory", "command_switch" };
  i.par_neurons = 8;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "cue_a", in_kind( "poisson", 300, 1500 ) }, { "cue_b", in_kind( "poisson", 300, 1500 ) }, { "a", micro::rate() }, { "b", micro::rate() } };
  i.bands = { { "lit", "%", 0, 100 } };
  s.gauges = { { "lit", { "lit_pct" } } };
  s.cells = { "a", "b" };
  i.recipe = "Two groups of 4 integrate-and-fire cells, A and B. cue_a > A and cue_b > B at 1500 pA. A > A and B > B at +375 pA (all to all), A > B and B > A at -500 pA (the same both ways). 'a' reads A and 'b' reads B. "
             "Much stronger inhibition (1000) keeps the first pattern for good: a new cue cannot overwrite it.";
  i.note = { "Spurious states",
    "A network that stores several patterns can fall into states that were never stored: mixtures of the patterns. Hopfield's original network has them, and so do its spiking cousins. Competition "
    "between the groups, symmetric like the rest of the weights, makes the pure patterns the states that win: the network chooses one and silences the other.",
    "Hopfield JJ (1982) Proc Natl Acad Sci USA 79:2554-2558, doi:10.1073/pnas.79.8.2554" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 4600;
    c.segs.push_back( { "cue_a", 300, 600, 300 } );
    c.segs.push_back( { "cue_b", 1700, 2000, 300 } );
    c.segs.push_back( { "cue_a", 3100, 3400, 300 } );
    c.segs.push_back( { "cue_b", 3100, 3400, 300 } );
    c.wins.push_back( { "pre", 100, 300 } );
    c.wins.push_back( { "wa", 900, 1500 } );
    c.wins.push_back( { "wb", 2300, 2900 } );
    c.wins.push_back( { "wm", 3700, 4300 } );
    return c;
  };
  s.mech_e = []( const Graph& g, Engine& e ) -> std::string {
    const auto A = g.out_pops( "a" ), B = g.out_pops( "b" );
    if ( A.empty() || B.empty() ) return "";
    const auto ca = out_cells( e, "a" ), cb = out_cells( e, "b" );
    for ( int n : ca )
      if ( cb.count( n ) ) return "'a' and 'b' read some of the same cells: the two patterns need cells of their own to tell them apart.";
    const auto ia = in_cells( e, "cue_a" ), ib = in_cells( e, "cue_b" );
    auto touches = []( const std::set< int >& cue, const std::set< int >& pat ) { for ( int n : pat ) if ( cue.count( n ) ) return true; return false; };
    if ( !touches( ia, ca ) || !touches( ib, cb ) ) return "Each cue has to reach its own pattern: 'cue_a' the cells that 'a' reads and 'cue_b' the cells that 'b' reads.";
    const CellGraph cg( e );
    if ( !cg.excitatory_loop( ca ) || !cg.excitatory_loop( cb ) )
      return "A pattern whose cells do not excite each other cannot stay lit after its cue: give each pattern recurrent excitation.";
    if ( !cg.inhibits( ca, cb ) || !cg.inhibits( cb, ca ) )
      return "Nothing separates the two patterns: without inhibition between them the network can fall into a blend of both. Each pattern has to inhibit the other.";
    std::set< int > all = ca;
    all.insert( cb.begin(), cb.end() );
    return asymmetry_cells( cg, all );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const int na = m.size_of( "a" ), nb = m.size_of( "b" );
    auto frac = [ & ]( const char* ch, double t0, double t1, int n ) { return n > 0 ? static_cast< double >( m.active( ch, t0, t1 ) ) / n : 0.0; };
    const double need = m.tol > 1.2 ? 0.7 : ( m.tol < 0.9 ? 0.9 : 0.8 );
    const double pre = m.n( "a", "pre" ) + m.n( "b", "pre" );
    const double a1 = frac( "a", 900, 1500, na ), b1 = m.n( "b", "wa" ), b2 = frac( "b", 2300, 2900, nb ), a2 = m.n( "a", "wb" );
    const double am = frac( "a", 3700, 4300, na ), bm = frac( "b", 3700, 4300, nb );
    const bool one = ( am >= need && m.n( "b", "wm" ) <= 3 ) || ( bm >= need && m.n( "a", "wm" ) <= 3 );
    p[ "lit_pct" ] = 100.0 * std::max( a1, b2 );
    p[ "a_after_cue_a_pct" ] = 100.0 * a1;
    p[ "b_after_cue_b_pct" ] = 100.0 * b2;
    p[ "a_cells_mixed" ] = am * na;
    p[ "b_cells_mixed" ] = bm * nb;
    c.push_back( { "quiet", pre <= 2, 1.0 - ramp( pre, 2, 40 ), "A pattern fires before any cue: both must be silent until they are cued." } );
    c.push_back( { "a_on", a1 >= need && b1 <= 3, 0.5 * ramp( a1, 0, need ) + 0.5 * ( 1.0 - ramp( b1, 3, 60 ) ),
      "'cue_a' does not leave pattern A lit and B silent: A has to keep itself on after the cue and hold B down." } );
    c.push_back( { "b_on", b2 >= need && a2 <= 3, 0.5 * ramp( b2, 0, need ) + 0.5 * ( 1.0 - ramp( a2, 3, 60 ) ),
      "'cue_b' does not overwrite pattern A with B: either B does not light up, or A is held on so strongly that a cue for B cannot silence it. The inhibition from B has to be able to beat A's self-excitation." } );
    c.push_back( { "one", one, ( am >= need || bm >= need ) ? 0.6 : 0.3,
      "With both cues at once the network lights both patterns or neither: a blend, a spurious state. The network has to settle into exactly one pattern." } );
  };
  s.ok_verdict = "Two stored patterns without blends: each cue calls up its own pattern and silences the other, and an ambiguous cue ends in exactly one pattern.";
  s.h1 = "With symmetric excitation inside each pattern and symmetric inhibition between them, the pure patterns are the states the network falls into; a blend of both is not.";
  s.h2 = "Each pattern needs recurrent excitation to stay lit, and the two patterns need inhibition between them, the same strength each way. Look at what happens when both cues come together.";
  s.h2_target = "a";
  s.h3 = "Two groups with excitation inside each and inhibition between them, the same weight in both directions. If a pattern never gives way to the other cue, weaken the inhibition between the groups or "
         "the excitation inside them; if both stay lit together, strengthen the inhibition.";
  s.metric_name = "lit_pct";
  s.metric_unit = "%";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return m.size_of( "a" ) > 0 ? 100.0 * m.active( "a", 900, 1500 ) / m.size_of( "a" ) : 0.0; };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_h_two()
{
  Spec s = spec();
  s.tick = []( Engine& e, double, Meas& m ) {
    if ( m.sets.count( "a" ) ) return;
    m.sets[ "a" ] = out_cells( e, "a" );
    m.sets[ "b" ] = out_cells( e, "b" );
  };
  return std::make_unique< Scripted >( std::move( s ) );
}

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( h_two, 621, "h_two.json", make_h_two ) }
