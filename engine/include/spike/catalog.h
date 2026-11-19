// Neuron parts bin (v0.2): the static list of cell models the game offers, as "cards".
//
// Each card carries a plain-language description, biological examples, the key parameters with unit and sensible range, a default
// input weight, E/I suitability and the DC (or Poisson) drive used for its thumbnail. `catalog_json()` returns the whole list as JSON
// for the game. Thumbnails (f-I curve points and a short V_m trace) are measured by the engine itself: run the `spike-catalog`
// tool, which writes catalog/thumbs.json (see catalog_probe_json(): one Engine per process, so each model is probed in its own process).
// No NEST dependency in the static part of this API.
#pragma once
#include <string>
#include <vector>

namespace spike
{

struct CatalogParam
{
  std::string name;   // NEST parameter name
  std::string unit;   // "mV", "ms", "pA", "nS", "pF", "Hz", "" (dimensionless)
  std::string desc;   // one plain-language line
  double def = 0, lo = 0, hi = 0;  // card default and slider range
};

struct CatalogDrive
{
  std::string kind;   // "dc" (pA through a dc_generator), "poisson" (Hz through a poisson_generator, weight 1) or "rate_drive" (0.6: rate models; the card is measured by its rate_value output)
  std::string unit;   // "pA" | "Hz" (Izhikevich cards use the model's own current units, labelled "pA" as NEST does)
  std::vector< double > levels;  // the steps of the f-I thumbnail
};

struct CatalogModel
{
  std::string id;       // card id: the NEST model name, or "izhikevich_RS" for a preset
  std::string model;    // NEST (or NESTML) model name to put in a circuit population
  std::string title, family;
  std::string summary;  // plain language: what it is and how it behaves
  std::vector< std::string > examples;  // biological examples
  std::string source;   // citation of the model / parameters
  std::vector< CatalogParam > params;
  std::string preset_json = "{}";       // parameters to set on top of the model defaults (a JSON object)
  double input_weight = 1.0;            // default weight of a sensor / excitatory synapse onto this cell
  std::string weight_unit;              // "pA" (current-based), "nS" (conductance-based) or "" (unit-free)
  std::string ei;                       // "excitatory" | "inhibitory" | "either" (suitability)
  std::string ei_note;
  CatalogDrive drive;
  bool gap_capable = false;             // can take gap_junction synapses
  bool nestml = false;                  // custom NESTML cell: available only if the module is registered in this build
  bool params_provisional = false;      // parameter list not yet checked against the model (never set once the model exists)
  // How the thumbnail is measured (slow oscillators need longer runs): each drive level runs run_ms and the first skip_ms are
  // discarded for the rate; the V_m trace is trace_ms long sampled every trace_dt_ms (a multiple of the 0.1 ms resolution).
  // Slow cells: the rate is measured over >= min_cycles spikes (one per cycle for pacemakers): a level is extended in run_ms chunks until
  // that many spikes were seen or max_ms is reached (max_ms <= run_ms: fixed window). With >= 2 spikes the rate is (n-1) / (t_last - t_first).
  double run_ms = 1200, skip_ms = 200, trace_ms = 300, trace_dt_ms = 0.5;
  double max_ms = 0;        // 0: same as run_ms (no extension)
  int min_cycles = 10;
  /// Seeded random initial V_m range [lo, hi] mV the engine uses for this card's model (vm_init_range); both 0 = not a membrane-potential cell.
  double vm_lo = 0, vm_hi = 0;
};

/// All cards, in display order.
const std::vector< CatalogModel >& catalog();
const CatalogModel* catalog_find( const std::string& id );
/// {"schema":1,"models":[{id, model, title, family, summary, examples, source, params:[{name,unit,desc,default,min,max}], preset, input_weight,
///   weight_unit, ei, ei_note, drive:{kind,unit,levels}, gap_capable, nestml, probe:{run_ms,skip_ms,max_ms,min_cycles,...}}, ...]}
std::string catalog_json();
/// A one-neuron circuit (JSON, docs/CIRCUIT-FORMAT.md) for the card, driven by input channel "drive" at `level`.
std::string catalog_circuit_json( const CatalogModel& m, double level );
/// Measure one card: runs the card's model under each drive level (run_ms each, the first skip_ms discarded) and records a V_m trace.
/// Returns {"id","available","error"?,"drive","fi":[{"level","rate_hz"}],"trace":{"level","t0_ms","dt_ms","v_mV":[],"spikes_ms":[]}}.
/// Creates an spike::Engine: call at most once per process, and not while another Engine is alive.
std::string catalog_probe_json( const std::string& id );

} // namespace spike
