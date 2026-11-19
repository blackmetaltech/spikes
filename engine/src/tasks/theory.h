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
}  // namespace spike::tasks::tl
