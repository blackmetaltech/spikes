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
  CHEAP_ROW_V( "d_switch", "holding current five times too high: the cell fires on its own", "rest quietly", cheap::from_ref( "d_switch", cheap::scale_input( "hold", 5.0 ) ) );
  CHEAP_ROW_V( "d_switch", "'set' not wired to the cell", "connected", cheap::from_ref( "d_switch", cheap::scale_input( "set", 0.0 ) ) );
  CHEAP_ROW( "d_switch", "'reset' pulse far too weak (x0.005: the low-rate firing state is barely stable, so a pulse of a tenth of the reference already switches it off)", cheap::from_ref( "d_switch", cheap::scale_input( "reset", 0.005 ) ) );
  CHEAP_ROW_V( "d_switch", "the on-state fires far too fast (the old reference: b 0.2 with a holding weight of 2.5, about 255 Hz)", "unrealistic", cheap::from_ref( "d_switch", cheap::both( cheap::set_params( "N", { { "b", 0.2 } } ), cheap::scale_input( "hold", 5.0 ) ) ) );
  CHEAP_ROW_V( "d_switch", "the reset far above the border (c -40): firing at several hundred Hz", "unrealistic", cheap::from_ref( "d_switch", cheap::both( cheap::set_params( "N", { { "b", 0.2 }, { "c", -40.0 }, { "d", 0.5 } } ), cheap::scale_input( "hold", 5.0 ) ) ) );
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
  CHEAP_ROW_V( "r_choose", "feed-forward bias: loops of 0.01 and a_in 4.0 against b_in 2.7 (the first side always leads, so a static bias looks like a memory)", "bias",
    cheap::from_ref( "r_choose", cheap::all( cheap::all( cut_f( "A", "A", 0.008 ), cut_f( "B", "B", 0.008 ), cut_f( "A", "B", 0.01 ), cut_f( "B", "A", 0.01 ) ), cheap::scale_input( "a_in", 4.0 ), cheap::scale_input( "b_in", 2.7 ) ) ) );
  CHEAP_ROW_V( "r_oscillate", "loop through the inhibitory unit four times too strong: the excitation is pinned just above the drive", "too strong", cheap::from_ref( "r_oscillate", cut_f( "E", "I", 4.0 ) ) );
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
  CHEAP_ROW_V( "r_meanfield", "prediction with K = 1 on every projection (J tuned by hand to match the rates)", "network's K", cheap::from_ref( "r_meanfield", [ & ]( json& c ) {
    for ( auto& p : c[ "projections" ] )
      if ( p[ "pre" ] == "SE" || p[ "pre" ] == "SI" ) p[ "synapse" ][ "K" ] = 1;
  } ) );
  CHEAP_ROW( "r_meanfield", "prediction driven at half the rate", cheap::from_ref( "r_meanfield", cheap::scale_input( "ext_mf", 0.5 ) ) );
  CHEAP_ROW( "r_meanfield", "the siegert drive not wired", cheap::from_ref( "r_meanfield", cheap::scale_input( "ext_mf", 0.0 ) ) );
  CHEAP_ROW( "r_meanfield", "network driven at half the rate (ext weight x0.5)", cheap::from_ref( "r_meanfield", cheap::scale_input( "ext", 0.5 ) ) );
  CHEAP_ROW( "r_meanfield", "network connections far weaker than the prediction's (E > E, E > I x0.2)", cheap::from_ref( "r_meanfield", cheap::both( cut_f( "E", "E", 0.2 ), cut_f( "E", "I", 0.2 ) ) ) );

  // ---- memory ----
  for ( const char* t : { "h_complete", "h_two", "h_learn" } )
  {
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW( t, "one tonic cell, I_e 450, every weight 0", cheap::tonic( t, 450 ) );
    if ( std::string( t ) != "h_learn" ) CHEAP_ROW( t, "reference with every weight 0", zero_f( t ) );   // (a plastic synapse of weight 0 is refused at load: excitatory only)
  }
  CHEAP_ROW( "h_learn", "reference with the sensors cut (every input weight 0)", cheap::from_ref( "h_learn", cheap::scale_all_inputs( 0.0 ) ) );
  // every projection of the reference gets this change
  auto all_proj = [ & ]( std::function< void( json& ) > f ) -> cheap::Mut { return [ = ]( json& c ) { for ( auto& p : c[ "projections" ] ) f( p ); }; };
  auto add_target = [ & ]( const std::string& input, const std::string& pop ) -> cheap::Mut {
    return [ = ]( json& c ) { for ( auto& i : c[ "inputs" ] ) if ( i[ "name" ] == input ) i[ "targets" ].push_back( { { "pop", pop } } ); };
  };
  CHEAP_ROW_V( "h_complete", "reference minus the recurrent excitation", "excite each other", cheap::from_ref( "h_complete", cheap::all( cut_f( "C", "C" ), cut_f( "C", "R" ), cut_f( "R", "C" ), cut_f( "R", "R" ) ) ) );
  CHEAP_ROW_V( "h_complete", "the cue reaches every cell of the pattern", "every cell", cheap::from_ref( "h_complete", add_target( "cue", "R" ) ) );
  CHEAP_ROW_V( "h_complete", "asymmetric weights (R > C four times C > R)", "symmetric", cheap::from_ref( "h_complete", cut_f( "R", "C", 4.0 ) ) );
  CHEAP_ROW( "h_complete", "recurrent weights far too weak (x0.25)", cheap::from_ref( "h_complete", all_proj( []( json& p ) { p[ "synapse" ][ "weight" ] = p[ "synapse" ][ "weight" ].get< double >() * 0.25; } ) ) );
  CHEAP_ROW( "h_complete", "every cell biased to fire on its own (I_e 450)", cheap::from_ref( "h_complete", cheap::both( cheap::set_params( "C", { { "I_e", 450.0 } } ), cheap::set_params( "R", { { "I_e", 450.0 } } ) ) ) );
  CHEAP_ROW_V( "h_complete", "'memory' reads only the three cued cells", "several cells", [] {
    json c = cheap::reference( "h_complete" );
    c[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "C" } } } );
    return c;
  }() );
  CHEAP_ROW_V( "h_complete", "a pattern of one cell", "several cells", [] {
    cheap::Bench b = cheap::channels( "h_complete", { "N1" }, { "N1" } );
    b.pop( "N1", "iaf_psc_alpha" ).proj( "N1", "N1", 400.0 );
    return b.c;
  }() );

  CHEAP_ROW_V( "h_two", "reference minus the inhibition between the patterns", "separates", cheap::from_ref( "h_two", cheap::both( cut_f( "A", "B" ), cut_f( "B", "A" ) ) ) );
  CHEAP_ROW_V( "h_two", "reference minus B > A (one-way inhibition)", "separates", cheap::from_ref( "h_two", cut_f( "B", "A" ) ) );
  CHEAP_ROW_V( "h_two", "inhibition asymmetric (A > B x0.3)", "symmetric", cheap::from_ref( "h_two", cut_f( "A", "B", 0.3 ) ) );
  CHEAP_ROW( "h_two", "inhibition too strong: the first pattern can never be overwritten (x2)", cheap::from_ref( "h_two", cheap::both( cut_f( "A", "B", 2.0 ), cut_f( "B", "A", 2.0 ) ) ) );
  CHEAP_ROW_V( "h_two", "reference minus the self-excitation", "excite each other", cheap::from_ref( "h_two", cheap::both( cut_f( "A", "A" ), cut_f( "B", "B" ) ) ) );
  CHEAP_ROW_V( "h_two", "'a' and 'b' read the same cells", "same cells", [] {
    json c = cheap::reference( "h_two" );
    c[ "outputs" ][ 1 ][ "neurons" ] = c[ "outputs" ][ 0 ][ "neurons" ];
    return c;
  }() );
  CHEAP_ROW( "h_two", "inhibition far too weak (x0.1): both patterns light together", cheap::from_ref( "h_two", cheap::both( cut_f( "A", "B", 0.1 ), cut_f( "B", "A", 0.1 ) ) ) );

  CHEAP_ROW_V( "h_learn", "static weights of 80 (nothing can be learned)", "fixed", cheap::from_ref( "h_learn", all_proj( []( json& p ) { p[ "synapse" ] = { { "synapse_model", "static_synapse" }, { "weight", 80.0 }, { "delay", 2.0 } }; } ) ) );
  CHEAP_ROW( "h_learn", "static weights of 400 (built in, recalls from the start)", cheap::from_ref( "h_learn", all_proj( []( json& p ) { p[ "synapse" ] = { { "synapse_model", "static_synapse" }, { "weight", 400.0 }, { "delay", 2.0 } }; } ) ) );
  CHEAP_ROW( "h_learn", "plastic but strong from the start (weight 400)", cheap::from_ref( "h_learn", all_proj( []( json& p ) { p[ "synapse" ][ "weight" ] = 400.0; } ) ) );
  CHEAP_ROW( "h_learn", "learning far too slow (lambda 0.0001)", cheap::from_ref( "h_learn", all_proj( []( json& p ) { p[ "synapse" ][ "lambda" ] = 0.0001; } ) ) );
  CHEAP_ROW( "h_learn", "learning so fast that the cue itself writes the pattern (lambda 0.03)", cheap::from_ref( "h_learn", all_proj( []( json& p ) { p[ "synapse" ][ "lambda" ] = 0.03; } ) ) );
  CHEAP_ROW( "h_learn", "no learning at all (lambda 0)", cheap::from_ref( "h_learn", all_proj( []( json& p ) { p[ "synapse" ][ "lambda" ] = 0.0; } ) ) );
  CHEAP_ROW( "h_learn", "weight ceiling just above the start (Wmax 100)", cheap::from_ref( "h_learn", all_proj( []( json& p ) { p[ "synapse" ][ "Wmax" ] = 100.0; } ) ) );
  CHEAP_ROW_V( "h_learn", "'show' reaches only the cued cells", "every cell of the pattern", cheap::from_ref( "h_learn", [ & ]( json& c ) {
    for ( auto& i : c[ "inputs" ] ) if ( i[ "name" ] == "show" ) i[ "targets" ] = json::array( { { { "pop", "C" } } } );
  } ) );
  CHEAP_ROW_V( "h_learn", "asymmetric start (R > C four times C > R)", "symmetric", cheap::from_ref( "h_learn", cut_f( "R", "C", 4.0 ) ) );
  CHEAP_ROW_V( "h_learn", "the cue reaches every cell", "every cell", cheap::from_ref( "h_learn", add_target( "cue", "R" ) ) );

  // ---- tuning ----
  for ( const char* t : { "t_tuned", "t_sharpen", "t_decode" } )
  {
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW( t, "one tonic cell, I_e 450, every weight 0", cheap::tonic( t, 450 ) );
    CHEAP_ROW( t, "reference with every weight 0", zero_f( t ) );
  }
  auto add_proj = [ & ]( const std::string& pre, const std::string& post, double w ) -> cheap::Mut {
    return [ = ]( json& c ) { c[ "projections" ].push_back( { { "pre", pre }, { "post", post }, { "rule", "all_to_all" }, { "synapse", { { "synapse_model", "static_synapse" }, { "weight", w }, { "delay", 2.0 } } } } ); };
  };
  // the A > T projection of the reference as explicit edges with a ring mapping k -> f(k)
  auto remap = [ & ]( std::function< int( int ) > f, double w, int n ) -> cheap::Mut {
    return [ = ]( json& c ) {
      json edges = json::array();
      for ( int k = 0; k < n; ++k ) edges.push_back( { k, f( k ), w, 2.0 } );
      c[ "projections" ][ 0 ] = { { "pre", "A" }, { "post", "T" }, { "edges", edges }, { "synapse", { { "synapse_model", "static_synapse" } } } };
    };
  };
  CHEAP_ROW_V( "t_tuned", "the cell listens to every afferent", "every afferent", cheap::from_ref( "t_tuned", cheap::both( add_proj( "A1", "R", 1500.0 ), add_proj( "A3", "R", 1500.0 ) ) ) );
  CHEAP_ROW( "t_tuned", "the cell listens to the wrong group (A1, the small angles)", cheap::from_ref( "t_tuned", [ & ]( json& c ) { c[ "projections" ][ 0 ][ "pre" ] = "A1"; } ) );
  CHEAP_ROW( "t_tuned", "the connection far too weak (x0.05)", cheap::from_ref( "t_tuned", cut_f( "A2", "R", 0.05 ) ) );
  CHEAP_ROW_V( "t_tuned", "the cell hears no afferent", "none of the afferents", cheap::from_ref( "t_tuned", cut_f( "A2", "R" ) ) );
  CHEAP_ROW_V( "t_tuned", "'resp' reads the afferent", "afferents themselves", [] {
    json c = cheap::reference( "t_tuned" );
    c[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "A2" } } } );
    return c;
  }() );

  CHEAP_ROW_V( "t_sharpen", "reference minus all inhibition", "inhibit one another", cheap::from_ref( "t_sharpen", [ & ]( json& c ) { c[ "projections" ].erase( 1 ); c[ "projections" ].erase( 1 ); } ) );
  CHEAP_ROW( "t_sharpen", "every afferent listened to by every cell (no one-to-one wiring)", cheap::from_ref( "t_sharpen", [ & ]( json& c ) { c[ "projections" ][ 0 ][ "rule" ] = "all_to_all"; c[ "projections" ][ 0 ][ "synapse" ][ "weight" ] = 400.0; } ) );
  CHEAP_ROW_V( "t_sharpen", "'resp' reads the afferents", "afferents themselves", [] {
    json c = cheap::reference( "t_sharpen" );
    c[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "A" } } } );
    return c;
  }() );

  CHEAP_ROW_V( "t_decode", "'heading' reads the afferents", "afferents themselves", [] {
    json c = cheap::reference( "t_decode" );
    c[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "A" } } } );
    return c;
  }() );
  CHEAP_ROW( "t_decode", "cells wired to the afferents three places round the ring (rotated readout)", cheap::from_ref( "t_decode", remap( []( int k ) { return ( k + 3 ) % 12; }, 1500.0, 12 ) ) );
  CHEAP_ROW( "t_decode", "cells wired in reverse order (mirrored readout)", cheap::from_ref( "t_decode", remap( []( int k ) { return 11 - k; }, 1500.0, 12 ) ) );
  CHEAP_ROW( "t_decode", "every cell listens to every afferent (no tuning left)", cheap::from_ref( "t_decode", [ & ]( json& c ) { c[ "projections" ][ 0 ][ "rule" ] = "all_to_all"; c[ "projections" ][ 0 ][ "synapse" ][ "weight" ] = 200.0; } ) );
  CHEAP_ROW( "t_decode", "afferent connections far too weak (x0.05)", cheap::from_ref( "t_decode", cut_f( "A", "T", 0.05 ) ) );
  CHEAP_ROW_V( "t_decode", "the readout lists only half of the cells", "too few cells", [] {
    json c = cheap::reference( "t_decode" );
    c[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "T" }, { "idx", json::array( { 0, 1, 2, 3, 4, 5 } ) } } } );
    return c;
  }() );
}

// ---- cheat-proofing (full review, P0-3)
CHEAP_ROW( "r_choose", "feed-forward gain units and a self-excitation of 0.001", fixture( "r_choose_feedforward" ) );
CHEAP_ROW( "h_learn", "the cue wired at 0.001", fixture( "h_learn_cue_token" ) );
CHEAP_ROW( "t_sharpen", "a sharp input tuning and a lateral inhibition of 0.01", fixture( "t_sharpen_sharp_input" ) );
