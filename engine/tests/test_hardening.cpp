// Review wave "engine": GSL errors never abort, parse-time budgets, projections that share (pre, post, synapse model) stay distinct,
// Engine::error() is cleared on success, zero-weight edges agree between structure() and connections(), the functional weight floor.
#include <cmath>
#include <string>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static bool has( const std::string& hay, const std::string& needle ) { return hay.find( needle ) != std::string::npos; }

static json base()
{
  return json::parse( R"({"schema_version":1,"resolution_ms":0.1,"seed":1,
    "populations":[{"id":"A","model":"iaf_psc_alpha","n":2},{"id":"B","model":"iaf_psc_alpha","n":2}],
    "projections":[{"pre":"A","post":"B","rule":"all_to_all","synapse":{"weight":500.0,"delay":1.0}}],
    "inputs":[{"name":"s","kind":"dc","targets":["A"],"value":500.0}],
    "outputs":[{"name":"m","neurons":["B"]}]})" );
}

static std::string load_error( const json& c )
{
  Engine e;
  return e.load( c.dump() ) ? std::string() : e.error();
}

// P0-1: a siegert_neuron driven far below its integral's range used to abort the process in GSL's default error handler.
static void test_negative_drive()
{
  json c = json::parse( R"({"schema_version":1,"seed":1,
    "populations":[{"id":"A","model":"siegert_neuron","n":1},{"id":"B","model":"siegert_neuron","n":1}],
    "projections":[{"pre":"A","post":"B","rule":"all_to_all","synapse":{"synapse_model":"diffusion_connection","weight":1.0,"K":100.0}}],
    "inputs":[{"name":"s","kind":"rate_drive","targets":["A"],"value":-1000000.0}],
    "outputs":[{"name":"m","kind":"rate_value","neurons":["B"]}]})" );
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  for ( int i = 0; i < 50; ++i )
    e.step( 1.0 );  // reaching here at all is the test: no abort()
  CHECK_MSG( e.last_step_ok() or not e.error().empty(), "a failed step must say why" );
  e.set_input( "s", 1e300 );
  for ( int i = 0; i < 20; ++i )
    e.step( 1.0 );
  CHECK_MSG( e.load( base().dump() ), e.error() );  // load() recovers
}

// P2: error() is cleared by a successful step, so the live run and CHECK agree.
static void test_error_cleared()
{
  Engine e;
  CHECK_MSG( e.load( base().dump() ), e.error() );
  e.set_input( "nope", 1.0 );
  CHECK( not e.error().empty() );
  e.step( 1.0 );
  CHECK( e.last_step_ok() and e.error().empty() );
  e.set_input( "s", 1e300 );  // clamped, not an error
  e.step( 1.0 );
  CHECK_MSG( e.last_step_ok() and e.error().empty(), e.error() );
  e.set_input( "s", std::nan( "" ) );
  CHECK( not e.error().empty() );
}

// P1-2: parse-time budgets with plain messages.
static void test_budgets()
{
  json c = base();
  c[ "populations" ][ 0 ][ "n" ] = 20001;
  CHECK_MSG( has( load_error( c ), "more than 20000 neurons" ), load_error( c ) );
  c = base();
  c[ "populations" ][ 0 ][ "n" ] = 10000;
  c[ "populations" ][ 1 ][ "n" ] = 10000;
  CHECK_MSG( has( load_error( c ), "connections; the limit" ), load_error( c ) );  // all_to_all 1e4 x 1e4
  c = base();
  c[ "populations" ] = json::array();
  for ( int i = 0; i < 300; ++i )
    c[ "populations" ].push_back( { { "id", "P" + std::to_string( i ) }, { "model", "iaf_psc_alpha" }, { "n", 10000000 } } );
  CHECK_MSG( has( load_error( c ), "neurons" ), load_error( c ) );  // used to overflow the int32 neuron counter
  c = base();
  c[ "projections" ][ 0 ][ "synapse" ][ "delay" ] = 1e6;
  CHECK_MSG( has( load_error( c ), "delay" ) and has( load_error( c ), "1000 ms" ), load_error( c ) );
  c = base();
  c[ "inputs" ][ 0 ] = { { "name", "s" }, { "kind", "poisson" }, { "targets", json::array( { "A" } ) }, { "value", 1e25 } };
  CHECK_MSG( has( load_error( c ), "Hz" ), load_error( c ) );
  c = base();
  c[ "inputs" ][ 0 ][ "value" ] = 1e6;
  CHECK_MSG( has( load_error( c ), "pA" ), load_error( c ) );
  c = base();
  c[ "seed" ] = 0;
  CHECK_MSG( has( load_error( c ), "seed" ), load_error( c ) );
  c = base();
  c.erase( "seed" );
  CHECK_MSG( load_error( c ).empty(), load_error( c ) );  // the default seed is 1
  c = base();
  c[ "populations" ] = { { { "id", "A" }, { "model", "hh_psc_alpha_gap" }, { "n", 2 } }, { { "id", "B" }, { "model", "hh_psc_alpha_gap" }, { "n", 2 } } };
  c[ "projections" ] = json::array( { { { "pre", "A" }, { "post", "B" }, { "edges", json::array( { { 0, 0, 1e6, 0.1 } } ) }, { "synapse", { { "synapse_model", "gap_junction" } } } } } );
  CHECK_MSG( has( load_error( c ), "gap_junction weight" ), load_error( c ) );
  std::string deep = "{\"schema_version\":1,\"meta\":" + std::string( 100000, '[' ) + std::string( 100000, ']' ) + "}";
  Engine e;
  CHECK( not e.load( deep ) );
  CHECK_MSG( has( e.error(), "nested deeper than" ), e.error() );
  // NESTML cells: weight ceiling and DC cap (the cells exist only when scripts/build-nestml.sh was run; the checks are in the parser)
  c = base();
  c[ "populations" ][ 0 ][ "model" ] = "butera_burster";
  c[ "inputs" ][ 0 ][ "value" ] = 2000.0;
  CHECK_MSG( has( load_error( c ), "1000 pA" ), load_error( c ) );
  c = base();
  c[ "populations" ][ 1 ][ "model" ] = "sa_node_pacemaker";
  c[ "projections" ][ 0 ][ "synapse" ][ "weight" ] = 1e8;
  CHECK_MSG( has( load_error( c ), "allowed range" ), load_error( c ) );
}

// P1-1: two projections with the same (pre, post, synapse model) are two projections everywhere.
static void test_shared_triple()
{
  json c = json::parse( R"({"schema_version":1,"seed":1,
   "populations":[{"id":"A","model":"iaf_psc_alpha","n":2},{"id":"B","model":"iaf_psc_alpha","n":2},{"id":"I","model":"iaf_psc_alpha","n":1}],
   "projections":[{"pre":"A","post":"B","rule":"all_to_all","synapse":{"weight":50.0}},
                  {"pre":"A","post":"B","rule":"one_to_one","synapse":{"weight":-200.0}},
                  {"pre":"B","post":"I","edges":[[0,0,0.0,1.0],[1,0,0.0,1.0],[0,0,0.000001,1.0]]},
                  {"pre":"I","post":"B","rule":"all_to_all","synapse":{"weight":-30.0}}],
   "modulation":[{"modulator":"NE","projection":1,"param":"weight","scale_per_unit":1.0}]})" );
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  Structure s = e.structure();
  CHECK( s.projections.size() == 4 );
  CHECK_MSG( s.projections[ 0 ].n_edges == 4 and s.projections[ 0 ].sign == 1 and s.projections[ 0 ].w_mean == 50.0, "projection 0 is the +50 all_to_all" );
  CHECK_MSG( s.projections[ 1 ].n_edges == 2 and s.projections[ 1 ].sign == -1 and s.projections[ 1 ].w_mean == -200.0, "projection 1 is the -200 one_to_one" );
  CHECK( s.projections[ 0 ].synapse_model == "static_synapse" and s.projections[ 1 ].synapse_model == "static_synapse" );
  CHECK( e.has_path( "A", "B", 0, 1 ) and e.has_path( "A", "B", 0, -1 ) );
  CHECK( e.has_loop( "B", 0, -1 ) );  // B -> I (+1e-6, the zero-weight edges are not edges) -> B (-30)
  // zero-weight edges agree between structure() and connections()
  CHECK_MSG( s.projections[ 2 ].n_edges == 1, "only the non-zero edge of projection 2 counts" );
  size_t per_proj[ 4 ] = { 0, 0, 0, 0 };
  for ( const auto& cn : e.connections() )
    ++per_proj[ cn.projection ];
  for ( int k = 0; k < 4; ++k )
    CHECK_MSG( static_cast< int >( per_proj[ k ] ) == s.projections[ k ].n_edges, "projection " + std::to_string( k ) );
  CHECK( e.connections().size() == 9 );
  // set_weight(1, 0, -1) touches projection 1 only
  CHECK_MSG( e.set_weight( 1, 0, -1.0 ), e.error() );
  s = e.structure();
  CHECK_MSG( s.projections[ 0 ].w_min == 50.0 and s.projections[ 0 ].w_max == 50.0, "projection 0 untouched" );
  CHECK_MSG( s.projections[ 1 ].w_min == -200.0 and s.projections[ 1 ].w_max == -1.0, "projection 1 edited" );
  // modulation of projection 1 only: the row scales its weights, projection 0 keeps 50
  e.set_modulator( "NE", 1.0 );
  e.step( 1.0 );
  s = e.structure();
  CHECK_MSG( s.projections[ 0 ].w_max == 50.0 and s.projections[ 0 ].w_min == 50.0, "modulation must not reach projection 0" );
  CHECK_MSG( s.projections[ 1 ].w_max != -1.0, "modulation reaches projection 1" );
}

// P2.6: the functional weight floor (add-only)
static void test_weight_floor()
{
  json c = json::parse( R"({"schema_version":1,"seed":1,
   "populations":[{"id":"A","model":"iaf_psc_alpha","n":2},{"id":"B","model":"iaf_psc_alpha","n":2},{"id":"C","model":"iaf_psc_alpha","n":2}],
   "projections":[{"pre":"A","post":"B","rule":"all_to_all","synapse":{"weight":100.0}},
                  {"pre":"B","post":"C","rule":"all_to_all","synapse":{"weight":0.01}},
                  {"pre":"C","post":"A","rule":"all_to_all","synapse":{"weight":-100.0}}]})" );
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  CHECK( e.has_path( "A", "C" ) and e.has_path( "A", "C", 0, 0, 0.0 ) );   // any weight: connected
  CHECK( not e.has_path( "A", "C", 0, 0, 1.0 ) );                         // a 0.01 pA wire does not count
  CHECK( e.has_path( "A", "B", 0, 0, 1.0 ) );
  CHECK( e.has_loop( "A" ) and e.has_loop( "A", 0, 0, 0.0 ) and not e.has_loop( "A", 0, 0, 1.0 ) );
  CHECK( e.has_loop_via( "A", "C", 0, 0 ) and not e.has_loop_via( "A", "C", 0, 0, 1.0 ) );
  CHECK( e.structure().projections[ 1 ].w_abs_max == 0.01 );
  CHECK( e.connections().size() == 12 and e.connections( 1.0 ).size() == 8 );
}

int main()
{
  test_negative_drive();
  test_error_cleared();
  test_budgets();
  test_shared_triple();
  test_weight_floor();
  return chk::finish( "test_hardening" );
}
