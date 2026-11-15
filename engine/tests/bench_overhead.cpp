// Benchmark: closed-loop overhead per 1 ms slice on the 1k network = step() wall time minus time inside NEST's
// run(), plus the per-slice set_input / output_rate / drain_spikes calls a game makes. Must be < 50 us.
#include <algorithm>
#include <chrono>
#include <vector>
#include "check.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

using clk = std::chrono::steady_clock;
using nlohmann::json;
using namespace spike;

int main()
{
  json c = json::parse( chk::slurp( std::string( SPIKE_GOLDEN_DIR ) + "/balanced_1k.json" ) )[ "circuit" ];
  Engine e;
  if ( not e.load( c.dump() ) )
  {
    std::printf( "load failed: %s\n", e.error().c_str() );
    return 1;
  }
  for ( int i = 0; i < 200; ++i )  // warm up
    e.step( 1.0 );
  e.drain_spikes();
  const int N = 3000;
  std::vector< double > ov( N );
  double run_total = 0, wall_total = 0, motor = 0;
  uint64_t spikes = 0;
  for ( int i = 0; i < N; ++i )
  {
    auto t0 = clk::now();
    e.set_input( "sensor", 200.0 + 2.0 * motor );
    e.step( 1.0 );
    motor = e.output_rate( "motor" ) / 100.0;
    spikes += e.drain_spikes().size();
    double wall = std::chrono::duration< double, std::micro >( clk::now() - t0 ).count();
    ov[ i ] = wall - e.last_run_us();
    wall_total += wall;
    run_total += e.last_run_us();
  }
  std::sort( ov.begin(), ov.end() );
  double mean = 0;
  for ( double v : ov ) mean += v;
  mean /= N;
  std::printf( "  %d slices of 1 ms on 1k neurons: wall %.1f us/slice, run() %.1f us/slice, %llu spikes\n", N, wall_total / N, run_total / N, (unsigned long long)spikes );
  std::printf( "  engine overhead per slice: mean %.2f us, median %.2f us, p99 %.2f us\n", mean, ov[ N / 2 ], ov[ N * 99 / 100 ] );
  CHECK( e.error().empty() );
  CHECK( ov[ N / 2 ] < 50.0 );
  CHECK( mean < 50.0 );
  return chk::finish( "bench_overhead" );
}
