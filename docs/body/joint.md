# Joint: hinge, agonist/antagonist, proprioceptors

`spike::body::Joint` (`body/joint.h`). One hinge (elbow-like) with a flexor and an extensor `Muscle`, rigid body dynamics and the
sensors of each muscle.

## Dynamics
`I theta'' = r (F_flex - F_ext) - b omega - m g c cos(theta) - (m_load) g L cos(theta) + tau_ext + tau_limit`, `theta = 0` horizontal,
positive = flexion (limb raised). Moment arm `r = 4 cm` (constant), flexor length `l = 1 - r (theta - theta_ref)/l_opt`, extensor
`1 + ...`; each muscle is `Muscle::force_at(l, v)` with `v = -/+ r omega / l_opt`. Defaults: forearm-like `I = 0.07 kg m^2`, `m = 1.5 kg`,
`c = 0.17 m`, `F_max = 350 N` per muscle, range -1.0..1.45 rad (the upper stop is below vertical so gravity never pulls past it).
Joint stops are a stiff spring-damper (150 N m/rad, 3 N m s/rad). A held `load_mass_kg` at `load_arm_m` adds torque and inertia;
`JointInput::ext_torque` is the push (N m). Semi-implicit Euler with 4 sub-steps per 1 ms slice; muscle twitch kinetics advance once per slice.
Tests: limp limb sags to the lower stop; equilibrium angle rises monotonically with flexor drive; antagonist and load pull it down;
co-contraction cuts push deflection (0.21 -> 0.15 rad); limits hold.

## Proprioceptors (rates in impulses/s, lengths in `l_opt`, velocities in `l_opt/s`)
- **Muscle spindle Ia** (primary): `max(0, b + k_L (l - 1) + k_V sgn(v)|v|^0.6)`: power-law velocity sensitivity with exponent 0.6
  (Houk, Rymer & Crago 1981; Prochazka & Gorassini 1998), release (shortening) at half gain, a dynamic fusimotor gain
  `(1 + gamma_dyn)` on the velocity term and a static one on bias/length (Mileusnic et al. 2006 describe the bag1/bag2/chain mechanics we collapse
  into these two scalars). Defaults `b = 40, k_L = 80, k_V = 40`, cap 250 Hz, 10 ms transduction low-pass.
- **Spindle II** (secondary): length only, `30 + 90 (l - 1)` (+ static gamma), cap 120 Hz.
- **Golgi tendon organ Ib**: `Ib = 120 F/(F + 0.25)` with `F = F_active/F_max + 0.3 F_passive/F_max` (Houk & Henneman 1967: mainly
  force, mostly active), 20 ms low-pass.
The coefficients are rescaled to the normalised units from the cat data and are illustrative; the *shapes* are tested: a ramp stretch
raises Ia far more than II and Ia scales with stretch velocity; a shortened antagonist's Ia falls; II is higher in the stretched muscle's posture;
Ib rises with force. Outputs `ag_aff / ant_aff = {ia, ii, ib}`; readouts `ag_Ia_hz, ag_II_hz, ag_Ib_hz, ant_*`, `theta_rad, omega_rad_s`, torques, forces, fatigue.
`view()`: normalised angle, activations, force fractions.

## Typical wiring
Sensors: `ag_Ia_hz`, `ant_Ia_hz`, `ag_Ib_hz` -> sensor channels; motors: flexor / extensor pool rate / 60 Hz -> `ag.excitation`, `ant.excitation`
(or per-neuron spikes). A stretch reflex is Ia -> homonymous motor neuron; Ib -> inhibitory interneuron -> homonymous motor neuron.
