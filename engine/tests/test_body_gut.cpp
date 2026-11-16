// Gut: slow waves, excitatory / inhibitory motor input, bolus propulsion by a phased (peristaltic) wave only.
#include <cstdio>
#include "check.h"
#include "spike/body/gut.h"

using namespace spike::body;

// Peristaltic command (Bayliss & Starling 1899; Costa & Furness): contraction ring just behind the front, relaxation ahead.
// front(t) in segment units, direction +1 aboral / -1 oral.
static void wave( double front, int n, std::vector< double >& ex, std::vector< double >& inh, double ring_w = 1.0 )
{
  ex.assign( n, 0.0 );
  inh.assign( n, 0.0 );
  for ( int i = 0; i < n; ++i )
  {
    const double c = i + 0.5;
    if ( c > front - ring_w - 0.5 && c < front - 0.5 + 0.0 )
      ex[ i ] = 1.0;  // contracting ring, oral of the front
    if ( c >= front - 0.5 && c < front + 2.5 )
      inh[ i ] = 1.0; // descending inhibition aboral of the ring
  }
}

static double drive_wave( Gut& g, double x0, double speed, double sec, bool slow_waves = false )
{
  std::vector< double > ex, inh;
  const int n = g.params().n_segments;
  GutInput in;
  in.slow_waves = slow_waves;
  for ( int t = 0; t < int( sec * 1000 ); ++t )
  {
    const double front = x0 + speed * t * 1e-3;
    wave( front, n, ex, inh );
    in.excitatory = &ex;
    in.inhibitory = &inh;
    g.step( 1.0, in );
  }
  return g.bolus_pos;
}

int main()
{
  GutParams P;
  // ---- 1. phased aboral wave carries the bolus aboral at about the wave speed ----
  {
    Gut g( P );
    g.reset( 2.0 );
    const double x = drive_wave( g, 2.0, 1.0, 5.0 );  // 1 segment/s, 5 s
    std::printf( "aboral wave: bolus 2.0 -> %.2f (wave moved 5 segments), distance %.2f\n", x, g.distance );
    CHECK( g.distance > 3.0 && g.distance < 6.0 );
    CHECK( g.readouts().count( "bolus_pos" ) && g.readouts().count( "seg3" ) );
    // the bolus stays near the wave
    CHECK( std::fabs( x - 7.0 ) < 2.0 );
  }
  // ---- 2. uniform contraction (all excitatory) does not move it ----
  {
    Gut g( P );
    g.reset( 5.0 );
    std::vector< double > ex( P.n_segments, 1.0 ), inh( P.n_segments, 0.0 );
    GutInput in;
    in.excitatory = &ex;
    in.inhibitory = &inh;
    in.slow_waves = false;
    for ( int t = 0; t < 10000; ++t )
      g.step( 1.0, in );
    std::printf( "uniform contraction: displacement %.3f segments, mean contraction %.2f\n", g.distance, g.readouts()[ "mean_contraction" ] );
    CHECK( std::fabs( g.distance ) < 0.1 );
    CHECK( g.readouts()[ "mean_contraction" ] > 0.9 );
    // uniform relaxation neither
    Gut h( P );
    h.reset( 5.0 );
    ex.assign( P.n_segments, 0.0 );
    for ( int t = 0; t < 10000; ++t )
      h.step( 1.0, in );
    CHECK( std::fabs( h.distance ) < 0.1 );
  }
  // ---- 3. wave too fast for the muscle (tau) fails to propel, wave in the wrong phase pushes orally ----
  {
    Gut g( P );
    g.reset( 8.0 );
    // oral-going wave (anti-peristalsis): contraction ring aboral of the bolus's front pushes it orally
    std::vector< double > ex, inh;
    GutInput in;
    in.slow_waves = false;
    for ( int t = 0; t < 4000; ++t )
    {
      const double front = 9.0 - 1.0 * t * 1e-3;
      ex.assign( P.n_segments, 0.0 );
      inh.assign( P.n_segments, 0.0 );
      for ( int i = 0; i < P.n_segments; ++i )
      {
        const double c = i + 0.5;
        if ( c > front + 0.5 && c < front + 1.5 )
          ex[ i ] = 1.0;
        if ( c <= front + 0.5 && c > front - 2.5 )
          inh[ i ] = 1.0;
      }
      in.excitatory = &ex;
      in.inhibitory = &inh;
      g.step( 1.0, in );
    }
    std::printf( "oral wave: displacement %.2f\n", g.distance );
    CHECK( g.distance < -1.5 );
  }
  // ---- 4. slow waves alone: below threshold, no propulsion (segmentation-like tone only) ----
  {
    Gut g( P );
    g.reset( 5.0 );
    GutInput in;  // no motor input, slow waves on
    double cmax = 0;
    for ( int t = 0; t < 30000; ++t )
    {
      g.step( 1.0, in );
      for ( double c : g.contraction )
        cmax = std::max( cmax, c );
    }
    std::printf( "slow waves only: max contraction %.2f, displacement %.2f\n", cmax, g.distance );
    CHECK( cmax < 0.2 );
    CHECK( std::fabs( g.distance ) < 0.3 );
  }
  // ---- 5. slow waves + sustained excitation: the slow wave gates phasic contractions that travel aboral ----
  {
    Gut g( P );
    g.reset( 5.0 );
    std::vector< double > ex( P.n_segments, 0.35 );  // sub-threshold ACh drive alone (0.35 < 0.6) ...
    GutInput in;
    in.excitatory = &ex;
    double cmax = 0, cmin = 1;
    for ( int t = 0; t < 30000; ++t )
    {
      g.step( 1.0, in );
      if ( t > 10000 )
        for ( double c : g.contraction )
          cmax = std::max( cmax, c ), cmin = std::min( cmin, c );
    }
    std::printf( "slow waves + ACh 0.35: contraction swing %.2f..%.2f, displacement %.2f\n", cmin, cmax, g.distance );
    CHECK( cmax > 0.6 && cmin < 0.2 );  // ... ex + slow wave crosses threshold in phase
    // slow waves lag aboral: within one cycle the contraction peak of segment 6 follows that of segment 2
    Gut h( P );
    h.reset( 5.0 );
    for ( int t = 0; t < 10000; ++t )
      h.step( 1.0, in );
    double t2 = 0, t6 = 0, m2 = 0, m6 = 0;
    for ( int t = 0; t < 5000; ++t )  // exactly one slow-wave cycle
    {
      h.step( 1.0, in );
      if ( h.contraction[ 2 ] > m2 ) m2 = h.contraction[ 2 ], t2 = t;
      if ( h.contraction[ 6 ] > m6 ) m6 = h.contraction[ 6 ], t6 = t;
    }
    const double lag = std::fmod( t6 - t2 + 5000.0, 5000.0 );
    std::printf( "slow-wave peak lag segment 2 -> 6: %.0f ms (expected %.0f)\n", lag, 4 * P.phase_lag_cycles / P.slow_wave_hz * 1000 );
    CHECK( std::fabs( lag - 4 * P.phase_lag_cycles / P.slow_wave_hz * 1000 ) < 150 );
  }
  // ---- 6. ends and determinism ----
  {
    Gut g( P );
    g.reset( 10.5 );
    drive_wave( g, 10.0, 1.0, 6.0 );
    CHECK( g.bolus_pos <= P.n_segments - 0.5 * P.bolus_len + 1e-9 );
    auto go = [ & ] {
      Gut h( P );
      return drive_wave( h, 2.0, 0.8, 4.0, true );
    };
    CHECK( go() == go() );
  }
  return chk::finish( "test_body_gut" );
}
