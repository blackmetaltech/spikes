// 0.5 engine work (add-only): output kind "population_vector" (bump angle and strength over an ordered ring), driven by hand.
#include <cmath>
#include <string>
#include <vector>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static const double kPi = 3.14159265358979323846;
static bool has( const std::string& s, const char* sub ) { return s.find( sub ) != std::string::npos; }

// N parrot cells on a ring, one spike_generator channel per cell ("c0".."c{N-1}") so a test places an exact number of spikes on each cell.
static json ring_circuit( int n, json neurons_sel = nullptr )
{
  json c = { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 5 } };
  c[ "populations" ] = json::array( { { { "id", "RING" }, { "model", "parrot_neuron" }, { "n", n } } } );
  json ins = json::array();
  for ( int k = 0; k < n; ++k )
    ins.push_back( { { "name", "c" + std::to_string( k ) }, { "kind", "spike_generator" }, { "targets", json::array( { { { "pop", "RING" }, { "idx", json::array( { k } ) } } } ) }, { "weight", 1.0 } } );
  c[ "inputs" ] = ins;
  c[ "outputs" ] = json::array( { { { "name", "heading" }, { "kind", "population_vector" }, { "neurons", neurons_sel.is_null() ? json::array( { "RING" } ) : neurons_sel } },
    { { "name", "all" }, { "kind", "rate" }, { "neurons", json::array( { "RING" } ) } } } );
  return c;
}

// Place counts[k] spikes (distinct grid times, 0.5 ms apart) on cell k within the next `window` ms and run the window as one step.
static void drive(Engine& e, const std::vector<int>& counts, double window = 100.0)
{
  const double t0 = e.stats().t_ms;
  for ( size_t k = 0; k < counts.size(); ++k )
  {
    std::vector< double > t;
    for ( int i = 0; i < counts[ k ]; ++i )
      t.push_back( t0 + 1.0 + 0.5 * i );
    CHECK_MSG( e.queue_spikes( "c" + std::to_string( k ), t ), e.error() );
  }
  e.step( window );
  CHECK_MSG( e.last_step_ok(), e.error() );
}

// half-rectified cosine bump of the given width, peak count `peak`, centred on ring position `centre` (in cell units, may be fractional)
static std::vector< int > bump( int n, double centre, int peak )
{
  std::vector< int > c( n );
  for ( int k = 0; k < n; ++k )
    c[ k ] = static_cast< int >( std::lround( peak * std::max( 0.0, std::cos( 2.0 * kPi * ( k - centre ) / n ) ) ) );
  return c;
}

static double circ_diff( double a, double b )
{
  double d = std::fmod( a - b + kPi, 2.0 * kPi );
  if ( d < 0 )
    d += 2.0 * kPi;
  return d - kPi;
}

static void test_hand_driven_bump()
{
  const int N = 16;
  Engine e;
  CHECK_MSG( e.load( ring_circuit( N ).dump() ), e.error() );
  CHECK( e.outputs().size() == 2 and e.outputs()[ 0 ].kind == "population_vector" and e.outputs()[ 0 ].neurons.size() == N );

  // before any step, and with no spikes: no direction, no strength (not NaN)
  CHECK( e.output_angle( "heading" ) == 0.0 and e.output_strength( "heading" ) == 0.0 );
  drive( e, std::vector< int >( N, 0 ) );
  CHECK( e.output_angle( "heading" ) == 0.0 and e.output_strength( "heading" ) == 0.0 and e.output_count( "heading" ) == 0.0 );

  // a bump centred on cell 4 (angle 2 pi 4 / 16 = pi / 2): the readout points there, from a broad profile
  drive( e, bump( N, 4.0, 20 ) );
  double a = e.output_angle( "heading" ), s = e.output_strength( "heading" );
  std::printf( "  bump at cell 4: angle %.4f rad (expect %.4f), strength %.3f\n", a, kPi / 2, s );
  CHECK( std::fabs( circ_diff( a, kPi / 2 ) ) < 1e-6 );
  CHECK( s > 0.5 and s < 1.0 );
  const double s_broad = s;

  // a bump between two cells (3.5 -> angle 2 pi 3.5 / 16): symmetric profile, so the angle is exact
  drive( e, bump( N, 3.5, 20 ) );
  CHECK( std::fabs( circ_diff( e.output_angle( "heading" ), 2.0 * kPi * 3.5 / N ) ) < 1e-6 );

  // sweep the bump round the ring (including the wrap at angle 0 / 2 pi): the angle follows, in [0, 2 pi)
  double prev = -1;
  for ( int k = 0; k < N; ++k )
  {
    drive( e, bump( N, k, 20 ) );
    const double ang = e.output_angle( "heading" );
    CHECK( ang >= 0.0 and ang < 2.0 * kPi );
    CHECK_MSG( std::fabs( circ_diff( ang, 2.0 * kPi * k / N ) ) < 1e-6, "k=" + std::to_string( k ) );
    if ( k > 0 and k < N )
      CHECK( ang > prev );  // monotonic round the ring
    prev = ang;
  }

  // a sharper bump (fewer cells fire) has a longer vector; one cell alone gives strength 1; a flat profile gives 0
  std::vector< int > narrow( N, 0 );
  narrow[ 4 ] = 20;
  narrow[ 3 ] = narrow[ 5 ] = 8;
  drive( e, narrow );
  const double s_narrow = e.output_strength( "heading" );
  CHECK( s_narrow > s_broad and std::fabs( circ_diff( e.output_angle( "heading" ), kPi / 2 ) ) < 1e-6 );
  std::vector< int > one( N, 0 );
  one[ 11 ] = 7;
  drive( e, one );
  CHECK( std::fabs( e.output_strength( "heading" ) - 1.0 ) < 1e-9 and std::fabs( circ_diff( e.output_angle( "heading" ), 2.0 * kPi * 11 / N ) ) < 1e-9 );
  drive( e, std::vector< int >( N, 5 ) );
  CHECK( e.output_strength( "heading" ) < 1e-9 );
  CHECK( e.output_count( "heading" ) == 80.0 );
  // two opposite bumps cancel (strength ~ 0); unequal ones leave the larger direction
  std::vector< int > two( N, 0 );
  two[ 2 ] = 10;
  two[ 10 ] = 10;
  drive( e, two );
  CHECK( e.output_strength( "heading" ) < 1e-9 );
  two[ 2 ] = 15;
  drive( e, two );
  CHECK( std::fabs( circ_diff( e.output_angle( "heading" ), 2.0 * kPi * 2 / N ) ) < 1e-6 and std::fabs( e.output_strength( "heading" ) - 0.2 ) < 1e-9 );

  // other outputs and unknown names read 0 through the new accessors
  CHECK( e.output_angle( "all" ) == 0.0 and e.output_strength( "all" ) == 0.0 );
  CHECK( e.output_angle( "nope" ) == 0.0 and e.output_strength( "nope" ) == 0.0 );
  // the output still counts spikes like a rate output
  CHECK( e.output_rate( "heading" ) == e.output_rate( "all" ) );
}

static void test_ring_order()
{
  // the ring order is the order of the selector: listing the cells reversed mirrors the angle (a population_vector is ordered, never sorted)
  const int N = 8;
  json rev = json::array();
  rev.push_back( { { "pop", "RING" }, { "idx", json::array( { 7, 6, 5, 4, 3, 2, 1, 0 } ) } } );
  {
    Engine e;
    CHECK_MSG( e.load( ring_circuit( N, rev ).dump() ), e.error() );
    std::vector< int > one( N, 0 );
    one[ 1 ] = 10;  // cell 1 is ring position 6 in the reversed list
    drive( e, one );
    CHECK( std::fabs( circ_diff( e.output_angle( "heading" ), 2.0 * kPi * 6 / N ) ) < 1e-9 );
  }
  // a half ring: 4 cells of the 8, evenly spaced over the full circle (the ring is the listed cells, not the population)
  {
    Engine f;
    json half = json::array( { { { "pop", "RING" }, { "idx", json::array( { 0, 1, 2, 3 } ) } } } );
    CHECK_MSG( f.load( ring_circuit( N, half ).dump() ), f.error() );
    std::vector< int > c( N, 0 );
    c[ 1 ] = 5;
    drive( f, c );
    CHECK( std::fabs( circ_diff( f.output_angle( "heading" ), 2.0 * kPi * 1 / 4 ) ) < 1e-9 );
  }
  // repeats are an error
  {
    json dup = json::array( { { { "pop", "RING" }, { "idx", json::array( { 0, 1, 1 } ) } } } );
    Engine g;
    CHECK( not g.load( ring_circuit( N, dup ).dump() ) );
    CHECK_MSG( has( g.error(), "outputs[0].neurons" ) and has( g.error(), "once" ), g.error() );
  }
  // the other output keys stay invalid for this kind
  json bad = ring_circuit( N );
  bad[ "outputs" ][ 0 ][ "gain" ] = 1.0;
  Engine h;
  CHECK( not h.load( bad.dump() ) );
}

static void test_poisson_driven_bump_and_structure()
{
  // a bump of Poisson rates (peak 120 Hz) on 16 parrots: the angle comes out within a few degrees of the centre, over a 500 ms step
  const int N = 16;
  json c = { { "schema_version", 1 }, { "resolution_ms", 0.1 }, { "seed", 9 } };
  c[ "populations" ] = json::array( { { { "id", "RING" }, { "model", "parrot_neuron" }, { "n", N } } } );
  json ins = json::array();
  for ( int k = 0; k < N; ++k )
    ins.push_back( { { "name", "c" + std::to_string( k ) }, { "kind", "poisson" }, { "targets", json::array( { { { "pop", "RING" }, { "idx", json::array( { k } ) } } } ) },
      { "weight", 1.0 }, { "value", 0.0 } } );
  c[ "inputs" ] = ins;
  c[ "outputs" ] = json::array( { { { "name", "heading" }, { "kind", "population_vector" }, { "neurons", json::array( { "RING" } ) } } } );
  Engine e;
  CHECK_MSG( e.load( c.dump() ), e.error() );
  const double centre = 10.0;  // cell units
  for ( int k = 0; k < N; ++k )
    e.set_input( "c" + std::to_string( k ), 20.0 + 120.0 * std::max( 0.0, std::cos( 2.0 * kPi * ( k - centre ) / N ) ) );
  e.step( 500.0 );
  CHECK_MSG( e.last_step_ok(), e.error() );
  const double want = 2.0 * kPi * centre / N;
  std::printf( "  poisson bump at cell 10: angle %.3f rad (expect %.3f), strength %.3f\n", e.output_angle( "heading" ), want, e.output_strength( "heading" ) );
  CHECK( std::fabs( circ_diff( e.output_angle( "heading" ), want ) ) < 0.15 );
  CHECK( e.output_strength( "heading" ) > 0.3 and e.output_strength( "heading" ) < 0.9 );
  // the bump moves: rotate by 90 degrees (4 cells)
  for ( int k = 0; k < N; ++k )
    e.set_input( "c" + std::to_string( k ), 20.0 + 120.0 * std::max( 0.0, std::cos( 2.0 * kPi * ( k - centre - 4 ) / N ) ) );
  e.step( 500.0 );
  CHECK( std::fabs( circ_diff( e.output_angle( "heading" ), want + kPi / 2 ) ) < 0.15 );

  // structure() lists the new output kind with the population it reads and its neurons, in ring order
  auto st = e.structure();
  CHECK( st.outputs.size() == 1 and st.outputs[ 0 ].kind == "population_vector" and st.outputs[ 0 ].pops == std::vector< std::string >{ "RING" } and
         st.outputs[ 0 ].neurons.size() == static_cast< size_t >( N ) );
  CHECK( st.outputs[ 0 ].neurons.front() == 0 and st.outputs[ 0 ].neurons.back() == N - 1 );
}

int main()
{
  test_hand_driven_bump();
  test_ring_order();
  test_poisson_driven_bump_and_structure();
  return chk::finish( "test_population_vector" );
}
