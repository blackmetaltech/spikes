// cheap-circuit rows (see tasks_cheap_test.cpp for the format). Owner: rows for the levels of agent b: the spinal chapter (stretch_reflex, renshaw,
// posture, reach, boss_spinal_sprint) and micro-puzzles 11-19 (m_recruitment, m_reciprocal, m_recurrent, m_negative_feedback, m_feedforward,
// m_timed_bursts, m_rhythm, m_wave, m_divergent). docs/REQ-0.4.md rule 3: an empty bench, a single cell, a tonic cell with every weight 0, the reference
// minus the taught mechanism, "max every weight", every cheap circuit of docs/REVIEW-HARSH.md's appendix (m_recurrent: one tonic cell, I_e 380 / 450 /
// 600 / 900, inputs at weight 0). A row with a word also pins that the verdict names the missing mechanism.
//
// cutw: like cheap::cut but also scales the weights of `edges` projections (the spinal references wire Ia > MN with edges and a weight spread).
auto cutw = []( const std::string& pre, const std::string& post, double f ) -> cheap::Mut {
  return [ = ]( json& c ) {
    bool hit = false;
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == pre && p[ "post" ] == post )
      {
        hit = true;
        if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) ) p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * f;
        if ( p.contains( "edges" ) ) for ( auto& e : p[ "edges" ] ) e[ 2 ] = e[ 2 ].get< double >() * f;
      }
    CHECK_MSG( hit, "edit matched no projection " + pre + ">" + post );
  };
};
auto cutw2 = [ & ]( const std::string& a1, const std::string& b1, const std::string& a2, const std::string& b2 ) -> cheap::Mut {
  return cheap::both( cutw( a1, b1, 0.0 ), cutw( a2, b2, 0.0 ) );
};
auto only_ia_to = []( const std::string& sensor, const std::string& pop ) -> cheap::Mut {
  return [ = ]( json& c ) {
    for ( auto& i : c[ "inputs" ] )
      if ( i[ "name" ] == sensor ) i[ "targets" ] = json::array( { { { "pop", pop } } } );
  };
};

// zerow / maxw: cheap::zero_all / a spinal variant of cheap::max_all (conductance posts by the population id; inputs doubled when any cell is conductance-based).
auto zerow = cheap::zero_all;
auto maxw = []( json& c ) {
  auto cond = [ & ]( const std::string& pop ) {
    for ( auto& p : c[ "populations" ] )
      if ( p[ "id" ] == pop ) return p[ "model" ].get< std::string >().find( "cond" ) != std::string::npos;
    return false;
  };
  for ( auto& p : c[ "projections" ] )
  {
    const bool cd = cond( p[ "post" ].get< std::string >() );  // conductance cells abort inside NEST at about 4x the reference weights: keep those to 2x
    if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) )
    {
      const double w = p[ "synapse" ][ "weight" ].get< double >();
      p[ "synapse" ][ "weight" ] = cd ? 2.0 * w : ( w < 0 ? -1.0 : 1.0 ) * 4000.0;
    }
    if ( p.contains( "edges" ) )
      for ( auto& e : p[ "edges" ] ) { const double w = e[ 2 ].get< double >(); e[ 2 ] = cd ? 2.0 * w : ( w < 0 ? -1.0 : 1.0 ) * 4000.0; }
  }
  for ( auto& i : c[ "inputs" ] )
  {
    const double w = i[ "weight" ].get< double >();
    bool any_cond = false;
    for ( auto& p : c[ "populations" ] ) any_cond = any_cond || p[ "model" ].get< std::string >().find( "cond" ) != std::string::npos;
    i[ "weight" ] = any_cond ? 2.0 * w : ( w < 0 ? -1.0 : 1.0 ) * ( i.value( "kind", "" ) == "poisson" ? 4000.0 : 16000.0 );
  }
};

// ---- the degenerate benches, every level of this owner: empty bench, one cell with the lab's first-connect weights, one tonic cell with every weight 0 ----
for ( const char* t : { "stretch_reflex", "renshaw", "posture", "reach", "boss_spinal_sprint", "m_recruitment", "m_reciprocal", "m_recurrent", "m_negative_feedback",
                        "m_feedforward", "m_timed_bursts", "m_rhythm", "m_wave", "m_divergent" } )
{
  CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
  CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
  CHEAP_ROW( t, "one tonic cell, I_e 450, every weight 0", cheap::tonic( t, 450 ) );
  CHEAP_ROW( t, "reference with every weight 0", cheap::from_ref( t, zerow ) );
  // (m_timed_bursts has its own extreme-weight row below: its taught mechanism is the DELAYED chain, and a chain of saturating links with the delays kept is still that mechanism)
  // (the spinal references are conductance-based and NEST aborts at about 4x their weights, so "extreme" can only be 2x there, and the references pass at 2x: no extreme row for them)
  if ( std::string( t ).rfind( "m_", 0 ) == 0 && std::string( t ) != "m_timed_bursts" ) CHEAP_ROW( t, "reference with every weight at an extreme", cheap::from_ref( t, maxw ) );
}
CHEAP_ROW( "m_timed_bursts", "reference with every weight at an extreme and the lab's default delay", cheap::from_ref( "m_timed_bursts", cheap::both( maxw, cheap::both( cheap::set_delay( "G1", "G2", 2.0 ), cheap::set_delay( "G2", "G3", 2.0 ) ) ) ) );
// the appendix of docs/REVIEW-HARSH.md: a lone tonic cell passed m_recurrent (I_e 380, 450, 600, 900; drive and kick at weight 0, no projections)
for ( double ie : { 380.0, 450.0, 600.0, 900.0 } )
  CHEAP_ROW_V( "m_recurrent", "appendix: one tonic cell, I_e " + std::to_string( static_cast< int >( ie ) ), "Renshaw", cheap::tonic( "m_recurrent", ie ) );
// a tonic cell that also gets its sensors
for ( const char* t : { "m_recurrent", "m_negative_feedback", "m_feedforward", "m_reciprocal", "m_wave", "m_divergent", "m_recruitment" } )
{
  cheap::Bench tb = cheap::channels( t, { "N1" }, { "N1" } );
  tb.pop( "N1", "iaf_psc_alpha", 1, { { "I_e", 600.0 } } );
  CHEAP_ROW( t, "one tonic cell, I_e 600, sensors wired", tb.c );
}

// ---- micro-puzzles: the reference minus the taught mechanism ----
CHEAP_ROW_V( "m_recruitment", "reference with identical thresholds (every bias 0)", "identical", cheap::from_ref( "m_recruitment", cheap::both( cheap::set_params( "MU_S", { { "I_e", 0.0 } } ), cheap::set_params( "MU_L", { { "I_e", 0.0 } } ) ) ) );
CHEAP_ROW_V( "m_recruitment", "reference with two pools sharing one threshold", "identical", cheap::from_ref( "m_recruitment", cheap::set_params( "MU_S", { { "I_e", 0.0 } } ) ) );
{
  cheap::Bench pb = cheap::channels( "m_recruitment", { "P" }, { "P" } );
  pb.pop( "P", "iaf_psc_alpha", 1, { { "I_e", 100.0 } } );
  CHEAP_ROW_V( "m_recruitment", "one pool read on all three channels", "identical", pb.c );
}

CHEAP_ROW_V( "m_reciprocal", "reference without the inhibitory interneuron's link", "inhibitory", cheap::from_ref( "m_reciprocal", cheap::cut( "IN", "MN_E" ) ) );
CHEAP_ROW_V( "m_reciprocal", "interneuron link left excitatory", "inhibitory", cheap::from_ref( "m_reciprocal", cheap::cut( "IN", "MN_E", -0.5 ) ) );
CHEAP_ROW_V( "m_reciprocal", "ia_flexor cut from the interneuron", "interneuron", cheap::from_ref( "m_reciprocal", only_ia_to( "ia_flexor", "MN_F" ) ) );

CHEAP_ROW_V( "m_recurrent", "reference without the Renshaw cells' link back", "inhibit", cheap::from_ref( "m_recurrent", cheap::cut( "RC", "MN" ) ) );
CHEAP_ROW_V( "m_recurrent", "reference without the motoneurons driving the Renshaw cells", "drive", cheap::from_ref( "m_recurrent", cheap::cut( "MN", "RC" ) ) );
CHEAP_ROW_V( "m_recurrent", "Renshaw cells set excitatory (wrong sign)", "inhibit", cheap::from_ref( "m_recurrent", cheap::cut( "RC", "MN", -1.0 ) ) );
{
  cheap::Bench sb = cheap::channels( "m_recurrent", { "MN" }, { "MN" } );
  sb.pop( "MN", "iaf_psc_alpha", 4 ).proj( "MN", "MN", -600.0 );
  CHEAP_ROW_V( "m_recurrent", "motoneurons inhibit themselves (no Renshaw cell)", "Renshaw", sb.c );
}
CHEAP_ROW( "m_recurrent", "Renshaw loop at the lab's default weight", cheap::from_ref( "m_recurrent", cheap::both( cheap::cut( "MN", "RC", 0.1 ), cheap::cut( "RC", "MN", 0.1 ) ) ) );

CHEAP_ROW_V( "m_negative_feedback", "reference without the feedback link", "inhibitory", cheap::from_ref( "m_negative_feedback", cheap::cut( "INH", "EFF" ) ) );
CHEAP_ROW_V( "m_negative_feedback", "feedback with the wrong sign (excitatory)", "wrong sign", cheap::from_ref( "m_negative_feedback", cheap::cut( "INH", "EFF", -1.0 ) ) );
CHEAP_ROW_V( "m_negative_feedback", "level sensor cut", "level", cheap::from_ref( "m_negative_feedback", cheap::scale_input( "level", 0.0 ) ) );

CHEAP_ROW_V( "m_feedforward", "reference without feedback (feed-forward only)", "feedback", cheap::from_ref( "m_feedforward", cheap::cut( "INH", "EFF" ) ) );
CHEAP_ROW_V( "m_feedforward", "reference without the cue (feedback only)", "cue", cheap::from_ref( "m_feedforward", cheap::scale_input( "cue", 0.0 ) ) );
CHEAP_ROW_V( "m_feedforward", "feedback with the wrong sign", "feedback", cheap::from_ref( "m_feedforward", cheap::cut( "INH", "EFF", -1.0 ) ) );

CHEAP_ROW_V( "m_timed_bursts", "reference without the second link (the third burst never comes)", "chain", cheap::from_ref( "m_timed_bursts", cheap::cut( "G2", "G3" ) ) );
CHEAP_ROW_V( "m_timed_bursts", "reference without the first link", "chain", cheap::from_ref( "m_timed_bursts", cheap::cut( "G1", "G2" ) ) );
CHEAP_ROW( "m_timed_bursts", "reference with no delays on the links", cheap::from_ref( "m_timed_bursts", cheap::both( cheap::set_delay( "G1", "G2", 2.0 ), cheap::set_delay( "G2", "G3", 2.0 ) ) ) );
{
  cheap::Bench gb = cheap::channels( "m_timed_bursts", { "G" }, { "G" } );
  gb.pop( "G", "iaf_psc_alpha", 4 );
  CHEAP_ROW_V( "m_timed_bursts", "one group read out on all three channels", "different groups", gb.c );
}

CHEAP_ROW_V( "m_rhythm", "reference with an integrate-and-fire cell (no burst mechanism)", "burst", cheap::from_ref( "m_rhythm", cheap::set_model( "PBC", "iaf_psc_alpha" ) ) );
CHEAP_ROW_V( "m_rhythm", "reference with the sensor cut", "chemo", cheap::from_ref( "m_rhythm", cheap::scale_input( "chemo", 0.0 ) ) );
CHEAP_ROW( "m_rhythm", "bursting cell with the lab's 300 pA per unit", cheap::from_ref( "m_rhythm", cheap::scale_input( "chemo", 60.0 ) ) );
CHEAP_ROW( "m_rhythm", "bursting cell on a bias current, sensor weight 0", cheap::from_ref( "m_rhythm", cheap::both( cheap::scale_input( "chemo", 0.0 ), cheap::set_params( "PBC", { { "I_e", 6.0 } } ) ) ) );

CHEAP_ROW_V( "m_wave", "reference without the descending inhibition", "descending", cheap::from_ref( "m_wave", cheap::cut( "IN", "SEG2" ) ) );
CHEAP_ROW_V( "m_wave", "descending link left excitatory", "descending", cheap::from_ref( "m_wave", cheap::cut( "IN", "SEG2", -1.0 ) ) );
CHEAP_ROW_V( "m_wave", "reference without the ascending excitation", "ascending", cheap::from_ref( "m_wave", only_ia_to( "dist1", "IN" ) ) );

CHEAP_ROW_V( "m_divergent", "reference without the inhibitory route to the gut", "inhibitory", cheap::from_ref( "m_divergent", cheap::cut( "INH", "GUT" ) ) );
CHEAP_ROW_V( "m_divergent", "command group no longer reaches heart and pupil", "command", cheap::from_ref( "m_divergent", cheap::both( cheap::cut( "CMD", "HEART" ), cheap::cut( "CMD", "PUPIL" ) ) ) );
CHEAP_ROW( "m_divergent", "every effector at the heart's weight", cheap::from_ref( "m_divergent", cheap::both( cheap::cut( "CMD", "PUPIL", 600.0 / 280.0 ), cheap::cut( "CMD", "SWEAT", 600.0 / 280.0 ) ) ) );
{
  cheap::Bench db = cheap::channels( "m_divergent", {}, {} );
  db.pop( "H" ).pop( "P" ).pop( "S" ).pop( "G" );
  for ( auto& i : db.c[ "inputs" ] )
    i[ "targets" ] = i[ "name" ] == "alarm" ? json::array( { { { "pop", "H" } }, { { "pop", "P" } }, { { "pop", "S" } } } ) : json::array( { { { "pop", "G" } } } );
  for ( auto& o : db.c[ "outputs" ] )
  {
    const std::string n = o[ "name" ].get< std::string >();
    o[ "neurons" ] = json::array( { { { "pop", n == "heart" ? "H" : ( n == "pupil" ? "P" : ( n == "sweat" ? "S" : "G" ) ) } } } );
  }
  CHEAP_ROW_V( "m_divergent", "alarm drives every effector directly (no command group)", "command", db.c );
}

// ---- spinal chapter: the reference minus the taught mechanism ----
CHEAP_ROW_V( "stretch_reflex", "Ia cut from its motoneurons (no monosynaptic reflex)", "monosynaptic", cheap::from_ref( "stretch_reflex", cutw2( "Ia_F", "MN_F", "Ia_E", "MN_E" ) ) );
CHEAP_ROW_V( "stretch_reflex", "no reciprocal inhibition (interneurons cut from the antagonist)", "reciprocal", cheap::from_ref( "stretch_reflex", cutw2( "IN_F", "MN_E", "IN_E", "MN_F" ) ) );
CHEAP_ROW_V( "stretch_reflex", "Ia cut from the inhibitory interneurons", "reciprocal", cheap::from_ref( "stretch_reflex", cutw2( "Ia_F", "IN_F", "Ia_E", "IN_E" ) ) );

CHEAP_ROW_V( "renshaw", "motoneuron collaterals cut (no Renshaw loop)", "recurrent", cheap::from_ref( "renshaw", cutw2( "MN_F", "RC_F", "MN_E", "RC_E" ) ) );
CHEAP_ROW_V( "renshaw", "Renshaw cells cannot inhibit the motoneurons", "recurrent", cheap::from_ref( "renshaw", cutw2( "RC_F", "MN_F", "RC_E", "MN_E" ) ) );

CHEAP_ROW_V( "posture", "spindle afferents cut from the motoneurons", "spindle", cheap::from_ref( "posture", cheap::both( cutw2( "Ia_F", "MN_F", "Ia_E", "MN_E" ), cutw2( "II_F", "MN_F", "II_E", "MN_E" ) ) ) );
CHEAP_ROW_V( "posture", "no fusimotor drive (tonic group cut from the gamma pool)", "fusimotor", cheap::from_ref( "posture", cutw( "TONIC", "GAM", 0.0 ) ) );

CHEAP_ROW_V( "reach", "no antagonist burst (second burst group cut from the extensor)", "antagonist", cheap::from_ref( "reach", cutw( "B2", "MN_E", 0.0 ) ) );
CHEAP_ROW_V( "reach", "no agonist burst (go cut from the first burst group)", "agonist", cheap::from_ref( "reach", cutw( "GO", "B1", 0.0 ) ) );

CHEAP_ROW_V( "boss_spinal_sprint", "no Renshaw loop (collaterals cut)", "recurrent", cheap::from_ref( "boss_spinal_sprint", cutw2( "MN_F", "RC_F", "MN_E", "RC_E" ) ) );
CHEAP_ROW_V( "boss_spinal_sprint", "no burst generator (second burst group cut from the extensor)", "antagonist", cheap::from_ref( "boss_spinal_sprint", cutw( "B2", "MN_E", 0.0 ) ) );
CHEAP_ROW_V( "boss_spinal_sprint", "no reciprocal inhibition", "reciprocal", cheap::from_ref( "boss_spinal_sprint", cutw2( "IN_F", "MN_E", "IN_E", "MN_F" ) ) );
CHEAP_ROW_V( "boss_spinal_sprint", "no spindle feedback", "spindle", cheap::from_ref( "boss_spinal_sprint", cheap::both( cutw2( "Ia_F", "MN_F", "Ia_E", "MN_E" ), cutw2( "II_F", "MN_F", "II_E", "MN_E" ) ) ) );
