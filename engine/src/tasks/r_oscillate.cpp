// Micro-puzzle "r_oscillate" (rates chapter, concept "wilson_cowan"): an excitatory and an inhibitory population as two rate units. Excitation that excites itself grows, drives the inhibitory
// unit, which shuts the excitatory one down; then the inhibition fades and the cycle starts again. The model is the Wilson-Cowan pair; the units are NEST threshold-linear rate neurons.
//
// WORLD (3000 ms; rate_drive sensor 'drive', weight = the player's): 0 until 600 ms, then a constant level (0.2 x weight is written to the excitatory unit's mu). Readouts: 'e' and 'i' (rate_value).
// SCORING (the units are threshold-linear: X' = (-X + drive + clip(sum of weight x rate))/tau, so the drive is added after the nonlinearity and inhibition cannot cancel it)
//   quiet    'e' stays below a small value in [200, 600)                              silent without drive
//   swing    in [1400, 3000) the rate of 'e' swings by at least a clear amount            it does not just settle
//   cycles   at least 4 (guided 3) cycles in that window                                  it keeps going
//   period   one cycle takes between 40 and 400 ms                                       a rhythm, not a flicker or a drift
//   lag      'i' follows 'e' by between 5 % and 70 % of the cycle                      the inhibition is the cause of the turn-around, not a copy
// MECHANISM (Engine::structure()): 'e' and 'i' are rate units on different populations; the drive reaches 'e'; 'e' excites itself and 'i'; 'i' inhibits 'e'.
// Biology: Wilson HR, Cowan JD (1972) Biophys J 12:1 (coupled excitatory and inhibitory populations, phase-plane limit cycles). The units here are NEST's threshold-linear rate neurons with a ceiling,
// not the original sigmoid; rates are in arbitrary units.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_r_oscillate();
namespace
{
using namespace bestiary;
using namespace tl;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "r_oscillate";
  i.title = "Two populations that make a rhythm";
  i.brief = "Instead of neurons, think of two whole populations, each as one number: its firing rate. An excitatory population that excites itself, and an inhibitory population that it excites and that "
            "inhibits it back, form the Wilson-Cowan pair, and it can oscillate: excitation builds and drives the inhibition, the inhibition shuts the excitation down, then fades, and excitation "
            "builds again. The sensor 'drive' is zero and then switches on. Build two rate units: 'e' reads the excitatory one and 'i' the inhibitory one. The excitatory rate must stay quiet before "
            "the drive, then swing up and down again and again, and the inhibitory rate must follow it a little later in each cycle. A rate unit has a time constant, a gain, a threshold and a "
            "ceiling; a connection between rate units is a weight (negative for inhibition). The drive adds on top of a unit's ceiling, so a rate can read a little above it. Two things have to be right: the "
            "excitation has to be able to run away by itself, and the loop through the inhibitory unit has to be strong enough to catch it but not so strong that it holds the excitation down for good. If the "
            "rate sticks near its ceiling, the inhibition is too weak to shut the excitation down; if it stays pinned just above the level of the drive, the inhibition is too strong; if it settles to a "
            "steady value in between, the excitation does not run away strongly enough compared with how fast the units follow their inputs.";
  i.chapter = "rates";
  i.view = "rates";
  i.sensors = { "drive" };
  i.motors = { "e", "i" };
  i.duration_ms = 3000;
  i.concepts_new = { "wilson_cowan" };
  i.concepts_used = { "negative_feedback" };
  i.par_neurons = 2;
  i.default_model = "threshold_lin_rate_ipn";
  i.kinds = { { "drive", in_kind( "rate_drive", 0.2, 1.0 ) }, { "e", Task::ChannelKind{ "rate_value", 0, "", 0.05, 1000.0, 0.0 } }, { "i", Task::ChannelKind{ "rate_value", 0, "", 0.05, 1000.0, 0.0 } } };
  i.bands = { { "swing", "", 0, 1.5 } };
  s.gauges = { { "swing", { "swing" } } };
  s.vals = { "e", "i" };
  i.recipe = "Two threshold-linear rate units, E (tau 10, ceiling 1, no noise) and I (tau 20, ceiling 1, no noise). drive > E at weight 1 (level 0.2). Rate connections: E > E +5, E > I +0.8, I > E -6. "
             "The excitatory rate swings between 0 and 1 about six times a second, with the inhibitory rate a little behind.";
  i.note = { "Excitation, inhibition and a rhythm",
    "Wilson and Cowan wrote equations for the activity of a localised population of excitatory and a population of inhibitory neurons. Phase-plane analysis showed that such a pair can sit at a "
    "steady rate, jump between two steady rates, or oscillate, and that the frequency of the oscillation rises with the strength of the stimulus. Linearising around the steady rate gives the condition: the excitation has to "
    "run away by itself (the self-connection above what the leak removes), and the loop through the inhibition has to be strong enough to catch it; the inhibition does not have to be slower, a slower inhibition only makes the "
    "oscillation start more easily and lowers its frequency. The model has been a template for cortical rhythms since.",
    "Wilson HR, Cowan JD (1972) Biophys J 12:1-24, doi:10.1016/S0006-3495(72)86068-5" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 3000;
    c.segs.push_back( { "drive", 600, 3000, 0.2 } );
    c.wins.push_back( { "pre", 200, 600 } );
    c.wins.push_back( { "on", 1400, 3000 } );
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto E = g.out_pops( "e" ), I = g.out_pops( "i" ), D = g.in_pops( "drive" );
    if ( E.empty() || I.empty() ) return "";
    if ( !all_rate( g, E ) || !all_rate( g, I ) ) return need_rate_units( "the activity of whole populations" );
    if ( Graph::overlap( E, I ) ) return "'e' and 'i' read the same unit: the excitatory and the inhibitory population need a unit each.";
    if ( !Graph::overlap( D, E ) ) return "The drive does not reach the excitatory unit: connect 'drive' to the unit that 'e' reads.";
    if ( !g.edge( E, E, +1 ) ) return "The excitatory unit does not excite itself. Without self-excitation the rate cannot build up and the inhibition has nothing to turn around: add a positive connection from the unit to itself.";
    if ( !g.edge( E, I, +1 ) ) return "The excitatory unit does not drive the inhibitory one: there is no connection that carries the rise of excitation to the inhibition.";
    if ( !g.edge( I, E, -1 ) ) return "The inhibitory unit does not inhibit the excitatory one: without a negative connection back, nothing ever shuts the excitation down.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double pre = m.vmax( "e", 200, 600 ), hi = m.vmax( "e", 1400, 3000 ), lo = m.vmin( "e", 1400, 3000 ), amp = hi - lo;
    const double mid = 0.5 * ( hi + lo );
    const auto up = up_crossings( m.val.count( "e" ) ? m.val.at( "e" ) : std::vector< float >(), 1400, 3000, mid, 0.2 * amp );
    const int cyc = static_cast< int >( up.size() ) - 1;
    const double period = cyc >= 1 ? ( up.back() - up.front() ) / cyc : 0.0;
    double lag = 0;
    if ( period > 0 && m.val.count( "e" ) && m.val.count( "i" ) ) lag = best_lag( m.val.at( "e" ), m.val.at( "i" ), 1400, 3000, 0.9 * period );
    p[ "swing" ] = amp;
    p[ "cycles" ] = std::max( cyc, 0 );
    p[ "period_ms" ] = period;
    p[ "lag_ms" ] = lag;
    const double need_amp = m.tol > 1.2 ? 0.3 : ( m.tol < 0.9 ? 0.5 : 0.4 );
    c.push_back( { "quiet", pre < 0.05, 1.0 - ramp( pre, 0.05, 0.5 ), "The excitatory rate is not zero before the drive comes: with no drive the pair must rest." } );
    // where did the rate settle? (the end of the drive phase against the highest it reached since the drive came on)
    const double peak = m.vmax( "e", 600, 3000 ), end = m.vmean( "e", 2500, 3000 );
    const bool pinned = peak > 0.05 && ( end < 0.7 * peak || end < 0.3 );
    const bool high = peak > 0.05 && !pinned && end >= 0.8;
    const std::string swing_msg = amp >= need_amp ? ""
      : peak <= 0.05 ? "The excitatory rate never rises: the drive does not reach the unit that 'e' reads, or the weights are too small for anything to happen."
      : pinned ? "The excitatory rate is pinned just above the level the drive alone gives it: every rise is shut down at once and held down, because the loop through the inhibitory unit is too strong. Weaken the connection "
                 "from the excitatory unit to the inhibitory one or the inhibition back (or strengthen the self-excitation) so that the excitation can build up before it is caught."
      : high && m.vmax( "i", 1400, 3000 ) < 0.05 ? "The excitatory rate climbs and stays at its ceiling and the inhibitory unit hardly responds: nothing shuts the excitation down. The excitatory unit has to drive the inhibitory one."
      : high ? "The excitatory rate settles at a high steady value, at or near its ceiling: the loop through the inhibitory unit is too weak to shut it down. Strengthen the connection from the excitatory unit to the inhibitory one "
               "or the inhibition back (or weaken the self-excitation)."
             : "The excitatory rate settles to a steady value instead of oscillating: the excitation does not run away strongly enough compared with how fast the units follow their inputs, so any overshoot dies out. "
               "Strengthen the self-excitation, make the excitatory unit faster or the inhibitory unit slower, and keep the loop through the inhibitory unit strong enough to catch the rise.";
    c.push_back( { "swing", amp >= need_amp, ramp( amp, 0, need_amp ), swing_msg } );
    const int need_cyc = m.tol > 1.2 ? 3 : 4;
    c.push_back( { "cycles", cyc >= need_cyc, ramp( cyc, 0, need_cyc ), "The rate swings only once or twice and then settles: the oscillation dies out. The loop has to keep overshooting." } );
    c.push_back( { "period", period >= 40.0 / m.tol && period <= 400.0 * m.tol, period > 0 ? ( period < 40.0 ? ramp( period, 0, 40 ) : 1.0 - ramp( period, 400, 1200 ) ) : 0.0,
      period > 0 && period < 40.0 ? "The oscillation is far too fast to be a rhythm: the units follow their input too quickly. Larger time constants (especially the inhibitory unit's) slow it down." : "The oscillation is far too slow: the inhibition takes too long to build or to fade. Speed the units up." } );
    c.push_back( { "lag", period > 0 && lag >= 0.05 * period && lag <= 0.7 * period, period > 0 ? 0.4 : 0.0,
      "The inhibitory rate moves together with the excitatory one instead of following it a little later: the inhibitory unit's own time constant is what gives the delay that makes the inhibition the cause of the turn-around, "
      "so it must not be negligible next to the length of the cycle." } );
  };
  s.ok_verdict = "A Wilson-Cowan oscillator: the excitatory population drives the inhibitory one, which shuts it down and then fades, over and over, and the inhibitory rate follows the excitatory one.";
  s.h1 = "Excitation that excites itself, plus inhibition that it drives and that inhibits it back, makes a rhythm: it overshoots, is shut down, recovers and overshoots again.";
  s.h2 = "The loop needs three connections: the excitatory unit onto itself, onto the inhibitory unit, and the inhibitory unit back onto it with a negative weight. The excitation has to be able to run away by itself (a self-connection that is strong enough), and the loop through the inhibitory unit has to catch it. A slower inhibitory unit makes that easier and lowers the frequency, but it is not required.";
  s.h2_target = "e";
  s.h3 = "Give the excitatory unit strong self-excitation and a drive, and an inhibitory loop of a strength in between: too weak and the rate sticks at its ceiling, too strong and it stays pinned just above the drive "
         "level. If it sticks at its ceiling, strengthen the connections through the inhibitory unit; if it is pinned low, weaken them; if it settles to a steady value in between, strengthen the self-excitation or slow the inhibitory unit down.";
  s.metric_name = "period_ms";
  s.metric_unit = "ms";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) {
    const double hi = m.vmax( "e", 1400, 3000 ), lo = m.vmin( "e", 1400, 3000 );
    const auto up = up_crossings( m.val.count( "e" ) ? m.val.at( "e" ) : std::vector< float >(), 1400, 3000, 0.5 * ( hi + lo ), 0.2 * ( hi - lo ) );
    return up.size() >= 2 ? ( up.back() - up.front() ) / ( up.size() - 1 ) : 0.0;
  };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_r_oscillate() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( r_oscillate, 610, "r_oscillate.json", make_r_oscillate ) }
