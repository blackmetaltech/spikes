#include "bridge.h"
#include "spike/analysis.h"

namespace spike_gd
{
static std::string error_json( const std::string& what )
{
  std::string m;
  for ( unsigned char c : what )
  {
    if ( c == '"' || c == '\\' )
      ( m += '\\' ) += static_cast< char >( c );
    else if ( c < 0x20 )
      m += ' ';
    else
      m += static_cast< char >( c );
  }
  return "{\"error\": \"" + m + "\"}";
}

static spike::analysis::Sign to_sign( int sign )
{
  return sign == 1 ? spike::analysis::Sign::Excitatory : sign == 2 ? spike::analysis::Sign::Inhibitory : spike::analysis::Sign::Any;
}

std::string motif_json( const spike::Engine& engine, int sign, int n_null, unsigned long long seed )
{
  try
  {
    auto edges = spike::analysis::edges_from_engine( engine );
    auto r = spike::analysis::motif_zscores( edges, engine.neuron_count(), to_sign( sign ), n_null, seed );
    return spike::analysis::motif_result_json( r );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

std::string motif_json_edges( const std::vector< double >& triples, int n, int sign, int n_null, unsigned long long seed )
{
  try
  {
    std::vector< spike::analysis::Edge > edges;
    for ( size_t i = 0; i + 2 < triples.size(); i += 3 )
      edges.push_back( { static_cast< int32_t >( triples[ i ] ), static_cast< int32_t >( triples[ i + 1 ] ), triples[ i + 2 ] } );
    auto r = spike::analysis::motif_zscores( edges, n, to_sign( sign ), n_null, seed );
    return spike::analysis::motif_result_json( r );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

Stats spike_stats( const std::vector< spike::Spike >& spikes, int n, double t0_ms, double t1_ms )
{
  Stats s;
  s.rates = spike::analysis::firing_rates( spikes, n, t0_ms, t1_ms );
  s.cv = spike::analysis::isi_cv( spikes, n, t0_ms, t1_ms );
  s.chi = spike::analysis::golomb_chi( spikes, n, t0_ms, t1_ms );
  return s;
}
} // namespace spike_gd
