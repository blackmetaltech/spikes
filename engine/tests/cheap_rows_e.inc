// cheap-circuit rows (see tasks_cheap_test.cpp for the format). Owner: the Bestiary chapters B of 0.5: fly compass (c_ring, c_shift, fly_compass), fly mushroom body (k_sparse, k_depress,
// fly_mb) and crab pyloric rhythm (s_pacemaker, s_phase, crab_pyloric). docs/REQ-0.4.md rule 3: an empty bench, a single cell, a tonic cell with every weight 0, the reference with every
// weight 0, the reference with the taught mechanism removed, "max every weight", plus level-specific cheats. A row with a word also pins that the verdict names the missing mechanism.
// No rows for the diagnosis cases (dx_compass, dx_mb_apl, dx_mb_dan, dx_crab): they are decided by the submitted answer (tasks_bestiary_b_test.cpp).
{
  // like cheap::cut, but also scales the weights of `edges` projections (the compass is wired with edges); `factor` 0 cuts
  auto cut_e = []( const std::string& pre, const std::string& post, double factor = 0.0 ) -> cheap::Mut {
    return [ = ]( json& c ) {
      bool hit = false;
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == pre && p[ "post" ] == post )
        {
          hit = true;
          if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) ) p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * factor;
          if ( p.contains( "edges" ) ) for ( auto& e : p[ "edges" ] ) e[ 2 ] = e[ 2 ].get< double >() * factor;
        }
      CHECK_MSG( hit, "edit matched no projection " + pre + ">" + post );
    };
  };
  // the weights of one projection are moved by `off` cells (an edges projection: post index + off, wrapping at n cells)
  auto reshift = []( const std::string& pre, const std::string& post, int off, int n ) -> cheap::Mut {
    return [ = ]( json& c ) {
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == pre && p[ "post" ] == post )
          for ( auto& e : p[ "edges" ] ) e[ 1 ] = ( ( e[ 0 ].get< int >() + off ) % n + n ) % n;
    };
  };

  const std::vector< std::string > levels_e = { "c_ring", "c_shift", "fly_compass" };
  for ( const auto& t : levels_e )
  {
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW( t, "one iaf cell, first-connect weights", cheap::single_cell( t, "iaf_psc_alpha" ) );
    CHEAP_ROW( t, "one tonic cell, I_e 450, every weight 0", cheap::tonic( t, 450 ) );
    CHEAP_ROW( t, "reference with every weight 0", cheap::zero_weights( t ) );
  }

  // ---- fly compass ----
  CHEAP_ROW_V( "c_ring", "reference minus the ring excitation (EPG > EPG cut)", "excite each other", cheap::from_ref( "c_ring", cut_e( "EPG", "EPG" ) ) );
  CHEAP_ROW_V( "c_ring", "reference minus the global inhibition (DELTA > EPG cut)", "inhibits the ring", cheap::from_ref( "c_ring", cut_e( "DELTA", "EPG" ) ) );
  CHEAP_ROW_V( "c_ring", "reference with the ring read as a plain rate", "plain rate", cheap::from_ref( "c_ring", []( json& c ) { c[ "outputs" ][ 0 ] = { { "name", "heading" }, { "kind", "rate" }, { "neurons", json::array( { { { "pop", "EPG" } } } ) } }; } ) );
  CHEAP_ROW( "c_ring", "ring excitation far too weak (1/10)", cheap::from_ref( "c_ring", cut_e( "EPG", "EPG", 0.1 ) ) );
  CHEAP_ROW( "c_ring", "ring excitation far too strong (x4)", cheap::from_ref( "c_ring", cut_e( "EPG", "EPG", 4.0 ) ) );
  CHEAP_ROW( "c_ring", "fast synapses, no bias (a ring that cannot hold)", cheap::from_ref( "c_ring", cheap::set_params( "EPG", { { "I_e", 0.0 }, { "tau_syn_ex", 2.0 } } ) ) );
  CHEAP_ROW( "c_ring", "ring cells tonic by themselves (bias 600 pA)", cheap::from_ref( "c_ring", cheap::set_params( "EPG", { { "I_e", 600.0 } } ) ) );

  CHEAP_ROW_V( "c_shift", "reference minus the left shifters' output (PENL > EPG cut)", "excite the ring", cheap::from_ref( "c_shift", cut_e( "PENL", "EPG" ) ) );
  CHEAP_ROW_V( "c_shift", "reference minus the right shifters' output (PENR > EPG cut)", "excite the ring", cheap::from_ref( "c_shift", cut_e( "PENR", "EPG" ) ) );
  CHEAP_ROW_V( "c_shift", "reference minus the ring input of the shifters", "listen to the ring", cheap::from_ref( "c_shift", cheap::both( cut_e( "EPG", "PENL" ), cut_e( "EPG", "PENR" ) ) ) );
  CHEAP_ROW_V( "c_shift", "reference with both turn sensors on one group", "same cells", cheap::from_ref( "c_shift", [ & ]( json& c ) { c[ "inputs" ][ c[ "inputs" ].size() - 1 ][ "targets" ] = c[ "inputs" ][ c[ "inputs" ].size() - 2 ][ "targets" ]; } ) );
  CHEAP_ROW_V( "c_shift", "reference with the turn sensors written straight onto the ring", "straight into the ring", cheap::from_ref( "c_shift", [ & ]( json& c ) {
    for ( auto& i : c[ "inputs" ] ) if ( i[ "name" ] == "turn_left" || i[ "name" ] == "turn_right" ) i[ "targets" ] = json::array( { { { "pop", "EPG" } } } );
  } ) );
  CHEAP_ROW( "c_shift", "reference with the shifts swapped (PENR one step left, PENL one step right)", cheap::from_ref( "c_shift", cheap::both( reshift( "PENR", "EPG", -1, 16 ), reshift( "PENL", "EPG", 1, 16 ) ) ) );
  CHEAP_ROW( "c_shift", "reference with no shift at all (offset 0)", cheap::from_ref( "c_shift", cheap::both( reshift( "PENR", "EPG", 0, 16 ), reshift( "PENL", "EPG", 0, 16 ) ) ) );
  CHEAP_ROW( "c_shift", "left shifters twice as strong as the right ones (drift)", cheap::from_ref( "c_shift", cut_e( "PENL", "EPG", 3.0 ) ) );
  CHEAP_ROW( "c_shift", "shifters far too weak (1/10)", cheap::from_ref( "c_shift", cheap::both( cut_e( "PENL", "EPG", 0.1 ), cut_e( "PENR", "EPG", 0.1 ) ) ) );

  CHEAP_ROW_V( "fly_compass", "reference minus the shifters (ring alone: holds, cannot turn)", "excite the ring", cheap::from_ref( "fly_compass", cheap::both( cut_e( "PENL", "EPG" ), cut_e( "PENR", "EPG" ) ) ) );
  CHEAP_ROW( "fly_compass", "right shifters only (left cut)", cheap::from_ref( "fly_compass", cut_e( "PENL", "EPG" ) ) );
  CHEAP_ROW( "fly_compass", "left shifters only (right cut)", cheap::from_ref( "fly_compass", cut_e( "PENR", "EPG" ) ) );
  CHEAP_ROW_V( "fly_compass", "reference minus the ring excitation", "bump is gone", cheap::from_ref( "fly_compass", cut_e( "EPG", "EPG" ) ) );
  CHEAP_ROW_V( "fly_compass", "reference minus the global inhibition", "inhibits the ring", cheap::from_ref( "fly_compass", cut_e( "DELTA", "EPG" ) ) );
  CHEAP_ROW( "fly_compass", "reference with the shifts swapped", cheap::from_ref( "fly_compass", cheap::both( reshift( "PENR", "EPG", -1, 16 ), reshift( "PENL", "EPG", 1, 16 ) ) ) );
  CHEAP_ROW( "fly_compass", "shifters far too weak (1/10)", cheap::from_ref( "fly_compass", cheap::both( cut_e( "PENL", "EPG", 0.1 ), cut_e( "PENR", "EPG", 0.1 ) ) ) );
  CHEAP_ROW( "fly_compass", "shifters far too strong (x30)", cheap::from_ref( "fly_compass", cheap::both( cut_e( "PENL", "EPG", 30.0 ), cut_e( "PENR", "EPG", 30.0 ) ) ) );
  CHEAP_ROW( "fly_compass", "turn sensors cut (no angular velocity)", cheap::from_ref( "fly_compass", cheap::both( cheap::scale_input( "turn_left", 0.0 ), cheap::scale_input( "turn_right", 0.0 ) ) ) );
}
