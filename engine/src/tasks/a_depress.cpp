// Micro-puzzle "a_depress" (aplysia chapter, concept "synaptic_depression": a synapse that runs out of transmitter makes a reflex habituate). Touch the siphon of the sea slug
// Aplysia again and again and the gill withdrawal gets smaller, although the touch-sensitive cells keep firing the same: the change is in the synapse onto the motor neuron.
//
// WORLD (3200 ms; Poisson sensor 'touch', 200 Hz while on, default weight 1000 pA onto a sensory cell, which fires once per input spike)
//   five touches of 100 ms at 300, 800, 1300, 1800, 2300 ms. The response of a touch is the number of spikes of 'gill' in [t, t + 150).
// SCORING
//   first   response of touch 1 >= 3 spikes (guided 2)            the reflex exists
//   habit   response of touch 5 <= 50 % of touch 1 (guided 65 %, researcher 40 %)   the reflex habituates
//   quiet   [0, 280) and [450, 780), ... between the touches: <= 2 spikes in total
// MECHANISM (Engine::structure()): the touch reaches 'gill' by excitation through at least one depressing synapse (tsodyks2 / tsodyks / quantal STP).
// Biology: Castellucci V, Pinsker H, Kupfermann I, Kandel ER (1970) Science 167:1745: habituation of the Aplysia gill-withdrawal reflex is a change in the efficacy of the synapses
// of the sensory neurons onto the motor neurons; Byrne JH, Castellucci VF, Carew TJ, Kandel ER (1978) J Neurophysiol 41:402: the sensory neurons themselves do not decrement. The level
// compresses the time scale (real habituation develops over minutes of stimulation and recovers over many minutes), and uses NEST's generic short-term depression synapse (tsodyks2).
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_a_depress();
namespace
{
using namespace bestiary;

const double kTouch[] = { 300, 800, 1300, 1800, 2300 };

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "a_depress";
  i.title = "Aplysia: getting used to a touch";
  i.brief = "Touch the siphon of the sea slug Aplysia and it pulls in its gill. Touch it again and again and the withdrawal gets smaller and smaller, although the touch-sensitive "
            "cells keep firing just the same: the reflex habituates, and the change is in the synapse between the sensory cell and the motor neuron. The sensor 'touch' is "
            "pressed five times; the first withdrawal on 'gill' must be a clear one, and the fifth clearly weaker, with nothing happening between the touches. A normal "
            "(static) synapse gives every touch the same answer. A depressing synapse runs out of transmitter when it is used hard and refills slowly, so each touch finds it "
            "a little emptier. Build a sensory cell, a motor neuron and that kind of synapse between them.";
  i.chapter = "aplysia";
  i.sensors = { "touch" };
  i.motors = { "gill" };
  i.duration_ms = 3200;
  i.concepts_new = { "synaptic_depression" };
  i.concepts_used = { "excitation" };
  i.par_neurons = 2;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "touch", in_kind( "poisson", 200, 1000 ) }, { "gill", micro::rate() } };
  i.bands = { { "gill_hz", "Hz", 0, 400 } };
  i.recipe = "Two integrate-and-fire cells: SN (sensory), MN (motor). touch > SN at the default 1000 pA; SN > MN with a depressing synapse (tsodyks2: U = 0.2, u = 0.2, x = 1, tau_rec = 1500 ms, "
             "tau_fac = 0) of weight 8000 pA; MN > 'gill'. The first touch gives a handful of spikes, the fifth a fraction of that.";
  i.note = { "Habituation is in the synapse",
    "Repeated light touch to the siphon of Aplysia makes the gill-withdrawal reflex smaller. Kandel's group traced this to the synapses of the sensory neurons onto the "
    "motor neurons, which become less effective with repetition, while the sensory neurons themselves keep firing the same. The change is short-lived: the reflex recovers after a rest. "
    "This level compresses the time scale and uses a generic model of synaptic depression.",
    "Castellucci V, Pinsker H, Kupfermann I, Kandel ER (1970) Science 167:1745-1748, doi:10.1126/science.167.3926.1745; Byrne JH, Castellucci VF, Carew TJ, Kandel ER (1978) J Neurophysiol 41:402-417, doi:10.1152/jn.1978.41.2.402" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 3200;
    for ( int k = 0; k < 5; ++k )
    {
      c.segs.push_back( { "touch", kTouch[ k ], kTouch[ k ] + 100, 200 } );
      c.wins.push_back( { "t" + std::to_string( k + 1 ), kTouch[ k ], kTouch[ k ] + 150 } );
    }
    c.wins.push_back( { "q0", 0, 280 } );
    for ( int k = 0; k < 4; ++k ) c.wins.push_back( { "q" + std::to_string( k + 1 ), kTouch[ k ] + 170, kTouch[ k + 1 ] - 20 } );
    c.wins.push_back( { "q5", kTouch[ 4 ] + 170, 3200 } );
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto t = g.in_pops( "touch" ), m = g.out_pops( "gill" );
    if ( t.empty() || m.empty() ) return "";
    if ( !g.path( t, m, +1, 3, true ) ) return "The touch does not reach 'gill' by excitation: connect the sensory cell to the motor neuron.";
    return depression_missing( g, t, m );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double r1 = m.n( "gill", "t1" ), r5 = m.n( "gill", "t5" );
    double q = 0;
    for ( const char* k : { "q0", "q1", "q2", "q3", "q4", "q5" } ) q += m.n( "gill", k );
    p[ "response_first" ] = r1;
    p[ "response_last" ] = r5;
    p[ "last_over_first" ] = r1 > 0 ? r5 / r1 : 0.0;
    p[ "quiet_spikes" ] = q;
    c.push_back( { "first", r1 >= ( m.tol > 1.2 ? 2.0 : 3.0 ), ramp( r1, 0, 3 ), "The first touch hardly moves the gill: the reflex has to be there before it can habituate. Strengthen the synapse onto the motor neuron." } );
    const double lim = 0.5 * m.tol;
    c.push_back( { "habit", r1 > 0 && r5 <= lim * r1, r1 > 0 ? 1.0 - ramp( r5 / r1, lim, 1.0 ) : 0.0,
      "The fifth touch is nearly as strong as the first: the reflex does not habituate. The synapse must lose strength with use, and not recover completely between touches (a slower recovery makes it stay depressed)." } );
    c.push_back( { "quiet", q <= 2, 1.0 - ramp( q, 2, 30 ), "The gill moves between the touches: the motor neuron must be silent unless the sensory cell drives it." } );
  };
  s.ok_verdict = "Habituation: the first touch gives a clear withdrawal and each repeat gives less, through a synapse that depresses with use.";
  s.h1 = "A synapse can change its strength on a time scale of seconds: a depressing synapse releases less each time it is used and recovers slowly.";
  s.h2 = "The synapse between the sensory cell and the motor neuron has to be of the depressing kind, not the static one. Select the connection and change its type.";
  s.h2_target = "gill";
  s.h3 = "touch to a sensory cell, the sensory cell to a motor neuron through a depressing synapse, the motor neuron to 'gill'. If the fifth touch is as large as the first, make the recovery slower or the "
         "depression stronger; if even the first touch is weak, make the synapse stronger.";
  s.metric_name = "last_over_first_pct";
  s.metric_unit = "%";
  s.metric = []( const Meas& m ) {
    const double r1 = m.n( "gill", "t1" );
    return r1 > 0 ? 100.0 * m.n( "gill", "t5" ) / r1 : 100.0;
  };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_a_depress() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( a_depress, 520, "a_depress.json", make_a_depress ) }
