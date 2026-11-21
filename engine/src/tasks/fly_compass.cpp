// Task "fly_compass" (fly compass chapter): the head-direction circuit of the fly. A landmark sets the heading, then the lights go out and the fly turns; the compass has to hold the
// heading in the dark and follow the turns by integrating the angular velocity. The wiring is a model after the central-complex cell types (E-PG ring, P-EN left and right shifters,
// Delta7 global inhibition), not synapse-level data.
//
// WORLD (Poisson sensors cue0..cue15: the landmark, one per cell of the 16-cell ring, 300 Hz; dc sensors turn_left / turn_right: a tonic level of 40 units on both and, during a turn, the side of the turn
// gets 0.25 units per degree/second more and the other side as much less; the ring is read on 'heading' as a population vector, cell k at 22.5 k degrees; 9200 ms, quick 5000 ms)
//   cal    [0, 500)   the landmark, at the bearing of cell k (the seed picks k); heading = that bearing; the bump must form at it (error <= 45 deg)
//   dark   from 500   the lights are out; the fly's turns (degrees per second, + = to the right, the direction of increasing cell index):
//            hold      [500, 1500)    no turn: the bump must stay (error <= 30 deg)                              window [1300, 1500)
//            turn 1    [1500, 2500)   to the right, 150 deg/s                                                       window [2700, 2900)
//            turn 2    [3300, 4300)   to the left, 150 deg/s (back at the landmark)                                 window [4500, 4700)
//            (full episode only) turn 3 [5500, 6700) to the right, 120 deg/s, window [6900, 7100); turn 4 [7700, 8500) to the left, 170 deg/s, window [8700, 8900)
//   The true heading is the landmark's bearing plus the turns so far; the error is the circular distance between the bump angle (circular mean over the window) and the true heading.
// SCORING  form, hold (stays in the dark), direction (the bump turns the way the fly does), track (error at every checkpoint after a turn <= 65 deg; guided 90; and the bump is still one bump).
//   Mode noise (researcher) scales the sensor levels. Slow turns (below about 60 deg/s) are inside the dead zone of this discrete ring and are not part of the world.
// MECHANISM (Engine::structure()): 'heading' is a population-vector readout of the sixteen ring cells; the ring excites itself in a loop and is inhibited broadly; 'turn_left' and 'turn_right' reach two
// separate groups of shifting cells, each excited by the ring and exciting the ring; the landmark sensors reach the ring.
// Biology: Seelig & Jayaraman 2015 Nature 521:186 (the bump in the dark follows self-motion; angular path integration); Turner-Evans et al. 2017 eLife 6:e23496 and Green et al. 2017 Nature
// 546:101 (P-EN cells shift the bump in proportion to angular velocity; silencing them breaks tracking in the dark; each side shifts one way). Real P-EN cells receive angular velocity as the
// difference between the two sides, as here. Cell counts and wiring are those of a ring-attractor model, after the cell types (Turner-Evans / Hulse), not synapse-level data.
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_fly_compass();
namespace
{
using namespace bestiary;

struct Check
{
  const char* key;
  double t0, t1;
  bool full_only;
};
const std::vector< Turn >& turns( bool quick )
{
  static const std::vector< Turn > q = { { 1500, 2500, 150 }, { 3300, 4300, -150 } };
  static const std::vector< Turn > f = { { 1500, 2500, 150 }, { 3300, 4300, -150 }, { 5500, 6700, 120 }, { 7700, 8500, -170 } };
  return quick ? q : f;
}
const std::vector< Check >& checks()
{
  static const std::vector< Check > c = { { "k1", 2700, 2900, false }, { "k2", 4500, 4700, false }, { "k3", 6900, 7100, true }, { "k4", 8700, 8900, true } };
  return c;
}
constexpr double kQuickEnd = 5000, kFullEnd = 9200;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "fly_compass";
  i.title = "Fly: the compass in the dark";
  i.brief = "A fly keeps track of where it is facing with a ring of compass cells: a bump of activity whose position is its heading. A landmark sets the bump; then the lights go out and the fly "
            "turns, and the bump has to hold its place when the fly is still and move round the ring when the fly turns, as fast as the fly turns. This is angular path integration. The wiring "
            "is a model after the central-complex cell types (after Turner-Evans/Hulse cell types, not synapse-level data). Build the ring from the previous level: sixteen compass cells (E-PG) that "
            "excite their neighbours, with global inhibition (Delta7). Add two groups of shifting cells (P-EN), a left group and a right group. Each group listens to the compass cells at the "
            "bump and to its own turn sensor ('turn_left' or 'turn_right'), and excites the compass cells one step to its own side. Both turn sensors carry a steady background and the side of "
            "the turn carries more: when the fly is still the two groups push equally and the bump stays; during a turn one group pushes harder and the bump moves. Read 'heading' as a "
            "population vector over the ring. You are scored on the heading error in the dark: how far the bump is from the true heading, after holding still and after each turn. "
            "A ring alone cannot move; shifters on one side cannot turn both ways. Each unit of a turn sensor is only a small current: the weight from a turn sensor onto the shifting cells must be small compared with the ring's own weights (a few picoamps per unit, not hundreds).";
  i.chapter = "fly_compass";
  i.view = "compass";
  i.sensors = cue_family();
  i.sensors.push_back( "turn_left" );
  i.sensors.push_back( "turn_right" );
  i.motors = { "heading" };
  i.duration_ms = kFullEnd;
  i.concepts_new = {};
  i.concepts_used = { "ring_attractor", "path_integration", "inhibition", "persistence" };
  i.par_neurons = 60;   // the reference has 52 cells; the budget leaves room for a larger inhibitory group
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "heading", ring_kind() }, { "turn_left", in_kind( "dc", kShiftBase, 3 ) }, { "turn_right", in_kind( "dc", kShiftBase, 3 ) } };
  for ( int k = 0; k < kWedges; ++k ) i.kinds[ "cue" + std::to_string( k ) ] = in_kind( "poisson", 300, 400 );
  i.bands = { { "heading_error_deg", "deg", 0, 65 } };
  i.recipe = "Fifty-two integrate-and-fire cells: EPG (16 cells, the ring: bias 320 pA, tau syn E 15 ms), DELTA (4 cells), PENL and PENR (16 cells each, default cells). cue0 to cue15 > EPG, sensor k to cell k, at "
             "400 pA. EPG > EPG with segment wiring to the cell on each side (shifts of minus one and plus one) at +100 pA; EPG > DELTA at +150 pA; DELTA > EPG at -400 pA. EPG > PENL and EPG > PENR with segment "
             "wiring (cell k to cell k) at +300 pA; turn_left > PENL and turn_right > PENR at 3 pA per unit of the sensor; PENR > EPG with segment wiring shifted by +1 and PENL > EPG shifted by -1 at "
             "+110 pA. 'heading' reads EPG as a population vector, cells in order 0 to 15. Real turning is not linear in this circuit for slow turns: below about half the speeds of the level the bump does not move.";
  i.note = { "Path integration in the fly's heading circuit",
    "The fly's compass is a bump of activity on a ring of E-PG cells. In the dark it still follows the animal's turns: shifting cells (P-EN) receive the angular velocity, and the left "
    "and right subtypes each push the bump one step to their own side, so the bump moves at a speed proportional to the turn. Silencing the shifting cells breaks tracking in the dark, "
    "and activating them shifts the bump in a predictable way. This level is a ring-attractor model after the published cell types, not synapse-level wiring.",
    "Turner-Evans D et al. (2017) eLife 6:e23496, doi:10.7554/eLife.23496; Green J et al. (2017) Nature 546:101-106, doi:10.1038/nature22343" };
  s.vec = { "heading" };
  s.schedule = [ & ]( bool quick ) {
    Schedule c;
    c.dur = quick ? kQuickEnd : kFullEnd;
    c.segs.push_back( { "cue", 0, 500, 300 } );
    shifter_segments( turns( quick ), c.dur, c.segs );
    c.wins = { { "cal", 350, 500 }, { "hold", 1300, 1500 } };
    for ( const auto& k : checks() )
      if ( !k.full_only || !quick ) c.wins.push_back( { k.key, k.t0, k.t1 } );
    return c;
  };
  s.timeline = []() -> std::vector< Task::Event > {
    std::vector< Task::Event > v = { { 0, 500, "landmark" } };
    for ( const auto& u : turns( false ) ) v.push_back( { u.t0, u.t1 - u.t0, u.dps > 0 ? "turn right" : "turn left" } );
    return v;
  };
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
    const bool quick = m.dur < 7000;
    const auto& tr = turns( quick );
    const double lim = 45.0 * m.tol;
    const double tl = m.tol > 1.2 ? 90.0 : 65.0;
    const double e_cal = std::fabs( wrap180( m.vec_deg( "heading", 350, 500 ) - lm ) ), s_cal = m.vec_strength( "heading", 350, 500 ), r_cal = m.hz( "heading", "cal" );
    const double e_hold = std::fabs( wrap180( m.vec_deg( "heading", 1300, 1500 ) - lm ) ), s_hold = m.vec_strength( "heading", 1300, 1500 ), r_hold = m.hz( "heading", "hold" );
    p[ "landmark_deg" ] = lm;
    p[ "error_cal_deg" ] = e_cal;
    p[ "strength_cal" ] = s_cal;
    p[ "error_hold_deg" ] = e_hold;
    p[ "strength_hold" ] = s_hold;
    double worst = 0, sum = 0, minstr = 1, gain_lo = 1e9, gain_hi = 0, prev_bump = m.vec_deg( "heading", 1300, 1500 ), prev_true = lm;
    int n = 0, wrong_way = 0, short_move = 0, long_move = 0, turn_idx = 0;
    double minrate = 1e9;
    for ( const auto& k : checks() )
    {
      if ( k.full_only && quick ) continue;
      const double truth = lm + heading_change( tr, ( k.t0 + k.t1 ) / 2 );
      const double bump = m.vec_deg( "heading", k.t0, k.t1 );
      const double e = std::fabs( wrap180( bump - truth ) );
      worst = std::max( worst, e );
      sum += e;
      ++n;
      minstr = std::min( minstr, m.vec_strength( "heading", k.t0, k.t1 ) );
      minrate = std::min( minrate, m.hz( "heading", k.key ) );
      p[ std::string( "error_" ) + k.key + "_deg" ] = e;
      p[ std::string( "bump_" ) + k.key + "_deg" ] = wrap180( bump - lm );
      const double dt = truth - prev_true, db = wrap180( bump - prev_bump );
      // The sign of a move that ends more than half a circle away is ambiguous (+196 looks like -164), so the DIRECTION is read off the start of the turn instead, where the bump has moved
      // only a little: the bump just before the turn against the bump 200-350 ms into it. A shifter that is too strong moves the bump the right way, fast; it is "too strong", not "wrong way".
      const double t_turn = turn_idx < static_cast< int >( tr.size() ) ? tr[ turn_idx ].t0 : k.t0;
      ++turn_idx;
      const double early = wrap180( m.vec_deg( "heading", t_turn + 200, t_turn + 350 ) - m.vec_deg( "heading", t_turn - 150, t_turn ) );
      const bool early_known = std::fabs( early ) >= 10.0, early_right_way = early * dt > 0;
      if ( std::fabs( dt ) > 20 )
      {
        if ( early_known && early_right_way && ( db * dt < 0 || std::fabs( db ) > std::fabs( dt ) * 1.6 ) ) ++long_move;   // right way, too far (past the half circle or not)
        else if ( early_known && !early_right_way ) ++wrong_way;
        else if ( db * dt < 0 && std::fabs( db ) > 30 && std::fabs( db ) < 150 ) ++wrong_way;
        else if ( db * dt < 0 && std::fabs( db ) >= 150 ) ++long_move;   // round the half circle: it overshot, it did not turn the other way
        else if ( std::fabs( db ) < std::fabs( dt ) * 0.5 ) ++short_move;
        else if ( std::fabs( db ) > std::fabs( dt ) * 1.6 ) ++long_move;
      }
      prev_bump = bump;
      prev_true = truth;
    }
    p[ "error_worst_deg" ] = worst;
    p[ "error_mean_deg" ] = n ? sum / n : 0.0;
    p[ "strength_min" ] = minstr;
    p[ "wrong_way" ] = wrong_way;
    p[ "too_short" ] = short_move;
    p[ "too_long" ] = long_move;
    (void)gain_lo;
    (void)gain_hi;
    const double smin = 0.4 / std::sqrt( m.tol );
    c.push_back( { "form", r_cal >= 3 && s_cal >= smin && e_cal <= lim, r_cal < 3 ? 0.0 : ramp( s_cal, 0, smin ) * 0.5 + 0.5 * ( 1.0 - ramp( e_cal, lim, 120.0 ) ),
      r_cal < 3 ? "Nothing much fires while the landmark is on: the landmark sensors do not reach the ring. Wire the cue sensors to the ring cells."
                : "There is no single bump at the landmark: activity covers the ring, or the bump sits elsewhere. The ring needs local excitation and global inhibition, wired in the order of the sensors." } );
    c.push_back( { "hold", r_hold >= 6 && s_hold >= smin && e_hold <= 30.0 * m.tol, r_hold < 6 ? 0.0 : 1.0 - ramp( e_hold, 30.0 * m.tol, 120.0 ),
      r_hold < 6 ? "The bump is gone in the dark: the ring does not keep itself going once the landmark has gone. Strengthen the excitation between neighbours."
                 : s_hold < smin ? "In the dark the activity spreads over the ring instead of staying a bump: the inhibition must be broader or stronger, and the two shifting groups must push the ring equally."
                                 : "The bump drifts away from the heading while the fly is still. When the fly does not turn, the left and right shifting groups must push with equal strength in opposite directions, so that their pushes cancel." } );
    if ( n > 0 )
    {
      c.push_back( { "direction", wrong_way == 0, wrong_way == 0 ? 1.0 : 0.2,
        "The bump moves the wrong way when the fly turns: the group that follows a right turn must push the bump to higher cell numbers (one step to the right), and the left group to lower ones." } );
      c.push_back( { "track", worst <= tl && minstr >= smin * 0.8 && minrate >= 4, 1.0 - ramp( worst, tl, 120.0 ),
        minrate < 4 || minstr < smin * 0.8 ? "During or after a turn the bump is lost: the shifters push so hard (or so little) that the ring cannot hold a single bump. Balance the push of the shifters against the ring."
        : short_move > 0 ? "The bump does not move as far as the fly turns: the heading error grows with every turn. Strengthen the shifters' drive from the turn sensor and onto the ring."
        : long_move > 0 ? "The bump overshoots: it moves further than the fly turns. Weaken the shifters' drive."
                        : "The bump does not keep up with the fly's turns: after the turns it is not at the true heading. Tune the gain of the shifters: how far the bump moves for a given turn." } );
    }
  };
  s.ok_verdict = "A compass that integrates: the landmark sets the bump, it holds its place in the dark, and it follows the fly's turns to the right and to the left by shifting one step at a time.";
  s.h1 = "Holding a heading needs a ring attractor; following turns needs two shifters, one for each direction, driven by the angular velocity and each pushing the bump one step to its own side.";
  s.h2 = "The two groups of shifting cells listen to the ring (so that only the cells at the bump are pushed) and to their own turn sensor, and each excites the ring one step to one side. The sensors carry a steady background: give the two groups equal strength, pushing in opposite directions, so that their pushes cancel and the bump stays when the fly is still.";
  s.h2_target = "heading";
  s.h3 = "Ring, with global inhibition, as before. Two shifting groups, each excited by the ring cell for cell and by its own turn sensor; the right group excites the ring cell k to cell k plus one, the left group cell k to cell k minus one. "
         "If the bump drifts when the fly is still, the two groups are not equal in strength (they must push with equal strength in opposite directions). If it falls behind the fly, strengthen the turn sensors' drive onto the shifters or the shifters' drive onto the ring; if it runs ahead, weaken them.";
  s.metric_name = "error_deg";
  s.metric_unit = "deg";
  s.metric = []( const Meas& m ) {
    const bool quick = m.dur < 7000;
    const double lm = m.extra.count( "landmark_deg" ) ? m.extra.at( "landmark_deg" ) : 0.0;
    double sum = 0;
    int n = 0;
    for ( const auto& k : checks() )
    {
      if ( k.full_only && quick ) continue;
      sum += std::fabs( wrap180( m.vec_deg( "heading", k.t0, k.t1 ) - ( lm + heading_change( turns( quick ), ( k.t0 + k.t1 ) / 2 ) ) ) );
      ++n;
    }
    return n ? sum / n : 0.0;
  };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_fly_compass() { return std::make_unique< Landmark >( spec(), []( double t, bool quick ) { return heading_change( turns( quick ), t ); } ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( fly_compass, 532, "fly_compass.json", make_fly_compass ) }
