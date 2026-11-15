#include "bridge.h"
#include "spike/analysis.h"

namespace spike_gd
{
std::string motif_json( const spike::Engine& engine, int sign, int n_null, unsigned long long seed )
{
  try
  {
    auto edges = spike::analysis::edges_from_engine( engine );
    auto sg = sign == 1 ? spike::analysis::Sign::Excitatory : sign == 2 ? spike::analysis::Sign::Inhibitory : spike::analysis::Sign::Any;
    auto r = spike::analysis::motif_zscores( edges, engine.neuron_count(), sg, n_null, seed );
    return spike::analysis::motif_result_json( r );
  }
  catch ( const std::exception& e )
  {
    std::string m = e.what();
    for ( auto& c : m )
      if ( c == '"' || c == '\\' )
        c = '\'';
    return "{\"error\": \"" + m + "\"}";
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
