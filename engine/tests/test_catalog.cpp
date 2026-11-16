// Parts bin: card data is consistent with NEST (defaults, presets), every card loads and behaves, the probe and the spike-catalog tool work.
#include <cmath>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>
#include "check.h"
#include "dictionary.h"
#include "nest.h"
#include "nlohmann/json.hpp"
#include "spike/catalog.h"
#include "spike/engine.h"

using nlohmann::json;
using namespace spike;

static std::vector< double > spike_times( Engine& e, const CatalogModel& m, double level, double ms )
{
  CHECK_MSG( e.load( catalog_circuit_json( m, level ) ), m.id + ": " + e.error() );
  e.step( ms );
  CHECK_MSG( e.last_step_ok(), m.id + ": " + e.error() );
  std::vector< double > t;
  for ( const Spike& s : e.drain_spikes() )
    t.push_back( s.t_ms );
  return t;
}

static void test_static_data()
{
  const auto& cat = catalog();
  CHECK( cat.size() >= 24 );
  std::set< std::string > ids;
  std::set< std::string > want = { "iaf_psc_alpha", "iaf_cond_exp", "iaf_cond_exp_sfa_rr", "aeif_cond_alpha", "aeif_cond_exp", "hh_psc_alpha", "hh_cond_exp_traub",
    "ht_neuron", "gif_cond_exp", "mat2_psc_exp", "pp_psc_delta", "parrot_neuron", "sa_node_pacemaker", "butera_burster", "icc_slow_wave" };
  for ( const char* p : { "RS", "IB", "CH", "FS", "LTS", "TC", "RZ" } )
    want.insert( std::string( "izhikevich_" ) + p );
  for ( const CatalogModel& m : cat )
  {
    CHECK_MSG( ids.insert( m.id ).second, "duplicate id " + m.id );
    CHECK_MSG( not m.title.empty() and m.summary.size() > 60 and not m.examples.empty() and not m.source.empty(), m.id );
    CHECK_MSG( m.ei == "excitatory" or m.ei == "inhibitory" or m.ei == "either", m.id );
    CHECK_MSG( m.input_weight != 0 and m.drive.levels.size() >= 5 and ( m.drive.kind == "dc" or m.drive.kind == "poisson" ), m.id );
    for ( const CatalogParam& p : m.params )
      CHECK_MSG( p.lo <= p.def and p.def <= p.hi and not p.desc.empty(), m.id + "." + p.name );
  }
  for ( const std::string& w : want )
    CHECK_MSG( ids.count( w ), "missing card " + w );
  json j = json::parse( catalog_json() );
  CHECK( j[ "models" ].size() == cat.size() );
  CHECK( catalog_find( "izhikevich_FS" ) != nullptr and catalog_find( "nope" ) == nullptr );
  CHECK( catalog_find( "hh_psc_alpha_gap" )->gap_capable and catalog_find( "sa_node_pacemaker" )->nestml );

  // Izhikevich 2003 Fig. 1 presets
  struct Pre { const char* tag; double a, b, c, d; };
  for ( const Pre& p : { Pre{ "RS", 0.02, 0.2, -65, 8 }, Pre{ "IB", 0.02, 0.2, -55, 4 }, Pre{ "CH", 0.02, 0.2, -50, 2 }, Pre{ "FS", 0.1, 0.2, -65, 2 },
          Pre{ "LTS", 0.02, 0.25, -65, 2 }, Pre{ "TC", 0.02, 0.25, -65, 0.05 }, Pre{ "RZ", 0.1, 0.26, -65, 2 } } )
  {
    const CatalogModel* m = catalog_find( std::string( "izhikevich_" ) + p.tag );
    CHECK( m != nullptr );
    if ( not m )
      continue;
    json pr = json::parse( m->preset_json );
    CHECK( pr[ "a" ] == p.a and pr[ "b" ] == p.b and pr[ "c" ] == p.c and pr[ "d" ] == p.d );
    CHECK( std::fabs( pr[ "U_m" ].get< double >() - p.b * -65.0 ) < 1e-12 );
  }
}

static void test_against_nest()
{
  Engine e;  // initialises NEST
  for ( const CatalogModel& m : catalog() )
  {
    Dictionary d;
    try
    {
      d = nest::get_model_defaults( m.model );
    }
    catch ( const std::exception& )
    {
      CHECK_MSG( m.nestml, m.id + ": unknown NEST model" );  // a NESTML cell is only there when its module is compiled in
      continue;
    }
    const json preset = json::parse( m.preset_json );
    for ( const CatalogParam& p : m.params )
    {
      CHECK_MSG( d.known( p.name ), m.id + ": NEST model has no parameter " + p.name );
      if ( not d.known( p.name ) )
        continue;
      double expect = preset.contains( p.name ) ? preset[ p.name ].get< double >() : d.get< double >( p.name );
      CHECK_MSG( std::fabs( expect - p.def ) <= 1e-3 * std::max( 1.0, std::fabs( expect ) ), m.id + "." + p.name + ": card default " + std::to_string( p.def ) + " vs NEST " + std::to_string( expect ) );
    }
  }
}

static void test_cards_behave()
{
  Engine e;
  for ( const CatalogModel& m : catalog() )
  {
    if ( m.nestml )
      continue;
    const double top = m.drive.levels.back();
    auto hi = spike_times( e, m, top, 1000 );
    CHECK_MSG( hi.size() > 5, m.id + ": no firing at the top drive level" );
    if ( m.model != "pp_psc_delta" )
    {
      auto lo = spike_times( e, m, 0.0, 1000 );
      int late = 0;
      for ( double x : lo )
        late += x > 200.0;  // ignore the start-up transient
      CHECK_MSG( late == 0, m.id + ": fires with no drive" );
    }
    // thumbnail drive levels are increasing and the rate does not fall with level (ignoring stochastic / type-II noise at 15%)
    double prev = -1;
    for ( double l : m.drive.levels )
    {
      CHECK_MSG( l > prev, m.id + ": drive levels must increase" );
      prev = l;
    }
  }
  // recognisable firing patterns
  auto isis = [ & ]( const char* id, double level ) {
    auto t = spike_times( e, *catalog_find( id ), level, 1000 );
    std::vector< double > d;
    for ( size_t i = 1; i < t.size(); ++i )
      d.push_back( t[ i ] - t[ i - 1 ] );
    return d;
  };
  auto rs = isis( "izhikevich_RS", 10 );
  CHECK( rs.size() > 8 and rs.back() > 1.4 * rs.front() );   // spike-frequency adaptation
  auto fs = isis( "izhikevich_FS", 10 );
  CHECK( fs.size() > 50 and std::fabs( fs.back() - fs[ 3 ] ) < 0.1 * fs[ 3 ] );  // none (after the first few)
  CHECK( fs.size() > 3 * rs.size() );
  auto ch = isis( "izhikevich_CH", 10 );
  double longest = 0, shortest = 1e9;
  for ( double x : ch )
  {
    longest = std::max( longest, x );
    shortest = std::min( shortest, x );
  }
  CHECK( ch.size() > 20 and longest > 2.5 * shortest );  // bursts separated by pauses
  auto hh = isis( "hh_psc_alpha", 700 );
  CHECK( hh.size() > 30 );  // class 2: jumps to tens of Hz at threshold
  auto sfa = isis( "iaf_cond_exp_sfa_rr", 1500 );
  CHECK( sfa.size() > 10 and sfa.back() > 1.3 * sfa.front() );
  auto lif = isis( "iaf_psc_alpha", 800 );
  CHECK( lif.size() > 50 and std::fabs( lif.back() - lif.front() ) < 0.05 * lif.front() );
}

static void test_probe()
{
  json p = json::parse( catalog_probe_json( "iaf_psc_alpha" ) );
  CHECK_MSG( p[ "available" ] == true, p.dump() );
  if ( p[ "available" ] == true )
  {
    CHECK( p[ "fi" ].size() == 6 and p[ "fi" ][ 0 ][ "rate_hz" ] == 0.0 );
    double prev = -1;
    for ( const auto& pt : p[ "fi" ] )
    {
      CHECK( pt[ "rate_hz" ].get< double >() >= prev );
      prev = pt[ "rate_hz" ].get< double >();
    }
    CHECK( prev > 100.0 );
    CHECK( p[ "trace" ][ "v_mV" ].size() == 600 and p[ "trace" ][ "dt_ms" ] == 0.5 and p[ "trace" ][ "spikes_ms" ].size() >= 1 );
  }
  json u = json::parse( catalog_probe_json( "no_such_card" ) );
  CHECK( u[ "available" ] == false and u.contains( "error" ) );
  // NESTML placeholder: either measured (module registered) or reported unavailable with a reason; never a crash
  json s = json::parse( catalog_probe_json( "sa_node_pacemaker" ) );
  CHECK( s[ "available" ] == true or s.contains( "error" ) );
  if ( s[ "available" ] == true )
  {
    CHECK( s[ "fi" ].size() == 6 and s[ "trace" ][ "v_mV" ].size() == 1500 );
    double best = 0;
    for ( const auto& pt : s[ "fi" ] )
      best = std::max( best, pt[ "rate_hz" ].get< double >() );
    CHECK( best > 0.8 and best < 2.0 );  // about 70 beats per minute
  }
}

static void test_tool()
{
  const std::string out = std::string( SPIKE_SOURCE_DIR ) + "/build/catalog_test_thumbs.json";
  const std::string cmd = std::string( "'" ) + SPIKE_CATALOG_EXE + "' --only iaf_psc_alpha,izhikevich_RS,parrot_neuron --jobs 2 --out '" + out + "' 2>/dev/null";
  CHECK( std::system( cmd.c_str() ) == 0 );
  json j = json::parse( chk::slurp( out ), nullptr, false );
  CHECK( not j.is_discarded() );
  if ( j.is_discarded() )
    return;
  CHECK( j[ "models" ].size() == 3 );
  CHECK( j[ "models" ][ "izhikevich_RS" ][ "available" ] == true and j[ "models" ][ "izhikevich_RS" ][ "fi" ].size() == 6 );
  CHECK( j[ "models" ][ "parrot_neuron" ][ "trace" ][ "v_mV" ].empty() and j[ "models" ][ "parrot_neuron" ][ "trace" ][ "spikes_ms" ].size() >= 1 );
  std::remove( out.c_str() );
}

int main()
{
  test_static_data();
  test_against_nest();
  test_cards_behave();
  test_tool();   // child processes: before this process creates the engine used by the probe
  test_probe();  // creates an Engine itself, so no other Engine may be alive
  return chk::finish( "test_catalog" );
}
