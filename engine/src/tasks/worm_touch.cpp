// Task "worm_touch" (worm chapter): touch on the head makes the worm reverse, touch on the tail makes it go forward, and the choice is held by two command groups that
// inhibit each other (the AVA / AVB switch of C. elegans). Wired, in the reference circuit, from the real touch circuit (tools/connectome/worm.py, Cook et al. 2019).
//
// WORLD (6000 ms; Poisson sensors, default weight 1000 pA, so a touch cell fires once per input spike; every event is a touch of 400 ms)
//   E1 head alone      [ 500,  900)   touch_head 150 Hz
//   E2 tail alone      [2000, 2400)   touch_tail 150 Hz
//   E3 head and tail   [3500, 3900)   touch_head 150 Hz, touch_tail 40 Hz    (the head touch is stronger: the worm must reverse)
//   E4 tail and head   [5000, 5400)   touch_tail 150 Hz, touch_head 40 Hz    (the tail touch is stronger: the worm must go forward)
//   Motors 'reverse' (the A-type motor neurons) and 'forward' (the B-type motor neurons): rate of the pool, Hz per cell.
// SCORING (a window opens 150 ms after the touch begins and closes 450 ms after it ends, so the command has to be decided and then HELD)
//   quiet  [0, 400)   both motors silent (<= 3 Hz)
//   each of E1-E4: the right motor fires >= 15 Hz per cell (guided 10) and has >= 90 % of the spikes of the two motors (guided 82 %, researcher 94 %)
//   value 100 when passed, else 85 * (criteria met / 5).
// MECHANISM (Engine::structure()): the head touch reaches 'reverse' and the tail touch 'forward' by excitation; each side (the cells on the way to its motor) holds its
// state by recurrent excitation (the bilateral partners of a command pair excite each other); the two sides inhibit each other (a cell on the reverse path reaches
// 'forward' by net inhibition and the other way round); the motors are read from different cells.
//
// Biology: Chalfie et al. 1985 J Neurosci 5:956 (touch circuit: ALM, AVM, PLM; command interneurons AVD, AVA, AVB, PVC); the wiring is Cook et al. 2019 Nature 571:63.
// The reference uses spiking cells; the real cells of this circuit signal mostly with graded potentials (what-is-real, docs/WHAT-IS-REAL.md).
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_worm_touch();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "worm_touch";
  i.title = "Worm: touch, reverse or go";
  i.brief = "A touch on the head makes a worm back up; a touch on the tail makes it go forward. Two sensors, 'touch_head' and 'touch_tail', and two motors, 'reverse' "
            "(the backward-crawl motor neurons) and 'forward' (the forward-crawl ones). Between them sit the command cells: a backward command and a forward command "
            "that are driven by their own touch cells, that each keep going after the touch has ended, and that silence each other, so that the worm is always doing "
            "one thing. Each touch lasts a short while; the right motor must be firing in the window around it and the wrong one must be (nearly) silent, "
            "including when BOTH ends are touched at once (the stronger touch wins) and while the worm is still in the other state from the touch before. "
            "The real circuit: three kinds of touch cell (ALM and AVM in the head half, PLM in the tail), the interneurons AVD, AVA (backward) and PVC, AVB (forward), "
            "and the A-type and B-type motor neurons. The reference circuit of this level is wired from the published wiring diagram (synapse counts); "
            "the real cells of this circuit signal mostly with graded potentials, which this game approximates with spikes.";
  i.chapter = "worm";
  i.sensors = { "touch_head", "touch_tail" };
  i.motors = { "reverse", "forward" };
  i.duration_ms = 6000;
  i.concepts_new = {};
  i.concepts_used = { "persistence", "command_switch", "excitation", "inhibition" };
  i.par_neurons = 17;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "touch_head", in_kind( "poisson", 150, 1000 ) }, { "touch_tail", in_kind( "poisson", 150, 1000 ) }, { "reverse", micro::rate() }, { "forward", micro::rate() } };
  i.bands = { { "reverse_hz", "Hz", 15, 600 }, { "forward_hz", "Hz", 15, 600 } };
  i.recipe = "The reference (circuits/worm_touch.json, made by tools/connectome/worm.py from the Cook et al. 2019 synapse counts) has 17 integrate-and-fire cells: ALM (2), AVM (1), PLM (2), AVD (2), AVA (2), "
             "PVC (2), AVB (2), MA (2) and MB (2). Every connection of the wiring diagram has the weight 25 pA per synapse (cell pair by cell pair, left and right cells separate); the touch inputs of the "
             "command cells have few synapses, so those cells are made more excitable (a lower threshold, V_th between -69.4 and -63.4 mV; the command cells also get a refractory period of 10 ms). "
             "The two members of each command pair (AVA, AVB) excite each other, which holds the state; every AVA cell inhibits every AVB cell and the other way round (-2000 pA).";
  i.note = { "Forward and backward in the worm's nerve ring",
    "C. elegans has six touch receptor neurons, five pairs of interneurons and 69 motor neurons in the touch circuit that Chalfie and colleagues mapped and "
    "tested by killing cells with a laser. Anterior touch leads to a reversal through the AVD cells, and posterior touch to a forward run through PVC. Backward and forward "
    "locomotion are driven by two sets of command interneurons that act as a switch. The synapse counts of the whole animal were later updated by electron microscopy "
    "(Cook et al. 2019). In this level the wiring is from that data set, the signs of the synapses and the mutual inhibition are assumptions, and the spiking cells stand "
    "in for neurons that mostly signal with graded potentials.",
    "Chalfie M, Sulston JE, White JG, Southgate E, Thomson JN, Brenner S (1985) J Neurosci 5:956-964, doi:10.1523/JNEUROSCI.05-04-00956.1985; "
    "Cook SJ et al. (2019) Nature 571:63-71, doi:10.1038/s41586-019-1352-7" };
  s.schedule = [ & ]( bool quick ) {
    Schedule c;
    const double t1 = quick ? 400 : 500, gap = quick ? 1100 : 1500;
    c.dur = t1 + 3 * gap + 400 + 450 + 50;
    const double a = t1, b = t1 + gap, d = t1 + 2 * gap, e = t1 + 3 * gap;
    c.segs = { { "touch_head", a, a + 400, 150 }, { "touch_tail", b, b + 400, 150 }, { "touch_head", d, d + 400, 150 }, { "touch_tail", d, d + 400, 40 },
      { "touch_tail", e, e + 400, 150 }, { "touch_head", e, e + 400, 40 } };
    c.wins = { { "quiet", 0, t1 - 100 }, { "E1", a + 150, a + 850 }, { "E2", b + 150, b + 850 }, { "E3", d + 150, d + 850 }, { "E4", e + 150, e + 850 } };
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto r = g.out_pops( "reverse" ), f = g.out_pops( "forward" ), th = g.in_pops( "touch_head" ), tt = g.in_pops( "touch_tail" );
    if ( r.empty() || f.empty() ) return "";
    if ( Graph::overlap( r, f ) ) return "'reverse' and 'forward' are read from the same cells: the two motors belong to two different groups of motor neurons.";
    if ( !th.empty() && !g.path( th, r, +1, 5, true ) ) return "The head touch does not reach 'reverse' by excitation: the touch cells must drive command cells that drive the backward motor neurons.";
    if ( !tt.empty() && !g.path( tt, f, +1, 5, true ) ) return "The tail touch does not reach 'forward' by excitation: the touch cells must drive command cells that drive the forward motor neurons.";
    // the cells on each side's way to its motor
    mech::PopSet rev_side, fwd_side;
    for ( int p = 0; p < static_cast< int >( g.st.populations.size() ); ++p )
    {
      if ( g.path( th, { p }, +1, 5, true ) && g.path( { p }, r, +1, 5, true ) ) rev_side.insert( p );
      if ( g.path( tt, { p }, +1, 5, true ) && g.path( { p }, f, +1, 5, true ) ) fwd_side.insert( p );
    }
    if ( !mech::excitatory_loop( g.st, rev_side ) )
      return "The backward command does not hold itself: the cells on the way to 'reverse' must excite each other (the two sides of a command pair do), "
             "otherwise the reversal ends as soon as the touch does.";
    if ( !mech::excitatory_loop( g.st, fwd_side ) )
      return "The forward command does not hold itself: the cells on the way to 'forward' must excite each other, otherwise the forward run ends as soon as the touch does.";
    if ( !g.crossed( r, f ) && !g.crossed( f, r ) )
      return "The backward and the forward command do not inhibit each other at all: a worm that is touched at both ends would try to do both. Each command must silence the other.";
    if ( !g.crossed( r, f ) || !g.crossed( f, r ) )
      return "The inhibition between the two commands runs one way only: the command that cannot be silenced keeps firing when the other wins. Each must inhibit the other.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double qr = m.hz( "reverse", "quiet" ), qf = m.hz( "forward", "quiet" );
    c.push_back( { "quiet", qr <= m.hi( 3 ) && qf <= m.hi( 3 ), 1.0 - ramp( std::max( qr, qf ), 3, 40 ),
      "A motor is firing before any touch: the command cells must be silent until a touch drives them. Lower the excitability of the cells or their drive." } );
    struct E
    {
      const char* key;
      const char* win;
      const char* right;
      const char* wrong;
      const char* what;
    };
    const E es[] = { { "E1", "E1", "reverse", "forward", "A touch on the head alone" }, { "E2", "E2", "forward", "reverse", "A touch on the tail alone" },
      { "E3", "E3", "reverse", "forward", "A strong head touch with a weak tail touch" }, { "E4", "E4", "forward", "reverse", "A strong tail touch with a weak head touch" } };
    const double need_share = 1.0 - 0.10 * m.tol;
    for ( const E& e : es )
    {
      const double hz = m.hz( e.right, e.win ), sh = m.share( e.right, e.wrong, e.win );
      std::string fail;
      if ( hz < m.lo( 15 ) && m.n( e.wrong, e.win ) >= m.n( e.right, e.win ) )
        fail = std::string( e.what ) + " does not make the " + e.right + " motor fire (and the other one fires instead or as well): check the path from that touch to its own command cells and motor.";
      else if ( hz < m.lo( 15 ) )
        fail = std::string( e.what ) + " does not keep the " + e.right + " motor going: the command must be driven strongly enough and hold itself until the other command takes over.";
      else
        fail = std::string( e.what ) + ": the " + e.wrong + " motor is still firing alongside. The two commands must silence each other so that only one is active; the stronger touch must win.";
      c.push_back( { e.key, hz >= m.lo( 15 ) && sh >= need_share, 0.5 * ramp( hz, 0, 15 ) + 0.5 * ramp( sh, 0.5, need_share ), fail } );
      p[ std::string( "share_" ) + e.key ] = sh;
    }
  };
  s.ok_verdict = "A command switch: each touch drives its own command, the command outlasts the touch, and when both ends are touched the stronger touch wins and silences the other.";
  s.h1 = "Two behaviours that exclude each other need two command groups that inhibit each other, and each command has to keep itself going once the touch is over.";
  s.h2 = "Look at the cells that sit between the touch cells and the motors: the backward pair and the forward pair. Each cell of a pair excites its partner, and each side inhibits the other side.";
  s.h2_target = "reverse";
  s.h3 = "touch_head to the head cells, which drive a backward command pair, which drives 'reverse'; touch_tail to the tail cells, which drive a forward command pair, which drives "
         "'forward'. The two members of each command pair excite each other. Every backward command cell inhibits the forward command cells and the other way round. If both motors fire "
         "together, strengthen the mutual inhibition; if a state ends with its touch, strengthen the excitation inside the command pair; if a command never starts, lower its threshold or "
         "strengthen its touch input.";
  s.h3_target = "forward";
  s.metric_name = "worst_winner_share_pct";
  s.metric_unit = "%";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) {
    return 100.0 * std::min( std::min( m.share( "reverse", "forward", "E1" ), m.share( "forward", "reverse", "E2" ) ),
      std::min( m.share( "reverse", "forward", "E3" ), m.share( "forward", "reverse", "E4" ) ) );
  };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_worm_touch() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( worm_touch, 502, "worm_touch.json", make_worm_touch ) }
