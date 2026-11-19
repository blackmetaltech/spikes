// Scaffolding for the Theory wing (docs/DESIGN-THEORY.md): helpers on top of bestiary_a.h for the levels that read a cell's own parameters (dynamics chapter)
// or its rate units (rates chapter). Same contract: mechanism checks read the circuit (Engine::structure(), the cell's live NEST parameters), verdicts and hint tiers 1-3 never
// carry a number.
#pragma once
#include <map>
#include <string>
#include <vector>
#include "bestiary_a.h"
#include "spike/theory.h"

namespace spike::tasks::tl
{
using namespace bestiary;

/// The cell a level judges: the first neuron of the output channel `ch` (its pool's first cell), with its model and the numeric parameters of the live NEST node.
struct Cell
{
  int32_t n = -1;
  std::string model;
  std::map< std::string, double > params;
  bool ok() const { return n >= 0; }
};
inline Cell cell_of( Engine& e, const std::string& ch )
{
  Cell c;
  for ( const auto& o : e.outputs() )
    if ( o.name == ch && !o.neurons.empty() ) c.n = o.neurons.front();
  if ( c.n < 0 ) return c;
  const Structure st = e.structure();
  if ( const PopulationInfo* p = st.population( e.neuron_population( c.n ) ) ) c.model = p->model;
  for ( const auto& k : theory::phase_plane_params( c.model ) )
  {
    const double v = e.neuron_state( c.n, k );
    if ( std::isfinite( v ) ) c.params[ k ] = v;
  }
  return c;
}
/// Weight (pA per unit of the channel's level) the circuit gives the dc input `ch` onto neuron `n`; 0 when the channel does not reach it.
inline double dc_weight( const Engine& e, int32_t n, const std::string& ch )
{
  for ( const auto& in : e.inputs() )
    if ( in.name == ch && std::find( in.neurons.begin(), in.neurons.end(), n ) != in.neurons.end() ) return e.input_weight( ch );
  return 0.0;
}
/// Start every two-variable cell of the circuit at its resting state with no input (the stable fixed point of its phase plane): NEST starts an Izhikevich cell at U = 0, which is not rest and
/// makes a resonator fire once at the start, and an aeif cell at E_L, which is only close. A cell with no stable fixed point is left alone.
inline void rest_state( Engine& e )
{
  const Structure st = e.structure();
  for ( const auto& p : st.populations )
    if ( theory::phase_plane_supported( p.model ) )
      for ( int32_t k = 0; k < p.n; ++k )
      {
        const int32_t n = p.first_index + k;
        const theory::PhasePlane pp = theory::phase_plane_of( e, n, 0.0 );
        const theory::FixedPoint* fp = nullptr;
        for ( const auto& f : pp.fixed )
          if ( f.stable && ( !fp || f.v < fp->v ) ) fp = &f;
        if ( !fp ) continue;
        e.set_neuron_param( n, "V_m", fp->v );
        e.set_neuron_param( n, pp.w_name == "U" ? "U_m" : "w", fp->w );
      }
}
inline std::string need_two_variables()
{
  return "This level is about the shape of the resting state in the phase plane, and that needs a cell with a recovery variable as well as a voltage: an Izhikevich or an adaptive "
         "exponential cell. A plain integrate-and-fire cell has one variable and no phase plane.";
}
/// The largest current the onset scan looks at (in the model's own current unit: pA for aeif, the model's units for Izhikevich).
inline double scan_max( const std::string& model ) { return model == "izhikevich" ? 100.0 : 3000.0; }

// ---------------------------------------------------------------- rate units (rates chapter)
inline bool is_rate_model( const std::string& m ) { return m == "siegert_neuron" || m.find( "rate" ) != std::string::npos; }
inline bool all_rate( const Graph& g, const std::vector< int >& pops )
{
  if ( pops.empty() ) return false;
  for ( int p : pops )
    if ( !is_rate_model( g.pop( p ).model ) ) return false;
  return true;
}
inline std::string need_rate_units( const std::string& what )
{
  return "This level is about " + what + ", so the cells have to be rate units (a population as one number), not spiking cells: add a rate unit from the parts bin and read the motor channels from it.";
}
/// Times (ms, linearly interpolated) at which the series `v` over [t0, t1) crosses `mid` upwards, with a hysteresis of `hyst` so that ripple does not count.
inline std::vector< double > up_crossings( const std::vector< float >& v, double t0, double t1, double mid, double hyst )
{
  std::vector< double > out;
  bool low = false;
  for ( long k = std::max( 0L, static_cast< long >( t0 ) ) + 1; k < static_cast< long >( t1 ) && k < static_cast< long >( v.size() ); ++k )
  {
    if ( v[ static_cast< size_t >( k ) ] < mid - hyst ) low = true;
    if ( low && v[ static_cast< size_t >( k - 1 ) ] < mid && v[ static_cast< size_t >( k ) ] >= mid )
    {
      const double a = v[ static_cast< size_t >( k - 1 ) ], b = v[ static_cast< size_t >( k ) ];
      out.push_back( static_cast< double >( k - 1 ) + ( b > a ? ( mid - a ) / ( b - a ) : 0.0 ) );
      low = false;
    }
  }
  return out;
}
/// The lag (ms, 0 .. max_lag) at which series `y` best follows series `x` over [t0, t1): the maximum of the correlation of x(t - lag) with y(t).
inline double best_lag( const std::vector< float >& x, const std::vector< float >& y, double t0, double t1, double max_lag )
{
  double best = -2, bl = 0;
  const long a = static_cast< long >( t0 ) + static_cast< long >( max_lag ), b = std::min( static_cast< long >( t1 ), static_cast< long >( std::min( x.size(), y.size() ) ) );
  if ( b <= a + 10 ) return 0;
  for ( long lag = 0; lag <= static_cast< long >( max_lag ); ++lag )
  {
    double mx = 0, my = 0;
    for ( long k = a; k < b; ++k ) { mx += x[ static_cast< size_t >( k - lag ) ]; my += y[ static_cast< size_t >( k ) ]; }
    mx /= static_cast< double >( b - a );
    my /= static_cast< double >( b - a );
    double sxy = 0, sxx = 0, syy = 0;
    for ( long k = a; k < b; ++k )
    {
      const double dx = x[ static_cast< size_t >( k - lag ) ] - mx, dy = y[ static_cast< size_t >( k ) ] - my;
      sxy += dx * dy;
      sxx += dx * dx;
      syy += dy * dy;
    }
    const double c = sxx > 1e-12 && syy > 1e-12 ? sxy / std::sqrt( sxx * syy ) : 0.0;
    if ( c > best ) { best = c; bl = static_cast< double >( lag ); }
  }
  return bl;
}
}  // namespace spike::tasks::tl
