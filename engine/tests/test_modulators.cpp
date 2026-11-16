// Neuromodulators: Engine::set_modulator / modulator, the circuit "modulation" table, and the "modulator" output kind.
#include <cmath>
#include <string>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static bool has( const std::string& s, const char* sub ) { return s.find( sub ) != std::string::npos; }

static json iaf( const char* id, json params = json::object() )
{
  params[ "V_m" ] = -70.0;  // no random initial V_m: the tests count spikes
  return { { "id", id }, { "model", "iaf_psc_alpha" }, { "n", 1 }, { "params", params } };
}

static json circuit( json pops, json modulation, json inputs = json::array(), json outputs = json::array() )
{
  return { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 1 }, { "populations", pops }, { "modulation", modulation }, { "inputs", inputs }, { "outputs", outputs } };
}

static std::string load_error( const json& c )
{
  Engine e;
  return e.load( c.dump() ) ? std::string() : e.error();
}

static double spikes_in( Engine& e, double ms )
{
  e.step( ms );
  double n = static_cast< double >( e.drain_spikes().size() );
  return n;
}

static void test_api_and_table()
{
  Engine e;
  CHECK( e.set_modulator( "NE", 0.5 ) == true );  // before load is fine: it is remembered
  CHECK( e.modulator( "NE" ) == 0.5 and e.modulator( "ACh" ) == 0.0 and e.modulator( "DA" ) == 0.0 );
  CHECK( e.set_modulator( "serotonin", 1.0 ) == false and has( e.error(), "unknown modulator" ) );
  CHECK( modulator_names().size() == 3 );
  e.set_modulator( "NE", 0.0 );

  // additive: I_e = 0 + 500 * NE; silent at NE = 0 (rheobase 375 pA), fires at NE = 1
  json c = circuit( json::array( { iaf( "N" ) } ), json::array( { { { "modulator", "NE" }, { "pop", "N" }, { "param", "I_e" }, { "gain", 500.0 } } } ) );
  CHECK_MSG( e.load( c.dump() ), e.error() );
  CHECK( spikes_in( e, 300 ) == 0 );
  e.set_modulator( "NE", 1.0 );
  CHECK( e.modulator( "NE" ) == 1.0 );
  const double hi = spikes_in( e, 300 );
  e.set_modulator( "NE", 0.0 );
  spikes_in( e, 100 );  // let it settle
  const double lo = spikes_in( e, 300 );
  std::printf( "  NE 0 -> 1 -> 0: spikes in 300 ms: 0, %.0f, %.0f\n", hi, lo );
  CHECK( hi > 5 and lo == 0 );
  CHECK_MSG( e.last_step_ok(), e.error() );

  // multiplicative with clamp: base I_e = 300, scaled by (1 + 0.5 NE) = 450 (fires); max 350 keeps it silent
  json s1 = circuit( json::array( { iaf( "N", { { "I_e", 300.0 } } ) } ), json::array( { { { "modulator", "ACh" }, { "pop", "N" }, { "param", "I_e" }, { "scale_per_unit", 0.5 } } } ) );
  CHECK_MSG( e.load( s1.dump() ), e.error() );
  CHECK( spikes_in( e, 300 ) == 0 );
  e.set_modulator( "ACh", 1.0 );
  CHECK( spikes_in( e, 300 ) > 5 );
  json s2 = s1;
  s2[ "modulation" ][ 0 ][ "max" ] = 350.0;
  CHECK_MSG( e.load( s2.dump() ), e.error() );
  e.set_modulator( "ACh", 1.0 );
  CHECK( spikes_in( e, 300 ) == 0 );
  // two rows on one parameter compose in file order
  json s3 = circuit( json::array( { iaf( "N", { { "I_e", 0.0 } } ) } ),
    json::array( { { { "modulator", "NE" }, { "pop", "N" }, { "param", "I_e" }, { "gain", 200.0 } }, { { "modulator", "DA" }, { "pop", "N" }, { "param", "I_e" }, { "gain", 200.0 } } } ) );
  CHECK_MSG( e.load( s3.dump() ), e.error() );
  e.set_modulator( "NE", 1.0 );
  CHECK( spikes_in( e, 300 ) == 0 );  // 200 pA: below rheobase
  e.set_modulator( "DA", 1.0 );
  CHECK( spikes_in( e, 300 ) > 5 );  // 400 pA
}

static void test_errors()
{
  json p = json::array( { iaf( "N" ) } );
  auto row = []( json r ) { return json::array( { r } ); };
  std::string e;
  e = load_error( circuit( p, row( { { "modulator", "NE" }, { "pop", "N" }, { "param", "no_such_param" }, { "gain", 1.0 } } ) ) );
  CHECK_MSG( has( e, "modulation[0].param" ) and has( e, "no_such_param" ), e );
  e = load_error( circuit( p, row( { { "modulator", "XX" }, { "pop", "N" }, { "param", "I_e" }, { "gain", 1.0 } } ) ) );
  CHECK_MSG( has( e, "modulation[0].modulator" ) and has( e, "NE, ACh or DA" ), e );
  e = load_error( circuit( p, row( { { "modulator", "NE" }, { "pop", "Q" }, { "param", "I_e" }, { "gain", 1.0 } } ) ) );
  CHECK_MSG( has( e, "modulation[0].pop" ), e );
  e = load_error( circuit( p, row( { { "modulator", "NE" }, { "pop", "N" }, { "param", "I_e" } } ) ) );
  CHECK_MSG( has( e, "exactly one of 'gain'" ), e );
  e = load_error( circuit( p, row( { { "modulator", "NE" }, { "pop", "N" }, { "param", "I_e" }, { "gain", 1.0 }, { "scale_per_unit", 1.0 } } ) ) );
  CHECK_MSG( has( e, "exactly one of 'gain'" ), e );
  e = load_error( circuit( p, row( { { "modulator", "NE" }, { "pop", "N" }, { "param", "I_e" }, { "gain", 1.0 }, { "min", 2.0 }, { "max", 1.0 } } ) ) );
  CHECK_MSG( has( e, "min must be <= max" ), e );
  json out = { { "name", "r" }, { "kind", "modulator" }, { "neurons", json::array( { "N" } ) } };
  e = load_error( circuit( p, json::array(), json::array(), json::array( { out } ) ) );
  CHECK_MSG( has( e, "outputs[0].modulator" ) and has( e, "required" ), e );
  out[ "modulator" ] = "NE";
  out[ "tau_ms" ] = 0.0;
  e = load_error( circuit( p, json::array(), json::array(), json::array( { out } ) ) );
  CHECK_MSG( has( e, "outputs[0].tau_ms" ), e );
  json rate = { { "name", "r" }, { "kind", "rate" }, { "neurons", json::array( { "N" } ) }, { "gain", 1.0 } };
  e = load_error( circuit( p, json::array(), json::array(), json::array( { rate } ) ) );
  CHECK_MSG( has( e, "only valid for kind" ), e );
}

static void test_release_kinetics_and_loop()
{
  // R fires regularly from DC; its spikes release NE (gain 0.1 per spike, tau 200 ms); NE raises the I_e of S (silent otherwise)
  json c = circuit( json::array( { iaf( "R" ), iaf( "S" ) } ), json::array( { { { "modulator", "NE" }, { "pop", "S" }, { "param", "I_e" }, { "gain", 100.0 } } } ),
    json::array( { { { "name", "drive" }, { "kind", "dc" }, { "targets", json::array( { "R" } ) }, { "weight", 1.0 }, { "value", 0.0 } } } ),
    json::array( { { { "name", "ne" }, { "kind", "modulator" }, { "modulator", "NE" }, { "neurons", json::array( { "R" } ) }, { "gain", 0.1 }, { "tau_ms", 200.0 } } } ) );
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  CHECK( e.outputs().size() == 1 and e.outputs()[ 0 ].kind == "modulator" );
  spikes_in( e, 200 );
  CHECK( e.modulator( "NE" ) == 0.0 );
  e.set_input( "drive", 600.0 );
  double nspk = 0;
  for ( int i = 0; i < 20; ++i )
    nspk += spikes_in( e, 100 );  // 2 s
  const double rate = nspk / 2.0;  // Hz (S is silent while NE is low enough; counted together, see below)
  const double ne = e.modulator( "NE" );
  std::printf( "  R+S spikes in 2 s: %.0f; NE level %.3f\n", nspk, ne );
  // steady state of first-order release: gain * rate * tau = 0.1 * f * 0.2 s; R alone fires at about rate_R (S joins only at NE >= 3.75)
  CHECK( ne > 0.2 and ne < 3.0 );
  // first-order release: steady level = gain * rate * tau (output_rate of a modulator output is the release population's rate)
  const double expect = 0.1 * e.output_rate( "ne" ) * 0.2;
  std::printf( "  steady NE %.3f vs gain*rate*tau %.3f (R at %.1f Hz)\n", ne, expect, e.output_rate( "ne" ) );
  CHECK( std::fabs( ne - expect ) < 0.25 * expect );
  (void)rate;
  // decay: silence R and watch NE fall as exp(-t/200 ms) (spikes in the next step are none)
  e.set_input( "drive", 0.0 );
  e.step( 100.0 );
  e.drain_spikes();
  const double a = e.modulator( "NE" );
  e.step( 100.0 );
  const double b = e.modulator( "NE" );
  std::printf( "  NE decay over 100 ms: %.4f -> %.4f (ratio %.3f, expected %.3f)\n", a, b, b / a, std::exp( -100.0 / 200.0 ) );
  CHECK( std::fabs( b / a - std::exp( -0.5 ) ) < 0.02 );
  // external level adds to the pool
  e.set_modulator( "NE", 0.25 );
  CHECK( std::fabs( e.modulator( "NE" ) - ( b + 0.25 ) ) < 1e-12 );
}

static void test_loop_drives_target()
{
  // the S cell only fires when R's release has raised NE: compare S spikes with R silent and R driven
  auto run = [ & ]( double drive ) {
    json c = circuit( json::array( { iaf( "R" ), iaf( "S" ) } ), json::array( { { { "modulator", "NE" }, { "pop", "S" }, { "param", "I_e" }, { "gain", 1000.0 } } } ),
      json::array( { { { "name", "drive" }, { "kind", "dc" }, { "targets", json::array( { "R" } ) }, { "weight", 1.0 }, { "value", drive } } } ),
      json::array( { { { "name", "ne" }, { "kind", "modulator" }, { "modulator", "NE" }, { "neurons", json::array( { "R" } ) }, { "gain", 0.1 }, { "tau_ms", 200.0 } },
        { { "name", "s" }, { "kind", "rate" }, { "neurons", json::array( { "S" } ) } } } ) );
    Engine e;
    CHECK_MSG( e.load( c.dump() ), e.error() );
    double n = 0;
    for ( int i = 0; i < 30; ++i )
    {
      e.step( 100.0 );
      n += e.output_count( "s" );
    }
    CHECK_MSG( e.last_step_ok(), e.error() );
    return n;
  };
  const double quiet = run( 0.0 ), active = run( 600.0 );
  std::printf( "  S spikes in 3 s: R silent %.0f, R driven %.0f\n", quiet, active );
  CHECK( quiet == 0 and active > 10 );
}

int main()
{
  test_api_and_table();
  test_errors();
  test_release_kinetics_and_loop();
  test_loop_drives_target();
  return chk::finish( "test_modulators" );
}
