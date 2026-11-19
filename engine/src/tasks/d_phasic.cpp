// Micro-puzzle "d_phasic" (dynamics chapter, concept "stable_fixed_point": a resting state that survives the input). A step of current moves the voltage nullcline up. A tonic cell
// then has no stable crossing left and runs round the limit cycle for as long as the current is on. A phasic cell keeps a stable crossing at the new current: the step knocks it across
// threshold once (or a few times), the trajectory spirals into the new fixed point and the cell falls silent although the current stays on. It reports change, not level.
//
// WORLD (2 s; dc sensor 'current', weight = the player's): rest until 400 ms, a step of current (level 1 x weight) from 400 to 1600 ms, rest again.
// SCORING
//   quiet   no spike in [100, 400)                          the cell rests without input
//   onset   at least one spike in [400, 650)                the step is noticed
//   late    no spike in [900, 1600)                         ... and the cell is silent again while the current is still on
// MECHANISM (the cell's phase plane at the held current): a two-variable cell that has a stable fixed point at the held current (so it can settle).
// Biology: Izhikevich EM (2003) IEEE Trans Neural Netw 14:1569 (phasic spiking, the 2-D cell with a resonant rest); phasic neurons in the auditory brainstem fire at the onset of a step and
// are silent after (Rothman JS, Manis PB (2003) J Neurophysiol 89:3097, doi:10.1152/jn.00127.2002, K-LT current). The level uses a generic 2-D cell, not an auditory neuron.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_d_phasic();
namespace
{
using namespace bestiary;
using namespace tl;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "d_phasic";
  i.title = "Only the change counts";
  i.brief = "Some neurons answer a step of current with one spike or a short burst and then fall silent, although the current stays on: they report the change, not the level. The phase plane "
            "says how. When the current steps up, the voltage nullcline rises. A tonic neuron has no crossing of the nullclines left that is stable, so it circles for as long as the current "
            "is on. A phasic neuron still has one: the step throws the voltage across threshold once, and then the trajectory spirals into the new resting point and stays there. The sensor "
            "'current' is zero, steps on for a while, and goes off again. Build a cell that is silent before the step, fires when the step comes, and is silent again well before it goes off. "
            "The size of the step is the weight you give the sensor, and it moves the nullcline as much as any parameter of the cell does.";
  i.chapter = "dynamics";
  i.view = "phase";
  i.sensors = { "current" };
  i.motors = { "spikes" };
  i.duration_ms = 2000;
  i.concepts_new = { "stable_fixed_point" };
  i.concepts_used = { "excitability_class" };
  i.par_neurons = 1;
  i.default_model = "izhikevich";
  i.kinds = { { "current", in_kind( "dc", 1.0, 0.5 ) }, { "spikes", micro::rate() } };
  i.bands = { { "onset_spikes", "spikes", 0, 20 } };
  s.gauges = { { "onset_spikes", { "onset_spikes" } } };
  i.recipe = "One Izhikevich cell: a 0.02, b 0.25, c -65, d 6 (the recovery nullcline is steeper than the default 0.2, so rest stays stable at the new current). current > cell at weight 0.5. The step "
             "gives one spike at the start and nothing after. A weight of 1 is too much (the cell circles), a weight of 0.2 too little (no spike at all).";
  i.note = { "A fixed point that survives the input",
    "In the phase plane a constant current only moves the voltage nullcline. If the two nullclines still cross at a stable point the cell ends up there, whatever happened on the way. Neurons of the "
    "auditory brainstem with low-threshold potassium currents fire at the onset of a step and are silent afterwards: they signal change. A cell either settles (phasic) or circles for good (tonic) "
    "depending on whether that stable crossing exists.",
    "Izhikevich EM (2003) IEEE Trans Neural Netw 14:1569-1572, doi:10.1109/TNN.2003.820440; Rothman JS, Manis PB (2003) J Neurophysiol 89:3097-3113, doi:10.1152/jn.00127.2002" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 2000;
    c.segs.push_back( { "current", 400, 1600, 1.0 } );
    c.wins.push_back( { "pre", 100, 400 } );
    c.wins.push_back( { "onset", 400, 650 } );
    c.wins.push_back( { "late", 900, 1600 } );
    return c;
  };
  s.start = []( Engine& e, std::map< std::string, double >& ) { rest_state( e ); };
  s.mech_e = []( const Graph&, Engine& e ) -> std::string {
    const Cell c = cell_of( e, "spikes" );
    if ( !c.ok() ) return "";
    if ( !theory::phase_plane_supported( c.model ) ) return need_two_variables();
    const double w = dc_weight( e, c.n, "current" );
    if ( w <= 0 ) return "The 'current' sensor does not reach the cell that 'spikes' reads: connect it, with a positive weight.";
    const theory::PhasePlane pp = theory::phase_plane( c.model, c.params, w );
    for ( const auto& f : pp.fixed )
      if ( f.stable ) return "";
    return "While the current is on, the two nullclines of this cell cross nowhere at a stable point (look at the phase plane at the held current), so after the first spike the cell has nothing "
           "to settle on and circles for as long as the current lasts. Move the nullclines so that the crossing survives the step: a steeper recovery nullcline, or a smaller step.";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double pre = m.n( "spikes", "pre" ), on = m.n( "spikes", "onset" ), late = m.n( "spikes", "late" );
    p[ "pre_spikes" ] = pre;
    p[ "onset_spikes" ] = on;
    p[ "late_spikes" ] = late;
    c.push_back( { "quiet", pre == 0, 1.0 - ramp( pre, 0, 6 ), "The cell fires before the current comes on: it has to rest quietly without input." } );
    c.push_back( { "onset", on >= 1, ramp( on, 0, 1 ), "The cell does not answer the step: nothing fires when the current comes on. Make the step bigger, or the cell easier to push across its threshold." } );
    c.push_back( { "late", late <= 0, 1.0 - ramp( late, 0, 12 ), "The cell is still firing long after the current came on: it has not settled into a resting point but is circling the limit cycle. A phasic cell must "
                                                              "fall silent while the current is still on." } );
  };
  s.ok_verdict = "The cell reports the change: it fires when the current steps on and is silent again while the current is still on, because the nullclines still cross at a stable point.";
  s.h1 = "A step of current only moves the voltage nullcline. Whether the cell keeps firing depends on whether the nullclines still cross at a stable point afterwards.";
  s.h2 = "Watch the phase plane while the current is on. A stable crossing means the trajectory ends there and the cell falls silent; with no stable crossing it circles for ever.";
  s.h2_target = "spikes";
  s.h3 = "Use a two-variable cell and make the recovery nullcline steeper (a larger coupling parameter) than the default. Then pick the step size with the sensor weight: enough to cross threshold once, "
         "not enough to remove the stable crossing. If the cell never fires, raise the weight a little; if it never stops, lower it.";
  s.metric_name = "onset_spikes";
  s.metric_unit = "spikes";
  s.metric = []( const Meas& m ) { return m.n( "spikes", "onset" ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_d_phasic() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( d_phasic, 601, "d_phasic.json", make_d_phasic ) }
