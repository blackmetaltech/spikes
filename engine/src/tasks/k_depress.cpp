// Micro-puzzle "k_depress" (fly mushroom body chapter, concept "dopamine_gated_depression": a synapse that weakens only when its input was active and dopamine arrives). In the fly mushroom body an odour is the
// activity of Kenyon cells; pairing an odour with punishment makes dopamine neurons fire, and that weakens the synapses from the odour's Kenyon cells onto the output neurons (MBONs) that drive avoidance... so afterwards that odour
// excites the MBON less. Other odours, whose Kenyon cells were silent, keep their synapses.
//
// WORLD (4400 ms; Poisson sensors odor_a, odor_b (100 Hz, each drives its own Kenyon cells) and punish (100 Hz, the dopamine neurons' sensor); the MBON group is read on 'avoid')
//   base [0, 400)               quiet
//   naive   odour A [500, 900) (window [550, 900)) and odour B [1100, 1500) (window [1150, 1500)): both must make the avoidance cells fire (response above baseline >= 8 Hz per cell; guided 5)
//   pairing odour A with punishment [1900, 2300) and again [2500, 2900)
//   trained odour A [3300, 3700) (window [3350, 3700)): the response must have fallen to at most 65 % of the naive one (guided 80, researcher 60); odour B [3900, 4300): at least 55 % of the naive one (guided 45, researcher 60)
// MECHANISM (Engine::structure()): the odour cells reach the avoidance cells through a dopamine-gated synapse (NEST stdp_dopamine_synapse), whose dopamine source is a cell group that the punishment sensor drives.
// WHY stdp_dopamine_synapse: its eligibility trace c is set by pre and post spikes (the odour cells' spikes and the MBON's own), and the weight changes only while dopamine is present (dw/dt = c n), so a silent input is never changed
// and a depression needs no extra NESTML synapse: with the amplitudes A_plus < 0 and A_minus > 0 (NEST subtracts A_minus for anti-causal pairs) every pairing makes c negative, i.e. it depresses.
// Biology: Hige et al. 2015 Neuron 88:985 (pairing an odour with activation of specific dopamine neurons induces odour-specific synaptic depression at the KC > MBON synapse, strictly ordered: odour first); Aso et al. 2014
// eLife 3:e04577, 3:e04580 (MBON compartments, DAN compartments, MBON ensembles encode valence). The time scale is compressed (seconds for minutes); the avoidance MBON is a generic cell.
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_k_depress();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "k_depress";
  i.title = "Fly: learning to avoid an odour";
  i.brief = "A fly learns that an odour is dangerous. In the mushroom body the odour is carried by Kenyon cells, which excite output neurons (MBONs); here the output neurons drive avoidance. Punishment makes dopamine neurons fire, and "
            "dopamine does not strengthen or weaken anything on its own: it weakens the synapses from the Kenyon cells that were active just before (the eligibility of a synapse is set by its own activity), so only the punished odour's "
            "synapses lose strength. The wiring is a model after the fly's cell types, not synapse-level data. The sensors 'odor_a' and 'odor_b' drive two groups of Kenyon cells, one for each odour; "
            "'punish' drives the dopamine neurons; read 'avoid' from the avoidance neurons. Both odours must make the avoidance cells fire before training. Then odour A is paired with punishment twice. "
            "Afterwards the avoidance cells must answer odour A much less than before, and odour B as much as before. Use a dopamine-gated synapse from each group of Kenyon cells onto the avoidance cells (a depressing "
            "synapse whose change is gated by a dopamine source), and let the punishment drive the dopamine neurons. The avoidance cells need a little baseline activity of their own, so that the synapse can compare the activity of both its sides.";
  i.chapter = "fly_mb";
  i.sensors = { "odor_a", "odor_b", "punish" };
  i.motors = { "avoid" };
  i.duration_ms = kMbEnd;
  i.concepts_new = { "dopamine_gated_depression" };
  i.concepts_used = { "excitation", "heterosynaptic_facilitation" };
  i.par_neurons = 6 + 6 + 4 + 2;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "odor_a", in_kind( "poisson", 100, 1000 ) }, { "odor_b", in_kind( "poisson", 100, 1000 ) }, { "punish", in_kind( "poisson", 100, 1000 ) }, { "avoid", micro::rate() } };
  i.bands = { { "avoid_hz", "Hz", 0, 400 } };
  i.recipe = "Eighteen integrate-and-fire cells: KCA and KCB (6 cells each), MBON (4 cells, bias 350 pA), DAN (2 cells). odor_a > KCA, odor_b > KCB and punish > DAN at the default 1000 pA. KCA > MBON and KCB > MBON "
             "with a dopamine-gated synapse (stdp_dopamine_synapse): weight 300 pA, Wmax 900 pA, learning amplitude 0.6 (A_plus -0.6, A_minus +0.6), tau c 30 ms, tau n 200 ms, dopamine source DAN. MBON > 'avoid'. "
             "Both odours make the MBON group fire at over a hundred Hz; after the pairing odour A makes it fire at a third of that and odour B at the same rate.";
  i.note = { "Depression that depends on both activity and dopamine",
    "Pairing an odour with activation of dopamine neurons induces a lasting depression of the synapses from the odour's Kenyon cells onto the mushroom body output neuron of that compartment, as shown in vivo. "
    "The plasticity requires the odour first and the dopamine second, and it does not spread to Kenyon cells that were silent, so it is odour-specific. Dopamine acts only inside its own compartment. "
    "In this level the three-factor rule is NEST's dopamine-gated STDP with the signs set to depress; the real rule needs no postsynaptic spike, which is why the output neurons here have a baseline firing.",
    "Hige T, Aso Y, Modi MN, Rubin GM, Turner GC (2015) Neuron 88:985-998, doi:10.1016/j.neuron.2015.11.003" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = kMbEnd;
    c.segs = { { "odor_a", 500, 900, 100 }, { "odor_b", 1100, 1500, 100 }, { "odor_a", 1900, 2300, 100 }, { "punish", 1900, 2300, 100 }, { "odor_a", 2500, 2900, 100 }, { "punish", 2500, 2900, 100 },
      { "odor_a", 3300, 3700, 100 }, { "odor_b", 3900, 4300, 100 } };
    mb_windows( c );
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string { return dopamine_gate_missing( g, "punish", "avoid", "odor_a" ); };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) { mb_judge( m, c, p, "avoid" ); };
  s.ok_verdict = "A memory: odour A, paired with punishment, now excites the avoidance cells much less, while odour B still excites them as before, through a dopamine-gated depression of the synapses of the odour that was active.";
  s.h1 = "Learning here is a three-factor rule: a synapse changes only if its input was active, the output cell was active, and dopamine arrives.";
  s.h2 = "The synapse from the Kenyon cells onto the avoidance cells must be a dopamine-gated one, with the dopamine cells as its source and the punishment sensor driving the dopamine cells.";
  s.h2_target = "avoid";
  s.h3 = "odor_a to a group of Kenyon cells and odor_b to another; both groups onto the avoidance cells through dopamine-gated synapses that depress (negative amplitude for causal pairs, positive for the other order), with the dopamine cells as source; "
         "punish to the dopamine cells. If nothing is learned, raise the amplitude or check that the dopamine cells fire in the punishment; if odour B is weakened too, lower the amplitude or shorten the eligibility trace.";
  s.metric_name = "a_after_over_before_pct";
  s.metric_unit = "%";
  s.metric = []( const Meas& m ) {
    const double base = m.hz( "avoid", "base" ), a0 = std::max( 0.0, m.hz( "avoid", "a0" ) - base ), a1 = std::max( 0.0, m.hz( "avoid", "a1" ) - base );
    return a0 > 0 ? 100.0 * a1 / a0 : 100.0;
  };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_k_depress() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( k_depress, 541, "k_depress.json", make_k_depress ) }
