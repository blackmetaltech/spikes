// Micro-puzzle "l_burst" (leech chapter, concept "bursting": a slow recovery variable inside the cell chops a steady drive into bursts). Leech heart interneurons do not
// fire evenly: they fire bursts of spikes with silent gaps between them. A cell that is driven steadily must answer with bursts.
//
// WORLD (4000 ms; dc sensor 'drive', level 2.0 x sensor weight, on from 300 ms)
// SCORING ('burst' pool, per cell)
//   pre  [0, 300)       silent (<= 1 spike)
//   a burst is a run of spikes with gaps of at most 25 ms, and counts when it has >= 3 spikes; analysis window [700, 4000)
//   bursts     >= 5 (guided 3)                           the drive is steady, so the bursts must come from the cell
//   in_bursts  >= 80 % of the spikes in the window are in a burst (guided 65 %)   a regular train (one spike at a time) is not a burst pattern
//   gaps       the longest silent stretch is >= 3 x the longest gap inside a burst (guided 2 x)   bursts are separated by silence
// MECHANISM: the cells behind 'burst' have a slow recovery variable (an Izhikevich cell, an adaptive exponential cell, a Hodgkin-Huxley cell): a plain integrate-and-fire cell
// resets only by firing, so it cannot end a burst by itself.
// Biology: heartbeat interneurons of the medicinal leech fire bursts (Nadim et al. 1995 J Comput Neurosci 2:215; Hill et al. 2001 J Comput Neurosci 10:281).
// The level uses a phenomenological burster (Izhikevich 2003), not a model of the leech cell: its bursts are short; the leech cell bursts for seconds.
#include "bestiary_a.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_l_burst();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "l_burst";
  i.title = "Leech: a cell that bursts";
  i.brief = "The heart interneurons of the leech do not fire evenly. They fire in bursts of spikes with silent gaps between the bursts, even when their input is steady. "
            "Drive one cell with the steady sensor 'drive' and read it on 'burst': it must be silent before the drive, and then fire in clear bursts, nearly all of its spikes "
            "inside bursts, with real silent gaps between them. An ordinary integrate-and-fire cell fires one spike at a time at a steady rate, because its only reset is the "
            "spike itself. A cell that can burst has a slow variable of its own that builds up during a burst and ends it. Choose such a cell.";
  i.chapter = "leech";
  i.sensors = { "drive" };
  i.motors = { "burst" };
  i.duration_ms = 4000;
  i.concepts_new = { "bursting" };
  i.concepts_used = { "adaptation" };
  i.par_neurons = 1;
  i.default_model = "izhikevich_CH";
  i.kinds = { { "drive", micro::dc( 2.0, 5.0 ) }, { "burst", micro::rate() } };
  i.bands = { { "burst_hz", "Hz", 0, 400 } };
  i.recipe = "One Izhikevich cell with the chattering preset (a 0.02, b 0.2, c -50, d 2); drive > the cell at the default weight of 5 pA per unit (10 pA at the level the task uses); "
             "the cell > 'burst'. It fires bursts of three to five spikes with silent gaps.";
  i.note = { "Neurons that fire in bursts",
    "The two oscillator interneurons of each segmental heartbeat oscillator in the medicinal leech fire alternating bursts: while one bursts the other is silent. "
    "Bursting is a property of the cell as well as of the circuit: a slow current builds up during the burst and ends it, and the model cells of the leech heartbeat "
    "need such currents to reproduce the rhythm. This level uses a generic burster; the real cells burst much more slowly.",
    "Nadim F, Olsen OH, De Schutter E, Calabrese RL (1995) J Comput Neurosci 2:215-235, doi:10.1007/BF00961435; Hill AA, Lu J, Masino MA, Olsen OH, Calabrese RL (2001) J Comput Neurosci 10:281-302, doi:10.1023/a:1011216131638" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 4000;
    c.segs = { { "drive", 300, 4000, 2.0 } };
    c.wins = { { "pre", 0, 300 }, { "run", 700, 4000 } };
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto d = g.in_pops( "drive" ), b = g.out_pops( "burst" );
    if ( b.empty() ) return "";
    if ( !d.empty() && !g.path( d, b, +1, 3, true ) ) return "The drive does not reach the cell behind 'burst': connect it to the cell.";
    for ( int p : b )
    {
      const std::string& m = g.pop( p ).model;
      if ( is_one_of( m, { "iaf_psc_alpha", "iaf_psc_exp", "iaf_psc_delta", "iaf_cond_alpha", "iaf_cond_exp", "parrot_neuron" } ) )
        return "The cell behind 'burst' is a plain integrate-and-fire cell: its only reset is the spike itself, so it can only fire one spike at a time. A bursting cell needs a slow variable "
               "that builds up during the burst and ends it: choose a cell type that has one (the chattering Izhikevich preset, or an adapting exponential cell).";
    }
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const auto& v = m.cnt.at( "burst" );
    const double pre = m.n( "burst", "pre" );
    const auto runs = runs_of( v, 700, 4000, 25 );
    int nb = 0, in_b = 0, tot = 0;
    for ( int r : runs )
    {
      tot += r;
      if ( r >= 3 ) { ++nb; in_b += r; }
    }
    // longest silence in the window, and the longest gap inside a burst
    double longest_gap = 0, last = -1, longest_in = 0;
    for ( long k = 700; k < 4000 && k < static_cast< long >( v.size() ); ++k )
      if ( v[ static_cast< size_t >( k ) ] > 0 )
      {
        if ( last >= 0 )
        {
          const double g = static_cast< double >( k ) - last;
          if ( g > 25 ) longest_gap = std::max( longest_gap, g );
          else longest_in = std::max( longest_in, g );
        }
        last = static_cast< double >( k );
      }
    const double frac = tot > 0 ? static_cast< double >( in_b ) / tot : 0.0;
    p[ "bursts" ] = nb;
    p[ "in_bursts_pct" ] = 100.0 * frac;
    p[ "longest_gap_ms" ] = longest_gap;
    c.push_back( { "pre", pre <= 1, 1.0 - ramp( pre, 1, 20 ), "The cell fires before any drive: it must be silent until the drive comes." } );
    int longest_run = 0;
    for ( int r : runs ) longest_run = std::max( longest_run, r );
    // the sentence follows what was measured: no firing, spike-by-spike firing, pairs only, or a few runs that never repeat
    const std::string bursts_fail = tot < 3 ? "The cell hardly fires: the drive does not make it fire. Strengthen the drive."
      : nb == 0 && longest_run < 2 ? "The cell fires spike by spike, evenly: its recovery after a spike is too fast to group spikes. A burst needs a slow variable that outlasts a single spike."
      : nb == 0 ? "The cell groups its spikes only in pairs: a burst holds several spikes before the slow variable ends it."
      : "The cell bursts too few times: a slow variable has to end each burst and release the next, so that bursts repeat through the drive.";
    c.push_back( { "bursts", nb >= std::ceil( 5 / m.tol ), ramp( nb, 0, 5 ), bursts_fail } );
    c.push_back( { "in_bursts", frac >= 0.8 / std::sqrt( m.tol ), ramp( frac, 0.3, 0.8 ), "Many spikes fall outside bursts: the cell fires as a mixture of bursts and single spikes. A bursting cell groups nearly all of its spikes." } );
    c.push_back( { "gaps", longest_gap >= 40, ramp( longest_gap, 0, 40 ), "There are no clear silent gaps between the bursts: a slow variable must end each burst and keep the cell silent for a while." } );
  };
  s.ok_verdict = "A bursting cell: a steady drive is turned into bursts of spikes with silent gaps.";
  s.h1 = "Bursting comes from a slow variable inside the cell: it builds up during a burst, ends it, and decays during the gap.";
  s.h2 = "Pick a cell type with a slow recovery variable for the cell behind 'burst'.";
  s.h2_target = "burst";
  s.h3 = "One cell of a bursting type (the chattering preset of the Izhikevich cell), driven by 'drive' and read on 'burst'. If it does not fire, strengthen the drive; if it fires "
         "evenly, the cell type has no slow variable.";
  s.metric_name = "bursts";
  s.metric_unit = "bursts";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) {
    int nb = 0;
    for ( int r : runs_of( m.cnt.at( "burst" ), 700, 4000, 25 ) ) nb += r >= 3 ? 1 : 0;
    return static_cast< double >( nb );
  };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_l_burst() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( l_burst, 510, "l_burst.json", make_l_burst ) }
