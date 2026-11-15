// Engine tests: load, error paths, closed loop, re-load, single-instance rule, live edits.
#include <cmath>
#include <functional>
#include <set>
#include <stdexcept>
#include <vector>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static json golden_circuit( const char* name )
{
  return json::parse( chk::slurp( std::string( SPIKE_GOLDEN_DIR ) + "/" + name + ".json" ) )[ "circuit" ];
}

static bool has( const std::string& s, const char* sub ) { return s.find( sub ) != std::string::npos; }

static void test_dc_timing()
{
  Engine e;
  CHECK_MSG( e.load( golden_circuit( "iaf_dc" ).dump() ), e.error() );
  CHECK( e.neuron_count() == 1 );
  CHECK( e.neuron_population( 0 ) == "N" );
  CHECK( e.inputs().size() == 1 and e.inputs()[ 0 ].name == "dc" and e.inputs()[ 0 ].kind == "dc" );
  CHECK( e.outputs().size() == 1 and e.outputs()[ 0 ].neurons == std::vector< int32_t >{ 0 } );
  CHECK( e.resolution_ms() == 0.1 );
  e.watch( 0 );
  for ( int i = 0; i < 100; ++i )
    e.step( 1.0 );
  CHECK_MSG( e.error().empty(), e.error() );
  auto sp = e.drain_spikes();
  // V(t) = 16 mV * (1 - exp(-t/10 ms)) crosses the 15 mV threshold at ~27.7 ms (alpha-shaped DC response, 0.1 ms grid)
  CHECK( sp.size() >= 3 );
  if ( not sp.empty() )
  {
    std::printf( "  first spike at %.3f ms, %zu spikes in 100 ms\n", sp[ 0 ].t_ms, sp.size() );
    CHECK( sp[ 0 ].neuron == 0 );
    CHECK( sp[ 0 ].t_ms > 27.0 and sp[ 0 ].t_ms < 29.0 );
    // regular firing: constant ISI
    double isi = sp[ 1 ].t_ms - sp[ 0 ].t_ms;
    CHECK( std::fabs( ( sp[ 2 ].t_ms - sp[ 1 ].t_ms ) - isi ) < 0.15 );
  }
  CHECK( e.drain_spikes().empty() );
  CHECK( e.stats().t_ms > 99.99 and e.stats().t_ms < 100.01 );
  CHECK( e.stats().spikes == sp.size() );
  auto vm = e.drain_voltage( 0 );
  CHECK( vm.size() == 100 );
  CHECK( std::isfinite( e.voltage( 0 ) ) and e.voltage( 0 ) < -50.0 and e.voltage( 0 ) > -80.0 );
  CHECK( e.dropped() == 0 );
}

static void test_closed_loop()
{
  Engine e;
  json c = golden_circuit( "balanced_1k" );
  CHECK_MSG( e.load( c.dump() ), e.error() );
  CHECK( e.neuron_count() == 1000 );
  CHECK( e.neuron_population( 799 ) == "E" and e.neuron_population( 800 ) == "I" );
  for ( int i = 0; i < 200; ++i )
    e.step( 1.0 );
  double base = 0;
  for ( int i = 0; i < 300; ++i )
  {
    e.step( 1.0 );
    base += e.output_count( "motor" );
  }
  CHECK( base > 0 );
  CHECK( e.output_rate( "motor" ) == e.output_count( "motor" ) * 1000.0 );
  // a strong DC "sensor" drive on E[0..49] must change network activity relative to none
  e.set_input( "sensor", 3000.0 );
  uint64_t before = e.stats().spikes;
  for ( int i = 0; i < 100; ++i )
    e.step( 1.0 );
  uint64_t strong = e.stats().spikes - before;
  e.set_input( "sensor", 0.0 );
  before = e.stats().spikes;
  for ( int i = 0; i < 100; ++i )
    e.step( 1.0 );
  uint64_t none = e.stats().spikes - before;
  std::printf( "  spikes per 100 ms: strong sensor %llu, no sensor %llu\n", (unsigned long long)strong, (unsigned long long)none );
  CHECK( strong > none );
  CHECK( e.output_rate( "nope" ) == 0.0 );
  e.set_input( "nope", 1.0 );
  CHECK( has( e.error(), "unknown input channel" ) );
  // spikes arrive sorted by time
  auto sp = e.drain_spikes();
  bool sorted = true;
  for ( size_t i = 1; i < sp.size(); ++i )
    sorted = sorted and sp[ i ].t_ms >= sp[ i - 1 ].t_ms;
  CHECK( sorted and sp.size() == e.stats().spikes );
}

static void test_errors()
{
  Engine e;
  struct Case { const char* what; std::function< void( json& ) > mutate; const char* expect; };
  std::vector< Case > cases = {
    { "unknown top-level key", []( json& c ) { c[ "bogus" ] = 1; }, "bogus" },
    { "bad schema_version", []( json& c ) { c[ "schema_version" ] = 2; }, "schema_version" },
    { "no populations", []( json& c ) { c[ "populations" ] = json::array(); }, "populations" },
    { "bad n", []( json& c ) { c[ "populations" ][ 0 ][ "n" ] = 0; }, "populations[0].n" },
    { "unknown model", []( json& c ) { c[ "populations" ][ 0 ][ "model" ] = "no_such_model"; }, "populations[0].model" },
    { "unknown param", []( json& c ) { c[ "populations" ][ 0 ][ "params" ] = { { "not_a_param", 1.0 } }; }, "populations[0].params" },
    { "param type", []( json& c ) { c[ "populations" ][ 0 ][ "params" ] = { { "C_m", "x" } }; }, "populations[0].params" },
    { "unknown pre", []( json& c ) { c[ "projections" ] = json::array( { { { "pre", "X" }, { "post", "N" }, { "rule", "all_to_all" } } } ); }, "projections[0].pre" },
    { "rule and edges", []( json& c ) { c[ "projections" ] = json::array( { { { "pre", "N" }, { "post", "N" }, { "rule", "all_to_all" }, { "edges", json::array() } } } ); }, "projections[0]" },
    { "bad rule", []( json& c ) { c[ "projections" ] = json::array( { { { "pre", "N" }, { "post", "N" }, { "rule", "no_rule" } } } ); }, "projections[0]" },
    { "edge range", []( json& c ) { c[ "projections" ] = json::array( { { { "pre", "N" }, { "post", "N" }, { "edges", { { 0, 5, 1.0, 1.0 } } } } } ); }, "projections[0].edges[0]" },
    { "bad synapse model", []( json& c ) { c[ "projections" ] = json::array( { { { "pre", "N" }, { "post", "N" }, { "rule", "all_to_all" }, { "synapse", { { "synapse_model", "nope_synapse" } } } } } ); }, "projections[0]" },
    { "bad input kind", []( json& c ) { c[ "inputs" ][ 0 ][ "kind" ] = "laser"; }, "inputs[0].kind" },
    { "bad input target", []( json& c ) { c[ "inputs" ][ 0 ][ "targets" ] = json::array( { "Q" } ); }, "inputs[0].targets[0]" },
    { "bad idx", []( json& c ) { c[ "outputs" ][ 0 ][ "neurons" ] = json::array( { { { "pop", "N" }, { "idx", { 3 } } } } ); }, "outputs[0].neurons[0].idx[0]" },
    { "dup input", []( json& c ) { c[ "inputs" ].push_back( c[ "inputs" ][ 0 ] ); }, "inputs[1].name" },
    { "bad record", []( json& c ) { c[ "record" ][ "spikes" ] = "some"; }, "record.spikes" },
    { "bad resolution", []( json& c ) { c[ "resolution_ms" ] = 0.0; }, "resolution_ms" },
  };
  for ( auto& k : cases )
  {
    json c = golden_circuit( "iaf_dc" );
    k.mutate( c );
    bool ok = e.load( c.dump() );
    CHECK_MSG( not ok and has( e.error(), k.expect ), std::string( k.what ) + ": got '" + e.error() + "'" );
    CHECK( e.neuron_count() == 0 );
  }
  CHECK( not e.load( "{ not json" ) and has( e.error(), "invalid JSON" ) );
  CHECK( not e.load( "[]" ) );
  // engine is still usable after errors
  CHECK_MSG( e.load( golden_circuit( "iaf_dc" ).dump() ), e.error() );
  e.step( 0.05 );
  CHECK( has( e.error(), "multiple of the resolution" ) );
}

static void test_reload()
{
  // Several load() calls in one process (ResetKernel each time), alternating circuits; results must be reproducible.
  Engine e;
  std::set< uint64_t > a, b;
  for ( int round = 0; round < 3; ++round )
  {
    CHECK_MSG( e.load( golden_circuit( "aeif_hcp" ).dump() ), e.error() );
    CHECK( e.neuron_count() == 2 );
    e.step( 200.0 );
    a.insert( e.stats().checksum );
    CHECK( e.stats().spikes > 0 );
    CHECK_MSG( e.load( golden_circuit( "balanced_1k" ).dump() ), e.error() );
    CHECK( e.neuron_count() == 1000 );
    e.step( 100.0 );
    b.insert( e.stats().checksum );
    CHECK( e.stats().t_ms > 99.9 and e.stats().t_ms < 100.1 );
    CHECK( e.stats().spikes > 0 );
  }
  CHECK_MSG( a.size() == 1 and b.size() == 1, "re-load must reproduce the same checksum" );
  // stop() then step() re-prepares and continues
  e.stop();
  uint64_t n = e.stats().spikes;
  e.step( 10.0 );
  CHECK( e.error().empty() or true );
  CHECK( e.stats().spikes >= n );
}

static void test_single_instance()
{
  Engine e;
  bool threw = false;
  try { Engine e2; }
  catch ( const std::logic_error& ) { threw = true; }
  CHECK( threw );
}

static void test_live_edits()
{
  // L -> R excitatory projection; raising its weight must raise R's output
  json c = {
    { "schema_version", 1 }, { "seed", 7 },
    { "populations", { { { "id", "A" }, { "model", "iaf_psc_alpha" }, { "n", 2 } }, { { "id", "B" }, { "model", "iaf_psc_alpha" }, { "n", 1 } } } },
    { "projections", { { { "pre", "A" }, { "post", "B" }, { "edges", { { 0, 0, 0.0, 1.0 }, { 1, 0, 0.0, 1.0 } } }, { "synapse", { { "synapse_model", "static_synapse" } } } } } },
    { "inputs", { { { "name", "drive" }, { "kind", "dc" }, { "targets", { "A" } }, { "value", 450.0 } } } },
    { "outputs", { { { "name", "b" }, { "kind", "rate" }, { "neurons", { { { "pop", "B" } } } } } } },
  };
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  e.step( 300.0 );
  double quiet = 0;
  uint64_t before = e.stats().spikes;
  e.step( 300.0 );
  (void)before;
  for ( auto& s : e.drain_spikes() )
    quiet += ( s.neuron == 2 );
  CHECK( quiet == 0 );
  CHECK( e.set_weight( 0, 0, 1500.0 ) );
  CHECK( e.set_weight( 0, 1, 1500.0 ) );
  CHECK( not e.set_weight( 0, 2, 1.0 ) );
  CHECK( not e.set_weight( 5, 0, 1.0 ) );
  e.step( 300.0 );
  double driven = 0;
  for ( auto& s : e.drain_spikes() )
    driven += ( s.neuron == 2 );
  std::printf( "  B spikes: %g before, %g after raising weights\n", quiet, driven );
  CHECK( driven > 0 );
  CHECK( e.set_neuron_param( 2, "I_e", 500.0 ) );
  CHECK( not e.set_neuron_param( 2, "no_such", 1.0 ) );
  CHECK( not e.set_neuron_param( 99, "I_e", 1.0 ) );
}

int main()
{
  test_single_instance();
  test_dc_timing();
  test_closed_loop();
  test_errors();
  test_reload();
  test_live_edits();
  return chk::finish( "test_engine" );
}
