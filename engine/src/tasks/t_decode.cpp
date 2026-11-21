// Task "t_decode" (tuning chapter): decoding a stimulus from a population. Twelve afferents with broad tuning curves around a circle of directions respond to a stimulus; a population of cells listens to
// them; the stimulus is read out as a population vector, the sum of the preferred directions of the cells weighted by how much each fires (Georgopoulos et al. 1986). The decoded direction must match the
// stimulus over random stimuli, and the readout must be of YOUR cells, in the order of their preferred directions.
//
// WORLD (2400 ms; tuned_poisson sensor 'stim', period 36zero degrees, peak 80 Hz, base 5 Hz, kappa 2; the afferents are cells you create, twelve of them, cell k prefers 30 k degrees): six trials of 400 ms, each
//   with a random direction (a different draw for every seed). Output 'heading' is a population_vector over the cells you list: cell k of N at 360 k / N degrees. The decoded direction of a trial is the
//   circular mean of the spikes of the last 300 ms.
// SCORING
//   spikes   the readout fires (at least 10 spikes per trial on average)
//   error    the mean absolute difference between decoded and shown direction over the six trials is at most 25 degrees (guided 35, researcher 18)
//   sharp    no trial is off by more than 6zero degrees (guided 80)
// MECHANISM (Engine::structure()): 'heading' reads cells that are not the afferents, that listen to them by excitation, and the readout lists at least eight cells.
// Biology: Georgopoulos AP, Schwartz AB, Kettner RE (1986) Science 233:1416 (neurons in the motor cortex are broadly tuned to the direction of arm movement; the population vector of their activity
// predicts the direction). Here the tuning curves are von Mises, the noise is Poisson and the readout is the engine's population_vector output.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_t_decode();
namespace
{
using namespace bestiary;
using namespace tl;

constexpr int kTrials = 6;

double circ_diff( double a, double b )
{
  double d = std::fmod( std::fabs( a - b ), 360.0 );
  return d > 180.0 ? 360.0 - d : d;
}

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "t_decode";
  i.title = "Reading a direction from a crowd";
  i.brief = "No single cell tells the direction of a stimulus: each is broadly tuned and noisy. A crowd of them does. Weight every cell's preferred direction by how much it fires and add them up: the population "
            "vector points where the stimulus is (Georgopoulos, in the motor cortex). The sensor 'stim' shows a direction, 0 to 36zero degrees, a different one in each trial. Make twelve afferent cells (parrot cells) and "
            "connect 'stim' to them: cell k of twelve prefers 30 k degrees. Then make a population of cells that listen to the afferents, and let 'heading' read it as a ring: 'heading' takes the cells in "
            "order and puts cell k of N at 360 k / N degrees, so the cells must be in the order of their preferred directions. The decoded direction must be near the shown one, trial after trial. "
            "If the cells are listed in the wrong order, or each listens to the wrong afferent, the arrow points somewhere else.";
  i.chapter = "tuning";
  i.view = "tuning";
  i.sensors = { "stim" };
  i.motors = { "heading" };
  i.duration_ms = 2400;
  i.concepts_new = { "population_vector" };
  i.concepts_used = { "tuning_curve" };
  i.par_neurons = 24;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "stim", in_kind( "tuned_poisson", 0, 1.0 ) }, { "heading", Task::ChannelKind{ "population_vector", 0, "", 0.05, 1000.0, 0.0 } } };
  i.bands = { { "error_deg", "deg", 0, 180 } };
  s.gauges = { { "error_deg", { "error_deg" } } };
  s.raw = { "stim" };
  s.stimuli = { { "stim", { { "kappa", 2.0 }, { "rate_max", 80.0 }, { "rate_base", 5.0 }, { "period_deg", 360.0 } } } };   // the level owns its stimulus (see t_sharpen)
  s.vec = { "heading" };
  i.recipe = "Afferents A (12 parrot cells) and cells T (12 integrate-and-fire cells). stim > A at weight 1 (broad: peak 80 Hz, base 5 Hz, kappa 2, period 360). A > T with the 'neuron k to k' wiring at +1500 pA. "
             "'heading' reads T as a ring (all twelve cells, in order). The decoded direction is within a few degrees on average.";
  i.note = { "The population vector",
    "Georgopoulos and colleagues recorded neurons in the monkey motor cortex and found each tuned, broadly, to the direction of the arm's movement. A single cell predicts the direction badly, but the "
    "population vector, the sum of the cells' preferred directions weighted by their firing, points in the direction of movement. The population vector is one linear decoder, not the best one: for cells with Poisson variability and bell-shaped tuning a maximum-likelihood readout is more accurate, and a winner-take-all readout is another choice. Population decoding has been used since for many sensory and motor variables.",
    "Georgopoulos AP, Schwartz AB, Kettner RE (1986) Science 233:1416-1419, doi:10.1126/science.3749885" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 2400;
    for ( int k = 0; k < kTrials; ++k )
    {
      c.segs.push_back( { "stim", 400.0 * k, 400.0 * ( k + 1 ), 30.0 * ( ( k * 5 ) % 12 ) } );   // (replaced by reseed with random directions)
      c.wins.push_back( { "t" + std::to_string( k ), 400.0 * k + 100, 400.0 * ( k + 1 ) } );
    }
    return c;
  };
  s.reseed = []( Schedule& c, uint64_t seed, std::map< std::string, double >& extra ) {
    Rng r( seed * 7919 + 13 );
    for ( int k = 0; k < kTrials; ++k )
    {
      const double a = std::floor( r.uniform( 0.0, 360.0 ) );
      c.segs[ static_cast< size_t >( k ) ].v = a;
      extra[ "a" + std::to_string( k ) ] = a;
    }
  };
  s.mech_e = []( const Graph& g, Engine& e ) -> std::string {
    const auto H = g.out_pops( "heading" ), X = g.in_pops( "stim" );
    if ( H.empty() ) return "";
    const auto sc = in_cells( e, "stim" ), hc = out_cells( e, "heading" );
    if ( sc.empty() ) return "";
    if ( hc.size() < 8 ) return "'heading' reads too few cells to point anywhere: a ring needs many cells around the circle.";
    for ( int n : hc )
      if ( sc.count( n ) ) return "'heading' reads the afferents themselves. Read the cells that listen to them: decoding has to work on your own population.";
    if ( !g.path( X, H, +1, 3, false ) ) return "The cells that 'heading' reads do not listen to the afferents by excitation: connect the afferents to them.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    double sum = 0, worst = 0, spikes = 0;
    for ( int k = 0; k < kTrials; ++k )
    {
      const Win* w = m.win( "t" + std::to_string( k ) );
      const auto a = m.extra.find( "a" + std::to_string( k ) );
      if ( !w || a == m.extra.end() ) continue;
      const double n = m.sum( "heading", w->t0, w->t1 );
      spikes += n;
      const double err = n > 0 ? circ_diff( m.vec_deg( "heading", w->t0, w->t1 ), a->second ) : 180.0;
      sum += err;
      worst = std::max( worst, err );
    }
    const double mean = sum / kTrials;
    p[ "error_deg" ] = mean;
    p[ "worst_deg" ] = worst;
    p[ "spikes_per_trial" ] = spikes / kTrials;
    const double lim = m.tol > 1.2 ? 35.0 : ( m.tol < 0.9 ? 18.0 : 25.0 );
    c.push_back( { "spikes", spikes / kTrials >= 10, ramp( spikes / kTrials, 0, 10 ), "The readout hardly fires: the cells must listen to the afferents with a weight that makes them fire." } );
    c.push_back( { "error", mean <= lim, 1.0 - ramp( mean, lim, 120.0 ),
      "The decoded direction is often far from the shown one. The readout puts each cell at its own share of the circle, in the order listed, so the cells must be listed in the order of their preferred directions, and each cell must "
      "listen to the afferent that prefers the direction it is listed at." } );
    c.push_back( { "sharp", worst <= ( m.tol > 1.2 ? 80.0 : 60.0 ), 0.5, "Some directions are decoded badly while others are right: look at which ones, and at the wrap-around between the last cell and the first." } );
  };
  s.ok_verdict = "A population vector read out of your own cells points at the shown direction, trial after trial, although every single cell is broadly tuned and noisy.";
  s.h1 = "One cell is broadly tuned and noisy; the sum of all the cells' preferred directions, weighted by their firing, points at the stimulus.";
  s.h2 = "The readout is a ring: the order of the cells you list is the order of their preferred directions around the circle. A cell listed at ninety degrees must listen to the afferent that prefers ninety degrees.";
  s.h2_target = "heading";
  s.h3 = "Make twelve afferents and twelve cells and connect afferent k to cell k with the 'neuron k to k' wiring, then connect the whole population of cells to 'heading'. If the arrow is rotated by a constant "
         "amount, the wiring is shifted; if it points to the opposite side, the order is reversed; if it is only noisy, strengthen the afferent connections.";
  s.metric_name = "error_deg";
  s.metric_unit = "deg";
  s.metric = []( const Meas& m ) {
    double sum = 0;
    for ( int k = 0; k < kTrials; ++k )
    {
      const Win* w = m.win( "t" + std::to_string( k ) );
      const auto a = m.extra.find( "a" + std::to_string( k ) );
      if ( w && a != m.extra.end() ) sum += m.sum( "heading", w->t0, w->t1 ) > 0 ? circ_diff( m.vec_deg( "heading", w->t0, w->t1 ), a->second ) : 180.0;
    }
    return sum / kTrials;
  };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_t_decode() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( t_decode, 632, "t_decode.json", make_t_decode ) }
