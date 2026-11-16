# Novice bench check, release 0.2.0

Can a player who starts from an EMPTY BENCH pass every task using only what the lab offers: parts from the catalogue, group size, wiring by dragging (default weight, then the weight and delay sliders), the E / I switch, the advanced neuron parameters (including the bias current `I_e`), the output kind (rate or modulator, with gain and tau) and the dc / Poisson switch of a sensor? This page records, per task, the simplest circuit that does, its scores, and what was changed so that it exists. It is also the source for hints: the recipes below are the ones written into the task briefs (`engine/src/tasks/*.cpp`, `brief()`).

## Method

- A headless harness builds each circuit with the lab's own rules and nothing else: the task skeleton (channels and their kinds, as `lab.gd:_skeleton` makes them), `Circuits.add_card` / `add_projection` (default weight = the catalogue's input weight of the post cell, delay 2 ms, sign from the E / I kind of the pre group), the first-connect sensor weight (`Circuits.input_weight`), group sizes from the stepper (1, 2, 3, 4, 6, 8, 10, 12, 20), segment wiring (`Circuits.set_wiring`), the numbered-channel family wiring (`Circuits.wire_family`) and every number snapped to its slider (range, step, square-law weights).
- Scored with `spike-run --task <id> --circuit <file> --seeds 1,2,3,4,5,6` (the circuit is reseeded as `1000 * seed + 1`, as in the lab's TEST x5). A pass counts only if every sensor and motor channel the task lists is wired, as in the lab (`Telemetry.wiring_issue`).
- Recipes were searched (grids, random search, hill climbing on the real task score) in the region the sliders reach, then rounded to values a player can set, and written into the briefs.

## Result

13 of the 14 tasks are passable from an empty bench on seeds 1-6 (6 of 6 each); **reach is not**: the best bench a novice can build with the editor passes 2 of 8 seeds. Details below.


| task | scores, seeds 1-6 | passes |
|---|---|---|
| reflex | 96.9 99.9 99.9 98.9 97.9 99.2 | 6/6 |
| worm | 75.7 77.3 75.5 74.5 75.8 76.7 | 6/6 |
| lamprey | 99.8 99.8 99.8 100.0 99.8 99.8 | 6/6 |
| stretch_reflex | 92.1 96.4 81.5 93.2 93.9 93.8 | 6/6 |
| renshaw | 95.9 100.0 96.9 100.0 100.0 100.0 | 6/6 |
| posture | 92.4 92.9 84.9 78.9 86.9 87.6 | 6/6 |
| reach | 71.3 67.0 56.0 4.7 53.5 7.3 | 2/6 |
| heart_pacemaker | 86.3 87.2 85.7 84.7 90.4 89.5 | 6/6 |
| heart_autonomic | 78.2 76.1 75.5 80.7 79.4 75.5 | 6/6 |
| baroreflex | 100 x6 | 6/6 |
| breathing | 100 x6 | 6/6 |
| pupil | 100 x6 | 6/6 |
| peristalsis | 100 x6 | 6/6 |
| fight_or_flight | 98.8 99.1 99.4 98.9 99.0 99.1 | 6/6 |


## Per-task recipes (what the brief now says, in editor operations)

Weights are in the post cell's unit (pA for `iaf_psc_alpha`, nS for the conductance and NESTML cells); sensor weights are per sensor (one weight for everything the sensor feeds); "bias" is the ADVANCED parameter `I_e` in pA; "segment" is the link inspector's wiring "neuron k to k" (shift 0 unless stated); a group is `iaf_psc_alpha` unless stated.

**reflex** (default weight 80 pA fails: "Missed pokes"; 120-200 pass, 240 fires on noise). `touch` > SN (10 cells) > MN (6 cells, link 140) > `withdraw`. The first connection gives the sensor weight 280.

**worm** (the lab blanks the pass until `turn_right` is wired too, so an idle group is connected to it). `ASER` (weight 1000) > A (8 cells) > TL (8 cells, +1000) > `turn_left`; `ASEL` (the same sensor weight: it is one number per sensor) > B (8 cells, set E / I to inhibitory) > TL (-1000); `turn_right` from an idle group of 4. With 4 cells per group it passes on some seeds only; one group between `ASER` and the pool is needed (ASER straight onto the pool scores 43-54).

**lamprey** (needs the dc weight range 0-2000 pA; before, a conductance cell capped the slider at 160). L and R: 8 `aeif_cond_alpha` cells each, preset Adapting (a 0, b 60, tau_w 100), both set inhibitory, L > R and R > L at the default -8 nS; `drive` (dc) at weight 600 onto both; L > `left_motor`, R > `right_motor`. Silent below 550 pA, a rhythm that fails on some seeds above 700.

**stretch_reflex** (the Ia weight is pA of current per Hz of afferent rate: 3, not the first-connect 300; the relay groups need tau m 60 ms or they never fire at the spindle rate). `ag_Ia` and `ant_Ia` (weight 3) > IaF, IaE (6 cells, tau m 60); `drive` (dc, weight 280) > MNF and MNE (20 cells each); IaF > MNF and IaE > MNE at 320; IaF > INF and IaE > INE (4 cells, inhibitory) at 240; INF > MNE and INE > MNF at -320; MNF > `ag_motor`, MNE > `ant_motor`. Ia > MN at 80 misses tendon taps on some seeds.

**renshaw** (stretch circuit, drive lowered to 200). Add `noise` (weight 100) > DESC (2 cells) > MNF and MNE at 40 (shared fibres), and Renshaw groups RF, RE (4 cells, inhibitory): MNF > RF, MNE > RE at 80; RF > MNF, RE > MNE at -160; RF > INF, RE > INE at -40; RF > RE, RE > RF at -160. The same circuit without the Renshaw cells also passes (see "Findings").

**posture** (the gamma pool must fire: TONIC > GAM needs 960 pA; at 80 gamma stays 0 and the limb rides up 0.2 rad). Spindle sensors at weight 3 > relay groups (Ia: 4 cells, II: 3 cells, tau m 60); pools MNF, MNE of 20 cells; Ia, II relays > their pool at 160; Ia relays > inhibitory groups of 2 at 80 > opposite pool at -320; `drive` (dc, 300) > TONIC (6 cells, tau m 60) > MNF at 160 (nothing onto MNE) and > GAM (4 cells) at 960; GAM > `gamma_motor`.

**reach** NOT passable robustly. The closest bench (the starting point in the brief): the stretch-reflex loop with the drive lowered to 160 pA so the pools are nearly silent at rest, `target` (dc, weight 2) > relay (3, tau m 60) > ag pool +800 and ant pool -320 (the hold command), `go` (dc, 3) > relay (3) > B1 (4 cells, tau syn 50 ms) 800 > ag pool 1000 / ant pool -400, B1 > B2 150 with delay 150 ms > ant pool 200, B2 > B3 400 with delay 100 ms > ag pool 900, each stage inhibiting the one before at -400. It passes seeds 1 and 2 (71, 67), seed 3 gets two reaches of three, seeds 4-8 fail (slow reach, creep before the cue of 0.10-0.12 rad, or a pool that never moves the arm); 2 of 8 overall. Several hundred candidate benches were scored on 1-4 seeds (random search, then hill climbing on the task score, including aeif motoneurons and graded-recruitment sub-pools): no bench passes more than 2 of 4 seeds. The hold-only equilibrium design (no bursts) reaches in 700-1250 ms, so the bursts are required, and they need delays of 100-150 ms (the delay slider now goes to 250 ms) and slow synapses (tau syn to 50 ms). The reference passes 12 of 12 seeds because its pools have per-synapse weight spread (`edges` with weights x U(0.5, 1.5), `gen_spinal.py`) and tuned cell parameters (C_m 500 pF, g_L 50 nS, V_th -52 mV) that no editor control reaches; identical cells make the pool onset, and the creep before 'go', depend on the seed.

**heart_pacemaker**. One group of 4 `sa_node_pacemaker` cells wired to itself (default weight 0.2 nS) > `sa_beat`: 70 beats/min, synchrony 0.6, score 85-90. A single cell scores 75.

**heart_autonomic** (needs the weight and gain sliders as they are now: links of 1500 pA, ACh gain 0.009, NE gain 0.003; the first-connect sensor weight 280 is too weak, 800 is needed). Sensors (weight 800) > CC, LOW, HIGH (4 cells each). NA (6, bias 380) > VG (6, segment, 1500) > `vagal` (ACh, gain 0.009, tau 500); IML (6, bias 370) > SG (6, segment, 1500) > `sympathetic` (NE, gain 0.003, tau 1500). Onto NA: CC -24, HIGH +10, LOW -6; onto IML: CC +12, LOW +10, HIGH -25. SA node (1 cell) > `sa_beat`; the engine's default modulation makes it follow ACh and NE. Bias 376 on NA gives 74 beats/min at rest, 380 gives 61-65, 385 gives 58-60: a few pA matter, the verdict ("resting tachycardia" / rate too low) points the right way.

**baroreflex** (the sensor weight must be the maximum, 1600: at 1000 the sympathetic side stays silent; RVLM bias 380, at 375 the pressure collapses). `baroreceptor` (1600) > AFF (6) > NTS (8) 280; NTS > NA (4, bias 370) 30; NA > VG (4, segment, 1500) > `vagal` (ACh, 0.012, 500); NTS > CVLM (6, inhibitory) 200; CVLM > RVLM (6, bias 380) -140; RVLM > IML (6, segment, 1500) > SG (6, segment, 1500) > `sympathetic` (NE, 0.0076, 1000); SA node > `sa_beat`.

**breathing** (needs tau syn up to 20 ms; the brief had left the synaptic time constants out, and without them the phrenic spikes read as 80-110 breaths/min). PBC: 10 `butera_burster` (g_NaP 2.8, g_tonic 0.29) onto itself at 0.03 nS; `drive` (Poisson, weight 0.005) > PBC. PHR: 8 iaf, bias 380, tau syn E and I 20 ms; PBC > PHR 4 pA; PHR > `phrenic`. EXP: 4 iaf, bias 430, tau syn I 10 ms, inhibitory; PBC > EXP -60, EXP > PHR -30. `chemo` (dc, weight 1) > RTN (6 iaf, bias 335) > PBC 0.09 nS and > PHR 8 pA. 15-18 breaths/min, ventilation x1.9-2.0, PaCO2 peak 51 mmHg.

**pupil** (every link at the default weight, +-80 pA, delay 15 ms on the reflex arm). `retina` > RGC (6, bias 100) > PON (4, bias 225) > EW (4, bias 340, tau syn I 10) > CG (4, bias 340) > `sphincter`; `threat` > HYP (4, bias 100) > IML (4, bias 410, tau syn E and I 10) > SCG (4, bias 340) > `dilator`; PON > IML inhibitory; HYP > EW inhibitory. Latency 166-195 ms (window 150-350: delay 2 ms gives 148 and fails on some seeds).

**peristalsis** (`ach` alone takes 32 s against the limit of 25; the `no` chain is needed). dist0..7 > S (8): dragging a sensor onto S wires dist k to neuron k. S > AIN (8, bias 340) segment, 100, delay 3; AIN > EMN (8, bias 380, tau syn I 10) segment shift -1, 36; EMN > ach0..7 (drag onto one channel: neuron k to ach k). S > DIN (8, bias 340) segment, 100, delay 3; DIN > IMN (8, bias 365) segment shift +1, 100; IMN > no0..7. Arrives in 10.6 s.

**fight_or_flight** (bias 380, not the reference's 377: the lab's I_e slider steps by 5, and 375 is the rheobase of an iaf cell). See the brief for the 10 groups; all links at the weights of the reference, the SA cell follows NE and ACh by the engine's default.

## What was changed

Lab (`godot/ui/screens/lab.gd`, `godot/game/circuits.gd`):
- **Square-law weight sliders** for projection and sensor weights (value = max x s^2): a burster onto an integrate-and-fire cell needs 0.005 nS, a relay 1500 pA, and the old linear slider (200 or 300 steps) could not set both (2 pA, 4 pA per Hz and 0.005 nS were unreachable).
- **Projection weight maximum 20 typical synapses** (was 12; a 1:1 relay needs 1500 pA = 19 x 80). The unit comes from the catalogue (`Catalog.unit_of`), so NESTML cells say nS, not pA.
- **dc sensor weights run 0-2000 pA whatever cell they feed** (was 20 x the catalogue weight in the cell's unit: 160 for an `aeif_cond_alpha`, which made lamprey impossible: the drive needs 600 pA).
- **Delay slider 0.1-250 ms** (square-law; was 0.1-20) for the burst generators; **tau syn E / I up to 50 ms** (was 20); **modulator gain from 0.001** (was 0.005: heart_autonomic needs 0.003, fight_or_flight 0.0015).

Briefs (`engine/src/tasks/*.cpp`): every task but the trivial heart_pacemaker now ends with "A recipe that works", with units, group sizes and the numbers measured above; worm says `turn_right` must be wired (the lab blanks the pass otherwise); peristalsis says the `no` chain is required; breathing gets its missing synaptic time constants; heart_autonomic, baroreflex and fight_or_flight give the bias and sensor weights that matter. Reach gets a starting point that passes some seeds.

Not changed: pass bars, reference circuits, lesion tables, catalogue default weights (80 pA stays: only reflex needs more and says so).

## Findings the recipes exposed

- **reach is not passable from the editor** (above). Remedy candidates, in order of cost: (a) a per-projection "weight spread" control in the lab (the reference needs it; first test: spread 0.3-0.5 changes the behaviour but needs re-tuning, not tried further); (b) widen the task's creep limit or lengthen the pre-cue window the creep is measured over; (c) make the hold-only equilibrium reach count (it takes 700-1250 ms against the 700 ms bar).
- **renshaw passes without Renshaw cells**: with the noise sent to one shared relay group the tremor is 0.1-0.4 % against the reference's unbraked 1.1-5.4 % (the control curve was measured on the reference's own pools and is not re-measured on the player's circuit); the same bench without the Renshaw groups scores 91-100 on seeds 1-6. The lesson "recurrent inhibition cuts tremor" is not enforced. Fix by measuring the control on the player's own circuit.
- **First-connect sensor weights are wrong for every dc sensor** (300 pA per unit for all): Ia needs 3 pA/Hz, the lamprey drive 600, posture 300, heart sensors 800 (poisson). A task-declared default weight in `Task::ChannelKind` (add-only) would fix it; for now the briefs state the numbers.
- **Bias values are on a knife edge for `iaf_psc_alpha`**: rheobase is 375 pA and the slider steps by 5, so 376/377 (the reference values) are unreachable; 380 works in the autonomic tasks, 375 leaves cells silent. The briefs say so.
- **Default synapse 80 pA** (was 250): reflex needs 120-200 (default fails with "missed pokes"), worm and the heart relays need 1000-1500 (any default would be wrong); lamprey, pupil, stretch and posture are unaffected.
