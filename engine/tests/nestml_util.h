// Shared helpers for the NESTML custom-model tests (test_nestml_*.cpp).
#pragma once
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "check.h"
#include "../src/modules/nestml_models.h"
#include "nlohmann/json.hpp"
#include "spike/engine.h"

namespace nm
{
using nlohmann::json;

// True if the NESTML module was generated and compiled in; otherwise the test is skipped (ctest SKIP_RETURN_CODE 77).
inline bool available( const char* model )
{
  for ( const auto& n : spike::nestml_model_names() )
    if ( n == model )
      return true;
  std::printf( "SKIP: NESTML model %s not compiled in (run scripts/build-nestml.sh and reconfigure)\n", model );
  return false;
}

// One-population circuit: n cells of `model` with `params`; optional modulation rows and dc input.
inline json circuit( const std::string& model, const json& params, int n = 1, double res_ms = 0.1, const json& modulation = json::array(),
  const json& inputs = json::array() )
{
  json c;
  c[ "schema_version" ] = 1;
  c[ "name" ] = model + " test";
  c[ "resolution_ms" ] = res_ms;
  c[ "seed" ] = 1;
  c[ "populations" ] = json::array( { { { "id", "N" }, { "model", model }, { "n", n }, { "params", params } } } );
  c[ "inputs" ] = inputs;
  c[ "outputs" ] = json::array( { { { "name", "out" }, { "kind", "rate" }, { "neurons", json::array( { { { "pop", "N" }, { "idx", json::array( { 0 } ) } } } ) } } } );
  if ( not modulation.empty() )
    c[ "modulation" ] = modulation;
  c[ "record" ] = { { "spikes", "all" } };
  return c;
}

// Run `total_ms` in 100 ms steps, collect the spike times of neuron `idx`.
inline std::vector< double > run_spikes( spike::Engine& e, double total_ms, int idx = 0 )
{
  std::vector< double > t;
  for ( double done = 0; done < total_ms - 1e-9; done += 100.0 )
  {
    e.step( 100.0 );
    for ( const auto& s : e.drain_spikes() )
      if ( s.neuron == idx )
        t.push_back( s.t_ms );
  }
  return t;
}

inline std::vector< double > after( const std::vector< double >& t, double t0 )
{
  std::vector< double > r;
  for ( double x : t )
    if ( x >= t0 )
      r.push_back( x );
  return r;
}
inline double mean_isi( const std::vector< double >& t )
{
  return t.size() < 2 ? NAN : ( t.back() - t.front() ) / double( t.size() - 1 );
}
inline double isi_cv( const std::vector< double >& t )
{
  if ( t.size() < 3 )
    return NAN;
  double m = mean_isi( t ), v = 0;
  for ( size_t i = 1; i < t.size(); ++i )
    v += ( t[ i ] - t[ i - 1 ] - m ) * ( t[ i ] - t[ i - 1 ] - m );
  return std::sqrt( v / double( t.size() - 1 ) ) / m;
}
// Split spikes into bursts: a gap longer than gap_ms starts a new burst. Returns {onset, offset} pairs.
inline std::vector< std::pair< double, double > > bursts( const std::vector< double >& t, double gap_ms )
{
  std::vector< std::pair< double, double > > b;
  for ( size_t i = 0; i < t.size(); ++i )
  {
    if ( i == 0 or t[ i ] - t[ i - 1 ] > gap_ms )
      b.push_back( { t[ i ], t[ i ] } );
    else
      b.back().second = t[ i ];
  }
  return b;
}
} // namespace nm
