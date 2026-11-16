// Lungs: inspiratory motor drive -> single-compartment mechanics -> breath volume / minute ventilation;
// CO2 gas exchange (arterial + tissue pools) and chemoreceptor afferents. See docs/body/lungs.md.
#pragma once
#include <deque>
#include "spike/body/common.h"

namespace spike::body
{

struct LungParams
{
  // Mechanics (Otis et al. 1956 single compartment): R dV/dt + (V - FRC)/C = Pmus
  double compliance_l_cmh2o = 0.10;  // total respiratory system ~ 0.1 L/cmH2O
  double resistance = 3.0;           // cmH2O s/L (tau = R C = 0.3 s)
  double frc_l = 2.4;
  double rv_l = 1.2;                 // floor (no active expiration modelled)
  double dead_space_l = 0.15;
  double pmus_max = 20;              // cmH2O at drive = 1 (rest ~ 0.25 -> VT 0.5 L; drive 1 -> VT ~ 2 L)
  double muscle_tau_ms = 80;         // diaphragm activation low-pass
  // CO2 (Grodins et al. 1967; Khoo et al. 1982 style, two pools). Pressures in mmHg, volumes in L (gas-equivalent).
  double k863 = 863;                 // mmHg: PACO2 = 863 VCO2 / VA (STPD -> BTPS)
  double vco2_rest_l_min = 0.20;
  double v_art = 4.0;                // fast pool (lung + blood) equivalent volume
  double v_tissue = 15.0;            // slow pool (tissue + brain) equivalent volume
  double q_c_l_min = 36.0;           // CO2 transport conductance blood <-> tissue (venous-arterial gap ~ 5 mmHg)
  double time_scale = 1.0;           // >1 speeds the gas dynamics up (game time)
  double pa_rest = 41.0;             // initial PaCO2
  // Chemoreceptors (Lahiri & DeLaney 1975 carotid body; Guyenet 2010 / Nattie & Li 2012 central)
  double periph_base_hz = 0.7, periph_gain_hz_mmhg = 0.45, periph_p0 = 35, periph_tau_s = 2.0, periph_max_hz = 40;
  double central_gain_hz_mmhg = 1.2, central_p0 = 38, central_max_hz = 60;  // on tissue/brain PCO2
};

struct LungInput
{
  double drive = 0;               // inspiratory motor drive 0..1 (phrenic pool rate / max)
  double metabolic = 1.0;         // VCO2 multiplier (exercise 3-8x)
  double inspired_pco2 = 0;       // mmHg (5% CO2 in air ~ 36 mmHg)
  double hypoxia = 0;             // 0..1 carotid body sensitisation (low PaO2)
};

class Lungs
{
public:
  explicit Lungs( LungParams p = {} ) : p_( p ) { reset(); }
  void reset();
  void step( double dt_ms, const LungInput& in );

  double volume_l = 2.4, flow_l_s = 0, pmus = 0;
  double tidal_l = 0;             // last completed breath
  double rate_bpm = 0;            // last breath-to-breath
  double ve_l_min = 0;            // minute ventilation, last 30 s window
  double va_l_min = 0;            // alveolar ventilation, same window
  double paco2 = 41, tissue_pco2 = 46;
  double peripheral_hz = 0, central_hz = 0;
  bool inspiring = false;
  int breaths = 0;
  double vco2_l_min = 0.2;
  Readouts readouts() const;
  /// volume_frac: 0..1 between RV and TLC-ish (lung size on screen); co2: PaCO2 / 40 for the colour of the blood gauge.
  struct View { double volume_frac, flow_l_s, paco2, ve_l_min, rate_bpm; bool inspiring; };
  View view() const;
  const LungParams& params() const { return p_; }

private:
  LungParams p_;
  Lowpass mus_{ 80, 80 }, per_{ 2000, 2000 };
  double va_f_ = 0.07, t_s_ = 0, breath_start_t_ = -1, v_start_ = 2.4, cum_in_ = 0, breath_va_ = 0;
  struct B { double t, vt, va; };
  std::deque< B > win_;
};

} // namespace spike::body
