// Micro-puzzle "w_switch" (worm chapter, concept "command_switch": mutual inhibition makes a winner-take-all choice). A worm cannot crawl forwards and backwards at once.
// Two sensors push the two command cells; when both are pushed at once exactly one command must win, the one with the stronger push.
//
// WORLD (5000 ms; Poisson sensors, default weight 1000 pA)
//   push_rev 150 Hz alone [400, 900)         push_fwd 150 Hz alone [1700, 2200)
//   conflict A [3000, 3500)  push_rev 150 Hz, push_fwd 60 Hz     conflict B [4000, 4500)  push_fwd 150 Hz, push_rev 60 Hz
// SCORING (windows start 100 ms after the push begins: the winner is decided)
//   solo_rev / solo_fwd : the pushed motor fires >= 20 Hz per cell (guided 13), the other is silent (<= 2 spikes)
//   conflictA / conflictB: the stronger push's motor has >= 85 % of the spikes (guided 78 %, researcher 90 %) and fires >= 20 Hz
// MECHANISM (Engine::structure()): the two motors read different cells, each push reaches its own motor by excitation, and each side inhibits the other
// (a cell on the way to one motor reaches the other motor by net inhibition, both ways).
// Biology: the forward command (AVB, PVC) and the backward command (AVA, AVD, AVE) cells of C. elegans work as a switch (Chalfie et al. 1985 J Neurosci 5:956);
// the mutual exclusion is a functional antagonism, not a single synapse in the wiring diagram.
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_w_switch();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "w_switch";
  i.title = "Worm: forward or backward";
  i.brief = "A worm cannot crawl forwards and backwards at the same time. Two sensors push two command cells: 'push_rev' pushes the one behind the motor 'reverse', "
            "'push_fwd' the one behind 'forward'. Pushed alone, each motor must answer and the other must stay quiet. Pushed together (one push stronger than "
            "the other) only the stronger command may win: the loser must be silenced, not just smaller. Plain cells cannot do that, because each simply "
            "follows its own push. Let the two commands compete.";
  i.chapter = "worm";
  i.sensors = { "push_rev", "push_fwd" };
  i.motors = { "reverse", "forward" };
  i.duration_ms = 5000;
  i.concepts_new = { "command_switch" };
  i.concepts_used = { "inhibition" };
  i.par_neurons = 2;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "push_rev", in_kind( "poisson", 150, 1000 ) }, { "push_fwd", in_kind( "poisson", 150, 1000 ) }, { "reverse", micro::rate() }, { "forward", micro::rate() } };
  i.bands = { { "reverse_hz", "Hz", 0, 400 }, { "forward_hz", "Hz", 0, 400 } };
  i.recipe = "Two integrate-and-fire cells R and F. push_rev > R and push_fwd > F at 1000 pA; R > F and F > R at -4000 pA (both cells inhibitory); R > 'reverse', "
             "F > 'forward'. When both are pushed the stronger one fires and the other goes quiet.";
  i.note = { "Forward or backward, never both",
    "C. elegans crawls forwards or backwards. The wiring diagram puts the two behaviours in two sets of command interneurons that are driven by different "
    "touch cells; the exclusive choice between them is a property of how the sets antagonise each other, which a circuit diagram alone does not show. "
    "Touch to the head normally ends in a reversal and touch to the tail in a forward run.",
    "Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) J Neurosci 5:956-964, doi:10.1523/JNEUROSCI.05-04-00956.1985" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 5000;
    c.segs = { { "push_rev", 400, 900, 150 }, { "push_fwd", 1700, 2200, 150 }, { "push_rev", 3000, 3500, 150 }, { "push_fwd", 3000, 3500, 60 },
      { "push_fwd", 4000, 4500, 150 }, { "push_rev", 4000, 4500, 60 } };
    c.wins = { { "solo_rev", 500, 900 }, { "solo_fwd", 1800, 2200 }, { "conflictA", 3100, 3500 }, { "conflictB", 4100, 4500 } };
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto r = g.out_pops( "reverse" ), f = g.out_pops( "forward" ), pr = g.in_pops( "push_rev" ), pf = g.in_pops( "push_fwd" );
    if ( r.empty() || f.empty() ) return "";
    if ( Graph::overlap( r, f ) ) return "'reverse' and 'forward' are read from the same cell: a switch has two sides, so each needs its own cell.";
    if ( !pr.empty() && !g.path( pr, r, +1, 3, true ) ) return "'push_rev' does not reach 'reverse' by excitation: connect it to the cell behind 'reverse'.";
    if ( !pf.empty() && !g.path( pf, f, +1, 3, true ) ) return "'push_fwd' does not reach 'forward' by excitation: connect it to the cell behind 'forward'.";
    if ( !g.crossed( r, f ) && !g.crossed( f, r ) )
      return "The two commands do not inhibit each other at all: a switch lets each command silence the other, so that only one can win.";
    if ( !g.crossed( r, f ) || !g.crossed( f, r ) )
      return "The inhibition runs one way only: the command that cannot silence its rival still fires when it is pushed. Each side must inhibit the other.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double rr = m.hz( "reverse", "solo_rev" ), ff = m.hz( "forward", "solo_fwd" );
    const double leak_r = m.n( "forward", "solo_rev" ), leak_f = m.n( "reverse", "solo_fwd" );
    c.push_back( { "solo_rev", rr >= m.lo( 20 ) && leak_r <= 2, ramp( rr, 0, 20 ) * ( leak_r <= 2 ? 1.0 : 0.5 ),
      rr < m.lo( 20 ) ? "'reverse' does not answer its own push: check that 'push_rev' reaches the cell behind it and is strong enough to make it fire."
                      : "'forward' fires when only 'push_rev' is pushed: the two commands must not share a drive." } );
    c.push_back( { "solo_fwd", ff >= m.lo( 20 ) && leak_f <= 2, ramp( ff, 0, 20 ) * ( leak_f <= 2 ? 1.0 : 0.5 ),
      ff < m.lo( 20 ) ? "'forward' does not answer its own push: check that 'push_fwd' reaches the cell behind it and is strong enough to make it fire."
                      : "'reverse' fires when only 'push_fwd' is pushed: the two commands must not share a drive." } );
    const double sa = m.share( "reverse", "forward", "conflictA" ), sb = m.share( "forward", "reverse", "conflictB" );
    const double need = 1.0 - 0.15 * m.tol;
    c.push_back( { "conflictA", sa >= need && m.hz( "reverse", "conflictA" ) >= m.lo( 20 ), ramp( sa, 0.5, need ),
      "With both pushed and 'reverse' pushed harder, the weaker command is still firing: a switch must silence the loser. Strengthen the mutual inhibition." } );
    c.push_back( { "conflictB", sb >= need && m.hz( "forward", "conflictB" ) >= m.lo( 20 ), ramp( sb, 0.5, need ),
      "With both pushed and 'forward' pushed harder, the weaker command is still firing: a switch must silence the loser. Strengthen the mutual inhibition." } );
    p[ "share_A" ] = sa;
    p[ "share_B" ] = sb;
  };
  s.ok_verdict = "A switch: each command answers its own push, and when both are pushed the stronger one wins and silences the other.";
  s.h1 = "A choice between two behaviours needs competition: each command must be able to silence the other, so that only one is ever active.";
  s.h2 = "Wire the two command cells to inhibit each other; each one has its own push and its own motor.";
  s.h2_target = "forward";
  s.h3 = "push_rev to cell R and push_fwd to cell F; R inhibits F and F inhibits R (both set inhibitory); R reads as 'reverse' and F as 'forward'. If the loser still fires, "
         "strengthen the mutual inhibition; if a command does not fire alone, strengthen its push.";
  s.metric_name = "winner_share_pct";
  s.metric_unit = "%";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return 100.0 * std::min( m.share( "reverse", "forward", "conflictA" ), m.share( "forward", "reverse", "conflictB" ) ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_w_switch() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( w_switch, 501, "w_switch.json", make_w_switch ) }
