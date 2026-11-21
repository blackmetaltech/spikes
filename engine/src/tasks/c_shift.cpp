// Micro-puzzle "c_shift" (fly compass chapter, concept "path_integration": a bump moved by asymmetric drive; the angular velocity of the fly is the difference between two shifter inputs).
// A ring holds a bump (previous level); now it has to be MOVED. Two groups of shifting cells (P-EN cells), one for each direction, each listen to the bump and to a turn sensor, and
// each push the bump one step to its own side. With equal drive the two pushes cancel and the bump stays; when one side is driven harder it wins and the bump moves.
//
// WORLD (4400 ms; Poisson sensors cue0..cue15 as in c_ring; dc sensors turn_left and turn_right: a tonic level on both from t = 0 and, during a turn, the side of the turn gets more and the other side less)
//   cal     [0, 500)         the landmark, the bump forms at its bearing (seed picks one of sixteen)
//   still   [500, 1500)      equal drive on both sides: the bump stays (<= 30 degrees from the landmark; guided 45, researcher 21)  window [1300, 1500)
//   right   [1500, 2200)     a turn to the right (150 deg/s on the sensors): the bump must move to the right by at least 60 degrees (guided 40) and not more than 170    window [2400, 2600)
//   left    [3000, 3700)     a turn to the left: the bump must move back to the left by at least 60 degrees (guided 40), ending within 70 degrees of the landmark       window [3900, 4100)
// MECHANISM (Engine::structure()): as for the ring (c_ring) and two separate shifter groups, each excited by the ring and exciting the ring, driven by their own turn sensor.
// Biology: Turner-Evans et al. 2017 eLife 6:e23496 and Green et al. 2017 Nature 546:101: P-EN cells of the left and right protocerebral bridge shift the E-PG bump in opposite directions; their activity
// follows the angular velocity. The wiring is a model after the cell types, not synapse-level data.
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_c_shift();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "c_shift";
  i.title = "Fly: moving the bump";
  i.brief = "The fly's compass bump has to move when the fly turns, even in the dark. The ring of compass cells (from the previous level: local excitation and global inhibition, a landmark that sets "
            "the bump) is given two groups of shifting cells, one for each direction (P-EN cells, after Turner-Evans and Hulse cell types, not synapse-level data). Each group listens to the compass cells "
            "at the bump and to its own turn sensor, and excites the compass cells one step to its own side: the right group the cell on one side, the left group the cell on the other. The sensors "
            "'turn_left' and 'turn_right' both carry a steady level, equal while the fly is still, and during a turn the sensor of that side carries more and the other less. While the fly is "
            "still the two groups push equally and the bump stays; during a turn one side pushes harder, and the bump moves in that direction. Build it: the bump must form at the landmark, stay while "
            "the fly is still, move to the right during a turn to the right, and move back during a turn to the left. A shifting group that listened only to the turn sensor would "
            "push everywhere on the ring; it must also need the bump, so that the push acts where the bump is. Each unit of a turn sensor is only a small current: the weight from a turn sensor onto the shifting cells must be small compared with the ring's own weights (a few picoamps per unit, not hundreds).";
  i.chapter = "fly_compass";
  i.view = "compass";
  i.sensors = cue_family();
  i.sensors.push_back( "turn_left" );
  i.sensors.push_back( "turn_right" );
  i.motors = { "heading" };
  i.duration_ms = 4400;
  i.concepts_new = { "path_integration" };
  i.concepts_used = { "ring_attractor", "inhibition", "persistence" };
  i.par_neurons = 60;   // the reference has 52 cells; the budget leaves room for a larger inhibitory group
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "heading", ring_kind() }, { "turn_left", in_kind( "dc", kShiftBase, 3 ) }, { "turn_right", in_kind( "dc", kShiftBase, 3 ) } };
  for ( int k = 0; k < kWedges; ++k ) i.kinds[ "cue" + std::to_string( k ) ] = in_kind( "poisson", 300, 400 );
  i.bands = { { "bump_shift_deg", "deg", 60, 170 } };
  i.recipe = "Fifty-two integrate-and-fire cells: EPG (16 cells, the ring: bias 320 pA, tau syn E 15 ms), DELTA (4 cells), PENL and PENR (16 cells each, default cells). cue0 to cue15 > EPG, sensor k to cell k, at "
             "400 pA. EPG > EPG with segment wiring to the cell on each side (shifts of minus one and plus one) at +100 pA; EPG > DELTA at +150 pA; DELTA > EPG at -400 pA. EPG > PENL and EPG > PENR with segment "
             "wiring (cell k to cell k) at +300 pA; turn_left > PENL and turn_right > PENR at 3 pA per unit of the sensor; PENR > EPG with segment wiring shifted by +1 and PENL > EPG shifted by -1 at +110 pA. "
             "'heading' reads EPG as a population vector, cells in order 0 to 15.";
  i.note = { "Shifting the bump",
    "In the fly's heading circuit the E-PG bump is moved by P-EN cells. They get their input from the E-PG cells at the bump and from a signal related to the fly's turning, and the left and right "
    "subtypes shift the bump in opposite directions. Because both subtypes are active at rest and their pushes cancel, the bump moves only when the fly turns; the speed of the bump follows the speed "
    "of the turn. This level is a ring-attractor model after the published cell types, not synapse-level wiring.",
    "Turner-Evans D et al. (2017) eLife 6:e23496, doi:10.7554/eLife.23496; Green J et al. (2017) Nature 546:101-106, doi:10.1038/nature22343" };
  s.vec = { "heading" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 4400;
    c.segs.push_back( { "cue", 0, 500, 300 } );
    shifter_segments( { { 1500, 2200, 150 }, { 3000, 3700, -150 } }, c.dur, c.segs );
    c.wins = { { "cal", 350, 500 }, { "still", 1300, 1500 }, { "right", 2400, 2600 }, { "left", 3900, 4100 } };
    return c;
  };
  s.timeline = []() -> std::vector< Task::Event > { return { { 0, 500, "landmark" }, { 1500, 700, "turn right" }, { 3000, 700, "turn left" } }; };
  s.mech = []( const Graph& g ) -> std::string {
    std::string r = ring_readout_missing( g, "heading" );
    if ( !r.empty() ) return r;
    r = landmarks_missing( g, "heading" );
    if ( !r.empty() ) return r;
    r = ring_dynamics_missing( g, "heading" );
    if ( !r.empty() ) return r;
    return shifters_missing( g, "heading", "turn_left", "turn_right" );
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double lm = m.extra.count( "landmark_deg" ) ? m.extra.at( "landmark_deg" ) : 0.0;
    const double smin = 0.4 / std::sqrt( m.tol );
    const double r_cal = m.hz( "heading", "cal" ), s_cal = m.vec_strength( "heading", 350, 500 ), e_cal = std::fabs( wrap180( m.vec_deg( "heading", 350, 500 ) - lm ) );
    const double r_st = m.hz( "heading", "still" ), s_st = m.vec_strength( "heading", 1300, 1500 ), d_st = wrap180( m.vec_deg( "heading", 1300, 1500 ) - lm );
    const double b_r = wrap180( m.vec_deg( "heading", 2400, 2600 ) - lm ), s_r = m.vec_strength( "heading", 2400, 2600 );
    const double b_l = wrap180( m.vec_deg( "heading", 3900, 4100 ) - lm ), s_l = m.vec_strength( "heading", 3900, 4100 );
    const double move_r = wrap180( b_r - d_st ), move_l = wrap180( b_l - b_r );
    p[ "landmark_deg" ] = lm;
    p[ "error_cal_deg" ] = e_cal;
    p[ "strength_cal" ] = s_cal;
    p[ "drift_still_deg" ] = d_st;
    p[ "shift_right_deg" ] = move_r;
    p[ "shift_left_deg" ] = move_l;
    p[ "error_end_deg" ] = std::fabs( b_l );
    const double lim = 45.0 * m.tol, need = 60.0 / m.tol;
    // A shifter that is too strong can carry the bump past half a circle, where +196 degrees reads as -164: the direction is taken from the start of the turn instead (see fly_compass.cpp).
    const double early_r = wrap180( m.vec_deg( "heading", 1700, 1850 ) - m.vec_deg( "heading", 1350, 1500 ) ), early_l = wrap180( m.vec_deg( "heading", 3200, 3350 ) - m.vec_deg( "heading", 2850, 3000 ) );
    const bool over_r = move_r < need && early_r >= 10.0 && ( move_r < 0 || move_r > 170 ), over_l = move_l > -need && early_l <= -10.0 && ( move_l > 0 || move_l < -170 );
    c.push_back( { "form", r_cal >= 3 && s_cal >= smin && e_cal <= lim, r_cal < 3 ? 0.0 : ramp( s_cal, 0, smin ) * 0.5 + 0.5 * ( 1.0 - ramp( e_cal, lim, 120.0 ) ),
      r_cal < 3 ? "Nothing much fires while the landmark is on: wire the landmark sensors (the cue family) to the ring cells."
                : "There is no single bump at the landmark. The ring needs local excitation and global inhibition (the previous level), wired in the order of the sensors." } );
    c.push_back( { "still", r_st >= 6 && s_st >= smin && std::fabs( d_st ) <= 30.0 * m.tol, r_st < 6 ? 0.0 : 1.0 - ramp( std::fabs( d_st ), 30.0 * m.tol, 120.0 ),
      r_st < 6 ? "The bump is gone when the fly is still: the ring does not hold it. Strengthen the excitation between neighbours."
               : s_st < smin ? "The activity spreads over the ring when the fly is still: the shifters push too hard, or the inhibition is too weak."
                             : "The bump drifts while the fly is still: with equal drive the left and right shifting groups must push with equal strength, in opposite directions (one step left, one step right), so that their pushes cancel." } );
    c.push_back( { "right", move_r >= need && move_r <= 170 && s_r >= smin * 0.8, move_r < 0 ? 0.1 : ramp( move_r, 0, need ),
      over_r ? "The bump moves too far in a turn to the right, past the half circle and round to the other side: the shifters push too hard. Weaken them."
      : move_r < -need * 0.5 ? "The bump moves the wrong way in a turn to the right: the group driven by 'turn_right' must excite the ring cell one step to the right (higher cell numbers)."
      : move_r < need ? "The bump hardly moves in a turn to the right: the group driven by 'turn_right' pushes too weakly. It must be driven by the sensor and by the bump together, and push the ring."
                      : "The bump moves too far, or breaks up, in a turn to the right: weaken the shifters." } );
    c.push_back( { "left", move_l <= -need && move_l >= -170 && s_l >= smin * 0.8 && std::fabs( b_l ) <= 70.0 * m.tol, move_l > 0 ? 0.1 : ramp( -move_l, 0, need ),
      over_l ? "The bump moves too far in a turn to the left, past the half circle and round to the other side: the shifters push too hard. Weaken them."
      : move_l > need * 0.5 ? "The bump moves the wrong way in a turn to the left: the group driven by 'turn_left' must excite the ring cell one step to the left (lower cell numbers)."
      : move_l > -need ? "The bump hardly moves back in a turn to the left: the left group pushes too weakly, or it is not driven by 'turn_left'. Each direction needs a group of its own."
                       : "The bump does not come back to the landmark after the turn to the left: the two directions must be equally strong." } );
  };
  s.ok_verdict = "A bump that moves: still when the drive is equal, moving to the right in a turn to the right and back in a turn to the left, by two shifting groups that each push one way.";
  s.h1 = "Two opposite pushes cancel; give one of them a little more drive and the bump moves. The angular velocity is the difference between the two sides.";
  s.h2 = "One group of shifting cells for each direction, each excited by the ring and by its own turn sensor, and each exciting the ring one step to its own side. Give the two groups equal strength, pushing in opposite directions, so that they cancel at rest.";
  s.h2_target = "heading";
  s.h3 = "Ring as before, plus two groups of shifting cells: cell k of the ring excites cell k of each group; 'turn_left' drives one group and 'turn_right' the other; the right group excites the ring cell k to cell k plus one, the left group "
         "cell k to cell k minus one. If the bump drifts when the fly is still, give the two groups equal strength, pushing in opposite directions. If it does not move, strengthen the turn sensors' drive or the shifters' output; if it breaks up, weaken them.";
  s.metric_name = "bump_shift_deg";
  s.metric_unit = "deg";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return wrap180( m.vec_deg( "heading", 2400, 2600 ) - m.vec_deg( "heading", 1300, 1500 ) ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_c_shift() { return std::make_unique< Landmark >( spec(), []( double t, bool ) { return heading_change( { { 1500, 2200, 150 }, { 3000, 3700, -150 } }, t ); } ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( c_shift, 531, "c_shift.json", make_c_shift ) }
