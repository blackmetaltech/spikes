// edges_from_engine(): the connection graph read back from NEST matches the circuit, and feeds the census.
#include <cstdio>
#include <set>
#include <tuple>

#include "spike/analysis.h"
#include "spike/engine.h"

using namespace spike;
using namespace spike::analysis;

static int g_fail = 0;
#define CHECK( cond )                                                                                                  \
  do                                                                                                                   \
  {                                                                                                                    \
    if ( !( cond ) )                                                                                                   \
    {                                                                                                                  \
      std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond );                                                    \
      ++g_fail;                                                                                                        \
    }                                                                                                                  \
  } while ( 0 )

// 3 neurons A(0) B(1) C(2): feed-forward loop A->B, B->C, A->C; B->C inhibitory. A driven by DC; devices must not appear.
static const char* kCircuit = R"({
  "schema_version": 1, "resolution_ms": 0.1, "seed": 1,
  "populations": [
    { "id": "A", "model": "iaf_psc_alpha", "n": 1 },
    { "id": "B", "model": "iaf_psc_alpha", "n": 1 },
    { "id": "C", "model": "iaf_psc_alpha", "n": 1 }
  ],
  "projections": [
    { "pre": "A", "post": "B", "edges": [[0, 0, 5.0, 1.0]] },
    { "pre": "B", "post": "C", "edges": [[0, 0, -7.0, 1.0]] },
    { "pre": "A", "post": "C", "rule": "all_to_all", "synapse": { "weight": 3.0, "delay": 1.0 } }
  ],
  "inputs": [ { "name": "dc", "kind": "dc", "targets": ["A"], "weight": 1.0, "value": 400.0 } ],
  "record": { "spikes": "all" }
})";

int main()
{
  Engine e;
  if ( !e.load( kCircuit ) )
  {
    std::printf( "load failed: %s\n", e.error().c_str() );
    return 1;
  }
  auto edges = edges_from_engine( e );
  std::set< std::tuple< int, int, double > > got;
  for ( const auto& x : edges )
    got.insert( { x.pre, x.post, x.weight } );
  CHECK( edges.size() == 3 );
  CHECK( got == ( std::set< std::tuple< int, int, double > >{ { 0, 1, 5.0 }, { 1, 2, -7.0 }, { 0, 2, 3.0 } } ) );
  const auto c = motif_census( edges, e.neuron_count() );
  CHECK( c[ 4 ] == 1 );  // feed-forward loop, Milo id 38
  CHECK( motif_info( 4 ).milo_id == 38 );
  // still correct after the network has run (devices and recorder keep existing)
  e.step( 20.0 );
  CHECK( edges_from_engine( e ).size() == 3 );
  std::printf( g_fail ? "analysis_engine_test: %d FAILED\n" : "analysis_engine_test: all passed\n", g_fail );
  return g_fail ? 1 : 0;
}
