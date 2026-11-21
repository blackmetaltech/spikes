// Micro-puzzle "w_latch" (worm chapter, concept "persistence": positive feedback holds a state). A touch makes the worm back up, and it keeps backing up after the
// touch has ended until a 'stop' cue arrives: the command cells of C. elegans (AVA) stay active for seconds after a brief touch.
//
// WORLD (4000 ms; Poisson sensors, 150 Hz while on, default sensor weight 1000 pA: one input spike makes a cell fire once)
//   touch  on [400, 600)      stop  on [2400, 2700)
// SCORING (rate of the 'reverse' pool, Hz per cell)
//   pre   [0, 400)      silent         <= 3 Hz (guided 4.5)       a cell that fires on its own is no latch
//   hold  [900, 2300)   keeps firing   >= 15 Hz (guided 10)        300 ms after the touch is over, until just before 'stop'
//   post  [3100, 4000)  silent again   <= 3 Hz                     'stop' must end it
// MECHANISM (Engine::structure()): touch reaches 'reverse' by excitation; the cells on that path lie on an excitatory loop (positive feedback); 'stop' reaches
// 'reverse' by net inhibition.
// Biology: a brief anterior touch evokes a reversal that outlasts the stimulus; the backward command cells (AVA, AVE, AVD) hold it (Chalfie et al. 1985
// J Neurosci 5:956; Gray, Hill & Bargmann 2005 PNAS 102:3184). Persistence here is recurrent excitation; in the animal electrical coupling and neuromodulators
// contribute as well, which this level does not model.
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_w_latch();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "w_latch";
  i.title = "Worm: holding a reversal";
  i.brief = "A worm that is touched on the head backs up, and keeps backing up for a while after the touch is over. The cells that command the "
            "backward crawl hold their state. Build that memory: the sensor 'touch' must switch 'reverse' on, 'reverse' must stay on after the touch ends, "
            "and the sensor 'stop' must switch it off again. Before the touch and after the stop the motor must be quiet. A cell that only relays the "
            "touch stops the moment the touch stops; to hold a state, cells must keep each other (or themselves) firing. And 'stop' needs a cell that "
            "inhibits, because a sensor can only excite.";
  i.chapter = "worm";
  i.sensors = { "touch", "stop" };
  i.motors = { "reverse" };
  i.duration_ms = 4000;
  i.concepts_new = { "persistence" };
  i.concepts_used = { "excitation", "inhibition" };
  i.par_neurons = 3;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "touch", in_kind( "poisson", 150, 1000 ) }, { "stop", in_kind( "poisson", 150, 1000 ) }, { "reverse", micro::rate() } };
  i.bands = { { "reverse_hz", "Hz", 15, 400 } };
  i.recipe = "Two cells A and B that excite each other (A > B and B > A, +1500 pA, delay 4 ms), both integrate-and-fire; touch > A at the default 1000 pA; "
             "a third cell S, inhibitory, with stop > S at 1000 pA and S > A and S > B at -1500 pA; 'reverse' reads B. After the touch the pair keeps "
             "firing at a few hundred Hz.";
  i.note = { "A reversal that outlasts the touch",
    "A gentle touch on the head of C. elegans makes it back away, and the backward movement continues for a while after the stimulus is gone. The cells "
    "that carry the command, the AVA interneurons and their partners, are coupled to each other and to the cells that excite them, so a brief input can "
    "start a state that sustains itself. In this level the memory is recurrent excitation; the real worm also uses gap junctions and neuromodulators.",
    "Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) J Neurosci 5:956-964, doi:10.1523/JNEUROSCI.05-04-00956.1985" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 4000;
    c.segs = { { "touch", 400, 600, 150 }, { "stop", 2400, 2700, 150 } };
    c.wins = { { "pre", 0, 400 }, { "hold", 900, 2300 }, { "post", 3100, 4000 } };
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto touch = g.in_pops( "touch" ), stop = g.in_pops( "stop" ), rev = g.out_pops( "reverse" );
    if ( touch.empty() || rev.empty() ) return "";
    if ( !g.path( touch, rev, +1, 4, true ) )
      return "The touch never reaches 'reverse' by excitation: connect the touch sensor to a cell that excites the cells behind 'reverse'.";
    mech::PopSet on_path;
    for ( int p = 0; p < static_cast< int >( g.st.populations.size() ); ++p )
      if ( g.path( touch, { p }, +1, 4, true ) && g.path( { p }, rev, +1, 4, true ) ) on_path.insert( p );
    if ( !mech::excitatory_loop( g.st, on_path, true ) )
      return "Nothing keeps the command going once the touch is over: the cells that drive 'reverse' must excite themselves or each other (positive feedback), "
             "otherwise the reversal ends with the touch.";
    if ( !stop.empty() && !g.path( stop, rev, -1, 4 ) )
      return "'stop' does not reach the held state by inhibition: a sensor can only excite, so send it through a cell that inhibits the cells holding the reversal.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double pre = m.hz( "reverse", "pre" ), hold = m.hz( "reverse", "hold" ), post = m.hz( "reverse", "post" );
    c.push_back( { "pre", pre <= m.hi( 3 ), 1.0 - ramp( pre, 3, 30 ), "'reverse' fires before any touch: a cell that is active on its own cannot remember anything. Lower its drive or its input." } );
    c.push_back( { "hold", hold >= m.lo( 15 ), ramp( hold, 0, 15 ), hold < 1 ? ( m.sum( "reverse", 400, 900 ) < 1
        ? "The touch never makes the cells behind 'reverse' fire: nothing can be held that never starts. Give them more input from the touch (a stronger touch weight) or a steady drive."
        : "'reverse' stops as soon as the touch stops. The cells already excite each other, but not strongly enough to keep each other going: let them excite each other more strongly (stronger feedback weights, or more cells in the loop)." ) : "The reversal fades after the touch: the positive feedback is too weak to keep the cells firing; strengthen it." } );
    c.push_back( { "post", post <= m.hi( 3 ), 1.0 - ramp( post, 3, 30 ), "'reverse' never switches off after 'stop': the inhibition must be strong enough to silence the cells that hold the state." } );
    p[ "hold_over_pre" ] = hold - pre;
  };
  s.ok_verdict = "A held state: the touch switches the reversal on, it outlasts the touch, and the stop switches it off.";
  s.h1 = "A memory in a circuit is positive feedback: cells that excite each other keep each other firing after the input that started them is gone.";
  s.h2 = "Make two cells excite each other and read 'reverse' from one of them; give 'stop' its own inhibitory cell.";
  s.h2_target = "reverse";
  s.h3 = "touch to cell A; A and B excite each other; 'reverse' reads B. stop to an inhibitory cell that silences both A and B. If the reversal never starts, strengthen the "
         "link from the touch; if it never ends, strengthen the inhibition; if it fires before the touch, the loop is too strong for the input.";
  s.metric_name = "hold_hz";
  s.metric_unit = "Hz";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return m.hz( "reverse", "hold" ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_w_latch() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( w_latch, 500, "w_latch.json", make_w_latch ) }
