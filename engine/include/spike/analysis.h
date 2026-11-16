// spike analysis: 3-node motif census with degree-preserving nulls, and spike-train statistics.
// Everything here works on plain data (an edge list and a spike list), so it is testable without NEST.
// The one NEST-touching helper is edges_from_engine(), implemented in src/analysis/analysis_nest.cpp.
// The Python package (python/spike/motifs.py, stats.py) mirrors this API and gives identical results.
#pragma once
#include <cmath>
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

/// What rewire() did: swap attempts vs accepted swaps. Small dense graphs reject most attempts, so the null barely differs from
/// the real graph; acceptance = accepted / attempts says how much randomisation actually happened.
struct RewireStats
{
  uint64_t attempts = 0;
  uint64_t accepted = 0;
};
Graph rewire( const Graph& g, uint64_t seed, int swaps_per_edge, RewireStats* stats );  // same result as the 3-argument form

/// Below this swap acceptance rate the null ensemble is called degenerate (too little randomisation to trust a z-score).
constexpr double kMinSwapAcceptance = 0.2;
/// A z-score counts as significant only if |z| > kSignificantZ and the null sd is at least kMinNullSd counts (counts are
/// integers: an sd below ~0.5 means the null is essentially a constant, and z explodes on a difference of one).
constexpr double kSignificantZ = 2.0;
constexpr double kMinNullSd = 0.5;

struct MotifResult
{
  Census counts;                  // real graph
  std::vector< double > null_mean;
  std::vector< double > null_sd;  // sample sd (n-1) across the null ensemble
  std::vector< double > z;        // (count - mean) / sd; 0 if count == mean, +-inf if sd == 0 and count != mean
  int null_samples = 0;
  // --- add-only diagnostics of the null model
  int32_t node_count = 0;
  int64_t edge_count = 0;
  uint64_t swap_attempts = 0;       // summed over the null ensemble
  uint64_t swap_accepted = 0;
  double swap_acceptance = 0.0;     // accepted / attempts (0 if no swap was possible)
  double effective_swaps_per_edge = 0.0;  // accepted swaps per edge per null graph
  bool degenerate = false;          // swap_acceptance < kMinSwapAcceptance (or fewer than 2 edges): nulls hardly differ from the data
  std::vector< int > significant;   // 1 if finite z, |z| > kSignificantZ and null_sd >= kMinNullSd; infinite/undefined z is never significant
};

/// Census plus z-scores against `n_null` rewired graphs (null k uses seed + k).
MotifResult motif_zscores( const Graph& g, int n_null, uint64_t seed, int swaps_per_edge = 10 );
MotifResult motif_zscores( const std::vector< Edge >& edges, int32_t n, Sign sign, int n_null, uint64_t seed,
  int swaps_per_edge = 10 );

/// True if the z-score of class c is finite and trustworthy enough to mark (see kSignificantZ / kMinNullSd) and the null is not degenerate.
bool motif_is_significant( const MotifResult& r, int c );

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


// ================================================================ v0.2 additions (REQ-0.2 section E), add-only
// Bursts, phase-response curves, heart-rate variability and the 4-node motif census. Plain data in, plain data out.
// Python mirrors: python/spike/{bursts,prc,hrv}.py and motifs.py (census4 / zscores4). C++ and Python agree (cross tests).

// ---------------------------------------------------------------- burst detection

/// Max-interval burst detection (Cocatre-Zilgien & Delcomyn 1992, J Neurosci Methods 41:19-30; the NeuroExplorer / Nex variant):
/// 1. a burst starts at two spikes closer than max_isi_start_ms and extends while successive ISIs are <= max_isi_end_ms;
/// 2. bursts whose gap (last spike -> first spike of the next) is < min_ibi_ms are merged (spikes in between included);
/// 3. bursts shorter than min_duration_ms or with fewer than min_spikes spikes are dropped.
/// A spike belongs to at most one burst. All comparisons on ISIs are "<=", the merge gap is "<".
struct BurstParams
{
  double max_isi_start_ms = 170.0;
  double max_isi_end_ms = 170.0;
  double min_ibi_ms = 200.0;
  double min_duration_ms = 10.0;
  int min_spikes = 3;
};

struct Burst
{
  double onset_ms = 0;      // time of the first spike
  double offset_ms = 0;     // time of the last spike
  double duration_ms = 0;   // offset - onset
  int32_t n_spikes = 0;
  int64_t first_index = 0;  // index of the first spike in the sorted spike-time vector
};

/// Bursts of one spike train (times need not be sorted; a sorted copy is used; first_index refers to the sorted copy).
std::vector< Burst > detect_bursts( const std::vector< double >& spike_times_ms, const BurstParams& p = BurstParams() );
/// Bursts of every neuron separately over [t0_ms, t1_ms): result has n entries.
std::vector< std::vector< Burst > > detect_bursts( const std::vector< Spike >& spikes, int32_t n, double t0_ms, double t1_ms,
  const BurstParams& p = BurstParams() );
/// Spike times (sorted) of `neurons` (empty = all) in [t0_ms, t1_ms): feed it to detect_bursts() for population bursts.
std::vector< double > pooled_spike_times( const std::vector< Spike >& spikes, int32_t n, const std::vector< int32_t >& neurons, double t0_ms,
  double t1_ms );

struct BurstSummary
{
  int32_t n_bursts = 0;
  int64_t n_spikes = 0;               // spikes in the train
  int64_t spikes_in_bursts = 0;
  double fraction_in_bursts = 0;      // spikes_in_bursts / n_spikes (0 if no spikes)
  double mean_duration_ms = NAN;      // NaN if no bursts
  double mean_spikes_per_burst = NAN;
  double mean_period_ms = NAN;        // mean onset-to-onset interval, NaN if < 2 bursts
  double mean_intra_burst_isi_ms = NAN;  // (duration / (n_spikes-1)) averaged over bursts
};
BurstSummary summarize_bursts( const std::vector< Burst >& b, int64_t n_spikes );
/// {"params":{...},"n_bursts":..,"summary":{...},"bursts":[{"onset_ms":..,"offset_ms":..,"duration_ms":..,"n_spikes":..},...]}
std::string bursts_json( const std::vector< Burst >& b, int64_t n_spikes, const BurstParams& p = BurstParams() );

// ---------------------------------------------------------------- phase-response curve

/// One perturbation that fell into a clean cycle. phase in [0,1) = (t_perturb - t_prev_spike) / period;
/// shift = (period - perturbed_cycle) / period, in cycles: positive = phase advance (the next spike came early).
struct PrcPoint
{
  double perturb_ms = 0;
  double phase = 0;
  double shift = 0;
  double cycle_ms = 0;       // length of the perturbed cycle (prev spike -> next spike)
  double prev_spike_ms = 0;
};

struct PrcResult
{
  double period_ms = NAN;     // unperturbed period used (median of clean cycles unless given)
  int32_t n_clean_cycles = 0; // cycles used for the period
  int32_t n_perturbations = 0;
  int32_t n_rejected = 0;     // perturbations outside the spike train, or not isolated (another one within 2 cycles before / in the cycle)
  std::vector< PrcPoint > points;  // sorted by phase
};

/// Phase-response curve from the spike (cycle-marker) times of an oscillator and the times of brief perturbations.
/// A cycle k runs between marker k and k+1. A cycle is "clean" if neither it nor the two cycles before it contain a perturbation;
/// the period is the median clean cycle (or `period_ms` if > 0). A perturbation is used if it is alone in its cycle and the
/// two cycles before are clean (so the oscillator has recovered), first-order PRC.
/// For a population, pass cycle markers such as the burst onsets of the pooled train (detect_bursts).
PrcResult prc_estimate( const std::vector< double >& spike_times_ms, const std::vector< double >& perturb_times_ms, double period_ms = 0.0 );
/// Same, taking the markers from the spikes of `neurons` (empty = all); spikes closer than merge_gap_ms to the previous
/// pooled spike count as the same cycle marker (0 = every spike is a marker).
PrcResult prc_estimate( const std::vector< Spike >& spikes, int32_t n, const std::vector< int32_t >& neurons,
  const std::vector< double >& perturb_times_ms, double period_ms = 0.0, double merge_gap_ms = 0.0 );

struct PrcBin
{
  double phase_center = 0;
  double mean_shift = NAN;   // NaN if the bin is empty
  double sd_shift = NAN;     // sample sd, NaN if < 2 points
  int32_t count = 0;
};
/// Mean shift per phase bin (nbins equal bins over [0,1)).
std::vector< PrcBin > prc_bins( const PrcResult& r, int nbins );
/// {"period_ms":..,"n_perturbations":..,"n_rejected":..,"points":[{"phase":..,"shift":..},...],"bins":[...]}
std::string prc_json( const PrcResult& r, int nbins = 10 );

/// Configuration of a PRC experiment driven through the Engine (prc_experiment.cpp): a periodic oscillator (a circuit that spikes
/// regularly under its baseline drive) receives brief pulses at evenly spaced phases through one input channel.
/// Pulses are applied by setting `channel` to pulse_value for pulse_ms, then back to `baseline`.
struct PrcExperiment
{
  std::string channel;                // input channel used for the pulses ("dc" or "poisson" kind)
  double baseline = 0;                // channel value between pulses (the drive that makes the oscillator spike)
  double pulse_value = 0;             // channel value during a pulse
  double pulse_ms = 1.0;              // pulse width (multiple of the resolution)
  std::vector< int32_t > neurons;     // observed neurons (empty = all)
  double merge_gap_ms = 0.0;          // population: spikes closer than this to the previous one are the same cycle marker
  int n_phases = 10;                  // phases (k + 0.5) / n_phases
  int repeats = 1;                    // trials per phase
  double settle_ms = 300.0;           // run before measuring
  int baseline_cycles = 6;            // unperturbed cycles used for the first period estimate
  int recovery_cycles = 3;            // quiet cycles after each pulse
  double timeout_ms = 60000.0;        // give up if the oscillator does not spike for this long
};
struct PrcExperimentResult
{
  bool ok = false;
  std::string error;
  PrcResult prc;
  std::vector< double > spike_ms, pulse_ms;  // markers and pulse onsets, for plotting
};
/// Drive the experiment on an already loaded Engine (call Engine::load first). Uses step() at the engine resolution.
PrcExperimentResult run_prc_experiment( Engine& engine, const PrcExperiment& cfg );

// ---------------------------------------------------------------- heart-rate variability

/// Heart-rate variability from beat times (ms). RR intervals = successive differences ("NN" = all beats are taken as normal).
/// Time domain (Task Force 1996, Circulation 93:1043): SDNN (sample sd of NN), RMSSD, pNN50 (% of |successive differences| > 50 ms).
/// Frequency domain: Lomb-Scargle periodogram (Lomb 1976; Scargle 1982) of the mean-removed RR tachogram sampled at the beat
/// times, no resampling needed for the uneven sampling. Density S(f) = P(f) * 2T/N (ms^2/Hz) with P the unnormalised LS power
/// (scipy.signal.lombscargle), T the span of the beats, N the number of RR values: white noise then integrates to its variance.
/// Band power = sum of S over band midpoints on a df grid, LF 0.04-0.15 Hz, HF 0.15-0.4 Hz.
struct HrvParams
{
  double lf_lo_hz = 0.04, lf_hi_hz = 0.15;
  double hf_lo_hz = 0.15, hf_hi_hz = 0.40;
  double df_hz = 0.001;        // frequency grid step
  int min_beats_spectrum = 9;  // fewer beats (8 RR values) -> spectral outputs NaN
};
struct HrvResult
{
  int32_t n_beats = 0;
  int32_t n_rr = 0;
  double mean_rr_ms = NAN;
  double mean_hr_bpm = NAN;
  double sdnn_ms = NAN;        // needs n_rr >= 2
  double rmssd_ms = NAN;       // needs n_rr >= 2
  double pnn50_pct = NAN;      // needs n_rr >= 2
  double lf_ms2 = NAN, hf_ms2 = NAN;
  double lf_hf = NAN;          // NaN if hf == 0
  double lf_nu = NAN, hf_nu = NAN;  // normalised units: LF/(LF+HF)*100 and HF/(LF+HF)*100
  bool spectrum_ok = false;
};
HrvResult hrv_analyze( const std::vector< double >& beat_times_ms, const HrvParams& p = HrvParams() );
/// Lomb-Scargle density S(f) in ms^2/Hz of the RR tachogram at the given frequencies (see above); empty if n_rr < 3.
std::vector< double > hrv_lomb_scargle( const std::vector< double >& beat_times_ms, const std::vector< double >& freqs_hz );
std::string hrv_json( const HrvResult& r );

// ---------------------------------------------------------------- 4-node motif census

constexpr int kMotifClasses4 = 199;

/// One connected 4-node class. `id` = minimum over node orderings of the 16-bit row-major adjacency matrix (bit 15 - (4i+j) for
/// edge i->j), the 4-node analogue of MotifInfo::milo_id. `index` runs 0..198 in increasing id and is the position in census vectors.
struct MotifInfo4
{
  int index;
  int id;
  int edges;
  int mutual_pairs;
};
const MotifInfo4& motif4_info( int index );
int motif4_index_from_id( int id );  // -1 if not one of the 199

/// Counts of induced weakly connected 4-node subgraphs per class (ESU enumeration, Wernicke 2006, table lookup per subgraph).
Census motif4_census( const Graph& g );
Census motif4_census( const std::vector< Edge >& edges, int32_t n = -1, Sign sign = Sign::Any );
/// Rough work estimate (number of 3-leaf stars + paths through the undirected skeleton): the census costs about this many table
/// lookups, times (1 + null samples) for z-scores. Use it to decline the census on big dense graphs.
double motif4_work_estimate( const Graph& g );

struct MotifResult4
{
  Census counts;
  std::vector< double > null_mean, null_sd, z;
  int null_samples = 0;
  int32_t node_count = 0;
  int64_t edge_count = 0;
  uint64_t swap_attempts = 0, swap_accepted = 0;
  double swap_acceptance = 0.0;
  double effective_swaps_per_edge = 0.0;
  bool degenerate = false;
  std::vector< int > significant;  // same rule as MotifResult (kSignificantZ, kMinNullSd, not degenerate)
};
MotifResult4 motif4_zscores( const Graph& g, int n_null, uint64_t seed, int swaps_per_edge = 10 );
MotifResult4 motif4_zscores( const std::vector< Edge >& edges, int32_t n, Sign sign, int n_null, uint64_t seed, int swaps_per_edge = 10 );
/// Only classes that occur in the real graph or in a null are listed (199 rows would swamp the game UI): {"classes":[{"index","id","edges","mutual_pairs","count","null_mean","null_sd","z","z_defined","significant"}...], ...}
std::string motif4_result_json( const MotifResult4& r, bool only_present = true );

} // namespace spike::analysis
