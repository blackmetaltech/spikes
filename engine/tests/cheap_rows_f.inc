// cheap-circuit rows (see tasks_cheap_test.cpp for the format). Owner: the Theory wing of 0.6: dynamics (d_class, d_phasic, d_switch), rates (r_oscillate, r_choose, r_meanfield),
// memory (h_complete, h_two, h_learn) and tuning (t_tuned, t_sharpen, t_decode). docs/REQ-0.4.md rule 3: an empty bench, a single cell, a tonic cell with every weight 0, the reference
// with every weight 0, the reference with the taught mechanism removed, plus level-specific cheats. A row with a word also pins that the verdict names the missing mechanism.
{
  // the weights of `edges` projections as well (the memory references are wired with edges); factor 0 cuts
  auto cut_f = []( const std::string& pre, const std::string& post, double factor = 0.0 ) -> cheap::Mut {
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
  auto zero_f = []( const std::string& t ) { return cheap::zero_weights( t ); };
  (void)cut_f;

  // ---- dynamics
  for ( const char* t : { "d_class", "d_phasic", "d_switch" } )
  {
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW_V( t, "one integrate-and-fire cell, first-connect weights", "recovery variable", cheap::single_cell( t, "iaf_psc_alpha" ) );
    CHEAP_ROW( t, "one tonic cell, I_e 450, every weight 0", cheap::tonic( t, 450 ) );
    CHEAP_ROW( t, "reference with every weight 0", zero_f( t ) );
    CHEAP_ROW_V( t, "reference with an integrate-and-fire cell", "recovery variable", cheap::from_ref( t, cheap::set_model( "N", "iaf_psc_alpha" ) ) );
  }
  CHEAP_ROW_V( "d_class", "regular-spiking cell (the catalogue default: class I)", "bottleneck", cheap::from_ref( "d_class", cheap::set_params( "N", { { "a", 0.02 }, { "b", 0.2 }, { "c", -65.0 }, { "d", 8.0 } } ) ) );
  CHEAP_ROW_V( "d_class", "saddle-node cell (b -0.1)", "bottleneck", cheap::from_ref( "d_class", cheap::set_params( "N", { { "a", 0.02 }, { "b", -0.1 }, { "c", -55.0 }, { "d", 6.0 } } ) ) );
  CHEAP_ROW( "d_class", "adaptive exponential cell, NEST defaults", cheap::from_ref( "d_class", cheap::set_model( "N", "aeif_cond_exp" ) ) );
  CHEAP_ROW( "d_class", "resonator that fires on its own (I_e 5)", cheap::from_ref( "d_class", cheap::set_params( "N", { { "I_e", 5.0 } } ) ) );
  CHEAP_ROW( "d_class", "resonator with a sluggish recovery (a 0.01)", cheap::from_ref( "d_class", cheap::set_params( "N", { { "a", 0.01 } } ) ) );
  CHEAP_ROW( "d_class", "sensor not wired to the cell", cheap::from_ref( "d_class", cheap::scale_input( "current", 0.0 ) ) );

  CHEAP_ROW_V( "d_phasic", "regular-spiking cell (b 0.2): the step is below its threshold", "answer", cheap::from_ref( "d_phasic", cheap::set_params( "N", { { "b", 0.2 } } ) ) );
  CHEAP_ROW_V( "d_phasic", "step four times too big: the cell circles", "circles", cheap::from_ref( "d_phasic", cheap::scale_input( "current", 4.0 ) ) );
  CHEAP_ROW_V( "d_phasic", "step far too small (x0.2)", "answer", cheap::from_ref( "d_phasic", cheap::scale_input( "current", 0.2 ) ) );
  CHEAP_ROW( "d_phasic", "tonic cell (b 0.2, c -50, d 0) with the step of the reference", cheap::from_ref( "d_phasic", cheap::set_params( "N", { { "b", 0.2 }, { "c", -50.0 }, { "d", 0.0 } } ) ) );
  CHEAP_ROW( "d_phasic", "adaptive exponential cell, NEST defaults, weight 100", cheap::from_ref( "d_phasic", cheap::both( cheap::set_model( "N", "aeif_cond_exp" ), cheap::scale_input( "current", 200.0 ) ) ) );

  CHEAP_ROW_V( "d_switch", "regular-spiking cell: the reset lands on the rest side of the border", "set", cheap::from_ref( "d_switch", cheap::set_params( "N", { { "c", -65.0 }, { "d", 8.0 } } ) ) );
  CHEAP_ROW_V( "d_switch", "reference without the holding current", "connected", cheap::from_ref( "d_switch", cheap::scale_input( "hold", 0.0 ) ) );
  CHEAP_ROW_V( "d_switch", "holding current twice too high: the cell fires on its own", "on its own", cheap::from_ref( "d_switch", cheap::scale_input( "hold", 2.0 ) ) );
  CHEAP_ROW_V( "d_switch", "'set' not wired to the cell", "connected", cheap::from_ref( "d_switch", cheap::scale_input( "set", 0.0 ) ) );
  CHEAP_ROW( "d_switch", "'reset' pulse far too weak (x0.05)", cheap::from_ref( "d_switch", cheap::scale_input( "reset", 0.05 ) ) );
  CHEAP_ROW( "d_switch", "reset adds a lot to the recovery variable (d 8): firing cannot persist", cheap::from_ref( "d_switch", cheap::set_params( "N", { { "d", 8.0 } } ) ) );
  CHEAP_ROW( "d_switch", "holding current too low (x0.2): 'set' cannot start firing", cheap::from_ref( "d_switch", cheap::scale_input( "hold", 0.2 ) ) );

  // ---- rates ----
  // (the rate levels' sensors drive rate units only, so the one-cell rows use the rate unit the level opens on; a spiking cell would not even load)
  for ( const char* t : { "r_oscillate", "r_choose" } )
  {
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one rate unit, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW( t, "reference with every weight 0", zero_f( t ) );
  }
  CHEAP_ROW_V( "r_oscillate", "reference minus I > E (nothing shuts the excitation down)", "negative", cheap::from_ref( "r_oscillate", cut_f( "I", "E" ) ) );
  CHEAP_ROW_V( "r_oscillate", "reference minus E > E (no self-excitation)", "excite itself", cheap::from_ref( "r_oscillate", cut_f( "E", "E" ) ) );
  CHEAP_ROW_V( "r_oscillate", "reference minus E > I", "inhibitory one", cheap::from_ref( "r_oscillate", cut_f( "E", "I" ) ) );
  CHEAP_ROW( "r_oscillate", "loop far too weak (self-excitation x0.3)", cheap::from_ref( "r_oscillate", cut_f( "E", "E", 0.3 ) ) );
  CHEAP_ROW( "r_oscillate", "inhibition far too weak (x0.2)", cheap::from_ref( "r_oscillate", cut_f( "I", "E", 0.2 ) ) );
  CHEAP_ROW( "r_oscillate", "drive five times too strong: the rate sits at its ceiling", cheap::from_ref( "r_oscillate", cheap::scale_input( "drive", 5.0 ) ) );
  CHEAP_ROW( "r_oscillate", "no drive wired", cheap::from_ref( "r_oscillate", cheap::scale_input( "drive", 0.0 ) ) );
  CHEAP_ROW( "r_oscillate", "inhibition as fast as excitation (I tau 10) and weak self-excitation", cheap::from_ref( "r_oscillate", cheap::both( cheap::set_params( "I", { { "tau", 10.0 } } ), cut_f( "E", "E", 0.5 ) ) ) );
  CHEAP_ROW_V( "r_oscillate", "both motors read the excitatory unit", "same unit", [] {
    json c = cheap::reference( "r_oscillate" );
    for ( auto& o : c[ "outputs" ] ) o[ "neurons" ] = json::array( { { { "pop", "E" } } } );
    return c;
  }() );

  CHEAP_ROW_V( "r_choose", "reference minus the self-excitation (A > A, B > B)", "excite itself", cheap::from_ref( "r_choose", cheap::both( cut_f( "A", "A" ), cut_f( "B", "B" ) ) ) );
  CHEAP_ROW_V( "r_choose", "reference minus the mutual inhibition", "inhibit each other", cheap::from_ref( "r_choose", cheap::both( cut_f( "A", "B" ), cut_f( "B", "A" ) ) ) );
  CHEAP_ROW_V( "r_choose", "reference minus B > A (one-way inhibition)", "inhibit each other", cheap::from_ref( "r_choose", cut_f( "B", "A" ) ) );
  CHEAP_ROW( "r_choose", "self-excitation far too weak (x0.3): nothing is amplified", cheap::from_ref( "r_choose", cheap::both( cut_f( "A", "A", 0.3 ), cut_f( "B", "B", 0.3 ) ) ) );
  CHEAP_ROW( "r_choose", "self-excitation and inhibition far too strong: the first winner never gives way", cheap::from_ref( "r_choose", cheap::all( cut_f( "A", "A", 3.0 ), cut_f( "B", "B", 3.0 ), cut_f( "A", "B", 3.0 ), cut_f( "B", "A", 3.0 ) ) ) );
  CHEAP_ROW( "r_choose", "inhibition far too weak (x0.1): both win", cheap::from_ref( "r_choose", cheap::both( cut_f( "A", "B", 0.1 ), cut_f( "B", "A", 0.1 ) ) ) );
  CHEAP_ROW( "r_choose", "b_in not wired", cheap::from_ref( "r_choose", cheap::scale_input( "b_in", 0.0 ) ) );
  CHEAP_ROW_V( "r_choose", "both motors read A", "same unit", [] {
    json c = cheap::reference( "r_choose" );
    for ( auto& o : c[ "outputs" ] ) o[ "neurons" ] = json::array( { { { "pop", "A" } } } );
    return c;
  }() );

  CHEAP_ROW( "r_meanfield", "empty bench", cheap::empty( "r_meanfield" ) );
  CHEAP_ROW( "r_meanfield", "one spiking cell and one siegert unit, first-connect weights, no connections", [] {
    cheap::Bench b = cheap::channels( "r_meanfield", { "N1" }, { "N1" } );
    b.pop( "N1", "iaf_psc_delta" ).pop( "S1", "siegert_neuron" );
    b.c[ "inputs" ][ 1 ][ "targets" ] = json::array( { { { "pop", "S1" } } } );
    b.c[ "outputs" ][ 1 ] = { { "name", "mf" }, { "kind", "rate_value" }, { "neurons", json::array( { { { "pop", "S1" } } } ) } };
    return b.c;
  }() );
  CHEAP_ROW( "r_meanfield", "reference with every weight 0", zero_f( "r_meanfield" ) );
  CHEAP_ROW_V( "r_meanfield", "network without inhibition (I > E, I > I cut)", "loop", cheap::from_ref( "r_meanfield", cheap::both( cut_f( "I", "E" ), cut_f( "I", "I" ) ) ) );
  CHEAP_ROW_V( "r_meanfield", "prediction without inhibition (SI > SE cut)", "prediction has no", cheap::from_ref( "r_meanfield", cheap::both( cut_f( "SI", "SE" ), cut_f( "SI", "SI" ) ) ) );
  CHEAP_ROW( "r_meanfield", "prediction with the wrong K for the excitatory recurrence (SE > SE and SE > SI: 15 instead of 60)", cheap::from_ref( "r_meanfield", [ & ]( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == "SE" ) p[ "synapse" ][ "K" ] = 15;
  } ) );
  CHEAP_ROW( "r_meanfield", "prediction with a quarter of the inhibition (SI K 5 instead of 20)", cheap::from_ref( "r_meanfield", [ & ]( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == "SI" ) p[ "synapse" ][ "K" ] = 5;
  } ) );
  CHEAP_ROW( "r_meanfield", "prediction driven at half the rate", cheap::from_ref( "r_meanfield", cheap::scale_input( "ext_mf", 0.5 ) ) );
  CHEAP_ROW( "r_meanfield", "the siegert drive not wired", cheap::from_ref( "r_meanfield", cheap::scale_input( "ext_mf", 0.0 ) ) );
  CHEAP_ROW( "r_meanfield", "network driven at half the rate (ext weight x0.5)", cheap::from_ref( "r_meanfield", cheap::scale_input( "ext", 0.5 ) ) );
  CHEAP_ROW( "r_meanfield", "network connections far weaker than the prediction's (E > E, E > I x0.2)", cheap::from_ref( "r_meanfield", cheap::both( cut_f( "E", "E", 0.2 ), cut_f( "E", "I", 0.2 ) ) ) );
}
