// Max-interval burst detection (Cocatre-Zilgien & Delcomyn 1992). Pure C++; python/spike/bursts.py mirrors it.
#include <algorithm>

#include "json_util.h"
#include "spike/analysis.h"

namespace spike::analysis
{

std::vector< Burst > detect_bursts( const std::vector< double >& spike_times_ms, const BurstParams& p )
{
  std::vector< double > t = spike_times_ms;
  std::sort( t.begin(), t.end() );
  const int64_t n = static_cast< int64_t >( t.size() );
  // 1. raw bursts (first, last spike index), spikes belong to at most one burst
  std::vector< std::pair< int64_t, int64_t > > raw;
  for ( int64_t i = 0; i + 1 < n; )
  {
    if ( t[ i + 1 ] - t[ i ] <= p.max_isi_start_ms )
    {
      int64_t j = i + 1;
      while ( j + 1 < n && t[ j + 1 ] - t[ j ] <= p.max_isi_end_ms )
        ++j;
      raw.emplace_back( i, j );
      i = j + 1;
    }
    else
      ++i;
  }
  // 2. merge bursts closer than min_ibi
  std::vector< std::pair< int64_t, int64_t > > merged;
  for ( const auto& b : raw )
  {
    if ( !merged.empty() && t[ b.first ] - t[ merged.back().second ] < p.min_ibi_ms )
      merged.back().second = b.second;
    else
      merged.push_back( b );
  }
  // 3. filter
  std::vector< Burst > out;
  for ( const auto& b : merged )
  {
    const int64_t ns = b.second - b.first + 1;
    const double dur = t[ b.second ] - t[ b.first ];
    if ( dur < p.min_duration_ms || ns < p.min_spikes )
      continue;
    out.push_back( Burst{ t[ b.first ], t[ b.second ], dur, static_cast< int32_t >( ns ), b.first } );
  }
  return out;
}

std::vector< double > pooled_spike_times( const std::vector< Spike >& spikes, int32_t n, const std::vector< int32_t >& neurons, double t0_ms,
  double t1_ms )
{
  std::vector< char > mask( n, neurons.empty() ? 1 : 0 );
  for ( int32_t i : neurons )
    if ( i >= 0 && i < n )
      mask[ i ] = 1;
  std::vector< double > t;
  for ( const auto& s : spikes )
    if ( s.neuron >= 0 && s.neuron < n && mask[ s.neuron ] && s.t_ms >= t0_ms && s.t_ms < t1_ms )
      t.push_back( s.t_ms );
  std::sort( t.begin(), t.end() );
  return t;
}

std::vector< std::vector< Burst > > detect_bursts( const std::vector< Spike >& spikes, int32_t n, double t0_ms, double t1_ms,
  const BurstParams& p )
{
  std::vector< std::vector< double > > t( n );
  for ( const auto& s : spikes )
    if ( s.neuron >= 0 && s.neuron < n && s.t_ms >= t0_ms && s.t_ms < t1_ms )
      t[ s.neuron ].push_back( s.t_ms );
  std::vector< std::vector< Burst > > out( n );
  for ( int32_t i = 0; i < n; ++i )
    out[ i ] = detect_bursts( t[ i ], p );
  return out;
}

BurstSummary summarize_bursts( const std::vector< Burst >& b, int64_t n_spikes )
{
  BurstSummary s;
  s.n_bursts = static_cast< int32_t >( b.size() );
  s.n_spikes = n_spikes;
  if ( b.empty() )
    return s;
  double dur = 0, isi = 0;
  for ( const auto& x : b )
  {
    s.spikes_in_bursts += x.n_spikes;
    dur += x.duration_ms;
    isi += x.duration_ms / static_cast< double >( x.n_spikes - 1 );
  }
  const double nb = static_cast< double >( b.size() );
  s.fraction_in_bursts = n_spikes > 0 ? static_cast< double >( s.spikes_in_bursts ) / static_cast< double >( n_spikes ) : 0.0;
  s.mean_duration_ms = dur / nb;
  s.mean_spikes_per_burst = static_cast< double >( s.spikes_in_bursts ) / nb;
  s.mean_intra_burst_isi_ms = isi / nb;
  if ( b.size() >= 2 )
    s.mean_period_ms = ( b.back().onset_ms - b.front().onset_ms ) / ( nb - 1 );
  return s;
}

std::string bursts_json( const std::vector< Burst >& b, int64_t n_spikes, const BurstParams& p )
{
  using detail::jnum;
  std::ostringstream o;
  o << "{\"params\":{\"max_isi_start_ms\":";
  jnum( o, p.max_isi_start_ms );
  o << ",\"max_isi_end_ms\":";
  jnum( o, p.max_isi_end_ms );
  o << ",\"min_ibi_ms\":";
  jnum( o, p.min_ibi_ms );
  o << ",\"min_duration_ms\":";
  jnum( o, p.min_duration_ms );
  o << ",\"min_spikes\":" << p.min_spikes << "},\"n_bursts\":" << b.size();
  const auto s = summarize_bursts( b, n_spikes );
  o << ",\"summary\":{\"n_spikes\":" << s.n_spikes << ",\"spikes_in_bursts\":" << s.spikes_in_bursts << ",\"fraction_in_bursts\":";
  jnum( o, s.fraction_in_bursts );
  o << ",\"mean_duration_ms\":";
  jnum( o, s.mean_duration_ms );
  o << ",\"mean_spikes_per_burst\":";
  jnum( o, s.mean_spikes_per_burst );
  o << ",\"mean_period_ms\":";
  jnum( o, s.mean_period_ms );
  o << ",\"mean_intra_burst_isi_ms\":";
  jnum( o, s.mean_intra_burst_isi_ms );
  o << "},\"bursts\":[";
  for ( size_t i = 0; i < b.size(); ++i )
  {
    o << ( i ? "," : "" ) << "{\"onset_ms\":";
    jnum( o, b[ i ].onset_ms );
    o << ",\"offset_ms\":";
    jnum( o, b[ i ].offset_ms );
    o << ",\"duration_ms\":";
    jnum( o, b[ i ].duration_ms );
    o << ",\"n_spikes\":" << b[ i ].n_spikes << "}";
  }
  o << "]}";
  return o.str();
}

} // namespace spike::analysis
