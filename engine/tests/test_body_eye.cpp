// Eye: pupil dynamics (sphincter vs dilator), light reflex latency / time course, retinal illuminance and afferent rate.
#include <cstdio>
#include "check.h"
#include "spike/body/eye.h"

using namespace spike::body;

// Minimal pupillary light reflex used only to exercise the plant: afferent rate -> (150 ms pretectum / EW pathway delay)
// -> sphincter drive, reciprocal inhibition of the dilator. In the game this arc is the player's circuit.
struct Reflex
{
  DelayLine arc;
  Reflex() { arc.delay_ms = 150; }
  void drive( double retinal_hz, EyeInput& in )
  {
    const double a = arc.step( retinal_hz, 1.0 );
    const double s = clamp( ( a - 20.0 ) / 60.0, 0.0, 1.0 );
    in.sphincter = s;
    in.dilator = 0.8 * ( 1.0 - s );
  }
};

int main()
{
  EyeParams P;
  // ---- 1. open loop: dilator only -> wide, sphincter only -> small, both silent -> mid ----
  {
    Eye e;
    EyeInput in;
    e.reset( 0, 0 );
    for ( int t = 0; t < 4000; ++t )
      e.step( 1.0, in );
    CHECK( std::fabs( e.diameter_mm - 0.5 * ( P.d_min_mm + P.d_max_mm ) ) < 0.05 );
    in.dilator = 1.0;
    for ( int t = 0; t < 6000; ++t )
      e.step( 1.0, in );
    CHECK( e.diameter_mm > 7.9 );
    in.dilator = 0;
    in.sphincter = 1.0;
    for ( int t = 0; t < 6000; ++t )
      e.step( 1.0, in );
    CHECK( e.diameter_mm < 2.1 );
  }
  // ---- 2. smooth-muscle kinetics: constriction faster than redilation; latency from EC coupling ----
  {
    Eye e;
    e.reset( 0, 1.0 );
    EyeInput in;
    in.dilator = 1.0;
    for ( int t = 0; t < 4000; ++t )
      e.step( 1.0, in );
    const double d_dark = e.diameter_mm;
    in.sphincter = 1.0;
    in.dilator = 0.0;
    double t_lat = -1, t63 = -1;
    for ( int t = 1; t <= 5000; ++t )
    {
      e.step( 1.0, in );
      if ( t_lat < 0 && e.diameter_mm < d_dark - 0.05 )
        t_lat = t;
      if ( t63 < 0 && e.diameter_mm < d_dark - 0.63 * ( d_dark - e.params().d_min_mm ) )
        t63 = t;
    }
    in.sphincter = 0;
    in.dilator = 1.0;
    const double d_con = e.diameter_mm;
    double r63 = -1;
    for ( int t = 1; t <= 6000; ++t )
    {
      e.step( 1.0, in );
      if ( r63 < 0 && e.diameter_mm > d_con + 0.63 * ( d_dark - d_con ) )
        r63 = t;
    }
    std::printf( "plant: constriction latency %.0f ms, 63%% at %.0f ms; redilation 63%% at %.0f ms\n", t_lat, t63, r63 );
    CHECK( t_lat >= 40 && t_lat < 150 );
    CHECK( r63 > 1.3 * t63 );
  }
  // ---- 3. retina: illuminance = luminance x area, monotone log-like afferent ----
  {
    Eye e;
    CHECK( std::fabs( e.retina_rate( 100, 4 ) - e.retina_rate( 100, 4 ) ) == 0 );
    double prev = -1;
    for ( double L : { 1e-3, 1e-2, 1e-1, 1., 10., 100., 1000. } )
    {
      const double r = e.retina_rate( L, 4.0 );
      CHECK( r > prev );
      prev = r;
    }
    CHECK( e.retina_rate( 1000, 4 ) <= P.r_max_hz && e.retina_rate( 0, 4 ) >= P.r_spont_hz - 1e-9 );
    // halving the pupil area reduces the rate (the reflex's negative feedback)
    CHECK( e.retina_rate( 50, 2.0 ) < e.retina_rate( 50, 4.0 ) );
    e.reset( 0, 0.5 );
    EyeInput in;
    in.luminance = 100;
    for ( int t = 0; t < 500; ++t )
      e.step( 1.0, in );
    const double area = kPi / 4 * e.diameter_mm * e.diameter_mm;
    CHECK( std::fabs( e.illuminance_td - 100 * area ) < 1e-6 );
  }
  // ---- 4. closed-loop light reflex: flash from dim to bright and back ----
  {
    Eye e;
    Reflex r;
    EyeInput in;
    in.luminance = 0.05;
    for ( int t = 0; t < 6000; ++t )
    {
      r.drive( e.retinal_hz, in );
      e.step( 1.0, in );
    }
    const double d0 = e.diameter_mm;
    std::vector< double > tr;
    in.luminance = 200;
    double t_lat = -1, t_min = 0, d_min = 99;
    for ( int t = 1; t <= 6000; ++t )
    {
      r.drive( e.retinal_hz, in );
      e.step( 1.0, in );
      tr.push_back( e.diameter_mm );
      if ( t_lat < 0 && e.diameter_mm < d0 - 0.1 )
        t_lat = t;
      if ( e.diameter_mm < d_min )
        d_min = e.diameter_mm, t_min = t;
    }
    const double d1 = e.diameter_mm;
    in.luminance = 0.05;
    double t_re = -1;
    for ( int t = 1; t <= 9000; ++t )
    {
      r.drive( e.retinal_hz, in );
      e.step( 1.0, in );
      if ( t_re < 0 && e.diameter_mm > d1 + 0.63 * ( d0 - d1 ) )
        t_re = t;
    }
    std::printf( "light reflex: dark %.2f mm -> bright %.2f mm (min %.2f at %.0f ms), latency %.0f ms, redilation 63%% at %.0f ms\n", d0, d1, d_min, t_min, t_lat, t_re );
    CHECK( d0 > 6.0 );                       // dark-adapted pupil wide
    CHECK( d1 < 4.0 && d1 > e.params().d_min_mm - 1e-9 );  // bright light: 2-4 mm
    CHECK( t_lat > 180 && t_lat < 350 );     // human PLR latency ~ 200-250 ms (Ellis 1981)
    double t90 = -1;
    for ( size_t i = 0; i < tr.size(); ++i )
      if ( tr[ i ] < d0 - 0.9 * ( d0 - d1 ) )
      {
        t90 = double( i + 1 );
        break;
      }
    std::printf( "constriction 90%% at %.0f ms\n", t90 );
    CHECK( t90 > 0 && t90 < 2000 );
    CHECK( t_re > 700 );                     // redilation is slow (seconds)
    // intermediate lights give intermediate diameters (monotone)
    double prev = 99;
    for ( double L : { 0.01, 0.5, 5., 50., 500. } )
    {
      Eye g;
      Reflex q;
      EyeInput i2;
      i2.luminance = L;
      for ( int t = 0; t < 15000; ++t )
      {
        q.drive( g.retinal_hz, i2 );
        g.step( 1.0, i2 );
      }
      CHECK_MSG( g.diameter_mm <= prev + 1e-6, "monotone at L=" + std::to_string( L ) );
      prev = g.diameter_mm;
    }
  }
  // ---- 5. readouts, determinism ----
  {
    Eye e;
    CHECK( e.readouts().count( "pupil_mm" ) && e.readouts().count( "retinal_hz" ) );
    auto go = [] {
      Eye g;
      Reflex q;
      EyeInput i;
      i.luminance = 30;
      for ( int t = 0; t < 3000; ++t )
      {
        q.drive( g.retinal_hz, i );
        g.step( 1.0, i );
      }
      return g.diameter_mm;
    };
    CHECK( go() == go() );
  }
  return chk::finish( "test_body_eye" );
}
