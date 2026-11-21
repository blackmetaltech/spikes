// Micro-puzzle "t_tuned" (tuning chapter, concept "tuning_curve"): a cell tuned to one stimulus by convergence. The afferents (parrot cells that you create and connect to 'stim') each respond best to
// one orientation, and cell k of N prefers 180 k / N degrees (the order the cells have in the circuit). A cell that listens to some of them and not to the others inherits a preference: the
// Hubel-Wiesel picture of an orientation-selective cell built from aligned inputs.
//
// WORLD (2400 ms; tuned_poisson sensor 'stim', period one hundred and eighty degrees, level = the stimulus orientation in degrees, set by the task): eight trials of 300 ms, orientations 0, 45, 90, 135, 0, 45, 90, 135.
//   The response of a trial is the spikes of 'resp' in its last 220 ms, summed over the two trials of the same orientation.
// SCORING
//   fires    at ninety degrees the cell fires at 12 Hz or more (guided 8)                             it responds
//   prefers  its rate at ninety degrees is at least 3 x its rate at zero degrees (and 3 at 0 + 2 spikes)  it is selective for that orientation
//   graded   and at least 1.4 x its rate at 45 and at one hundred and thirty-five degrees                                  not just 'anything near 90'
// MECHANISM (Engine::structure()): the afferents are the cells 'stim' reaches; the cell that 'resp' reads is not one of them and weighs them unequally (excitation from the ones that prefer its orientation, more than from the rest; or inhibition from the flanks).
// Biology: Hubel DH, Wiesel TN (1962) J Physiol 160:106 (orientation-selective cells in the cat's visual cortex; the proposal that they are built from aligned inputs). Here the inputs are Poisson
// cells with von Mises tuning curves, and the cell is an integrate-and-fire one.
#include "theory.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_t_tuned();
namespace
{
using namespace bestiary;
using namespace tl;

const double kAng[] = { 0, 45, 90, 135, 0, 45, 90, 135 };

/// The weights with which the afferent populations `X` reach the cell populations `R` (direct projections): one net weight per afferent cell (a projection's mean weight scaled by the
/// share of the pair's cells it connects; cells that no projection reaches count as zero). `exc` / `inh` count the excitatory and inhibitory projections.
struct AffW
{
  std::vector< double > cell;
  int exc = 0, inh = 0;
  /// A preference exists when the weakest net weight is at most half the strongest (and the strongest is excitatory).
  bool graded() const
  {
    if ( cell.empty() ) return false;
    double lo = cell.front(), hi = cell.front();
    for ( double v : cell ) { lo = std::min( lo, v ); hi = std::max( hi, v ); }
    return hi > 0 && lo <= 0.5 * hi;
  }
};
AffW afferent_weights( const Graph& g, const std::vector< int >& X, const std::vector< int >& R )
{
  AffW a;
  const double nr = std::max( 1, g.cells( R ) );
  for ( int x : X )
  {
    double net = 0;
    for ( const auto& p : g.st.projections )
    {
      if ( p.n_edges <= 0 || p.sign == 0 || p.pre_index != x || std::find( R.begin(), R.end(), p.post_index ) == R.end() ) continue;
      net += p.w_mean * p.n_edges / ( std::max( 1, g.pop( x ).n ) * nr );
      ( p.sign > 0 ? a.exc : a.inh )++;
    }
    for ( int k = 0; k < g.pop( x ).n; ++k ) a.cell.push_back( net );
  }
  return a;
}

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "t_tuned";
  i.title = "A cell tuned by its inputs";
  i.brief = "Many sensory neurons answer best to one value of a stimulus, an orientation, a direction, a pitch: their tuning curve peaks at a preferred value. A cell that listens to the right inputs inherits a "
            "preference. The sensor 'stim' is the orientation of a bar, from 0 to one hundred and eighty degrees, shown for a few hundred milliseconds in turn. Make a population of afferent cells (parrot cells, which repeat the spikes of the stimulus) and connect 'stim' to it: "
            "each afferent fires Poisson-wise with a rate that peaks at its own preferred orientation, and the cells are spread evenly over the one hundred and eighty degrees in the order they were made (cell k of N prefers "
            "180 k / N degrees, so a group that comes first prefers the small angles). Then build one more cell, 'resp', that listens to some of the afferents and not to others, so that it responds to "
            "ninety degrees much more than to 0, and more than to forty-five and one hundred and thirty-five degrees. Which afferents it listens to, and how strongly, is the whole puzzle; a cell that weighs them all alike has no preference. The groups of afferents "
            "can be separate populations, in the order you create them.";
  i.chapter = "tuning";
  i.view = "tuning";
  i.sensors = { "stim" };
  i.motors = { "resp" };
  i.duration_ms = 2400;
  i.concepts_new = { "tuning_curve" };
  i.concepts_used = { "convergence" };
  i.par_neurons = 9;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "stim", in_kind( "tuned_poisson", 0, 1.0 ) }, { "resp", micro::rate() } };
  i.bands = { { "peak_hz", "Hz", 0, 100 } };
  s.gauges = { { "peak_hz", { "peak_hz" } } };
  s.raw = { "stim" };
  s.cells = { "resp" };
  i.recipe = "Eight parrot cells in three groups, made in this order: A1 (4 cells, preferring 0 to 67 degrees), A2 (1 cell, ninety degrees), A3 (3 cells, 112 to 157 degrees). stim > A1, A2, A3 at weight 1 (the "
             "afferents: peak 80 Hz, base 5 Hz). One integrate-and-fire cell R: A2 > R at +1500 pA, nothing from A1 and A3. 'resp' reads R. It fires at about 60 Hz at ninety degrees and at the afferent base rate elsewhere.";
  i.note = { "Tuning from convergence",
    "Hubel and Wiesel found cells in cat visual cortex that fire for a bar at one orientation and little for others, and proposed that such a cell receives input from cells of the lateral geniculate nucleus "
    "whose receptive fields lie along a line. Whether the tuning of real cortical cells comes from that feed-forward convergence, from recurrent cortical connections or both is still debated; here the input is feed-forward.",
    "Hubel DH, Wiesel TN (1962) J Physiol 160:106-154, doi:10.1113/jphysiol.1962.sp006837" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 2400;
    for ( int k = 0; k < 8; ++k )
    {
      c.segs.push_back( { "stim", 300.0 * k, 300.0 * ( k + 1 ), kAng[ k ] } );
      c.wins.push_back( { "t" + std::to_string( k ), 300.0 * k + 80, 300.0 * ( k + 1 ) } );
    }
    return c;
  };
  s.mech_e = []( const Graph& g, Engine& e ) -> std::string {
    const auto R = g.out_pops( "resp" ), X = g.in_pops( "stim" );
    if ( R.empty() ) return "";
    const auto sc = in_cells( e, "stim" ), rc = out_cells( e, "resp" );
    if ( sc.empty() ) return "";
    for ( int n : rc )
      if ( sc.count( n ) ) return "'resp' reads the afferents themselves: it has to be a cell of its own that listens to them.";
    // afferents that are not parrot cells and are driven by 'stim' at a weight of about one never fire: nothing downstream can be tuned
    for ( int x : X )
      if ( g.pop( x ).model != "parrot_neuron" && e.input_weight( "stim" ) < 100.0 )
        return "The afferents hardly fire: a cell that is driven by 'stim' at weight one stays silent. Make the afferents parrot cells, which repeat the Poisson spikes of the stimulus.";
    const auto w = afferent_weights( g, X, R );
    if ( w.exc == 0 ) return "'resp' hears none of the afferents by excitation: connect some afferents to the cell.";
    // preference is in the weights, not in the number of connections: a cell that weighs every afferent about alike has none
    if ( !w.graded() ) return "'resp' listens to every afferent with about the same weight: a cell that hears all orientations alike has no preference. Give the afferents that prefer ninety degrees a stronger say than the rest.";
    return "";
  };
  s.ok_g = []( const Graph& g ) -> std::string {
    const auto w = afferent_weights( g, g.in_pops( "stim" ), g.out_pops( "resp" ) );
    if ( w.inh > 0 )
      return "A tuned cell: it fires for an orientation of ninety degrees and much less for the others, because the afferents that prefer other orientations inhibit it (flank inhibition) while the ones that prefer ninety degrees excite it.";
    return "A tuned cell: it fires for an orientation of ninety degrees and much less for the others, because it weighs the afferents that prefer it far more than the rest.";
  };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    auto rate = [ & ]( int a, int b ) { return ( m.n( "resp", "t" + std::to_string( a ) ) + m.n( "resp", "t" + std::to_string( b ) ) ) / ( 2.0 * 0.22 * std::max( 1.0, m.pool.count( "resp" ) ? m.pool.at( "resp" ) : 1.0 ) ); };
    const double r0 = rate( 0, 4 ), r45 = rate( 1, 5 ), r90 = rate( 2, 6 ), r135 = rate( 3, 7 );
    p[ "peak_hz" ] = r90;
    p[ "rate_0_hz" ] = r0;
    p[ "rate_45_hz" ] = r45;
    p[ "rate_90_hz" ] = r90;
    p[ "rate_135_hz" ] = r135;
    const double need = m.tol > 1.2 ? 8.0 : 12.0;
    c.push_back( { "fires", r90 >= need, ramp( r90, 0, need ), "The cell hardly fires at ninety degrees: it has to listen to afferents that respond to that orientation, with a weight that makes it fire." } );
    const double f = m.tol > 1.2 ? 2.0 : 3.0;
    c.push_back( { "prefers", r90 >= f * r0 + 2.0, ramp( r90, r0, f * r0 + 2.0 ), "The cell responds to zero degrees almost as much as to ninety: it listens to afferents of other orientations too. Leave those out." } );
    c.push_back( { "graded", r90 >= 1.4 * std::max( r45, r135 ), ramp( r90, std::max( r45, r135 ), 1.4 * std::max( r45, r135 ) ), "It responds about equally to ninety degrees and to the orientations next to it: listen to fewer afferents, those closest to ninety degrees." } );
  };
  s.ok_verdict = "A tuned cell: it fires for an orientation of ninety degrees and much less for the others, because it listens only to the afferents that prefer it.";
  s.h1 = "A cell inherits the preference of the inputs it listens to: connect it to afferents that prefer one orientation and leave out the rest.";
  s.h2 = "Afferent cells prefer angles in the order they were made, evenly spread over one hundred and eighty degrees. Which of them prefer ninety degrees? Those are the ones to connect.";
  s.h2_target = "resp";
  s.h3 = "Make the afferents in separate groups in the order of their preferred angle: those below ninety degrees, those at about ninety, and those above. Connect only the middle group to the cell, strongly enough to make it fire. "
         "If the cell responds to the neighbouring angles too, make the middle group smaller; if it does not fire, strengthen the connection.";
  s.metric_name = "peak_hz";
  s.metric_unit = "Hz";
  s.metric_lower = false;
  s.metric = []( const Meas& m ) { return ( m.n( "resp", "t2" ) + m.n( "resp", "t6" ) ) / ( 2.0 * 0.22 * std::max( 1.0, m.pool.count( "resp" ) ? m.pool.at( "resp" ) : 1.0 ) ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_t_tuned() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( t_tuned, 630, "t_tuned.json", make_t_tuned ) }
