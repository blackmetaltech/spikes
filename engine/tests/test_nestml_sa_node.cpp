// sa_node_pacemaker (models/nestml/sa_node_pacemaker.nestml, docs/models/sa_node_pacemaker.md):
// spontaneous beating at 60-80/min, slowed by ACh, sped up by NE, input-responsive, deterministic.
#include "nestml_util.h"

using namespace nm;

// beats per minute of one cell after a 4 s transient; mod rows map the engine's ACh / NE channels onto ach / ne
static double rate_bpm( double ach, double ne, double ms = 24000, const json& extra = json::object(), json* spikes_out = nullptr )
{
  json params = { { "V_m", -60.0 } };
  for ( auto& [ k, v ] : extra.items() )
    params[ k ] = v;
  spike::Engine e;
  json mod = json::array( { { { "modulator", "ACh" }, { "pop", "N" }, { "param", "ach" }, { "gain", 1.0 } },
    { { "modulator", "NE" }, { "pop", "N" }, { "param", "ne" }, { "gain", 1.0 } } } );
  CHECK_MSG( e.load( circuit( "sa_node_pacemaker", params, 1, 0.1, mod ).dump() ), e.error() );
  e.set_modulator( "ACh", ach );
  e.set_modulator( "NE", ne );
  auto t = after( run_spikes( e, ms ), 4000.0 );
  if ( spikes_out )
    *spikes_out = t;
  return t.size() < 2 ? 0.0 : 60000.0 / mean_isi( t );
}

static void test_baseline()
{
  json sp;
  double bpm = rate_bpm( 0, 0, 24000, json::object(), &sp );
  std::vector< double > t = sp.get< std::vector< double > >();
  std::printf( "  baseline %.1f beats/min over %zu beats, ISI CV %.4f\n", bpm, t.size(), isi_cv( t ) );
  CHECK( bpm >= 60.0 and bpm <= 80.0 );
  CHECK( t.size() >= 15 );
  CHECK( isi_cv( t ) < 0.02 );  // a limit cycle: strictly regular
}

static void test_membrane_waveform()
{
  spike::Engine e;
  CHECK_MSG( e.load( circuit( "sa_node_pacemaker", { { "V_m", -60.0 } } ).dump() ), e.error() );
  e.watch( 0 );
  e.step( 6000 );
  e.drain_voltage( 0 );
  double vmin = 1e9, vmax = -1e9;
  for ( int i = 0; i < 20; ++i )
  {
    e.step( 100 );
    for ( double v : e.drain_voltage( 0 ) )
    {
      vmin = std::min( vmin, v );
      vmax = std::max( vmax, v );
    }
  }
  std::printf( "  V_m range %.1f .. %.1f mV\n", vmin, vmax );
  CHECK( vmin > -85.0 and vmin < -55.0 );  // maximum diastolic potential
  CHECK( vmax > 0.0 and vmax < 40.0 );     // overshoot
}

static void test_ach_slows()
{
  double base = rate_bpm( 0, 0 ), a1 = rate_bpm( 0.3, 0 ), a2 = rate_bpm( 1.0, 0 );
  std::printf( "  ACh 0 / 0.3 / 1.0: %.1f / %.1f / %.1f beats/min\n", base, a1, a2 );
  CHECK( a1 < base and a2 < a1 );
  CHECK( a2 < 0.9 * base );  // ~20-25 % slowing at saturating ACh
  CHECK( a2 > 0.0 );
}

static void test_ne_speeds()
{
  double base = rate_bpm( 0, 0 ), n1 = rate_bpm( 0, 0.3 ), n2 = rate_bpm( 0, 1.0 );
  std::printf( "  NE 0 / 0.3 / 1.0: %.1f / %.1f / %.1f beats/min\n", base, n1, n2 );
  CHECK( n1 > base and n2 > n1 );
  CHECK( n2 > 1.25 * base );  // ~+50 % at saturating NE
  CHECK( n2 < 160.0 );
  // both together: sympathetic and vagal effects roughly cancel (accentuated antagonism not modelled)
  double both = rate_bpm( 0.3, 0.3 );
  CHECK( both > rate_bpm( 0.3, 0 ) and both < n1 );
}

static void test_param_via_set_neuron_param()
{
  spike::Engine e;
  CHECK_MSG( e.load( circuit( "sa_node_pacemaker", { { "V_m", -60.0 } } ).dump() ), e.error() );
  CHECK( e.set_neuron_param( 0, "ne", 1.0 ) );
  auto t = after( run_spikes( e, 20000 ), 4000.0 );
  double bpm = 60000.0 / mean_isi( t );
  std::printf("  set_neuron_param ne=1: %.1f beats/min\n", bpm);
  CHECK( bpm > 85.0 );
  CHECK( not e.set_neuron_param( 0, "no_such_param", 1.0 ) );
}

static double poisson_bpm( double hz, double w )
{
  json inputs = json::array( { { { "kind", "poisson" }, { "name", "p" }, { "targets", json::array( { "N" } ) }, { "value", hz }, { "weight", w } } } );
  spike::Engine e;
  CHECK_MSG( e.load( circuit( "sa_node_pacemaker", { { "V_m", -60.0 } }, 1, 0.1, json::array(), inputs ).dump() ), e.error() );
  auto t = after( run_spikes( e, 20000 ), 4000.0 );
  return t.size() < 2 ? 0.0 : 60000.0 / mean_isi( t );
}

static void test_input()
{
  double base = rate_bpm( 0, 0 );
  // DC current (continuous input port, and the I_e parameter): depolarising current speeds the cell up
  double dc = rate_bpm( 0, 0, 20000, { { "I_e", 3.0 } } ), hyp = rate_bpm( 0, 0, 20000, { { "I_e", -3.0 } } );
  std::printf( "  I_e +3 / -3 pA: %.1f / %.1f beats/min (baseline %.1f)\n", dc, hyp, base );
  CHECK( dc > base + 5.0 );
  CHECK( hyp < base - 5.0 );
  // synaptic input: positive weight -> excitatory conductance (E = 0 mV), negative -> inhibitory (E = -85 mV)
  double exc = poisson_bpm( 200.0, 0.03 ), inh = poisson_bpm( 200.0, -0.02 );
  std::printf( "  Poisson 200 Hz: excitatory 0.03 nS %.1f, inhibitory 0.02 nS %.1f beats/min\n", exc, inh );
  CHECK( std::fabs( exc - base ) > 3.0 );  // delivered and effective (strong depolarisation suppresses beating: depolarisation block)
  CHECK( inh < base - 3.0 and inh > 0.0 );
}

static void test_deterministic()
{
  json a, b;
  rate_bpm( 0, 0.5, 12000, json::object(), &a );
  rate_bpm( 0, 0.5, 12000, json::object(), &b );
  CHECK( a == b );
}

int main()
{
  if ( not available( "sa_node_pacemaker" ) )
    return 77;
  test_baseline();
  test_membrane_waveform();
  test_ach_slows();
  test_ne_speeds();
  test_param_via_set_neuron_param();
  test_input();
  test_deterministic();
  return chk::finish( "test_nestml_sa_node" );
}
