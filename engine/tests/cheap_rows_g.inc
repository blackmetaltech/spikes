// cheap-circuit rows (see tasks_cheap_test.cpp for the format). Owner: the Atlas wing of 0.6, iteration A1: the development-only locked-wiring level atlas_probe.
// docs/REQ-0.4.md rule 3: an empty bench, a single cell, the reference with every weight 0, then the cheats of a locked level: every cell type excitatory, the inhibitory type's sign right but its
// scale at an extreme, and tampering with the locked wiring (a circuit that would behave like the reference, but is not the given one). A row with a word pins that the verdict names the cause.
// (A circuit that is not the given one is rejected whatever it does, so the structural rows below also prove that the editable parameters alone decide the level.)
{
  auto group_set = []( const std::string& id, const std::string& key, json v ) -> cheap::Mut {
    return [ = ]( json& c ) {
      bool hit = false;
      for ( auto& g : c[ "groups" ] )
        if ( g[ "id" ] == id ) { g[ key ] = v; hit = true; }
      CHECK_MSG( hit, "edit matched no group " + id );
    };
  };
  auto scale_edges = []( const std::string& pre, const std::string& post, double factor ) -> cheap::Mut {
    return [ = ]( json& c ) {
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == pre && p[ "post" ] == post )
          for ( auto& e : p[ "edges" ] ) e[ 2 ] = e[ 2 ].get< double >() * factor;
    };
  };
  const char* t = "atlas_probe";
  CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
  CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
  CHEAP_ROW( t, "reference with every weight 0", cheap::zero_weights( t ) );
  CHEAP_ROW_V( t, "every cell type excitatory", "inhibit", cheap::from_ref( t, group_set( "I", "sign", 1 ) ) );
  CHEAP_ROW_V( t, "inhibitory sign, scale at the lowest allowed", "weak", cheap::from_ref( t, group_set( "I", "scale", 0.25 ) ) );
  CHEAP_ROW_V( t, "inhibitory sign, scale at the highest allowed", "strong", cheap::from_ref( t, group_set( "I", "scale", 2.0 ) ) );
  CHEAP_ROW_V( t, "tampered: the lowest scale with the stored weights multiplied to match the reference", "wiring",
               cheap::from_ref( t, cheap::both( group_set( "I", "scale", 0.25 ), scale_edges( "I", "M", 0.98 / 0.25 ) ) ) );
  CHEAP_ROW_V( t, "tampered: a stronger excitation edit that the free values could not make", "wiring", cheap::from_ref( t, scale_edges( "P", "M", 1.5 ) ) );
  CHEAP_ROW_V( t, "tampered: a fixed type made free and set", "cell type", cheap::from_ref( t, cheap::both( group_set( "P", "scale", 0.5 ), group_set( "I", "scale", 1.0 ) ) ) );
  CHEAP_ROW_V( t, "the free scale pushed past its range", "range", cheap::from_ref( t, group_set( "I", "scale", 3.0 ) ) );
}
