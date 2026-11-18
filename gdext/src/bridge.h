#pragma once
// Plain C++ helpers that may throw: compiled in their own target (godot-cpp builds with -fno-exceptions), so no Godot types here.
#include <string>
#include <vector>
#include "spike/engine.h"

namespace spike_gd
{
/// Motif census JSON (analysis.h motif_result_json) or {"error": "..."} if the graph can't be read.
std::string motif_json( const spike::Engine& engine, int sign, int n_null, unsigned long long seed );
/// Same census on an explicit edge list (flat triples pre, post, weight; node indices 0..n-1), e.g. the population graph.
std::string motif_json_edges( const std::vector< double >& triples, int n, int sign, int n_null, unsigned long long seed );
struct Stats
{
  std::vector< double > rates, cv;
  double chi = 0;
};
Stats spike_stats( const std::vector< spike::Spike >& spikes, int n, double t0_ms, double t1_ms );

// ---- v0.2 analyses (add-only). Every function returns JSON text, or {"error": "..."} on bad input / failure.
/// Burst detection on the spikes of `neurons` (empty = pooled over all) in [t0_ms, t1_ms); analysis.h bursts_json.
std::string burst_json( const std::vector< spike::Spike >& spikes, int n, const std::vector< int >& neurons, double t0_ms, double t1_ms,
  double max_isi_start_ms = 170.0, double max_isi_end_ms = 170.0, double min_ibi_ms = 200.0, double min_duration_ms = 10.0, int min_spikes = 3 );
/// PRC from marker spikes of `neurons` (empty = all) and perturbation times; merge_gap_ms collapses population spikes into one marker.
std::string prc_json( const std::vector< spike::Spike >& spikes, int n, const std::vector< int >& neurons, const std::vector< double >& perturb_ms,
  double period_ms = 0.0, double merge_gap_ms = 0.0, int nbins = 10 );
/// PRC experiment on a loaded engine (pulses through an input channel at n_phases phases); result JSON = prc_json plus "spikes_ms", "pulses_ms".
std::string prc_experiment_json( spike::Engine& engine, const std::string& channel, double baseline, double pulse_value, double pulse_ms, int n_phases,
  const std::vector< int >& neurons, double merge_gap_ms = 0.0, int repeats = 1, int nbins = 10 );
/// Heart-rate variability from beat times in ms (SDNN, RMSSD, pNN50, LF/HF).
std::string hrv_json( const std::vector< double >& beat_times_ms );
/// 4-node motif census JSON (199 classes, only those present are listed). Refuses with an error if the estimated work
/// (motif4_work_estimate * (1 + n_null)) exceeds `max_work` (0 = no limit).
std::string motif4_json( const spike::Engine& engine, int sign, int n_null, unsigned long long seed, double max_work = 5e7 );
std::string motif4_json_edges( const std::vector< double >& triples, int n, int sign, int n_null, unsigned long long seed, double max_work = 5e7 );

// ---- v0.3 instant check (add-only): headless task scoring in a worker thread, one spike-run process per job
// (the engine is one per process, so a job never shares NEST state with the game's own SpikeSim).
/// Path of the spike-run executable. Resolution order when empty: $SPIKE_RUN, next to this library (the GDExtension), next to the executable
/// (and their bin/), directories added with headless_add_search_dir (the project's res://bin), a development tree (./spike-run, build/bin/..., ../build/...), PATH.
void headless_set_runner( const std::string& path );
void headless_add_search_dir( const std::string& dir );
/// The runner that would be used now, or "" if none is found.
std::string headless_runner_path();
/// Start a job; returns its id (> 0), or 0 with `err` set (also when no spike-run is found). mode/variant may be empty; seeds empty = {1}.
/// circuit_seeds: empty = the task seed reseeds the circuit (1000 * seed + circuit seed); one value = pin the circuit seed for every task seed;
/// one per seed = pin each (pass the live run's circuit seed so CHECK scores the same circuit). The circuit goes through a private mkstemp file (0600), deleted when the job ends.
long headless_start( const std::string& task_id, const std::string& circuit_json, const std::vector< long long >& seeds, const std::string& mode,
  const std::string& variant, std::string& err, const std::vector< long long >& circuit_seeds = {}, bool quick = false );
/// quick = true passes --quick (Task::quick_duration_ms() instead of the full episode). Seeds always run in parallel child processes inside spike-run (cap = cores).
/// Kill the job's spike-run process group, delete its temp files and forget the job. True if it was still running.
bool headless_cancel( long job );
/// JSON: {"done":bool,"ok":bool,"error":"","ms":n,"result":{spike-run --task output}} (result only when ok). Unknown id -> done, not ok.
/// A finished job is forgotten after its result has been polled once.
std::string headless_poll( long job );
} // namespace spike_gd
