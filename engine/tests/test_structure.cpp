// Engine::structure() and the path / loop helpers.
#include <cmath>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static json circuit()
{
  json c = json::parse( R"({"schema_version":1,"name":"structure","resolution_ms":0.1,"seed":3,
    "populations":[
      {"id":"S","model":"iaf_psc_alpha","n":2},
      {"id":"M","model":"iaf_psc_alpha","n":3,"params":{"I_e":100.0,"tau_m":12.5}},
      {"id":"R","model":"iaf_psc_alpha","n":2},
      {"id":"X","model":"iaf_psc_alpha","n":1}],
    "projections":[
      {"pre":"S","post":"M","rule":"all_to_all","synapse":{"weight":20.0,"delay":1.0}},
      {"pre":"M","post":"R","rule":"fixed_indegree","indegree":1,"synapse":{"weight":10.0,"delay":1.0}},
      {"pre":"R","post":"M","edges":[[0,0,-30.0,1.0],[1,2,-10.0,1.0]]},
      {"pre":"X","post":"X","rule":"all_to_all","allow_autapses":true,"synapse":{"weight":0.0,"delay":1.0}}],
    "inputs":[{"name":"drive","kind":"dc","targets":[{"pop":"S"}],"weight":1.0,"value":0.0}],
    "outputs":[{"name":"mot","kind":"rate","neurons":[{"pop":"M"}]}]})" );
  return c;
}

int main()
{
  Engine e;
  CHECK( e.structure().populations.empty() );
  CHECK_MSG( e.load( circuit().dump() ), e.error() );
  if ( e.structure().populations.empty() ) { std::printf( "load: %s\n", e.error().c_str() ); return 1; }
  Structure s = e.structure();
  CHECK( s.populations.size() == 4 and s.projections.size() == 4 );
  CHECK( s.populations[ 1 ].id == "M" and s.populations[ 1 ].n == 3 and s.populations[ 1 ].first_index == 2 );
  CHECK( s.populations[ 1 ].model == "iaf_psc_alpha" and s.populations[ 1 ].params.at( "I_e" ) == 100.0 );
  CHECK( s.populations[ 0 ].sign == 1 and s.populations[ 1 ].sign == 1 and s.populations[ 2 ].sign == -1 );
  CHECK( s.populations[ 3 ].sign == 0 );  // only weight-0 outgoing connections
  const auto& p0 = s.projections[ 0 ];
  CHECK( p0.index == 0 and p0.pre_pop == "S" and p0.post_pop == "M" and p0.rule == "all_to_all" and p0.synapse_model == "static_synapse" );
  CHECK( p0.n_edges == 6 and p0.sign == 1 and std::fabs( p0.w_mean - 20 ) < 1e-9 and p0.indegree_max == 2 and std::fabs( p0.indegree_mean - 2 ) < 1e-9 );
  CHECK( s.projections[ 1 ].rule == "fixed_indegree" and s.projections[ 1 ].n_edges == 2 );
  const auto& p2 = s.projections[ 2 ];
  CHECK( p2.rule == "edges" and p2.n_edges == 2 and p2.sign == -1 and p2.w_min == -30 and p2.w_max == -10 and std::fabs( p2.w_mean + 20 ) < 1e-9 );
  CHECK( s.projections[ 3 ].sign == 0 and s.projections[ 3 ].n_edges == 1 );
  CHECK( s.inputs.size() == 1 and s.inputs[ 0 ].name == "drive" and s.inputs[ 0 ].pops == std::vector< std::string >{ "S" } );
  CHECK( s.outputs.size() == 1 and s.outputs[ 0 ].pops == std::vector< std::string >{ "M" } and s.outputs[ 0 ].neurons.size() == 3 );

  // paths and loops
  CHECK( e.has_path( "S", "R" ) and e.has_path( "S", "R", 2 ) and not e.has_path( "S", "R", 1 ) );
  CHECK( e.has_path( "S", "R", 0, 1 ) and not e.has_path( "S", "M", 1, -1 ) and e.has_path( "R", "M", 1, -1 ) );
  CHECK( not e.has_path( "R", "S" ) and not e.has_path( "S", "nope" ) );
  CHECK( e.has_path( "M", "M", 2 ) and not e.has_path( "M", "M", 1 ) );  // M -> R -> M
  CHECK( e.has_loop( "M", 2, -1 ) and e.has_loop( "R", 2, -1 ) and e.has_loop( "M", 2, 1 ) );
  CHECK( not e.has_loop( "S" ) and not e.has_loop( "X" ) );  // weight-0 self-projection is not a loop
  CHECK( e.has_loop_via( "M", "R", 2 ) and not e.has_loop_via( "M", "R", 1 ) and not e.has_loop_via( "M", "S" ) );
  CHECK( not e.has_loop_via( "M", "R", 2, -1 ) );  // the M -> R leg is excitatory

  // live edits: lesion the inhibitory projection (weight 0) -> loop disappears; sign flips are seen
  CHECK( e.set_weight( 2, 0, 0.0 ) and e.set_weight( 2, 1, 0.0 ) );
  CHECK( not e.has_loop( "M", 2, -1 ) and e.structure().projections[ 2 ].sign == 0 );
  CHECK( e.set_weight( 2, 0, 5.0 ) );
  CHECK( e.structure().projections[ 2 ].sign == 1 and e.has_loop( "M", 2, 1 ) and not e.has_loop( "M", 2, -1 ) );
  // survives stepping
  e.step( 5.0 );
  CHECK( e.structure().projections.size() == 4 );

  // re-load replaces the snapshot
  json c = circuit();
  c[ "populations" ].erase( 3 );
  c[ "projections" ].erase( 3 );
  CHECK_MSG( e.load( c.dump() ), e.error() );
  CHECK( e.structure().populations.size() == 3 and e.has_loop( "M", 2, -1 ) );
  return chk::finish( "test_structure" );
}
