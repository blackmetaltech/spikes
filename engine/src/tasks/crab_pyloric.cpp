// Task "crab_pyloric" (crab chapter): the pyloric rhythm of the crab stomatogastric ganglion. A pacemaker kernel (the anterior burster AB with the pyloric dilator cells PD) bursts about once a second; the lateral pyloric (LP)
// and pyloric (PY) cells are silenced by it and fire by rebound when it lets go, LP first and PY later (LP also inhibits PY), so that the three motor outputs fire in the order PD, LP, PY, round and round.
// The cells are the NESTML conductance-based model stg_cell (Na, Kd, CaT, CaS, A, KCa, H and a leak, kinetics after the Prinz et al. 2003 family); the synapses are spike-triggered inhibitory conductances.
//
// WORLD (dc sensor 'modulator', level 1.0 on from t = 0 (a neuromodulator bath: the pacemaker needs it); episode 11000 ms, quick 8000 ms; analysis window (3000 ms, end])
//   Bursts: runs of spikes of a read-out pool separated by gaps of at most 120 ms. A cycle is the interval between two successive burst onsets of 'pd'; its phases are the fractions of the cycle at which the 'lp' and 'py' bursts start.
// SCORING  rhythm (at least 3 complete cycles in the window with one LP and one PY burst each), period (0.7 to 1.5 s; guided 0.6 to 1.8; researcher 0.8 to 1.3) and its regularity (coefficient of variation <= 0.16, guided 0.25),
//   order (LP starts after PD, PY after LP by at least 0.1 of a cycle, and before the next PD), duty cycles (PD 0.15 to 0.5, LP and PY 0.08 to 0.45; guided wider).
// MECHANISM (Engine::structure()): the three read-outs come from three different groups; 'pd' reaches 'lp' and 'py' by net inhibition, 'lp' inhibits 'py', and the cells of the three groups are conductance-based cells (not plain
// integrate-and-fire cells): followers have to rebound, the pacemaker has to burst.
// Biology: Marder & Bucher 2001 Curr Biol 11:R986 (central pattern generators); Prinz, Bucher & Marder 2004 Nat Neurosci 7:1345 (the three-cell pyloric model: virtually indistinguishable activity from widely disparate parameters);
// Prinz, Billimoria & Marder 2003 J Neurophysiol 90:3998 (single-neuron model databases). The reference parameters are game values in the ranges of those models, NOT the published sets; the AB/PD electrical coupling is
// approximated by fast reciprocal excitation, and the graded glutamatergic synapses by spike-triggered ones.
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_crab_pyloric();
namespace
{
using namespace bestiary;

constexpr double kFrom = 3000;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "crab_pyloric";
  i.title = "Crab: the pyloric rhythm";
  i.brief = "In the stomatogastric ganglion of the crab a small network of about a dozen cells drives the filtering of food in the stomach, one burst after another, about once a second. The pacemaker kernel (the anterior burster AB "
            "with the two pyloric dilator cells PD) bursts by itself; the lateral pyloric cell LP and the pyloric cells PY are silenced by the pacemaker and fire when it lets go, by rebound. LP comes first, "
            "and LP inhibits PY, so PY comes last: the three outputs fire in the order PD, LP, PY. The cells are conductance-based (cell type stg_cell: sodium, delayed rectifier, two calcium currents, A-type and calcium-activated "
            "potassium, and the hyperpolarisation-activated current H); the wiring is the published three-cell scheme, the parameters are not. Build a pacemaker kernel of AB and PD, an LP cell and PY cells, and read 'pd', 'lp' and 'py'. "
            "The sensor 'modulator' is the neuromodulator bath that the pacemaker needs. AB and PD inhibit LP and PY; LP inhibits PD and PY; PY inhibits LP. You are scored on the rhythm: cycles of about a second, regular, "
            "with the bursts in the order PD, LP, PY, and with sensible burst lengths. Many different sets of conductances give the same pattern (that is the point of degeneracy); the game gives you one. A new stg_cell has only the spike currents and is silent: the conductances you add make it burst or rebound. Conductances are in nS, a bias in pA, and a connection's weight is a synaptic "
            "conductance in nS (negative inhibits); the modulator's weight is a current in pA per unit of modulator.";
  i.chapter = "crab";
  i.sensors = { "modulator" };
  i.motors = { "pd", "lp", "py" };
  i.duration_ms = 11000;
  i.concepts_new = {};
  i.concepts_used = { "pacemaker_kernel", "rebound", "inhibition", "bursting" };
  i.par_neurons = 6;
  i.default_model = "stg_cell";
  i.kinds = { { "modulator", micro::dc( 1.0, 50.0 ) }, { "pd", micro::rate() }, { "lp", micro::rate() }, { "py", micro::rate() } };
  i.bands = { { "period_s", "s", 0.7, 1.5 }, { "pd_duty", "", 0.15, 0.5 } };
  i.recipe = "Six stg_cell cells. AB (1 cell: g_Na 251200, g_CaT 1570, g_CaS 3768, g_A 31400, g_KCa 3140, g_Kd 62800, g_H 6.28, g_L 0, bias -50 pA); PD (2 cells: g_Na 62800, the others as AB but g_L 6.28, bias -50 pA); LP (1 cell: g_Na 62800, g_CaT 0, "
             "g_CaS 1256, g_A 6280, g_KCa 0, g_Kd 31400, g_H 18.84, g_L 6.28); PY (2 cells: g_Na 62800, g_CaT 2512, g_CaS 628, g_A 50240, g_KCa 0, g_Kd 78500, g_H 62.8, g_L 6.28, bias 50 pA). Conductances in nS (a 1 mS/cm2 conductance "
             "of a 0.628e-3 cm2 cell is 628 nS). modulator > AB and modulator > PD at 50 pA per unit (it cancels their bias). AB > PD +100 nS and PD > AB +20 nS (the electrical coupling, approximated); AB > LP and AB > PY at -40 nS; PD > LP and PD > PY at -20 nS; "
             "LP > PD at -10 nS; LP > PY at -10 nS; PY > LP at -30 nS. 'pd' reads PD, 'lp' LP, 'py' PY. The period is about a second and the duty cycles 0.4, 0.15 and 0.2.";
  i.note = { "The pyloric rhythm and degeneracy",
    "The pyloric rhythm of the crab stomatogastric ganglion is a triphasic motor pattern: the pacemaker kernel (AB and PD) bursts, then LP, then PY, about once a second. Computer models of the three-cell circuit show that virtually "
    "indistinguishable activity can arise from widely disparate sets of synaptic strengths and membrane conductances, so animals may differ a great deal in the underlying parameters and still make the same rhythm. "
    "In this level the cells are a conductance-based model with the usual stomatogastric currents; the parameter sets are game values.",
    "Prinz AA, Bucher D, Marder E (2004) Nat Neurosci 7:1345-1352, doi:10.1038/nn1352; Marder E, Bucher D (2001) Curr Biol 11:R986-R996, doi:10.1016/s0960-9822(01)00581-4" };
  s.schedule = [ & ]( bool quick ) {
    Schedule c;
    c.dur = quick ? 8000 : 11000;
    c.segs = { { "modulator", 0, c.dur, 1.0 } };
    c.wins = { { "window", kFrom, c.dur } };
    return c;
  };
  s.gauges = { { "period_s", { "period_ms", 0.001 } }, { "pd_duty", { "pd_duty", 1.0 } } };
  s.mech = []( const Graph& g ) -> std::string {
    const auto pd = g.out_pops( "pd" ), lp = g.out_pops( "lp" ), py = g.out_pops( "py" );
    if ( pd.empty() || lp.empty() || py.empty() ) return "";
    if ( Graph::overlap( pd, lp ) || Graph::overlap( pd, py ) || Graph::overlap( lp, py ) ) return "'pd', 'lp' and 'py' are read from the same cells: the pattern has three parts, so each needs its own group of cells.";
    if ( !g.path( pd, lp, -1, 2 ) || !g.path( pd, py, -1, 2 ) )
      return "The pacemaker does not inhibit the followers: LP and PY are held silent by the pacemaker's bursts and fire when they end. Connect the pacemaker cells to LP and to PY with inhibitory synapses.";
    if ( !g.edge( lp, py, -1 ) ) return "LP does not inhibit PY: PY comes after LP because LP's burst holds it back. Add that inhibition.";
    for ( int p : lp ) if ( plain_cell( g.pop( p ).model ) ) return "LP is a plain integrate-and-fire cell: it has no way to rebound after inhibition. Use a conductance-based cell (stg_cell) whose H and calcium currents produce the rebound.";
    for ( int p : py ) if ( plain_cell( g.pop( p ).model ) ) return "PY is a plain integrate-and-fire cell: it has no way to rebound after inhibition. Use a conductance-based cell (stg_cell).";
    for ( int p : pd ) if ( plain_cell( g.pop( p ).model ) && !g.edge( pd, pd, +1 ) )
      return "The pacemaker cells are plain integrate-and-fire cells: nothing in them makes a burst end and a silence follow. Use conductance-based cells (stg_cell) for the pacemaker kernel.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const Pyloric r = pyloric_stats( m, "pd", "lp", "py", kFrom, m.dur );
    p[ "cycles" ] = r.cycles;
    p[ "cycles_complete" ] = r.ok;
    p[ "period_ms" ] = r.period;
    p[ "period_cv" ] = r.period_cv;
    p[ "pd_duty" ] = r.duty_a;
    p[ "lp_onset" ] = r.on_b;
    p[ "lp_duty" ] = r.duty_b;
    p[ "py_onset" ] = r.on_c;
    p[ "py_duty" ] = r.duty_c;
    const double tpd = m.n( "pd", "window" ), tlp = m.n( "lp", "window" ), tpy = m.n( "py", "window" );
    const bool wide = m.tol > 1.2, tight = m.tol < 0.9;
    c.push_back( { "fires", tpd >= 1 && tlp >= 1 && tpy >= 1, ( tpd >= 1 ? 1.0 : 0.0 ) / 3 + ( tlp >= 1 ? 1.0 : 0.0 ) / 3 + ( tpy >= 1 ? 1.0 : 0.0 ) / 3,
      std::string( "A group is silent: " ) + ( tpd < 1 ? "the pacemaker does not burst (it needs the modulator and the right conductances)" : tlp < 1 ? "LP never fires (it must rebound when the pacemaker lets go)" : "PY never fires (it must rebound when LP and the pacemaker let go)" ) + "." } );
    c.push_back( { "rhythm", r.ok >= 3, ramp( r.ok, 0, 3 ),
      r.cycles < 3 ? "The pacemaker does not burst regularly: fewer than three cycles in the analysis window. The kernel needs a slow calcium-activated potassium current to end each burst and an inward current to start the next."
                   : "The cycles are not clean: in many of them LP or PY fires twice or not at all. Each follower should fire exactly one burst per pacemaker cycle." } );
    const double plo = wide ? 600 : ( tight ? 800 : 700 ), phi = wide ? 1800 : ( tight ? 1300 : 1500 );
    c.push_back( { "period", r.period >= plo && r.period <= phi, r.period <= 0 ? 0.0 : 1.0 - clampd( std::fabs( std::log( r.period / clampd( r.period, plo, phi ) ) ), 0.0, 1.0 ),
      r.period > phi ? "The cycle is too slow for a pyloric rhythm. The cycle is set mainly by what starts the pacemaker's next burst: strengthen the H current of the burster AB, or the modulator's drive onto the pacemaker cells "
                       "(more of either speeds the cycle up; change it in small steps, the rhythm is fragile). The calcium-activated potassium current mostly sets how long the bursts last."
                     : "The cycle is too fast for a pyloric rhythm. The cycle is set mainly by what starts the pacemaker's next burst: weaken the H current of the burster AB, or the modulator's drive onto the pacemaker cells "
                       "(less of either slows the cycle down; change it in small steps, the rhythm is fragile). The calcium-activated potassium current mostly sets how long the bursts last." } );
    c.push_back( { "regular", r.period_cv <= ( wide ? 0.25 : 0.16 ), 1.0 - ramp( r.period_cv, wide ? 0.25 : 0.16, 0.5 ),
      "The cycles vary a lot in length: the rhythm is irregular. The pacemaker kernel should be locked together (AB and PD coupled both ways) and the followers should not disturb it." } );
    const bool order = r.ok >= 1 && r.on_b > 0.12 && r.on_c >= r.on_b + 0.1 && r.on_c < 0.95;
    const std::string order_msg = r.on_b <= 0.12 ? "LP starts together with the pacemaker instead of after it: the pacemaker's inhibition of LP is too weak, or LP does not need to rebound."
      : r.on_c >= 0.95 ? "PY starts at the very end of the cycle, just as the pacemaker bursts again, and is cut short: its rebound comes too late. Give the PY cells more H or T-type calcium conductance, or more bias current, so that they rebound earlier."
                       : "PY does not come after LP: the order must be PD, then LP, then PY. LP must inhibit PY, and PY needs to fire only once both the pacemaker and LP have let go.";
    c.push_back( { "order", order, r.ok < 1 ? 0.0 : ( r.on_b > 0.12 ? 0.5 : 0.0 ) + ( r.on_c >= r.on_b + 0.1 && r.on_c < 0.95 ? 0.5 : 0.0 ), order_msg } );
    const double dlo = wide ? 0.08 : 0.15, dhi = wide ? 0.6 : 0.5, flo = wide ? 0.05 : 0.08, fhi = wide ? 0.55 : 0.45;
    const bool duty = r.ok >= 1 && r.duty_a >= dlo && r.duty_a <= dhi && r.duty_b >= flo && r.duty_b <= fhi && r.duty_c >= flo && r.duty_c <= fhi;
    const std::string duty_msg = r.duty_a < dlo ? "The pacemaker's bursts are too short for the cycle: they should fill between a sixth and a half of it. Their length is set by the calcium-activated potassium current that ends them: less of it makes them longer."
      : r.duty_a > dhi ? "The pacemaker's bursts fill too much of the cycle: they should fill between a sixth and a half of it. More calcium-activated potassium in the pacemaker cells ends them sooner."
      : r.duty_c < flo ? "The PY bursts are far too short: PY rebounds late and is cut off by the pacemaker. More H or T-type calcium conductance, or more bias current, in the PY cells makes them start earlier and last longer."
      : r.duty_c > fhi ? "The PY bursts fill most of the cycle: PY rebounds too easily. Less H or T-type calcium conductance in the PY cells, or stronger inhibition onto them, shortens the burst."
      : r.duty_b < flo ? "The LP burst is far too short: more H current in LP, or less inhibition onto it, makes it last longer."
                       : "The LP burst fills most of the cycle: less H current in LP, or more inhibition onto it, shortens it.";
    c.push_back( { "duty", duty, r.ok < 1 ? 0.0 : 0.6, duty_msg } );
  };
  s.ok_verdict = "The pyloric rhythm: a regular cycle of about a second in which the pacemaker kernel bursts, then LP rebounds, then PY, with sensible burst lengths.";
  s.h1 = "The pattern comes from a pacemaker that bursts by itself and followers that are silenced by it and rebound when it stops; a second inhibition (LP onto PY) puts PY last.";
  s.h2 = "AB and PD (coupled both ways) are the pacemaker kernel; both inhibit LP and PY. LP inhibits PD and PY, and PY inhibits LP. LP and PY need conductance-based cells with an H current and calcium currents for the rebound.";
  s.h2_target = "lp";
  s.h3 = "A pacemaker with slow calcium-activated potassium (it ends its bursts); followers with the H current and a calcium current (they rebound); the inhibition from the pacemaker to each follower sets when they start. If PY fires with LP, strengthen LP's "
         "inhibition of PY or make PY's rebound slower; if the rhythm stops, check the modulator and the pacemaker's bias; if the cycle is too fast or slow, change the H current of AB or the modulator's drive (more speeds it up).";
  s.metric_name = "period_ms";
  s.metric_unit = "ms";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return pyloric_stats( m, "pd", "lp", "py", kFrom, m.dur ).period; };
  s.kind = "task";
  return s;
}
}  // namespace

std::unique_ptr< Task > make_crab_pyloric() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( crab_pyloric, 552, "crab_pyloric.json", make_crab_pyloric ) }
