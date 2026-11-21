// 0.5 engine work (add-only): short-term depression as a projection's synapse_model, and the "modulation" table targeting a projection's
// parameter (Aplysia-style sensitisation). Spike trains come from the new spike_generator input.
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

// Connection parameters of one synapse model, read straight from NEST (the engine owns the kernel).
static std::vector< Dictionary > conns_of( const std::string& model )
{
  Dictionary q;
  q[ "synapse_model" ] = model;
  std::deque< nest::ConnectionID > flat;
  for ( auto& dq : nest::get_connections( q ) )
    flat.insert( flat.end(), dq.begin(), dq.end() );
  return nest::get_connection_status( flat );
}

// spike_generator "pre" -> parrot PRE --(synapse)--> POST (iaf_psc_exp at rest, no noise): the PSP is the cleanest readout of synaptic efficacy.
static json psp_circuit( json syn, json modulation = json::array(), json times = json::array() )
{
  json c = { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 7 } };
  c[ "populations" ] = json::array( { { { "id", "PRE" }, { "model", "parrot_neuron" }, { "n", 1 } },
    { { "id", "POST" }, { "model", "iaf_psc_exp" }, { "n", 1 },
      { "params", { { "E_L", -70.0 }, { "V_m", -70.0 }, { "V_th", -50.0 }, { "C_m", 250.0 }, { "tau_m", 10.0 }, { "tau_syn_ex", 2.0 } } } } } );
  c[ "projections" ] = json::array( { { { "pre", "PRE" }, { "post", "POST" }, { "rule", "one_to_one" }, { "synapse", syn } } } );
  c[ "inputs" ] = json::array( { { { "name", "pre" }, { "kind", "spike_generator" }, { "targets", json::array( { "PRE" } ) }, { "weight", 1.0 }, { "spike_times", times } } } );
  c[ "outputs" ] = json::array( { { { "name", "post" }, { "kind", "rate" }, { "neurons", json::array( { "POST" } ) } } } );
  c[ "modulation" ] = modulation;
  return c;
}

static json tsodyks2( double U, double tau_rec, double tau_fac = 0.0, double weight = 200.0 )
{
  return { { "synapse_model", "tsodyks2_synapse" }, { "weight", weight }, { "delay", 1.0 }, { "U", U }, { "tau_rec", tau_rec }, { "tau_fac", tau_fac } };
}

// Run to model time t_ms (1 ms steps), keeping the largest V_m seen from t_from on; returns max - V at t_from.
struct Probe
{
  Engine& e;
  explicit Probe( Engine& eng ) : e( eng ) { e.watch( 1 ); }
  double t() const { return e.stats().t_ms; }
  void run_to( double t_ms )
  {
    while ( t() < t_ms - 1e-9 )
      e.step( 1.0 );
  }
  // One PSP: queue a presynaptic spike at t_spike, then record up to t_spike + 30 ms.
  double psp( double t_spike )
  {
    CHECK_MSG( e.queue_spikes( "pre", { t_spike } ), e.error() );
    run_to( t_spike - 1.0 );
    const double v0 = e.voltage( 1 );
    double peak = v0;
    while ( t() < t_spike + 30.0 - 1e-9 )
    {
      e.step( 1.0 );
      peak = std::max( peak, e.voltage( 1 ) );
    }
    CHECK_MSG( e.last_step_ok(), e.error() );
    return peak - v0;
  }
};

static void test_depression_and_recovery()
{
  Engine e;
  CHECK_MSG( e.load( psp_circuit( tsodyks2( 0.5, 800.0 ) ).dump() ), e.error() );
  Probe p( e );
  // 20 Hz train (50 ms apart) of 8 spikes, then 3 s of silence, then one test spike
  std::vector< double > a;
  for ( int i = 0; i < 8; ++i )
    a.push_back( p.psp( 10.0 + 50.0 * i ) );
  const double after_pause = p.psp( 10.0 + 50.0 * 7 + 3000.0 );
  std::printf( "  tsodyks2 U=0.5 tau_rec=800: PSP (mV) at 20 Hz:" );
  for ( double x : a )
    std::printf( " %.3f", x );
  std::printf( " | after 3 s pause %.3f\n", after_pause );
  CHECK( a[ 0 ] > 0.2 );  // a real, readable PSP
  for ( int i = 1; i < 5; ++i )
    CHECK( a[ i ] < a[ i - 1 ] );          // each spike of the train is weaker than the one before
  CHECK( a[ 4 ] < 0.65 * a[ 0 ] );          // and the depression is large
  CHECK( a[ 7 ] < 0.65 * a[ 0 ] );
  CHECK( after_pause > 0.9 * a[ 0 ] );      // recovered after the pause (x back to ~ 1 - 0.5 * exp(-3000/800) of rest)
  CHECK( after_pause > 1.4 * a[ 7 ] );
  // the parameters reached the connection
  auto cs = conns_of( "tsodyks2_synapse" );
  CHECK( cs.size() == 1 );
  if ( cs.size() == 1 )
  {
    CHECK( std::fabs( cs[ 0 ].get< double >( "U" ) - 0.5 ) < 1e-12 );
    CHECK( std::fabs( cs[ 0 ].get< double >( "tau_rec" ) - 800.0 ) < 1e-12 );
    CHECK( std::fabs( cs[ 0 ].get< double >( "tau_fac" ) ) < 1e-12 );
  }
}

static void test_facilitation_and_x_u()
{
  // the same projection with a small U and tau_fac = 500 facilitates (second PSP larger than the first); u and x are initial states
  {
    Engine e;
    json syn = tsodyks2( 0.1, 100.0, 500.0 );
    syn[ "u" ] = 0.1;
    syn[ "x" ] = 1.0;
    CHECK_MSG( e.load( psp_circuit( syn ).dump() ), e.error() );
    Probe p( e );
    const double a0 = p.psp( 10.0 ), a1 = p.psp( 60.0 );
    std::printf( "  tsodyks2 U=0.1 tau_fac=500: PSP %.3f then %.3f mV\n", a0, a1 );
    CHECK( a1 > 1.3 * a0 );
  }
  // x = 0.5 at the start halves the first PSP relative to x = 1 (same U)
  auto first = [ & ]( double x ) {
    Engine f;
    json s = tsodyks2( 0.5, 800.0 );
    s[ "x" ] = x;
    CHECK_MSG( f.load( psp_circuit( s ).dump() ), f.error() );
    Probe q( f );
    return q.psp( 10.0 );
  };
  const double full = first( 1.0 ), half = first( 0.5 );
  CHECK( std::fabs( half / full - 0.5 ) < 0.03 );
}

static void test_tsodyks_original()
{
  // tsodyks_synapse (the older model) is accepted with its own parameters and also depresses
  json syn = { { "synapse_model", "tsodyks_synapse" }, { "weight", 400.0 }, { "delay", 1.0 }, { "U", 0.5 }, { "tau_rec", 800.0 }, { "tau_fac", 0.0 }, { "tau_psc", 3.0 } };
  {
    Engine e;
    CHECK_MSG( e.load( psp_circuit( syn ).dump() ), e.error() );
    Probe p( e );
    double a[ 4 ];
    for ( int i = 0; i < 4; ++i )
      a[ i ] = p.psp( 10.0 + 50.0 * i );
    std::printf( "  tsodyks_synapse: PSP %.3f %.3f %.3f %.3f mV\n", a[ 0 ], a[ 1 ], a[ 2 ], a[ 3 ] );
    CHECK( a[ 0 ] > 0.1 and a[ 1 ] < a[ 0 ] and a[ 2 ] < a[ 1 ] and a[ 3 ] < a[ 2 ] );
  }
  json bad = syn;
  bad[ "tau_psc" ] = 0.0;
  Engine f;
  CHECK( not f.load( psp_circuit( bad ).dump() ) and has( f.error(), "tau_psc" ) );
}

// The efficacy of one fully recovered synapse: PSP of a spike at t after a long silence.
static void test_sensitisation_U()
{
  json mod = json::array( { { { "modulator", "NE" }, { "projection", 0 }, { "param", "U" }, { "gain", 0.4 }, { "min", 0.0 }, { "max", 1.0 } } } );
  // NEST's tsodyks2 starts from the connection's own `u` (default 0.5): the first spike of a connection uses it, later spikes use U (tau_fac = 0).
  // With u = U the first PSP is weight * U, so the baseline here is the same efficacy the table modulates.
  json syn = tsodyks2( 0.2, 300.0 );
  syn[ "u" ] = 0.2;
  Engine e;
  CHECK_MSG( e.load( psp_circuit( syn, mod ).dump() ), e.error() );
  CHECK( conns_of( "tsodyks2_synapse" ).size() == 1 );
  Probe p( e );
  const double a_low = p.psp( 10.0 );  // NE = 0: U = 0.2
  CHECK( std::fabs( conns_of( "tsodyks2_synapse" )[ 0 ].get< double >( "U" ) - 0.2 ) < 1e-12 );

  // between steps: set_modulator changes nothing until the next step() starts
  e.set_modulator( "NE", 1.0 );
  CHECK( std::fabs( conns_of( "tsodyks2_synapse" )[ 0 ].get< double >( "U" ) - 0.2 ) < 1e-12 );
  const double a_high = p.psp( 2000.0 );  // NE = 1: U = 0.6, same law as the neuron table: value + gain * conc
  CHECK( std::fabs( conns_of( "tsodyks2_synapse" )[ 0 ].get< double >( "U" ) - 0.6 ) < 1e-12 );
  std::printf( "  sensitisation (gain 0.4 on U): PSP %.3f mV at NE=0, %.3f mV at NE=1 (x%.2f)\n", a_low, a_high, a_high / a_low );
  CHECK( std::fabs( a_high / a_low - 3.0 ) < 0.15 );  // U 0.2 -> 0.6 with the resources fully recovered

  // the clamp: NE = 5 would give U = 2.2, the table's max keeps it at 1
  e.set_modulator( "NE", 5.0 );
  const double a_max = p.psp( 4000.0 );
  CHECK( std::fabs( conns_of( "tsodyks2_synapse" )[ 0 ].get< double >( "U" ) - 1.0 ) < 1e-12 );
  CHECK( a_max > a_high );
  // and back: the table reapplies from the base value, so the efficacy returns to the baseline
  e.set_modulator( "NE", 0.0 );
  const double a_back = p.psp( 6000.0 );
  CHECK( std::fabs( a_back / a_low - 1.0 ) < 0.02 );
  CHECK( std::fabs( e.modulator( "NE" ) ) < 1e-12 );
}

static void test_sensitisation_weight_and_dishabituation()
{
  // multiplicative law on the weight (scale_per_unit): weight * (1 + 1.0 * conc). A depressed synapse (habituated by a 20 Hz train)
  // answers the next spike with a larger PSP once the modulator is raised: Aplysia dishabituation / sensitisation.
  json mod = json::array( { { { "modulator", "ACh" }, { "projection", 0 }, { "param", "weight" }, { "scale_per_unit", 1.0 } } } );
  {
    Engine e;
    CHECK_MSG( e.load( psp_circuit( tsodyks2( 0.5, 800.0 ), mod ).dump() ), e.error() );
    Probe p( e );
    std::vector< double > a;
    for ( int i = 0; i < 6; ++i )
      a.push_back( p.psp( 10.0 + 50.0 * i ) );
    CHECK( a[ 5 ] < 0.65 * a[ 0 ] );               // habituated
    e.set_modulator( "ACh", 1.0 );                  // the "tail shock"
    const double shocked = p.psp( 10.0 + 50.0 * 6 );
    std::printf( "  habituated PSP %.3f mV -> %.3f mV after modulator (weight x2)\n", a[ 5 ], shocked );
    CHECK( shocked > 1.6 * a[ 5 ] );                // the weight doubled; resources barely changed within one interval
    auto cs = conns_of( "tsodyks2_synapse" );
    CHECK( cs.size() == 1 and std::fabs( cs[ 0 ].get< double >( "weight" ) - 400.0 ) < 1e-9 );
  }

  // the weight ceiling of the postsynaptic cell holds for a modulated weight: no divergence from a huge gain
  json huge = json::array( { { { "modulator", "DA" }, { "projection", 0 }, { "param", "weight" }, { "gain", 1e9 } } } );
  Engine f;
  CHECK_MSG( f.load( psp_circuit( tsodyks2( 0.5, 800.0 ), huge ).dump() ), f.error() );
  f.set_modulator( "DA", 1.0 );
  f.step( 1.0 );
  CHECK_MSG( f.last_step_ok(), f.error() );
  auto cf = conns_of( "tsodyks2_synapse" );
  CHECK( cf.size() == 1 and std::fabs( cf[ 0 ].get< double >( "weight" ) ) <= 10000.0 );
}

static void test_modulation_errors()
{
  auto err = [ & ]( json mod, json syn = tsodyks2( 0.5, 800.0 ) ) {
    Engine e;
    return e.load( psp_circuit( syn, mod ).dump() ) ? std::string() : e.error();
  };
  json row = { { "modulator", "NE" }, { "projection", 0 }, { "param", "U" }, { "gain", 0.1 } };
  CHECK_MSG( err( json::array( { row } ) ).empty(), err( json::array( { row } ) ) );
  json r;
  r = row; r[ "projection" ] = 3;
  std::string e = err( json::array( { r } ) );
  CHECK_MSG( has( e, "modulation[0].projection" ) and has( e, "projection number" ), e );
  r = row; r[ "pop" ] = "POST";
  e = err( json::array( { r } ) );
  CHECK_MSG( has( e, "exactly one of 'pop'" ), e );
  r = row; r.erase( "projection" );
  e = err( json::array( { r } ) );
  CHECK_MSG( has( e, "exactly one of 'pop'" ), e );
  r = row; r[ "param" ] = "no_such_param";
  e = err( json::array( { r } ) );
  CHECK_MSG( has( e, "modulation[0].param" ) and has( e, "no_such_param" ), e );
  r = row; r[ "param" ] = "delay";
  e = err( json::array( { r } ) );
  CHECK_MSG( has( e, "cannot be modulated" ), e );
  r = row; r[ "param" ] = "weight";
  e = err( json::array( { r } ), { { "synapse_model", "stdp_synapse" }, { "weight", 5.0 }, { "delay", 1.0 } } );
  CHECK_MSG( has( e, "plasticity rule" ), e );
  r = row; r[ "extra" ] = 1;
  e = err( json::array( { r } ) );
  CHECK_MSG( has( e, "unknown key" ), e );
  // a neuron-parameter row still works next to a synapse row, and the old shape is unchanged
  json both = json::array( { row, { { "modulator", "NE" }, { "pop", "POST" }, { "param", "I_e" }, { "gain", 10.0 } } } );
  CHECK_MSG( err( both ).empty(), err( both ) );
  // two projections between the same populations with the same synapse model are accepted: each keeps its own synapse model copy, so a row modulates only its projection
  json c = psp_circuit( tsodyks2( 0.5, 800.0 ), json::array( { row } ) );
  c[ "projections" ].push_back( c[ "projections" ][ 0 ] );
  Engine f;
  CHECK_MSG( f.load( c.dump() ), f.error() );
}

int main()
{
  test_depression_and_recovery();
  test_facilitation_and_x_u();
  test_tsodyks_original();
  test_sensitisation_U();
  test_sensitisation_weight_and_dishabituation();
  test_modulation_errors();
  return chk::finish( "test_synapse_modulation" );
}
