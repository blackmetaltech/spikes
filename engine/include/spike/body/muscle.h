// Hill-type muscle driven by a pool of motor units (see docs/body/muscle.md for equations and citations).
#pragma once
#include <vector>
#include "spike/body/common.h"

namespace spike::body
{

struct MuscleParams
{
  int n_units = 20;                // motor units in the pool, index 0 = smallest (S), n-1 = largest (FF)
  double fmax_N = 100;             // fused-tetanus maximum isometric force of the whole pool at optimal length
  double l_opt_m = 0.10;           // optimal fibre length (only used to convert to SI)
  // Size principle (Henneman 1965; Fuglevand, Winters & Enoka 1993): unit i has relative size P_i = recruit_force_range^(i/(n-1)),
  // twitch force and recruitment threshold increase with i, contraction time decreases with i.
  double force_range = 30;         // force of largest / smallest unit (Fuglevand uses 100 for 120 units)
  double contraction_ms_slow = 90; // twitch time to peak of unit 0 (soleus-like S unit: Burke 1973 ~ 80-100 ms)
  double contraction_ms_fast = 30; // twitch time to peak of unit n-1 (FF unit: ~ 25-40 ms)
  double sat_c0 = 3.0;             // Ca2+ / actin saturation: act = 1 - exp(-c/c0), c = summed twitches (1 = one twitch peak)
  double thr_first = 0.02;         // recruitment threshold (pool excitation 0..1) of unit 0 ...
  double thr_last = 0.80;          // ... and of the last unit, geometrically spaced (Fuglevand 1993)
  double min_rate_hz = 8;          // firing rate at recruitment
  double rate_gain_hz = 50;        // Hz per unit of excitation above threshold
  double peak_rate_slow_hz = 35;   // rate cap of unit 0
  double peak_rate_fast_hz = 45;   // rate cap of unit n-1 (fast units need ~ 40-50 Hz to fuse)
  double rate_cv = 0.0;            // optional inter-spike-interval jitter in rate mode (CV of the phase increment), seeded
  uint64_t seed = 1;
  // Hill force-length / velocity (Zajac 1989; Millard et al. 2013)
  double fl_width = 0.45;          // Gaussian width of the active force-length curve (normalised length)
  double vmax_slow = 3.0;          // shortening speed limit of unit 0, optimal lengths per second
  double vmax_fast = 8.0;          // ... of unit n-1
  double curvature_k = 0.25;       // Hill a/F0 = b/Vmax
  double ecc_plateau = 1.8;        // eccentric force plateau (x isometric)
  double pe_k = 4.0, pe_e0 = 0.6;  // passive element exp(k*strain/e0) (Schutte 1993): F_pe = F0 at strain e0
  double damping = 0.05;           // parallel viscous damping, F0 per (l_opt/s)
  // Fatigue (after Xia & Frey-Law 2008; simplified to two states per unit: capacity <-> fatigued)
  bool fatigue = true;
  double fatigue_rate_slow = 0.002;  // 1/s at full activation, unit 0 (illustrative; "fatigue resistant")
  double fatigue_rate_fast = 0.03;   // 1/s, unit n-1 ("fast fatigable")
  double recovery_rate = 0.004;      // 1/s
  double fatigue_time_scale = 1.0;   // >1 compresses minutes of fatigue into game seconds
};

struct MuscleInput
{
  /// Pool excitation 0..1 (rate mode): the pool's mean motor-neuron rate normalised to the circuit's max (e.g. Hz / 60).
  /// Units fire regularly at rates given by the size principle. Ignored when `spikes` is set.
  double excitation = 0;
  /// Spike mode: spikes[i] = number of spikes of motor unit i in this slice (n_units entries). Typical wiring: one motor
  /// neuron per unit, ordered small to large. If the vector has n_units entries it is used instead of `excitation`.
  const std::vector< int >* spikes = nullptr;
  double length = 1.0;    // l / l_opt
  double velocity = 0.0;  // d(l/l_opt)/dt in 1/s, positive = lengthening
};

struct MotorUnit
{
  double weight = 0;         // share of pool fmax (sum = 1)
  double contraction_ms = 0; // twitch time to peak (T of the 2-stage kernel)
  double half_relax_ms = 0;  // measured/derived half relaxation time (~1.68 T)
  double threshold = 0;
  double peak_rate_hz = 0;
  double vmax = 0;
  double fatigue_rate = 0;
  // state
  double x1 = 0, x2 = 0;     // twitch cascade states (x2 in units of one twitch peak)
  double act = 0;            // saturated activation 0..1
  double capacity = 1;       // 1 = fresh
  double rate_hz = 0;        // commanded rate (rate mode) or last-slice rate (spike mode)
  double phase = 0;
};

class Muscle
{
public:
  explicit Muscle( MuscleParams p = {} ) { configure( p ); }
  void configure( const MuscleParams& p );
  void reset();
  /// Advance motor units by dt_ms (twitch kinetics, fatigue) and update force at (length, velocity).
  void step( double dt_ms, const MuscleInput& in );
  /// Force (N) at an arbitrary length / velocity with the present activation (does not change state). Used by Joint substeps.
  double force_at( double length, double velocity ) const;
  /// Hill curves (dimensionless), exposed for tests and docs.
  double force_length( double l ) const;
  double passive( double l ) const;
  static double force_velocity( double v_norm, double k, double ecc );  // v_norm = shortening speed / vmax, shortening > 0

  const MuscleParams& params() const { return p_; }
  const std::vector< MotorUnit >& units() const { return units_; }
  // outputs (updated by step)
  double force_N = 0, active_N = 0, passive_N = 0;
  double activation = 0;      // sum w_i * capacity_i * act_i, 0..1
  double fatigue_level = 0;   // 1 - mean capacity (weighted), 0 = fresh
  int n_active = 0;           // units that fired within the last 100 ms
  Readouts readouts() const;
  /// Render summary: per-unit activation (small to large) and overall activation / force fraction.
  struct View { std::vector< float > unit_activation; double activation, force_frac, fatigue; };
  View view() const;

private:
  MuscleParams p_;
  std::vector< MotorUnit > units_;
  std::vector< double > last_spike_ms_;
  double time_ms_ = 0;
  Rng rng_;
};

} // namespace spike::body
