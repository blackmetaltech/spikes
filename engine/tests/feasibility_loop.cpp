// Feasibility: NEST kernel embedded in C++ (no Python at runtime), closed loop at 1 ms slices.
#include <chrono>
#include <cstdio>
#include <vector>
#include "nest.h"
#include "kernel_manager.h"
#include "dictionary.h"
#include "node_collection.h"

using namespace nest;
using clk = std::chrono::steady_clock;
static int STAGE = 0;

static double ms_since( clk::time_point t ) { return std::chrono::duration< double, std::milli >( clk::now() - t ).count(); }

int main( int argc, char** argv )
{ try {
  init_nest( &argc, &argv );
  Dictionary k; k[ "resolution" ] = 0.1; k[ "rng_seed" ] = 42L; k[ "print_time" ] = false;
  set_kernel_status( k );
  STAGE = 1;

  const size_t NE = 800, NI = 200;
  auto E = create( "iaf_psc_alpha", NE );
  STAGE = 2;
  auto I = create( "iaf_psc_alpha", NI );
  STAGE = 3;
  auto drive = create( "poisson_generator", 1 );
  STAGE = 4;
  Dictionary pg; pg[ "rate" ] = 8000.0; set_nc_status( drive, *new std::vector< Dictionary >{ pg } );
  STAGE = 5;
  auto sensor = create( "dc_generator", 1 );          // the "sensor" we update every slice
  auto rec = create( "spike_recorder", 1 );
  STAGE = 6;

  STAGE = 7;
  Dictionary c_in; c_in[ "rule" ] = std::string( "fixed_indegree" ); c_in[ "indegree" ] = 80L;
  Dictionary se; se[ "synapse_model" ] = std::string( "static_synapse" ); se[ "weight" ] = 20.0; se[ "delay" ] = 1.5;
  Dictionary si; si[ "synapse_model" ] = std::string( "static_synapse" ); si[ "weight" ] = -100.0; si[ "delay" ] = 1.5;
  Dictionary c_in_i; c_in_i[ "rule" ] = std::string( "fixed_indegree" ); c_in_i[ "indegree" ] = 20L;
  Dictionary a2a; a2a[ "rule" ] = std::string( "all_to_all" );
  Dictionary sd; sd[ "synapse_model" ] = std::string( "static_synapse" ); sd[ "weight" ] = 20.0; sd[ "delay" ] = 0.1;
  connect( E, E, c_in, { se } ); connect( E, I, c_in, { se } );
  STAGE = 8;
  connect( I, E, c_in_i, { si } ); connect( I, I, c_in_i, { si } );
  STAGE = 9;
  connect( drive, E, a2a, { sd } ); connect( drive, I, a2a, { sd } );
  STAGE = 10;
  Dictionary ss; ss[ "synapse_model" ] = std::string( "static_synapse" ); ss[ "weight" ] = 1.0; ss[ "delay" ] = 0.1;
  auto sensed = slice_nc( E, 1, 50, 1 );
  connect( sensor, sensed, a2a, { ss } );
  STAGE = 11;
  connect( E, rec, a2a, { sd } );
  STAGE = 12;

  printf( "network: %zu neurons, %ld synapses\n", NE + NI, (long)get_kernel_status().get< long >( "num_connections" ) );

  // closed loop: sensor current = f(motor rate of the previous slice)
  const int SLICES = 2000;  // 2 s of model time at 1 ms
  long total_spikes = 0; double t_api = 0, t_run = 0;
  prepare();
  auto t0 = clk::now();
  double motor_rate = 0; size_t cursor = 0;
  for ( int s = 0; s < SLICES; ++s )
  {
    auto ta = clk::now();
    Dictionary dc; dc[ "amplitude" ] = 200.0 + 2.0 * motor_rate;   // pA
    set_node_status( ( *sensor )[ 0 ], dc );
    t_api += ms_since( ta );
    auto tr = clk::now();
    run( 1.0 );
    t_run += ms_since( tr );
    ta = clk::now();
    Dictionary st = get_node_status( ( *rec )[ 0 ] );
    auto ev = st.get< Dictionary >( "events" );
    const auto& snd = ev.get< std::vector< long > >( "senders" );
    long n = snd.size() - cursor;
    long motor = 0;
    for ( size_t q = cursor; q < snd.size(); ++q ) { long id = snd[ q ]; if ( id >= 900 && id <= 1000 ) ++motor; }  // last 100 E cells read out as "motor"
    motor_rate = motor * 1000.0 / 100.0;  // Hz per motor neuron in this 1 ms slice
    total_spikes += n;
    cursor = snd.size();
    t_api += ms_since( ta );
  }
  double wall = ms_since( t0 );
  cleanup();
  printf( "closed loop: %d slices of 1 ms, wall %.1f ms (%.2fx real time)\n", SLICES, wall, SLICES / wall );
  printf( "  run() %.1f ms, API in/out %.1f ms (%.1f us per slice)\n", t_run, t_api, 1000.0 * t_api / SLICES );
  printf( "  spikes %ld, mean rate %.1f Hz\n", total_spikes, total_spikes / ( double )NE / ( SLICES / 1000.0 ) );

  auto t1 = clk::now();
  simulate( 2000.0 );
  printf( "batch simulate(2000 ms): wall %.1f ms\n", ms_since( t1 ) );
  return 0;
  } catch ( std::exception& e ) { printf( "EXC at stage %d: %s\n", STAGE, e.what() ); return 1; }
}
