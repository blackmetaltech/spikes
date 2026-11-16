#include "bridge.h"
#include "spike/analysis.h"

#include <algorithm>
#include <cstdio>

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

std::string burst_json( const std::vector< spike::Spike >& spikes, int n, const std::vector< int >& neurons, double t0_ms, double t1_ms,
  double max_isi_start_ms, double max_isi_end_ms, double min_ibi_ms, double min_duration_ms, int min_spikes )
{
  try
  {
    spike::analysis::BurstParams p;
    p.max_isi_start_ms = max_isi_start_ms;
    p.max_isi_end_ms = max_isi_end_ms;
    p.min_ibi_ms = min_ibi_ms;
    p.min_duration_ms = min_duration_ms;
    p.min_spikes = min_spikes;
    const std::vector< int32_t > sel( neurons.begin(), neurons.end() );
    const auto t = spike::analysis::pooled_spike_times( spikes, n, sel, t0_ms, t1_ms );
    return spike::analysis::bursts_json( spike::analysis::detect_bursts( t, p ), static_cast< int64_t >( t.size() ), p );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

std::string prc_json( const std::vector< spike::Spike >& spikes, int n, const std::vector< int >& neurons, const std::vector< double >& perturb_ms,
  double period_ms, double merge_gap_ms, int nbins )
{
  try
  {
    const std::vector< int32_t > sel( neurons.begin(), neurons.end() );
    return spike::analysis::prc_json( spike::analysis::prc_estimate( spikes, n, sel, perturb_ms, period_ms, merge_gap_ms ), nbins );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

static void json_array( std::string& out, const char* key, const std::vector< double >& v )
{
  out += std::string( ",\"" ) + key + "\":[";
  char buf[ 40 ];
  for ( size_t i = 0; i < v.size(); ++i )
  {
    std::snprintf( buf, sizeof buf, "%.10g", v[ i ] );
    out += ( i ? "," : "" );
    out += buf;
  }
  out += "]";
}

std::string prc_experiment_json( spike::Engine& engine, const std::string& channel, double baseline, double pulse_value, double pulse_ms, int n_phases,
  const std::vector< int >& neurons, double merge_gap_ms, int repeats, int nbins )
{
  try
  {
    spike::analysis::PrcExperiment cfg;
    cfg.channel = channel;
    cfg.baseline = baseline;
    cfg.pulse_value = pulse_value;
    cfg.pulse_ms = pulse_ms;
    cfg.n_phases = n_phases;
    cfg.neurons.assign( neurons.begin(), neurons.end() );
    cfg.merge_gap_ms = merge_gap_ms;
    cfg.repeats = repeats;
    const auto r = spike::analysis::run_prc_experiment( engine, cfg );
    if ( !r.ok )
      return error_json( r.error );
    std::string j = spike::analysis::prc_json( r.prc, nbins );
    j.pop_back();  // reopen the object
    json_array( j, "spikes_ms", r.spike_ms );
    json_array( j, "pulses_ms", r.pulse_ms );
    return j + "}";
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

std::string hrv_json( const std::vector< double >& beat_times_ms )
{
  try
  {
    return spike::analysis::hrv_json( spike::analysis::hrv_analyze( beat_times_ms ) );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

static std::string motif4_run( const spike::analysis::Graph& g, int n_null, unsigned long long seed, double max_work )
{
  const double work = spike::analysis::motif4_work_estimate( g ) * ( 1.0 + std::max( n_null, 0 ) );
  if ( max_work > 0 && work > max_work )
    return error_json( "4-node census too expensive for this graph (estimated work " + std::to_string( static_cast< long long >( work ) )
      + " > limit " + std::to_string( static_cast< long long >( max_work ) ) + "); use fewer null samples or the 3-node census" );
  return spike::analysis::motif4_result_json( spike::analysis::motif4_zscores( g, n_null, seed ) );
}

std::string motif4_json( const spike::Engine& engine, int sign, int n_null, unsigned long long seed, double max_work )
{
  try
  {
    auto edges = spike::analysis::edges_from_engine( engine );
    return motif4_run( spike::analysis::make_graph( edges, engine.neuron_count(), to_sign( sign ) ), n_null, seed, max_work );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}

std::string motif4_json_edges( const std::vector< double >& triples, int n, int sign, int n_null, unsigned long long seed, double max_work )
{
  try
  {
    std::vector< spike::analysis::Edge > edges;
    for ( size_t i = 0; i + 2 < triples.size(); i += 3 )
      edges.push_back( { static_cast< int32_t >( triples[ i ] ), static_cast< int32_t >( triples[ i + 1 ] ), triples[ i + 2 ] } );
    return motif4_run( spike::analysis::make_graph( edges, n, to_sign( sign ) ), n_null, seed, max_work );
  }
  catch ( const std::exception& e )
  {
    return error_json( e.what() );
  }
}
} // namespace spike_gd
