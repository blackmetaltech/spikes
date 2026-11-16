// icc_slow_wave (models/nestml/icc_slow_wave.nestml, docs/models/icc_slow_wave.md): gut slow-wave pacemaker,
// 3 cycles/min (period 20 s), amplitude ~25 mV between about -70 and -45 mV; ACh speeds, NE slows it; accepts input.
#include "nestml_util.h"

using namespace nm;

static double cycles_per_min( const json& params, const json& inputs = json::array(), double ms = 260000, std::vector< double >* out = nullptr )
{
  json p = params;
  spike::Engine e;
  CHECK_MSG( e.load( circuit( "icc_slow_wave", p, 1, 0.5, json::array(), inputs ).dump() ), e.error() );
  std::vector< double > t;
  for ( double done = 0; done < ms; done += 1000.0 )
  {
    e.step( 1000.0 );
    for ( auto& s : e.drain_spikes() )
      t.push_back( s.t_ms );
  }
  t = after( t, 40000.0 );
  if ( out )
    *out = t;
  return t.size() < 2 ? 0.0 : 60000.0 / mean_isi( t );
}

static void test_frequency()
{
  std::vector< double > t;
  double cpm = cycles_per_min( json::object(), json::array(), 260000, &t );
  std::printf( "  baseline %.3f cycles/min (period %.2f s), %zu cycles, ISI CV %.4f\n", cpm, 60.0 / cpm, t.size(), isi_cv( t ) );
  CHECK( cpm > 2.85 and cpm < 3.15 );  // target 3 cycles/min
  CHECK( t.size() >= 9 );
  CHECK( isi_cv( t ) < 0.01 );
}

static void test_waveform()
{
  spike::Engine e;
  CHECK_MSG( e.load( circuit( "icc_slow_wave", json::object(), 1, 0.5 ).dump() ), e.error() );
  e.watch( 0 );
  e.step( 60000 );
  e.drain_voltage( 0 );
  double vmin = 1e9, vmax = -1e9;
  std::vector< double > v;
  for ( int i = 0; i < 60; ++i )
  {
    e.step( 1000 );
    for ( double x : e.drain_voltage( 0 ) )
    {
      vmin = std::min( vmin, x );
      vmax = std::max( vmax, x );
      v.push_back( x );
    }
  }
  std::printf( "  V_m range %.1f .. %.1f mV\n", vmin, vmax );
  CHECK( vmin > -72.0 and vmin < -66.0 );
  CHECK( vmax > -48.0 and vmax < -42.0 );
  // plateau: spends a good fraction of the cycle above the half-amplitude level
  size_t hi = 0;
  for ( double x : v )
    hi += x > ( vmin + vmax ) / 2;
  double frac = double( hi ) / double( v.size() );
  std::printf( "  fraction of the cycle above half amplitude %.2f\n", frac );
  CHECK( frac > 0.25 and frac < 0.75 );
}

static void test_modulators()
{
  double base = cycles_per_min( json::object() );
  double ach = cycles_per_min( { { "ach", 1.0 } } ), ne = cycles_per_min( { { "ne", 1.0 } } );
  std::printf( "  ACh 1: %.3f, NE 1: %.3f (baseline %.3f) cycles/min\n", ach, ne, base );
  CHECK( ach > base * 1.03 );
  CHECK( ne < base * 0.95 and ne > 0.0 );
}

static void test_input()
{
  double base = cycles_per_min( json::object() );
  double dc = cycles_per_min( { { "I_e", 20.0 } } );
  std::printf( "  I_e = 20 pA: %.3f cycles/min\n", dc );
  CHECK( dc > base );
  // Poisson excitatory current input (weight in pA) also speeds the cycle
  json inputs = json::array( { { { "kind", "poisson" }, { "name", "p" }, { "targets", json::array( { "N" } ) }, { "value", 100.0 }, { "weight", 1.0 } } } );
  double syn = cycles_per_min( json::object(), inputs );
  std::printf( "  100 Hz x 1 pA excitatory input: %.3f cycles/min\n", syn );
  CHECK( syn > base );
}

int main()
{
  if ( not available( "icc_slow_wave" ) )
    return 77;
  test_frequency();
  test_waveform();
  test_modulators();
  test_input();
  return chk::finish( "test_nestml_icc" );
}
