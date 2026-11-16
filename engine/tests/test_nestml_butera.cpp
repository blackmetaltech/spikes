// butera_burster = Butera, Rinzel & Smith 1999 model 1 (persistent-Na bursting pacemaker, preBotzinger complex),
// docs/models/butera_burster.md. Published regime: with g_NaP = 2.8 nS the cell is silent at zero drive, bursts with
// period and duration set by the slow I_NaP inactivation (tau_h,max = 10 s) once tonic excitation g_tonic-e is added,
// bursts faster as drive grows, and finally spikes tonically.
#include "nestml_util.h"

using namespace nm;

struct Burst
{
  size_t spikes = 0;
  std::vector< std::pair< double, double > > b;
  double period_s = NAN, dur_s = NAN, spb = NAN;
};

static Burst run_burst( double g_nap, double g_tonic, double ms = 70000, double skip_ms = 10000, const json& extra = json::object() )
{
  json params = { { "V_m", -60.0 }, { "g_NaP", g_nap }, { "g_tonic", g_tonic } };
  for ( auto& [ k, v ] : extra.items() )
    params[ k ] = v;
  spike::Engine e;
  CHECK_MSG( e.load( circuit( "butera_burster", params ).dump() ), e.error() );
  auto t = after( run_spikes( e, ms ), skip_ms );
  Burst r;
  r.spikes = t.size();
  r.b = bursts( t, 500.0 );
  if ( r.b.size() >= 3 )
  {
    double dur = 0, nsp = 0;
    for ( auto& [ on, off ] : r.b )
      dur += off - on;
    r.dur_s = dur / double( r.b.size() ) / 1000.0;
    r.period_s = ( r.b.back().first - r.b.front().first ) / double( r.b.size() - 1 ) / 1000.0;
    r.spb = double( t.size() ) / double( r.b.size() );
    (void) nsp;
  }
  std::printf( "  g_NaP %.1f g_tonic %.2f: %zu spikes, %zu bursts, period %.2f s, duration %.2f s, %.1f spikes/burst\n", g_nap, g_tonic, r.spikes, r.b.size(),
    r.period_s, r.dur_s, r.spb );
  return r;
}

static void test_silent_without_drive()
{
  CHECK( run_burst( 2.8, 0.0 ).spikes == 0 );
}

static void test_bursting_regime()
{
  Burst b = run_burst( 2.8, 0.3 );
  CHECK( b.b.size() >= 8 );
  CHECK( b.period_s > 3.0 and b.period_s < 8.0 );  // seconds-long cycle: tau_h,max = 10 s
  CHECK( b.dur_s > 0.2 and b.dur_s < 1.2 );        // short active phase, long silent phase
  CHECK( b.spb > 5.0 );
  CHECK( b.dur_s < 0.3 * b.period_s );
}

static void test_drive_speeds_bursts()
{
  Burst lo = run_burst( 2.8, 0.30 ), mid = run_burst( 2.8, 0.35 ), hi = run_burst( 2.8, 0.40 );
  CHECK( lo.period_s > mid.period_s and mid.period_s > hi.period_s );
  CHECK( hi.period_s > 0.8 and hi.period_s < 2.5 );
}

static void test_tonic_at_strong_drive()
{
  Burst t = run_burst( 2.8, 0.6 );
  CHECK( t.b.size() <= 1 );              // no silent gaps >500 ms: tonic spiking
  CHECK( t.spikes > 3 * 60 );             // > 3 Hz over the 60 s analysed
}

static void test_gnap_dependence()
{
  CHECK( run_burst( 2.0, 0.4 ).spikes == 0 );        // too little I_NaP: quiescent
  Burst a = run_burst( 2.8, 0.30 ), b = run_burst( 3.5, 0.30 );
  CHECK( b.period_s > 0.0 and b.period_s < a.period_s );  // more I_NaP: faster bursting
}

static void test_modulators_add_drive()
{
  // ne adds excitatory conductance g_ne * ne (0.3 nS per unit): 0.3 nS tonic == ne 1 with g_tonic 0 is not identical
  // (different driver), but both rescue a silent cell
  Burst base = run_burst( 2.8, 0.0 );
  Burst ne = run_burst( 2.8, 0.0, 70000, 10000, { { "ne", 1.0 } } );
  CHECK( base.spikes == 0 and ne.spikes > 50 );
  Burst ne_lo = run_burst( 2.8, 0.0, 70000, 10000, { { "ne", 1.2 } } );
  CHECK( ne_lo.period_s < ne.period_s or ne_lo.spikes >= ne.spikes );
}

static void test_synaptic_input_drives()
{
  // a Poisson excitatory conductance input replaces g_tonic
  json inputs = json::array( { { { "kind", "poisson" }, { "name", "p" }, { "targets", json::array( { "N" } ) }, { "value", 500.0 }, { "weight", 0.05 } } } );
  spike::Engine e;
  CHECK_MSG( e.load( circuit( "butera_burster", { { "V_m", -60.0 }, { "g_tonic", 0.0 } }, 1, 0.1, json::array(), inputs ).dump() ), e.error() );
  auto t = after( run_spikes( e, 40000 ), 5000 );
  std::printf( "  500 Hz x 0.05 nS Poisson drive: %zu spikes in 35 s\n", t.size() );
  CHECK( t.size() > 20 );
}

int main()
{
  if ( not available( "butera_burster" ) )
    return 77;
  test_silent_without_drive();
  test_bursting_regime();
  test_drive_speeds_bursts();
  test_tonic_at_strong_drive();
  test_gnap_dependence();
  test_modulators_add_drive();
  test_synaptic_input_drives();
  return chk::finish( "test_nestml_butera" );
}
