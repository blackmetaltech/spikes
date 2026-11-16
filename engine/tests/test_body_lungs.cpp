// Lungs: breath mechanics, minute ventilation, PaCO2 balance, hypo/hyperventilation, exercise, inspired CO2, chemoreceptors.
#include <cstdio>
#include "check.h"
#include "spike/body/lungs.h"

using namespace spike::body;

// Rhythmic inspiratory burst: `amp` for `ti` ms every `period` ms (what a pre-Botzinger / phrenic pool would deliver).
static double burst( double t_ms, double period, double ti, double amp ) { return std::fmod( t_ms, period ) < ti ? amp : 0.0; }

struct Out { double pa = 0, ve = 0, va = 0, vt = 0, rate = 0, per = 0, cen = 0, pt = 0; };
static Out run( Lungs& l, double sec, double amp, double period = 5000, LungInput base = {}, double ti = 1500 )
{
  Out o;
  int n = 0;
  const int total = int( sec * 1000 );
  for ( int t = 0; t < total; ++t )
  {
    base.drive = burst( t, period, ti, amp );
    l.step( 1.0, base );
    if ( t >= total - 20000 )
    {
      o.pa += l.paco2;
      ++n;
    }
  }
  o.pa /= n;
  o.ve = l.ve_l_min; o.va = l.va_l_min; o.vt = l.tidal_l; o.rate = l.rate_bpm;
  o.per = l.peripheral_hz; o.cen = l.central_hz; o.pt = l.tissue_pco2;
  return o;
}

int main()
{
  // ---- 1. breath mechanics: tidal volume ~ C * Pmus, passive expiration with tau = R C ----
  {
    Lungs l;
    double vmax = 0, v_end_insp = 0;
    for ( int t = 0; t < 5000; ++t )
    {
      LungInput in;
      in.drive = t < 1500 ? 0.25 : 0.0;
      l.step( 1.0, in );
      vmax = std::max( vmax, l.volume_l );
      if ( t == 1499 )
        v_end_insp = l.volume_l;
    }
    const double vt = v_end_insp - l.params().frc_l;
    std::printf( "single breath: VT %.3f L, end volume %.3f L\n", vt, l.volume_l );
    CHECK( vt > 0.4 && vt < 0.6 );
    CHECK( std::fabs( l.volume_l - l.params().frc_l ) < 0.01 );  // passive return to FRC
    CHECK( l.flow_l_s <= 0.01 );
    // exponential expiration: after tau = R C = 0.3 s volume is e^-1 of the tidal volume
    Lungs m;
    for ( int t = 0; t < 1500; ++t )
    {
      LungInput in;
      in.drive = 0.25;
      m.step( 1.0, in );
    }
    const double v0 = m.volume_l - m.params().frc_l;
    for ( int t = 0; t < 400; ++t )
    {
      LungInput in;
      m.step( 1.0, in );
    }
    CHECK( std::fabs( ( m.volume_l - m.params().frc_l ) / v0 - std::exp( -1.0 ) ) < 0.1 );
  }
  // ---- 2. eupnoea: 12 breaths/min, VT 0.5 L -> VE ~ 6 L/min, PaCO2 ~ 40 mmHg ----
  {
    Lungs l;
    Out o = run( l, 240, 0.25 );
    std::printf( "eupnoea: rate %.1f/min VT %.2f L VE %.2f VA %.2f L/min PaCO2 %.1f tissue %.1f periph %.1f Hz central %.1f Hz\n", o.rate, o.vt, o.ve, o.va, o.pa, o.pt, o.per, o.cen );
    CHECK( std::fabs( o.rate - 12 ) < 0.5 );
    CHECK( o.ve > 5.5 && o.ve < 6.5 );
    CHECK( o.va > 3.8 && o.va < 4.6 );
    CHECK( o.pa > 38 && o.pa < 44 );
    CHECK( l.paco2 > 38 && l.paco2 < 44 );
    CHECK( o.per > 1 && o.per < 8 && o.cen > 0 );
    CHECK( l.readouts().count( "paco2_mmHg" ) && l.readouts().count( "ve_l_min" ) );
  }
  // ---- 3. hypoventilation raises PaCO2, hyperventilation lowers it (monotone) ----
  {
    double prev = 1e9;
    for ( double amp : { 0.12, 0.2, 0.25, 0.4, 0.5 } )
    {
      LungParams p;
      p.time_scale = 5;  // faster gas dynamics to reach steady state
      Lungs l( p );
      Out o = run( l, 400, amp );
      std::printf( "drive %.2f: VE %.1f VA %.1f PaCO2 %.1f\n", amp, o.ve, o.va, o.pa );
      CHECK( o.pa < prev );
      prev = o.pa;
    }
  }
  // ---- 4. steady-state matches PaCO2 = 863 VCO2 / VA ----
  {
    LungParams p;
    p.time_scale = 5;
    Lungs l( p );
    Out o = run( l, 600, 0.18 );
    const double expect = 863.0 * l.vco2_l_min / o.va;
    std::printf( "alveolar gas equation: %.1f vs simulated %.1f\n", expect, o.pa );
    CHECK( std::fabs( expect - o.pa ) < 0.07 * expect );
  }
  // ---- 5. exercise (VCO2 x4) at fixed ventilation raises PaCO2; with a chemoreflex controller PaCO2 is defended ----
  {
    LungParams p;
    p.time_scale = 5;
    Lungs a( p ), b( p );
    LungInput ex;
    ex.metabolic = 4.0;
    run( a, 200, 0.25 );
    Out fixed = run( a, 300, 0.25, 5000, ex );
    // proportional chemoreflex: drive = 0.5 + g * (central+peripheral - rest) (the circuit's job in the game)
    double rest = 0;
    {
      Lungs r( p );
      run( r, 200, 0.25 );
      rest = r.peripheral_hz + r.central_hz;
    }
    double pa_sum = 0;
    int n = 0;
    for ( int t = 0; t < 500000; ++t )
    {
      LungInput in = t > 100000 ? ex : LungInput{};
      const double amp = clamp( 0.25 + 0.02 * ( b.peripheral_hz + b.central_hz - rest ), 0.0, 1.0 );
      in.drive = burst( t, 5000 - 2000 * clamp( ( amp - 0.25 ) / 0.5, 0.0, 1.0 ), 1500, amp );
      b.step( 1.0, in );
      if ( t > 480000 )
        pa_sum += b.paco2, ++n;
    }
    const double pa_reflex = pa_sum / n;
    std::printf( "exercise x4: PaCO2 fixed ventilation %.1f, with chemoreflex %.1f, VE %.1f\n", fixed.pa, pa_reflex, b.ve_l_min );
    CHECK( fixed.pa > 55 );
    CHECK( pa_reflex < fixed.pa - 8 );
    CHECK( b.ve_l_min > 1.5 * 6 );
    CHECK( b.peripheral_hz > 0 );
  }
  // ---- 6. inspired CO2 raises PaCO2 and the chemoreceptor rates ----
  {
    LungParams p;
    p.time_scale = 5;
    Lungs l( p );
    Out base = run( l, 200, 0.25 );
    LungInput co2;
    co2.inspired_pco2 = 30;  // ~4 % CO2
    Out hi = run( l, 400, 0.25, 5000, co2 );
    std::printf( "inspired CO2 30 mmHg: PaCO2 %.1f -> %.1f, periph %.1f -> %.1f Hz, central %.1f -> %.1f Hz\n", base.pa, hi.pa, base.per, hi.per, base.cen, hi.cen );
    CHECK( hi.pa > base.pa + 15 && hi.per > base.per + 4 && hi.cen > base.cen + 5 );
    // hypoxia sensitises the carotid body
    Lungs h( p );
    LungInput hyp;
    hyp.hypoxia = 1;
    Out ho = run( h, 200, 0.25, 5000, hyp );
    CHECK( ho.per > 2 * base.per - 1 );
  }
  // ---- 7. apnoea: PaCO2 rises at a physiological few mmHg/min ----
  {
    Lungs l;
    run( l, 120, 0.25 );
    const double p0 = l.paco2;
    for ( int t = 0; t < 60000; ++t )
      l.step( 1.0, LungInput{} );
    const double rise = l.paco2 - p0;
    std::printf( "apnoea: PaCO2 rises %.1f mmHg in 60 s\n", rise );
    CHECK( rise > 3 && rise < 16 );
    CHECK( l.ve_l_min < 3.5 );  // window empties
  }
  // ---- 8. determinism ----
  {
    auto go = [] {
      Lungs l;
      return run( l, 30, 0.3 ).pa;
    };
    CHECK( go() == go() );
  }
  return chk::finish( "test_body_lungs" );
}
