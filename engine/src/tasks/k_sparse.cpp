// Micro-puzzle "k_sparse" (fly mushroom body chapter, concept "sparse_coding": a sparse code through random wiring and feedback inhibition). In the fly mushroom body an odour is stored as the activity of a
// few of about two thousand Kenyon cells (KC); a giant inhibitory neuron (APL) that the KCs excite and that inhibits all of them keeps the code sparse over a wide range of odour strengths.
//
// WORLD (2200 ms; Poisson sensors pn0..pn19, one per glomerulus, 1000 pA onto a projection neuron; an odour is the pattern of six glomeruli that fire at the odour's rate; the Kenyon cells are read on 'kc')
//   quiet  [0, 400)        nothing on the sensors: the Kenyon cells are silent (<= 2 spikes)
//   a_lo   [500, 1000)     odour A at a weak level (100 Hz on its glomeruli)                  cells that fire at least once: at least 3, at most a quarter of the group (guided 40 %)
//   b_lo   [1100, 1600)    odour B (another pattern of six, sharing one glomerulus with A) at the same level: its cells overlap those of A by at most half
//   a_hi   [1700, 2200)    odour A three times stronger (300 Hz): still at most a quarter of the group fire (guided 40 %): the code does not follow the concentration
// MECHANISM (Engine::structure()): the glomeruli sensors reach the Kenyon cells (a large group), each Kenyon cell listens to only a few of the projection neurons (indegree at most 8), and the Kenyon cells
// excite a group that inhibits them (a net-inhibitory loop).
// Biology: Aso et al. 2014 eLife 3:e04577 (about 2000 KCs, a sparse odour code); Papadopoulou et al. 2011 Science 332:721 (in the locust a giant nonspiking neuron makes a negative-feedback loop that keeps the KC code sparse
// over a wide range of input; the Drosophila APL is its analogue). Spiking APL and random sparse wiring after the published cell types, not synapse-level data.
#include "bestiary_b.h"

namespace spike::tasks
{
std::unique_ptr< Task > make_k_sparse();
namespace
{
using namespace bestiary;

Spec spec()
{
  Spec s;
  auto& i = s.info;
  i.id = "k_sparse";
  i.title = "Fly: a sparse odour code";
  i.brief = "In the fly's mushroom body an odour is stored as the activity of a handful of Kenyon cells out of two thousand. Two things make the code sparse: each Kenyon cell listens to only a few of the "
            "projection neurons that carry the odour (the wiring is random), so it fires only for the odours that happen to hit most of its inputs; and one giant inhibitory neuron (APL) listens to all the "
            "Kenyon cells and inhibits them all, so that a stronger odour does not simply recruit more of them. The wiring here is a model after the fly's cell types (after the Aso and Papadopoulou papers), not synapse-level data. "
            "The sensors pn0 to pn19 are twenty glomeruli; an odour is the pattern of six of them that are active. Build twenty projection neurons driven by the sensors (one sensor for each), a hundred Kenyon cells, and read "
            "'kc' from the Kenyon cells. Each Kenyon cell should listen to just four randomly chosen projection neurons (use random sparse wiring, a few inputs per cell). Odour A at a weak level must make a few Kenyon cells fire; "
            "odour B must make mostly other cells fire; odour A three times stronger must still make only a few fire, not many more. Without the inhibitory loop a strong odour recruits far too many cells.";
  i.chapter = "fly_mb";
  i.sensors = glomerulus_family();
  i.motors = { "kc" };
  i.duration_ms = 2200;
  i.concepts_new = { "sparse_coding" };
  i.concepts_used = { "excitation", "inhibition", "negative_feedback" };
  i.par_neurons = 20 + 100 + 4;
  i.default_model = "iaf_psc_alpha";
  i.kinds = { { "kc", micro::rate() } };
  for ( const auto& c : i.sensors ) i.kinds[ c ] = glomerulus_kind();
  i.bands = { { "kc_active_cells", "cells", 3, 25 } };
  i.recipe = "One hundred and twenty-four integrate-and-fire cells: PN (20 cells), KC (100 cells), APL (4 cells). pn0 to pn19 > PN, sensor k to cell k, at the default 1000 pA. PN > KC with random sparse wiring, 4 inputs "
             "per Kenyon cell, at +230 pA; KC > APL at +5000 pA; APL > KC at -5000 pA. 'kc' reads KC. Without the APL the strong odour makes more than a third of the Kenyon cells fire.";
  i.note = { "A sparse code in the mushroom body",
    "Each Kenyon cell of the fly receives input from only a handful of projection neurons, chosen almost at random, and fires only when several of them are active together, so each odour excites a small subset of "
    "the roughly two thousand cells. A single giant inhibitory neuron, the APL, is excited by the Kenyon cells and inhibits them: negative feedback that keeps the number of active cells small whatever the odour concentration "
    "(this loop was shown in the locust, where it is a nonspiking neuron). Sparse codes make memories specific: two odours share few cells.",
    "Aso Y et al. (2014) eLife 3:e04577, doi:10.7554/eLife.04577; Papadopoulou M, Cassenaer S, Nowotny T, Laurent G (2011) Science 332:721-725, doi:10.1126/science.1201835" };
  s.cells = { "kc" };
  s.schedule = [ & ]( bool ) {
    Schedule c;
    c.dur = 2200;
    odour_segments( odour_a(), 500, 1000, 100, c.segs );
    odour_segments( odour_b(), 1100, 1600, 100, c.segs );
    odour_segments( odour_a(), 1700, 2200, 300, c.segs );
    c.wins = { { "quiet", 0, 400 }, { "a_lo", 500, 1000 }, { "b_lo", 1100, 1600 }, { "a_hi", 1700, 2200 } };
    return c;
  };
  s.timeline = []() -> std::vector< Task::Event > { return { { 500, 500, "odour A" }, { 1100, 500, "odour B" }, { 1700, 500, "odour A, strong" } }; };
  s.mech = []( const Graph& g ) -> std::string { return sparse_code_missing( g, "kc" ); };
  s.judge = []( const Meas& m, std::vector< Crit >& c, std::map< std::string, double >& p ) {
    const double pool = std::max( 1.0, m.pool.count( "kc" ) ? m.pool.at( "kc" ) : 1.0 );
    const int a_lo = m.active( "kc", 500, 1000 ), b_lo = m.active( "kc", 1100, 1600 ), a_hi = m.active( "kc", 1700, 2200 ), shared = m.both( "kc", 500, 1000, 1100, 1600 );
    const double q = m.n( "kc", "quiet" );
    p[ "active_a_lo" ] = a_lo;
    p[ "active_b_lo" ] = b_lo;
    p[ "active_a_hi" ] = a_hi;
    p[ "shared_a_b" ] = shared;
    p[ "active_fraction_hi" ] = a_hi / pool;
    p[ "quiet_spikes" ] = q;
    const double cap = ( m.tol > 1.2 ? 0.40 : 0.25 ) * pool;
    c.push_back( { "quiet", q <= 2, 1.0 - ramp( q, 2, 30 ), "The Kenyon cells fire with no odour: they should be silent unless several of their inputs are active together. Lower their excitability or weaken the projection neurons' drive." } );
    c.push_back( { "answers", a_lo >= 3, ramp( a_lo, 0, 3 ),
      "Odour A makes (almost) no Kenyon cell fire: each cell needs several of its inputs active at once to fire, so the input to the Kenyon cells is too weak, or the projection neurons do not reach them." } );
    c.push_back( { "sparse", a_lo <= cap && b_lo <= cap, 1.0 - ramp( std::max( a_lo, b_lo ), cap, pool ),
      "Even the weak odour makes many Kenyon cells fire: the code is dense. Each cell should listen to few projection neurons only, and need most of them active to fire." } );
    c.push_back( { "separate", shared <= 0.5 * std::min( a_lo, b_lo ) + 0.5, shared <= 0 ? 1.0 : 1.0 - ramp( shared / std::max( 1.0, double( std::min( a_lo, b_lo ) ) ), 0.5, 1.0 ),
      "Odours A and B excite the same Kenyon cells: a code that overlaps cannot tell them apart. Fewer inputs per Kenyon cell (and random choice of them) make the codes of different odours differ." } );
    c.push_back( { "normalised", a_hi <= cap, 1.0 - ramp( a_hi, cap, pool ),
      "The strong odour recruits many more Kenyon cells: the code gets dense with concentration. Feedback inhibition must grow with the activity of the Kenyon cells: strengthen the cell that they excite and that inhibits them." } );
  };
  s.ok_verdict = "A sparse code: each odour makes a few Kenyon cells fire, different odours use mostly different cells, and a three times stronger odour still recruits only a few, held in check by feedback inhibition.";
  s.h1 = "Sparse codes come from two things: each cell needs several of its few random inputs to fire, and one inhibitory cell that every active cell excites inhibits them all.";
  s.h2 = "Look at the wiring from the projection neurons to the Kenyon cells (a few inputs per cell, not all) and at the loop from the Kenyon cells to an inhibitory cell and back.";
  s.h2_target = "kc";
  s.h3 = "Projection neurons driven one-to-one by the glomeruli sensors; many Kenyon cells, each with random sparse wiring from a few projection neurons; an inhibitory group excited by all Kenyon cells and inhibiting all of them. "
         "If nothing fires, strengthen the wiring onto the Kenyon cells; if too many fire at the weak odour, weaken it or give each cell fewer inputs; if the strong odour recruits too many, strengthen the loop.";
  s.metric_name = "active_cells_strong_odour";
  s.metric_unit = "cells";
  s.metric = []( const Meas& m ) { return static_cast< double >( m.active( "kc", 1700, 2200 ) ); };
  return s;
}
}  // namespace

std::unique_ptr< Task > make_k_sparse() { return std::make_unique< Scripted >( spec() ); }

}  // namespace spike::tasks

namespace spike::tasks { SPIKE_REGISTER_TASK( k_sparse, 540, "k_sparse.json", make_k_sparse ) }
