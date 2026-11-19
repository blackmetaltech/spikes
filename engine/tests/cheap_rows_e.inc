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

  const std::vector< std::string > levels_e = { "c_ring", "c_shift", "fly_compass", "k_sparse", "k_depress", "fly_mb" };
  // every weight 0; a dopamine-gated synapse cannot have weight 0 (the loader refuses it), so it gets the smallest weight it accepts
  auto zero_e = []( json& c ) {
    for ( auto& p : c[ "projections" ] )
    {
      const bool stdp = p.contains( "synapse" ) && p[ "synapse" ].value( "synapse_model", "" ).find( "stdp" ) != std::string::npos;
      if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) ) p[ "synapse" ][ "weight" ] = stdp ? 0.001 : 0.0;
      if ( p.contains( "edges" ) ) for ( auto& e : p[ "edges" ] ) e[ 2 ] = 0.0;
    }
    for ( auto& i : c[ "inputs" ] ) i[ "weight" ] = 0.0;
  };
  for ( const auto& t : levels_e )
  {
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW( t, "one iaf cell, first-connect weights", cheap::single_cell( t, "iaf_psc_alpha" ) );
    CHEAP_ROW( t, "one tonic cell, I_e 450, every weight 0", cheap::tonic( t, 450 ) );
    CHEAP_ROW( t, "reference with every weight 0", cheap::from_ref( t, zero_e ) );
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
  // ---- fly mushroom body ----
  // every projection pre > post gets this synapse model; stdp parameters are dropped (the dopamine source key goes with them)
  auto static_syn = []( const std::string& pre, const std::string& post ) -> cheap::Mut {
    return [ = ]( json& c ) {
      bool hit = false;
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == pre && p[ "post" ] == post )
        {
          hit = true;
          const double w = p[ "synapse" ][ "weight" ].get< double >();
          p[ "synapse" ] = { { "synapse_model", "static_synapse" }, { "weight", w }, { "delay", 2.0 } };
          p.erase( "dopamine_source" );
        }
      CHECK_MSG( hit, "edit matched no projection " + pre + ">" + post );
    };
  };
  auto set_syn = []( const std::string& pre, const std::string& post, json kv ) -> cheap::Mut {
    return [ = ]( json& c ) {
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == pre && p[ "post" ] == post )
          for ( auto it = kv.begin(); it != kv.end(); ++it ) p[ "synapse" ][ it.key() ] = it.value();
    };
  };
  // all-to-all PN > KC (every Kenyon cell listens to every projection neuron)
  auto dense_pn = []( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == "PN" && p[ "post" ] == "KC" ) { p.erase( "indegree" ); p[ "rule" ] = "all_to_all"; }
  };
  auto pn_indegree = []( int k ) -> cheap::Mut {
    return [ = ]( json& c ) {
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == "PN" && p[ "post" ] == "KC" ) p[ "indegree" ] = k;
    };
  };

  CHEAP_ROW_V( "k_sparse", "reference minus the APL loop (APL > KC cut)", "inhibits them all", cheap::from_ref( "k_sparse", cut_e( "APL", "KC" ) ) );
  CHEAP_ROW_V( "k_sparse", "reference minus KC > APL", "inhibits them all", cheap::from_ref( "k_sparse", cut_e( "KC", "APL" ) ) );
  CHEAP_ROW_V( "k_sparse", "every Kenyon cell listens to every projection neuron", "few of them", cheap::from_ref( "k_sparse", dense_pn ) );
  CHEAP_ROW( "k_sparse", "each Kenyon cell listens to 8 projection neurons and the weights are doubled", cheap::from_ref( "k_sparse", cheap::both( pn_indegree( 8 ), cut_e( "PN", "KC", 2.0 ) ) ) );
  CHEAP_ROW( "k_sparse", "PN > KC far too weak (1/4)", cheap::from_ref( "k_sparse", cut_e( "PN", "KC", 0.25 ) ) );
  CHEAP_ROW( "k_sparse", "PN > KC far too strong (x2.5)", cheap::from_ref( "k_sparse", cut_e( "PN", "KC", 2.5 ) ) );
  CHEAP_ROW( "k_sparse", "Kenyon cells tonic by themselves (bias 500 pA)", cheap::from_ref( "k_sparse", cheap::set_params( "KC", { { "I_e", 500.0 } } ) ) );
  CHEAP_ROW( "k_sparse", "a handful of Kenyon cells: 3 cells", cheap::from_ref( "k_sparse", [ & ]( json& c ) { cheap::find_pop( c, "KC" )->operator[]( "n" ) = 3; { auto& pp = cheap::find_pop( c, "KC" )->operator[]( "pos" ); pp.erase( pp.begin() + 3, pp.end() ); } for ( auto& p : c[ "projections" ] ) if ( p[ "post" ] == "KC" && p.contains( "indegree" ) ) p[ "indegree" ] = 3; } ) );

  CHEAP_ROW_V( "k_depress", "reference with a static synapse (cannot learn)", "ordinary one", cheap::from_ref( "k_depress", cheap::both( static_syn( "KCA", "MBON" ), static_syn( "KCB", "MBON" ) ) ) );
  CHEAP_ROW( "k_depress", "dopamine cells never driven (punish cut)", cheap::from_ref( "k_depress", cheap::scale_input( "punish", 0.0 ) ) );
  CHEAP_ROW_V( "k_depress", "dopamine source is the odour cells themselves", "dopamine cells", cheap::from_ref( "k_depress", [ & ]( json& c ) { for ( auto& p : c[ "projections" ] ) p[ "dopamine_source" ] = p[ "pre" ]; } ) );
  CHEAP_ROW( "k_depress", "learning amplitude zero (A 0)", cheap::from_ref( "k_depress", cheap::both( set_syn( "KCA", "MBON", { { "A_plus", 0.0 }, { "A_minus", 0.0 } } ), set_syn( "KCB", "MBON", { { "A_plus", 0.0 }, { "A_minus", 0.0 } } ) ) ) );
  CHEAP_ROW( "k_depress", "potentiation instead of depression (signs swapped)", cheap::from_ref( "k_depress", cheap::both( set_syn( "KCA", "MBON", { { "A_plus", 0.6 }, { "A_minus", -0.6 } } ), set_syn( "KCB", "MBON", { { "A_plus", 0.6 }, { "A_minus", -0.6 } } ) ) ) );
  CHEAP_ROW( "k_depress", "one group of Kenyon cells for both odours (no specificity)", cheap::from_ref( "k_depress", [ & ]( json& c ) {
    for ( auto& i : c[ "inputs" ] ) if ( i[ "name" ] == "odor_b" ) i[ "targets" ] = json::array( { { { "pop", "KCA" } } } );
  } ) );

  CHEAP_ROW_V( "fly_mb", "reference minus the APL loop", "inhibits them all", cheap::from_ref( "fly_mb", cut_e( "APL", "KC" ) ) );
  CHEAP_ROW_V( "fly_mb", "reference minus the punishment pathway (TSN > DAN cut)", "punishment does not reach", cheap::from_ref( "fly_mb", cut_e( "TSN", "DAN" ) ) );
  CHEAP_ROW_V( "fly_mb", "reference with a static KC > MBON synapse", "ordinary one", cheap::from_ref( "fly_mb", static_syn( "KC", "MBON" ) ) );
  CHEAP_ROW_V( "fly_mb", "every Kenyon cell listens to every projection neuron", "few of them", cheap::from_ref( "fly_mb", dense_pn ) );
  CHEAP_ROW( "fly_mb", "learning amplitude zero", cheap::from_ref( "fly_mb", set_syn( "KC", "MBON", { { "A_plus", 0.0 }, { "A_minus", 0.0 } } ) ) );
  CHEAP_ROW( "fly_mb", "potentiation instead of depression", cheap::from_ref( "fly_mb", set_syn( "KC", "MBON", { { "A_plus", 1000.0 }, { "A_minus", -1000.0 } } ) ) );
  CHEAP_ROW( "fly_mb", "avoidance cells without bias and without drive", cheap::from_ref( "fly_mb", cheap::both( set_syn( "KC", "MBON", { { "weight", 0.001 } } ), cheap::set_params( "MBON", { { "I_e", 0.0 } } ) ) ) );
}
