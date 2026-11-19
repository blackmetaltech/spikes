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
}
