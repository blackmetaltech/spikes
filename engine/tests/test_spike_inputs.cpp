// 0.5 engine work (add-only): spike-train input kinds "spike_generator" (spike_times in the circuit, Engine::queue_spikes) and
// "sinusoidal_poisson" (rate, amplitude, freq_hz, phase; Engine::set_input_param), on the resolution grid.
#include <cmath>
#include <string>
#include <vector>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static bool has( const std::string& s, const char* sub ) { return s.find( sub ) != std::string::npos; }

static json circuit( json input, int n_cells = 1, double res = 0.1 )
{
  json c = { { "schema_version", 1 }, { "resolution_ms", res }, { "seed", 11 } };
  c[ "populations" ] = json::array( { { { "id", "P" }, { "model", "parrot_neuron" }, { "n", n_cells } } } );
  input[ "targets" ] = json::array( { "P" } );
  input[ "weight" ] = 1.0;
  c[ "inputs" ] = json::array( { input } );
  c[ "outputs" ] = json::array( { { { "name", "o" }, { "kind", "rate" }, { "neurons", json::array( { "P" } ) } } } );
  return c;
}

static std::string load_error( const json& c )
{
  Engine e;
  return e.load( c.dump() ) ? std::string() : e.error();
}

static std::vector< double > times_of( const std::vector< Spike >& s )
{
  std::vector< double > t;
  for ( const Spike& x : s )
    t.push_back( x.t_ms );
  return t;
}

static void test_spike_generator()
{
  // JSON spike_times: the parrot relays each spike once, spaced exactly as given (grid times, no jitter)
  {
    Engine e;
    json in = { { "name", "g" }, { "kind", "spike_generator" }, { "spike_times", json::array( { 5.0, 10.0, 12.5 } ) } };
    CHECK_MSG( e.load( circuit( in ).dump() ), e.error() );
    e.step( 50.0 );
    CHECK_MSG( e.last_step_ok(), e.error() );
    auto t = times_of( e.drain_spikes() );
    CHECK( t.size() == 3 );
    if ( t.size() == 3 )
    {
      CHECK( std::fabs( ( t[ 1 ] - t[ 0 ] ) - 5.0 ) < 1e-9 and std::fabs( ( t[ 2 ] - t[ 1 ] ) - 2.5 ) < 1e-9 );
      CHECK( t[ 0 ] >= 5.0 and t[ 0 ] < 5.5 );
    }
    CHECK( e.output_count( "o" ) == 3.0 );
    // the input is a spike_generator: it is not set_input()-able, and it is listed with its kind
    e.set_input( "g", 100.0 );
    CHECK( has( e.error(), "queue_spikes" ) );
    CHECK( e.inputs().size() == 1 and e.inputs()[ 0 ].kind == "spike_generator" );
  }
  // queue_spikes: appended between steps, absolute model time; unsorted input is fine; spikes already in the circuit are kept
  {
    Engine e;
    json in = { { "name", "g" }, { "kind", "spike_generator" }, { "spike_times", json::array( { 5.0 } ) } };
    CHECK_MSG( e.load( circuit( in ).dump() ), e.error() );
    e.step( 20.0 );
    CHECK( e.output_count( "o" ) == 1.0 );
    CHECK_MSG( e.queue_spikes( "g", { 40.0, 30.0, 31.5 } ), e.error() );
    CHECK_MSG( e.queue_spikes( "g", { 60.0 } ), e.error() );  // a second call adds to the first
    e.step( 60.0 );        // now 80 ms
    CHECK_MSG( e.last_step_ok(), e.error() );
    auto t = times_of( e.drain_spikes() );
    CHECK( t.size() == 5 );
    if ( t.size() == 5 )
    {
      CHECK( std::fabs( t[ 1 ] - t[ 0 ] - 25.0 ) < 1.0 );  // 5 -> 30
      CHECK( std::fabs( ( t[ 2 ] - t[ 1 ] ) - 1.5 ) < 1e-9 and std::fabs( ( t[ 3 ] - t[ 2 ] ) - 8.5 ) < 1e-9 and std::fabs( ( t[ 4 ] - t[ 3 ] ) - 20.0 ) < 1e-9 );
    }
    CHECK( e.output_count( "o" ) == 4.0 );  // last step only
    // refused: in the past, not after the time reached, off the grid, unknown channel, wrong kind of channel
    CHECK( not e.queue_spikes( "g", { 70.0 } ) and has( e.error(), "not after" ) );
    CHECK( not e.queue_spikes( "g", { 80.0 } ) and has( e.error(), "not after" ) );
    CHECK( not e.queue_spikes( "g", { 90.05 } ) and has( e.error(), "multiple of the resolution" ) );
    CHECK( not e.queue_spikes( "nope", { 90.0 } ) and has( e.error(), "unknown input channel" ) );
    CHECK( e.queue_spikes( "g", {} ) );
    // nothing was half-applied by the refusals: the next step produces no extra spike
    e.step( 20.0 );
    CHECK( e.output_count( "o" ) == 0.0 );
  }
  // an empty spike_generator (no spike_times) loads, is silent, and is fed by queue_spikes from the first step
  {
    Engine e;
    json in = { { "name", "g" }, { "kind", "spike_generator" } };
    CHECK_MSG( e.load( circuit( in ).dump() ), e.error() );
    CHECK( e.queue_spikes( "g", { 0.1, 0.2 } ) );  // the first grid steps are valid before anything ran
    e.step( 5.0 );
    CHECK( e.output_count( "o" ) == 2.0 );
    CHECK( e.structure().inputs.size() == 1 and e.structure().inputs[ 0 ].kind == "spike_generator" );
  }
  // the resolution grid: any resolution_ms works, off-grid times are errors that name the path
  {
    json in = { { "name", "g" }, { "kind", "spike_generator" }, { "spike_times", json::array( { 1.0, 2.5 } ) } };
    CHECK_MSG( load_error( circuit( in, 1, 0.5 ) ).empty(), load_error( circuit( in, 1, 0.5 ) ) );
    in[ "spike_times" ] = json::array( { 1.0, 1.2 } );
    std::string e = load_error( circuit( in, 1, 0.5 ) );
    CHECK_MSG( has( e, "inputs[0].spike_times[1]" ) and has( e, "multiple of resolution_ms" ), e );
    in[ "spike_times" ] = json::array( { 2.0, 1.0 } );
    e = load_error( circuit( in ) );
    CHECK_MSG( has( e, "spike_times[1]" ) and has( e, "sorted" ), e );
    in[ "spike_times" ] = json::array( { 0.0 } );
    e = load_error( circuit( in ) );
    CHECK_MSG( has( e, "spike_times[0]" ) and has( e, "> 0" ), e );
    in[ "spike_times" ] = "x";
    e = load_error( circuit( in ) );
    CHECK_MSG( has( e, "inputs[0].spike_times" ) and has( e, "array" ), e );
    in[ "spike_times" ] = json::array( { 1.0 } );
    in[ "value" ] = 3.0;
    e = load_error( circuit( in ) );
    CHECK_MSG( has( e, "inputs[0].value" ), e );
    json dc = { { "name", "g" }, { "kind", "dc" }, { "spike_times", json::array( { 1.0 } ) } };
    e = load_error( circuit( dc ) );
    CHECK_MSG( has( e, "inputs[0].spike_times" ) and has( e, "spike_generator" ), e );
    json bad = { { "name", "g" }, { "kind", "foo" } };
    e = load_error( circuit( bad ) );
    CHECK_MSG( has( e, "spike_generator" ) and has( e, "sinusoidal_poisson" ), e );
  }
  // float times that are not exactly representable (0.3 / 0.1 = 2.9999999999999996 steps): accepted and delivered once
  {
    Engine e;
    json in = { { "name", "g" }, { "kind", "spike_generator" }, { "spike_times", json::array( { 0.3, 0.7 } ) } };
    CHECK_MSG( e.load( circuit( in ).dump() ), e.error() );
    e.step( 3.0 );
    CHECK( e.output_count( "o" ) == 2.0 );
  }
}

// counts of a rate output over `steps` steps of 10 ms: [first half-period, second half-period] of a 10 Hz (100 ms) sinusoid, summed over the run
struct Halves
{
  double a = 0, b = 0, all = 0;
};

static Halves run_halves( Engine& e, int periods )
{
  Halves h;
  for ( int p = 0; p < periods; ++p )
    for ( int k = 0; k < 10; ++k )  // ten 10 ms steps per 100 ms period
    {
      e.step( 10.0 );
      const double n = e.output_count( "o" );
      ( k < 5 ? h.a : h.b ) += n;
      h.all += n;
    }
  return h;
}

static json sine( double rate, double amp, double f, double phase )
{
  return { { "name", "s" }, { "kind", "sinusoidal_poisson" }, { "rate", rate }, { "amplitude", amp }, { "freq_hz", f }, { "phase", phase } };
}

static void test_sinusoidal_poisson()
{
  const int N = 200;  // individual spike trains per target: 200 parrots average out the Poisson noise
  {
    Engine e;
    CHECK_MSG( e.load( circuit( sine( 100.0, 100.0, 10.0, 0.0 ), N ).dump() ), e.error() );
    Halves h = run_halves( e, 5 );
    // rate(t) = 100 + 100 sin(2 pi 10 t): mean 163.7 Hz over the first half-period and 36.3 Hz over the second
    std::printf( "  sinusoidal_poisson 100 Hz +- 100 Hz @ 10 Hz, phase 0: first half %.0f spikes, second half %.0f (expect ~8185 / ~1815)\n", h.a, h.b );
    CHECK( std::fabs( h.a / 8185.0 - 1.0 ) < 0.06 and std::fabs( h.b / 1815.0 - 1.0 ) < 0.10 );
    CHECK( std::fabs( h.all / ( N * 0.5 * 100.0 ) - 1.0 ) < 0.05 );  // the mean rate is `rate`
    // between steps, with the engine running: phase 180 swaps the halves (set_input_param re-prepares NEST)
    CHECK_MSG( e.set_input_param( "s", "phase", 180.0 ), e.error() );
    Halves g = run_halves( e, 5 );
    std::printf( "    after set_input_param phase=180: %.0f / %.0f\n", g.a, g.b );
    CHECK( g.b > 3.0 * g.a );
    // amplitude 0: flat
    CHECK( e.set_input_param( "s", "amplitude", 0.0 ) );
    Halves f = run_halves( e, 5 );
    std::printf( "    after amplitude=0: %.0f / %.0f\n", f.a, f.b );
    CHECK( std::fabs( f.a / f.b - 1.0 ) < 0.12 );
    CHECK( std::fabs( f.all / ( N * 0.5 * 100.0 ) - 1.0 ) < 0.05 );
    // set_input sets the mean rate (like the Poisson input): 100 -> 200 Hz doubles the count
    e.set_input( "s", 200.0 );
    Halves d = run_halves( e, 5 );
    CHECK( std::fabs( d.all / f.all - 2.0 ) < 0.15 );
    // frequency: 20 Hz fills both 50 ms halves equally (two full cycles per half... a flat average over the half-periods)
    CHECK( e.set_input_param( "s", "amplitude", 100.0 ) and e.set_input_param( "s", "freq_hz", 20.0 ) and e.set_input_param( "s", "phase", 0.0 ) );
    e.set_input( "s", 100.0 );
    Halves q = run_halves( e, 5 );
    CHECK( std::fabs( q.a / q.b - 1.0 ) < 0.12 );
    CHECK_MSG( e.last_step_ok(), e.error() );
    // errors
    CHECK( not e.set_input_param( "s", "bogus", 1.0 ) and has( e.error(), "unknown parameter" ) );
    CHECK( not e.set_input_param( "s", "amplitude", -1.0 ) );
    CHECK( not e.set_input_param( "nope", "phase", 1.0 ) );
    CHECK( not e.set_input_param( "s", "freq_hz", 1e6 ) and has( e.error(), "Nyquist" ) );
  }
  // same circuit, same seed: identical spikes (deterministic)
  {
    auto run = [ & ]() {
      Engine e;
      CHECK_MSG( e.load( circuit( sine( 50.0, 50.0, 8.0, 90.0 ), 20 ).dump() ), e.error() );
      e.step( 300.0 );
      return e.stats().checksum;
    };
    CHECK( run() == run() );
  }
  // errors in the JSON
  {
    std::string e = load_error( circuit( sine( -1.0, 10.0, 5.0, 0.0 ) ) );
    CHECK_MSG( has( e, "inputs[0].rate" ), e );
    e = load_error( circuit( sine( 10.0, -1.0, 5.0, 0.0 ) ) );
    CHECK_MSG( has( e, "inputs[0].amplitude" ), e );
    e = load_error( circuit( sine( 10.0, 1.0, 5000.0, 0.0 ) ) );  // Nyquist at 0.1 ms is 5000 Hz
    CHECK_MSG( has( e, "inputs[0].freq_hz" ), e );
    json bad = sine( 10.0, 1.0, 5.0, 0.0 );
    bad[ "value" ] = 3.0;
    e = load_error( circuit( bad ) );
    CHECK_MSG( has( e, "inputs[0].value" ) and has( e, "rate" ), e );
    json p = { { "name", "p" }, { "kind", "poisson" }, { "amplitude", 1.0 } };
    e = load_error( circuit( p ) );
    CHECK_MSG( has( e, "inputs[0].amplitude" ) and has( e, "sinusoidal_poisson" ), e );
  }
  // the structure report lists the new kinds with their target populations
  {
    Engine e;
    CHECK( e.load( circuit( sine( 10.0, 5.0, 5.0, 0.0 ), 3 ).dump() ) );
    auto st = e.structure();
    CHECK( st.inputs.size() == 1 and st.inputs[ 0 ].kind == "sinusoidal_poisson" and st.inputs[ 0 ].pops == std::vector< std::string >{ "P" } and st.inputs[ 0 ].neurons.size() == 3 );
  }
}

int main()
{
  test_spike_generator();
  test_sinusoidal_poisson();
  return chk::finish( "test_spike_inputs" );
}
