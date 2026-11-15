#pragma once
// Plain C++ helpers that may throw: compiled in their own target (godot-cpp builds with -fno-exceptions), so no Godot types here.
#include <string>
#include <vector>
#include "spike/engine.h"

namespace spike_gd
{
/// Motif census JSON (analysis.h motif_result_json) or {"error": "..."} if the graph can't be read.
std::string motif_json( const spike::Engine& engine, int sign, int n_null, unsigned long long seed );
struct Stats
{
  std::vector< double > rates, cv;
  double chi = 0;
};
Stats spike_stats( const std::vector< spike::Spike >& spikes, int n, double t0_ms, double t1_ms );
} // namespace spike_gd
