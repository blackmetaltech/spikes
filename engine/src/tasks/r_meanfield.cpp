// Task "r_meanfield" (rates chapter, concept "mean_field"): a spiking excitatory-inhibitory network and its mean-field prediction, side by side. Replace every spiking cell of a large, sparse, random
// network by its rate and the whole network by a few siegert units: the mean and the variance of the input one population gets are the sums over its inputs, J K r and J^2 K r, and the rate of
// an integrate-and-fire cell driven by such an input is the Siegert formula. The prediction holds when the network is large, sparse and asynchronous. The player builds both, with the same J and K, and
// the level scores the agreement.
//
// WORLD (2100 ms; sensors 'ext' (poisson, onto the spiking cells) and 'ext_mf' (rate_drive, onto the driving siegert unit); both are given the same level in Hz: 6000, 8000 and 10000 for 700 ms each,
//   the last 450 ms of each level is measured). Readouts: 'net' (mean rate per excitatory cell, Hz) and 'mf' (the siegert prediction, Hz).
// SCORING
//   alive   at the highest level both the network and the prediction fire (>= 5 Hz)               the pair is not trivially silent
//   rises   the network's rate is higher at each level than at the one before                       it follows the drive
//   agree   at all three levels |network - prediction| <= 15 % of the larger (+ 1.5 Hz; guided 25 %, researcher 10 %)   the mean field holds
// MECHANISM (Engine::structure()): the network cells that 'net' reads are excited by the external drive and are part of an excitatory-inhibitory loop with sparse random connections (each cell
// gets several inputs from each side); the prediction is siegert units driven through the driving unit with its own excitatory-inhibitory loop. How well the J and K of the two descriptions match
// is not checked in the structure: it is what the agreement measures.
// Biology and theory: Amit DJ, Brunel N (1997) Cereb Cortex 7:237 (self-consistent rates of integrate-and-fire networks); Brunel N (2000) J Comput Neurosci 8:183 (the network and its regimes); Fourcaud N,
// Brunel N (2002) Neural Comput 14:2057 (the rate formula with synaptic filtering); Hahne J et al. (2017) Front Neuroinform 11:34 (the siegert neuron and its use, same network).
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_r_meanfield();
namespace
{
using namespace bestiary;
using namespace tl;

const double kLevel[] = { 6000, 8000, 10000 };

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "r_meanfield";
  i.title = "The network and its mean field";
  i.brief = "A large, sparse network of excitatory and inhibitory integrate-and-fire cells, driven by a lot of independent Poisson input, fires asynchronously, and its mean rate can be predicted without "
            "simulating a single spike. Every cell sees the sum of many small inputs: its mean grows with J K r and its variance with J squared K r (J the size of one input in mV, K how many it has, r the "
            "rate). The rate of an integrate-and-fire cell driven by such an input is the Siegert formula, and a rate unit that applies it (the siegert unit) can stand for a whole population. Build the "
            "network (excitatory and inhibitory cells, random sparse connections, the Poisson drive 'ext') and its prediction (a siegert unit for each population and one for the drive, with projections "
            "that carry the same J and K, driven by 'ext_mf' at the same rate). 'net' reads the mean rate of the excitatory cells, 'mf' the prediction. At three drive levels the two must agree. "
            "Where they do not, something differs between the two descriptions: J, K, the sign of the inhibition or the drive.";
  i.chapter = "rates";
  i.view = "rates";
  i.sensors = { "ext", "ext_mf" };
  i.motors = { "net", "mf" };
  i.duration_ms = 2100;
  i.concepts_new = { "mean_field" };
  i.concepts_used = { "wilson_cowan", "inhibition" };
  i.par_neurons = 255;
  i.default_model = "iaf_psc_delta";
  i.kinds = { { "ext", in_kind( "poisson", 8000, 0.2 ) }, { "ext_mf", in_kind( "rate_drive", 8000, 1.0 ) }, { "net", micro::rate() }, { "mf", Task::ChannelKind{ "rate_value", 0, "", 0.05, 1000.0, 0.0 } } };
  i.bands = { { "agreement", "%", 0, 100 } };
  s.gauges = { { "agreement", { "agreement_pct" } } };
  s.vals = { "mf" };
  i.recipe = "Spiking: E (200 iaf_psc_delta cells) and I (50), both with tau_m 20, C_m 1, t_ref 2, E_L 0, V_reset 0, V_th 20. Connections with a fixed in-degree: E > E and E > I with 60 inputs per cell at +0.3 mV, "
             "I > E and I > I with 20 inputs per cell at -1.5 mV (delay 1.5). ext > E and ext > I at 0.2 mV. Mean field: siegert units SE and SI (tau_m 20, t_ref 2, theta 20, V_reset 0, tau 5) and a driving unit SD "
             "(its mean is set by ext_mf). Projections with the same J and K: SD > SE and SD > SI J 0.2 K 1; SE > SE and SE > SI J 0.3 K 60; SI > SE and SI > SI J -1.5 K 20. net reads E, mf reads SE. "
             "The rates are about 19, 31 and 43 Hz. Without the inhibition in the prediction it would be about 100 Hz or more.";
  i.note = { "When the mean field holds",
    "A population of integrate-and-fire neurons with many weak inputs can be replaced by one equation: the firing rate is a function of the mean and the variance of the input, and the input of one "
    "population is the sum of the rates of the others. Solving the self-consistent equations predicts the rates of the whole network. NEST's siegert neuron does the iteration as a network of rate units. "
    "The prediction is exact for large, sparse, asynchronous networks and fails for small or strongly coupled ones.",
    "Amit DJ, Brunel N (1997) Cereb Cortex 7:237-252, doi:10.1093/cercor/7.3.237; Hahne J et al. (2017) Front Neuroinform 11:34-34, doi:10.3389/fninf.2017.00034" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 2100;
    for ( int k = 0; k < 3; ++k )
    {
      c.segs.push_back( { "ext", 700.0 * k, 700.0 * ( k + 1 ), kLevel[ k ] } );
      c.segs.push_back( { "ext_mf", 700.0 * k, 700.0 * ( k + 1 ), kLevel[ k ] } );
      c.wins.push_back( { "l" + std::to_string( k + 1 ), 700.0 * k + 250, 700.0 * ( k + 1 ) } );
    }
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto N = g.out_pops( "net" ), M = g.out_pops( "mf" ), X = g.in_pops( "ext" ), D = g.in_pops( "ext_mf" );
    if ( N.empty() || M.empty() ) return "";
    for ( int p : N )
      if ( is_rate_model( g.pop( p ).model ) ) return "'net' has to read spiking cells (the network that is being predicted), not rate units.";
    if ( !all_rate( g, M ) || g.pop( M.front() ).model != "siegert_neuron" )
      return "'mf' has to read siegert units: they apply the Siegert formula to the mean and the variance of their input, which is what turns the network into a prediction.";
    if ( !Graph::overlap( X, N ) ) return "The Poisson drive 'ext' does not reach the cells that 'net' reads: connect it, and to the inhibitory cells too.";
    // the spiking loop: some population E' that net reads excites an inhibitory population that inhibits it back, with several inputs per cell on each side
    auto loop = [ & ]( const std::vector< int >& E, bool spiking, std::vector< int >* others ) {
      for ( const auto& pe : g.st.projections )
        if ( pe.n_edges > 0 && pe.sign > 0 && Graph::overlap( E, { pe.pre_index } ) && !Graph::overlap( E, { pe.post_index } ) )
          for ( const auto& pi : g.st.projections )
            if ( pi.n_edges > 0 && pi.sign < 0 && pi.pre_index == pe.post_index && Graph::overlap( E, { pi.post_index } ) && is_rate_model( g.pop( pe.post_index ).model ) != spiking )
            {
              if ( spiking && ( pe.indegree_mean < 5 || pi.indegree_mean < 2 ) ) continue;
              if ( others ) others->push_back( pe.post_index );
              return true;
            }
      return false;
    };
    if ( !loop( N, true, nullptr ) )
      return "The network has no excitatory-inhibitory loop: the cells that 'net' reads must excite a population of inhibitory cells (several inputs per cell, random and sparse) that inhibits them back.";
    if ( !g.path( D, M, +1, 3, false ) ) return "The driving siegert unit does not reach the unit 'mf' reads: ext_mf drives a siegert unit that projects onto the prediction units.";
    if ( !loop( M, false, nullptr ) )
      return "The prediction has no excitatory-inhibitory loop: the siegert unit that 'mf' reads must project onto an inhibitory siegert unit that projects back with a negative J, as the network does.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    double sim[ 3 ], mf[ 3 ];
    bool agree = true;
    double worst = 0;
    for ( int k = 0; k < 3; ++k )
    {
      const std::string w = "l" + std::to_string( k + 1 );
      sim[ k ] = m.hz( "net", w );
      const Win* wn = m.win( w );
      mf[ k ] = wn ? m.vmean( "mf", wn->t0, wn->t1 ) : 0.0;
      const double tolr = m.tol > 1.2 ? 0.25 : ( m.tol < 0.9 ? 0.10 : 0.15 );
      const double err = std::fabs( sim[ k ] - mf[ k ] ), allowed = tolr * std::max( sim[ k ], mf[ k ] ) + 1.5;
      agree = agree && err <= allowed;
      worst = std::max( worst, err / allowed );
      p[ "net_hz_l" + std::to_string( k + 1 ) ] = sim[ k ];
      p[ "mf_hz_l" + std::to_string( k + 1 ) ] = mf[ k ];
    }
    p[ "agreement_pct" ] = std::max( 0.0, 100.0 * ( 1.0 - 0.5 * worst ) );
    c.push_back( { "alive", sim[ 2 ] >= 5 && mf[ 2 ] >= 5, 0.5 * ramp( sim[ 2 ], 0, 5 ) + 0.5 * ramp( mf[ 2 ], 0, 5 ),
      "At the highest drive the network or the prediction is (almost) silent: the drive has to make the cells fire, and the prediction has to get the same drive." } );
    c.push_back( { "rises", sim[ 0 ] < sim[ 1 ] && sim[ 1 ] < sim[ 2 ], 0.4, "The network's rate does not rise with the drive: the cells must follow the external input." } );
    c.push_back( { "agree", agree, 1.0 - ramp( worst, 1.0, 4.0 ),
      "The mean-field prediction and the simulation disagree at some drive. They describe the same network, so something differs: the size of one input (J), the number of inputs per cell (K), the sign "
      "and strength of the inhibition, or the drive each one gets. Compare the two descriptions connection by connection." } );
  };
  s.ok_verdict = "The prediction holds: a few siegert units with the J and K of the spiking network give the mean rate of the network at every drive level.";
  s.h1 = "Many weak, independent inputs make the input to a cell Gaussian: its mean and variance, summed over the inputs, are all the cell's rate depends on. A siegert unit computes that rate.";
  s.h2 = "For each population of the network there is one siegert unit, and each connection of the network has a projection between the units that carries the same J and the same K. The drive is a siegert "
         "unit of its own.";
  s.h2_target = "mf";
  s.h3 = "Check the three things that must be equal: J (size of one input) and K (inputs per cell) on every projection, including the negative ones, and the external drive the two get. Then see "
         "which drive level disagrees first: a disagreement only at high rates points at the network being too small or too strongly coupled for the mean field.";
  s.metric_name = "agreement_pct";
  s.metric_unit = "%";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) {
    double worst = 0;
    for ( int k = 0; k < 3; ++k )
    {
      const std::string w = "l" + std::to_string( k + 1 );
      const Win* wn = m.win( w );
      const double a = m.hz( "net", w ), b = wn ? m.vmean( "mf", wn->t0, wn->t1 ) : 0.0;
      worst = std::max( worst, std::fabs( a - b ) / std::max( 1.0, std::max( a, b ) ) );
    }
    return 100.0 * std::max( 0.0, 1.0 - worst );
  };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_r_meanfield() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( r_meanfield, 612, "r_meanfield.json", make_r_meanfield ) }
