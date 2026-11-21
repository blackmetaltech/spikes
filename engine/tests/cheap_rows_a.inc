// Cheap-circuit rows for the levels of micro set A: reflex, worm, lamprey, m_threshold, m_excitation, m_convergence, m_inhibition, m_subtraction,
// m_adaptation, m_half_centre, m_pacemaking, m_coupling, m_neuromodulation. Included inside rows() of tasks_cheap_test.cpp (a row is a statement).
// Every row must FAIL on seeds 1 and 2 (quick episode). Per level: the empty bench, a single cell, a tonic cell (REVIEW-HARSH appendix: I_e 380, 450,
// 600, 900, every input at weight 0), the reference with every weight at 0, with every weight at an extreme, the reference minus its mechanism, and the
// appendix row of the level.

const std::vector< std::string > A_LEVELS = { "reflex", "worm", "lamprey", "m_threshold", "m_excitation", "m_convergence", "m_inhibition", "m_subtraction",
  "m_adaptation", "m_half_centre", "m_pacemaking", "m_coupling", "m_neuromodulation" };
for ( const auto& t : A_LEVELS )
{
  CHEAP_ROW( t, "empty bench", empty( t ) );
  CHEAP_ROW( t, "a single default cell on every channel", single_cell( t ) );
  // m_pacemaking's reference has no weights at all (a lone cell), so "every weight 0 / extreme" IS the reference there
  if ( t != "m_pacemaking" ) CHEAP_ROW( t, "the reference with every weight 0", zero_weights( t ) );
  // "max every weight" is only a degenerate circuit where the weights are the puzzle. It is still a valid solution where the lesson is not a weight:
  //   worm        an ON-minus-OFF difference at saturating gain still steers (the mechanism, not the gain, is what the level asks for)
  //   lamprey     the reference's inhibition x2 still swims (x4 crashes NEST's integrator, see the report)
  //   m_adaptation / m_coupling / m_neuromodulation / m_pacemaking   the lesson is a property of the cell (its adaptation current, its own rhythm, a chemical)
  // For those, hand-built rows below hold the degenerate circuits (default cells at extreme weights).
  if ( t != "worm" && t != "lamprey" && t != "m_adaptation" && t != "m_coupling" && t != "m_neuromodulation" && t != "m_pacemaking" )
    CHEAP_ROW( t, "the reference with every weight at an extreme", max_weights( t ) );
  CHEAP_ROW( t, "one silent cell, all inputs at weight 0", [ & ] {
    Bench b = channels( t, { "N1" }, { "N1" }, 0.0 );
    b.pop( "N1" );
    return b.c;
  }() );
  const bool heavy = t == "worm" || t == "lamprey" || t == "reflex";  // long episodes: two of the four bias currents
  for ( double ie : { 380.0, 450.0, 600.0, 900.0 } )
    if ( !heavy || ie == 450.0 || ie == 900.0 )
    CHEAP_ROW( t, "one tonic cell, I_e " + std::to_string( static_cast< int >( ie ) ) + " pA, inputs at weight 0", tonic( t, ie ) );
}
// the pacemaker cards are the one place a tonic-looking cell can be the right part: a tonic SA node cell with a bias current is still pushed
for ( const char* t : { "m_pacemaking", "m_coupling", "m_neuromodulation" } )
  CHEAP_ROW( t, "SA node cell(s) pushed by a bias current", single_cell( t, "sa_node_pacemaker", { { "I_e", 100.0 } } ) );

// ---- m_threshold: the appendix row was "the reference, pulse weight swept: pass band 400-600 pA". The band is now narrower and moves with the seed.
for ( double w : { 300.0, 640.0, 800.0, 1000.0, 1500.0 } )
  CHEAP_ROW( "m_threshold", "the reference with the pulse weight " + std::to_string( static_cast< int >( w ) ), from_ref( "m_threshold", [ = ]( json& c ) { c[ "inputs" ][ 0 ][ "weight" ] = w; } ) );
CHEAP_ROW_V( "m_threshold", "reference + a bias current on the cell", "bias", from_ref( "m_threshold", set_params( "N1", { { "I_e", 200.0 } } ) ) );
CHEAP_ROW( "m_threshold", "reference with the pulse cut", from_ref( "m_threshold", scale_input( "pulse", 0 ) ) );

// ---- m_excitation
CHEAP_ROW_V( "m_excitation", "reference minus the synapse (both cells fed by the tap)", "synapse", [] {
  Bench b;
  b.pop( "N1" ).pop( "N2" ).in( "tap", "dc", 300, { "N1", "N2" } ).out( "source", { "N1" } ).out( "echo", { "N2" } );
  return b.c;
}() );
CHEAP_ROW_V( "m_excitation", "reference with the link inhibitory", "excit", from_ref( "m_excitation", cut( "N1", "N2", -1 ) ) );
CHEAP_ROW( "m_excitation", "reference with the link cut", from_ref( "m_excitation", cut( "N1", "N2", 0 ) ) );
CHEAP_ROW_V( "m_excitation", "one cell on source and on echo", "same cell", [] {
  Bench b;
  b.pop( "N1" ).in( "tap", "dc", 300, { "N1" } ).out( "source", { "N1" } ).out( "echo", { "N1" } );
  return b.c;
}() );
CHEAP_ROW( "m_excitation", "reference with the default delay", from_ref( "m_excitation", set_delay( "N1", "N2", 2.0 ) ) );

// ---- m_convergence: one input is enough to fire at a big weight; three one-input cells; one input wired
CHEAP_ROW( "m_convergence", "one cell fed by in_a alone, any weight", [] {
  Bench b;
  b.pop( "N1" ).in( "in_a", "dc", 450, { "N1" } ).in( "in_b", "dc", 0, {} ).in( "in_c", "dc", 0, {} ).out( "out", { "N1" } );
  return b.c;
}() );
CHEAP_ROW_V( "m_convergence", "reference minus convergence: each input has a cell of its own", "converge", [] {
  Bench b;
  b.pop( "N1" ).pop( "N2" ).pop( "N3" ).in( "in_a", "dc", 150, { "N1" } ).in( "in_b", "dc", 150, { "N2" } ).in( "in_c", "dc", 150, { "N3" } ).out( "out", { "N1" } );
  return b.c;
}() );
CHEAP_ROW( "m_convergence", "reference with in_a cut", from_ref( "m_convergence", scale_input( "in_a", 0 ) ) );
CHEAP_ROW( "m_convergence", "reference with the weights x2 (pairs fire)", from_ref( "m_convergence", scale_all_inputs( 2.0 ) ) );

// ---- m_inhibition: the stop must act through an inhibitory cell
CHEAP_ROW_V( "m_inhibition", "stop wired straight onto the cell (a sensor can only excite)", "inhibitory", [] {
  Bench b;
  b.pop( "N1" ).in( "drive", "dc", 300, { "N1" } ).in( "stop", "dc", 300, { "N1" } ).out( "out", { "N1" } );
  return b.c;
}() );
CHEAP_ROW_V( "m_inhibition", "stop cell made excitatory (link sign flipped)", "inhibitory", from_ref( "m_inhibition", cut( "N2", "N1", -1 ) ) );
CHEAP_ROW( "m_inhibition", "reference with the inhibitory link cut", from_ref( "m_inhibition", cut( "N2", "N1", 0 ) ) );
CHEAP_ROW( "m_inhibition", "stop on a cell that is not connected to the output", [] {
  Bench b;
  b.pop( "N1" ).pop( "N2" ).in( "drive", "dc", 300, { "N1" } ).in( "stop", "dc", 300, { "N2" } ).out( "out", { "N1" } );
  return b.c;
}() );
CHEAP_ROW( "m_inhibition", "reference with the inhibition at the default weight", from_ref( "m_inhibition", cut( "N2", "N1", 0.08 ) ) );

// ---- m_subtraction
CHEAP_ROW_V( "m_subtraction", "minus excites the output too (no inhibition)", "inhibitory", from_ref( "m_subtraction", cut( "N2", "N1", -1 ) ) );
CHEAP_ROW( "m_subtraction", "reference with minus cut off", from_ref( "m_subtraction", cut( "N2", "N1", 0 ) ) );
CHEAP_ROW_V( "m_subtraction", "plus and minus on the same cell as two excitatory inputs", "inhibitory", [] {
  Bench b;
  b.pop( "N1" ).in( "plus", "dc", 300, { "N1" } ).in( "minus", "dc", 300, { "N1" } ).out( "diff", { "N1" } );
  return b.c;
}() );
CHEAP_ROW( "m_subtraction", "reference with the inhibition far too strong", from_ref( "m_subtraction", cut( "N2", "N1", 3.0 ) ) );
CHEAP_ROW( "m_subtraction", "reference with plus cut", from_ref( "m_subtraction", scale_input( "plus", 0 ) ) );

// ---- m_adaptation: the cell has to adapt by itself
CHEAP_ROW_V( "m_adaptation", "the reference with a plain integrate-and-fire cell", "adaptation", from_ref( "m_adaptation", set_model( "N1", "iaf_psc_alpha" ) ) );
CHEAP_ROW_V( "m_adaptation", "an adaptive cell with the jump b = 0", "adaptation", from_ref( "m_adaptation", set_params( "N1", { { "b", 0.0 } } ) ) );
CHEAP_ROW( "m_adaptation", "an adaptive cell as it comes (default tau_w)", from_ref( "m_adaptation", set_params( "N1", { { "tau_w", 144.0 } } ) ) );
CHEAP_ROW( "m_adaptation", "an adaptive cell with a very short tau_w", from_ref( "m_adaptation", set_params( "N1", { { "tau_w", 20.0 } } ) ) );
CHEAP_ROW( "m_adaptation", "an adaptive cell as it comes at the largest step weight", [] {
  Bench b;
  b.pop( "N1", "aeif_cond_alpha" ).in( "step", "dc", 2000, { "N1" } ).out( "spikes", { "N1" } );
  return b.c;
}() );
CHEAP_ROW( "m_adaptation", "plain cell at a huge step weight", [] {
  Bench b;
  b.pop( "N1" ).in( "step", "dc", 3000, { "N1" } ).out( "spikes", { "N1" } );
  return b.c;
}() );

// ---- m_half_centre: reciprocal inhibition
CHEAP_ROW_V( "m_half_centre", "reference minus one inhibitory link", "inhibit", from_ref( "m_half_centre", cut( "N2", "N1", 0 ) ) );
CHEAP_ROW_V( "m_half_centre", "reference minus both inhibitory links", "inhibit", from_ref( "m_half_centre", both( cut( "N1", "N2", 0 ), cut( "N2", "N1", 0 ) ) ) );
CHEAP_ROW_V( "m_half_centre", "mutual excitation instead of inhibition", "inhibit", from_ref( "m_half_centre", both( cut( "N1", "N2", -1 ), cut( "N2", "N1", -1 ) ) ) );
CHEAP_ROW( "m_half_centre", "reference without adaptation", from_ref( "m_half_centre", both( set_params( "N1", { { "b", 0.0 }, { "a", 0.0 } } ), set_params( "N2", { { "b", 0.0 }, { "a", 0.0 } } ) ) ) );
CHEAP_ROW( "m_half_centre", "two plain cells with strong inhibition (winner takes all)", [] {
  Bench b;
  b.pop( "N1" ).pop( "N2" ).in( "drive", "dc", 450, { "N1", "N2" } ).out( "left", { "N1" } ).out( "right", { "N2" } ).proj( "N1", "N2", -1600 ).proj( "N2", "N1", -1600 );
  return b.c;
}() );
CHEAP_ROW_V( "m_half_centre", "one cell on left and right", "same cell", [] {
  Bench b;
  b.pop( "N1", "aeif_cond_alpha" ).in( "drive", "dc", 600, { "N1" } ).out( "left", { "N1" } ).out( "right", { "N1" } );
  return b.c;
}() );

// ---- m_pacemaking: the lab opens on a plain cell; a pacemaker must be intrinsic
CHEAP_ROW( "m_pacemaking", "the lab's start: a plain cell on beat", [] {
  Bench b;
  b.pop( "N1" ).out( "beat", { "N1" } );
  return b.c;
}() );
CHEAP_ROW_V( "m_pacemaking", "a plain cell with a bias current just over its rheobase", "bias", [] {
  Bench b;
  b.pop( "N1", "iaf_psc_alpha", 1, { { "I_e", 380.0 } } ).out( "beat", { "N1" } );
  return b.c;
}() );
CHEAP_ROW_V( "m_pacemaking", "an SA node cell pushed by another cell", "driven", [] {
  Bench b;
  b.pop( "N0", "iaf_psc_alpha", 1, { { "I_e", 450.0 } } ).pop( "N1", "sa_node_pacemaker" ).out( "beat", { "N1" } ).proj( "N0", "N1", 2.0 );
  return b.c;
}() );
CHEAP_ROW( "m_pacemaking", "an adaptive cell with a bias current", [] {
  Bench b;
  b.pop( "N1", "aeif_cond_alpha", 1, { { "I_e", 500.0 } } ).out( "beat", { "N1" } );
  return b.c;
}() );

// ---- m_coupling
CHEAP_ROW_V( "m_coupling", "two SA node cells, uncoupled", "coupl", from_ref( "m_coupling", both( cut( "N1", "N2", 0 ), cut( "N2", "N1", 0 ) ) ) );
CHEAP_ROW( "m_coupling", "two SA node cells at the default weight", from_ref( "m_coupling", both( cut( "N1", "N2", 0.1 ), cut( "N2", "N1", 0.1 ) ) ) );
CHEAP_ROW_V( "m_coupling", "one SA node cell on a and b", "same cell", [] {
  Bench b;
  b.pop( "N1", "sa_node_pacemaker" ).out( "a", { "N1" } ).out( "b", { "N1" } );
  return b.c;
}() );
CHEAP_ROW( "m_coupling", "plain cells with bias currents, coupled", [] {
  Bench b;
  b.pop( "N1", "iaf_psc_alpha", 1, { { "I_e", 380.0 } } ).pop( "N2", "iaf_psc_alpha", 1, { { "I_e", 380.0 } } ).out( "a", { "N1" } ).out( "b", { "N2" } ).proj( "N1", "N2", 200 ).proj( "N2", "N1", 200 );
  return b.c;
}() );

// ---- m_neuromodulation: the slowing has to come through the chemical
CHEAP_ROW( "m_neuromodulation", "reference with the ACh gain x0.05", from_ref( "m_neuromodulation", scale_output_gain( "vagal", 0.05 ) ) );
CHEAP_ROW( "m_neuromodulation", "reference with the vagus cut", from_ref( "m_neuromodulation", scale_input( "vagus", 0 ) ) );
CHEAP_ROW_V( "m_neuromodulation", "pacemaker slowed by a synapse from the vagal cell", "synapse", [] {
  Bench b;
  const json m = { { "modulator", "ACh" }, { "gain", 0.012 }, { "tau_ms", 500.0 } };
  b.pop( "N1" ).pop( "N2", "sa_node_pacemaker" ).in( "vagus", "dc", 300, { "N1" } ).out( "beat", { "N2" } ).out( "vagal", { "N1" }, m ).proj( "N1", "N2", -2.0 );
  return b.c;
}() );
CHEAP_ROW_V( "m_neuromodulation", "pacemaker driven by the vagus sensor itself", "synapse", [] {
  Bench b;
  const json m = { { "modulator", "ACh" }, { "gain", 0.012 }, { "tau_ms", 500.0 } };
  b.pop( "N1" ).pop( "N2", "sa_node_pacemaker" ).in( "vagus", "dc", 300, { "N1", "N2" } ).out( "beat", { "N2" } ).out( "vagal", { "N1" }, m );
  return b.c;
}() );
CHEAP_ROW( "m_neuromodulation", "one SA node cell on beat and vagal", [] {
  Bench b;
  const json m = { { "modulator", "ACh" }, { "gain", 0.012 }, { "tau_ms", 500.0 } };
  b.pop( "N1", "sa_node_pacemaker" ).in( "vagus", "dc", 300, {} ).out( "beat", { "N1" } ).out( "vagal", { "N1" }, m );
  return b.c;
}() );
CHEAP_ROW( "m_neuromodulation", "reference with far too many cells on vagal (the node stops)", from_ref( "m_neuromodulation", []( json& c ) {
  c[ "populations" ][ 0 ][ "n" ] = 40;
  c[ "populations" ][ 0 ][ "pos" ] = json::array();
  for ( int k = 0; k < 40; ++k ) c[ "populations" ][ 0 ][ "pos" ].push_back( { 100 + 10 * k, 60 } );
} ) );

// ---- reflex: convergence
CHEAP_ROW_V( "reflex", "touch straight onto a motor pool, one afferent per cell (no convergence)", "converge", [] {
  Bench b;
  b.pop( "MN", "iaf_psc_alpha", 8 ).in( "touch", "poisson", 280, { "MN" } ).out( "withdraw", { "MN" } );
  return b.c;
}() );
CHEAP_ROW_V( "reflex", "a lone touch-to-motor cell", "receptor", [] {
  Bench b;
  b.pop( "SN" ).pop( "MN" ).in( "touch", "poisson", 280, { "SN" } ).out( "withdraw", { "MN" } ).proj( "SN", "MN", 380 );
  return b.c;
}() );
CHEAP_ROW_V( "reflex", "ten receptor cells, each wired to one motor cell (one_to_one)", "converge", [] {
  Bench b;
  b.pop( "SN", "iaf_psc_alpha", 10 ).pop( "MN", "iaf_psc_alpha", 10 ).in( "touch", "poisson", 280, { "SN" } ).out( "withdraw", { "MN" } ).proj( "SN", "MN", 380 );
  b.c[ "projections" ][ 0 ][ "rule" ] = "one_to_one";
  return b.c;
}() );
CHEAP_ROW( "reflex", "reference with SN > IN cut", from_ref( "reflex", cut( "SN", "IN", 0 ) ) );
CHEAP_ROW( "reflex", "reference with the receptor link far too strong (fires on the chatter)", from_ref( "reflex", cut( "SN", "IN", 12.0 ) ) );

// ---- worm: the ON/OFF difference
CHEAP_ROW_V( "worm", "ASER alone drives a turn (no subtraction)", "difference", [] {
  Bench b;
  b.pop( "A", "iaf_psc_alpha", 8 ).pop( "TL", "iaf_psc_alpha", 8 ).pop( "TR", "iaf_psc_alpha", 1 ).pop( "X", "iaf_psc_alpha", 1 ).in( "ASER", "poisson", 1000, { "A" } )
    .in( "ASEL", "poisson", 1000, { "X" } ).out( "turn_left", { "TL" } ).out( "turn_right", { "TR" } ).proj( "A", "TL", 1000 );
  return b.c;
}() );
CHEAP_ROW_V( "worm", "reference with ASEL > AIY cut (nothing subtracts)", "difference", from_ref( "worm", cut( "ASEL", "AIY", 0 ) ) );
CHEAP_ROW_V( "worm", "both salt cells excite the same turn pool", "difference", from_ref( "worm", cut( "AIY", "AIB", -0.6 ) ) );
CHEAP_ROW( "worm", "reference with ASER > AIB cut", from_ref( "worm", cut( "ASER", "AIB", 0 ) ) );
CHEAP_ROW( "worm", "the worm always turns (tonic turn pool)", [] {
  Bench b;
  b.pop( "TL", "iaf_psc_alpha", 4, { { "I_e", 600.0 } } ).pop( "TR", "iaf_psc_alpha", 4 ).pop( "X", "iaf_psc_alpha", 1 ).in( "ASEL", "poisson", 1000, { "X" } ).in( "ASER", "poisson", 1000, { "X" } ).out( "turn_left", { "TL" } ).out( "turn_right", { "TR" } );
  return b.c;
}() );

// ---- lamprey: crossed inhibition
CHEAP_ROW_V( "lamprey", "reference minus the crossing (commissural links cut)", "inhibit", from_ref( "lamprey", both( cut( "CC_L", "E_R", 0 ), both( cut( "CC_L", "MN_R", 0 ), both( cut( "CC_R", "E_L", 0 ), cut( "CC_R", "MN_L", 0 ) ) ) ) ) );
CHEAP_ROW_V( "lamprey", "crossed inhibition one way only", "one way", from_ref( "lamprey", both( cut( "CC_R", "E_L", 0 ), cut( "CC_R", "MN_L", 0 ) ) ) );
CHEAP_ROW( "lamprey", "reference with the commissural links made (weakly) excitatory", from_ref( "lamprey", both( cut( "CC_L", "E_R", -0.05 ), both( cut( "CC_L", "MN_R", -0.05 ), both( cut( "CC_R", "E_L", -0.05 ), cut( "CC_R", "MN_L", -0.05 ) ) ) ) ) );
CHEAP_ROW( "lamprey", "two plain pools, tonic, no coupling", [] {
  Bench b;
  b.pop( "L", "aeif_cond_alpha", 8 ).pop( "R", "aeif_cond_alpha", 8 ).in( "drive", "dc", 600, { "L", "R" } ).out( "left_motor", { "L" } ).out( "right_motor", { "R" } );
  return b.c;
}() );
CHEAP_ROW_V( "lamprey", "one pool on both motor channels", "same cells", [] {
  Bench b;
  b.pop( "L", "aeif_cond_alpha", 8 ).in( "drive", "dc", 600, { "L" } ).out( "left_motor", { "L" } ).out( "right_motor", { "L" } );
  return b.c;
}() );

// ---- cheat-proofing (full review, P0-3): token weights and recorded inputs must fail. The circuits are the reviewers' evidence (engine/tests/cheats/)
CHEAP_ROW_V( "m_excitation", "token synapse (0.01 pA) plus a parallel cell fed by the same tap", "synapse", fixture( "m_excitation_token" ) );
CHEAP_ROW( "m_coupling", "two pacemakers coupled at 0.001 nS", fixture( "m_coupling_token" ) );
CHEAP_ROW_V( "m_pacemaking", "an integrate-and-fire cell that rests above its own threshold", "threshold", fixture( "m_pacemaking_el_above_vth" ) );
