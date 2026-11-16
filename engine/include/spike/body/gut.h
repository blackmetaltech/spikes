// Gut: a chain of N smooth-muscle segments (ICC slow waves + enteric excitatory / inhibitory motor input) and a bolus
// propelled by the pressure gradient across it. See docs/body/gut.md.
#pragma once
#include "spike/body/common.h"

namespace spike::body
{

struct GutParams
{
  int n_segments = 12;
  // Slow waves (interstitial cells of Cajal; Sanders 1996; Huizinga & Lammers 2009): 12/min in the small intestine,
  // aboral phase lag per segment. Alone they stay below the contraction threshold (no propulsion).
  double slow_wave_hz = 0.2;
  double slow_wave_amp = 0.4;
  double phase_lag_cycles = 0.04;      // per segment (wave travels aboral)
  // Smooth-muscle membrane drive m = ex - inh + amp * slow_wave; contraction target = sigmoid((m - threshold) / width)
  double threshold = 0.6, width = 0.07;
  double tau_up_ms = 500, tau_down_ms = 800;   // contraction / relaxation time constants
  double p_max = 10;                    // luminal pressure (cmH2O) at full contraction
  // Bolus (plug-flow in a compliant tube): v = mu * (p_oral - p_aboral - p_static) segments/s per cmH2O, low-passed
  double bolus_len = 1.2;               // segments
  double mu = 0.25;
  double p_static = 0.3;                // cmH2O, friction / wall adhesion threshold
  double v_tau_ms = 200;
};

struct GutInput
{
  const std::vector< double >* excitatory = nullptr;  // per segment 0..1 (ACh / substance P from enteric motor neurons)
  const std::vector< double >* inhibitory = nullptr;  // per segment 0..1 (NO / VIP)
  bool slow_waves = true;
};

class Gut
{
public:
  explicit Gut( GutParams p = {} ) : p_( p ) { reset(); }
  void reset( double bolus_pos = 2.0 );
  void step( double dt_ms, const GutInput& in );

  std::vector< double > contraction;   // 0..1 per segment
  std::vector< double > pressure;      // cmH2O per segment
  double bolus_pos = 2.0;              // centre, in segment units (segment i spans [i, i+1))
  double bolus_vel = 0;                // segments/s (positive = aboral)
  double distance = 0;                 // cumulative net aboral displacement
  Readouts readouts() const;
  struct View { std::vector< float > contraction; double bolus_pos_frac; double bolus_len_frac; };
  View view() const;
  const GutParams& params() const { return p_; }
  /// Pressure interpolated between segment centres at position x (segment units), clamped at the ends.
  double pressure_at( double x ) const;

private:
  GutParams p_;
  double t_s_ = 0;
};

} // namespace spike::body
