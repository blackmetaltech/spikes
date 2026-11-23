---
title: See it in action
order: 5
section: Start here
summary: Every clip and screenshot of the game, captured from the released build, with what each one shows.
---

`scripts/site-media.sh` captured everything here from the exported game. It drives the game through its screenshot hooks and records with Godot's movie mode, so every frame is rendered and the circuit runs at its true speed (the lab's speed is shown in the top right of each full-screen clip). Clips play while on screen. With reduced motion on in your system, they wait for you to press play. Select a screenshot to see it full size.

> **Note:** These media predate several changes: the Bestiary specimen views, the rewritten circuit editor with its Undo and Redo buttons, the score stamp with its criteria rows, and the map's wing selector. The game looks a little different now, but the circuits behave as shown. Run `scripts/site-media.sh` to make new ones.

## Bodies

<div class="gallery">

![A lamprey swims on a half-centre oscillator: the raster fills with alternating bursts and the body bends left and right](../assets/media/lamprey.webm "The lamprey. Left and right pools inhibit each other and tire, so they take turns. The toy swimmer turns the alternation into a travelling wave and forward speed. 1x.")

![The heart: the drawn heart beats, the ECG scrolls, heart rate and pressure rise as exercise starts](../assets/media/heart.webm "The heart. Sympathetic and vagal pools steer a sinoatrial pacemaker cell; at rest the vagal tone holds the rate near 65, and when exercise starts it climbs toward 120. 2x.")

![The baroreflex: the body stands up, mean pressure dips on the gauge and is pulled back](../assets/media/baroreflex.webm "The baroreflex. Standing up pools blood in the legs and pressure falls. Baroreceptor afferents drive the brainstem circuit, which answers through the vagal and sympathetic outputs within seconds. 4x.")

![The stretch reflex: tendon taps stretch the muscle and the limb answers](../assets/media/limb.webm "The stretch reflex. A tap stretches the muscle and spindle Ia afferents fire. The motoneurons answer within tens of milliseconds, and reciprocal inhibition holds the antagonist off. The joint-angle plot shows each tap. 1x.")

</div>

## The Bestiary

<div class="gallery">

![The leech heartbeat: two heart interneurons burst in turn](../assets/media/leech.webm "The leech heartbeat. Two heart interneurons inhibit each other and burst in turn. Each silences its own side's motor neuron, so the sides beat alternately. The gauges read the period and the overlap. 1x.")

![The fly compass: a bump of activity moves round the ring as the fly turns in the dark](../assets/media/compass.webm "The fly's compass. Sixteen compass cells in a ring hold a bump of activity. Shifting cells move it when the fly turns, and it follows the fly's true heading. 1x.")

![The crab pyloric rhythm: the raster fills in with the triphasic pattern](../assets/media/raster.webm "The pyloric rhythm. Conductance-based crab STG cells start from rest. Within a second the raster settles into the pacemaker burst, then LP, then PY. 1x.")

</div>

## Theory, CHECK and the result

<div class="gallery">

![The phase plane of one neuron: the current steps past onset and the trajectory circles a loop of repeated firing](../assets/media/phase.webm "The phase plane on Where firing starts. Nullclines and fixed points come from the cell's live parameters. Once the current passes onset the resting state is gone (the readout says so) and the trajectory runs round a loop of repeated firing. 0.05x, so you can follow the loop.")

![CHECK on a hand-built lamprey circuit: a toast reports the pass and the gauge marks the check](../assets/media/check.webm "CHECK. The circuit is scored headless, in five worlds, while the live run carries on. The verdict arrives as a toast and says in words what the circuit did. 1x.")

![A CHECK result card: not passed, left and right are not alternating](../assets/media/check-fail.webp "A failing verdict: the same lamprey without its crossed inhibition. It names the missing mechanism, never a number to aim for.")

![An end card: passed, two stars, the third star explained](../assets/media/check-pass.webp "A hand-built circuit passes in all five worlds and earns two stars. The card says how to earn the third (16 neurons or fewer).")

</div>

## The campaign

<div class="gallery">

![The campaign map on the heart chapter, with levels pinned to the drawn heart](../assets/media/map-body.webp "Plate III, the heart: levels pinned to the anatomy, with the selected level and its neighbours labelled.")

![The campaign map on the leech chapter of the Bestiary](../assets/media/map-bestiary.webp "Plate VII, the leech: the engraved plate of the Bestiary wing, with its levels and a diagnosis case.")

![The campaign map on the dynamics chapter of the Theory wing](../assets/media/map-theory.webp "Plate XII, dynamics: phase-plane figures computed from the FitzHugh-Nagumo model.")

![The title screen](../assets/media/title.webp "The title: Spike, beside procedurally inked Golgi cells.")

</div>

## The lab, close up

<div class="gallery">

![The lab on the lamprey level](../assets/media/lab-lamprey.webp "The lab: the bench, the body, the gauge and verdict, the raster, the scopes and the brief.")

![The stretch reflex level](../assets/media/limb.webp "The stretch reflex, mid-run: Ia afferents, interneurons and a column of motoneurons.")

![The baroreflex level](../assets/media/baroreflex.webp "The baroreflex: the brainstem circuit from the baroreceptor afferents to the vagal and sympathetic outputs.")

![The autonomic heart level](../assets/media/heart.webp "The autonomic heart: the drawn heart, its readouts and the ECG.")

![The leech heartbeat level](../assets/media/leech.webp "The leech heartbeat with its period and overlap gauges.")

![The fly compass level](../assets/media/compass.webp "The fly compass: landmark cues, the ring and the two rings of shifting cells.")

![The crab pyloric level](../assets/media/crab.webp "The crab pyloric rhythm on conductance-based STG cells.")

![The one-neuron phase plane level](../assets/media/phase.webp "Where firing starts: the phase plane above the raster and the voltage trace.")

</div>

## New in this build

<div class="gallery">

![The interactive tutorial on the first level](../assets/media/tutorial.webp "Plate 0, the tutorial: twelve steps on the first neuron, the control to press lit and the rest dimmed.")

![The mushroom body level with its Kenyon cells as one ellipse](../assets/media/editor-large.webp "A large circuit: two hundred Kenyon cells drawn as one stained ellipse; the bench zooms, pans and undoes.")

![The end card after a pass](../assets/media/end-card.webp "The end card: the PASSED stamp, the score out of 100, the stars and the next level.")

![The campaign map on a fresh save](../assets/media/map-fresh.webp "The campaign map: the three wings, one chapter row and Continue to the next open level.")

</div>

## Diagnose and dissect

<div class="gallery">

![A diagnosis case on the crab pyloric circuit](../assets/media/diagnosis.webp "A diagnosis case: symptoms, a probe counter, the lesion lab and five candidate lesions.")

![A diagnosis case on the fly compass](../assets/media/diagnosis2.webp "The compass that walks off: the bump holds in the dark but turns only one way.")

![The lesion lab on the leech heartbeat](../assets/media/lesion.webp "The lesion lab: every single cut swept over three seeds, each row with the score's mean and range and what broke.")

![The Analysis screen](../assets/media/analysis.webp "Analysis: a three-node motif census against degree-preserving rewiring, and spike statistics per cell.")

![The parts bin in the lab](../assets/media/parts.webp "The parts bin: every cell type with an f-I curve and a voltage trace measured by the engine.")

![The lab notebook](../assets/media/notebook.webp "The notebook: a cited finding for every level you pass.")

![The What is real screen](../assets/media/real.webp "What is real, inside the game.")

</div>

## Making these again

One command rebuilds the media from the game. See [Building from source](building-from-source.md) and `docs/BUILD.md` in the repository. Each clip also exists as an MP4 and a small GIF beside the WebM in `site/assets/media/`, for READMEs and old browsers.
