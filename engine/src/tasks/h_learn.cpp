// Task "h_learn" (memory chapter): the pattern is not built in, it is written by plasticity. The recurrent connections of the pattern start weak and plastic (NEST stdp_synapse). A cue first fails to
// recall anything; then the whole pattern is shown for a while, so that its cells fire together and spike-timing-dependent plasticity strengthens the connections among them; then the same partial
// cue recalls the whole pattern. What was learned is the weights, so the weights must have grown, and the recall must come from them.
//
// WORLD (3800 ms; Poisson sensors 'cue' (200 Hz, 200 ms) and 'show' (60 Hz), weights = the player's). Cue at 200 ms, exposure 'show' from 1200 to 2400 ms, cue again at 2900 ms. The cells 'memory' reads are the
//   pattern. Windows: before [700, 1100) and after [3400, 3800) the second cue.
// SCORING
//   naive      before the exposure the cue completes nothing: at most 3 spikes in the cells the cue never reached     the pattern was not there from the start
//   learned    the mean weight of the plastic recurrent connections has at least 1.5 x grown during the episode        plasticity wrote it
//   persist    after the exposure at least 80 % of the pattern's cells fire, at 5 Hz or more on average                recall holds after the cue
//   complete   at least 80 % of the cells the cue never reached fire                                                        and it is a completion
// MECHANISM (Engine::structure()): the pattern's cells excite each other in a loop through plastic (stdp) synapses, the connections are symmetric at the start, 'show' reaches every cell of the pattern, 'cue'
// only some. Nothing else is checked: whether the exposure is strong or long enough is the behaviour.
// Biology: Hopfield JJ (1982) PNAS 79:2554 (the weight rule that stores patterns is Hebbian); Bi G, Poo M (1998) J Neurosci 18:10464 (the timing dependence of potentiation and depression); Song S, Miller KD, Abbott LF
// (2000) Nat Neurosci 3:919 (STDP in models). The exposure here compresses minutes of learning into a second; NEST's stdp_synapse is excitatory only, so the inhibition of a bigger network cannot be learned.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_h_learn();
namespace
{
using namespace bestiary;
using namespace tl;

double plastic_mean( Engine& e, const std::set< int >& mem )
{
  const Structure st = e.structure();
  double sum = 0, n = 0;
  for ( const auto& p : st.projections )
  {
    if ( p.n_edges <= 0 || p.synapse_model != "stdp_synapse" || p.pre_index < 0 || p.post_index < 0 ) continue;
    const auto& a = st.populations[ static_cast< size_t >( p.pre_index ) ];
    const auto& b = st.populations[ static_cast< size_t >( p.post_index ) ];
    if ( !mem.count( a.first_index ) || !mem.count( b.first_index ) ) continue;
    sum += p.w_mean * p.n_edges;
    n += p.n_edges;
  }
  return n > 0 ? sum / n : 0.0;
}

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "h_learn";
  i.title = "Writing a memory with plasticity";
  i.brief = "So far the pattern was built in. In the brain it is written by experience. Make the connections among a group of cells plastic (spike-timing-dependent: a connection grows when its presynaptic "
            "cell fires just before its postsynaptic one) and start them weak. First 'cue' reaches part of the group and recalls nothing. Then 'show' drives the whole pattern for a while, so that its cells "
            "fire together and the connections among them strengthen. Then the same 'cue' must recall the whole pattern, and it must stay lit after the cue. 'memory' reads the pattern. The connections must "
            "really have grown, and they must start symmetric. Too strong at the start and the cue completes the pattern before it was learned; too weak or too slow and nothing is learned. "
            "The synapse rule has a learning rate, an asymmetry between strengthening and weakening, and a ceiling for the weight.";
  i.chapter = "memory";
  i.view = "memory";
  i.sensors = { "cue", "show" };
  i.motors = { "memory" };
  i.duration_ms = 3800;
  i.concepts_new = { "hebbian_storage" };
  i.concepts_used = { "attractor_memory" };
  i.par_neurons = 8;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "cue", in_kind( "poisson", 200, 1500 ) }, { "show", in_kind( "poisson", 60, 1500 ) }, { "memory", micro::rate() } };
  i.bands = { { "recalled", "%", 0, 100 } };
  s.gauges = { { "recalled", { "recalled_pct" } } };
  s.cells = { "memory" };
  s.tick = []( Engine& e, double t, Meas& m ) {
    pattern_sets( e, m, "memory", "cue" );
    if ( !m.extra.count( "w0" ) && t >= 1.0 ) m.extra[ "w0" ] = plastic_mean( e, m.sets[ "memory" ] );
    if ( !m.extra.count( "w1" ) && t >= 3500.0 ) m.extra[ "w1" ] = plastic_mean( e, m.sets[ "memory" ] );
  };
  i.recipe = "Two groups of integrate-and-fire cells C (3 cells, the cued ones) and R (5 cells), no bias. cue > C at 1500 pA, show > C and R at 1500 pA. C > C, C > R, R > C, R > R all to all through stdp_synapse, "
             "weight 80 pA (the same both ways), learning rate (lambda) 0.0015, asymmetry (alpha) 0.5, ceiling (Wmax) 800. 'memory' reads C and R. Before the exposure the cue lights only C; afterwards all eight cells "
             "stay lit. A static weight of 80 recalls nothing, a static weight of 300 recalls from the start.";
  i.note = { "Plasticity writes the pattern",
    "Hopfield's storage rule is Hebbian: connections between cells that are active together are strengthened. Spike-timing-dependent plasticity is a biological form of it: a presynaptic spike just before a "
    "postsynaptic one strengthens the connection, the reverse order weakens it. Show a pattern to a network with such synapses and the connections among its cells grow, until a part of the pattern is enough "
    "to recall the rest.",
    "Hopfield JJ (1982) Proc Natl Acad Sci USA 79:2554-2558, doi:10.1073/pnas.79.8.2554; Bi GQ, Poo MM (1998) J Neurosci 18:10464-10472, doi:10.1523/JNEUROSCI.18-24-10464.1998; Song S, Miller KD, Abbott LF (2000) Nat Neurosci 3:919-926, doi:10.1038/78829" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 3800;
    c.segs.push_back( { "cue", 200, 400, 200 } );
    c.segs.push_back( { "show", 1200, 2400, 60 } );
    c.segs.push_back( { "cue", 2900, 3100, 200 } );
    c.wins.push_back( { "before", 700, 1100 } );
    c.wins.push_back( { "after", 3400, 3800 } );
    return c;
  };
  s.mech_e = []( const Graph& g, Engine& e ) -> std::string {
    const auto M = g.out_pops( "memory" );
    if ( M.empty() ) return "";
    const std::set< int > mc = out_cells( e, "memory" ), cc = in_cells( e, "cue" ), sc = in_cells( e, "show" );
    if ( mc.size() < 4 ) return "A pattern needs several cells: 'memory' reads too few of them.";
    bool plastic = false;
    for ( const auto& p : g.st.projections )
      if ( p.n_edges > 0 && p.synapse_model == "stdp_synapse" && Graph::overlap( M, { p.pre_index } ) && Graph::overlap( M, { p.post_index } ) ) plastic = true;
    if ( !plastic ) return "The connections among the cells of the pattern are fixed: nothing can be learned. Make them plastic (spike-timing-dependent) and start them weak, so that showing the pattern writes it.";
    if ( !mech::excitatory_loop( g.st, mech::PopSet( M.begin(), M.end() ) ) ) return "The cells of the pattern do not excite each other, so there is nothing for the plasticity to strengthen.";
    int cued = 0, shown = 0;
    for ( int n : mc ) { cued += cc.count( n ) ? 1 : 0; shown += sc.count( n ) ? 1 : 0; }
    if ( cued == 0 ) return "The cue does not reach the pattern: connect 'cue' to some of the cells that 'memory' reads.";
    if ( cued == static_cast< int >( mc.size() ) ) return "The cue reaches every cell of the pattern: it must reach only part of it, or nothing is completed.";
    if ( shown < static_cast< int >( mc.size() ) ) return "'show' has to drive every cell of the pattern, so that all of them fire together and the connections among them can strengthen.";
    return asymmetry( g, M );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const int nm = m.size_of( "memory" ), nu = m.size_of( "uncued" );
    const double before = m.active_in( "memory", 700, 1100, "uncued" );
    const double fr = nm > 0 ? static_cast< double >( m.active( "memory", 3400, 3800 ) ) / nm : 0.0, fu = nu > 0 ? static_cast< double >( m.active_in( "memory", 3400, 3800, "uncued" ) ) / nu : 0.0;
    const double hz = m.hz( "memory", "after" );
    const double w0 = m.extra.count( "w0" ) ? m.extra.at( "w0" ) : 0.0, w1 = m.extra.count( "w1" ) ? m.extra.at( "w1" ) : 0.0;
    const double growth = w0 > 0 ? w1 / w0 : 0.0;
    const double need = m.tol > 1.2 ? 0.7 : ( m.tol < 0.9 ? 0.9 : 0.8 );
    p[ "recalled_pct" ] = 100.0 * fr;
    p[ "completed_pct" ] = 100.0 * fu;
    p[ "weight_growth" ] = growth;
    p[ "uncued_active_before" ] = before;
    const double mult = m.tol > 1.2 ? 1.3 : 1.5;
    c.push_back( { "naive", before == 0 && nu > 0, 1.0 - ramp( before, 0, 3 ),
      "The cue already completes the pattern before anything was shown: the connections were strong enough from the start, so nothing was learned. Start them weak and let the exposure strengthen them." } );
    c.push_back( { "learned", growth >= mult, ramp( growth, 1.0, mult ),
      "The connections among the pattern's cells did not grow during the exposure. A connection grows when its presynaptic cell fires just before its postsynaptic one: the cells must fire together while the pattern is shown, "
      "and the learning rate and the asymmetry of the rule must favour strengthening over weakening." } );
    c.push_back( { "persist", fr >= need && hz >= 5.0, 0.5 * ramp( fr, 0, need ) + 0.5 * ramp( hz, 0, 5 ),
      "After the exposure the cue does not leave the pattern lit: what was learned is too weak to hold the pattern (a longer or stronger exposure, a higher learning rate or a higher weight ceiling)." } );
    c.push_back( { "complete", nu > 0 && fu >= need, ramp( fu, 0, need ), "After the exposure the cells the cue never touched do not light up: the pattern is not completed from the part." } );
  };
  s.ok_verdict = "The pattern was learned, not built in: the cue recalled nothing at first, the exposure strengthened the plastic connections among the cells, and afterwards the same partial cue recalls the whole pattern.";
  s.h1 = "Spike-timing-dependent plasticity strengthens connections between cells that fire together, so showing a pattern writes it into the weights.";
  s.h2 = "The connections among the pattern's cells have to be plastic, start weak, and be symmetric. 'show' has to reach every cell, so that the whole pattern fires together while it is shown.";
  s.h2_target = "memory";
  s.h3 = "Use a plastic synapse on every connection among the cells, weak at first, with a small learning rate, an asymmetry that favours strengthening, and a weight ceiling well above the starting weight. "
         "If the cue completes the pattern before the exposure, start weaker or learn slower; if nothing is learned, learn faster or raise the ceiling.";
  s.metric_name = "recalled_pct";
  s.metric_unit = "%";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return m.size_of( "memory" ) > 0 ? 100.0 * m.active( "memory", 3400, 3800 ) / m.size_of( "memory" ) : 0.0; };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_h_learn() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( h_learn, 622, "h_learn.json", make_h_learn ) }
