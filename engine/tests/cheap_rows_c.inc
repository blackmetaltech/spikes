// cheap-circuit rows (see tasks_cheap_test.cpp for the format). Owner: rows for the levels of agent c: the heart chapter (heart_pacemaker, heart_autonomic,
// baroreflex), breathing and the autonomic chapter (pupil, peristalsis, fight_or_flight, boss_autonomic_exam).
// docs/REQ-0.4.md rule 3: an empty bench, a single cell, a tonic cell with every weight 0, the reference minus the taught mechanism, "max every weight",
// the cheap circuits of docs/REVIEW-HARSH.md's appendix (heart_pacemaker: one default sa_node_pacemaker, no projections, passed 5/5 with 75 points; one
// tonic cell, I_e 380 / 450 / 600 / 900, passed nothing but m_recurrent). A row with a word also pins that the verdict names the missing mechanism.
//
// No rows for the six diagnosis cases (dx_*): they are decided by the submitted answer, never by the circuit, and this harness cannot submit one (its
// control, "the reference passes", would fail on every one of them). Their negative tests (nothing submitted, every wrong candidate, an unknown id,
// the right answer only after the circuit is the patient's) are in tasks_boss_dx_test.cpp.
//
// Names here are local to a block so that they cannot clash with the other owners' rows (all three files are included in one function).
{
  // like cheap::cut, but also scales the weights of `edges` projections (peristalsis wires its chains with edges)
  auto cut_c = []( const std::string& pre, const std::string& post ) -> cheap::Mut {
    return [ = ]( json& c ) {
      bool hit = false;
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == pre && p[ "post" ] == post )
        {
          hit = true;
          if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) ) p[ "synapse" ][ "weight" ] = 0.0;
          if ( p.contains( "edges" ) ) for ( auto& e : p[ "edges" ] ) e[ 2 ] = 0.0;
        }
      CHECK_MSG( hit, "edit matched no projection " + pre + ">" + post );
    };
  };
  auto cut2_c = [ & ]( const std::string& a1, const std::string& b1, const std::string& a2, const std::string& b2 ) -> cheap::Mut {
    return cheap::both( cut_c( a1, b1 ), cut_c( a2, b2 ) );
  };

  // ---- the degenerate benches, for every non-diagnosis level of this owner ----
  const std::vector< std::string > levels_c = { "heart_pacemaker", "heart_autonomic", "baroreflex", "breathing", "pupil", "peristalsis", "fight_or_flight",
                                                "boss_autonomic_exam" };
  for ( const auto& t : levels_c )
  {
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW( t, "one tonic cell, I_e 450, every weight 0", cheap::tonic( t, 450 ) );
    CHEAP_ROW( t, "reference with every weight 0", cheap::zero_weights( t ) );
    // not for heart_pacemaker: its only weights are the coupling, and a cluster coupled as strongly as possible is a correct answer to a coupling level
    // (it fires as one, regular, at the vagal-tone rate): there it is no cheap circuit. The lone cell, the uncoupled and the inhibitory rows below are.
    // Not for peristalsis either: its solution is the topology (every projection is a segment-shifted one-to-one chain, ascending and descending), and
    // saturating the weights keeps that topology: the bolus still arrives (in 9 s). Its cheap circuits are the missing-chain rows below.
    // Nor fight_or_flight: its doubled circuit still passes (a robust command with wide bands), so doubling is no cheap circuit there.
    if ( t != "heart_pacemaker" && t != "peristalsis" && t != "fight_or_flight" )
      CHEAP_ROW( t, "reference with every weight doubled (conductance weights much above 2x abort inside NEST)", cheap::from_ref( t, []( json& c ) {
        for ( auto& p : c[ "projections" ] )
        {
          if ( p.contains( "synapse" ) ) p[ "synapse" ][ "weight" ] = 2.0 * p[ "synapse" ][ "weight" ].get< double >();
          if ( p.contains( "edges" ) ) for ( auto& e : p[ "edges" ] ) e[ 2 ] = 2.0 * e[ 2 ].get< double >();
        }
        for ( auto& i : c[ "inputs" ] ) i[ "weight" ] = 2.0 * i[ "weight" ].get< double >();
      } ) );
  }
  // ---- the appendix of docs/REVIEW-HARSH.md ----
  // heart_pacemaker: 1 sa_node_pacemaker (catalogue default), no projections: 5/5 passes, 75.0 points, 64-71 bpm
  CHEAP_ROW_V( "heart_pacemaker", "appendix: one default sa_node_pacemaker, no projections", "cluster", cheap::single_cell( "heart_pacemaker" ) );
  {
    json one = cheap::reference( "heart_pacemaker" );  // the reference's own cell, alone
    one[ "populations" ] = json::array( { one[ "populations" ][ 0 ] } );
    one[ "populations" ][ 0 ][ "n" ] = 1;
    one[ "populations" ][ 0 ][ "pos" ] = json::array( { json::array( { 120, 200 } ) } );
    one[ "projections" ] = json::array();
    one[ "modulation" ] = json::array();
    one[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "SA_c" } } } );
    CHEAP_ROW_V( "heart_pacemaker", "one cell of the reference, no projections", "cluster", one );
    json lone_loop = one;  // one cell wired to itself is still one cell
    lone_loop[ "projections" ] = json::array( { { { "pre", "SA_c" }, { "post", "SA_c" }, { "rule", "all_to_all" },
      { "synapse", { { "synapse_model", "static_synapse" }, { "weight", 6.0 }, { "delay", 1.0 } } } } } );
    CHEAP_ROW_V( "heart_pacemaker", "one cell wired to itself", "cluster", lone_loop );
    json clones = cheap::reference( "heart_pacemaker" );  // five uncoupled identical clones
    clones[ "populations" ] = json::array( { clones[ "populations" ][ 0 ] } );
    clones[ "populations" ][ 0 ][ "n" ] = 5;
    clones[ "populations" ][ 0 ][ "pos" ] = json::array( { json::array( { 100, 100 } ), json::array( { 130, 100 } ), json::array( { 100, 130 } ),
                                                            json::array( { 130, 130 } ), json::array( { 160, 100 } ) } );
    clones[ "projections" ] = json::array();
    clones[ "modulation" ] = json::array();
    clones[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "SA_c" } } } );
    CHEAP_ROW_V( "heart_pacemaker", "five uncoupled identical clones", "cluster", clones );
    json inhib = cheap::reference( "heart_pacemaker" );  // coupled, but by inhibition: not the stand-in for gap junctions
    for ( auto& p : inhib[ "projections" ] ) p[ "synapse" ][ "weight" ] = -6.0;
    CHEAP_ROW_V( "heart_pacemaker", "reference with inhibitory coupling", "cluster", inhib );
  }
  CHEAP_ROW_V( "heart_pacemaker", "reference minus the coupling", "coupled", cheap::from_ref( "heart_pacemaker", cut2_c( "SA_c", "SA_p", "SA_p", "SA_c" ) ) );
  // every other level: one tonic cell with the appendix's strongest current (a lone cell reads every channel); 380 / 600 add nothing a 450 / 900 do not show
  for ( double ie : { 900.0 } )
    for ( const auto& t : levels_c )
      if ( t != "heart_pacemaker" )
        CHEAP_ROW( t, "appendix: one tonic cell, I_e " + std::to_string( static_cast< int >( ie ) ), cheap::tonic( t, ie ) );

  // ---- the reference minus the taught mechanism ----
  // heart_autonomic: two release branches, each driven by the cue or the error
  CHEAP_ROW_V( "heart_autonomic", "reference minus the vagal branch (NA > VG cut)", "vagal", cheap::from_ref( "heart_autonomic", cut_c( "NA", "VG" ) ) );
  CHEAP_ROW_V( "heart_autonomic", "reference minus the sympathetic branch (IML > SG cut)", "sympathetic", cheap::from_ref( "heart_autonomic", cut_c( "IML", "SG" ) ) );
  CHEAP_ROW_V( "heart_autonomic", "reference with no input reaching the sympathetic cells", "sympathetic", cheap::from_ref( "heart_autonomic", cheap::all( cut_c( "CC", "IML" ), cut_c( "LOW", "IML" ), cut_c( "HIGH", "IML" ) ) ) );
  CHEAP_ROW_V( "heart_autonomic", "reference with no input reaching the vagal cells", "vagal", cheap::from_ref( "heart_autonomic", cheap::all( cut_c( "CC", "NA" ), cut_c( "LOW", "NA" ), cut_c( "HIGH", "NA" ) ) ) );
  CHEAP_ROW_V( "heart_autonomic", "both outputs read one pool", "same cells", cheap::from_ref( "heart_autonomic", []( json& c ) {
    for ( auto& o : c[ "outputs" ] )
      if ( o[ "name" ] == "sympathetic" ) o[ "neurons" ] = json::array( { { { "pop", "VG" } } } );
  } ) );
  // baroreflex: both limbs behind one relay
  CHEAP_ROW_V( "baroreflex", "reference minus the vagal limb (NTS > NA cut)", "vagal limb", cheap::from_ref( "baroreflex", cut_c( "NTS", "NA" ) ) );
  CHEAP_ROW_V( "baroreflex", "reference minus the inhibitory relay (CVLM > RVLM cut)", "sympathetic limb", cheap::from_ref( "baroreflex", cut_c( "CVLM", "RVLM" ) ) );
  CHEAP_ROW_V( "baroreflex", "reference minus the depressor relay (NTS > CVLM cut)", "sympathetic limb", cheap::from_ref( "baroreflex", cut_c( "NTS", "CVLM" ) ) );
  CHEAP_ROW_V( "baroreflex", "reference minus the afferent (AFF > NTS cut)", "limb", cheap::from_ref( "baroreflex", cut_c( "AFF", "NTS" ) ) );
  CHEAP_ROW_V( "baroreflex", "CVLM > RVLM made excitatory: high pressure would raise sympathetic output", "sympathetic limb", cheap::from_ref( "baroreflex", []( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == "CVLM" && p[ "post" ] == "RVLM" ) p[ "synapse" ][ "weight" ] = 140.0;
  } ) );
  // breathing: a coupled rhythm group, reached by the chemo input
  CHEAP_ROW_V( "breathing", "reference minus the recurrent coupling (PBC > PBC cut)", "rhythm", cheap::from_ref( "breathing", cut_c( "PBC", "PBC" ) ) );
  CHEAP_ROW_V( "breathing", "reference minus the chemo input onto the rhythm group (RTN > PBC cut)", "chemoreceptor", cheap::from_ref( "breathing", cut_c( "RTN", "PBC" ) ) );
  // (not a row: PBC > PHR cut still leaves the burst network driving the phrenic pool by disinhibition, PBC -| EXP -| PHR, which is a rhythm path and passes)
  // pupil: two pathways onto two pools
  CHEAP_ROW_V( "pupil", "reference minus the parasympathetic chain (PON > EW cut)", "parasympathetic", cheap::from_ref( "pupil", cut_c( "PON", "EW" ) ) );
  CHEAP_ROW_V( "pupil", "reference minus the sympathetic chain (HYP > IML cut)", "sympathetic", cheap::from_ref( "pupil", cut_c( "HYP", "IML" ) ) );
  CHEAP_ROW_V( "pupil", "sphincter and dilator read one pool", "same cells", cheap::from_ref( "pupil", []( json& c ) {
    for ( auto& o : c[ "outputs" ] )
      if ( o[ "name" ] == "dilator" ) o[ "neurons" ] = json::array( { { { "pop", "CG" } } } );
  } ) );
  // peristalsis: ascending excitatory and descending inhibitory chains
  CHEAP_ROW_V( "peristalsis", "reference minus the descending chain (DIN > IMN cut)", "descending", cheap::from_ref( "peristalsis", cut_c( "DIN", "IMN" ) ) );
  CHEAP_ROW_V( "peristalsis", "reference minus the descending chain (S > DIN cut)", "descending", cheap::from_ref( "peristalsis", cut_c( "S", "DIN" ) ) );
  CHEAP_ROW_V( "peristalsis", "reference minus the ascending chain (AIN > EMN cut)", "ascending", cheap::from_ref( "peristalsis", cut_c( "AIN", "EMN" ) ) );
  CHEAP_ROW_V( "peristalsis", "ascending chain only: the simple build, no no0..7 outputs", "descending", cheap::from_ref( "peristalsis", []( json& c ) {
    json keep = json::array();
    for ( auto& o : c[ "outputs" ] )
      if ( o[ "name" ].get< std::string >().rfind( "no", 0 ) != 0 ) keep.push_back( o );
    c[ "outputs" ] = keep;
  } ) );
  // fight_or_flight: one command fanning out
  CHEAP_ROW_V( "fight_or_flight", "reference minus the command (HYP > RVLM cut)", "heart", cheap::from_ref( "fight_or_flight", cut_c( "HYP", "RVLM" ) ) );
  CHEAP_ROW_V( "fight_or_flight", "reference minus the pupil branch (RVLM > SCG cut)", "pupil", cheap::from_ref( "fight_or_flight", cut_c( "RVLM", "SCG" ) ) );
  CHEAP_ROW_V( "fight_or_flight", "reference minus the breathing branch (HYP > VRG cut)", "breathing", cheap::from_ref( "fight_or_flight", cut_c( "HYP", "VRG" ) ) );
  CHEAP_ROW_V( "fight_or_flight", "reference minus the vessel branch (RVLM > SV cut)", "vessels", cheap::from_ref( "fight_or_flight", cut_c( "RVLM", "SV" ) ) );
  // boss: every component of the chapter
  CHEAP_ROW_V( "boss_autonomic_exam", "reference minus the baroreflex's inhibitory relay (CVLM > RVLM cut)", "baroreflex", cheap::from_ref( "boss_autonomic_exam", cut_c( "CVLM", "RVLM" ) ) );
  CHEAP_ROW_V( "boss_autonomic_exam", "reference minus the chemoreflex (CHEMO > VRG cut)", "chemoreflex", cheap::from_ref( "boss_autonomic_exam", cut_c( "CHEMO", "VRG" ) ) );
  CHEAP_ROW_V( "boss_autonomic_exam", "reference minus central command (CC > RVLM and CC > CVLM cut)", "central command", cheap::from_ref( "boss_autonomic_exam", cut2_c( "CC", "RVLM", "CC", "CVLM" ) ) );
  CHEAP_ROW_V( "boss_autonomic_exam", "reference minus the pupil branch (HYP > SCG cut)", "pupil", cheap::from_ref( "boss_autonomic_exam", cut_c( "HYP", "SCG" ) ) );
  CHEAP_ROW_V( "boss_autonomic_exam", "reference minus the vagal limb (NTS > NA cut)", "vagal limb", cheap::from_ref( "boss_autonomic_exam", cut_c( "NTS", "NA" ) ) );
}

// ---- cheat-proofing (full review, P0-3)
CHEAP_ROW_V( "heart_autonomic", "a recorded beat train on a parrot cell, nothing wired", "stimulus", fixture( "heart_autonomic_tape" ) );
CHEAP_ROW( "heart_pacemaker", "two uncoupled pacemaker cells (coupling 0.001 nS), I_e -4", fixture( "heart_pacemaker_uncoupled" ) );
