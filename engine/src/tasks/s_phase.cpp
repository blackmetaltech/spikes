// Micro-puzzle "s_phase" (crab chapter, concept "phase_lag": inhibition and rebound set when a follower fires). A follower of the stomatogastric rhythm, the lateral pyloric cell LP, is inhibited by the pacemaker's burst
// and fires by rebound when the inhibition stops: that is what puts its burst after the pacemaker's in the cycle.
//
// WORLD (6500 ms; Poisson sensor 'pd' (the pacemaker's inhibitory input, 100 Hz) on for 350 ms at the start of each 1000 ms cycle: [500, 850), [1500, 1850), ... ; the follower is read on 'lp')
//   per cycle k = 1..5 (inhibition from t_k = 500 + 1000 k - 1000... the cycles starting at 500, 1500, 2500, 3500, 4500; the first is a warm-up and is not scored):
//   quiet     [t + 30, t + 350)   the follower is silent while it is inhibited (at most 1 spike per cycle on average; guided 2)
//   rebound   [t + 350, t + 1000) it fires a burst of at least 3 spikes (guided 2) after the inhibition ends in at least 3 of the 4 scored cycles
//   phase     the first spike of that burst falls at a phase of 0.4 to 0.8 of the cycle (guided 0.35 to 0.85)
// MECHANISM (Engine::structure()): the 'pd' sensor reaches the follower by inhibition, and the follower is a conductance-based cell (a plain integrate-and-fire cell has nothing that makes it fire after inhibition).
// Biology: Marder & Bucher 2001 Curr Biol 11:R986; Prinz, Bucher & Marder 2004 Nat Neurosci 7:1345. The inhibition is spike-triggered (the real synapse is graded); the time scale is the real one.
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_s_phase();
namespace
{
using namespace bestiary;

const double kT[] = { 500, 1500, 2500, 3500, 4500 };

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "s_phase";
  i.title = "Crab: phase from rebound";
  i.brief = "In the crab's pyloric rhythm the lateral pyloric cell LP does not fire at the same time as the pacemaker: the pacemaker's burst inhibits it, and LP fires when the inhibition lets go, by rebound. The wait is what "
            "gives LP its place in the cycle. The sensor 'pd' stands for the pacemaker's inhibition: a burst of inhibitory input of a third of a second at the start of each second. Build the follower as a single "
            "conductance-based cell (cell type stg_cell, with the H current and calcium currents that make a rebound, and without the calcium-activated potassium current that would make it a pacemaker itself), connect 'pd' to it as an "
            "inhibitory input (a negative weight), and read 'lp'. The cell must stay silent while it is inhibited and fire a burst after the inhibition ends, with its first spike somewhere between the middle of the cycle and the "
            "last quarter of it. An integrate-and-fire cell just resumes its steady firing the moment the inhibition stops, or never fires at all.";
  i.chapter = "crab";
  i.sensors = { "pd" };
  i.motors = { "lp" };
  i.duration_ms = 6500;
  i.concepts_new = { "phase_lag" };
  i.concepts_used = { "rebound", "inhibition", "bursting" };
  i.par_neurons = 1;
  i.default_model = "stg_cell";
  i.kinds = { { "pd", in_kind( "poisson", 100, 20 ) }, { "lp", micro::rate() } };
  i.bands = { { "lp_first_spike_phase", "", 0.4, 0.8 } };
  i.recipe = "One stg_cell cell (LP): g_Na 62800, g_CaT 0, g_CaS 1256, g_A 6280, g_KCa 0, g_Kd 31400, g_H 18.84, g_L 6.28 (nS; a 1 mS/cm2 conductance of a 0.628e-3 cm2 cell is 628 nS). 'pd' > LP at -20 nS (inhibitory). "
             "'lp' reads LP. The cell is silent while inhibited and fires about five spikes starting a little less than half a second after the inhibition ends.";
  i.note = { "Phase from inhibition and rebound",
    "In the pyloric network the follower neurons LP and PY are inhibited by the pacemaker kernel and fire when the inhibition wears off. The delay comes from currents that are activated or de-inactivated by "
    "hyperpolarisation (the H current and the low-threshold T-type calcium current): the cell fires a rebound burst, and the phase of that burst in the cycle depends on how strong the inhibition is and how fast those currents recover. "
    "Phase relationships of this kind are what the pattern is made of.",
    "Marder E, Bucher D (2001) Curr Biol 11:R986-R996, doi:10.1016/s0960-9822(01)00581-4; Prinz AA, Bucher D, Marder E (2004) Nat Neurosci 7:1345-1352, doi:10.1038/nn1352" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 6500;
    for ( double t : kT ) c.segs.push_back( { "pd", t, t + 350, 100 } );
    c.wins = { { "all", 1500, 6500 } };
    return c;
  };
  s.gauges = { { "lp_first_spike_phase", { "first_spike_phase", 1.0 } } };
  s.mech = []( const Graph& g ) -> std::string {
    const auto f = g.out_pops( "lp" ), p = g.in_pops( "pd" );
    if ( f.empty() || p.empty() ) return "";
    for ( int x : f ) if ( plain_cell( g.pop( x ).model ) ) return "The follower is a plain integrate-and-fire cell: when the inhibition stops it simply resumes, and nothing makes it fire after a delay. Use a conductance-based cell (stg_cell) with the H and calcium currents.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    auto it = m.cnt.find( "lp" );
    int rebounds = 0;
    double quiet = 0, onset_sum = 0;
    int onset_n = 0;
    if ( it != m.cnt.end() )
      for ( int k = 1; k < 5; ++k )
      {
        const double t = kT[ k ];
        quiet += m.sum( "lp", t + 30, t + 350 );
        const auto b = burst_list( it->second, t + 350, t + 1000, 150 );
        if ( !b.empty() && b[ 0 ].n >= 3 )
        {
          ++rebounds;
          onset_sum += ( b[ 0 ].t0 - t ) / 1000.0;
          ++onset_n;
        }
      }
    const double pool = std::max( 1.0, m.pool.count( "lp" ) ? m.pool.at( "lp" ) : 1.0 );
    quiet /= 4.0 * pool;
    const double phase = onset_n ? onset_sum / onset_n : 0.0;
    p[ "silent_spikes_per_cycle" ] = quiet;
    p[ "rebounds" ] = rebounds;
    p[ "first_spike_phase" ] = phase;
    const bool wide = m.tol > 1.2;
    c.push_back( { "silent", quiet <= ( wide ? 2.0 : 1.0 ), 1.0 - ramp( quiet, wide ? 2.0 : 1.0, 12 ),
      "The cell keeps firing while it is inhibited: the inhibition does not silence it. Connect 'pd' with a negative weight (an inhibitory input) and make it strong enough, or lower the cell's own drive." } );
    c.push_back( { "rebounds", rebounds >= 3, ramp( rebounds, 0, 3 ),
      "The cell does not fire a burst after the inhibition ends: nothing makes it rebound. The H current and a low-threshold calcium current are activated by hyperpolarisation and fire the cell when it is released." } );
    const double lo = wide ? 0.35 : 0.4, hi = wide ? 0.85 : 0.8;
    c.push_back( { "phase", rebounds >= 3 && phase >= lo && phase <= hi, rebounds < 3 ? 0.0 : 1.0 - ramp( std::fabs( phase - 0.6 ), 0.2, 0.5 ),
      phase < lo ? "The cell fires too soon after the pacemaker's burst begins: its first spike should come after the burst has ended and a delay has passed. A stronger H current or a longer inhibition delays it."
                 : "The cell fires too late in the cycle: its rebound is too slow. A stronger H current or calcium current brings it earlier." } );
  };
  s.ok_verdict = "A follower with phase: silent while the pacemaker inhibits it, then a rebound burst that starts after the burst has ended, at a steady place in the cycle.";
  s.h1 = "Inhibition removes the follower from the pacemaker's phase; currents that wake up when it is released bring it back after a delay: the delay is the phase.";
  s.h2 = "One conductance-based cell with the H current and a calcium current, no calcium-activated potassium; the sensor connected with a negative weight.";
  s.h2_target = "lp";
  s.h3 = "A stg_cell with its H conductance up, its T-type calcium conductance at zero or modest, the calcium-activated potassium at zero, and a small leak; 'pd' as an inhibitory input. If it fires while inhibited, strengthen the inhibition; "
         "if it never rebounds, strengthen H; if the rebound is too early or too late, change H.";
  s.metric_name = "first_spike_phase";
  s.metric_unit = "";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) {
    auto it = m.cnt.find( "lp" );
    double sum = 0;
    int n = 0;
    if ( it != m.cnt.end() )
      for ( int k = 1; k < 5; ++k )
      {
        const auto b = burst_list( it->second, kT[ k ] + 350, kT[ k ] + 1000, 150 );
        if ( !b.empty() && b[ 0 ].n >= 3 ) { sum += ( b[ 0 ].t0 - kT[ k ] ) / 1000.0; ++n; }
      }
    return n ? sum / n : 0.0;
  };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_s_phase() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( s_phase, 551, "s_phase.json", make_s_phase ) }
