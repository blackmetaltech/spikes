# Body modules

Reusable, deterministic physics for the "Body systems" tasks (REQ-0.2 section B). Pure C++20, **no NEST**: the modules take
rates or spike counts in and give forces, pressures and afferent rates out. They run on the engine's 1 ms slice
(`step(dt_ms, input)`; muscles/joint sub-step internally). Headers `engine/include/spike/body/*.h`, sources `engine/src/body/*.cpp`,
tests `engine/tests/test_body_*.cpp`.

| Module | Header | Doc |
|---|---|---|
| Muscle (Hill + motor units) | `body/muscle.h` | [muscle.md](muscle.md) |
| Joint (hinge, muscle pair, spindles, GTO) | `body/joint.h` | [joint.md](joint.md) |
| Heart + circulation + baroreceptor | `body/heart.h` | [heart.md](heart.md) |
| Lungs + CO2 + chemoreceptors | `body/lungs.h` | [lungs.md](lungs.md) |
| Eye (pupil, retina) | `body/eye.h` | [eye.md](eye.md) |
| Gut (peristalsis) | `body/gut.h` | [gut.md](gut.md) |

## Common API
Every module is a class `X` with `XParams` (defaults give the textbook resting state), `XInput`, `reset()`, `step(dt_ms, in)`,
public output members, `readouts()` (a `std::map<string,double>` for `BodyState::readouts` / HUD gauges, names carry the unit)
and `view()` (a small struct for the renderer: phases, fractions, per-segment arrays). `common.h` holds the shared helpers
(`Lowpass`, `DelayLine`, `Rng` = splitmix64, `clamp`, `sigmoid`). Nothing uses `std::random` distributions, so results are
identical on every platform (each test asserts run-to-run equality).

## Wiring to a task (what the task does, not the module)
- **Efferent**: map the circuit's motor channel to the module input: mean rate per neuron / reference rate -> 0..1 drive
  (`MotorReader::hz_per_neuron / 60`), or per-neuron spike counts -> `MuscleInput::spikes`, `HeartInput::sa_spikes`.
- **Afferent**: the module reports rates in Hz (`Ia`, `II`, `Ib`, `baro_hz`, `peripheral_hz`, `central_hz`, `retinal_hz`); the
  task writes them to its sensor channels (the input weight of the circuit turns them into current).
- Unit conventions: lengths normalised to optimal length (`l/l_opt`), pressures mmHg (cmH2O in the lung mechanics), volumes ml (L for lungs), time ms in `step`, s elsewhere.

## Honesty about parameters
Structures and orders of magnitude follow the cited literature; many coefficients are simplified or tuned so that the model
sits at textbook resting values and behaves plausibly in a game, and are marked "illustrative" in the per-module docs.
Every citation is in `catalog/citations.json` (docs/CITATIONS.md) and is checked against PubMed or Crossref with `tools/citations/check.py`; see `docs/WHAT-IS-REAL.md` for which numbers are literature values and which are game values.
