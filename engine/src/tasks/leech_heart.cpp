// Task "leech_heart" (leech chapter): the heartbeat half-centre. Two heart interneurons (HN) inhibit each other and fire in alternating bursts; the rhythm has to come from the
// circuit and its period must fall in a band. (docs/research/bestiary-a.md section 2.)
//
// WORLD (dc sensor 'drive', level 2.0 x sensor weight, on from t = 0; full episode 14 s, quick episode 9 s: the analysis window is (3000 ms, end])
//   At t = 0 the task starts the 'left' output cells at V_m = -50 mV and the 'right' ones at -75 mV, and the cells that inhibit them the other way round, so that the symmetry between
//   identical cells is broken the same way on every seed (the left motor side is released first).
//   Motors 'left' and 'right': the two sides' HN cells.
// SCORING over the analysis window, in 25 ms bins (a bin is 'left' when left spikes > right spikes, 'right' when right > left; empty bins are skipped)
//   switches   changes between left and right bins                      >= 4 (guided 3, researcher 5)
//   period_ms  window length / (switches / 2): the cycle time            in [900, 3500] ms (guided [735, 4285], researcher [1060, 2980])
//   overlap    bins where both fire / bins where either fires            <= 0.15 (guided 0.25, researcher 0.10)
//   balance    smaller side's share of all spikes                        >= 0.30
//   bursts     the mean number of spikes in a burst (spikes closer than 100 ms) of each side  >= 4: the cells burst, they do not tick
// MECHANISM (Engine::structure()): the sides are read from different cells; each side inhibits the other (a cell excited by or equal to one side's cells reaches the other
// side by net inhibition, both ways); the cells behind the outputs have a slow recovery (an adapting or bursting cell type): without it the winner of mutual inhibition keeps
// the other silent for ever.
// TIME: the real isolated heartbeat oscillator cycles over many seconds and varies between animals; the numbers here are for a game level that must check in a few seconds, so the
// period band is a compressed one. It is a property of this level, not a measurement.
// Biology: Calabrese lab models of the leech heartbeat timing network (Nadim et al. 1995; Olsen et al. 1995; Hill et al. 2001, 2002); see docs/research/bestiary-a.md.
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_leech_heart();
namespace
{
using namespace bestiary;

constexpr double kW0 = 3000, kBin = 25;

class LeechHeart : public Scripted
{
public:
  using Scripted::Scripted;

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
        if ( pr.sign >= 0 || pr.n_edges <= 0 || pr.pre_index < 0 || pr.post_index < 0 ) continue;
        const auto& post = g_.st.populations[ static_cast< size_t >( pr.post_index ) ];
        bool feeds = false;
        for ( int n : o.neurons ) feeds = feeds || ( n >= post.first_index && n < post.first_index + post.n );
        if ( !feeds ) continue;
        const auto& pre = g_.st.populations[ static_cast< size_t >( pr.pre_index ) ];
        for ( int k = 0; k < pre.n; ++k ) e.set_neuron_param( pre.first_index + k, "V_m", left ? -75.0 : -50.0 );
      }
    }
  }
};

struct Stats
{
  double switches = 0, overlap = 1, balance = 0, period = 0, burst_l = 0, burst_r = 0;
};

Stats stats( const Meas& m, double t_end )
{
  Stats st;
  const auto& l = m.cnt.at( "left" );
  const auto& r = m.cnt.at( "right" );
  const int nb = static_cast< int >( ( t_end - kW0 ) / kBin );
  int last = 0, active = 0, both = 0;
  double tl = 0, tr = 0;
  for ( int b = 0; b < nb; ++b )
  {
    double x = 0, y = 0;
    for ( int k = 0; k < static_cast< int >( kBin ); ++k )
    {
      const size_t i = static_cast< size_t >( kW0 + b * kBin + k );
      if ( i < l.size() ) x += l[ i ];
      if ( i < r.size() ) y += r[ i ];
    }
    tl += x;
    tr += y;
    if ( x > 0 || y > 0 ) ++active;
    if ( x > 0 && y > 0 ) ++both;
    const int d = x > y ? 1 : ( y > x ? -1 : 0 );
    if ( d == 0 ) continue;
    if ( last != 0 && d != last ) st.switches += 1;
    last = d;
  }
  st.overlap = active > 0 ? static_cast< double >( both ) / active : 1.0;
  st.balance = tl + tr > 0 ? std::min( tl, tr ) / ( tl + tr ) : 0.0;
  st.period = st.switches >= 2 ? ( t_end - kW0 ) / ( st.switches / 2.0 ) : 0.0;
  auto mean_burst = [ & ]( const std::vector< float >& v ) {
    const auto rs = runs_of( v, kW0, t_end, 100 );
    double s = 0;
    for ( int x : rs ) s += x;
    return rs.empty() ? 0.0 : s / static_cast< double >( rs.size() );
  };
  st.burst_l = mean_burst( l );
  st.burst_r = mean_burst( r );
  return st;
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
            "full cycle must take a few seconds. Two plain interneurons that inhibit each other do not alternate: the one that fires first silences the other for ever. Each "
            "interneuron needs a slow process that wears out its own burst (adaptation) so that the silenced partner can come back. The real heartbeat cycle is much slower than "
            "this level's: the game compresses time so that a check takes seconds.";
  i.chapter = "leech";
  i.sensors = { "drive" };
  i.motors = { "left", "right" };
  i.duration_ms = 14000;
  i.concepts_new = {};
  i.concepts_used = { "half_centre", "bursting", "rebound", "adaptation", "inhibition" };
  i.par_neurons = 4;
  i.default_model = "aeif_cond_alpha";
  i.kinds = { { "drive", micro::dc( 2.0, 300.0 ) }, { "left", micro::rate() }, { "right", micro::rate() } };
  i.bands = { { "period_s", "s", 0.9, 3.5 }, { "overlap_pct", "%", 0, 15 } };
  i.recipe = "Four adaptive exponential cells. HN_L and HN_R: a = 0, b = 5, tau_w = 1500, V_reset = -58; HE_L and HE_R: a = 0, b = 0 (fast cells). drive > all four at 480 pA per unit (the "
             "task sets level 2.0). HN_L > HN_R and HN_R > HN_L at -50 nS (both interneurons inhibitory); HN_L > HE_L and HN_R > HE_R at -300 nS; HE_L > 'left', HE_R > 'right'. "
             "Each side bursts for about a second and gives way to the other, a cycle of about two seconds. (The task starts the left motor neurons slightly depolarised, and the "
             "interneurons that inhibit them the other way round, so that the left side is released first.)";
  i.note = { "The heartbeat of the leech",
    "The leech heart is driven by a timing network in a few ganglia of the nerve cord. In each segmental oscillator two heart interneurons, HN cells, inhibit each other "
    "reciprocally; each fires a burst while the other is silent. The switch comes from slow processes in the cells: one burst tires (release), or the silenced partner "
    "recovers from inhibition and escapes (rebound, with a hyperpolarisation-activated current). Two oscillators in neighbouring segments are coupled by coordinating "
    "interneurons and lock to the faster of them. This level is the single oscillator, with spiking cells and a compressed time scale; the real inhibition is partly graded "
    "(transmission that depends smoothly on the presynaptic voltage), which the game's spike-based synapses do not model.",
    "Hill AA, Lu J, Masino MA, Olsen OH, Calabrese RL (2001) J Comput Neurosci 10:281-302, doi:10.1023/a:1011216131638; Nadim F, Olsen OH, De Schutter E, Calabrese RL (1995) J Comput Neurosci 2:215-235, doi:10.1007/BF00961435" };
  s.schedule = [ & ]( bool quick ) {
    Schedule c;
    c.dur = quick ? 9000 : 14000;
    c.segs = { { "drive", 0, c.dur, 2.0 } };
    c.wins = { { "window", kW0, c.dur } };
    return c;
  };
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
      if ( pr.sign < 0 && pr.n_edges > 0 )
      {
        any = true;
        const std::string& m = g.pop( pr.pre_index ).model;
        if ( !is_one_of( m, { "iaf_psc_alpha", "iaf_psc_exp", "iaf_psc_delta", "iaf_cond_alpha", "iaf_cond_exp", "parrot_neuron" } ) ) slow = true;
      }
    if ( any && !slow )
      return "The cells that do the inhibiting are plain integrate-and-fire cells: they have no slow process to wear out a burst, so whichever side fires first silences the other for ever. "
             "Use a cell type with a slow adaptation variable for them.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const Stats st = stats( m, m.dur );
    p[ "switches" ] = st.switches;
    p[ "period_ms" ] = st.period;
    p[ "overlap_pct" ] = 100.0 * st.overlap;
    p[ "balance" ] = st.balance;
    p[ "burst_left" ] = st.burst_l;
    p[ "burst_right" ] = st.burst_r;
    const double tl = m.n( "left", "window" ), tr = m.n( "right", "window" );
    const double sw_need = m.tol > 1.2 ? 3 : ( m.tol < 0.9 ? 5 : 4 );
    c.push_back( { "fires", tl >= 1 && tr >= 1, ( tl >= 1 ? 0.5 : 0.0 ) + ( tr >= 1 ? 0.5 : 0.0 ) * 0.5,
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
  };
  s.ok_verdict = "A heartbeat oscillator: the two sides burst in turn with almost no overlap, at a steady cycle.";
  s.h1 = "A half-centre needs two things: mutual inhibition, so that the side that fires silences the other, and a slow process that wears the active side out so that the other can take over.";
  s.h2 = "Each side is a cell that inhibits the other, with adaptation to end its burst; the steady drive goes to both. The cycle time is set by how slowly the adaptation builds up and decays.";
  s.h2_target = "left";
  s.h3 = "drive to both sides; HNL and HNR adaptive cells that inhibit each other; HNL read as 'left' and HNR as 'right'. If one side wins for good, the adaptation is too weak; if both fire together, the "
         "inhibition is too weak; if the cycle is too slow, shorten the adaptation time constant, and if it is too fast, lengthen it.";
  s.metric_name = "period_ms";
  s.metric_unit = "ms";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return stats( m, m.dur ).period; };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_leech_heart() { return std::make_unique< LeechHeart >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( leech_heart, 512, "leech_heart.json", make_leech_heart ) }
