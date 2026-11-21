// "Theory" wing (0.6): helpers for the phase-plane, mean-field and reverse-correlation levels. Plain data in, plain data out: testable without NEST.
//   phase_plane()      nullclines, fixed points (with their type) and the reset of a two-variable cell (izhikevich, aeif_cond_exp, aeif_cond_alpha), from the
//                      model's own parameters and a constant input current. The equations are the models' own (NEST izhikevich.h, aeif_cond_exp.h): the
//                      synaptic conductances of the aeif models are not part of the picture (a constant current only).
//   onset()            how the resting state is lost as the current grows: a saddle-node (the fixed point vanishes through a node: class I, f-I continuous
//                      from zero) or a Hopf bifurcation (the fixed point is a focus that loses stability: class II, f-I starts abruptly).
//   spike_triggered_average()  reverse correlation of a stimulus time series with a spike train.
#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>
#include "spike/engine.h"

namespace spike::theory
{

struct FixedPoint
{
  double v = 0, w = 0;       // mV and the recovery variable (pA for aeif's w, the model's own units for izhikevich's U)
  double trace = 0, det = 0; // of the Jacobian
  std::string kind;          // "stable node" | "stable focus" | "saddle" | "unstable node" | "unstable focus"
  bool stable = false;
};

struct PhasePlane
{
  bool ok = false;           // false: the model has no phase plane here (see `note`)
  std::string model, w_name, note;
  double v_lo = 0, v_hi = 0, w_lo = 0, w_hi = 0;      // a window that shows the nullclines and the fixed points
  std::vector< std::array< double, 2 > > v_null, w_null;  // (V, w) samples of the V-nullcline and the w-nullcline
  std::vector< FixedPoint > fixed;
  double v_reset = 0, v_peak = 0, w_jump = 0;        // the spike resets V to v_reset and adds w_jump to w (aeif: b; izhikevich: d)
  double i_total = 0;                                // the constant current the picture is drawn for (pA; the model's I_e plus the external current)
};

/// True for the models phase_plane() knows.
bool phase_plane_supported( const std::string& model );
/// The NEST parameter names phase_plane() reads for `model` (so a caller can fetch them from a running neuron).
std::vector< std::string > phase_plane_params( const std::string& model );
/// `params`: values of phase_plane_params(model) (missing entries take NEST's defaults). `i_ext`: extra constant current in pA (izhikevich: in the model's units).
PhasePlane phase_plane( const std::string& model, const std::map< std::string, double >& params, double i_ext );
/// The phase plane of a neuron of a loaded engine, with the parameters read from the live NEST node (NaN-free fallbacks to NEST's defaults).
PhasePlane phase_plane_of( const Engine& engine, int32_t neuron, double i_ext );

struct Onset
{
  bool found = false;        // the resting state is lost somewhere in [0, i_max]
  double current = 0;        // the first current (a grid point) with no stable fixed point
  std::string kind;          // "saddle-node" | "hopf" | "" (no stable fixed point at current 0 / not found)
  double freq_hz = 0;        // the damped oscillation frequency of the last stable fixed point before the onset (0 for a node): how resonant the resting state is
};
/// Scan the current from `i_lo` to `i_hi` in `n` steps for the first current with no stable fixed point; the type of the last stable fixed point before it says how rest is lost.
Onset onset( const std::string& model, const std::map< std::string, double >& params, double i_lo, double i_hi, int n = 400 );

/// Spike-triggered average: for each spike time (ms) the mean of stimulus[t - window_before .. t + window_after] (stimulus sampled every dt_ms, spike times in the same clock),
/// minus the mean of the whole stimulus (so a stimulus with a DC level reads as the deviation from it: 0 means "no different from the average input", a positive lobe before the spike means
/// the cell fires after an above-average input); returns window = (before + after) / dt + 1 values, index 0 = `before_ms` before the spike. Spikes too close to the ends are skipped;
/// `n_used` is how many entered the average. `stim_mean` / `stim_sd` are the mean and standard deviation of the stimulus (the average is in the stimulus' own unit; sd is the scale for it).
struct Sta
{
  std::vector< double > mean;
  int n_used = 0;
  double dt_ms = 1.0, before_ms = 0;
  double stim_mean = 0, stim_sd = 0;
};
Sta spike_triggered_average( const std::vector< double >& stimulus, double dt_ms, const std::vector< double >& spike_times_ms, double before_ms, double after_ms );

} // namespace spike::theory
