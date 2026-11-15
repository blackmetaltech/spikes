// NEST glue for the analysis library: reads the connection graph of the network loaded in the kernel.
// All NEST calls of the analysis module live here; analysis.cpp stays NEST-free.
#include <deque>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "dictionary.h"
#include "kernel_manager.h"
#include "nest.h"
#include "spike/analysis.h"

namespace spike::analysis
{

std::vector< Edge > edges_from_engine( const Engine& engine )
{
  // 1. neurons = nodes whose element_type is "neuron", in creation (= circuit file) order.
  std::unordered_map< long, int32_t > index_of;
  const size_t total = nest::kernel().node_manager.size();
  for ( size_t id = 1; id <= total; ++id )
  {
    const Dictionary st = nest::get_node_status( id );
    if ( st.known( "element_type" ) && st.get< std::string >( "element_type" ) == "neuron" )
      index_of.emplace( static_cast< long >( id ), static_cast< int32_t >( index_of.size() ) );
  }
  if ( static_cast< int32_t >( index_of.size() ) != engine.neuron_count() )
    throw std::runtime_error( "edges_from_engine: NEST holds " + std::to_string( index_of.size() ) + " neurons but the engine reports "
      + std::to_string( engine.neuron_count() ) );

  // 2. every connection between two neurons, with its weight.
  std::vector< Edge > edges;
  const auto conns = nest::get_connections( Dictionary() );
  for ( const auto& group : conns )
  {
    if ( group.empty() )
      continue;
    const auto status = nest::get_connection_status( group );
    size_t k = 0;
    for ( const auto& c : group )
    {
      const auto pre = index_of.find( c.get_source_node_id() );
      const auto post = index_of.find( c.get_target_node_id() );
      if ( pre != index_of.end() && post != index_of.end() )
        edges.push_back( Edge{ pre->second, post->second, status[ k ].get< double >( "weight" ) } );
      ++k;
    }
  }
  return edges;
}

} // namespace spike::analysis
