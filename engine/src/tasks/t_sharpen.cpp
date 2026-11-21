// Micro-puzzle "t_sharpen" (tuning chapter, concept "lateral_inhibition_tuning"): a population of cells, each fed by one afferent, inherits the afferents' broad tuning. Inhibition among the cells,
// from the neighbours that prefer other orientations, narrows it: the cell that prefers the stimulus wins and silences its neighbours.
//
// WORLD (14400 ms; tuned_poisson sensor 'stim', period one hundred and eighty degrees, broad tuning curves (kappa 0.5, base 10 Hz, peak 80 Hz); the afferents are cells you create, eight of them, cell k prefers 22.5 k degrees):
//   twenty-four trials of 300 ms, orientations 0, 45, 90, 135 six times. 'resp' reads the eight cells that listen to the afferents; the one that prefers ninety degrees is the fifth in the circuit.
//   The response is read over all twenty-four trials: the mean rate of the cell that prefers the orientation shown (peak) and of the two cells forty-five degrees away from it (side), in the last 220 ms of each trial.
// SCORING
//   fires   the cells fire at 10 Hz or more at their preferred orientation (guided 7)
//   narrow  the side rate is at most 58 % of the peak rate (guided 62 %, researcher 52 %). Without the loop the ratio is about 67 %; the ratio of two small spike counts is noisy (about 5 points over the 48 trials), so the limit sits well clear of that null and the inhibition-only circuits (45 to 50 %)    the tuning is sharper than the afferents'
// MECHANISM (Engine::structure()): the eight cells are driven by the afferents by excitation and are not the afferents; they inhibit one another (directly, or through a cell they excite).
// Biology: sharpening of orientation tuning by intracortical inhibition (Ben-Yishai R, Bar-Or RL, Sompolinsky H (1995) PNAS 92:3844: a recurrent network model in which the tuning width is set by
// the cortical interactions; Hubel DH, Wiesel TN (1962) J Physiol 160:106 for the feed-forward picture it sharpens). Whether real cortical tuning is sharpened this way is debated.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_t_sharpen();
namespace
{
using namespace bestiary;
using namespace tl;

constexpr int kTrials = 48;   // six rounds of the four orientations: the ratio of two small spike counts is noisy, and more trials keep a circuit that sharpens from passing or failing by the draw of the seed
inline double ang_of( int k ) { return 45.0 * ( k % 4 ); }

/// Spikes of the cell with ring index `k` (0..7, in circuit order) in trial window `key`.
double cell_spikes( const Meas& m, int k, const std::string& key )
{
  auto it = m.spk.find( "resp" );
  auto ex = m.extra.find( "c" + std::to_string( ( ( k % 8 ) + 8 ) % 8 ) );
  const Win* w = m.win( key );
  if ( it == m.spk.end() || ex == m.extra.end() || !w ) return 0;
  double n = 0;
  for ( const auto& [ t, c ] : it->second )
    if ( t >= w->t0 && t < w->t1 && c == static_cast< int >( ex->second ) ) ++n;
  return n;
}
/// Mean rate (Hz) of the cell that prefers the shown orientation, and of the two cells two steps (forty-five degrees) away from it, over the trials of one orientation set.
void profile( const Meas& m, double& peak, double& side )
{
  peak = side = 0;
  for ( int k = 0; k < kTrials; ++k )
  {
    const int pref = static_cast< int >( ang_of( k ) / 22.5 + 0.5 ), w = k;
    const std::string key = "t" + std::to_string( w );
    peak += cell_spikes( m, pref, key );
    side += 0.5 * ( cell_spikes( m, pref + 2, key ) + cell_spikes( m, pref - 2, key ) );
  }
  peak /= kTrials * 0.22;
  side /= kTrials * 0.22;
}

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "t_sharpen";
  i.title = "Sharpening a tuning curve";
  i.brief = "Afferents with broad tuning curves feed a population of cells, one cell to one afferent, and the cells inherit the broad tuning. Inhibition among them can narrow it: the cell that prefers the "
            "stimulus pulls ahead and pushes its neighbours down. 'stim' shows an orientation, 0 to one hundred and eighty degrees, in turn. Make eight afferent cells (parrot cells), connect 'stim' to them, and make eight "
            "cells that each listen to their own afferent (neuron k of the afferents to neuron k of the cells, the 'neuron k to k' wiring). 'resp' reads the eight cells. The cell that prefers the orientation shown "
            "must fire well, and the cells that prefer forty-five degrees away from it at only a small fraction of that. Without inhibition among the cells the tuning is as broad as the afferents'. Inhibition among the cells narrows the tuning: that is what the level asks for. "
            "A cell that also excites itself holds its lead against the others, which is optional; how strong each is decides how much it sharpens.";
  i.chapter = "tuning";
  i.view = "tuning";
  i.sensors = { "stim" };
  i.motors = { "resp" };
  i.duration_ms = 14400;
  i.concepts_new = { "lateral_inhibition_tuning" };
  i.concepts_used = { "tuning_curve", "inhibition" };
  i.par_neurons = 16;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "stim", in_kind( "tuned_poisson", 0, 1.0 ) }, { "resp", micro::rate() } };
  i.bands = { { "ratio", "%", 0, 100 } };
  s.gauges = { { "ratio", { "ratio_pct" } } };
  s.raw = { "stim" };
  s.stimuli = { { "stim", { { "kappa", 0.5 }, { "rate_max", 80.0 }, { "rate_base", 10.0 }, { "period_deg", 180.0 } } } };   // the level owns its stimulus: the tuning curves of the afferents are the level's, whatever the circuit file says
  s.cells = { "resp" };
  s.tick = []( Engine& e, double, Meas& m ) {
    if ( m.extra.count( "c0" ) ) return;
    const auto rc = out_cells( e, "resp" );
    int k = 0;
    for ( int n : rc )
      if ( k < 8 ) m.extra[ "c" + std::to_string( k++ ) ] = n;
    for ( ; k < 8; ++k ) m.extra[ "c" + std::to_string( k ) ] = -1;
  };
  i.recipe = "Afferents A (8 parrot cells) and cells T (8 integrate-and-fire cells). stim > A at weight 1 (broad: peak 80 Hz, base 10 Hz). A > T with the 'neuron k to k' wiring at +1500 pA. T > T inhibition at -2500 pA, every pair (the cells inhibit each other), and T > T self-excitation at +2500 pA (the "
             "'neuron k to k' wiring: each cell excites itself, so the leader holds on while it silences the rest). 'resp' reads T. Without the loop the cells forty-five degrees away fire at about two thirds of the peak rate; with it, at about a third. The inhibition is what the level requires; the self-excitation only makes the sharpening stronger and steadier, and inhibition alone (about 2500 pA) already passes.";
  i.note = { "Sharpening tuning with inhibition",
    "The feed-forward inputs to a cortical cell are only broadly tuned. In recurrent network models inhibition among cells of different preferences, or between them and a competitor, makes the "
    "tuning of the response much sharper than that of the input, so that the width of the tuning is set by the cortical interactions rather than by the input. Experimental evidence for and against such "
    "sharpening has accumulated for decades.",
    "Ben-Yishai R, Bar-Or RL, Sompolinsky H (1995) Proc Natl Acad Sci USA 92:3844-3848, doi:10.1073/pnas.92.9.3844" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 14400;
    for ( int k = 0; k < kTrials; ++k )
    {
      c.segs.push_back( { "stim", 300.0 * k, 300.0 * ( k + 1 ), ang_of( k ) } );
      c.wins.push_back( { "t" + std::to_string( k ), 300.0 * k + 80, 300.0 * ( k + 1 ) } );
    }
    return c;
  };
  s.mech_e = []( const Graph& g, Engine& e ) -> std::string {
    const auto R = g.out_pops( "resp" ), X = g.in_pops( "stim" );
    if ( R.empty() ) return "";
    const auto sc = in_cells( e, "stim" ), rc = out_cells( e, "resp" );
    if ( sc.empty() ) return "";
    if ( rc.size() < 5 ) return "'resp' has to read the whole population of cells (eight), so that the cell that prefers ninety degrees is among them.";
    for ( int n : rc )
      if ( sc.count( n ) ) return "'resp' reads the afferents themselves: it has to read the cells that listen to them.";
    if ( !g.path( X, R, +1, 3, false ) ) return "The cells do not listen to the afferents by excitation: connect the afferents to them.";
    if ( !g.path( R, R, -1, 3, false ) )
      return "The cells do not inhibit one another, so nothing narrows the tuning: the cells' tuning is just the afferents' broad one. Add inhibition among the cells (directly, or through an inhibitory cell they excite).";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    double r90 = 0, side = 0;
    profile( m, r90, side );
    const double ratio = r90 > 0 ? side / r90 : 1.0;
    p[ "peak_hz" ] = r90;
    p[ "side_hz" ] = side;
    p[ "ratio_pct" ] = 100.0 * ratio;
    const double need = m.tol > 1.2 ? 7.0 : 10.0, lim = m.tol > 1.2 ? 0.62 : ( m.tol < 0.9 ? 0.52 : 0.58 );
    c.push_back( { "fires", r90 >= need, ramp( r90, 0, need ), "The cell that prefers ninety degrees hardly fires: the afferent must drive it enough, and inhibition must not silence it too." } );
    c.push_back( { "narrow", r90 > 0 && ratio <= lim, r90 > 0 ? 1.0 - ramp( ratio, lim, 1.0 ) : 0.0,
      "The cell responds nearly as much to the neighbouring orientations as to its own: its tuning is as broad as the afferents'. Inhibition from the other cells must be strong enough to push down the response to the orientations it does not prefer." } );
  };
  s.ok_verdict = "Sharper than the input: the cell that prefers ninety degrees keeps its response there and loses most of its response to the neighbouring orientations, because the cells inhibit one another.";
  s.h1 = "Inhibition among cells with different preferences narrows the tuning of each: the cell with the strongest input wins and pushes the others down.";
  s.h2 = "The inhibition has to act among the cells, not on the afferents. A cell at forty-five degrees gets less input than its neighbour at ninety; inhibition makes the difference bigger.";
  s.h2_target = "resp";
  s.h3 = "Connect the afferents to the cells one to one, and add an inhibitory connection among the cells. If the tuning is still broad, strengthen the inhibition; if the preferred cell stops firing, weaken it or strengthen the afferent drive.";
  s.metric_name = "side_over_peak_pct";
  s.metric_unit = "%";
  s.metric = []( const Meas& m ) {
    double peak = 0, side = 0;
    profile( m, peak, side );
    return peak > 0 ? 100.0 * side / peak : 100.0;
  };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_t_sharpen() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( t_sharpen, 631, "t_sharpen.json", make_t_sharpen ) }
