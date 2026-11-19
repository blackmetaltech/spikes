// Micro-puzzle "m_coupling" (concept 8: coupling and synchrony). Two pacemaker cells that differ in speed must beat in step.
//
// WORLD (5000 ms; no sensors; the task holds ACh = 0 and NE = 0 so the cells beat at their intrinsic rate)
//   Motors "a" and "b": spikes of the cell(s) wired to each. At the first step the task sets the membrane capacitance of every cell wired
//   to "a" to 28.8 pF and of every cell wired to "b" to 35.2 pF (32 pF -10 % and +10 %: node cells differ in size, and so in rate). A
//   sa_node_pacemaker cell then beats every 580 ms (a) or every 635 ms (b) on its own, and two uncoupled cells drift apart by 55 ms a beat.
// SCORING over (1500, 5000] ms (the first beats are allowed to pull in)
//   For every beat of a, the nearest beat of b gives a phase offset d = t_b - t_a. T = mean interval of a.
//   beats_ok : at least 4 beats of a and of b in the window, and |n_a - n_b| <= 1            (1:1 lock)
//   drift    : max d - min d over the window                needs <= 0.12 T (guided 0.2 T, researcher 0.08 T)
//   lag      : |mean d|                                     needs <= 0.3 T
//   value 100 when passed, else 85 * (criteria met / 3).
// Excitatory synapses stand in for the gap junctions of the node (as in heart_pacemaker). The lab default weight 0.2 nS pulls the cells a
// little (drift ~140 ms in the window); 1 nS or more locks them with a lag of about 60 ms; the slider tops at 4 nS.
//
// Biology: the sinoatrial node is a population of cells with different intrinsic rates that beat at one common rate because they are coupled
// (electrotonically, through gap junctions): mutual entrainment (Jalife 1984 J Physiol 356:221; Michaels, Matyas & Jalife 1987 Circ Res 61:704).
#include <algorithm>
#include "micro_a.h"

namespace spike::tasks
{
namespace
{
using micro::MicroBase;
using micro::Graph;

constexpr double kDur = 5000, kW0 = 1500, kCmA = 28.8, kCmB = 35.2;

class MCoupling : public MicroBase
{
public:
  MCoupling() : MicroBase( info() ) { reset( 1 ); }

  static Info info()
  {
    Info i;
    i.id = "m_coupling";
    i.title = "Coupling";
    i.brief = "Cells of the heart's pacemaker, the sinoatrial node, do not all beat at the same speed: some are a little larger and slower, "
              "some smaller and faster. Yet the node beats as one. Here are two pacemaker cells, 'a' (10 % faster) and 'b' (10 % slower), "
              "each beating on its own. Build one cell for each channel and couple them so that they lock: they must beat one for one, "
              "with the delay between them staying constant (changing by no more than 12 % of a beat over the last 3.5 seconds). "
              "Real node cells couple through gap junctions; here excitatory synapses between the two cells play that part. Uncoupled, "
              "the gap between them grows with every beat. Score: one-for-one beating and a steady phase.";
    i.chapter = "heart";
    i.sensors = {};
    i.motors = { "a", "b" };
    i.duration_ms = kDur;
    i.concepts_new = { "coupling" };
    i.concepts_used = { "pacemaking", "excitation" };
    i.par_neurons = 2;
    i.default_model = "sa_node_pacemaker";
    i.recipe = "N1 (SA node cell) > 'a' and N2 (SA node cell) > 'b'. Link N1 > N2 and N2 > N1 (both excitatory) and raise their weight "
               "from 0.2 nS to about 2 nS.";
    i.kinds = { { "a", micro::rate() }, { "b", micro::rate() } };
    i.bands = { { "drift_pct", "% of a beat", 0, 12 }, { "rate_diff_pct", "%", 0, 4 } };
    i.modulators = { "ACh", "NE" };
    i.note = { "A node of many clocks that beats as one",
      "The cells of the rabbit sinoatrial node differ in their own beating rate, yet the whole node beats at a single frequency. Jalife "
      "showed in 1984 that this is mutual entrainment: when two node cells are coupled electrically, the faster one speeds the slower "
      "one up and the slower holds the faster back until they fire at a common rate, and with weak coupling they lock only partly. "
      "Coupling is what turns thousands of imperfect clocks into one heartbeat.",
      "Jalife J (1984) Mutual entrainment and electrical coupling as mechanisms for synchronous firing of rabbit sino-atrial pace-maker "
      "cells. J Physiol 356:221-243" };
    return i;
  }

  void reset( uint64_t seed ) override
  {
    begin( seed );
    ta_.clear();
    tb_.clear();
    started_ = false;
    shared_ = false;
  }

  std::vector< Hint > hints() const override
  {
    return { { 1, "Two oscillators of different speed lock together when each pulls the other: coupling. The stronger the coupling, the "
                  "more the faster one is held back and the slower one speeded up, until they share one rhythm.", "" },
      { 2, "Use two sinoatrial-node cells, one wired to 'a' and one to 'b', and connect them to each other in both directions. Look at the "
           "weight of those two links: the default is only a small nudge.", "a" },
      { 3, "The first SA node cell > 'a' and the second SA node cell > 'b'. Link them in both directions, both excitatory, and raise the weight well above the "
           "default until the verdict says they are locked. Too weak and the beats still slip past each other.", "a" } };
  }

  std::map< std::string, Metric > metrics() const override
  {
    Stats st = stats();
    return { { "drift_ms", { st.n >= 3 ? st.drift : 999.0, "ms", true } } };
  }

  void inspect( Engine& ) override
  {
    const auto a = g_.out_pops( "a" ), b = g_.out_pops( "b" );
    if ( a.empty() || b.empty() ) return;
    for ( int p : a )
    {
      const double ie = g_.param( p, "I_e" );
      if ( !std::isnan( ie ) && std::fabs( ie ) > 1e-9 ) mech_ = "A cell is pushed by a bias current: these must be pacemaker cells that beat by themselves.";
    }
    for ( int p : b )
    {
      const double ie = g_.param( p, "I_e" );
      if ( !std::isnan( ie ) && std::fabs( ie ) > 1e-9 ) mech_ = "A cell is pushed by a bias current: these must be pacemaker cells that beat by themselves.";
    }
    if ( !mech_.empty() || Graph::overlap( a, b ) ) return;
    if ( !g_.edge( a, b ) && !g_.edge( b, a ) )
      mech_ = "The two cells are not coupled: there is no synapse between them, so each beats at its own pace and they drift apart. Coupling is what makes "
              "separate clocks share one rhythm.";
  }

  Score score() const override
  {
    Score s;
    Stats st = stats();
    const double dmax = ( mode_ == "guided" ? 0.2 : ( mode_ == "researcher" ? 0.08 : 0.12 ) );
    s.parts[ "beats_a" ] = st.na;
    s.parts[ "beats_b" ] = st.nb;
    s.parts[ "period_a_ms" ] = st.T;
    s.parts[ "drift_ms" ] = st.n >= 3 ? st.drift : -1.0;
    s.parts[ "drift_pct" ] = st.n >= 3 && st.T > 0 ? 100.0 * st.drift / st.T : 100.0;
    s.parts[ "lag_ms" ] = st.n >= 3 ? st.lag : -1.0;
    const bool a = st.na >= 4 && st.nb >= 4 && std::fabs( st.na - st.nb ) <= 1 && !shared_;
    const bool b = a && st.T > 0 && st.drift <= dmax * st.T;
    const bool c = a && st.T > 0 && st.lag <= 0.3 * st.T;
    finish( s, ( a ? 1 : 0 ) + ( b ? 1 : 0 ) + ( c ? 1 : 0 ), 3 );
    if ( !done() )
      s.passed = false;
    apply_mech( s );
    std::ostringstream o;
    if ( !wiring_.ok() )
      o << "Not wired: " << wiring_.message() << ".";
    else if ( !done() )
      o << "Running.";
    else if ( shared_ )
      o << "'a' and 'b' are read from the same cell. Use two different cells, one on each channel.";
    else if ( !mech_.empty() )
      o << mech_;
    else if ( st.na < 4 || st.nb < 4 )
      o << "The " << ( st.na < 4 ? "'a'" : "'b'" ) << " cell does not beat on its own. Use a sinoatrial-node cell: it is a pacemaker, it needs no input.";
    else if ( std::fabs( st.na - st.nb ) > 1 )
      o << "Not locked: one cell beats more often than the other. Couple the two cells in both directions and make the coupling stronger.";
    else if ( !b && st.drift > 0.35 * st.T )
      o << "Not locked: the beats slip past each other, the delay between a and b sweeps through a whole beat (the faster cell gains on the slower "
           "one). Strengthen the coupling between the cells.";
    else if ( !b )
      o << "Almost locked but still drifting: the delay between a and b keeps changing. Strengthen the coupling between the cells.";
    else if ( !c )
      o << "The two beat together but b lags a too far behind. Make the coupling stronger so the faster cell can pull the slower one in step.";
    else
      o << "Locked: both cells beat together and the delay between them stays steady.";
    s.verdict = o.str();
    return s;
  }

protected:
  void drive( Engine& e, double ) override
  {
    if ( started_ ) return;
    started_ = true;
    e.set_modulator( "ACh", 0.0 );
    e.set_modulator( "NE", 0.0 );
    std::vector< int > na, nb;
    for ( const auto& o : e.outputs() )
    {
      if ( o.name == "a" ) na = std::vector< int >( o.neurons.begin(), o.neurons.end() );
      if ( o.name == "b" ) nb = std::vector< int >( o.neurons.begin(), o.neurons.end() );
    }
    for ( int n : na )
    {
      e.set_neuron_param( n, "C_m", kCmA );
      if ( std::find( nb.begin(), nb.end(), n ) != nb.end() ) shared_ = true;
    }
    for ( int n : nb )
      if ( std::find( na.begin(), na.end(), n ) == na.end() ) e.set_neuron_param( n, "C_m", kCmB );
  }

  void observe( Engine& e, double dt ) override
  {
    if ( motor_.spike_count( e, "a", dt ) > 0.5 ) ta_.push_back( t_ );
    if ( motor_.spike_count( e, "b", dt ) > 0.5 ) tb_.push_back( t_ );
    Stats st = stats();
    readouts_[ "a_beats" ] = static_cast< double >( ta_.size() );
    readouts_[ "b_beats" ] = static_cast< double >( tb_.size() );
    readouts_[ "drift_pct" ] = st.n >= 3 && st.T > 0 ? 100.0 * st.drift / st.T : 0.0;
    readouts_[ "phase_lag_ms" ] = st.n >= 1 ? st.lag : 0.0;
    readouts_[ "rate_diff_pct" ] = std::fabs( st.na - st.nb ) * 100.0 / std::max( 1.0, std::max( st.na, st.nb ) );
  }

private:
  struct Stats
  {
    double na = 0, nb = 0, T = 0, drift = 0, lag = 0;
    int n = 0;
  };

  Stats stats() const
  {
    Stats st;
    std::vector< double > a, b;
    for ( double t : ta_ ) if ( t > kW0 ) a.push_back( t );
    for ( double t : tb_ ) if ( t > kW0 ) b.push_back( t );
    st.na = static_cast< double >( a.size() );
    st.nb = static_cast< double >( b.size() );
    if ( a.size() >= 2 ) st.T = ( a.back() - a.front() ) / ( a.size() - 1 );
    if ( a.size() < 2 || b.empty() ) return st;
    double lo = 1e9, hi = -1e9, sum = 0;
    for ( double x : a )
    {
      double best = 1e9;
      for ( double y : b )
        if ( std::fabs( y - x ) < std::fabs( best ) ) best = y - x;
      lo = std::min( lo, best );
      hi = std::max( hi, best );
      sum += best;
      ++st.n;
    }
    st.drift = hi - lo;
    st.lag = std::fabs( sum / st.n );
    return st;
  }

  std::vector< double > ta_, tb_;
  bool started_ = false, shared_ = false;
};

} // namespace

std::unique_ptr< Task > make_m_coupling() { return std::make_unique< MCoupling >(); }

} // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( m_coupling, 199, "m_coupling.json", make_m_coupling ) }
