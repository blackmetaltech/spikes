// Micro-puzzle "r_choose" (rates chapter, concept "rate_competition"): two rate units that compete. Each excites itself and inhibits the other, so a small difference in their drive is amplified into
// a large difference in their rates: one wins and the other is silenced, and because the winner holds itself up, the choice outlasts the evidence until a much stronger input for the other
// side overturns it.
//
// WORLD (1.9 s; rate_drive sensors 'a_in' and 'b_in', weights = the player's; readouts 'a' and 'b', rate_value). Levels (x weight): from 400 to 900 ms A a little stronger (0.5 against 0.45),
//   from 900 to 1400 ms equal (0.5 and 0.5), from 1400 to 1900 ms B much stronger (0.2 against 0.9). Windows: the last 150 ms of each phase.
// SCORING
//   win    at the end of the first phase 'a' exceeds 'b' by a clear margin (the inputs differ by a tenth of that)   a small difference is amplified
//   hold   at the end of the second phase, with equal inputs, 'a' still exceeds 'b' by that margin                   the choice is held
//   flip   at the end of the third phase 'b' exceeds 'a' by that margin                                              a strong enough input overturns it
// MECHANISM (Engine::structure()): 'a' and 'b' are rate units on different populations, each excites itself, and each inhibits the other (directly or through an interneuron).
// Biology: competition between populations by mutual inhibition and self-excitation is the core of models of decision making and of attention (Wilson HR, Cowan JD (1972) Biophys J 12:1 for the
// population equations; the winner-take-all reading is the textbook one). Rates are in arbitrary units.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_r_choose();
namespace
{
using namespace bestiary;
using namespace tl;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "r_choose";
  i.title = "Two populations, one winner";
  i.brief = "Two populations compete for the same job, say two possible answers to an ambiguous stimulus. Each is driven by its own evidence ('a_in', 'b_in'), each excites itself and each inhibits the "
            "other. A small advantage in the drive should be amplified into a large advantage in the output: one population wins and the other is silenced. And the winner should hold itself up: when "
            "the two inputs become equal again the winner must stay the winner, until a much stronger input for the other side overturns it. Build two rate units; 'a' reads one and 'b' the other. "
            "Without self-excitation nothing is amplified, without mutual inhibition both win together.";
  i.chapter = "rates";
  i.view = "rates";
  i.sensors = { "a_in", "b_in" };
  i.motors = { "a", "b" };
  i.duration_ms = 1900;
  i.concepts_new = { "rate_competition" };
  i.concepts_used = { "command_switch", "wilson_cowan" };
  i.par_neurons = 2;
  i.default_model = "threshold_lin_rate_ipn";
  i.kinds = { { "a_in", in_kind( "rate_drive", 0.5, 1.0 ) }, { "b_in", in_kind( "rate_drive", 0.5, 1.0 ) }, { "a", Task::ChannelKind{ "rate_value", 0, "", 0.05, 1000.0, 0.0 } },
    { "b", Task::ChannelKind{ "rate_value", 0, "", 0.05, 1000.0, 0.0 } } };
  i.bands = { { "margin", "", 0, 1.5 } };
  s.gauges = { { "margin", { "margin" } } };
  s.vals = { "a", "b" };
  i.recipe = "Two threshold-linear rate units A and B (tau 10, ceiling 1, no noise). a_in > A and b_in > B at weight 1. Self-excitation A > A and B > B at +1.25, mutual inhibition A > B and B > A at -1. "
             "The winner sits at about 1.5, the loser at about 0.5.";
  i.note = { "Winner takes all",
    "Two populations that excite themselves and inhibit each other have two stable states, each with one population up and the other down, and the competition turns a small difference in input into "
    "a large difference in output. The winner holds itself up, so the choice outlasts the evidence. The same wiring has been proposed for decisions, attention and working memory.",
    "Wilson HR, Cowan JD (1972) Biophys J 12:1-24, doi:10.1016/S0006-3495(72)86068-5" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 1900;
    c.segs.push_back( { "a_in", 400, 900, 0.5 } );
    c.segs.push_back( { "b_in", 400, 900, 0.45 } );
    c.segs.push_back( { "a_in", 900, 1400, 0.5 } );
    c.segs.push_back( { "b_in", 900, 1400, 0.5 } );
    c.segs.push_back( { "a_in", 1400, 1900, 0.2 } );
    c.segs.push_back( { "b_in", 1400, 1900, 0.9 } );
    c.wins.push_back( { "p1", 750, 900 } );
    c.wins.push_back( { "p2", 1250, 1400 } );
    c.wins.push_back( { "p3", 1750, 1900 } );
    return c;
  };
  s.mech = []( const Graph& g ) -> std::string {
    const auto A = g.out_pops( "a" ), B = g.out_pops( "b" );
    if ( A.empty() || B.empty() ) return "";
    if ( !all_rate( g, A ) || !all_rate( g, B ) ) return need_rate_units( "two whole populations competing" );
    if ( Graph::overlap( A, B ) ) return "'a' and 'b' read the same unit: the two competitors need a unit each.";
    if ( !Graph::overlap( g.in_pops( "a_in" ), A ) || !Graph::overlap( g.in_pops( "b_in" ), B ) ) return "Each input has to drive its own unit: 'a_in' the one that 'a' reads, 'b_in' the one that 'b' reads.";
    if ( !g.edge( A, A, +1 ) || !g.edge( B, B, +1 ) )
      return "A competitor that does not excite itself cannot amplify its advantage: give each unit a positive connection onto itself.";
    if ( !g.crossed( A, B ) || !g.crossed( B, A ) )
      return "The two units do not inhibit each other: each has to push the other down (a negative connection each way), or both end up winning.";
    return "";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double d1 = m.vmean( "a", 750, 900 ) - m.vmean( "b", 750, 900 ), d2 = m.vmean( "a", 1250, 1400 ) - m.vmean( "b", 1250, 1400 ), d3 = m.vmean( "b", 1750, 1900 ) - m.vmean( "a", 1750, 1900 );
    const double need = m.tol > 1.2 ? 0.45 : ( m.tol < 0.9 ? 0.75 : 0.6 );
    p[ "margin" ] = std::max( 0.0, d1 );
    p[ "margin_win" ] = d1;
    p[ "margin_hold" ] = d2;
    p[ "margin_flip" ] = d3;
    c.push_back( { "win", d1 >= need, ramp( d1, 0, need ), "A small advantage in the drive is not amplified: the two rates stay close. Each unit has to excite itself and inhibit the other so that the one ahead pulls away." } );
    c.push_back( { "hold", d2 >= need, ramp( d2, 0, need ), "When the two inputs become equal the winner loses its lead: the choice is not held. The winner must keep itself up (stronger self-excitation) while it holds the other down." } );
    c.push_back( { "flip", d3 >= need, ramp( d3, 0, need ), "A much stronger input for 'b' does not overturn the choice: the winner is stuck. The inhibition from the challenger has to be able to beat the winner's self-excitation." } );
  };
  s.ok_verdict = "Winner takes all: a small advantage is amplified into a large one, the winner holds its lead when the inputs become equal, and a strong enough challenger overturns it.";
  s.h1 = "Self-excitation plus mutual inhibition between two populations turns a small difference in drive into a large difference in rate, and holds it.";
  s.h2 = "Each unit needs a positive connection onto itself and a negative connection onto the other. The loser's rate should be pushed down while the winner's is held up.";
  s.h2_target = "a";
  s.h3 = "Excite each unit with its own input, give each unit moderate positive self-connection and a moderate negative connection onto the other. If both stay equal, strengthen the self-excitation; "
         "if the winner never gives way to a strong challenger, weaken its self-excitation or strengthen the inhibition.";
  s.metric_name = "margin";
  s.metric_unit = "";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return m.vmean( "a", 750, 900 ) - m.vmean( "b", 750, 900 ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_r_choose() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( r_choose, 611, "r_choose.json", make_r_choose ) }
