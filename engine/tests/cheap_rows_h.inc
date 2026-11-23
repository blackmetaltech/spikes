// cheap-circuit rows (see tasks_cheap_test.cpp for the format). Owner: the Atlas wing of 0.6, iteration A2: the worm chapter's level a_path (locked touch circuit, five interneuron
// classes with a free scale from 0 to 1). docs/REQ-0.4.md rule 3: an empty bench, a single cell, the reference with every weight 0, then every wrong way to set the five classes, each with the word
// its verdict must carry, and tampering with the given wiring. (a_predict and a_motif are bets decided by a committed answer, level_kind diagnosis: a row here would need a reference that passes without an answer; tasks_atlas_worm_test holds their cases.) A row passes when the level does not pass on seeds 1 and 2 and says why.
{
  // set the scale of the five classes; a class not named keeps the value in `rest`
  auto classes = []( std::map< std::string, double > set, double rest ) -> cheap::Mut {
    return [ = ]( json& c ) {
      for ( auto& g : c[ "groups" ] )
      {
        const std::string id = g[ "id" ];
        g[ "scale" ] = set.count( id ) ? set.at( id ) : rest;
      }
    };
  };
  auto scale_edges = []( const std::string& pre, double factor ) -> cheap::Mut {
    return [ = ]( json& c ) {
      for ( auto& p : c[ "projections" ] )
        if ( p[ "pre" ] == pre )
          for ( auto& e : p[ "edges" ] ) e[ 2 ] = e[ 2 ].get< double >() * factor;
    };
  };
  auto group_free = []( const std::string& id, json v ) -> cheap::Mut {
    return [ = ]( json& c ) { for ( auto& g : c[ "groups" ] ) if ( g[ "id" ] == id ) g[ "free" ] = v; };
  };
  {
    const char* t = "a_path";
    CHEAP_ROW( t, "empty bench", cheap::empty( t ) );
    CHEAP_ROW( t, "one cell, first-connect weights", cheap::single_cell( t ) );
    CHEAP_ROW( t, "reference with every weight 0", cheap::zero_weights( t ) );
    CHEAP_ROW_V( t, "nothing silenced (the whole circuit)", "nothing is silenced", cheap::from_ref( t, classes( {}, 1.0 ) ) );
    CHEAP_ROW_V( t, "everything silenced", "no route is left", cheap::from_ref( t, classes( {}, 0.0 ) ) );
    CHEAP_ROW_V( t, "two routes kept (AVB and AVD)", "more than one route", cheap::from_ref( t, classes( { { "AVB", 1.0 }, { "AVD", 1.0 } }, 0.0 ) ) );
    CHEAP_ROW_V( t, "all three routes kept (AVB, AVD, AVE)", "more than one route", cheap::from_ref( t, classes( { { "AVB", 1.0 }, { "AVD", 1.0 }, { "AVE", 1.0 } }, 0.0 ) ) );
    CHEAP_ROW_V( t, "a class that is on no shortest route (AVA)", "not on any shortest route", cheap::from_ref( t, classes( { { "AVA", 1.0 } }, 0.0 ) ) );
    CHEAP_ROW_V( t, "a route plus a class off every route (AVD and PVC)", "not on any shortest route", cheap::from_ref( t, classes( { { "AVD", 1.0 }, { "PVC", 1.0 } }, 0.0 ) ) );
    CHEAP_ROW_V( t, "the route class only half on", "partly", cheap::from_ref( t, classes( { { "AVD", 0.5 } }, 0.0 ) ) );
    CHEAP_ROW_V( t, "a silenced class only almost silenced", "partly", cheap::from_ref( t, classes( { { "AVD", 1.0 }, { "AVB", 0.05 } }, 0.0 ) ) );
    CHEAP_ROW_V( t, "tampered: the kept route's weights raised", "wiring", cheap::from_ref( t, scale_edges( "AVD", 2.0 ) ) );
    CHEAP_ROW_V( t, "tampered: a class made free to sign", "cell type", cheap::from_ref( t, group_free( "AVD", { "scale", "sign" } ) ) );
  }
}
