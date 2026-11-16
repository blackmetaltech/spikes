// Eye: pupil diameter from sphincter (parasympathetic, Edinger-Westphal) vs dilator (sympathetic) smooth-muscle
// activation, and retinal illuminance (luminance x pupil area, trolands) -> retinal afferent rate. See docs/body/eye.md.
#pragma once
#include "spike/body/common.h"

namespace spike::body
{

struct EyeParams
{
  double d_min_mm = 2.0, d_max_mm = 8.0;  // anatomical pupil range (Watson & Yellott 2012: ~2-8 mm)
  // Smooth-muscle kinetics (illustrative values matching Ellis 1981 / Loewenfeld 1993 time courses)
  double ec_delay_ms = 40;                // excitation-contraction coupling delay of the iris muscles
  double sph_tau_up_ms = 130, sph_tau_down_ms = 500;   // sphincter: fast contraction, slower relaxation
  double dil_tau_up_ms = 800, dil_tau_down_ms = 300;   // dilator: slow (redilation is slower than constriction)
  // Retina: Naka-Rushton on log10 trolands (Naka & Rushton 1966), plus a fixed latency
  double retina_latency_ms = 30;
  double r_spont_hz = 5, r_max_hz = 100;
  double e0_td = 1.0, x50 = 2.3, n = 2.0;  // x = log10(1 + E/e0); x50 = 2.3 ~ 200 td
};

struct EyeInput
{
  double sphincter = 0;   // parasympathetic drive 0..1 (EW nucleus pool rate / max)
  double dilator = 0;     // sympathetic drive 0..1 (superior cervical ganglion pool rate / max)
  double luminance = 1;   // scene luminance, cd/m^2
};

class Eye
{
public:
  explicit Eye( EyeParams p = {} ) : p_( p ) { reset(); }
  void reset( double sphincter0 = 0, double dilator0 = 0.5 );
  void step( double dt_ms, const EyeInput& in );

  double diameter_mm = 6;
  double sphincter_act = 0, dilator_act = 0;   // smooth-muscle activation 0..1
  double illuminance_td = 0;                    // retinal illuminance
  double retinal_hz = 0;                        // afferent rate (after retinal latency)
  Readouts readouts() const;
  struct View { double diameter_mm, diameter_frac, retinal_hz, luminance_log10; };
  View view() const;
  const EyeParams& params() const { return p_; }
  /// Retina transfer function (static): luminance cd/m^2 and pupil diameter -> afferent Hz.
  double retina_rate( double luminance, double d_mm ) const;

private:
  EyeParams p_;
  Lowpass sph_, dil_;
  DelayLine ec_s_, ec_d_, ret_;
  double lum_ = 1;
};

} // namespace spike::body
