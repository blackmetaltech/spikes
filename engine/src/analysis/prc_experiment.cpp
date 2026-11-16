// PRC experiment driven through the Engine: a periodic oscillator gets brief pulses at evenly spaced phases.
// This is the only part of the PRC code that touches the Engine (and so NEST); the estimator lives in prc.cpp.
#include <algorithm>
#include <cmath>
#include <limits>

#include "spike/analysis.h"
#include "spike/engine.h"

namespace spike::analysis
{
namespace
{
struct Runner
{
  Engine& e;
  const PrcExperiment& cfg;
  double res;
  double t = 0;  // model time reached
  std::vector< char > mask;
  double last_pooled = -std::numeric_limits< double >::infinity();
  std::vector< double > marks, pulses;
  bool failed = false;

  Runner( Engine& eng, const PrcExperiment& c ) : e( eng ), cfg( c ), res( eng.resolution_ms() )
  {
    mask.assign( e.neuron_count(), c.neurons.empty() ? 1 : 0 );
    for ( int32_t i : c.neurons )
      if ( i >= 0 && i < e.neuron_count() )
        mask[ i ] = 1;
    t = e.stats().t_ms;
  }
  // one resolution step; returns true if a new cycle marker appeared in it
  bool tick()
  {
    e.step( res );
    t = e.stats().t_ms;
    if ( !e.last_step_ok() )
      failed = true;
    bool fresh = false;
    auto sp = e.drain_spikes();
    std::sort( sp.begin(), sp.end(), []( const Spike& a, const Spike& b ) { return a.t_ms < b.t_ms; } );
    for ( const auto& s : sp )
    {
      if ( s.neuron < 0 || s.neuron >= static_cast< int32_t >( mask.size() ) || !mask[ s.neuron ] )
        continue;
      if ( marks.empty() || cfg.merge_gap_ms <= 0 || s.t_ms - last_pooled > cfg.merge_gap_ms )
      {
        marks.push_back( s.t_ms );
        fresh = true;
      }
      last_pooled = s.t_ms;
    }
    return fresh;
  }
  void run_for( double ms, double timeout_t )
  {
    const double end = t + ms;
    while ( t < end - 0.5 * res && !failed && t < timeout_t )
      tick();
  }
  // run until the next marker; false on timeout / failure
  bool wait_marker()
  {
    const double limit = t + cfg.timeout_ms;
    while ( !failed && t < limit )
      if ( tick() )
        return true;
    return false;
  }
};
} // namespace

PrcExperimentResult run_prc_experiment( Engine& engine, const PrcExperiment& cfg )
{
  PrcExperimentResult out;
  if ( engine.resolution_ms() <= 0 )
  {
    out.error = "run_prc_experiment: no circuit loaded";
    return out;
  }
  bool has_channel = false;
  for ( const auto& c : engine.inputs() )
    has_channel = has_channel || c.name == cfg.channel;
  if ( !has_channel )
  {
    out.error = "run_prc_experiment: unknown input channel '" + cfg.channel + "'";
    return out;
  }
  if ( cfg.n_phases < 1 || cfg.repeats < 1 || cfg.pulse_ms <= 0 )
  {
    out.error = "run_prc_experiment: n_phases, repeats and pulse_ms must be positive";
    return out;
  }
  Runner r( engine, cfg );
  const double inf = std::numeric_limits< double >::infinity();
  engine.set_input( cfg.channel, cfg.baseline );
  r.run_for( cfg.settle_ms, inf );
  // period from unperturbed cycles
  const size_t first = r.marks.size();
  for ( int i = 0; i <= cfg.baseline_cycles; ++i )
    if ( !r.wait_marker() )
    {
      out.error = r.failed ? "run_prc_experiment: engine step failed: " + engine.error() : "run_prc_experiment: the oscillator is silent (no spikes)";
      return out;
    }
  std::vector< double > isi;
  for ( size_t i = first + 1; i < r.marks.size(); ++i )
    isi.push_back( r.marks[ i ] - r.marks[ i - 1 ] );
  std::sort( isi.begin(), isi.end() );
  const double period = isi[ isi.size() / 2 ];
  for ( int rep = 0; rep < cfg.repeats; ++rep )
    for ( int k = 0; k < cfg.n_phases; ++k )
    {
      if ( !r.wait_marker() )
      {
        out.error = r.failed ? "run_prc_experiment: engine step failed: " + engine.error() : "run_prc_experiment: the oscillator stopped spiking";
        return out;
      }
      const double ts = r.marks.back();
      const double phi = ( k + 0.5 ) / cfg.n_phases;
      const double target = ts + phi * period;
      while ( r.t < target - 0.5 * r.res && !r.failed )
        r.tick();
      r.pulses.push_back( r.t );
      engine.set_input( cfg.channel, cfg.pulse_value );
      r.run_for( cfg.pulse_ms, inf );
      engine.set_input( cfg.channel, cfg.baseline );
      r.run_for( ( cfg.recovery_cycles + 1 ) * period, inf );
      if ( r.failed )
      {
        out.error = "run_prc_experiment: engine step failed: " + engine.error();
        return out;
      }
    }
  r.wait_marker();  // close the last perturbed cycle
  out.spike_ms = r.marks;
  out.pulse_ms = r.pulses;
  out.prc = prc_estimate( r.marks, r.pulses );
  out.ok = true;
  return out;
}

} // namespace spike::analysis
