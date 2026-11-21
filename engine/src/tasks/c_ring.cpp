// Micro-puzzle "c_ring" (fly compass chapter, concept "ring_attractor": local excitation and broad inhibition hold one bump of activity on a ring). The fly's heading is stored as a
// bump of activity on a ring of compass cells (E-PG cells, one per wedge of the ellipsoid body). A landmark makes the bump; when the landmark goes the bump stays.
//
// WORLD (3000 ms; the landmark is one of sixteen Poisson sensors cue0..cue15, 300 Hz on [0, 400), one per ring cell, at the bearing 22.5 k degrees; the seed picks k; the ring is read on 'heading' as a
// population vector: neuron k of N at the angle 360 k / N degrees)
//   form     [250, 400)    the bump exists while the landmark is on, at the landmark (strength of the vector >= 0.5; guided 0.33; within 45 degrees of the bearing)
//   hold1    [900, 1400)   500 ms after the landmark went: still one bump, still firing (>= 8 Hz per cell on average over the ring; strength >= 0.45)
//   hold2    [2400, 2900)  and 1.5 s later: same, and the bump has not wandered more than 45 degrees (guided 65, researcher 31) from the landmark
// MECHANISM (Engine::structure()): 'heading' is read as a population vector over at least four cells; the ring cells lie on an excitatory loop (local excitation); and they inhibit
// themselves through another group (global inhibition, a net-inhibitory path from the ring back to the ring).
// Biology: the E-PG cells carry a single bump of activity that is kept in the dark by recurrent excitation and competition (Seelig & Jayaraman 2015 Nature 521:186). The wiring here is a
// ring-attractor model after the cell types of the fly central complex, not synapse-level data: the real E-PG to E-PG excitation is largely indirect (through P-EN cells and others).
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_c_ring();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "c_ring";
  i.title = "Fly: a bump that stays";
  i.brief = "The fly's sense of heading is a single bump of activity on a ring of compass cells (E-PG cells, one for each wedge of a ring-shaped structure in the brain). A landmark "
            "makes the bump; when the landmark disappears the bump must stay where it was, for seconds. The wiring here is a model after the cell types of the fly's central complex "
            "(after Turner-Evans and Hulse cell types, not synapse-level data). Build a ring of sixteen cells and read 'heading' as a population vector over them in ring order (the readout turns "
            "the pattern of activity into one direction). The sensors cue0 to cue15 are the landmark seen at each of sixteen bearings, one sensor for each ring cell; one of them is on for "
            "a moment and then goes off. While it is on there must be a single bump at the landmark (a few neighbouring cells firing, the rest quiet), and one and a half seconds after it has gone "
            "the bump must still be there, in the same place. Cells that excite their neighbours keep each other firing; but excitation alone spreads round the whole ring. A cell "
            "that the ring excites, and that inhibits every ring cell, makes the bump compete with itself: one bump wins. "
            "A default cell is silent until it is pushed, and its fast synapses let a bump die at once: to hold a bump the ring cells need a steady drive of their own (a bias current that keeps them "
            "just below threshold) and slower excitatory synapses (a synaptic time constant clearly longer than the default), so that neighbours can keep each other going.";
  i.chapter = "fly_compass";
  i.view = "compass";
  i.sensors = cue_family();
  i.motors = { "heading" };
  i.duration_ms = 3000;
  i.concepts_new = { "ring_attractor" };
  i.concepts_used = { "excitation", "inhibition", "persistence" };
  i.par_neurons = 24;   // the reference has 20 cells; the budget leaves room for a larger inhibitory group
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "heading", ring_kind() } };
  for ( const auto& c : i.sensors ) i.kinds[ c ] = in_kind( "poisson", 300, 400 );
  i.bands = { { "bump_strength", "", 0.45, 1.0 } };
  i.recipe = "Twenty integrate-and-fire cells: EPG (16 cells, the ring: bias 320 pA, tau syn E 15 ms) and DELTA (4 inhibitory cells). cue0 to cue15 > EPG, sensor k to cell k, at 400 pA. EPG > EPG with segment wiring (shifts of minus one and plus one) at +100 pA; "
             "EPG > DELTA at +150 pA; DELTA > EPG at -400 pA. 'heading' reads EPG as a population vector, cells in order 0 to 7. The cue seeds a bump on a few cells; it then holds itself.";
  i.note = { "A bump that holds the heading",
    "In the fly's central complex the E-PG cells tile a ring, and at any moment one wedge of it is active: a bump whose position is the fly's heading. The bump stays when the "
    "landmark is taken away, even in darkness for many seconds, which marks it as a ring attractor: neighbouring cells excite each other, and broad inhibition keeps a single bump "
    "from spreading. This level uses a ring-attractor model built from the published cell types, not synapse-level wiring.",
    "Seelig JD, Jayaraman V (2015) Nature 521:186-191, doi:10.1038/nature14446" };
  s.vec = { "heading" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 3000;
    c.segs = { { "cue", 0, 400, 300 } };   // re-addressed to the sensor of the landmark's wedge in Landmark::reset
    c.wins = { { "form", 250, 400 }, { "hold1", 900, 1400 }, { "hold2", 2400, 2900 } };
    return c;
  };
  s.timeline = []() -> std::vector< Task::Event > { return { { 0, 400, "landmark" }, { 1000, 0, "dark: hold" } }; };
  s.mech = []( const Graph& g ) -> std::string {
    std::string r = ring_readout_missing( g, "heading" );
    if ( !r.empty() ) return r;
    r = landmarks_missing( g, "heading" );
    if ( !r.empty() ) return r;
    r = ring_dynamics_missing( g, "heading" );
    if ( !r.empty() ) return r;
    return ring_neighbours_missing( g, "heading" );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double lm = m.extra.count( "landmark_deg" ) ? m.extra.at( "landmark_deg" ) : 0.0;
    const double s0 = m.vec_strength( "heading", 250, 400 ), s1 = m.vec_strength( "heading", 900, 1400 ), s2 = m.vec_strength( "heading", 2400, 2900 );
    const double r0 = m.hz( "heading", "form" ), r1 = m.hz( "heading", "hold1" ), r2 = m.hz( "heading", "hold2" );
    const double e0 = std::fabs( wrap180( m.vec_deg( "heading", 250, 400 ) - lm ) ), e2 = std::fabs( wrap180( m.vec_deg( "heading", 2400, 2900 ) - lm ) );
    p[ "landmark_deg" ] = lm;
    p[ "strength_form" ] = s0;
    p[ "strength_hold1" ] = s1;
    p[ "strength_hold2" ] = s2;
    p[ "rate_hold1_hz" ] = r1;
    p[ "rate_hold2_hz" ] = r2;
    p[ "error_form_deg" ] = e0;
    p[ "error_hold_deg" ] = e2;
    const double smin = 0.5 / m.tol, lim = 45.0 * m.tol;
    c.push_back( { "form", r0 >= 3 && s0 >= smin && e0 <= lim, r0 < 3 ? 0.0 : ramp( s0, 0, smin ) * 0.5 + 0.5 * ( 1.0 - ramp( e0, lim, 120.0 ) ),
      r0 < 3 ? "Nothing much fires while the landmark is on: the landmark sensors do not reach the ring cells, or too weakly. Wire the cue sensors to the ring cells and strengthen them."
      : s0 < smin ? "While the landmark is on there is no single bump: activity covers the ring (or two bumps fight). The ring needs inhibition that grows with the activity, so that one bump wins over the rest."
                  : "The bump forms away from the landmark: sensor k must drive ring cell k, and the ring must be read in the same order, cell 0 first." } );
    const double hmin = 0.45 / m.tol;
    c.push_back( { "hold", r1 >= 8 && r2 >= 8 && s1 >= hmin && s2 >= hmin, std::min( ramp( r1, 0, 8 ), ramp( r2, 0, 8 ) ) * 0.5 + 0.5 * std::min( ramp( s1, 0, hmin ), ramp( s2, 0, hmin ) ),
      ( r1 < 8 || r2 < 8 ) ? "The bump dies when the landmark goes: the ring cells do not keep each other firing. Strengthen the excitation between neighbouring cells, give the ring cells a bias drive of their own (so that they sit just below threshold), or slow their excitatory synapses (a longer synaptic time constant)."
                           : "After the landmark goes the activity spreads over the ring instead of staying a single bump: strengthen the broad inhibition, or weaken the excitation." } );
    c.push_back( { "still", e2 <= lim, 1.0 - ramp( e2, lim, 120.0 ), "The bump wanders away from where the landmark put it. Cells should excite their two nearest neighbours equally, and the inhibition should not favour any cell." } );
  };
  s.ok_verdict = "A ring attractor: the landmark makes a single bump, and the bump stays in place after the landmark is gone, held by local excitation and broad inhibition.";
  s.h1 = "A bump on a ring is held by two opposite things: cells that excite their neighbours, and an inhibition that every cell shares.";
  s.h2 = "The ring cells must excite their two nearest neighbours (wire the ring to itself, neuron k to its neighbours) and a separate inhibitory cell must be excited by the ring and inhibit all of it.";
  s.h2_target = "heading";
  s.h3 = "Ring cells with segment wiring to the cell on each side (two connections, shifts of minus one and plus one); one inhibitory cell that the ring excites and that inhibits the ring. If the bump dies "
         "when the landmark goes, strengthen the ring excitation, give the ring cells a bias drive of their own, or make their excitatory synapses slower; if it spreads over the ring, strengthen the inhibition or weaken the excitation.";
  s.metric_name = "error_deg";
  s.metric_unit = "deg";
  s.metric = []( const Meas& m ) { return std::fabs( wrap180( m.vec_deg( "heading", 2400, 2900 ) - ( m.extra.count( "landmark_deg" ) ? m.extra.at( "landmark_deg" ) : 0.0 ) ) ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_c_ring() { return std::make_unique< Landmark >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( c_ring, 530, "c_ring.json", make_c_ring ) }
