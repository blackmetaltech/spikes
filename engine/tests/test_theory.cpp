// 0.6 "Theory" wing, engine side (add-only): neuron_state / input_weight, the phase plane (nullclines, fixed points, onset type) against the NEST cell itself, rate neurons
// (lin_rate_ipn with rate_connection_instantaneous, rate_drive input, rate_value output, no spike recorder on them), siegert_neuron with a diffusion_connection (J, K),
// the tuned_poisson input and the spike-triggered average.
#include <cmath>
#include <string>
#include <vector>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"
#include "spike/theory.h"

using nlohmann::json;
using namespace spike;

static json one_cell( const std::string& model, json params )
{
  return { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 3 },
    { "populations", json::array( { { { "id", "N" }, { "model", model }, { "n", 1 }, { "params", params } } } ) },
    { "inputs", json::array( { { { "name", "cur" }, { "kind", "dc" }, { "targets", json::array( { "N" } ) }, { "weight", 2.0 }, { "value", 0.0 } } } ) },
    { "outputs", json::array( { { { "name", "sp" }, { "kind", "rate" }, { "neurons", json::array( { "N" } ) } } } ) } };
}

static int spikes_in( Engine& e, double ms )
{
  int n = 0;
  for ( double t = 0; t < ms; t += 1.0 )
  {
    e.step( 1.0 );
    n += static_cast< int >( e.output_count( "sp" ) + 0.5 );
  }
  return n;
}

static void test_state_and_phase_plane()
{
  // izhikevich: the cell settles on the stable fixed point the phase plane computes (I = 2 * 0.5 = 1 through the input weight)
  theory::PhasePlane pp;
  const theory::FixedPoint* st = nullptr;
  {
  Engine e;
  CHECK_MSG( e.load( one_cell( "izhikevich", { { "a", 0.02 }, { "b", 0.2 }, { "c", -65.0 }, { "d", 8.0 } } ).dump() ), e.error() );
  CHECK( e.input_weight( "cur" ) == 2.0 && e.input_weight( "nope" ) == 0.0 );
  CHECK( std::fabs( e.neuron_state( 0, "a" ) - 0.02 ) < 1e-12 );
  CHECK( std::isnan( e.neuron_state( 0, "no_such_key" ) ) && std::isnan( e.neuron_state( 5, "V_m" ) ) );
  e.set_input( "cur", 0.5 );
  spikes_in( e, 1500 );
  pp = theory::phase_plane_of( e, 0, 2.0 * 0.5 );
  CHECK( pp.ok && pp.model == "izhikevich" && pp.w_name == "U" );
  for ( const auto& f : pp.fixed ) if ( f.stable ) st = &f;
  CHECK( st != nullptr && pp.fixed.size() == 2 );
  if ( st )
  {
    CHECK_MSG( std::fabs( e.neuron_state( 0, "V_m" ) - st->v ) < 0.5, "V_m " + std::to_string( e.neuron_state( 0, "V_m" ) ) + " fixed point " + std::to_string( st->v ) );
    CHECK_MSG( std::fabs( e.neuron_state( 0, "U_m" ) - st->w ) < 0.5, "U_m" );
    CHECK( st->kind == "stable node" || st->kind == "stable focus" );
  }
  // the nullclines cross at the fixed points
  for ( const auto& f : pp.fixed ) CHECK( std::fabs( 0.04 * f.v * f.v + 5 * f.v + 140 + pp.i_total - f.w ) < 1e-9 && std::fabs( 0.2 * f.v - f.w ) < 1e-9 );
  }
  {
  // aeif_cond_exp: the same, with its own parameters (defaults of NEST)
  Engine a;
  CHECK_MSG( a.load( one_cell( "aeif_cond_exp", { { "a", 4.0 }, { "b", 0.0 } } ).dump() ), a.error() );
  a.set_input( "cur", 100.0 );   // 200 pA through weight 2: below rheobase
  spikes_in( a, 2500 );
  pp = theory::phase_plane_of( a, 0, 200.0 );
  CHECK( pp.ok && pp.w_name == "w" );
  st = nullptr;
  for ( const auto& f : pp.fixed ) if ( f.stable ) st = &f;
  CHECK( st != nullptr );
  if ( st )
  {
    CHECK_MSG( std::fabs( a.neuron_state( 0, "V_m" ) - st->v ) < 0.5, "aeif V_m " + std::to_string( a.neuron_state( 0, "V_m" ) ) + " vs " + std::to_string( st->v ) );
    CHECK_MSG( std::fabs( a.neuron_state( 0, "w" ) - st->w ) < 2.0, "aeif w" );
  }
  }
  // a model without a recovery variable has no phase plane
  Engine f;
  CHECK( f.load( one_cell( "iaf_psc_alpha", json::object() ).dump() ) );
  CHECK( !theory::phase_plane_of( f, 0, 0.0 ).ok && !theory::phase_plane_supported( "iaf_psc_alpha" ) );
}

static void test_onset()
{
  // regular spiking (class I): the resting state is lost through a saddle-node; resonator (class II): through a Hopf bifurcation
  auto rs = theory::onset( "izhikevich", { { "a", 0.02 }, { "b", 0.2 }, { "c", -65.0 }, { "d", 8.0 } }, 0, 30, 600 );
  CHECK_MSG( rs.found && std::fabs( rs.current - 4.0 ) < 0.3 && rs.freq_hz < 15.0, "found " + std::to_string( rs.found ) + " kind " + rs.kind + " current " + std::to_string( rs.current ) + " f " + std::to_string( rs.freq_hz ) );
  auto sn = theory::onset( "izhikevich", { { "a", 0.02 }, { "b", -0.1 }, { "c", -55.0 }, { "d", 6.0 } }, 0, 40, 800 );
  CHECK( sn.found && sn.kind == "saddle-node" && sn.freq_hz == 0.0 );
  auto res = theory::onset( "izhikevich", { { "a", 0.1 }, { "b", 0.26 }, { "c", -65.0 }, { "d", 2.0 } }, 0, 30, 600 );
  CHECK_MSG( res.found && res.kind == "hopf" && res.freq_hz > 15.0, "res f " + std::to_string( res.freq_hz ) + " kind " + res.kind );
  CHECK( !theory::onset( "iaf_psc_alpha", {}, 0, 10 ).found );
  // and the behaviour agrees: the class II cell starts firing abruptly, the class I cell from nothing. 200 ms windows at the current just above each onset
  for ( int cls = 0; cls < 2; ++cls )
  {
    Engine e;
    json p = cls ? json{ { "a", 0.1 }, { "b", 0.26 }, { "c", -65.0 }, { "d", 2.0 } } : json{ { "a", 0.02 }, { "b", 0.2 }, { "c", -65.0 }, { "d", 8.0 } };
    CHECK( e.load( one_cell( "izhikevich", p ).dump() ) );
    e.set_input( "cur", ( cls ? res.current : rs.current ) / 2.0 + 0.5 );
    spikes_in( e, 400 );
    const double hz = spikes_in( e, 1000 );
    CHECK_MSG( cls ? hz >= 25 : hz <= 20, std::string( cls ? "class II" : "class I" ) + " onset rate " + std::to_string( hz ) );
  }
}

static json rate_pair( double w_self, double sigma )
{
  json c = { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 3 },
    { "populations", json::array( { { { "id", "R" }, { "model", "lin_rate_ipn" }, { "n", 1 }, { "params", { { "tau", 10.0 }, { "sigma", sigma }, { "mu", 0.0 } } } } } ) },
    { "projections", json::array( { { { "pre", "R" }, { "post", "R" }, { "rule", "all_to_all" }, { "allow_autapses", true },
      { "synapse", { { "synapse_model", "rate_connection_instantaneous" }, { "weight", w_self } } } } } ) },
    { "inputs", json::array( { { { "name", "drive" }, { "kind", "rate_drive" }, { "targets", json::array( { "R" } ) }, { "weight", 2.0 }, { "value", 0.5 } } } ) },
    { "outputs", json::array( { { { "name", "r" }, { "kind", "rate_value" }, { "neurons", json::array( { "R" } ) } } } ) } };
  return c;
}

static void test_rate_neuron()
{
  // tau dr/dt = -r + mu + w r  ->  r = mu / (1 - w), mu = weight * value = 1
  Engine e;
  CHECK_MSG( e.load( rate_pair( 0.5, 0.0 ).dump() ), e.error() );
  for ( int i = 0; i < 400; ++i ) e.step( 1.0 );
  CHECK_MSG( std::fabs( e.output_value( "r" ) - 2.0 ) < 0.02, "r = " + std::to_string( e.output_value( "r" ) ) );
  e.set_input( "drive", 0.0 );
  for ( int i = 0; i < 400; ++i ) e.step( 1.0 );
  CHECK_MSG( std::fabs( e.output_value( "r" ) ) < 0.02, "r after the drive stops = " + std::to_string( e.output_value( "r" ) ) );
  CHECK( e.output_value( "nope" ) == 0.0 );
  CHECK( e.drain_spikes().empty() );
  const Structure s = e.structure();
  CHECK( s.projections.size() == 1 && s.projections[ 0 ].n_edges == 1 && s.projections[ 0 ].sign == 1 && s.outputs[ 0 ].kind == "rate_value" && s.inputs[ 0 ].kind == "rate_drive" );
  // a spike input onto a rate population, rate_drive onto a spiking one, and a plain synapse onto rate neurons are refused with a message that names the way
  // (Engine::load refuses a bad circuit before NEST sees it, so the same engine can try them one after the other)
  json bad = rate_pair( 0.5, 0.0 );
  bad[ "inputs" ][ 0 ][ "kind" ] = "dc";
  CHECK( !e.load( bad.dump() ) && e.error().find( "rate_drive" ) != std::string::npos );
  bad = rate_pair( 0.5, 0.0 );
  bad[ "projections" ][ 0 ][ "synapse" ][ "synapse_model" ] = "static_synapse";
  CHECK( !e.load( bad.dump() ) && e.error().find( "rate_connection" ) != std::string::npos );
  bad = rate_pair( 0.5, 0.0 );
  bad[ "populations" ][ 0 ][ "model" ] = "iaf_psc_alpha";
  bad[ "populations" ][ 0 ][ "params" ] = json::object();
  CHECK( !e.load( bad.dump() ) );
}

static void test_siegert_projection()
{
  // a siegert_neuron driven by a driver siegert (mean = rate in 1/s) through a diffusion_connection of J and K: drift = tau_m K J / 1000, diffusion = tau_m K J^2 / 1000
  json c = { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 3 },
    { "populations", json::array( { { { "id", "D" }, { "model", "siegert_neuron" }, { "n", 1 }, { "params", { { "mean", 0.0 } } } },
      { { "id", "S" }, { "model", "siegert_neuron" }, { "n", 1 }, { "params", { { "tau_m", 20.0 }, { "t_ref", 2.0 }, { "theta", 20.0 }, { "V_reset", 0.0 }, { "tau", 5.0 } } } } } ) },
    { "projections", json::array( { { { "pre", "D" }, { "post", "S" }, { "rule", "all_to_all" }, { "synapse", { { "synapse_model", "diffusion_connection" }, { "weight", 0.1 }, { "K", 1000 } } } } } ) },
    { "inputs", json::array( { { { "name", "ext" }, { "kind", "rate_drive" }, { "targets", json::array( { "D" } ) }, { "weight", 1.0 }, { "value", 0.0 } } } ) },
    { "outputs", json::array( { { { "name", "s" }, { "kind", "rate_value" }, { "neurons", json::array( { "S" } ) } } } ) } };
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  e.set_input( "ext", 20.0 );   // 20 spikes/s on each of K = 1000 inputs of 0.1 mV: mu = 20e-3 * 20 * 1000 * 0.1 = 40 mV
  for ( int i = 0; i < 300; ++i ) e.step( 1.0 );
  const double hi = e.output_value( "s" );
  CHECK_MSG( hi > 20.0, "mean-field rate " + std::to_string( hi ) );
  e.set_input( "ext", 5.0 );     // mu = 10 mV, sigma^2 = 20e-3 * 5 * 1000 * 0.01 = 1 mV^2: below threshold, a small rate
  for ( int i = 0; i < 300; ++i ) e.step( 1.0 );
  const double lo = e.output_value( "s" );
  CHECK_MSG( lo < 2.0 && lo < hi, "low drive rate " + std::to_string( lo ) );
  const Structure s = e.structure();
  CHECK( s.projections[ 0 ].n_edges == 1 && s.projections[ 0 ].sign == 1 );
  CHECK( e.set_weight( 0, 0, -0.1 ) );
  CHECK( e.structure().projections[ 0 ].sign == -1 );
  json bad = c;
  bad[ "projections" ][ 0 ][ "synapse" ][ "delay" ] = 1.0;
  CHECK( !e.load( bad.dump() ) );
}

static void test_tuned_poisson()
{
  // 8 parrot cells with preferred angles 0, 45, ... 315 degrees: at a stimulus of 90 degrees cell 2 fires most, cell 6 (opposite) least
  json c = { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 3 },
    { "populations", json::array( { { { "id", "A" }, { "model", "parrot_neuron" }, { "n", 8 } } } ) },
    { "inputs", json::array( { { { "name", "stim" }, { "kind", "tuned_poisson" }, { "targets", json::array( { "A" } ) }, { "weight", 1.0 }, { "value", 90.0 }, { "rate_max", 100.0 }, { "rate_base", 5.0 }, { "kappa", 2.0 } } } ) },
    { "outputs", json::array( { { { "name", "all" }, { "kind", "rate" }, { "neurons", json::array( { "A" } ) } } } ) } };
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  std::vector< double > cnt( 8, 0.0 );
  for ( int i = 0; i < 4000; ++i )
  {
    e.step( 1.0 );
    for ( const auto& s : e.drain_spikes() ) cnt[ s.neuron ] += 1.0;
  }
  auto expect = [ & ]( int k, double angle ) { return 5.0 + 95.0 * std::exp( 2.0 * ( std::cos( 2 * 3.14159265358979 * ( angle - 45.0 * k ) / 360.0 ) - 1.0 ) ); };
  for ( int k = 0; k < 8; ++k )
  {
    const double hz = cnt[ k ] / 4.0;
    CHECK_MSG( std::fabs( hz - expect( k, 90.0 ) ) < 0.25 * expect( k, 90.0 ) + 4.0, "cell " + std::to_string( k ) + " " + std::to_string( hz ) + " Hz, expected " + std::to_string( expect( k, 90.0 ) ) );
  }
  CHECK( cnt[ 2 ] > cnt[ 1 ] && cnt[ 2 ] > cnt[ 3 ] && cnt[ 2 ] > 2.0 * cnt[ 6 ] );
  // a new angle re-tunes the population
  e.set_input( "stim", 270.0 );
  std::vector< double > c2( 8, 0.0 );
  for ( int i = 0; i < 4000; ++i )
  {
    e.step( 1.0 );
    for ( const auto& s : e.drain_spikes() ) c2[ s.neuron ] += 1.0;
  }
  CHECK( c2[ 6 ] > c2[ 5 ] && c2[ 6 ] > c2[ 7 ] && c2[ 6 ] > 2.0 * c2[ 2 ] );
}

static void test_sta()
{
  // a stimulus of ramps followed by a spike one step later: the average is the ramp
  std::vector< double > s( 1000, 0.0 );
  std::vector< double > sp;
  for ( int k = 100; k < 900; k += 100 )
  {
    s[ k - 1 ] = 1.0;
    s[ k - 2 ] = 0.5;
    sp.push_back( k );
  }
  auto r = theory::spike_triggered_average( s, 1.0, sp, 5.0, 2.0 );
  CHECK( r.n_used == 8 && r.mean.size() == 8 );
  CHECK( std::fabs( r.mean[ 5 - 1 ] - 1.0 ) < 1e-12 && std::fabs( r.mean[ 5 - 2 ] - 0.5 ) < 1e-12 && std::fabs( r.mean[ 5 ] ) < 1e-12 );
  CHECK( theory::spike_triggered_average( s, 1.0, { 1.0, 999.0 }, 5.0, 2.0 ).n_used == 0 );
}

int main()
{
  test_state_and_phase_plane();
  test_onset();
  test_rate_neuron();
  test_siegert_projection();
  test_tuned_poisson();
  test_sta();
  return chk::finish( "test_theory" );
}
