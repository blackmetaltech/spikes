// Task "leech_heart" (leech chapter): the heartbeat half-centre, and what makes it switch. Two heart interneurons (HN) inhibit each other and fire in alternating bursts; the rhythm
// has to come from the circuit, its period must fall in a band, and the lab then does the experiment that tells the two classical mechanisms of a half-centre apart (Skinner, Kopell &
// Marder 1994; Sharp, Skinner & Marder 1996): in RELEASE the active cell's burst ends because the active cell tires, so the active phase sets the pace; in ESCAPE the silenced cell
// comes back by itself while the other is still active, so the silent phase sets the pace. (research/bestiary-a.md section 2.)
//
// WORLD (dc sensor 'drive', level 2.0 x sensor weight, on from t = 0; full and quick episode 30 s)
//   At t = 0 the task starts the 'left' output cells at V_m = -50 mV and the 'right' ones at -75 mV, and the cells that inhibit them the other way round, so that the symmetry between
//   identical cells is broken the same way on every seed (the left motor side is released first).
//   BASELINE [3000, 15000) ms: the rhythm as the circuit makes it.
//   TEST CURRENT at 15000 ms: the experimenter injects a little extra current (5 % of the drive current each cell gets) into the cells whose activity goes with the left motor (the cells
//   that excite 'left' and inhibit 'right', found in the circuit's structure), as the dynamic-clamp experiments of the literature do; the rhythm is read again over [18000, 30000) ms.
//   Motors 'left' and 'right': the two sides' output cells (in the reference the heart motor neurons HE, each silenced by the interneuron of its own side).
// SCORING over the baseline, in 25 ms bins (a bin is 'left' when left spikes > right spikes, 'right' when right > left; empty bins are skipped)
//   switches   changes between left and right bins                      >= 4 (guided 3, researcher 5)
//   period_ms  the cycle time (twice the median interval between turns) in [900, 3500] ms (guided [735, 4285], researcher [1060, 2980])
//   overlap    bins where both fire / bins where either fires            <= 0.15 (guided 0.25, researcher 0.10)
//   balance    smaller side's share of all spikes                        >= 0.30
//   bursts     the mean number of spikes in a burst (spikes closer than 100 ms) of each side  >= 4: the cells burst, they do not tick
//   SIGNATURE  the median length of the left side's active phases (first to last spike of a burst) and silent phases (last spike to the next burst), baseline against test current:
//              dA = active(test) / active(baseline) - 1, dS = silent(test) / silent(baseline) - 1. The cells that were given more drive are the cells that are active when 'left' is.
//              RELEASE: dA >= 0.08 (guided about 0.05, researcher about 0.12) and dA > -dS: the cell that is active for longer when it is driven harder is the one whose tiring ends the burst.
//              ESCAPE:  -dS >= 0.08 (same scaling) and -dS > dA: the silenced cell returns sooner when it is driven harder.
//              Neither (both phases within the band): the cycle does not depend on the cells' own state. The rhythm must also go on alternating under the test current (>= 3 turns).
//   Measured on the reference over seeds 1-30 and 101-105 in the three modes: dA +19 to +26 %, dS -9 to -3 % (release), period 1775 to 2550 ms. No circuit of the parts bin was found whose
//   silent phase follows its own drive (docs/circuits/bestiary_a.md), so the escape branch has not been exercised on a real circuit.
// MECHANISM (Engine::structure()): the sides are read from different cells; each side inhibits the other (a cell excited by or equal to one side's cells reaches the other
// side by net inhibition, both ways); the cells behind the outputs have a slow recovery (an adapting or bursting cell type): without it the winner of mutual inhibition keeps
// the other silent for ever.
// TIME: the real isolated heartbeat oscillator cycles over many seconds and varies between animals; the numbers here are for a game level that must check in a few seconds, so the
// period band is a compressed one. It is a property of this level, not a measurement.
// Biology: Calabrese lab models of the leech heartbeat timing network (Nadim et al. 1995; Olsen et al. 1995; Hill et al. 2001, 2002); escape and release: Skinner, Kopell & Marder 1994;
// Sharp, Skinner & Marder 1996; see research/bestiary-a.md.
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_leech_heart();
namespace
{
using namespace bestiary;

constexpr double kW0 = 3000, kBase1 = 15000, kTiltAt = 15000, kW1 = 18000, kEnd = 30000, kBin = 25, kTiltFrac = 0.05, kGap = 300;

class LeechHeart : public Scripted
{
public:
  using Scripted::Scripted;

  void reset( uint64_t seed ) override
  {
    Scripted::reset( seed );
    tilted_ = false;
    tilt_cells_.clear();
    tilt_pA_ = 0;
  }

  Score score() const override
  {
    Score s = Scripted::score();
    if ( s.passed )
    {
      const double k = s.parts.count( "signature" ) ? s.parts.at( "signature" ) : 0.0;
      if ( k > 0.5 ) s.verdict = "A release oscillator: the two sides burst in turn with almost no overlap, and the side that was given more drive burst for longer: the active cell's own tiring is what ends its burst.";
      if ( k < -0.5 ) s.verdict = "An escape oscillator: the two sides burst in turn with almost no overlap, and the side that was given more drive came back sooner: the silenced cell's own recovery is what ends its silence.";
    }
    return s;
  }

protected:
  // The command reaches the left side a moment earlier: the left output cells start just over threshold (V_m -50 mV), the right ones at -75 mV; and the cells that inhibit an output
  // pool (the heart interneurons of that side) start the other way round, so that the left motor neurons are the first to be released. Without this, identical cells can start in
  // perfect step and an oscillator of mutual inhibition has no way to choose which side goes first.
  void on_start( Engine& e ) override
  {
    for ( const auto& o : e.outputs() )
    {
      if ( o.name != "left" && o.name != "right" ) continue;
      const bool left = o.name == "left";
      for ( int n : o.neurons ) e.set_neuron_param( n, "V_m", left ? -50.0 : -75.0 );
      for ( const auto& pr : g_.st.projections )
      {
        if ( pr.sign >= 0 || !functional( g_.st, pr ) || pr.pre_index < 0 ) continue;
        const auto& post = g_.st.populations[ static_cast< size_t >( pr.post_index ) ];
        bool feeds = false;
        for ( int n : o.neurons ) feeds = feeds || ( n >= post.first_index && n < post.first_index + post.n );
        if ( !feeds ) continue;
        const auto& pre = g_.st.populations[ static_cast< size_t >( pr.pre_index ) ];
        for ( int k = 0; k < pre.n; ++k ) e.set_neuron_param( pre.first_index + k, "V_m", left ? -75.0 : -50.0 );
      }
    }
    // the cells whose activity goes with the left motor: they excite (or are) the left output and inhibit the right one
    const auto l = g_.out_pops( "left" ), r = g_.out_pops( "right" );
    if ( !l.empty() && !r.empty() )
      for ( size_t p = 0; p < g_.st.populations.size(); ++p )
      {
        const std::vector< int > P = { static_cast< int >( p ) };
        if ( !g_.path( P, l, +1, 3, true ) || !g_.path( P, r, -1, 3 ) ) continue;
        const auto& pi = g_.st.populations[ p ];
        for ( int k = 0; k < pi.n; ++k ) tilt_cells_.push_back( pi.first_index + k );
      }
    // 5 % of the drive current one cell gets (the sensor's weight times the level the task sets): the same relative nudge whatever the cell and the weight the player chose
    tilt_pA_ = std::max( 5.0, kTiltFrac * std::fabs( e.input_weight( "drive" ) ) * 2.0 );
  }

  void drive( Engine& e, double dt ) override
  {
    Scripted::drive( e, dt );
    if ( tilted_ || t_ < kTiltAt ) return;
    tilted_ = true;
    for ( int n : tilt_cells_ )
    {
      double cur = e.neuron_state( n, "I_e" );
      if ( std::isnan( cur ) ) cur = 0.0;
      e.set_neuron_param( n, "I_e", cur + tilt_pA_ );
    }
  }

private:
  bool tilted_ = false;
  std::vector< int > tilt_cells_;
  double tilt_pA_ = 0;
};

struct Stats
{
  double switches = 0, overlap = 1, balance = 0, period = 0, burst_l = 0, burst_r = 0;
};

Stats stats( const Meas& m, double w0, double w1 )
{
  Stats st;
  const auto& l = m.cnt.at( "left" );
  const auto& r = m.cnt.at( "right" );
  const int nb = static_cast< int >( ( w1 - w0 ) / kBin );
  int last = 0, active = 0, both = 0;
  std::vector< double > times;   // the bins at which the winning side changed
  double tl = 0, tr = 0;
  for ( int b = 0; b < nb; ++b )
  {
    double x = 0, y = 0;
    for ( int k = 0; k < static_cast< int >( kBin ); ++k )
    {
      const size_t i = static_cast< size_t >( w0 + b * kBin + k );
      if ( i < l.size() ) x += l[ i ];
      if ( i < r.size() ) y += r[ i ];
    }
    tl += x;
    tr += y;
    if ( x > 0 || y > 0 ) ++active;
    if ( x > 0 && y > 0 ) ++both;
    const int d = x > y ? 1 : ( y > x ? -1 : 0 );
    if ( d == 0 ) continue;
    if ( last != 0 && d != last )
    {
      st.switches += 1;
      times.push_back( w0 + b * kBin );
    }
    last = d;
  }
  st.overlap = active > 0 ? static_cast< double >( both ) / active : 1.0;
  st.balance = tl + tr > 0 ? std::min( tl, tr ) / ( tl + tr ) : 0.0;
  // the cycle time from the MEDIAN interval between turns (two turns make a cycle), not from the window length over the number of turns: that one is quantised by the window and by a
  // missed or an extra turn at its edges, and a quick episode (shorter window) would read a different period from the full one for the same circuit
  if ( times.size() >= 2 )
  {
    std::vector< double > iv;
    for ( size_t k = 1; k < times.size(); ++k ) iv.push_back( times[ k ] - times[ k - 1 ] );
    std::sort( iv.begin(), iv.end() );
    const size_t h = iv.size() / 2;
    st.period = 2.0 * ( iv.size() % 2 ? iv[ h ] : 0.5 * ( iv[ h - 1 ] + iv[ h ] ) );
  }
  auto mean_burst = [ & ]( const std::vector< float >& v ) {
    const auto rs = runs_of( v, w0, w1, 100 );
    double s = 0;
    for ( int x : rs ) s += x;
    return rs.empty() ? 0.0 : s / static_cast< double >( rs.size() );
  };
  st.burst_l = mean_burst( l );
  st.burst_r = mean_burst( r );
  return st;
}

/// The left side's phases in [w0, w1): the active phases (first to last spike of a burst) and the silent phases between bursts. A burst counts when it has at least three spikes and a
/// silence of at least kGap ms on both sides inside the window (so a burst cut by the window edge is not read as a short one).
struct Phases
{
  std::vector< double > active, silent;
};
Phases phases_of( const std::vector< float >& v, double w0, double w1 )
{
  struct B
  {
    double t0, t1;
    int n;
  };
  std::vector< B > bs;
  double last = -1e9;
  for ( long k = static_cast< long >( w0 ); k < static_cast< long >( w1 ) && k < static_cast< long >( v.size() ); ++k )
  {
    const int c = static_cast< int >( v[ static_cast< size_t >( k ) ] + 0.5f );
    if ( c <= 0 ) continue;
    const double t = static_cast< double >( k );
    if ( bs.empty() || t - last > kGap ) bs.push_back( { t, t, 0 } );
    bs.back().t1 = t;
    bs.back().n += c;
    last = t;
  }
  std::vector< B > good;
  for ( const auto& b : bs )
    if ( b.n >= 3 ) good.push_back( b );
  Phases p;
  for ( size_t i = 0; i < good.size(); ++i )
  {
    const bool inner = good[ i ].t0 > w0 + kGap && good[ i ].t1 < w1 - kGap;
    if ( inner ) p.active.push_back( good[ i ].t1 - good[ i ].t0 );
    if ( i + 1 < good.size() && good[ i ].t0 > w0 + kGap && good[ i + 1 ].t1 < w1 - kGap ) p.silent.push_back( good[ i + 1 ].t0 - good[ i ].t1 );
  }
  return p;
}
double median_of( std::vector< double > v )
{
  if ( v.empty() ) return 0.0;
  std::sort( v.begin(), v.end() );
  const size_t h = v.size() / 2;
  return v.size() % 2 ? v[ h ] : 0.5 * ( v[ h - 1 ] + v[ h ] );
}

struct Signature
{
  bool measured = false;
  double dA = 0, dS = 0;
  int kind = 0;   // +1 release, -1 escape, 0 neither
};
/// Active and silent phase of the left side with and without the test current. Needs two complete active phases and one silent phase in both windows.
Signature signature( const Meas& m, double tol )
{
  Signature g;
  const auto& l = m.cnt.at( "left" );
  const Phases b = phases_of( l, kW0, kBase1 ), t = phases_of( l, kW1, kEnd );
  if ( b.active.size() < 2 || b.silent.size() < 1 || t.active.size() < 2 || t.silent.size() < 1 ) return g;
  g.measured = true;
  g.dA = median_of( t.active ) / median_of( b.active ) - 1.0;
  g.dS = median_of( t.silent ) / median_of( b.silent ) - 1.0;
  const double need = 0.08 / std::sqrt( tol ) * ( tol > 1.2 ? 0.8 : 1.0 ) * ( tol < 0.9 ? 1.25 : 1.0 );   // guided about 0.05, standard 0.08, researcher about 0.12
  if ( g.dA >= need && g.dA > -g.dS ) g.kind = 1;
  else if ( -g.dS >= need && -g.dS > g.dA ) g.kind = -1;
  return g;
}

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "leech_heart";
  i.title = "Leech: the heartbeat";
  i.brief = "The leech's heart is paced by a small oscillator in its nerve cord: two heart interneurons (HN) that inhibit each other and fire in alternating bursts, one side "
            "silent while the other fires, then the other way round. Each interneuron inhibits the heart motor neuron (HE) of its own side, so the two motor neurons beat in turn. "
            "A steady 'drive' excites all the cells equally; the rhythm has to come from the circuit. Build the two interneurons and the two motor neurons and read the motor neurons "
            "on 'left' and 'right': the two sides must alternate (clean turns, almost never firing together, neither side winning for good), each side must fire in bursts, and a "
            "full cycle must take a few seconds. Two plain interneurons that inhibit each other do not alternate: the one that fires first silences the other for ever, so each "
            "needs a slow process of its own. There are two ways a half-centre can turn: the active cell can tire (release), or the silenced cell can recover and come back "
            "while the other is still firing (escape). They look alike in the bursts. So after fifteen seconds the lab does an experiment: it injects a little extra current into "
            "the cells of the left side and reads how the rhythm moves. If the cell that was driven harder bursts for longer, the burst is ended by that cell tiring; if it comes "
            "back sooner, its own recovery is what brings it back. The cells must show one of the two clearly, and go on alternating. The real heartbeat cycle is much slower than "
            "this level's: the game compresses time so that a check takes seconds.";
  i.chapter = "leech";
  i.sensors = { "drive" };
  i.motors = { "left", "right" };
  i.duration_ms = kEnd;
  i.concepts_new = { "escape_release" };
  i.concepts_used = { "half_centre", "bursting", "rebound", "adaptation", "inhibition" };
  i.par_neurons = 4;
  i.default_model = "aeif_cond_alpha";
  i.kinds = { { "drive", micro::dc( 2.0, 300.0 ) }, { "left", micro::rate() }, { "right", micro::rate() } };
  i.bands = { { "period_s", "s", 0.9, 3.5 }, { "overlap_pct", "%", 0, 15 } };
  i.recipe = "Four adaptive exponential cells. HN_L and HN_R: a = 0, b = 5, tau_w = 1500, V_reset = -58; HE_L and HE_R: a = 0, b = 0 (fast cells). drive > all four at 480 pA per unit (the "
             "task sets level 2.0). HN_L > HN_R and HN_R > HN_L at -50 nS (both interneurons inhibitory); HN_L > HE_L and HN_R > HE_R at -300 nS; HE_L > 'left', HE_R > 'right'. "
             "Each side bursts for about a second and gives way to the other, a cycle of about two seconds. (The task starts the left motor neurons slightly depolarised, and the "
             "interneurons that inhibit them the other way round, so that the left side is released first.) This is a RELEASE oscillator: the extra current makes the driven side's burst "
             "longer by about a quarter. The parts bin has no cell whose silent phase follows its own drive, so there is no reference for the escape kind.";
  i.note = { "Escape or release?",
    "The leech heart is driven by a timing network in a few ganglia of the nerve cord. In each segmental oscillator two heart interneurons, HN cells, inhibit each other "
    "reciprocally; each fires a burst while the other is silent. Theory and dynamic-clamp experiments on pairs of reciprocally inhibitory cells distinguish how such a pair turns: "
    "by release, when the active cell's burst ends because of its own slow process and the silenced cell is let go, or by escape, when the silenced cell's own slow inward "
    "current brings it back against the inhibition. The two are told apart by how the period responds to a change of one cell's drive or of the synaptic threshold. In the "
    "published model of the leech oscillator the inhibition itself activates an inward current in the silenced cell, so the silenced cell has a hand in the turn. This level is "
    "the single oscillator, with spiking cells and a compressed time scale; the real inhibition is partly graded (transmission that depends smoothly on the presynaptic voltage), "
    "which the game's spike-based synapses do not model.",
    "Skinner FK, Kopell N, Marder E (1994) J Comput Neurosci 1:69-87, doi:10.1007/BF00962719; Sharp AA, Skinner FK, Marder E (1996) J Neurophysiol 76:867-883, doi:10.1152/jn.1996.76.2.867; "
    "Nadim F, Olsen OH, De Schutter E, Calabrese RL (1995) J Comput Neurosci 2:215-235, doi:10.1007/BF00961435; Hill AA, Lu J, Masino MA, Olsen OH, Calabrese RL (2001) J Comput Neurosci 10:281-302, doi:10.1023/a:1011216131638" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = kEnd;   // the quick check is the full episode: four cells run it in about two and a half seconds, and a shorter window passed circuits whose overlap and period drift later
    c.segs = { { "drive", 0, c.dur, 2.0 } };
    c.wins = { { "window", kW0, kBase1 } };
    return c;
  };
  s.gauges = { { "period_s", { "period_ms", 0.001, true, kW0 + 500 } }, { "overlap_pct", { "overlap_pct", 1.0, false, kW0 + 500 } } };
  s.mech = []( const Graph& g ) -> std::string {
    const auto l = g.out_pops( "left" ), r = g.out_pops( "right" );
    if ( l.empty() || r.empty() ) return "";
    if ( Graph::overlap( l, r ) ) return "'left' and 'right' are read from the same cell: the oscillator has two sides, so each needs its own cell.";
    if ( !g.crossed( l, r ) && !g.crossed( r, l ) )
      return "The two sides do not inhibit each other at all: the heartbeat oscillator is built on mutual inhibition (each side's bursts silencing the other).";
    if ( !g.crossed( l, r ) || !g.crossed( r, l ) )
      return "The inhibition runs one way only: one side silences the other, which cannot answer back. Each side must inhibit the other.";
    // the cells that do the inhibiting (every population with an inhibitory projection out of it) need a slow process of their own
    bool any = false, slow = false;
    for ( const auto& pr : g.st.projections )
      if ( pr.sign < 0 && functional( g.st, pr ) )
      {
        any = true;
        const std::string& m = g.pop( pr.pre_index ).model;
        if ( !is_one_of( m, { "iaf_psc_alpha", "iaf_psc_exp", "iaf_psc_delta", "iaf_cond_alpha", "iaf_cond_exp", "parrot_neuron" } ) ) slow = true;
      }
    if ( any && !slow )
      return "The cells that do the inhibiting are plain integrate-and-fire cells: they have no slow process to wear out a burst or to bring a silenced cell back, so whichever side fires first "
             "silences the other for ever. Use a cell type with a slow variable for them.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const Stats st = stats( m, kW0, std::min( m.dur, kBase1 ) );
    p[ "switches" ] = st.switches;
    p[ "period_ms" ] = st.period;
    p[ "overlap_pct" ] = 100.0 * st.overlap;
    p[ "balance" ] = st.balance;
    p[ "burst_left" ] = st.burst_l;
    p[ "burst_right" ] = st.burst_r;
    const double tl = m.n( "left", "window" ), tr = m.n( "right", "window" );
    const double sw_need = m.tol > 1.2 ? 3 : ( m.tol < 0.9 ? 5 : 4 );
    c.push_back( { "fires", tl >= 1 && tr >= 1, 0.5 * ( ( tl >= 1 ? 0.5 : 0.0 ) + ( tr >= 1 ? 0.5 : 0.0 ) ),
      tl + tr < 1 ? "Nothing fired in the analysis window. The drive must reach both cells and be over their rheobase: strengthen the drive."
                  : std::string( "Only the " ) + ( tl >= 1 ? "left" : "right" ) + " side fires: the other is silenced for good (or never got the drive). The side that is silenced needs a way back: slow adaptation in "
                    "the active cell so that it gives way, and the drive on both cells." } );
    c.push_back( { "balance", st.balance >= 0.30, ramp( st.balance, 0, 0.30 ), "One side wins and keeps the other mostly silent. Each cell needs slow adaptation so that the active side tires and lets the other go." } );
    c.push_back( { "overlap", st.overlap <= m.hi( 0.15 ), 1.0 - ramp( st.overlap, 0.15, 0.6 ), "Both sides fire together much of the time: they are not inhibiting each other enough. Check that both cells are inhibitory and connected to each other, and strengthen the links." } );
    c.push_back( { "switches", st.switches >= sw_need, ramp( st.switches, 0, sw_need ), "The sides alternate too seldom: the active side holds on for too long. More adaptation makes a side tire sooner, or weaker inhibition lets the other side in sooner." } );
    const double lo = 900 / std::sqrt( m.tol ), hi = 3500 * std::sqrt( m.tol );
    const bool in_band = st.period >= lo && st.period <= hi;
    c.push_back( { "period", in_band, st.period <= 0 ? 0.0 : 1.0 - clampd( std::fabs( std::log( st.period / clampd( st.period, lo, hi ) ) ), 0.0, 1.0 ),
      st.period > hi ? "The cycle is too slow for a heartbeat: each side holds on for too long. Speed up the adaptation (a shorter time constant) or strengthen its effect."
                     : "The cycle is too fast: the sides swap before a real burst has formed. Slow the adaptation (a longer time constant) so that each side bursts for longer." } );
    c.push_back( { "bursts", st.burst_l >= 4 && st.burst_r >= 4, ramp( std::min( st.burst_l, st.burst_r ), 0, 4 ), "The sides take turns, but each fires only one or two spikes at a time: the heart interneurons fire bursts. Drive the cells harder, or give them adaptation that ends a burst rather than a single spike." } );
    if ( m.dur < kEnd - 1e-6 ) return;   // the live gauges read the judge while the episode runs: the test current has not been read yet
    const Signature g = signature( m, m.tol );
    const Stats tt = stats( m, kW1, kEnd );
    p[ "tilt_active_pct" ] = 100.0 * g.dA;
    p[ "tilt_silent_pct" ] = 100.0 * g.dS;
    p[ "signature" ] = g.kind;
    p[ "tilt_switches" ] = tt.switches;
    c.push_back( { "tilt_alternates", tt.switches >= 3, ramp( tt.switches, 0, 3 ),
      "With the test current the rhythm stops alternating: one side takes over for good. A half-centre must survive a small difference between its two cells." } );
    c.push_back( { "signature", g.kind != 0, g.measured ? 0.3 : 0.0,
      g.measured ? "The experimenter gave the left side a little more drive and neither of its phases moved: the cycle does not depend on what the cells themselves are doing. In a half-centre the "
                   "phase that sets the pace belongs to a cell's own slow process, so a cell driven harder must either burst for longer (its tiring ends the burst) or come back sooner (its recovery ends its silence)."
                 : "Too few complete bursts to read how the rhythm responds to the test current: the cycle must be fast enough to show several turns in each half of the run." } );
  };
  s.ok_verdict = "A half-centre oscillator: the two sides burst in turn with almost no overlap, at a steady cycle.";
  s.h1 = "A half-centre needs two things: mutual inhibition, so that the side that fires silences the other, and a slow process that gives the silenced side its turn. The slow process can wear the active side out, or bring the silenced side back.";
  s.h2 = "Each side is a cell that inhibits the other, with a slow adaptation; the steady drive goes to both. The cycle time is set by how slowly the adaptation builds up and decays. When one side is driven a little harder, ask which phase of it changes.";
  s.h2_target = "left";
  s.h3 = "drive to both sides; HNL and HNR adaptive cells that inhibit each other; HNL read as 'left' and HNR as 'right'. If one side wins for good, the adaptation is too weak; if both fire together, the "
         "inhibition is too weak; if the cycle is too slow, shorten the adaptation time constant, and if it is too fast, lengthen it. If the test current moves nothing, the cells are saturated or the rhythm is not theirs.";
  s.metric_name = "period_ms";
  s.metric_unit = "ms";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return stats( m, kW0, std::min( m.dur, kBase1 ) ).period; };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_leech_heart() { return std::make_unique< LeechHeart >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( leech_heart, 512, "leech_heart.json", make_leech_heart ) }
