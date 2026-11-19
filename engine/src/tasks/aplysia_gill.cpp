// Task "aplysia_gill" (aplysia chapter): the gill-withdrawal reflex of the sea slug Aplysia. Repeated touches of the siphon habituate it, a rest lets it recover, and a shock to the tail
// sensitises it. Habituation is a depressing synapse (short-term depression, tsodyks2); sensitisation is a modulator (serotonin) released by a facilitating interneuron that raises the
// release of that same synapse (a modulation row on the synapse).
//
// WORLD (6000 ms; Poisson sensors 'touch' 200 Hz for 100 ms and 'shock' 150 Hz for 300 ms, default weight 1000 pA; the response of a touch is the number of spikes of 'gill' in [t, t + 150))
//   H1..H4 touches at 400, 900, 1400, 1900       a train of four, 500 ms apart: habituation
//   R      a touch at 4400                       after a rest of 2.5 s: recovery
//   S0     a touch at 4900                       500 ms after R: the control (no shock)
//   R2     a touch at 6400, shock [6480, 6730), S1 a touch at 6900   S1 comes 500 ms after R2 as S0 comes 500 ms after R, so that the recovery between the two touches is
//          the same and only the shock differs: sensitisation
// SCORING
//   first   H1 >= 3 spikes (guided 2)                    the reflex exists
//   habit   H4 <= 50 % of H1 (guided 65 %)
//   recover R >= 60 % of H1 (guided 50 %)    the synapse recovers during the rest: depression that never recovers is no habituation but damage
//   sens    S1 >= 2 x S0 (guided 1.6 x), and S1 >= 3 spikes
//   quiet   <= 2 spikes per cell of 'gill' outside the touch windows and the shock window (responses are spikes per cell: a bigger motor pool does not change the scale)
// MECHANISM (Engine::structure()): the touch reaches 'gill' by excitation through a depressing synapse; the shock reaches the cell that releases the modulator (output 'serotonin' of
// kind modulator), and that modulator acts, in the modulation table, on a synapse of the touch path.
// Biology: Castellucci et al. 1970 Science 167:1745 (habituation and dishabituation at the sensory-motor synapse); Byrne et al. 1978 J Neurophysiol 41:402; Hawkins, Kandel & Bailey 2006
// Biol Bull 210:174 (serotonin, short-term facilitation). Time is compressed by a large factor (real habituation develops over many touches and minutes); serotonin is the engine's NE
// channel; the shock's effect on the synapse is a direct change of its release probability.
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_aplysia_gill();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "aplysia_gill";
  i.title = "Aplysia: the gill-withdrawal reflex";
  i.brief = "Touch the siphon of the sea slug Aplysia and it withdraws its gill. Touch it four times in a row and the withdrawal shrinks: it habituates. Leave it alone for a few "
            "seconds and the reflex comes back. Give the tail a shock and the reflex becomes stronger: it is sensitised. Build the reflex arc and both kinds of learning. 'touch' drives a "
            "sensory cell that drives the motor neuron behind 'gill'; habituation lives in that synapse, which must lose strength with use and recover with rest (a depressing synapse). "
            "'shock' drives a facilitating interneuron that releases serotonin ('serotonin' channel), and the serotonin must act on the same synapse and strengthen it. The reflex must be there, "
            "must be smaller on the fourth touch than on the first, must be back after the rest, must be clearly larger after the shock than just before it, and the gill must be still when "
            "nothing touches it. The tail shock may also make the gill move by itself. The real slug needs many more touches and minutes; this level compresses time.";
  i.chapter = "aplysia";
  i.sensors = { "touch", "shock" };
  i.motors = { "gill", "serotonin" };
  i.duration_ms = 6200;
  i.concepts_new = {};
  i.concepts_used = { "synaptic_depression", "heterosynaptic_facilitation", "neuromodulation" };
  i.par_neurons = 9;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "touch", in_kind( "poisson", 200, 1000 ) }, { "shock", in_kind( "poisson", 150, 1000 ) }, { "gill", micro::rate() }, { "serotonin", micro::modulator( "NE", 0.05, 1000.0 ) } };
  i.modulators = { "NE" };
  i.bands = { { "gill_hz", "Hz", 0, 400 } };
  i.recipe = "The reference (circuits/aplysia_gill.json) has nine integrate-and-fire cells: SN (6 sensory cells), MN, TSN, FN. touch > SN and shock > TSN at 1000 pA; SN > MN with a depressing synapse "
             "(tsodyks2: U = 0.1, u = 0.1, x = 1, tau_rec = 1000 ms, tau_fac = 0) of weight 2500 pA; TSN > MN at +1500 pA (the shock also moves the gill directly); TSN > FN at +1000 pA; "
             "MN > 'gill'; FN > 'serotonin' (release a modulator: NE channel, gain 0.05 per spike, decay 1000 ms). One modulation row on the SN > MN connection: modulator NE, "
             "parameter U, 'add per unit' +1.5, clamp 0 to 1. With one sensory cell instead of six the reflex is only a few spikes and the learning is hard to see.";
  i.note = { "Two kinds of memory in one synapse",
    "In the gill-withdrawal reflex of Aplysia the sensory neurons of the siphon excite the motor neurons of the gill directly. Repeated touch makes that synapse weaker "
    "(habituation, recovering with rest), and a noxious stimulus to the tail makes it stronger through facilitating interneurons that release serotonin onto the sensory terminals "
    "(sensitisation). A single synapse thus stores two opposite changes with different causes. Short-term changes last minutes; repeated, spaced training leads to changes that last days.",
    "Castellucci V, Pinsker H, Kupfermann I, Kandel ER (1970) Science 167:1745-1748, doi:10.1126/science.167.3926.1745; Hawkins RD, Kandel ER, Bailey CH (2006) Biol Bull 210:174-191, doi:10.2307/4134556" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 7100;
    const double ts[] = { 400, 900, 1400, 1900, 4400, 4900, 6900 };
    const char* nm[] = { "H1", "H2", "H3", "H4", "R", "S0", "S1" };
    for ( int k = 0; k < 7; ++k )
    {
      c.segs.push_back( { "touch", ts[ k ], ts[ k ] + 100, 200 } );
      c.wins.push_back( { nm[ k ], ts[ k ], ts[ k ] + 150 } );
    }
    c.segs.push_back( { "touch", 6400, 6500, 200 } );   // R2: the touch before the shock, 500 ms before S1, as R is 500 ms before S0
    c.segs.push_back( { "shock", 6480, 6730, 150 } );
    c.wins.push_back( { "q0", 0, 380 } );
    c.wins.push_back( { "q1", 2070, 4380 } );
    c.wins.push_back( { "q2", 4570, 4880 } );
    c.wins.push_back( { "q3", 5070, 6380 } );
    c.wins.push_back( { "q4", 6780, 6880 } );
    c.wins.push_back( { "q5", 7070, 7100 } );
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto t = g.in_pops( "touch" ), m = g.out_pops( "gill" ), sh = g.in_pops( "shock" );
    if ( t.empty() || m.empty() ) return "";
    if ( !g.path( t, m, +1, 3, true ) ) return "The touch does not reach 'gill' by excitation: connect the sensory cell to the motor neuron.";
    std::string d = depression_missing( g, t, m );
    if ( !d.empty() ) return d;
    return facilitator_missing( g, t, m, sh, "serotonin" );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double h1 = m.c( "gill", "H1" ), h4 = m.c( "gill", "H4" ), r = m.c( "gill", "R" ), s0 = m.c( "gill", "S0" ), s1 = m.c( "gill", "S1" );
    double q = 0;
    for ( const char* k : { "q0", "q1", "q2", "q3", "q4", "q5" } ) q += m.c( "gill", k );
    p[ "H1" ] = h1;
    p[ "H4" ] = h4;
    p[ "R" ] = r;
    p[ "S0" ] = s0;
    p[ "S1" ] = s1;
    p[ "habituation" ] = h1 > 0 ? h4 / h1 : 0.0;
    p[ "recovery" ] = h1 > 0 ? r / h1 : 0.0;
    p[ "sensitisation" ] = s0 > 0 ? s1 / s0 : ( s1 > 0 ? 9.99 : 0.0 );
    p[ "quiet_spikes" ] = q;
    c.push_back( { "first", h1 >= ( m.tol > 1.2 ? 2.0 : 3.0 ), ramp( h1, 0, 3 ), "The first touch hardly moves the gill: the reflex has to be there before it can learn anything. Strengthen the synapse onto the motor neuron." } );
    const double hab = m.tol > 1.2 ? 0.65 : 0.5;
    c.push_back( { "habit", h1 > 0 && h4 <= hab * h1, h1 > 0 ? 1.0 - ramp( h4 / h1, hab, 1.0 ) : 0.0,
      "The fourth touch is nearly as strong as the first: the reflex does not habituate. The sensory synapse must lose strength with use, and recover slowly enough not to be full again 500 ms later." } );
    const double rec = m.tol > 1.2 ? 0.5 : 0.6;
    c.push_back( { "recover", h1 > 0 && r >= rec * h1, h1 > 0 ? ramp( r / h1, 0, rec ) : 0.0,
      "After the rest the reflex has not come back: the synapse recovers too slowly. A habituated reflex recovers within seconds of rest in this level: speed up the recovery of the synapse." } );
    const double sens = m.tol > 1.2 ? 1.6 : 2.0;
    c.push_back( { "sens", s1 >= 3 && s1 >= sens * s0, s0 > 0 ? ramp( s1 / s0, 1.0, sens ) : ramp( s1, 0, 3 ),
      "The touch after the tail shock is no stronger than the one before it: the shock does not sensitise the reflex. The shock must make the facilitating interneuron release its modulator, and "
      "the modulator must strengthen the synapse of the touch pathway enough to show." } );
    c.push_back( { "quiet", q <= 2, 1.0 - ramp( q, 2, 30 ), "The gill moves when nothing touches it: the motor neuron must be silent unless a touch (or the shock) drives it." } );
  };
  s.ok_verdict = "A gill-withdrawal reflex that learns: it habituates to repeated touch, recovers with rest, and is sensitised by a shock to the tail, through one depressing synapse that a modulator strengthens.";
  s.h1 = "One synapse can store two opposite changes: it depresses with use (habituation) and is strengthened by a modulator released by another pathway (sensitisation).";
  s.h2 = "The habituation is in the synapse from the sensory cell to the motor neuron: make it a depressing one. The sensitisation is a modulator from the tail pathway acting on that same synapse.";
  s.h2_target = "gill";
  s.h3 = "touch to a sensory cell, then a depressing synapse onto the motor neuron and 'gill'; shock to a facilitating interneuron that releases a modulator on the 'serotonin' channel, and a modulation row on the "
         "sensory synapse that raises its release probability with that modulator. If the reflex does not come back after the rest, shorten the recovery of the synapse; if it habituates too little, "
         "lengthen the recovery or deepen the depression; if the shock changes nothing, strengthen the modulation.";
  s.metric_name = "sensitisation_x";
  s.metric_unit = "x";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) {
    const double a = m.c( "gill", "S0" ), b = m.c( "gill", "S1" );
    return a > 0 ? b / a : 0.0;
  };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_aplysia_gill() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( aplysia_gill, 522, "aplysia_gill.json", make_aplysia_gill ) }
