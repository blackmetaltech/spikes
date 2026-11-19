// cheap-circuit rows (see tasks_cheap_test.cpp for the format). Owner: the Bestiary chapters A of 0.5: worm (w_latch, w_switch, worm_touch), leech (l_burst, l_rebound,
// leech_heart) and aplysia (a_depress, a_facilitate, aplysia_gill). docs/REQ-0.4.md rule 3: an empty bench, a single cell, a tonic cell with every weight 0, the reference with
// every weight 0, the reference with the taught mechanism removed, "max every weight", plus level-specific cheats. A row with a word also pins that the verdict names the
// missing mechanism. No rows for the diagnosis cases (dx_worm, dx_leech, dx_aplysia): they are decided by the submitted answer (tasks_bestiary_a_test.cpp).
{
  // like cheap::cut, but also scales the weights of `edges` projections (the worm reference is wired with edges); `factor` 0 cuts
  auto cut_d = []( const std::string& pre, const std::string& post, double factor = 0.0 ) -> cheap::Mut {
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
  // every projection pre > post gets this synapse model (extra keys replace the old ones)
  auto syn_d = []( const std::string& pre, const std::string& post, const std::string& model, json extra = json::object() ) -> cheap::Mut {
    return [ = ]( json& c ) {
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == pre && p[ "post" ] == post )
        {
          const double w = p[ "synapse" ][ "weight" ].get< double >(), d = p[ "synapse" ].value( "delay", 2.0 );
          p[ "synapse" ] = { { "synapse_model", model }, { "weight", w }, { "delay", d } };
          for ( auto it = extra.begin(); it != extra.end(); ++it ) p[ "synapse" ][ it.key() ] = it.value();
        }
    };
  };
  // "every weight at its maximum" for circuits whose conductance-cell weights are already near the ceiling (|w| <= 500 nS): those go to 500, current-cell weights to 4000, sensors as max_all
  auto max_d = []( json& c ) {
    auto cond = [ & ]( const std::string& pop ) {
      for ( auto& p : c[ "populations" ] )
        if ( p[ "id" ] == pop ) return p[ "model" ].get< std::string >().find( "cond" ) != std::string::npos;
      return false;
    };
    for ( auto& p : c[ "projections" ] )
      if ( p.contains( "synapse" ) && p[ "synapse" ].contains( "weight" ) )
      {
        const double w = p[ "synapse" ][ "weight" ].get< double >(), m = cond( p[ "post" ].get< std::string >() ) ? 500.0 : 4000.0;
        p[ "synapse" ][ "weight" ] = w < 0 ? -m : m;
      }
    for ( auto& i : c[ "inputs" ] ) i[ "weight" ] = ( i[ "weight" ].get< double >() < 0 ? -1.0 : 1.0 ) * ( i.value( "kind", "" ) == "poisson" ? 4000.0 : 8000.0 );
  };
  // a modulation row on a synapse that is no longer a depressing one acts on its weight instead
  auto mod_on_weight = []( json& c ) {
    for ( auto& r : c[ "modulation" ] ) { r = { { "modulator", "NE" }, { "projection", r[ "projection" ] }, { "param", "weight" }, { "scale_per_unit", 4.0 } }; }
  };
  auto no_modulation = []( json& c ) { c.erase( "modulation" ); };

  const std::vector< std::string > levels_d = { "w_latch", "w_switch", "worm_touch", "l_burst", "l_rebound", "leech_heart", "a_depress", "a_facilitate", "aplysia_gill" };
  for ( const auto& t : levels_d )
  {
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW( t, "one iaf cell, first-connect weights", cheap::single_cell( t, "iaf_psc_alpha" ) );
    CHEAP_ROW( t, "one tonic cell, I_e 450, every weight 0", cheap::tonic( t, 450 ) );
    CHEAP_ROW( t, "reference with every weight 0", cheap::zero_weights( t ) );
    // not for w_latch, w_switch and l_rebound: their circuits are robust to strength (a latch, a switch or a rebound with every weight at its maximum still works), so saturation is a
    // correct answer there and no cheap circuit; their cheap circuits are the missing-mechanism rows below
    if ( t != "w_latch" && t != "w_switch" && t != "l_rebound" )
      CHEAP_ROW( t, "reference with every weight at the maximum", t == "leech_heart" ? cheap::from_ref( t, max_d ) : cheap::max_weights( t ) );
  }

  // ---- worm ----
  CHEAP_ROW_V( "w_latch", "reference minus the memory (B > A cut): a relay", "positive feedback", cheap::from_ref( "w_latch", cut_d( "B", "A" ) ) );
  CHEAP_ROW_V( "w_latch", "reference minus the stop (S > A and S > B cut)", "stop", cheap::from_ref( "w_latch", cheap::both( cut_d( "S", "A" ), cut_d( "S", "B" ) ) ) );
  CHEAP_ROW( "w_latch", "stop made excitatory (the stop cell excites the pair)", cheap::from_ref( "w_latch", cheap::both( cut_d( "S", "A", -1 ), cut_d( "S", "B", -1 ) ) ) );
  CHEAP_ROW( "w_latch", "a touch-driven cell that fires on its own (A tonic 800 pA)", cheap::from_ref( "w_latch", cheap::set_params( "A", { { "I_e", 800.0 } } ) ) );

  CHEAP_ROW_V( "w_switch", "reference minus R > F", "one way", cheap::from_ref( "w_switch", cut_d( "R", "F" ) ) );
  CHEAP_ROW_V( "w_switch", "reference minus F > R", "one way", cheap::from_ref( "w_switch", cut_d( "F", "R" ) ) );
  CHEAP_ROW_V( "w_switch", "reference minus the mutual inhibition", "inhibit each other", cheap::from_ref( "w_switch", cheap::both( cut_d( "R", "F" ), cut_d( "F", "R" ) ) ) );
  CHEAP_ROW( "w_switch", "mutual inhibition made weak (1/20)", cheap::from_ref( "w_switch", cheap::both( cut_d( "R", "F", 0.05 ), cut_d( "F", "R", 0.05 ) ) ) );
  CHEAP_ROW( "w_switch", "both pushes on the same cell, both motors read from it", [] {
    cheap::Bench b = cheap::channels( "w_switch", { "R" }, { "R" } );
    b.pop( "R" );
    return b.c;
  }() );

  CHEAP_ROW_V( "worm_touch", "reference minus AVA > AVB", "inhibit", cheap::from_ref( "worm_touch", cut_d( "AVA", "AVB" ) ) );
  CHEAP_ROW_V( "worm_touch", "reference minus AVB > AVA", "inhibit", cheap::from_ref( "worm_touch", cut_d( "AVB", "AVA" ) ) );
  CHEAP_ROW_V( "worm_touch", "reference minus the mutual inhibition", "inhibit each other", cheap::from_ref( "worm_touch", cheap::both( cut_d( "AVA", "AVB" ), cut_d( "AVB", "AVA" ) ) ) );
  CHEAP_ROW_V( "worm_touch", "reference minus the backward pair's coupling (AVA > AVA)", "backward command does not hold", cheap::from_ref( "worm_touch", cut_d( "AVA", "AVA" ) ) );
  CHEAP_ROW_V( "worm_touch", "reference minus the forward pair's coupling (AVB > AVB)", "forward command does not hold", cheap::from_ref( "worm_touch", cut_d( "AVB", "AVB" ) ) );
  CHEAP_ROW( "worm_touch", "reference minus the head pathway (AVD > AVA)", cheap::from_ref( "worm_touch", cut_d( "AVD", "AVA" ) ) );
  CHEAP_ROW( "worm_touch", "reference minus the tail pathway (PLM > PVC)", cheap::from_ref( "worm_touch", cut_d( "PLM", "PVC" ) ) );
  CHEAP_ROW( "worm_touch", "mutual inhibition far too weak (1/20)", cheap::from_ref( "worm_touch", cheap::both( cut_d( "AVA", "AVB", 0.05 ), cut_d( "AVB", "AVA", 0.05 ) ) ) );
  CHEAP_ROW( "worm_touch", "both touches drive both motors (no command layer): head and tail onto MA and MB", [] {
    cheap::Bench b = cheap::channels( "worm_touch", { "M" }, { "M" } );
    b.pop( "M", "iaf_psc_alpha", 2 );
    return b.c;
  }() );

  // ---- leech ----
  CHEAP_ROW_V( "l_burst", "reference with a plain integrate-and-fire cell", "plain", cheap::from_ref( "l_burst", cheap::set_model( "HN", "iaf_psc_alpha" ) ) );
  CHEAP_ROW( "l_burst", "regular-spiking Izhikevich cell (no burst)", cheap::from_ref( "l_burst", cheap::set_params( "HN", { { "c", -65.0 }, { "d", 8.0 } } ) ) );
  CHEAP_ROW( "l_burst", "fast-spiking Izhikevich cell", cheap::from_ref( "l_burst", cheap::set_params( "HN", { { "a", 0.1 }, { "c", -65.0 }, { "d", 2.0 } } ) ) );
  CHEAP_ROW( "l_burst", "drive far too weak (the cell never fires)", cheap::from_ref( "l_burst", cheap::scale_input( "drive", 0.1 ) ) );

  CHEAP_ROW_V( "l_rebound", "reference with an integrate-and-fire target", "integrate-and-fire", cheap::from_ref( "l_rebound", cheap::set_model( "T", "iaf_psc_alpha" ) ) );
  CHEAP_ROW_V( "l_rebound", "reference with a weakly adapting target (a 4, the catalogue default)", "sub-threshold", cheap::from_ref( "l_rebound", cheap::set_params( "T", { { "a", 4.0 } } ) ) );
  CHEAP_ROW_V( "l_rebound", "reference minus the inhibition (S > T cut)", "inhibition", cheap::from_ref( "l_rebound", cut_d( "S", "T" ) ) );
  CHEAP_ROW( "l_rebound", "target that fires on its own (bias 900 pA)", cheap::from_ref( "l_rebound", cheap::set_params( "T", { { "I_e", 900.0 } } ) ) );
  CHEAP_ROW( "l_rebound", "no bias: silent, never rebounds", cheap::from_ref( "l_rebound", cheap::set_params( "T", { { "I_e", 0.0 } } ) ) );

  CHEAP_ROW_V( "leech_heart", "reference minus HN_L > HN_R", "one way", cheap::from_ref( "leech_heart", cut_d( "HN_L", "HN_R" ) ) );
  CHEAP_ROW_V( "leech_heart", "reference minus the mutual inhibition", "inhibit each other", cheap::from_ref( "leech_heart", cheap::both( cut_d( "HN_L", "HN_R" ), cut_d( "HN_R", "HN_L" ) ) ) );
  CHEAP_ROW_V( "leech_heart", "reference with plain integrate-and-fire interneurons", "plain", cheap::from_ref( "leech_heart", cheap::both( cheap::set_model( "HN_L", "iaf_psc_alpha" ), cheap::set_model( "HN_R", "iaf_psc_alpha" ) ) ) );
  CHEAP_ROW( "leech_heart", "the m_half_centre rhythm: far too fast (adaptation time constant 100 ms)", cheap::from_ref( "leech_heart", cheap::both( cheap::set_params( "HN_L", { { "tau_w", 100.0 } } ), cheap::set_params( "HN_R", { { "tau_w", 100.0 } } ) ) ) );
  CHEAP_ROW( "leech_heart", "far too slow (adaptation time constant 20 s)", cheap::from_ref( "leech_heart", cheap::both( cheap::set_params( "HN_L", { { "tau_w", 20000.0 } } ), cheap::set_params( "HN_R", { { "tau_w", 20000.0 } } ) ) ) );
  CHEAP_ROW( "leech_heart", "no adaptation (b 0): one side wins", cheap::from_ref( "leech_heart", cheap::both( cheap::set_params( "HN_L", { { "b", 0.0 } } ), cheap::set_params( "HN_R", { { "b", 0.0 } } ) ) ) );
  CHEAP_ROW( "leech_heart", "motor neurons not inhibited by the interneurons", cheap::from_ref( "leech_heart", cheap::both( cut_d( "HN_L", "HE_L" ), cut_d( "HN_R", "HE_R" ) ) ) );

  // ---- aplysia ----
  CHEAP_ROW_V( "a_depress", "reference with a static synapse", "static", cheap::from_ref( "a_depress", syn_d( "SN", "MN", "static_synapse" ) ) );
  CHEAP_ROW( "a_depress", "depression with an instant recovery (tau_rec 5 ms)", cheap::from_ref( "a_depress", [ & ]( json& c ) { c[ "projections" ][ 0 ][ "synapse" ][ "tau_rec" ] = 5.0; } ) );
  CHEAP_ROW( "a_depress", "synapse too weak (1/20)", cheap::from_ref( "a_depress", cut_d( "SN", "MN", 0.05 ) ) );

  CHEAP_ROW_V( "a_facilitate", "reference minus the modulation row", "modulator does not act", cheap::from_ref( "a_facilitate", no_modulation ) );
  CHEAP_ROW_V( "a_facilitate", "reference minus TSN > FN (the shock reaches nothing)", "tail", cheap::from_ref( "a_facilitate", cut_d( "TSN", "FN" ) ) );
  CHEAP_ROW( "a_facilitate", "touch synapse already strong (x10): nothing left to facilitate", cheap::from_ref( "a_facilitate", cut_d( "SN", "MN", 10.0 ) ) );
  CHEAP_ROW( "a_facilitate", "modulation far too weak (x0.05)", cheap::from_ref( "a_facilitate", [ & ]( json& c ) { c[ "modulation" ][ 0 ][ "scale_per_unit" ] = 0.05; } ) );

  CHEAP_ROW_V( "aplysia_gill", "reference with a static synapse (no habituation)", "static", cheap::from_ref( "aplysia_gill", cheap::both( syn_d( "SN", "MN", "static_synapse" ), mod_on_weight ) ) );
  CHEAP_ROW_V( "aplysia_gill", "reference minus the modulation row", "modulator does not act", cheap::from_ref( "aplysia_gill", no_modulation ) );
  CHEAP_ROW_V( "aplysia_gill", "reference minus TSN > FN", "tail", cheap::from_ref( "aplysia_gill", cut_d( "TSN", "FN" ) ) );
  CHEAP_ROW( "aplysia_gill", "depression that never recovers (tau_rec 60 s)", cheap::from_ref( "aplysia_gill", [ & ]( json& c ) { c[ "projections" ][ 0 ][ "synapse" ][ "tau_rec" ] = 60000.0; } ) );
  CHEAP_ROW( "aplysia_gill", "depression that is gone at once (tau_rec 5 ms)", cheap::from_ref( "aplysia_gill", [ & ]( json& c ) { c[ "projections" ][ 0 ][ "synapse" ][ "tau_rec" ] = 5.0; } ) );
  CHEAP_ROW( "aplysia_gill", "modulation far too weak (0.02)", cheap::from_ref( "aplysia_gill", [ & ]( json& c ) { c[ "modulation" ][ 0 ][ "gain" ] = 0.02; } ) );
}
