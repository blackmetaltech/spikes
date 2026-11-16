// Muscle: twitch shape, summation / tetanus fusion, size-principle recruitment, Hill curves, fatigue.
#include <cstdio>
#include "check.h"
#include "spike/body/muscle.h"

using namespace spike::body;

// Single motor unit pool member `u` driven by a spike train at f Hz (isometric); returns force trace of that unit alone
// (activation * weight is read through the muscle activation with only that unit firing).
static std::vector< double > unit_trace( int u, double f_hz, double dur_ms, double* fmax_pool = nullptr )
{
  MuscleParams p;
  p.fatigue = false;
  Muscle m( p );
  std::vector< int > sp( p.n_units, 0 );
  std::vector< double > tr;
  const double per = f_hz > 0 ? 1000.0 / f_hz : 1e12;
  double next = 5.0;
  for ( double t = 0; t < dur_ms; t += 1.0 )
  {
    sp[ u ] = 0;
    if ( t >= next )
    {
      sp[ u ] = 1;
      next += per;
    }
    MuscleInput in;
    in.spikes = &sp;
    m.step( 1.0, in );
    tr.push_back( m.units()[ u ].act );
  }
  if ( fmax_pool )
    *fmax_pool = m.units()[ u ].weight;
  return tr;
}

// ripple index = (max-min)/mean over the last 200 ms of a 1.5 s train
static double ripple( int u, double f )
{
  auto tr = unit_trace( u, f, 1500 );
  double mx = 0, mn = 1e9, mean = 0;
  int n = 0;
  for ( size_t i = 1300; i < tr.size(); ++i )
    mx = std::max( mx, tr[ i ] ), mn = std::min( mn, tr[ i ] ), mean += tr[ i ], ++n;
  return ( mx - mn ) / ( mean / n );
}
static double fusion_freq( int u )
{
  for ( double f = 4; f <= 120; f += 1 )
    if ( ripple( u, f ) < 0.10 )
      return f;
  return 999;
}

int main()
{
  MuscleParams P;
  const int slow = 0, fast = P.n_units - 1;

  // ---- 1. twitch shape: time to peak = contraction time, half-relaxation ~ 1.68 T ----
  for ( int u : { slow, fast } )
  {
    auto tr = unit_trace( u, 0, 1500 );
    size_t pk = 0;
    for ( size_t i = 0; i < tr.size(); ++i )
      if ( tr[ i ] > tr[ pk ] )
        pk = i;
    const double T = Muscle( P ).units()[ u ].contraction_ms;
    const double t_peak = double( pk ) - 5.0;
    CHECK_MSG( std::fabs( t_peak - T ) < 0.1 * T + 2, "unit " + std::to_string( u ) + " peak " + std::to_string( t_peak ) );
    size_t h = pk;
    while ( h < tr.size() && tr[ h ] > 0.5 * tr[ pk ] )
      ++h;
    const double hrt = double( h - pk );
    // saturation makes the recorded half-relaxation a little shorter than the linear 1.678 T
    CHECK_MSG( hrt > 1.1 * T && hrt < 1.9 * T, "unit " + std::to_string( u ) + " HRT " + std::to_string( hrt ) );
    std::printf( "unit %d: CT %.0f ms (T %.0f), HRT %.0f ms, twitch/tetanus %.2f\n", u, t_peak, T, hrt, tr[ pk ] );
  }
  {
    auto a = unit_trace( slow, 0, 1000 ), b = unit_trace( fast, 0, 1000 );
    double pa = *std::max_element( a.begin(), a.end() ), pb = *std::max_element( b.begin(), b.end() );
    CHECK( pa > 0.2 && pa < 0.45 && pb > 0.2 && pb < 0.45 );  // twitch is 20-45 % of tetanus (tetanus/twitch ~ 3)
  }

  // ---- 2. summation: a doublet at 10 ms gives more than a single twitch ----
  {
    Muscle m( [] { MuscleParams p; p.fatigue = false; return p; }() );
    std::vector< int > sp( P.n_units, 0 );
    double peak1 = 0, peak2 = 0;
    for ( int t = 0; t < 600; ++t )
    {
      sp[ fast ] = ( t == 5 ) ? 1 : 0;
      MuscleInput in;
      in.spikes = &sp;
      m.step( 1.0, in );
      peak1 = std::max( peak1, m.units()[ fast ].act );
    }
    m.reset();
    for ( int t = 0; t < 600; ++t )
    {
      sp[ fast ] = ( t == 5 || t == 15 ) ? 1 : 0;
      MuscleInput in;
      in.spikes = &sp;
      m.step( 1.0, in );
      peak2 = std::max( peak2, m.units()[ fast ].act );
    }
    CHECK( peak2 > 1.5 * peak1 );
  }

  // ---- 3. tetanus: ripple falls with rate, fusion frequencies ----
  {
    const double fs = fusion_freq( slow ), ff = fusion_freq( fast );
    std::printf( "fusion (ripple<10%%): slow %.0f Hz, fast %.0f Hz\n", fs, ff );
    CHECK( fs >= 8 && fs <= 35 );   // slow units fuse at low rates (Burke 1973: 10-25 Hz)
    CHECK( ff >= 25 && ff <= 60 );  // fast units need ~30-60 Hz
    CHECK( ff > 1.5 * fs );
    CHECK( ripple( slow, 5 ) > 0.25 );
    CHECK( ripple( fast, 10 ) > 0.5 );
    // force-frequency: monotone rising and saturating, tetanus >> twitch
    double last = 0;
    for ( double f : { 5., 10., 20., 40., 80. } )
    {
      auto tr = unit_trace( fast, f, 1500 );
      double mean = 0;
      for ( size_t i = 1300; i < tr.size(); ++i )
        mean += tr[ i ] / 200.0;
      CHECK( mean > last );
      last = mean;
    }
    CHECK( last > 0.85 );
  }

  // ---- 4. Henneman: recruitment order and force rise (rate mode) ----
  {
    Muscle m( [] { MuscleParams p; p.fatigue = false; return p; }() );
    double prev_force = -1;
    int prev_n = -1;
    for ( int k = 0; k <= 10; ++k )
    {
      const double e = k / 10.0;
      m.reset();
      MuscleInput in;
      in.excitation = e;
      for ( int t = 0; t < 3000; ++t )
        m.step( 1.0, in );
      const double F = m.force_N;
      int n_act = 0;
      for ( const auto& u : m.units() )
        if ( u.rate_hz > 0 )
          ++n_act;
      // recruited units are exactly those with the lowest thresholds
      bool contiguous = true;
      for ( size_t i = 0; i + 1 < m.units().size(); ++i )
        if ( m.units()[ i ].rate_hz == 0 && m.units()[ i + 1 ].rate_hz > 0 )
          contiguous = false;
      CHECK_MSG( contiguous, "size principle at e=" + std::to_string( e ) );
      CHECK( n_act >= prev_n );
      CHECK_MSG( F >= prev_force - 1e-6, "force rises with excitation at e=" + std::to_string( e ) );
      prev_force = F;
      prev_n = n_act;
      if ( k == 1 || k == 5 || k == 10 )
        std::printf( "excitation %.1f: %d/%d units recruited, force %.1f N (%.0f %% of pool max)\n", e, n_act, P.n_units, F, 100 * F / P.fmax_N );
    }
    CHECK( prev_n == P.n_units );
    CHECK( prev_force > 0.6 * P.fmax_N );
    // small units come in first: at e = 0.05 unit 0 fires and the last unit does not
    m.reset();
    MuscleInput in;
    in.excitation = 0.05;
    for ( int t = 0; t < 500; ++t )
      m.step( 1.0, in );
    CHECK( m.units().front().rate_hz > 0 && m.units().back().rate_hz == 0 );
    // size of a unit grows with index; contraction time falls
    for ( size_t i = 0; i + 1 < m.units().size(); ++i )
      CHECK( m.units()[ i ].weight < m.units()[ i + 1 ].weight && m.units()[ i ].contraction_ms > m.units()[ i + 1 ].contraction_ms );
  }

  // ---- 5. spike mode with pool rate split: same pool, spikes from outside ----
  {
    Muscle m( [] { MuscleParams p; p.fatigue = false; return p; }() );
    std::vector< int > sp( P.n_units, 0 );
    // every unit fires at 40 Hz -> near-maximal force
    for ( int t = 0; t < 2000; ++t )
    {
      for ( auto& s : sp )
        s = 0;
      for ( int i = 0; i < P.n_units; ++i )
        if ( ( t + 3 * i ) % 25 == 0 )
          sp[ i ] = 1;
      MuscleInput in;
      in.spikes = &sp;
      m.step( 1.0, in );
    }
    CHECK( m.force_N > 0.7 * P.fmax_N );
  }

  // ---- 6. Hill curves ----
  {
    Muscle m;
    CHECK( std::fabs( m.force_length( 1.0 ) - 1.0 ) < 1e-12 );
    CHECK( m.force_length( 0.6 ) < 0.5 && m.force_length( 1.5 ) < 0.5 );
    CHECK( std::fabs( m.force_length( 0.8 ) - m.force_length( 1.2 ) ) < 1e-12 );
    CHECK( m.passive( 0.9 ) == 0 && m.passive( 1.3 ) > 0 && m.passive( 1.6 ) > m.passive( 1.3 ) );
    CHECK( std::fabs( m.passive( 1.6 ) - 1.0 ) < 1e-9 );
    const double k = 0.25;
    CHECK( std::fabs( Muscle::force_velocity( 0, k, 1.8 ) - 1.0 ) < 1e-12 );
    CHECK( Muscle::force_velocity( 1.0, k, 1.8 ) == 0 );
    // Hill: at u = 0.2 f = 0.8/1.8
    CHECK( std::fabs( Muscle::force_velocity( 0.2, k, 1.8 ) - 0.8 / 1.8 ) < 1e-12 );
    double prev = 2;
    for ( double u = -1.5; u <= 1.0; u += 0.05 )
    {
      const double f = Muscle::force_velocity( u, k, 1.8 );
      CHECK( f <= prev + 1e-12 );
      prev = f;
    }
    CHECK( Muscle::force_velocity( -5, k, 1.8 ) < 1.8 && Muscle::force_velocity( -5, k, 1.8 ) > 1.6 );
    // isometric at full tetanus, shortening reduces and lengthening raises force
    MuscleParams p;
    p.fatigue = false;
    Muscle a( p );
    MuscleInput in;
    in.excitation = 1.0;
    for ( int t = 0; t < 2000; ++t )
      a.step( 1.0, in );
    const double iso = a.force_at( 1.0, 0 ), sh = a.force_at( 1.0, -2.0 ), le = a.force_at( 1.0, 2.0 );
    CHECK( sh < 0.8 * iso && le > 1.1 * iso );
  }

  // ---- 7. fatigue: sustained effort loses force (fast units first); rest recovers ----
  {
    MuscleParams p;
    p.fatigue_time_scale = 60;  // one second = one minute of physiological time
    Muscle m( p );
    MuscleInput in;
    in.excitation = 1.0;
    for ( int t = 0; t < 1000; ++t )
      m.step( 1.0, in );
    const double F0 = m.force_N;
    for ( int t = 0; t < 60000; ++t )
      m.step( 1.0, in );
    const double F1 = m.force_N;
    CHECK_MSG( F1 < 0.75 * F0, "fatigue lowers force: " + std::to_string( F1 / F0 ) );
    CHECK( m.units().back().capacity < m.units().front().capacity - 0.2 );
    CHECK( m.fatigue_level > 0.2 );
    in.excitation = 0;
    for ( int t = 0; t < 60000; ++t )
      m.step( 1.0, in );
    CHECK( m.fatigue_level < 0.6 * ( 1 - F1 / F0 ) + 0.2 );
    std::printf( "fatigue: force %.0f%% of fresh after 60 s (x60), fatigue level after rest %.2f\n", 100 * F1 / F0, m.fatigue_level );
  }

  // ---- 8. determinism ----
  {
    auto run = [] {
      MuscleParams p;
      p.rate_cv = 0.2;
      Muscle m( p );
      MuscleInput in;
      in.excitation = 0.4;
      for ( int t = 0; t < 2000; ++t )
        m.step( 1.0, in );
      return m.force_N;
    };
    CHECK( run() == run() );
  }
  return chk::finish( "test_body_muscle" );
}
