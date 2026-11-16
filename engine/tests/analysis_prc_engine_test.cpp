// PRC experiment through the real Engine: an integrate-and-fire oscillator under DC drive, brief current pulses at several phases.
// A leaky integrate-and-fire neuron has a type-I PRC: no shift right after the spike (refractory) and an advance that grows with phase.
#include <cmath>
#include <cstdio>

#include "check.h"
#include "spike/analysis.h"
#include "spike/engine.h"

using namespace spike;
using namespace spike::analysis;

static const char* kCircuit = R"({
  "schema_version": 1, "resolution_ms": 0.1, "seed": 1,
  "populations": [ { "id": "A", "model": "iaf_psc_alpha", "n": 1, "params": { "V_m": -70.0 } } ],
  "projections": [],
  "inputs": [ { "name": "drive", "kind": "dc", "targets": ["A"], "weight": 1.0, "value": 500.0 } ],
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
  PrcExperiment cfg;
  cfg.channel = "drive";
  cfg.baseline = 500.0;
  cfg.pulse_value = 1300.0;
  cfg.pulse_ms = 1.0;
  cfg.n_phases = 8;
  cfg.repeats = 1;
  const auto r = run_prc_experiment( e, cfg );
  CHECK_MSG( r.ok, r.error );
  if ( r.ok )
  {
    std::printf( "%s\n", prc_json( r.prc, 8 ).c_str() );
    // analytic period: refractory 2 ms + tau ln(RI / (RI - 15)) with R = 40 MOhm, tau = 10 ms
    const double analytic = 2.0 + 10.0 * std::log( 20.0 / 5.0 );
    CHECK( std::fabs( r.prc.period_ms - analytic ) < 0.5 );
    CHECK( r.prc.points.size() == 8 );
    CHECK( r.pulse_ms.size() == 8 );
    if ( r.prc.points.size() == 8 )
    {
      for ( const auto& p : r.prc.points )
        CHECK( p.shift > -0.03 );
      CHECK( r.prc.points.front().shift < 0.05 );  // early (refractory) phase: almost no effect
      CHECK( r.prc.points[ 4 ].shift > 0.2 );      // mid phase: clear advance
      CHECK( r.prc.points[ 4 ].shift > r.prc.points.front().shift + 0.15 );
      CHECK( r.prc.points.back().shift < r.prc.points[ 4 ].shift );  // a pulse just before the spike has little left to advance
    }
  }
  // errors are reported, not thrown
  PrcExperiment bad = cfg;
  bad.channel = "nope";
  CHECK( !run_prc_experiment( e, bad ).ok );
  return chk::finish( "analysis_prc_engine_test" );
}
