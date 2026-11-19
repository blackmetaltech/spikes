// Task "d_switch" (dynamics chapter): a single neuron as a switch. At a steady holding current the cell has two stable states at once, rest (a stable fixed point) and repetitive firing (a
// limit cycle). A brief pulse that carries the trajectory across the separatrix between them turns it on for good, and a brief pulse the other way turns it off: one cell, one bit.
//
// WORLD (3600 ms; dc sensors 'hold' (level 1 all the time), 'set' (a 50 ms pulse up at 400 and at 2000 ms) and 'reset' (a 50 ms pulse DOWN at 1200 and at 2800 ms: the sensor itself carries a negative current, so the weight stays positive); weights = the player's)
//   windows: quiet [100, 400), on1 [700, 1100), off1 [1500, 1900), on2 [2300, 2700), off2 [3100, 3500)
// SCORING
//   quiet   no spike before the first pulse
//   on      at least 6 spikes (guided 4, researcher 8) in each 'on' window: switched on, and stays on long after the pulse
//   off     no spike in each 'off' window: switched off, and stays off
// MECHANISM (the cell's phase plane at the holding current, and the pulses' signs): a two-variable cell with a stable fixed point at the holding current, and all three sensors reaching it.
// The behaviour decides the rest: only a second, firing attractor makes the 'on' windows pass.
// Biology: the Izhikevich (2003) IEEE Trans Neural Netw 14:1569 cell with a high reset has a stable rest and a stable firing state side by side; Touboul J, Brette R (2008) Biol Cybern 99:319 analyse how
// the reset interacts with the differential equations of the adaptive exponential model. Persistent firing of single real cells (plateau potentials) is a different, channel-based mechanism and is not modelled.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_d_switch();
namespace
{
using namespace bestiary;
using namespace tl;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "d_switch";
  i.title = "A neuron that remembers a pulse";
  i.brief = "A neuron can have two stable states at once: quiet at rest, and firing on and on. In the phase plane the resting point is a stable crossing of the two nullclines, and the firing is a loop "
            "around it that the trajectory can also settle into. Between them runs a border, and a brief pulse that carries the cell across it flips the state for good. The sensor 'hold' is a "
            "steady current, 'set' is a short pulse and 'reset' a short pulse too. Build a single cell that is quiet at first, fires on and on after 'set' (long after the pulse is gone), and is quiet "
            "again and stays so after 'reset'; the same must work a second time. 'hold' puts the cell near the edge of firing without starting it; 'set' is a pulse that pushes the cell up and 'reset' a pulse that pushes it down (give both a positive weight: the sensor itself is the push). "
            "What makes firing survive after a spike is where the spike resets the cell, relative to the border between the states.";
  i.chapter = "dynamics";
  i.view = "phase";
  i.sensors = { "hold", "set", "reset" };
  i.motors = { "spikes" };
  i.duration_ms = 3600;
  i.concepts_new = { "bistability" };
  i.concepts_used = { "excitability_class", "stable_fixed_point" };
  i.par_neurons = 1;
  i.default_model = "izhikevich";
  i.kinds = { { "hold", in_kind( "dc", 1.0, 2.5 ) }, { "set", in_kind( "dc", 1.0, 10.0 ) }, { "reset", in_kind( "dc", 1.0, 10.0 ) }, { "spikes", micro::rate() } };
  i.bands = { { "on_spikes", "spikes", 0, 200 } };
  s.gauges = { { "on_spikes", { "on1_spikes" } } };
  i.recipe = "One Izhikevich cell: a 0.02, b 0.2, c -50, d 0 (the reset lands above the border between rest and firing, and nothing is added to the recovery variable, so the cell keeps firing). "
             "hold > cell at weight 2.5, set > cell at 10, reset > cell at 10 (a downward pulse). Holding currents from about 1.5 to 3.5 work; 4 and above fires on its own.";
  i.note = { "One neuron, two states",
    "A model neuron can sit near the edge of firing with a stable resting state and a stable firing state side by side. Which one it is in depends on its history: a brief pulse that crosses the border "
    "between their basins switches it, and it stays there. In two-variable models with a reset this comes from where the spike resets the cell; the reset interacts with the differential equations "
    "and gives these models a rich structure of states.",
    "Izhikevich EM (2003) IEEE Trans Neural Netw 14:1569-1572, doi:10.1109/TNN.2003.820440; Touboul J, Brette R (2008) Biol Cybern 99:319-334, doi:10.1007/s00422-008-0267-4" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 3600;
    c.segs.push_back( { "hold", 0, 3600, 1.0 } );
    for ( double t : { 400.0, 2000.0 } ) c.segs.push_back( { "set", t, t + 50, 1.0 } );
    for ( double t : { 1200.0, 2800.0 } ) c.segs.push_back( { "reset", t, t + 50, 1.0 } );
    c.wins.push_back( { "quiet", 100, 400 } );
    c.wins.push_back( { "on1", 700, 1100 } );
    c.wins.push_back( { "off1", 1500, 1900 } );
    c.wins.push_back( { "on2", 2300, 2700 } );
    c.wins.push_back( { "off2", 3100, 3500 } );
    return c;
  };
  s.start = []( Engine& e, std::map< std::string, double >& scale ) {
    rest_state( e );
    scale[ "reset" ] = -1.0;   // 'reset' is a pulse DOWN: the sensor's level is negative, the weight the player sets is positive
  };
  s.mech_e = []( const Graph&, Engine& e ) -> std::string {
    const Cell c = cell_of( e, "spikes" );
    if ( !c.ok() ) return "";
    if ( !theory::phase_plane_supported( c.model ) ) return need_two_variables();
    const double hold = dc_weight( e, c.n, "hold" ), set = dc_weight( e, c.n, "set" ), reset = dc_weight( e, c.n, "reset" );
    if ( hold == 0 || set == 0 || reset == 0 ) return "'hold', 'set' and 'reset' all have to reach the cell that 'spikes' reads.";
    if ( set < 0 || reset < 0 ) return "'set' and 'reset' need a positive weight: the 'set' sensor pushes the cell up and the 'reset' sensor pushes it down by itself.";
    const theory::PhasePlane pp = theory::phase_plane( c.model, c.params, hold );
    for ( const auto& f : pp.fixed )
      if ( f.stable ) return "";
    return "At the holding current this cell has no stable resting state to be switched back to (the nullclines do not cross at a stable point in the phase plane), so it fires on its own and 'reset' "
           "has nothing to return it to. Lower the holding current, or move the nullclines so that rest survives it.";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double q = m.n( "spikes", "quiet" ), on1 = m.n( "spikes", "on1" ), off1 = m.n( "spikes", "off1" ), on2 = m.n( "spikes", "on2" ), off2 = m.n( "spikes", "off2" );
    const double need = m.tol > 1.2 ? 4.0 : ( m.tol < 0.9 ? 8.0 : 6.0 );
    p[ "on1_spikes" ] = on1;
    p[ "off1_spikes" ] = off1;
    p[ "on2_spikes" ] = on2;
    p[ "off2_spikes" ] = off2;
    p[ "quiet_spikes" ] = q;
    c.push_back( { "quiet", q == 0, 1.0 - ramp( q, 0, 10 ), "The cell fires before anything has switched it on: with only the holding current it has to rest quietly." } );
    c.push_back( { "on1", on1 >= need, ramp( on1, 0, need ),
      "The 'set' pulse does not switch the cell on for good: it is quiet again soon after the pulse (or never starts). A switch needs a second stable state, repetitive firing, that the pulse carries the cell "
      "into and that it then stays in. Look at where the spike resets the cell." } );
    c.push_back( { "off1", off1 == 0, 1.0 - ramp( off1, 0, 20 ), "The 'reset' pulse does not switch the cell off: it is still firing long after the pulse. The pulse has to carry the trajectory back across the border to the resting state." } );
    c.push_back( { "on2", on2 >= need, ramp( on2, 0, need ), "The cell switched on once, but a second 'set' pulse does not switch it on again: the resting state must flip to firing every time." } );
    c.push_back( { "off2", off2 == 0, 1.0 - ramp( off2, 0, 20 ), "The cell switched off once, but a second 'reset' pulse does not switch it off again." } );
  };
  s.ok_verdict = "A one-cell switch: quiet at the holding current, firing on after a brief 'set' pulse and quiet again after a brief 'reset' pulse, twice. The cell has two stable states, rest and firing.";
  s.h1 = "The same current can leave a neuron with two stable states: rest and repetitive firing. A pulse that crosses the border between them flips the state, and it stays flipped.";
  s.h2 = "Look at where the spike resets the cell in the phase plane. If the reset lands on the rest side of the border the cell falls back to rest; if it lands on the firing side it goes round again.";
  s.h2_target = "spikes";
  s.h3 = "Use a two-variable cell. Reset the voltage to a high value and add little or nothing to the recovery variable at a spike, and hold the cell just below the current at which it fires by itself. "
         "Then 'set' (positive) and 'reset' (negative) have to be big enough to cross the border, and no bigger. If the cell never turns off, hold it a little lower; if it never turns on, reset it higher.";
  s.metric_name = "on_spikes";
  s.metric_unit = "spikes";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return 0.5 * ( m.n( "spikes", "on1" ) + m.n( "spikes", "on2" ) ); };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_d_switch() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( d_switch, 602, "d_switch.json", make_d_switch ) }
