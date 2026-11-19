// Measures a catalog card with the engine: f-I curve points and a short V_m trace (used by the spike-catalog tool).
#include <algorithm>
#include <cmath>
#include <vector>

#include "nlohmann/json.hpp"
#include "spike/catalog.h"
#include "spike/engine.h"

namespace spike
{
using nlohmann::json;

namespace
{
double round_to( double x, double q ) { return std::round( x / q ) * q; }
} // namespace

std::string catalog_probe_json( const std::string& id )
{
  json out = { { "id", id }, { "available", false } };
  const CatalogModel* m = catalog_find( id );
  if ( not m )
  {
    out[ "error" ] = "unknown catalog id";
    return out.dump();
  }
  const double kRunMs = m->run_ms, kSkipMs = m->skip_ms, kTraceMs = m->trace_ms, kTraceDt = m->trace_dt_ms;
  out[ "drive" ] = { { "kind", m->drive.kind }, { "unit", m->drive.unit }, { "levels", m->drive.levels } };
  try
  {
    Engine e;
    if ( m->drive.kind == "rate_drive" )
    {
      // rate models: the "f-I" points are the steady rate (rate_value) at each drive level; there is no voltage trace
      json rfi = json::array();
      for ( double level : m->drive.levels )
      {
        if ( not e.load( catalog_circuit_json( *m, level ) ) )
        {
          out[ "error" ] = e.error();
          return out.dump();
        }
        e.step( kRunMs );
        if ( not e.last_step_ok() )
        {
          out[ "error" ] = e.error();
          return out.dump();
        }
        rfi.push_back( { { "level", level }, { "rate_hz", round_to( e.output_value( "out" ), 0.001 ) } } );
      }
      out[ "fi" ] = rfi;
      out[ "trace" ] = { { "level", m->drive.levels.empty() ? 0.0 : m->drive.levels.back() }, { "t0_ms", kSkipMs }, { "dt_ms", kTraceDt }, { "v_mV", json::array() }, { "spikes_ms", json::array() } };
      out[ "available" ] = true;
      return out.dump();
    }
    json fi = json::array();
    double best = 1e18, best_level = m->drive.levels.empty() ? 0.0 : m->drive.levels.back();
    for ( double level : m->drive.levels )
    {
      if ( not e.load( catalog_circuit_json( *m, level ) ) )
      {
        out[ "error" ] = e.error();
        return out.dump();
      }
      // run in chunks (extended for slow cells until min_cycles spikes were seen or max_ms is reached)
      const double kMaxMs = std::max( m->max_ms, kRunMs );
      double elapsed = 0;
      std::vector< double > times;
      for ( ;; )
      {
        const double chunk = elapsed == 0 ? kRunMs : kRunMs - kSkipMs;
        e.step( chunk );
        elapsed += chunk;
        if ( not e.last_step_ok() )
        {
          out[ "error" ] = e.error();
          return out.dump();
        }
        for ( const Spike& s : e.drain_spikes() )
          if ( s.t_ms >= kSkipMs )
            times.push_back( s.t_ms );
        if ( static_cast< int >( times.size() ) > m->min_cycles or elapsed + 1e-9 >= kMaxMs )
          break;
      }
      std::sort( times.begin(), times.end() );
      const int n = static_cast< int >( times.size() );
      const double hz = n >= 2 and m->max_ms > m->run_ms ? ( n - 1 ) * 1000.0 / ( times.back() - times.front() ) : n * 1000.0 / ( elapsed - kSkipMs );
      fi.push_back( { { "level", level }, { "rate_hz", round_to( hz, 0.001 ) } } );
      if ( hz > 0 and std::fabs( hz - 20.0 ) < best )
      {
        best = std::fabs( hz - 20.0 );
        best_level = level;
      }
    }
    out[ "fi" ] = fi;
    // trace at the level whose rate is nearest 20 Hz (the highest level if none fires)
    if ( not e.load( catalog_circuit_json( *m, best_level ) ) )
    {
      out[ "error" ] = e.error();
      return out.dump();
    }
    e.watch( 0 );
    if ( kSkipMs > 0 )
      e.step( kSkipMs );
    e.drain_spikes();
    e.drain_voltage( 0 );
    const int k = static_cast< int >( std::llround( kTraceMs / kTraceDt ) );
    json v = json::array(), sp = json::array();
    for ( int i = 0; i < k; ++i )
      e.step( kTraceDt );
    for ( double x : e.drain_voltage( 0 ) )
      v.push_back( std::isfinite( x ) ? json( round_to( x, 0.01 ) ) : json( nullptr ) );
    for ( const Spike& s : e.drain_spikes() )
      sp.push_back( round_to( s.t_ms - kSkipMs, 0.1 ) );
    // keep it a pure-null trace out of the file for models without V_m (parrot)
    bool any = std::any_of( v.begin(), v.end(), []( const json& x ) { return not x.is_null(); } );
    out[ "trace" ] = { { "level", best_level }, { "t0_ms", kSkipMs }, { "dt_ms", kTraceDt }, { "v_mV", any ? v : json::array() }, { "spikes_ms", sp } };
    out[ "available" ] = true;
  }
  catch ( const std::exception& ex )
  {
    out[ "error" ] = ex.what();
  }
  return out.dump();
}

} // namespace spike
