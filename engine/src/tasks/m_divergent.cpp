// Micro-puzzle "m_divergent" (concept 19: divergent command). One alarm must reach many effectors at once, some excited and one inhibited.
//
// WORLD (3200 ms; both sensors are dc, multiplied by the sensor weight (pA per unit) in the circuit)
//   "alarm": a threat cue, level 3.0 on [900 + J, 2100 + J) (J = U(0,100) ms seeded; 900 pA at 300 pA per unit), 0 elsewhere.
//   "tone" : level 1.4 all episode (420 pA at 300 pA per unit: a cell wired to it fires at about 40 spikes/s). The gut is the only effector that
//            holds a tone (digestion at rest); the others are silent until the alarm.
//   Motor "heart", "pupil", "sweat", "gut": four effectors' motor pools.
// SCORING (windows in model time)
//   rest      [300, 850]:               heart, pupil, sweat <= 3 spikes/s (silent at rest) and gut >= 8
//   excite    [1100 + J, 2100 + J]:     heart >= 100 spikes/s (a strong response); pupil and sweat each in [15, 80] (a gentler one)
//                                       (guided: heart >= 80, [12, 100]; researcher: heart >= 118, [19, 64])
//   inhibit   the same window:          gut <= 0.3 * its rest rate (guided 0.4, researcher 0.24)
//   together  the three excited effectors start within 100 ms of each other (first 30 ms window with >= max(2, 0.35 N) spikes), each within 200 ms of the alarm
//   recovery  [2400 + J, 3200]:         heart, pupil, sweat <= 5 spikes/s and gut >= 0.6 * its rest rate
//   value 100 when passed, else 85 * credit of the five.
// Why it works out: one source has to drive targets of different gain, and one of them with the opposite sign. The sensor has a single weight, so the
// differences are made downstream: a command group that the alarm drives and that contacts heart (about 600 pA), pupil and sweat (about 280 pA) with
// different synapses, plus an inhibitory interneuron onto the gut. The same alarm straight onto all three would drive them equally (135 spikes/s):
// the heart is fine, pupil and sweat are far too strong.
//
// Biology: Cannon WB (1915) Bodily Changes in Pain, Hunger, Fear and Rage: the sympathetic division discharges as a unit, speeding the heart,
// dilating the pupil and driving sweat while slowing digestion. A single preganglionic neuron contacts many postganglionic cells, and
// the command acts on many organs at once (Jänig W (2006) The Integrative Action of the Autonomic Nervous System, Cambridge UP).
#include "micro_b.h"

namespace spike::tasks
{
namespace
{
using namespace microb;

constexpr double kDur = 3200, kAlarm = 3.0, kTone = 1.4;
const char* const kEff[ 4 ] = { "heart", "pupil", "sweat", "gut" };

class MDivergent : public MicroB
{
public:
  MDivergent() : MicroB( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_divergent";
    i.title = "Divergent command";
    i.brief = "When something frightening appears the body does not decide organ by organ: one command fans out to many effectors at once, each with "
              "its own strength and sign. The heart races, the pupils widen, the skin sweats, and digestion is switched off. Here a threat cue 'alarm' "
              "arrives at about 1 s for 1.2 s. Four effectors listen: heart must go from silence to at least 100 spikes/s, pupil and sweat to a gentler "
              "15-80 spikes/s, all starting within 100 ms of each other, and gut (which holds a little tone from 'tone', about 40 spikes/s) must fall to "
              "a third of that or less. Everything must return to rest after the cue. Give each effector a group and a channel; tone goes to the gut only. "
              "The alarm sensor has one weight, so it drives every cell it touches equally (900 pA, about 135 spikes/s): too much for the pupil and the "
              "sweat. Put a command group between them: the alarm drives it, and it contacts each effector with its own weight (the lab's default 80 pA "
              "is far too weak; about 600 pA makes the heart fire, about 280 pA a gentle response). The gut needs an inhibitory route: a cell set to "
              "inhibitory between the alarm and the gut.";
    i.chapter = "autonomic";
    i.sensors = { "alarm", "tone" };
    i.motors = { "heart", "pupil", "sweat", "gut" };
    i.duration_ms = kDur;
    i.concepts_new = { "divergent_command" };
    i.concepts_used = { "excitation", "inhibition" };
    i.par_neurons = 5;
    i.kinds = { { "alarm", dc( kAlarm ) }, { "tone", dc( kTone ) }, { "heart", rate() }, { "pupil", rate() }, { "sweat", rate() }, { "gut", rate() } };
    i.bands = { { "gut_vs_rest", "", 0.0, 0.3 } };
    i.timeline = { { 900, 1200, "alarm" } };
    i.concept_hint = "Divergence: one command neuron or one sensory signal fans out to many targets, so many organs act together; the sign can differ per "
                     "target (excitation for some, inhibition for others).";
    i.generic_hints = { { 1, i.concept_hint, "" },
      { 2, "The effectors need different strengths and one the opposite sign: a command group with a separate weight to each target does that.", "heart" },
      { 3, "alarm onto a command group of 2 cells. The command group onto heart at about 600 pA, onto pupil and onto sweat at about 280 pA (1 cell each). "
           "alarm also onto an inhibitory group of 2 cells that projects onto gut at about -160 pA. tone onto gut only.", "alarm" } };
    i.note = { "Fight or flight",
      "Walter Cannon described in 1915 how fear or rage makes the sympathetic nervous system act as a unit: the heart races, the pupils dilate, "
      "sweat appears, blood is diverted from the gut and digestion stops. The unity comes from divergence: a sympathetic "
      "preganglionic neuron contacts many postganglionic cells, and descending commands from the hypothalamus reach all the pools at once.",
      "Cannon WB (1915) Bodily Changes in Pain, Hunger, Fear and Rage. D. Appleton and Company, New York" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    j_ = rng_.uniform( 0, 100 );
    base_gut_ = 40;
  }

  std::map< std::string, Metric > metrics() const override
  {
    std::map< std::string, double > p;
    std::vector< Crit > c;
    std::string m;
    measure( p, c, m );
    return { { "onset_spread_ms", { p.count( "onset_spread_ms" ) ? p[ "onset_spread_ms" ] : 999.0, "ms", true } } };
  }

protected:
  bool on( double t ) const { return t >= 900 + j_ && t < 2100 + j_; }

  void drive( Engine& e, double ) override
  {
    e.set_input( "tone", kTone );
    e.set_input( "alarm", on( t_ ) ? kAlarm * ( 1.0 + 0.03 * noise() * rng_.normal() ) : 0.0 );
  }

  void update_readouts() override
  {
    if ( t_ >= 850 && t_ < 860 )
      base_gut_ = std::max( 5.0, hz( "gut", 300, 850 ) );
    for ( int i = 0; i < 4; ++i )
      readouts_[ std::string( kEff[ i ] ) + "_hz" ] = hz_now( kEff[ i ] );
    readouts_[ "alarm" ] = on( t_ ) ? 1.0 : 0.0;
    readouts_[ "gut_vs_rest" ] = on( t_ ) && t_ > 900 + j_ + 200 ? hz_now( "gut" ) / base_gut_ : 0.0;
  }

  void measure( std::map< std::string, double >& parts, std::vector< Crit >& crits, std::string& pass_msg ) const override
  {
    const double tl = tol();
    const double a0 = 900 + j_, a1 = 2100 + j_;
    double rest[ 4 ], act[ 4 ], rec[ 4 ];
    for ( int i = 0; i < 4; ++i )
    {
      rest[ i ] = hz( kEff[ i ], 300, 850 );
      act[ i ] = hz( kEff[ i ], a0 + 200, a1 );
      rec[ i ] = hz( kEff[ i ], 2400 + j_, kDur );
      parts[ std::string( kEff[ i ] ) + "_rest_hz" ] = rest[ i ];
      parts[ std::string( kEff[ i ] ) + "_alarm_hz" ] = act[ i ];
    }
    double on_t[ 3 ];
    double lo_t = 1e9, hi_t = -1e9;
    bool started = true;
    for ( int i = 0; i < 3; ++i )
    {
      const double need = std::max( 2.0, std::ceil( 0.35 * pool( kEff[ i ] ) - 1e-9 ) );
      on_t[ i ] = first_window( kEff[ i ], a0, a0 + 600, 30, need );
      if ( on_t[ i ] < 0 ) started = false;
      else lo_t = std::min( lo_t, on_t[ i ] ), hi_t = std::max( hi_t, on_t[ i ] );
    }
    const double spread = started ? hi_t - lo_t : 999.0;
    parts[ "onset_spread_ms" ] = spread;

    Crit r;
    r.key = "rest";
    const bool quiet = rest[ 0 ] <= 3 && rest[ 1 ] <= 3 && rest[ 2 ] <= 3;
    r.ok = quiet && rest[ 3 ] >= 8;
    r.soft = quiet ? ramp( rest[ 3 ], 0, 8 ) : 0.3;
    r.fail = !quiet ? "At rest the heart, pupil or sweat pool fires (" + fmt( rest[ 0 ] ) + " / " + fmt( rest[ 1 ] ) + " / " + fmt( rest[ 2 ] ) + " spikes/s): they must be silent until the alarm. Tone goes to the gut only."
                    : "The gut is silent at rest (" + fmt( rest[ 3 ] ) + " spikes/s): digestion needs its tone, or the alarm has nothing to switch off. Wire 'tone' to the gut group.";
    r.place = !quiet ? "A pool that fires at rest: it is wired to 'tone' (or has a bias)." : "The gut gets no tone.";
    r.target = !quiet ? "heart" : "gut";
    r.recipe = "tone onto the gut group only; no bias current on the other groups.";
    crits.push_back( r );

    Crit ex;
    ex.key = "excite";
    const double heart_min = 100.0 / tl, g_lo = 15.0 / tl, g_hi = 80.0 * tl;
    const bool heart_ok = act[ 0 ] >= heart_min;
    const bool pupil_ok = act[ 1 ] >= g_lo && act[ 1 ] <= g_hi, sweat_ok = act[ 2 ] >= g_lo && act[ 2 ] <= g_hi;
    ex.ok = heart_ok && pupil_ok && sweat_ok;
    ex.soft = ( ramp( act[ 0 ], 0, heart_min ) + ( pupil_ok ? 1.0 : 0.3 ) + ( sweat_ok ? 1.0 : 0.3 ) ) / 3.0;
    if ( !heart_ok )
      ex.fail = "The heart reaches only " + fmt( act[ 0 ] ) + " spikes/s in the alarm (needs " + fmt( heart_min ) + "): it needs a strong synapse from the command. Raise its weight (about 600 pA).";
    else if ( !pupil_ok )
      ex.fail = std::string( "The pupil fires " ) + fmt( act[ 1 ] ) + " spikes/s in the alarm, the gentle response is " + fmt( g_lo ) + "-" + fmt( g_hi ) +
                ( act[ 1 ] > g_hi ? ": too strong. The same weight cannot serve the heart and the pupil; give the pupil its own, weaker synapse from the command group (about 280 pA)."
                                  : ": too weak or silent. Wire the alarm to the pupil group through the command group (about 280 pA).");
    else
      ex.fail = std::string( "Sweat fires " ) + fmt( act[ 2 ] ) + " spikes/s in the alarm, the gentle response is " + fmt( g_lo ) + "-" + fmt( g_hi ) +
                ( act[ 2 ] > g_hi ? ": too strong. Give it its own, weaker synapse from the command group (about 280 pA)."
                                  : ": too weak or silent. Wire the alarm to the sweat group through the command group (about 280 pA).");
    ex.place = !heart_ok ? "The heart: its synapse from the command must be strong." : ( !pupil_ok ? "The pupil: its synapse must be weaker than the heart's." : "Sweat: its synapse must be weaker than the heart's." );
    ex.target = !heart_ok ? "heart" : ( !pupil_ok ? "pupil" : "sweat" );
    ex.recipe = "alarm onto a command group (2 cells); command onto heart at about 600 pA, onto pupil and sweat at about 280 pA (1 cell each).";
    crits.push_back( ex );

    Crit in;
    in.key = "inhibit";
    const double gr = act[ 3 ] / std::max( rest[ 3 ], 1.0 );
    in.ok = gr <= 0.3 * tl;
    in.soft = 1.0 - ramp( gr, 0.3, 1.0 );
    in.fail = "During the alarm the gut still fires " + fmt( act[ 3 ] ) + " spikes/s (" + fmt( 100 * gr ) + " % of its rest " + fmt( rest[ 3 ] ) + "): the alarm must switch digestion off. "
              "Send it through an inhibitory cell onto the gut.";
    in.place = "The gut: the alarm reaches it with the opposite sign.";
    in.target = "gut";
    in.recipe = "alarm onto an inhibitory group of 2 cells, projecting onto the gut group at about -160 pA.";
    crits.push_back( in );

    Crit tg;
    tg.key = "together";
    tg.ok = started && spread <= 100 * tl && lo_t - a0 <= 200 * tl;
    tg.soft = started ? 1.0 - ramp( spread, 100, 400 ) : 0.2;
    tg.fail = started ? "The effectors do not start together: their onsets are " + fmt( spread ) + " ms apart (limit " + fmt( 100 * tl ) + ") or too late. A command should reach all of them at the same moment: "
                        "use the same kind of route (same weights and delays) to each."
                      : "One of the three excited effectors never starts in the first 0.6 s of the alarm.";
    tg.place = "The routes to the effectors differ in delay or strength: they should start together.";
    tg.target = "alarm";
    tg.recipe = "Use the same weight and the default delay (2 ms) on every route from the alarm to heart, pupil and sweat.";
    crits.push_back( tg );

    Crit rc;
    rc.key = "recovery";
    rc.ok = rec[ 0 ] <= 5 && rec[ 1 ] <= 5 && rec[ 2 ] <= 5 && rec[ 3 ] >= 0.6 * rest[ 3 ];
    rc.soft = rc.ok ? 1.0 : 0.4;
    rc.fail = "After the alarm the effectors are at " + fmt( rec[ 0 ] ) + " / " + fmt( rec[ 1 ] ) + " / " + fmt( rec[ 2 ] ) + " spikes/s and the gut at " + fmt( rec[ 3 ] ) +
              " (rest " + fmt( rest[ 3 ] ) + "): the response must end with the cue. A loop that keeps a group firing, or an inhibition that never lets go, would do this.";
    rc.place = "A group that does not return to rest: look for a loop or a persistent inhibition.";
    rc.target = "gut";
    rc.recipe = "Links run from the alarm outward only; keep tone onto the gut group.";
    crits.push_back( rc );

    pass_msg = "The alarm reached heart " + fmt( act[ 0 ] ) + ", pupil " + fmt( act[ 1 ] ) + " and sweat " + fmt( act[ 2 ] ) + " spikes/s within " + fmt( spread ) +
               " ms of each other while the gut fell from " + fmt( rest[ 3 ] ) + " to " + fmt( act[ 3 ] ) + ": one command, many effectors.";
  }

private:
  double j_ = 0, base_gut_ = 40;
};

} // namespace

std::unique_ptr< Task > make_m_divergent() { return std::make_unique< MDivergent >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_divergent, 419, "m_divergent.json", make_m_divergent ) }
