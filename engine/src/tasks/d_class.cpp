// Micro-puzzle "d_class" (dynamics chapter, concept "excitability_class": how a neuron starts to fire as the current grows). A cell either starts firing slowly, from almost nothing (class I,
// the resting state vanishes by running into the saddle that separates it from firing), or starts at once at a clear rate (class II, the resting state is a spiral that the current makes unstable
// and it is the oscillation of that spiral that fires). In the phase plane the difference is the shape of the fixed point just before it is lost: a node or a resonant focus.
//
// WORLD (4300 ms; dc sensor 'current', weight = the player's): a staircase of 21 steps of 200 ms from 70 % to 130 % of the cell's own onset current (the current at which its resting
//   state is lost, read from its phase plane: the staircase is scaled to the cell, so the weight the player chooses cannot make a slowly-starting cell look abrupt). The rate of a step is
//   measured over its last 140 ms.
// SCORING
//   rest    the three lowest steps are silent                                        the cell is at rest without input
//   fires   some step reaches a clear rate                                              the current does start firing
//   abrupt  the busiest of the first three steps that spike has >= 3 spikes in 140 ms (guided 2, researcher 4)   firing starts at a clear rate (class II)
//   keeps   the highest step still fires
// MECHANISM (the cell's own parameters, theory::onset): a two-variable cell whose resting state is lost through a resonant focus (Hopf), not a node (saddle-node).
// Biology: Hodgkin AL (1948) J Physiol 107:165 (class 1 and class 2 axons); Izhikevich EM (2003) IEEE Trans Neural Netw 14:1569 (the 2-D cells and their classes);
// Ermentrout B (1996) Neural Comput 8:979 (class I membranes). The 2-D reset models are class I or II in a continuum: the cells near a Bogdanov-Takens point (the default regular spiker) sit between.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_d_class();
namespace
{
using namespace bestiary;
using namespace tl;

constexpr int kSteps = 21;
const double kFrom = 0.70, kStep = 0.03;
constexpr double kStepMs = 200;
constexpr double kMinFreqHz = 15.0;   // resonance of the resting state that counts as a spiral

struct Onset { bool ok = false; Cell cell; theory::Onset o; };
Onset onset_of( Engine& e )
{
  Onset r;
  r.cell = cell_of( e, "spikes" );
  if ( !r.cell.ok() || !theory::phase_plane_supported( r.cell.model ) ) return r;
  r.o = theory::onset( r.cell.model, r.cell.params, 0.0, scan_max( r.cell.model ), 800 );
  r.ok = r.o.found;
  return r;
}

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "d_class";
  i.title = "Where firing starts";
  i.brief = "Raise the current into a neuron step by step and watch where firing begins. Some neurons start slowly, from almost nothing, and speed up as the current grows: class I. Others start "
            "at once at a clear rate, as if a switch had been thrown: class II. The phase plane (voltage against the recovery variable, with the two nullclines) shows why. As the current grows "
            "the voltage nullcline rises, and the resting state, the point where the nullclines cross, is lost. If it runs into the second crossing and vanishes, the cell can only creep through "
            "the bottleneck that is left: class I. If it turns into a spiral that the current makes unstable, the cell begins at the frequency of that spiral: class II. The sensor 'current' "
            "steps up through the cell's own onset (the staircase is scaled to the cell), and 'spikes' reads the cell. Build a cell that is silent at the bottom, and whose firing starts at "
            "a clear rate, not from zero. Look at where the nullclines cross, and what kind of point that is. The class is not looked up from the cell's name: it is computed for this cell, from the Jacobian "
            "(the local slopes) of its two equations at the resting state, so a cell with other parameters can land on either side.";
  i.chapter = "dynamics";
  i.view = "phase";
  i.sensors = { "current" };
  i.motors = { "spikes" };
  i.duration_ms = kFrom * 0 + 4300;
  i.concepts_new = { "excitability_class" };
  i.concepts_used = { "threshold" };
  i.par_neurons = 1;
  i.default_model = "izhikevich";
  i.kinds = { { "current", in_kind( "dc", 1.0, 1.0 ) }, { "spikes", micro::rate() } };
  i.bands = { { "onset_hz", "Hz", 0, 200 } };
  s.gauges = { { "onset_hz", { "onset_hz" } } };
  i.recipe = "One Izhikevich cell: a 0.1, b 0.26, c -65, d 2 (a resonator: the recovery variable is fast and feeds back strongly). current > cell at weight 1. The first step that fires gives "
             "about 5 spikes in 140 ms. With the default regular-spiking cell (a 0.02, b 0.2, d 8) the same stairs start with one spike: class I.";
  i.note = { "How the resting state is lost",
    "Hodgkin found two kinds of axon. Class 1 axons fire at any low rate if the current is just above threshold; class 2 axons start at a clear rate. In a two-variable model the difference is how "
    "the resting state disappears: through a node colliding with a saddle (class 1), or as a spiral losing stability (class 2). Real neurons, and these cells, lie on a continuum between the two.",
    "Hodgkin AL (1948) J Physiol 107:165-181, doi:10.1113/jphysiol.1948.sp004260; Izhikevich EM (2003) IEEE Trans Neural Netw 14:1569-1572, doi:10.1109/TNN.2003.820440" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = kSteps * kStepMs + 100;
    for ( int k = 0; k < kSteps; ++k )
    {
      c.segs.push_back( { "current", k * kStepMs, ( k + 1 ) * kStepMs, kFrom + kStep * k } );
      c.wins.push_back( { "s" + std::to_string( k ), k * kStepMs + 60, ( k + 1 ) * kStepMs } );
    }
    return c;
  };
  s.start = []( Engine& e, std::map< std::string, double >& scale ) {
    rest_state( e );
    const Onset o = onset_of( e );
    const double w = o.cell.ok() ? dc_weight( e, o.cell.n, "current" ) : 0.0;
    if ( o.ok && w > 0 ) scale[ "current" ] = o.o.current / w;
  };
  s.mech_e = []( const Graph&, Engine& e ) -> std::string {
    const Onset o = onset_of( e );
    if ( !o.cell.ok() ) return "";
    if ( !theory::phase_plane_supported( o.cell.model ) ) return need_two_variables();
    if ( !o.ok )
      return "In the phase plane this cell has no stable resting state to lose: either it fires on its own, or the nullclines keep crossing at a stable point however far the current is raised. "
             "Look for a cell whose resting state is lost somewhere along the staircase.";
    if ( dc_weight( e, o.cell.n, "current" ) <= 0 ) return "The 'current' sensor does not reach the cell that 'spikes' reads: connect it.";
    if ( o.o.kind != "hopf" || o.o.freq_hz < kMinFreqHz )
      return "The resting state of this cell is lost by running into the other crossing of the nullclines (or by a spiral that turns too slowly to matter): the voltage then has to creep through a "
             "bottleneck, and firing starts slowly from zero rate. For firing that starts at once, the resting state must be a clear spiral before it is lost: the recovery variable has to follow the "
             "voltage quickly and feed back on it strongly.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    std::vector< double > n( kSteps );
    for ( int k = 0; k < kSteps; ++k ) n[ k ] = m.c( "spikes", "s" + std::to_string( k ) );
    double low = n[ 0 ] + n[ 1 ] + n[ 2 ], top = 0;
    int first = -1;
    for ( int k = 0; k < kSteps; ++k )
    {
      top = std::max( top, n[ k ] );
      if ( first < 0 && n[ k ] >= 1 ) first = k;
    }
    // the oscillation of a spiral grows for a while before it reaches threshold, so the step that first spikes can be caught mid-way: the abruptness is read over that step and the next two
    double first_n = 0;
    if ( first >= 0 )
      for ( int k = first; k < std::min( first + 3, kSteps ); ++k ) first_n = std::max( first_n, n[ k ] );
    p[ "first_step" ] = first;
    p[ "first_step_spikes" ] = first_n;
    p[ "onset_hz" ] = first_n / 0.14;
    p[ "top_spikes" ] = n[ kSteps - 1 ];
    c.push_back( { "rest", low == 0, 1.0 - ramp( low, 0, 12 ), "The cell fires at the bottom of the staircase, where the current is still well below its onset: it must rest quietly there." } );
    c.push_back( { "fires", top >= 3, ramp( top, 0, 3 ), "The current never starts the cell firing: connect the sensor with a weight that reaches the cell's onset, and look at where the nullclines stop crossing." } );
    const double need = m.tol > 1.2 ? 2.0 : ( m.tol < 0.9 ? 4.0 : 3.0 );
    c.push_back( { "abrupt", first_n >= need, ramp( first_n, 0, need ), "Firing starts slowly, from one spike or none: the cell is class I. Class II firing starts at a clear rate the moment the resting state is lost." } );
    c.push_back( { "keeps", n[ kSteps - 1 ] >= 3, ramp( n[ kSteps - 1 ], 0, 3 ), "The cell stops firing again at the top of the staircase: it must keep firing as the current grows." } );
  };
  s.ok_verdict = "Firing starts at a clear rate: for this cell, by the eigenvalues of its Jacobian at rest, the resting state is a spiral that the current makes unstable (class II), and the staircase shows the abrupt start.";
  s.h1 = "How firing starts depends on how the resting state is lost: through a collision of two fixed points (slow start from zero) or as a spiral that turns unstable (abrupt start).";
  s.h2 = "Look at the fixed point where the nullclines cross, just before it is lost. A node has no oscillation to give; a spiral does. Which parameters make the recovery variable respond fast and strongly?";
  s.h2_target = "spikes";
  s.h3 = "Use a two-variable cell and make its recovery variable quick (a larger time-scale parameter) and strongly coupled to the voltage (a larger coupling parameter), so that rest is a spiral. If the cell "
         "still creeps into firing, increase both; if it fires without any current, lower the reset or the coupling a little.";
  s.metric_name = "onset_rate_hz";
  s.metric_unit = "Hz";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) {
    for ( int k = 0; k < kSteps; ++k )
      if ( m.n( "spikes", "s" + std::to_string( k ) ) >= 1 )
      {
        double b = 0;
        for ( int j = k; j < std::min( k + 3, kSteps ); ++j ) b = std::max( b, m.n( "spikes", "s" + std::to_string( j ) ) );
        return b / 0.14;
      }
    return 0.0;
  };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_d_class() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( d_class, 600, "d_class.json", make_d_class ) }
