// Synapse models in the circuit loader: gap_junction, tsodyks2, quantal_stp, stdp, stdp_dopamine (+ volume_transmitter), and validation errors.
#include <cmath>
#include <deque>
#include <string>
#include <vector>
#include "check.h"
#include "connection_id.h"
#include "nest.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static bool has( const std::string& s, const char* sub ) { return s.find( sub ) != std::string::npos; }

static json pop( const char* id, const char* model, int n, json params = json::object() )
{
  return { { "id", id }, { "model", model }, { "n", n }, { "params", params } };
}

// Parameters of the connections of a synapse model, read straight from NEST (the engine owns the kernel).
static std::vector< Dictionary > conns_of( const std::string& model )
{
  Dictionary q;
  q[ "synapse_model" ] = model;
  std::deque< nest::ConnectionID > flat;
  for ( auto& dq : nest::get_connections( q ) )
    flat.insert( flat.end(), dq.begin(), dq.end() );
  return nest::get_connection_status( flat );
}

static std::string load_error( json c )
{
  Engine e;
  return e.load( c.dump() ) ? std::string() : e.error();
}

static json base( json pops, json projs )
{
  return { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 3 }, { "populations", pops }, { "projections", projs } };
}

static void test_gap_junction()
{
  json gp = { { "V_m", -70.0 } };
  json c = base( json::array( { pop( "A", "hh_psc_alpha_gap", 1, gp ), pop( "B", "hh_psc_alpha_gap", 1, gp ) } ),
    json::array( { { { "pre", "A" }, { "post", "B" }, { "rule", "one_to_one" }, { "synapse", { { "synapse_model", "gap_junction" }, { "weight", 2.0 } } } } } ) );
  c[ "inputs" ] = json::array( { { { "name", "drive_A" }, { "kind", "dc" }, { "targets", json::array( { "A" } ) }, { "weight", 1.0 }, { "value", 1000.0 } },
    { { "name", "drive_B" }, { "kind", "dc" }, { "targets", json::array( { "B" } ) }, { "weight", 1.0 }, { "value", 0.0 } } } );
  auto peak_b = [ & ]( json circuit, const char* drive ) {
    Engine e;
    CHECK_MSG( e.load( circuit.dump() ), e.error() );
    e.set_input( drive, 1000.0 );
    e.watch( 0 );
    e.watch( 1 );
    double mx = -1e9;
    int other = std::string( drive ) == "drive_A" ? 1 : 0;
    for ( int i = 0; i < 300; ++i )
    {
      e.step( 1.0 );
      mx = std::max( mx, e.voltage( other ) );
    }
    CHECK_MSG( e.last_step_ok(), e.error() );
    return mx;
  };
  json off = c;
  off[ "projections" ] = json::array();
  const double uncoupled = peak_b( off, "drive_A" );
  const double coupled = peak_b( c, "drive_A" );
  std::printf( "  gap pair: follower max V_m %.1f mV coupled vs %.1f mV uncoupled\n", coupled, uncoupled );
  CHECK( coupled > uncoupled + 1.0 );
  // symmetric: driving B instead of A depolarises A just the same
  const double reverse = peak_b( c, "drive_B" );
  CHECK( std::fabs( reverse - coupled ) < 1e-6 or reverse > uncoupled + 1.0 );
  {  // edges form: mirrored by the engine, delay column ignored
    json ce = c;
    ce[ "projections" ] = json::array( { { { "pre", "A" }, { "post", "B" }, { "edges", json::array( { json::array( { 0, 0, 2.0, 1.0 } ) } ) },
      { "synapse", { { "synapse_model", "gap_junction" } } } } } );
    CHECK( peak_b( ce, "drive_B" ) > uncoupled + 1.0 );  // only reaches A through the mirror edge
    Engine e;
    CHECK( e.load( ce.dump() ) );
    CHECK( conns_of( "gap_junction" ).size() == 2 );
    CHECK( e.set_weight( 0, 0, 5.0 ) );
    for ( auto& d : conns_of( "gap_junction" ) )
      CHECK( std::fabs( d.get< double >( "weight" ) - 5.0 ) < 1e-12 );  // both directions follow
  }
  {  // all_to_all inside one population: no autapses, symmetric
    json ca = base( json::array( { pop( "G", "hh_cond_beta_gap_traub", 3 ) } ),
      json::array( { { { "pre", "G" }, { "post", "G" }, { "rule", "all_to_all" }, { "synapse", { { "synapse_model", "gap_junction" }, { "weight", 1.0 } } } } } ) );
    Engine e;
    CHECK_MSG( e.load( ca.dump() ), e.error() );
    CHECK( conns_of( "gap_junction" ).size() == 6 );
  }
}

static void test_errors()
{
  json gp = json::array( { pop( "A", "hh_psc_alpha_gap", 1 ), pop( "B", "hh_psc_alpha_gap", 1 ), pop( "I", "iaf_psc_alpha", 2 ) } );
  auto proj = [ & ]( json p ) { return base( gp, json::array( { p } ) ); };
  auto syn = []( const char* m, json extra = json::object() ) {
    json s = { { "synapse_model", m } };
    s.update( extra );
    return s;
  };
  json oto = { { "pre", "A" }, { "post", "B" }, { "rule", "one_to_one" } };
  auto with = [ & ]( json p, json s ) {
    p[ "synapse" ] = s;
    return load_error( proj( p ) );
  };
  std::string e;
  e = with( { { "pre", "A" }, { "post", "I" }, { "rule", "all_to_all" } }, syn( "gap_junction", { { "weight", 1.0 } } ) );
  CHECK_MSG( has( e, "projections[0].synapse.synapse_model" ) and has( e, "gap-capable" ) and has( e, "iaf_psc_alpha" ), e );
  e = with( oto, syn( "gap_junction", { { "weight", 1.0 }, { "delay", 1.0 } } ) );
  CHECK_MSG( has( e, "synapse.delay" ) and has( e, "instantaneous" ), e );
  e = with( oto, syn( "gap_junction", { { "weight", -1.0 } } ) );
  CHECK_MSG( has( e, "gap_junction weight" ), e );
  { json p = oto; p[ "make_symmetric" ] = false; e = with( p, syn( "gap_junction", { { "weight", 1.0 } } ) ); }
  CHECK_MSG( has( e, "symmetric" ), e );
  { json p = oto; p[ "rule" ] = "fixed_indegree"; p[ "indegree" ] = 1; e = with( p, syn( "gap_junction", { { "weight", 1.0 } } ) ); }
  CHECK_MSG( has( e, "gap_junction supports the rules" ), e );
  json ie = { { "pre", "I" }, { "post", "I" }, { "rule", "all_to_all" } };
  e = with( ie, syn( "stdp_synapse", { { "weight", -5.0 } } ) );
  CHECK_MSG( has( e, "synapse.weight" ) and has( e, "excitatory" ) and has( e, "Inhibitory plasticity" ), e );
  e = with( ie, syn( "stdp_synapse", { { "weight", 5.0 }, { "Wmax", 3.0 } } ) );
  CHECK_MSG( has( e, "exceeds Wmax" ), e );
  {
    json p = { { "pre", "I" }, { "post", "I" }, { "edges", json::array( { json::array( { 0, 1, -2.0, 1.0 } ) } ) } };
    e = with( p, syn( "stdp_synapse" ) );
    CHECK_MSG( has( e, "excitatory" ), e );
  }
  e = with( ie, syn( "tsodyks2_synapse", { { "U", 1.5 } } ) );
  CHECK_MSG( has( e, "synapse.U" ) and has( e, "[0, 1]" ), e );
  e = with( ie, syn( "tsodyks2_synapse", { { "tau_rec", 0.0 } } ) );
  CHECK_MSG( has( e, "tau_rec" ), e );
  e = with( ie, syn( "quantal_stp_synapse", { { "n", 0 } } ) );
  CHECK_MSG( has( e, "synapse.n" ), e );
  e = with( ie, syn( "quantal_stp_synapse", { { "n", 2 }, { "a", 5 } } ) );
  CHECK_MSG( has( e, "synapse.a" ), e );
  e = with( ie, syn( "stdp_dopamine_synapse", { { "weight", 1.0 } } ) );
  CHECK_MSG( has( e, "dopamine_source" ), e );
  { json p = ie; p[ "dopamine_source" ] = "nope"; e = with( p, syn( "stdp_dopamine_synapse", { { "weight", 1.0 } } ) ); }
  CHECK_MSG( has( e, "unknown population 'nope'" ), e );
  { json p = ie; p[ "dopamine_source" ] = "A"; e = with( p, syn( "static_synapse" ) ); }
  CHECK_MSG( has( e, "only valid with" ), e );
  CHECK_MSG( load_error( base( gp, json::array( { { { "pre", "I" }, { "post", "I" }, { "rule", "all_to_all" },
    { "synapse", syn( "stdp_synapse", { { "weight", 1.0 } } ) } } } ) ) ).empty(), "valid stdp must load" );
}

static void test_tsodyks_depression()
{
  // Poisson (20 Hz per synapse) -> 40 parrots -> N; output spikes in 2 s. The depressing synapses start with the same PSP as the
  // static one (weight / U) and then lose most of it.
  auto total = [ & ]( json syn ) {
    json c = base( json::array( { pop( "P", "parrot_neuron", 40 ), pop( "N", "iaf_psc_alpha", 1 ) } ),
      json::array( { { { "pre", "P" }, { "post", "N" }, { "rule", "all_to_all" }, { "synapse", syn } } } ) );
    c[ "inputs" ] = json::array( { { { "name", "in" }, { "kind", "poisson" }, { "targets", json::array( { "P" } ) }, { "weight", 1.0 }, { "value", 20.0 } } } );
    c[ "outputs" ] = json::array( { { { "name", "o" }, { "kind", "rate" }, { "neurons", json::array( { "N" } ) } } } );
    Engine e;
    CHECK_MSG( e.load( c.dump() ), e.error() );
    double n = 0;
    for ( int i = 0; i < 20; ++i )
    {
      e.step( 100.0 );
      n += e.output_count( "o" );
    }
    CHECK_MSG( e.last_step_ok(), e.error() );
    return n;
  };
  const double st = total( { { "synapse_model", "static_synapse" }, { "weight", 100.0 }, { "delay", 1.0 } } );
  const double dep = total( { { "synapse_model", "tsodyks2_synapse" }, { "weight", 200.0 }, { "delay", 1.0 }, { "U", 0.5 }, { "tau_rec", 800.0 }, { "tau_fac", 0.0 } } );
  const double qs = total( { { "synapse_model", "quantal_stp_synapse" }, { "weight", 25.0 }, { "delay", 1.0 }, { "U", 0.5 }, { "tau_rec", 800.0 }, { "n", 4 } } );
  std::printf( "  output spikes in 2 s: static %.0f, tsodyks2 %.0f, quantal_stp %.0f\n", st, dep, qs );
  CHECK( st > 20 );
  CHECK( dep < 0.6 * st );
  CHECK( qs < 0.8 * st );
}

static void test_stdp_wmax()
{
  json c = base( json::array( { pop( "A", "iaf_psc_alpha", 2 ), pop( "B", "iaf_psc_alpha", 2 ) } ),
    json::array( { { { "pre", "A" }, { "post", "B" }, { "rule", "all_to_all" }, { "synapse", { { "synapse_model", "stdp_synapse" }, { "weight", 7.0 }, { "delay", 1.0 } } } },
      { { "pre", "A" }, { "post", "B" }, { "edges", json::array( { json::array( { 0, 0, 3.0, 1.0 } ), json::array( { 1, 1, 5.0, 1.0 } ) } ) },
        { "synapse", { { "synapse_model", "stdp_synapse" } } } },
      { { "pre", "A" }, { "post", "B" }, { "rule", "one_to_one" }, { "synapse", { { "synapse_model", "stdp_synapse" }, { "weight", 7.0 }, { "Wmax", 11.0 } } } } } ) );
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  int rule_ok = 0, edge_ok = 0, given_ok = 0;
  for ( auto& d : conns_of( "stdp_synapse" ) )
  {
    const double w = d.get< double >( "weight" ), wm = d.get< double >( "Wmax" );
    if ( w == 7.0 and wm == 14.0 )
      ++rule_ok;
    if ( ( w == 3.0 or w == 5.0 ) and wm == 10.0 )
      ++edge_ok;  // edges: 2 * the largest edge weight
    if ( w == 7.0 and wm == 11.0 )
      ++given_ok;
  }
  CHECK( rule_ok == 4 and edge_ok == 2 and given_ok == 2 );
}

static void test_dopamine()
{
  auto run = [ & ]( double da, std::vector< double >* weights ) {
    json c = base( json::array( { pop( "P", "parrot_neuron", 1 ), pop( "N", "iaf_psc_alpha", 1 ), pop( "D", "parrot_neuron", 1 ) } ),
      json::array( { { { "pre", "P" }, { "post", "N" }, { "rule", "all_to_all" }, { "dopamine_source", "D" },
        { "synapse", { { "synapse_model", "stdp_dopamine_synapse" }, { "weight", 20.0 }, { "delay", 1.0 }, { "A_plus", 5.0 }, { "A_minus", 5.0 }, { "tau_c", 100.0 } } } } } ) );
    c[ "inputs" ] = json::array( { { { "name", "in" }, { "kind", "poisson" }, { "targets", json::array( { "P" } ) }, { "weight", 1.0 }, { "value", 30.0 } },
      { { "name", "bias" }, { "kind", "dc" }, { "targets", json::array( { "N" } ) }, { "weight", 1.0 }, { "value", 400.0 } } } );
    Engine e;
    CHECK_MSG( e.load( c.dump() ), e.error() );
    e.set_modulator( "DA", da );
    for ( int i = 0; i < 20; ++i )
      e.step( 100.0 );
    CHECK_MSG( e.last_step_ok(), e.error() );
    for ( auto& d : conns_of( "stdp_dopamine_synapse__p0" ) )
      weights->push_back( d.get< double >( "weight" ) );
    return e.stats().spikes;
  };
  std::vector< double > w0, w1;
  run( 0.0, &w0 );
  run( 1.0, &w1 );
  CHECK( w0.size() == 1 and w1.size() == 1 );
  if ( w0.size() == 1 and w1.size() == 1 )
  {
    std::printf( "  stdp_dopamine weight after 2 s: DA=0 -> %.3f, DA=1 -> %.3f (start 20)\n", w0[ 0 ], w1[ 0 ] );
    CHECK( w0[ 0 ] == 20.0 );  // no dopamine, no change
    CHECK( std::fabs( w1[ 0 ] - 20.0 ) > 0.01 );
  }
}

int main()
{
  test_gap_junction();
  test_errors();
  test_tsodyks_depression();
  test_stdp_wmax();
  test_dopamine();
  return chk::finish( "test_synapses" );
}
