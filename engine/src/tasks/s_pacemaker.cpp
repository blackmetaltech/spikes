// Micro-puzzle "s_pacemaker" (crab chapter, concept "pacemaker_kernel": a bursting cell coupled to its partners makes a rhythm that the whole kernel shares). In the crab stomatogastric ganglion the anterior burster (AB)
// is electrically coupled to the pyloric dilator cells (PD); together they are the pacemaker kernel of the pyloric rhythm and burst in step, about once a second.
//
// WORLD (dc sensor 'modulator', level 1.0 from t = 0 (the neuromodulator bath that the burster needs); 8000 ms; analysis window (3000 ms, 8000 ms])
//   Bursts: runs of spikes separated by at most 120 ms. 'ab' reads the burster, 'pd' the two PD cells.
// SCORING  bursting (at least 4 bursts of 'pd' in the window, each of at least 3 spikes on average), period (0.7 to 1.5 s, guided 0.6 to 1.8, researcher 0.8 to 1.3, regular: coefficient of variation <= 0.16, guided 0.25),
//   locked (every burst of 'pd' has a burst of 'ab' starting within 150 ms of it: the kernel bursts as one; at least 80 % of the cycles), duty (PD bursts fill between 0.15 and 0.5 of the cycle, guided 0.08 to 0.6).
// MECHANISM (Engine::structure()): AB and PD are conductance-based cells (not plain integrate-and-fire cells, which do not burst) and coupled both ways (reciprocal excitation: the engine's electrical coupling needs gap-junction cell
// models, which are tonic; fast reciprocal excitation approximates it).
// Biology: Marder & Bucher 2001 Curr Biol 11:R986; Prinz, Bucher & Marder 2004 Nat Neurosci 7:1345. The stg_cell conductances are game values in the ranges of the published models.
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_s_pacemaker();
namespace
{
using namespace bestiary;

constexpr double kFrom = 3000;

struct Kernel
{
  int bursts = 0;
  double period = 0, cv = 0, duty = 0, spikes_per_burst = 0, locked = 0;
};
Kernel kernel( const Meas& m )
{
  Kernel k;
  auto a = m.cnt.find( "ab" ), d = m.cnt.find( "pd" );
  if ( a == m.cnt.end() || d == m.cnt.end() ) return k;
  const auto A = burst_list( a->second, kFrom, m.dur, 120 ), D = burst_list( d->second, kFrom, m.dur, 120 );
  k.bursts = static_cast< int >( D.size() );
  if ( D.size() < 2 ) return k;
  std::vector< double > per;
  for ( size_t i = 0; i + 1 < D.size(); ++i ) per.push_back( D[ i + 1 ].t0 - D[ i ].t0 );
  double sum = 0, sq = 0, dut = 0, sp = 0;
  for ( double x : per ) sum += x;
  k.period = sum / static_cast< double >( per.size() );
  for ( double x : per ) sq += ( x - k.period ) * ( x - k.period );
  k.cv = k.period > 0 ? std::sqrt( sq / static_cast< double >( per.size() ) ) / k.period : 0.0;
  for ( size_t i = 0; i + 1 < D.size(); ++i ) dut += ( D[ i ].t1 - D[ i ].t0 + 10 ) / per[ i ];
  k.duty = dut / static_cast< double >( per.size() );
  for ( const auto& b : D ) sp += b.n;
  k.spikes_per_burst = sp / static_cast< double >( D.size() ) / std::max( 1.0, m.pool.count( "pd" ) ? m.pool.at( "pd" ) : 1.0 );
  int hit = 0;
  for ( const auto& b : D )
    for ( const auto& x : A )
      if ( std::fabs( x.t0 - b.t0 ) <= 150 ) { ++hit; break; }
  k.locked = static_cast< double >( hit ) / static_cast< double >( D.size() );
  return k;
}

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "s_pacemaker";
  i.title = "Crab: a pacemaker kernel";
  i.brief = "The rhythm of the crab's stomach is paced by a small kernel: one anterior burster (AB) and two pyloric dilator cells (PD) that are electrically coupled to it, so that all three burst together about once a second. "
            "The cells are conductance-based (cell type stg_cell): sodium and delayed-rectifier currents make the spikes, two calcium currents and a calcium-activated potassium current make the slow bursts, and the "
            "hyperpolarisation-activated current H helps to start the next one. The sensor 'modulator' is the neuromodulator bath that the pacemaker needs. The game's engine couples cells electrically only for tonic cell models, so here "
            "the coupling is approximated by strong fast excitation both ways between AB and PD. Build the burster AB and two PD cells, couple them both ways, and read 'ab' and 'pd'. The kernel must burst regularly with a cycle of "
            "about a second, each burst must fill a sensible part of the cycle, and the PD cells must burst together with AB, cycle after cycle. A plain integrate-and-fire cell does not burst; the PD cells without the coupling "
            "would burst on their own schedule, out of step with AB.";
  i.chapter = "crab";
  i.sensors = { "modulator" };
  i.motors = { "ab", "pd" };
  i.duration_ms = 8000;
  i.concepts_new = { "pacemaker_kernel" };
  i.concepts_used = { "bursting", "excitation", "coupling" };
  i.par_neurons = 3;
  i.default_model = "stg_cell";
  i.kinds = { { "modulator", micro::dc( 1.0, 50.0 ) }, { "ab", micro::rate() }, { "pd", micro::rate() } };
  i.bands = { { "period_s", "s", 0.7, 1.5 } };
  i.recipe = "Three stg_cell cells. AB (1 cell: g_Na 251200, g_CaT 1570, g_CaS 3768, g_A 31400, g_KCa 3140, g_Kd 62800, g_H 6.28, g_L 0, bias -50 pA); PD (2 cells: g_Na 62800, g_CaT 1570, g_CaS 3768, g_A 31400, g_KCa 3140, g_Kd 62800, "
             "g_H 6.28, g_L 6.28, bias -50 pA). Conductances in nS (a 1 mS/cm2 conductance of a 0.628e-3 cm2 cell is 628 nS). modulator > AB and modulator > PD at 50 pA per unit (it cancels the bias: without the modulator nothing bursts). AB > PD at +100 nS and PD > AB at +20 nS. 'ab' reads AB, 'pd' reads PD. "
             "The cycle is about a second and PD bursts fill about a third of it.";
  i.note = { "The pacemaker kernel of the pyloric rhythm",
    "The pyloric rhythm of the crab stomatogastric ganglion is paced by the anterior burster neuron AB, which is electrically coupled to the two pyloric dilator neurons PD. The coupled group bursts as one unit, and the modulators "
    "that the ganglion needs for this rhythm act on its intrinsic currents. Bursts end through slow calcium-dependent outward currents and start again as inward currents recover. This level models the kernel with a "
    "conductance-based cell and approximates the electrical coupling by fast excitation.",
    "Marder E, Bucher D (2001) Curr Biol 11:R986-R996, doi:10.1016/s0960-9822(01)00581-4; Prinz AA, Bucher D, Marder E (2004) Nat Neurosci 7:1345-1352, doi:10.1038/nn1352" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 8000;
    c.segs = { { "modulator", 0, 8000, 1.0 } };
    c.wins = { { "window", kFrom, 8000 } };
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto ab = g.out_pops( "ab" ), pd = g.out_pops( "pd" );
    if ( ab.empty() || pd.empty() ) return "";
    if ( Graph::overlap( ab, pd ) ) return "'ab' and 'pd' are read from the same cells: the kernel has two parts, the burster and the dilator cells, so each needs its own group.";
    for ( int p : ab ) if ( plain_cell( g.pop( p ).model ) ) return "The pacemaker cell is a plain integrate-and-fire cell: it fires or it does not, but it cannot make bursts with silent gaps. Use a conductance-based cell (stg_cell).";
    if ( !g.edge( ab, pd, +1 ) || !g.edge( pd, ab, +1 ) )
      return "AB and PD are not coupled both ways: electrical coupling passes current in both directions, and the kernel bursts as one unit. Approximate it with excitation from AB to PD and from PD to AB.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const Kernel k = kernel( m );
    p[ "bursts" ] = k.bursts;
    p[ "period_ms" ] = k.period;
    p[ "period_cv" ] = k.cv;
    p[ "pd_duty" ] = k.duty;
    p[ "spikes_per_burst" ] = k.spikes_per_burst;
    p[ "locked" ] = k.locked;
    const bool wide = m.tol > 1.2, tight = m.tol < 0.9;
    c.push_back( { "bursts", k.bursts >= 4 && k.spikes_per_burst >= 3, ramp( k.bursts, 0, 4 ) * 0.7,
      k.bursts < 4 ? "The kernel does not burst regularly: there are fewer than four bursts in the analysis window. The burster needs the modulator, a slow calcium-activated potassium current to end bursts, and inward currents to start the next."
                   : "The 'bursts' are one or two spikes: the cells fire singly. A burst is a cluster of spikes on a slow wave, ended by a slow outward current." } );
    const double plo = wide ? 600 : ( tight ? 800 : 700 ), phi = wide ? 1800 : ( tight ? 1300 : 1500 );
    c.push_back( { "period", k.period >= plo && k.period <= phi, k.period <= 0 ? 0.0 : 1.0 - clampd( std::fabs( std::log( k.period / clampd( k.period, plo, phi ) ) ), 0.0, 1.0 ),
      k.period > phi ? "The cycle is too slow: speed up the burster (less calcium-activated potassium, or more bias or modulator drive)." : "The cycle is too fast: slow the burster (more calcium-activated potassium, or less modulator drive)." } );
    c.push_back( { "regular", k.cv <= ( wide ? 0.25 : 0.16 ), 1.0 - ramp( k.cv, wide ? 0.25 : 0.16, 0.5 ), "The cycles vary a lot in length: the kernel is irregular. The PD cells and AB should lock each other, so that they burst as one unit." } );
    c.push_back( { "locked", k.locked >= 0.8, k.locked, "The PD cells and AB do not burst together: some bursts of one group have no partner in the other. Couple them both ways, strongly enough that each burst of AB makes the PD cells burst and the other way round." } );
    const double dlo = wide ? 0.08 : 0.15, dhi = wide ? 0.6 : 0.5;
    c.push_back( { "duty", k.duty >= dlo && k.duty <= dhi, k.bursts < 4 ? 0.0 : 0.6, "The bursts fill too little or too much of the cycle: they should fill between a sixth and a half of it. Their length is set by the calcium-activated potassium current that ends them." } );
  };
  s.ok_verdict = "A pacemaker kernel: the burster and the dilator cells burst together, cycle after cycle, about once a second, each burst filling a sensible part of the cycle.";
  s.h1 = "A kernel is a bursting cell plus partners that it drags into its rhythm. Slow currents make a cell burst; coupling makes cells burst together.";
  s.h2 = "AB and the PD cells must be conductance-based cells (not integrate-and-fire), excite each other both ways, and the modulator must reach the burster.";
  s.h2_target = "ab";
  s.h3 = "Conductance-based cells with a calcium-activated potassium current (it ends the bursts), two calcium currents and the H current; the modulator to AB; AB to the PD cells strongly and the PD cells back to AB more weakly. If it is too slow, "
         "reduce the calcium-activated potassium; if the PD cells do not follow AB, strengthen the coupling from AB to them.";
  s.metric_name = "period_ms";
  s.metric_unit = "ms";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return kernel( m ).period; };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_s_pacemaker() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( s_pacemaker, 550, "s_pacemaker.json", make_s_pacemaker ) }
