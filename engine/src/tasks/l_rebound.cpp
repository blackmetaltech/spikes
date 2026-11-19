// Micro-puzzle "l_rebound" (leech chapter, concept "rebound": a cell that is silenced fires when the inhibition is released). The leech half-centre switches sides by
// release: the silenced cell recovers from inhibition with a burst of its own. Here an inhibitory cell silences a target cell for a while; the target must stay quiet
// during the inhibition AND fire when the inhibition ends.
//
// WORLD (4000 ms; Poisson sensor 'inhibit', 300 Hz while on, default weight 1000 pA onto an inhibitory cell, which then fires about 280 Hz: the inhibition is smooth)
//   inhibit on [800, 1800) and [2800, 3300)
// SCORING ('rebound' pool, spikes)
//   quiet   [100, 800) and [1000, 1800) and [2000, 2800)    silent (<= 1 spike): no spontaneous firing, silent while inhibited, silent between the pulses
//   reb1    [1800, 2000)   >= 2 spikes (guided 1)          the cell fires when the first pulse ends
//   reb2    [3300, 3500)   >= 2 spikes (guided 1)          and the second
// MECHANISM: 'inhibit' reaches the target by net inhibition (it must be silenced by an inhibitory cell, not just left without drive); the target is a cell type that can rebound.
// In this engine that is the adaptive exponential cell with strong sub-threshold adaptation (a): hyperpolarisation drives its adaptation variable negative, and when the
// inhibition ends that variable is a depolarising current for a while (anode-break excitation, as in the Hodgkin-Huxley cell). Integrate-and-fire cells only return to rest.
// (The Izhikevich and Hodgkin-Huxley cells of the parts bin did not give a clean rebound with spike-train inhibition in the tuning of this level.)
// Biology: post-inhibitory rebound drives the alternation of the leech heartbeat half-centres through the hyperpolarisation-activated current and a low-threshold calcium
// current (Nadim et al. 1995 J Comput Neurosci 2:215; Olsen, Nadim & Calabrese 1995 J Comput Neurosci 2:237). The level's cell is a generic rebounding cell.
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_l_rebound();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "l_rebound";
  i.title = "Leech: rebound after inhibition";
  i.brief = "One half of the leech heartbeat oscillator is held silent by its partner; when the partner lets go, the silenced cell does not just return to rest, it fires. "
            "Build that: the sensor 'inhibit' drives an inhibitory cell, which silences a target cell that you read on 'rebound'. The target must stay quiet before "
            "the inhibition, quiet during it and quiet between the two pulses, and it must fire right after each pulse ends. Most cells cannot: an integrate-and-fire cell "
            "inhibited for a while simply returns to rest. A cell that rebounds has a slow variable that the inhibition pushes in the 'wrong' direction, so that it "
            "overshoots when the inhibition ends. The target also needs a steady drive of its own, a bias that holds it just below firing: a cell sitting far "
            "from threshold has nothing to rebound to.";
  i.chapter = "leech";
  i.sensors = { "inhibit" };
  i.motors = { "rebound" };
  i.duration_ms = 4000;
  i.concepts_new = { "rebound" };
  i.concepts_used = { "inhibition", "adaptation" };
  i.par_neurons = 2;
  i.default_model = "aeif_cond_alpha";
  i.kinds = { { "inhibit", in_kind( "poisson", 300, 1000 ) }, { "rebound", micro::rate() } };
  i.bands = { { "rebound_hz", "Hz", 0, 400 } };
  i.recipe = "Two cells. S: an integrate-and-fire cell set inhibitory, inhibit > S at the default 1000 pA; T: an adaptive exponential cell with a = 20 nS, b = 0, tau_w = 200 ms and a bias current "
             "I_e = 580 pA (just below its rheobase, so it is silent on its own); S > T at -400 nS; T > 'rebound'. T is silent during the pulses and fires a short burst of three or four spikes "
             "when each pulse ends.";
  i.note = { "Escape and release",
    "In the leech heartbeat oscillator two interneurons inhibit each other. When one stops firing, the other is released from inhibition, and its own slow currents, a "
    "hyperpolarisation-activated current and a low-threshold calcium current, help it to fire strongly just after release. The modelling papers distinguish a "
    "'release' mode, in which the switch comes when the active cell tires, from an 'escape' mode, in which the silenced cell escapes the inhibition while the other is still active.",
    "Nadim F, Olsen OH, De Schutter E, Calabrese RL (1995) J Comput Neurosci 2:215-235, doi:10.1007/BF00961435; Olsen OH, Nadim F, Calabrese RL (1995) J Comput Neurosci 2:237-257, doi:10.1007/BF00961436" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 4000;
    c.segs = { { "inhibit", 800, 1800, 300 }, { "inhibit", 2800, 3300, 300 } };
    c.wins = { { "q1", 100, 800 }, { "q2", 1000, 1800 }, { "q3", 2000, 2800 }, { "reb1", 1800, 2000 }, { "reb2", 3300, 3500 } };
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto in = g.in_pops( "inhibit" ), t = g.out_pops( "rebound" );
    if ( t.empty() ) return "";
    if ( !in.empty() && !g.path( in, t, -1, 3 ) )
      return "'inhibit' does not reach the target by inhibition: the target must be silenced by a cell that inhibits it (the sensor itself can only excite).";
    for ( int p : t )
    {
      const std::string& m = g.pop( p ).model;
      if ( is_one_of( m, { "iaf_psc_alpha", "iaf_psc_exp", "iaf_psc_delta", "iaf_cond_alpha", "iaf_cond_exp", "parrot_neuron" } ) )
        return "The target is a plain integrate-and-fire cell: after inhibition it only returns to rest and never overshoots. A cell that rebounds needs a slow variable that the "
               "inhibition pushes the other way: choose a cell type that has one (the adaptive exponential cell, with its sub-threshold adaptation turned up).";
      if ( m.rfind( "aeif", 0 ) == 0 )
      {
        const double a = g.param( p, "a" );
        if ( std::isnan( a ) || a < 10 )
          return "The target's slow variable barely responds to the voltage below threshold, so inhibition does not push it anywhere: raise the sub-threshold adaptation of the cell (a), "
                 "and give the cell a bias so that it sits just below firing.";
      }
    }
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double q = m.n( "rebound", "q1" ) + m.n( "rebound", "q2" ) + m.n( "rebound", "q3" );
    const double r1 = m.n( "rebound", "reb1" ), r2 = m.n( "rebound", "reb2" );
    p[ "rebound_spikes" ] = r1 + r2;
    p[ "quiet_spikes" ] = q;
    c.push_back( { "quiet", q <= 1, 1.0 - ramp( q, 1, 20 ), "The target fires when it should be quiet (before the first pulse, during the inhibition or between the pulses). It must be silent until the inhibition is released: "
                                                          "check that nothing else drives it, and that the inhibition is strong enough to hold it." } );
    const double need = m.tol > 1.2 ? 1 : 2;
    c.push_back( { "reb1", r1 >= need, ramp( r1, 0, need ),
      r1 < 0.5 ? "The target does not fire when the first pulse ends: it only returns to rest. It needs a cell type that overshoots after inhibition, a bias that holds it just below firing, and an inhibition strong and long enough to push its slow variable."
               : "The target fires too weakly when the first pulse ends: a rebound is a short burst, not a single spike. Push its slow variable further with stronger or longer inhibition." } );
    c.push_back( { "reb2", r2 >= need, ramp( r2, 0, need ),
      r2 < 0.5 ? "The target does not fire when the second pulse ends: the rebound must come after every release."
               : "The rebound after the second pulse is too weak: a short burst is needed after every release, not a single spike." } );
  };
  s.ok_verdict = "A rebound: the target is silent while it is inhibited and fires as soon as the inhibition is released.";
  s.h1 = "A rebound needs a slow variable in the cell that the inhibition drives past its resting value, so that the cell overshoots when the inhibition ends.";
  s.h2 = "The sensor reaches the target through an inhibitory cell; give the target a cell type that can rebound, and a little bias current so that it sits just below firing.";
  s.h2_target = "rebound";
  s.h3 = "inhibit to an inhibitory cell that silences the target; the target is an adaptive exponential cell with strong sub-threshold adaptation (a), no spike-triggered adaptation (b zero) and a bias current "
         "just below its firing threshold, read on 'rebound'. If the target never fires after the pulses, strengthen the inhibition or raise the bias a little; if it fires on its own or during the pulses, lower the bias.";
  s.metric_name = "rebound_spikes";
  s.metric_unit = "spikes";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return m.n( "rebound", "reb1" ) + m.n( "rebound", "reb2" ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_l_rebound() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( l_rebound, 511, "l_rebound.json", make_l_rebound ) }
