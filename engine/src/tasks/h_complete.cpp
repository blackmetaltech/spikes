// Micro-puzzle "h_complete" (memory chapter, concept "attractor_memory"): a pattern stored in symmetric recurrent excitation is an attractor. Touch a few of its cells and the whole pattern
// lights up, and it stays lit after the cue is gone: the network completes a partial cue and holds it. (Hopfield's content-addressable memory, with spiking cells and a pattern that is a group.)
//
// WORLD (2200 ms; Poisson sensor 'cue', 200 Hz from 300 to 600 ms, weight = the player's). The cells that 'memory' reads are the pattern; the cells 'cue' reaches are the cue. Windows: quiet [100, 300),
//   recall [900, 1700) (long after the cue is gone).
// SCORING
//   quiet     at most 2 spikes in the pattern before the cue                                   the pattern is silent by itself
//   persist   in the recall window at least 80 % of the pattern's cells fire, at 5 Hz or more on average      the activity outlasts the cue
//   complete  at least 80 % of the cells the cue did NOT reach fire in the recall window             the pattern is completed from a part
// MECHANISM (Engine::structure() and Engine::connections(), the edge list between cells): the pattern has several cells; its cells excite each other in a loop; the cue reaches some of them but not all; the
// connections among the cells are roughly symmetric (judged cell by cell, so a single population with per-edge weights is judged like several populations, and a directed random graph is not symmetric).
// Biology: Hopfield JJ (1982) PNAS 79:2554 (a network with symmetric connections has stable states that a partial cue recalls); Amit DJ, Brunel N (1997) Cereb Cortex 7:237 (the same in networks of
// integrate-and-fire neurons: persistent, selective activity). The sustained rate here is far above real delay activity (no balanced inhibition), and the pattern is a group, not a +1 / -1 vector.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_h_complete();
namespace
{
using namespace bestiary;
using namespace tl;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "h_complete";
  i.title = "Recalling a whole from a part";
  i.brief = "A memory can be a state a network falls into. If a group of cells excite each other symmetrically, each cell's weight to another equal to the other's weight back, the group has a stable state "
            "in which all of them fire, and a few cells nudged into it pull the rest after them: a partial cue recalls the whole pattern, and the pattern stays after the cue is gone (Hopfield's "
            "content-addressable memory). Build a pattern of several cells that is silent by itself. 'memory' reads the whole pattern; 'cue' reaches only part of it, for a moment. After the cue the "
            "whole pattern must fire, including the cells the cue never touched, and keep firing. Weights in a Hopfield network are symmetric, and one group of cells can be built from two groups "
            "(the cued cells and the rest) that excite each other and themselves.";
  i.chapter = "memory";
  i.view = "memory";
  i.sensors = { "cue" };
  i.motors = { "memory" };
  i.duration_ms = 2200;
  i.concepts_new = { "attractor_memory" };
  i.concepts_used = { "persistence", "excitation" };
  i.par_neurons = 8;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "cue", in_kind( "poisson", 200, 1500 ) }, { "memory", micro::rate() } };
  i.bands = { { "recalled", "%", 0, 100 } };
  s.gauges = { { "recalled", { "recalled_pct" } } };
  s.cells = { "memory" };
  s.tick = []( Engine& e, double, Meas& m ) { pattern_sets( e, m, "memory", "cue" ); };
  i.recipe = "Two groups of integrate-and-fire cells: C (3 cells, the cued ones) and R (5 cells). cue > C at 1500 pA. Every pair of groups excites each other and itself: C > C, C > R, R > C, R > R, all at +400 pA "
             "(all to all), the same weight in both directions. 'memory' reads C and R. After the cue all eight cells fire for as long as you watch.";
  i.note = { "A partial cue recalls the whole",
    "Hopfield showed that a network of simple units with symmetric connections has stable states, and that the states can be made to be stored patterns: start the network near a pattern, by cueing a part of "
    "it, and it falls into the whole. Such an attractor is a content-addressable memory. In networks of spiking cells the same stable states are persistent, selective activity, which is how delay activity in "
    "cortex has been modelled.",
    "Hopfield JJ (1982) Proc Natl Acad Sci USA 79:2554-2558, doi:10.1073/pnas.79.8.2554; Amit DJ, Brunel N (1997) Cereb Cortex 7:237-252, doi:10.1093/cercor/7.3.237" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 2200;
    c.segs.push_back( { "cue", 300, 600, 200 } );
    c.wins.push_back( { "pre", 100, 300 } );
    c.wins.push_back( { "recall", 900, 1700 } );
    return c;
  };
  s.mech_e = []( const Graph& g, Engine& e ) -> std::string {
    const auto M = g.out_pops( "memory" );
    if ( M.empty() ) return "";
    const std::set< int > mc = out_cells( e, "memory" ), cc = in_cells( e, "cue" );
    if ( mc.size() < 4 ) return "A pattern needs several cells: 'memory' reads too few of them to be a pattern that a part can recall.";
    const CellGraph cg( e );
    if ( !cg.excitatory_loop( mc ) ) return "The cells of the pattern do not excite each other: without recurrent excitation nothing pulls the rest of the pattern along, and the activity dies with the cue.";
    int cued = 0;
    for ( int n : mc ) cued += cc.count( n ) ? 1 : 0;
    if ( cued == 0 ) return "The cue does not reach the pattern: connect 'cue' to some of the cells that 'memory' reads.";
    if ( cued == static_cast< int >( mc.size() ) ) return "The cue reaches every cell of the pattern: it must reach only part of it, or nothing is completed.";
    return asymmetry_cells( cg, mc );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const int nm = m.size_of( "memory" ), nu = m.size_of( "uncued" );
    const double pre = m.n( "memory", "pre" );
    const double fr = nm > 0 ? static_cast< double >( m.active( "memory", 900, 1700 ) ) / nm : 0.0, fu = nu > 0 ? static_cast< double >( m.active_in( "memory", 900, 1700, "uncued" ) ) / nu : 0.0;
    const double hz = m.hz( "memory", "recall" );
    const double need = m.tol > 1.2 ? 0.7 : ( m.tol < 0.9 ? 0.9 : 0.8 );
    p[ "recalled_pct" ] = 100.0 * fr;
    p[ "completed_pct" ] = 100.0 * fu;
    p[ "uncued_cells" ] = nu;
    c.push_back( { "quiet", pre <= 2, 1.0 - ramp( pre, 2, 40 ), "The pattern fires before the cue: it has to be silent until it is cued." } );
    c.push_back( { "persist", fr >= need && hz >= 5.0, 0.5 * ramp( fr, 0, need ) + 0.5 * ramp( hz, 0, 5 ),
      "The pattern does not stay lit after the cue is gone. The cells have to excite each other strongly enough to keep each other firing: recurrent excitation with enough weight." } );
    c.push_back( { "complete", nu > 0 && fu >= need, ramp( fu, 0, need ),
      "The cells the cue never touched do not light up: the cue is remembered but the pattern is not completed. Every cell of the pattern has to be driven by the others, not only the cued ones." } );
  };
  s.ok_verdict = "A partial cue recalls the whole pattern, and the pattern stays lit after the cue: the cells excite each other symmetrically, so the full pattern is a state the network falls into.";
  s.h1 = "A group of cells that excite each other symmetrically has a stable state in which all of them fire. A part of the group, nudged into it, pulls the rest along.";
  s.h2 = "Look at which cells the cue reaches and which it does not: those the cue never touches can only be lit by the other cells. Every group must excite every other group, and itself, with the same strength each way.";
  s.h2_target = "memory";
  s.h3 = "Make two groups, one the cue reaches and one it does not, and give every pair of groups (and each group itself) an excitatory connection of the same weight in both directions. If the pattern dies with the cue, "
         "strengthen those connections; if it fires before the cue, weaken them.";
  s.metric_name = "recalled_pct";
  s.metric_unit = "%";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return m.size_of( "memory" ) > 0 ? 100.0 * m.active( "memory", 900, 1700 ) / m.size_of( "memory" ) : 0.0; };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_h_complete() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( h_complete, 620, "h_complete.json", make_h_complete ) }
