# Playtest report (0.5 in progress)

Method: game under xvfb, 1920x1080; novice circuits written from each brief + lab defaults, scored with spike-run, loaded in game with SP_CIRCUIT + SP_CHECK. Circuits in scratchpad/c/.

## High
1. Reference-copy flag catches self-built circuits: weights compared to 3 sig figs, so round novice values (450 pA m_threshold, 150 m_convergence, 800/-800 m_recurrent, likely -1000 m_inhibition) are flagged "earns no stars". (06, 08, 22)
2. worm: brief says turning one way is enough, CHECK demands turn_right be wired. (12)
3. w_latch rejects a self-excitation latch (autapse holds at 270 Hz); check ignores self-loops. (34)
4. Campaign screen overflows at 1920 px with 8 tabs: right panel cut, Sandbox off-screen. (02, 26-29)
5. Boss CHECK 13-18 s vs 8 s target; the game never uses the quick episode. worm 3.4 s, lamprey 4.6 s also over. (24)
6. Numbers in verdicts/hints: ~14 pass verdicts quote measurements; 8 fail verdicts append "(The run itself: ...)"; baroreflex fail verdict prints band "80-100"; pupil tier-1 hint "200-250 ms". (22, 23)
7. Weight slider caps at 1600 pA, references need 8000 (a_depress), 2500 (aplysia_gill), -4000 (w_switch), -2000 (worm_touch); at cap verdict still says strengthen. (36)
8. Fly compass levels (c_ring, c_shift, fly_compass) not in godot/bin or campaign; c_ring brief says 8 cells, verdict demands 16. (30)

## Medium
9. Verdicts contradict parts: l_burst IB card called "a plain cell"; lamprey silent cells "fires steadily"; a_facilitate "already at its largest" at 2.2x; aplysia_gill "no stronger" for 0->2 spikes; l_rebound "only returns to rest" despite rebound spikes.
10. False "You built a rhythm by accident!" on loop-free circuits with scheduled stimuli (m_convergence, Aplysia).
11. Overlays stack over the result modal (CHECK toast, hints toast, surprise, stamp, notebook).
12. "Hints unlocked: two runs have missed" after the first CHECK.
13. Unfed gauges: leech_heart period and overlap (0.0 in green), fly gauges.
14. dx_leech patient fires both sides at 42 Hz although symptoms say right side almost silent.
15. dx "no diagnosis submitted" verdicts name the missing connection (hidden in game today); dx_leech says "Running.".
16. Pass criteria hidden behind More in every brief; boss brief's More cut off.
17. Serotonin slider labelled noradrenaline.
18. Diagnosis: Reveal answer after one wrong pick; dx_aplysia symptom text names the answer.

## Low
Repeated axis ticks; sentinel -1 shown; gauge vs strip mismatch; 0-400 Hz gauge bands; empty bench scores 28 (l_rebound); map label overlaps, unlabelled Bestiary middle nodes, blank plates; clipped limb labels; raster row labels overlap; title says v0.4.0; Parts button label wrong.

## Difficulty
Spikes: worm, m_half_centre (default drive can't fire), l_rebound (needs unmentioned bias), aplysia_gill (needs 5x gain). Dips: lamprey passes with 2 cells (repeat of m_half_centre), m_subtraction first try, heart_pacemaker easy. worm_touch: a 4-cell toy passes the real-wiring level.

## Design top 10
1. Reference lock punishes honest players. 2. Structure checks enforced but not stated in the brief. 3. Real brief buried behind More. 4. Lumpy difficulty curve. 5. Slow boss CHECK breaks the loop. 6. Overlays compete at feedback time. 7. Slider caps below answers, no numeric entry. 8. Diagnosis brute-forceable / symptoms leak. 9. Bestiary looks unfinished. 10. On-screen numbers disagree.
