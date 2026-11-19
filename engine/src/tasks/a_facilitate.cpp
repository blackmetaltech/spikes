// Micro-puzzle "a_facilitate" (aplysia chapter, concept "heterosynaptic_facilitation": activity in one pathway changes the strength of a synapse in another). A shock to the tail
// of Aplysia sensitises the gill-withdrawal reflex: serotonin released by facilitating interneurons strengthens the synapses of the siphon sensory neurons.
//
// WORLD (3800 ms; Poisson sensors, default weight 1000 pA)
//   touch (200 Hz, 100 ms) at 300 and 900 ms (before the shock), at 2100 and 2700 ms (after it); shock (150 Hz) on [1400, 1700)
//   The response of a touch is the number of spikes of 'gill' in [t, t + 150). Motors: 'gill' (rate) and 'serotonin' (the circuit's own modulator release: the spikes of the
//   cell wired to it add to the level of the engine's NE channel, which decays over a second; the game's serotonin).
// SCORING
//   before   mean response of the two touches before the shock: 0.5 to 6 spikes (guided 0.5 to 9)     the reflex exists and has room to grow
//   after    mean response after the shock >= 2 x the response before (guided 1.6 x, researcher 2.4 x)
//   quiet    no spikes between the touches (<= 2 in total)
// MECHANISM (Engine::structure()): touch reaches 'gill' by excitation; the shock reaches the cell that releases the modulator ('serotonin' output of kind modulator);
// that modulator is wired, in the modulation table, to a synapse on the touch path (a row on a projection, not on a cell).
// Biology: Castellucci V et al. (1970) Science 167:1745 (dishabituation and sensitisation act at the same synapses); Hawkins RD, Kandel ER, Bailey CH (2006) Biol Bull 210:174
// (serotonin, cAMP and PKA, in short-, intermediate- and long-term phases). The engine has three generic modulator channels (NE, ACh, DA); this level uses the NE channel as the
// stand-in for serotonin, and a direct change of the synapse (its weight, or the release probability of a depressing one) as the stand-in for the cAMP cascade.
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_facilitate();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "a_facilitate";
  i.title = "Aplysia: a shock to the tail";
  i.brief = "A shock to the tail of the sea slug Aplysia makes its gill-withdrawal reflex stronger for a while: the reflex is sensitised. The tail pathway does not drive the gill: "
            "it drives a facilitating interneuron that releases serotonin, and serotonin strengthens the synapse of the siphon's sensory cell onto the motor neuron. Build it: "
            "'touch' drives a sensory cell, which drives the motor neuron behind 'gill'; 'shock' drives a facilitating interneuron whose spikes release the modulator "
            "('serotonin' channel); and the released modulator must act on the sensory synapse (select that connection and let the modulator change it). Two touches before the "
            "shock give a modest withdrawal; two after it must give clearly more, with nothing happening between the touches.";
  i.chapter = "aplysia";
  i.sensors = { "touch", "shock" };
  i.motors = { "gill", "serotonin" };
  i.duration_ms = 3800;
  i.concepts_new = { "heterosynaptic_facilitation" };
  i.concepts_used = { "neuromodulation", "excitation" };
  i.par_neurons = 4;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "touch", in_kind( "poisson", 200, 1000 ) }, { "shock", in_kind( "poisson", 150, 1000 ) }, { "gill", micro::rate() }, { "serotonin", micro::modulator( "NE", 0.05, 1000.0 ) } };
  i.modulators = { "NE" };
  i.bands = { { "gill_hz", "Hz", 0, 400 } };
  i.recipe = "Four integrate-and-fire cells: SN, MN, TSN, FN. touch > SN and shock > TSN at the default 1000 pA; SN > MN at +450 pA (a static synapse); TSN > FN at +1000 pA; MN > 'gill'; "
             "FN > 'serotonin' with the channel set to release a modulator (NE channel, gain 0.05 per spike, decay 1000 ms). Then one modulation row on the SN > MN connection: "
             "modulator NE, parameter weight, 'scale per unit' +4.0. A touch gives a few spikes before the shock and several times as many after it.";
  i.note = { "Sensitisation in a sea slug",
    "A strong stimulus to the tail of Aplysia enhances the gill-withdrawal reflex for minutes. The tail pathway excites facilitating interneurons that release serotonin onto the "
    "terminals of the siphon sensory neurons, which then release more transmitter onto the motor neurons. The same synapses that depress with repeated touch are the ones that are "
    "strengthened, which is why a strong stimulus elsewhere can also restore a habituated response (dishabituation). Short-term facilitation uses cAMP and protein kinase A; "
    "repeated, spaced shocks recruit slower phases that need gene expression.",
    "Castellucci V, Pinsker H, Kupfermann I, Kandel ER (1970) Science 167:1745-1748, doi:10.1126/science.167.3926.1745; Hawkins RD, Kandel ER, Bailey CH (2006) Biol Bull 210:174-191, doi:10.2307/4134556" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 3800;
    const double ts[] = { 300, 900, 2100, 2700 };
    for ( int k = 0; k < 4; ++k )
    {
      c.segs.push_back( { "touch", ts[ k ], ts[ k ] + 100, 200 } );
      c.wins.push_back( { "t" + std::to_string( k + 1 ), ts[ k ], ts[ k ] + 150 } );
    }
    c.segs.push_back( { "shock", 1400, 1700, 150 } );
    c.wins.push_back( { "q0", 0, 280 } );
    c.wins.push_back( { "q1", 470, 880 } );
    c.wins.push_back( { "q2", 1070, 2080 } );
    c.wins.push_back( { "q3", 2270, 2680 } );
    c.wins.push_back( { "q4", 2870, 3800 } );
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto t = g.in_pops( "touch" ), m = g.out_pops( "gill" ), sh = g.in_pops( "shock" );
    if ( t.empty() || m.empty() ) return "";
    if ( !g.path( t, m, +1, 3, true ) ) return "The touch does not reach 'gill' by excitation: connect the sensory cell to the motor neuron.";
    return facilitator_missing( g, t, m, sh, "serotonin" );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double before = 0.5 * ( m.n( "gill", "t1" ) + m.n( "gill", "t2" ) ), after = 0.5 * ( m.n( "gill", "t3" ) + m.n( "gill", "t4" ) );
    double q = 0;
    for ( const char* k : { "q0", "q1", "q2", "q3", "q4" } ) q += m.n( "gill", k );
    p[ "before" ] = before;
    p[ "after" ] = after;
    p[ "facilitation" ] = before > 0 ? after / before : 0.0;
    p[ "quiet_spikes" ] = q;
    const double top = m.tol > 1.2 ? 9 : 6;
    c.push_back( { "before", before >= 0.5 && before <= top, before < 0.5 ? ramp( before, 0, 0.5 ) : 1.0 - ramp( before, top, 3 * top ),
      before < 0.5 ? "The touch hardly moves the gill before the shock: the reflex has to be there before it can be strengthened. Strengthen the synapse onto the motor neuron a little."
                : ( before > 0 && after >= 2.0 * before
                      ? "The shock does strengthen the reflex, but the touches before it are already stronger than a modest withdrawal. Make the touch synapse weaker, and let the modulator do the strengthening."
                      : "The reflex is already strong before any shock, so a shock has little room to strengthen it. Make the touch synapse weaker, and let the modulator do the strengthening." ) } );
    const double need = m.tol > 1.2 ? 1.6 : ( m.tol < 0.9 ? 2.4 : 2.0 );
    c.push_back( { "after", before > 0 && after >= need * before, before > 0 ? ramp( after / before, 1.0, need ) : 0.0,
      after > before ? "The touches after the shock are stronger than those before it, but not by enough: make the modulator act on the touch synapse more strongly, or let the facilitating interneuron fire more."
                     : "The touches after the shock are no stronger than those before it: the shock does not facilitate the reflex. Check that the tail pathway makes the facilitating interneuron fire, that "
                       "its modulator is released, and that the modulator acts on the touch synapse strongly enough." } );
    c.push_back( { "quiet", q <= 2, 1.0 - ramp( q, 2, 30 ), "The gill moves between the touches: the motor neuron must be silent unless the sensory cell drives it." } );
  };
  s.ok_verdict = "Sensitisation: the tail shock releases a modulator that strengthens the touch synapse, so the touches after it give a clearly larger withdrawal.";
  s.h1 = "One pathway can change how strongly another one transmits: a modulator released by the first acts on the synapse of the second.";
  s.h2 = "The tail pathway ends in a cell that releases the modulator; the modulator must then be connected to the synapse between the sensory cell and the motor neuron. Select that connection.";
  s.h2_target = "serotonin";
  s.h3 = "shock to the facilitating cell, whose spikes release the modulator on the 'serotonin' channel; the sensory synapse onto the motor neuron has a modulation row that raises its strength with the "
         "modulator. If the touches after the shock are not stronger, make the modulation stronger or the facilitating cell fire more; if the touches are already strong before the shock, make the "
         "synapse weaker.";
  s.metric_name = "facilitation_x";
  s.metric_unit = "x";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) {
    const double b = 0.5 * ( m.n( "gill", "t1" ) + m.n( "gill", "t2" ) ), a = 0.5 * ( m.n( "gill", "t3" ) + m.n( "gill", "t4" ) );
    return b > 0 ? a / b : 0.0;
  };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_a_facilitate() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_facilitate, 521, "a_facilitate.json", make_a_facilitate ) }
