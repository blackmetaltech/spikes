// spike analysis: 3-node motif census with degree-preserving nulls, and spike-train statistics.
// Everything here works on plain data (an edge list and a spike list), so it is testable without NEST.
// The one NEST-touching helper is edges_from_engine(), implemented in src/analysis/analysis_nest.cpp.
// The Python package (python/spike/motifs.py, stats.py) mirrors this API and gives identical results.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "spike/engine.h"

namespace spike::analysis
{

// ---------------------------------------------------------------- graphs

/// One directed connection between circuit-level neuron indices, with its synaptic weight (sign = E/I).
struct Edge
{
  int32_t pre;
  int32_t post;
  double weight = 1.0;
};

/// Which edges enter the census: all, only excitatory (weight > 0), only inhibitory (weight < 0).
enum class Sign { Any, Excitatory, Inhibitory };

/// Simple directed graph on n nodes: self-loops dropped, multi-edges merged. Out-adjacency lists are sorted.
struct Graph
{
  int32_t n = 0;
  std::vector< std::vector< int32_t > > out;
  int64_t edge_count() const;
  bool has_edge( int32_t a, int32_t b ) const;
};

/// Build a Graph from an edge list. `n` < 0 means "1 + the largest index". Edges failing the sign filter are skipped.
Graph make_graph( const std::vector< Edge >& edges, int32_t n = -1, Sign sign = Sign::Any );

// ---------------------------------------------------------------- motifs

constexpr int kMotifClasses = 13;

/// Description of one connected 3-node class. `milo_id` is the id used by Milo et al. 2002 / mfinder: the minimum over
/// node orderings of the row-major 9-bit adjacency matrix (6, 12, 14, 36, 38, 46, 74, 78, 98, 102, 108, 110, 238).
/// `index` runs 0..12 in increasing milo_id and is the position in every census vector.
struct MotifInfo
{
  int index;
  int milo_id;
  const char* man;   // Holland-Leinhardt (MAN) triad code, as in networkx: 021D, 021U, 021C, 111D, 111U, 030T, 030C, 201, 120D, 120U, 120C, 210, 300
  const char* name;  // fan_out, fan_in, chain, feed_forward_loop, feedback_loop, ... (see analysis.cpp)
  int edges;
  bool has_mutual;   // contains a reciprocal pair
};

const MotifInfo& motif_info( int index );
int motif_index_from_milo_id( int milo_id );  // -1 if not one of the 13

/// Counts of induced connected 3-node subgraphs per class, indexed by MotifInfo::index.
using Census = std::vector< int64_t >;

Census motif_census( const Graph& g );
Census motif_census( const std::vector< Edge >& edges, int32_t n = -1, Sign sign = Sign::Any );

/// Degree-preserving randomisation (Maslov-Sneppen edge swaps keeping every in/out degree, no self-loops or multi-edges).
/// `swaps_per_edge` attempted swaps per edge. Deterministic for a given seed (SplitMix64, identical in Python).
Graph rewire( const Graph& g, uint64_t seed, int swaps_per_edge = 10 );

struct MotifResult
{
  Census counts;                  // real graph
  std::vector< double > null_mean;
  std::vector< double > null_sd;  // sample sd (n-1) across the null ensemble
  std::vector< double > z;        // (count - mean) / sd; 0 if count == mean, +-inf if sd == 0 and count != mean
  int null_samples = 0;
};

/// Census plus z-scores against `n_null` rewired graphs (null k uses seed + k).
MotifResult motif_zscores( const Graph& g, int n_null, uint64_t seed, int swaps_per_edge = 10 );
MotifResult motif_zscores( const std::vector< Edge >& edges, int32_t n, Sign sign, int n_null, uint64_t seed,
  int swaps_per_edge = 10 );

/// Results as one JSON object (class table + counts + z), for the Godot analysis screen and the CLI tool.
std::string motif_result_json( const MotifResult& r );

// ---------------------------------------------------------------- spike statistics

/// Firing rate in Hz of every neuron over [t0_ms, t1_ms). Result has n entries.
std::vector< double > firing_rates( const std::vector< Spike >& spikes, int32_t n, double t0_ms, double t1_ms );

/// ISI coefficient of variation per neuron (sd/mean of inter-spike intervals, population sd). NaN if < 3 spikes in the window.
std::vector< double > isi_cv( const std::vector< Spike >& spikes, int32_t n, double t0_ms, double t1_ms );

/// Golomb synchrony chi (Golomb 2007, Scholarpedia) computed on spike counts instead of voltages:
/// chi = sqrt( var_t( mean_i s_i(t) ) / mean_i var_t( s_i(t) ) ), s_i(t) = spike count of neuron i in bin t.
/// 0 = asynchronous, 1 = fully synchronous. Silent neurons count in the mean. Returns 0 if nobody spikes.
double golomb_chi( const std::vector< Spike >& spikes, int32_t n, double t0_ms, double t1_ms, double bin_ms = 1.0 );

/// Population PSTH: mean rate in Hz per neuron in each bin over [t0_ms, t1_ms), over `neurons` (empty = all n).
std::vector< double > psth( const std::vector< Spike >& spikes, int32_t n, const std::vector< int32_t >& neurons, double t0_ms,
  double t1_ms, double bin_ms );
/// Event-aligned PSTH over [-pre_ms, +post_ms), averaged over events, Hz per neuron.
std::vector< double > psth_aligned( const std::vector< Spike >& spikes, int32_t n, const std::vector< int32_t >& neurons,
  const std::vector< double >& events_ms, double pre_ms, double post_ms, double bin_ms );

// ---------------------------------------------------------------- NEST glue (analysis_nest.cpp)

/// Connection graph of the network currently loaded in the NEST kernel by `engine` (call after Engine::load()).
/// Uses NEST get_connections + get_connection_status; devices (generators, recorders, multimeters) are ignored and
/// node ids are mapped back to circuit-level indices (neurons are the kernel's nodes of element_type "neuron", in
/// creation order, which is the circuit's file order). Throws std::runtime_error if the counts disagree.
/// The accessor relies on no private Engine state; the engine only needs to expose neuron_count().
std::vector< Edge > edges_from_engine( const Engine& engine );

} // namespace spike::analysis
