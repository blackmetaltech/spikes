// Task "fly_mb" (fly mushroom body chapter): learning to avoid an odour. A fly smells two odours; odour A is paired with punishment; afterwards its avoidance output neuron answers odour A less than odour B.
// The circuit is a model of the fly mushroom body after the published cell types (projection neurons, Kenyon cells, the APL, MBONs, DANs), not synapse-level data: random sparse PN > KC wiring (four inputs per KC),
// a sparse code held by feedback inhibition (KC > APL > KC), and a dopamine-gated depression of the KC > MBON synapse (NEST stdp_dopamine_synapse, A_plus < 0, A_minus > 0: see k_depress.cpp for why it is enough).
//
// WORLD (4400 ms; the protocol of bestiary_b.h: sensors pn0..pn19 (glomeruli; odour A = {1, 4, 7, 9, 12, 16}, odour B = {0, 3, 7, 10, 13, 18}, 150 Hz) and punish (100 Hz); read 'avoid' (MBONs, as a rate) and 'kc' (the cells that fire)
//   base [0, 400) quiet; naive: odour A [500, 900), odour B [1100, 1500); pairings of odour A with punishment [1900, 2300), [2500, 2900); trained: odour A [3300, 3700), odour B [3900, 4300)
// SCORING  answers (the MBONs answer both odours before training), learns (response to A <= 65 % of the naive one, guided 80, researcher 60), specific (response to B >= 55 % of the naive one, guided 45, researcher 60),
//   sparse (while odour A is naive, between 3 cells and a quarter of the Kenyon cells fire, guided 40 %), separate (the cells of A and B overlap by at most half).
// MECHANISM (Engine::structure()): the glomeruli reach a large group of Kenyon cells by sparse wiring; the Kenyon cells excite an inhibitory group that inhibits them; the Kenyon cells reach the avoidance cells through a
// dopamine-gated synapse whose dopamine source is a group the punishment sensor drives.
// Biology: Aso et al. 2014 eLife 3:e04577, 3:e04580; Hige et al. 2015 Neuron 88:985; Papadopoulou et al. 2011 Science 332:721 (APL analogue in the locust). Compressed in time (a pairing of 400 ms); odour patterns, cell counts and
// weights are game values; the sign of the plasticity and its odour specificity are from the papers.
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_fly_mb();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "fly_mb";
  i.title = "Fly: the mushroom body learns to avoid";
  i.brief = "A fly smells odour A, and gets punished; later it avoids odour A but not odour B. The mushroom body does this with three parts you have met: projection neurons carry the odour as a pattern on twenty glomeruli (sensors pn0 to pn19); "
            "Kenyon cells, each listening to four random projection neurons, turn it into a sparse code, kept sparse by a giant inhibitory neuron (APL); and the Kenyon cells excite output neurons (MBONs) through a synapse "
            "that dopamine weakens, but only if the Kenyon cell was active. The wiring is a model after the fly's cell types (after the Aso, Hige and Papadopoulou papers), not synapse-level data. Build it: twenty projection neurons "
            "driven by the sensors, two hundred Kenyon cells with random sparse wiring, the APL loop, a small group of avoidance output neurons read on 'avoid', and the Kenyon cells read on 'kc'. The "
            "sensor 'punish' makes a punishment cell fire, which drives the dopamine neurons; the dopamine neurons are the dopamine source of the Kenyon cell to MBON synapse. You are scored on the "
            "protocol: both odours make the avoidance cells fire; odour A is then paired with punishment twice; afterwards the avoidance cells must answer odour A much less than before and odour B almost as before. "
            "Along the way the Kenyon-cell code must be sparse and the codes of the two odours must mostly differ. Without the APL the code is dense and the odours overlap; without dopamine nothing is learned.";
  i.chapter = "fly_mb";
  i.sensors = glomerulus_family();
  i.sensors.push_back( "punish" );
  i.motors = { "avoid", "kc" };
  i.duration_ms = kMbEnd;
  i.concepts_new = {};
  i.concepts_used = { "sparse_coding", "dopamine_gated_depression", "negative_feedback", "inhibition" };
  i.par_neurons = 20 + 200 + 4 + 4 + 1 + 2;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "avoid", micro::rate() }, { "kc", micro::rate() }, { "punish", in_kind( "poisson", 100, 1000 ) } };
  for ( const auto& c : glomerulus_family() ) i.kinds[ c ] = glomerulus_kind();
  i.bands = { { "avoid_hz", "Hz", 0, 400 } };
  i.recipe = "Two hundred and thirty-one integrate-and-fire cells: PN (20), KC (200), APL (4), MBON (4; the reference gives them a bias of 350 pA, which is not required), TSN (1), DAN (2). pn0 to pn19 > PN, sensor k to cell k, and punish > TSN, at the default 1000 pA. PN > KC with random sparse "
             "wiring, 4 inputs per Kenyon cell, +230 pA; KC > APL +2500 pA; APL > KC -5000 pA; TSN > DAN +1000 pA. KC > MBON with a dopamine-gated synapse (stdp_dopamine_synapse): weight 1500 pA, Wmax 4500 pA, A_plus -1000, "
             "A_minus +1000, tau c 30 ms, tau n 200 ms, dopamine source DAN. MBON > 'avoid'; KC > 'kc'.";
  i.note = { "How a fly learns to avoid an odour",
    "In the mushroom body of Drosophila an odour is a sparse pattern over about two thousand Kenyon cells. Their axons run through compartments, each with its own output neuron and its own dopamine neurons. Pairing an odour with "
    "punishment activates the dopamine neurons of an avoidance compartment, which depresses the synapses of the active Kenyon cells onto the output neuron: the odour now drives that output neuron less. Because the code is sparse, an "
    "odour that shares few cells with the punished one is hardly affected; the more two odours overlap, the more the memory generalises between them.",
    "Aso Y et al. (2014) eLife 3:e04577, doi:10.7554/eLife.04577; Aso Y et al. (2014) eLife 3:e04580, doi:10.7554/eLife.04580; Hige T, Aso Y, Modi MN, Rubin GM, Turner GC (2015) Neuron 88:985-998, doi:10.1016/j.neuron.2015.11.003" };
  s.cells = { "kc" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = kMbEnd;
    odour_segments( odour_a(), 500, 900, kMbHz, c.segs );
    odour_segments( odour_b(), 1100, 1500, kMbHz, c.segs );
    for ( double t : { 1900.0, 2500.0 } )
    {
      odour_segments( odour_a(), t, t + 400, kMbHz, c.segs );
      c.segs.push_back( { "punish", t, t + 400, 100 } );
    }
    odour_segments( odour_a(), 3300, 3700, kMbHz, c.segs );
    odour_segments( odour_b(), 3900, 4300, kMbHz, c.segs );
    mb_windows( c );
    return c;
  };
  s.timeline = []() -> std::vector< Task::Event > { return { { 500, 400, "odour A" }, { 1100, 400, "odour B" }, { 1900, 400, "odour A + punishment" }, { 2500, 400, "odour A + punishment" }, { 3300, 400, "odour A" }, { 3900, 400, "odour B" } }; };
  s.mech = []( const Graph& g ) -> std::string {
    std::string r = sparse_code_missing( g, "kc" );
    if ( !r.empty() ) return r;
    const auto kc = g.out_pops( "kc" ), av = g.out_pops( "avoid" );
    if ( !kc.empty() && !av.empty() && !g.path( kc, av, +1, 2 ) )
      return "The Kenyon cells do not reach the avoidance cells: the odour's code has to excite the output neurons that drive avoidance.";
    return dopamine_gate_missing( g, "punish", "avoid", "pn1" );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double pool = std::max( 1.0, m.pool.count( "kc" ) ? m.pool.at( "kc" ) : 1.0 );
    const int a = m.active( "kc", 550, 900 ), b = m.active( "kc", 1150, 1500 ), shared = m.both( "kc", 550, 900, 1150, 1500 );
    p[ "kc_active_a" ] = a;
    p[ "kc_active_b" ] = b;
    p[ "kc_shared" ] = shared;
    const double cap = ( m.tol > 1.2 ? 0.40 : 0.25 ) * pool;
    c.push_back( { "sparse", a >= 3 && a <= cap && b <= cap, a < 3 ? ramp( a, 0, 3 ) : 1.0 - ramp( std::max( a, b ), cap, pool ),
      a < 3 ? "Odour A makes (almost) no Kenyon cell fire: the glomeruli must drive the projection neurons and those must excite the Kenyon cells strongly enough that several inputs together reach threshold."
            : "The Kenyon-cell code is dense: many cells fire for one odour. Each cell should listen to few projection neurons and need most of them, and the APL must inhibit the group in proportion to its activity." } );
    c.push_back( { "separate", shared <= 0.5 * std::min( a, b ) + 0.5, 1.0 - ramp( shared / std::max( 1.0, double( std::min( a, b ) ) ), 0.5, 1.0 ),
      "Odours A and B excite many of the same Kenyon cells, so whatever is learned about one is learned about the other: the code must be sparser (fewer inputs per cell, stronger feedback inhibition)." } );
    mb_judge( m, c, p, "avoid" );
  };
  s.ok_verdict = "The fly has learned: both odours drove the avoidance cells before training, the Kenyon-cell code is sparse and different for the two odours, and after pairing odour A with punishment the avoidance cells answer it much less than odour B.";
  s.h1 = "Three parts: a sparse odour code (random few-input wiring plus feedback inhibition), a punishment pathway to dopamine neurons, and a synapse from the odour code onto the output neurons that dopamine can weaken.";
  s.h2 = "The memory lives in the synapse from the Kenyon cells onto the avoidance output cells: it must be dopamine-gated and its dopamine source is driven by the punishment. The sparse code is what keeps odour B's synapses untouched.";
  s.h2_target = "avoid";
  s.h3 = "Glomeruli sensors to projection neurons one-to-one; projection neurons to many Kenyon cells with random sparse wiring; Kenyon cells excite an inhibitory group that inhibits them. Kenyon cells to a few output cells through a "
         "dopamine-gated depressing synapse whose source is the dopamine group; punish to a relay cell that drives the dopamine group; the output cells need a little bias. If the response to A does not fall, strengthen the learning; if B falls too, "
         "make the code sparser or lower the learning.";
  s.metric_name = "a_after_over_before_pct";
  s.metric_unit = "%";
  s.metric = []( const Meas& m ) {
    const double base = m.hz( "avoid", "base" ), a0 = std::max( 0.0, m.hz( "avoid", "a0" ) - base ), a1 = std::max( 0.0, m.hz( "avoid", "a1" ) - base );
    return a0 > 0 ? 100.0 * a1 / a0 : 100.0;
  };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_fly_mb() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( fly_mb, 542, "fly_mb.json", make_fly_mb ) }
