// Heart + circulation: baseline haemodynamics, Windkessel pulse, HR / sympathetic / orthostatic / haemorrhage effects, baroreceptor.
#include <cstdio>
#include "check.h"
#include "spike/body/heart.h"

using namespace spike::body;

struct Stat { double map = 0, sys = 0, dia = 0, hr = 0, sv = 0, co = 0, cvp = 0, baro = 0; };

// run for `sec` seconds, then average over the last `avg` seconds
static Stat run( Heart& h, const HeartInput& in, double sec, double avg = 15 )
{
  Stat s;
  int n = 0;
  const int total = int( sec * 1000 );
  for ( int t = 0; t < total; ++t )
  {
    h.step( 1.0, in );
    if ( t >= total - int( avg * 1000 ) )
    {
      s.map += h.pressure;
      s.sys += h.systolic;
      s.dia += h.diastolic;
      s.hr += h.hr_bpm;
      s.sv += h.stroke_ml;
      s.co += h.co_l_min;
      s.cvp += h.cvp;
      s.baro += h.baro_hz;
      ++n;
    }
  }
  s.map /= n; s.sys /= n; s.dia /= n; s.hr /= n; s.sv /= n; s.co /= n; s.cvp /= n; s.baro /= n;
  return s;
}
static HeartInput at_hr( double bpm )
{
  HeartInput in;
  in.sa_rate_hz = bpm / 60.0;
  return in;
}

int main()
{
  // ---- 1. baseline: 70 bpm ----
  {
    Heart h;
    Stat s = run( h, at_hr( 70 ), 60 );
    std::printf( "baseline: MAP %.1f  %0.f/%0.f mmHg  HR %.1f  SV %.1f ml  CO %.2f L/min  CVP %.1f  baro %.0f Hz\n", s.map, s.sys, s.dia, s.hr, s.sv, s.co, s.cvp, s.baro );
    CHECK( s.map > 90 && s.map < 100 );
    CHECK( s.sys > 105 && s.sys < 135 && s.dia > 65 && s.dia < 90 );
    CHECK( s.sys - s.dia > 30 && s.sys - s.dia < 60 );  // pulse pressure ~ SV / C
    CHECK( std::fabs( s.hr - 70 ) < 1.0 );
    CHECK( s.co > 4.4 && s.co < 5.5 );
    CHECK( s.cvp > 3 && s.cvp < 7 );
    CHECK( std::fabs( h.map_beat - s.map ) < 3 );
    // MAP = CVP + CO * TPR (steady state)
    CHECK( std::fabs( s.map - ( s.cvp + s.co * 1000.0 / 60.0 * h.resistance ) ) < 4 );
    // intrinsic pacemaker at resting autonomic tone also gives ~70 bpm
    Heart g;
    HeartInput in;  // defaults: sym 0.2, vagal 0.6, no rate -> intrinsic
    Stat q = run( g, in, 40 );
    CHECK( std::fabs( q.hr - 70 ) < 2 && q.map > 88 && q.map < 102 );
    // vagal stimulation slows, sympathetic speeds
    Heart v, y;
    in.vagal = 0.9;
    Stat sv = run( v, in, 30 );
    in.vagal = 0.6;
    in.sym_cardiac = 0.7;
    Stat sy = run( y, in, 30 );
    CHECK( sv.hr < q.hr - 15 && sy.hr > q.hr + 25 );
  }
  // ---- 2. Windkessel: diastolic run-off is exponential with tau = R C ----
  {
    Heart h;
    HeartInput in = at_hr( 60 );
    run( h, in, 30 );
    // record one cycle, fit decay in the second half of diastole
    std::vector< double > p;
    while ( !h.beat_now )
      h.step( 1.0, in );
    p.push_back( h.pressure );
    for ( int t = 1; t < 1000; ++t )
    {
      h.step( 1.0, in );
      p.push_back( h.pressure );
    }
    const double tau_fit = ( 800 - 600 ) * 1e-3 / std::log( ( p[ 600 ] - h.cvp ) / ( p[ 800 ] - h.cvp ) );
    const double tau = h.resistance * h.params().c_art;
    std::printf( "diastolic tau fit %.2f s, R*C %.2f s\n", tau_fit, tau );
    CHECK( std::fabs( tau_fit - tau ) < 0.25 * tau );
  }
  // ---- 3. higher HR raises MAP and CO (monotone), lower HR lowers it ----
  {
    double prev_map = 0, prev_co = 0;
    for ( double bpm : { 50., 70., 90., 120. } )
    {
      Heart h;
      Stat s = run( h, at_hr( bpm ), 80 );
      std::printf( "HR %3.0f: MAP %.1f  SV %.1f  CO %.2f\n", bpm, s.map, s.sv, s.co );
      CHECK( s.map > prev_map && s.co > prev_co );
      prev_map = s.map;
      prev_co = s.co;
    }
    Heart a, b;
    Stat lo = run( a, at_hr( 70 ), 60 ), hi = run( b, at_hr( 100 ), 60 );
    CHECK( hi.map > lo.map + 5 );
    CHECK( hi.sv < lo.sv );  // shorter filling time -> smaller stroke volume
  }
  // ---- 4. orthostatic: standing drops MAP without reflex; reflex-level sympathetic drive limits the fall ----
  {
    Heart h;
    HeartInput in = at_hr( 70 );
    Stat base = run( h, in, 40 );
    in.standing = 1.0;
    Stat st = run( h, in, 60 );
    std::printf( "standing, no reflex: MAP %.1f -> %.1f, SV %.1f -> %.1f, CVP %.1f -> %.1f\n", base.map, st.map, base.sv, st.sv, base.cvp, st.cvp );
    CHECK( st.map < base.map - 8 );
    CHECK( st.sv < 0.9 * base.sv );
    CHECK( st.cvp < base.cvp );
    in.standing = 0;
    Stat back = run( h, in, 60 );
    CHECK( std::fabs( back.map - base.map ) < 4 );
    // compensated: tachycardia + vasoconstriction + venoconstriction (what the baroreflex would do)
    Heart g;
    HeartInput c = at_hr( 85 );
    c.sym_cardiac = c.sym_vascular = 0.4;
    c.standing = 1.0;
    Stat comp = run( g, c, 90 );
    std::printf( "standing with sympathetic response: MAP %.1f\n", comp.map );
    CHECK( comp.map > st.map + 10 );
    // the baroreceptor reports the drop
    CHECK( st.baro < base.baro - 5 );
  }
  // ---- 5. haemorrhage ----
  {
    Heart h;
    HeartInput in = at_hr( 70 );
    Stat base = run( h, in, 40 );
    in.haemorrhage_ml_s = 10;
    run( h, in, 30 );  // 300 ml lost
    in.haemorrhage_ml_s = 0;
    Stat after = run( h, in, 30 );
    std::printf( "haemorrhage 300 ml: MAP %.1f -> %.1f, CVP %.1f -> %.1f, lost %.0f ml\n", base.map, after.map, base.cvp, after.cvp, h.blood_lost_ml );
    CHECK( std::fabs( h.blood_lost_ml - 300 ) < 1 );
    CHECK( after.map < base.map - 8 && after.sv < base.sv );
  }
  // ---- 6. contractility (sympathetic) raises stroke volume; resistance raises MAP ----
  {
    Heart a, b, c;
    HeartInput in = at_hr( 70 );
    Stat base = run( a, in, 60 );
    in.sym_cardiac = 0.8;
    Stat con = run( b, in, 60 );
    CHECK( con.sv > 1.3 * base.sv && con.map > base.map + 10 );
    in = at_hr( 70 );
    in.sym_vascular = 0.8;
    Stat vas = run( c, in, 60 );
    CHECK( vas.map > base.map + 8 && vas.co < con.co );
  }
  // ---- 7. baroreceptor: sigmoid in pressure, dP/dt sensitive ----
  {
    Heart h;
    HeartInput in = at_hr( 70 );
    run( h, in, 30 );
    // within one cycle, at equal pressure the rising limb fires more than the falling limb
    double rise = -1, fall = -1;
    std::vector< double > P, R, D;
    for ( int t = 0; t < 1000; ++t )
    {
      h.step( 1.0, in );
      P.push_back( h.pressure );
      R.push_back( h.baro_hz );
      D.push_back( h.dpdt );
    }
    size_t pk = std::max_element( P.begin(), P.end() ) - P.begin();
    const double target = 0.5 * ( *std::max_element( P.begin(), P.end() ) + *std::min_element( P.begin(), P.end() ) );
    for ( size_t i = 1; i < P.size(); ++i )
      if ( i < pk && P[ i - 1 ] < target && P[ i ] >= target )
        rise = R[ i ];
      else if ( i > pk && P[ i - 1 ] > target && P[ i ] <= target )
        fall = R[ i ];
    std::printf( "baro at equal pressure: rising %.1f Hz, falling %.1f Hz\n", rise, fall );
    CHECK( rise > fall + 3 );
    // static curve: monotone in mean pressure (via resistance / HR), bounded by max
    double prev = -1;
    for ( double bpm : { 40., 60., 90., 150. } )
    {
      Heart g;
      Stat s = run( g, at_hr( bpm ), 60 );
      CHECK( s.baro >= prev );
      CHECK( s.baro <= h.params().baro_max_hz );
      prev = s.baro;
    }
  }
  // ---- 8. SA spikes drive beats one-to-one (refractory merges doublets) ----
  {
    Heart h;
    int beats_seen = 0;
    for ( int t = 0; t < 10000; ++t )
    {
      HeartInput in;
      in.sa_rate_hz = 0;  // spikes only
      in.sa_spikes = ( t % 800 == 0 || t % 800 == 5 ) ? 1 : 0;  // 75 bpm with a doublet each cycle
      h.step( 1.0, in );
      beats_seen += h.beat_now;
    }
    CHECK_MSG( beats_seen >= 12 && beats_seen <= 13, std::to_string( beats_seen ) );
    CHECK( std::fabs( h.hr_bpm - 75 ) < 1 );
    CHECK( h.readouts().count( "map_mmHg" ) && h.readouts().count( "hr_bpm" ) );
  }
  // ---- 9. determinism ----
  {
    auto go = [] {
      Heart h;
      HeartInput in = at_hr( 72 );
      in.standing = 0.5;
      for ( int t = 0; t < 20000; ++t )
        h.step( 1.0, in );
      return h.pressure;
    };
    CHECK( go() == go() );
  }
  return chk::finish( "test_body_heart" );
}
