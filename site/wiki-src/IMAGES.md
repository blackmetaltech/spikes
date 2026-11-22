# Images wanted by the wiki pages

Every image is referenced from a page as `img/<name>.png`; the files are WebP (`img/<name>.webp`), and the builder falls back from a missing `.png` to the `.webp` of the same name. They are made by `scripts/site-media.sh` (the `wiki` lines, with the window size, the crop and the `SP_*` hooks of each), from the exported game at 1920x1080 in the desktop layout, so a recapture needs no manual cropping. Shots come from the game unless marked "diagram".

| File | Used on | What it should show |
|---|---|---|
| `first-hour-lab.png` | first-hour | The lab on the first level, Threshold, mid-run: the sensor card `pulse` on the left wired to one sienna neuron, wired to the `spike` card on the right; the inspector on the lower left showing the brief with TO BUILD and TO PASS; the raster, a trace and the gauge on the right; the timeline labelled "small blip" and "big step". |
| `lab-annotated.png` | how-to-read-the-lab | The lab on a task level (the reflex arc) with numbered callouts (a diagram overlay): 1 top bar, 2 palette, 3 bench with sensor and motor cards, 4 inspector, 5 world panel with body and timeline, 6 score card, 7 raster, 8 traces, 9 gauges. |
| `end-card.png` | how-to-read-the-lab | The end-of-episode card after a pass: the PASSED stamp, the score out of 100 and the stars, with one primary Next level button. |
| `campaign-invertebrates.png` | campaign-body | The campaign map on the Invertebrates chapter: the worm plate with its pins, some of them passed. |
| `campaign-spinal.png` | campaign-body | The Spinal cord chapter: the forearm plate with its pins and the boss pin. |
| `campaign-heart.png` | campaign-body | The Heart chapter: the heart plate with its pins. |
| `campaign-breathing.png` | campaign-body | The Breathing chapter: the lungs plate with its pins. |
| `campaign-autonomic.png` | campaign-body | The Autonomic chapter: the eye, gut and body plate with pins and the boss pin. |
| `bestiary-worm.png` | campaign-bestiary | The Bestiary Worm chapter: the worm plate with its touch and command cells and its pins. |
| `bestiary-leech.png` | campaign-bestiary | The Leech chapter: the leech plate, nerve cord and two heart tubes. |
| `bestiary-aplysia.png` | campaign-bestiary | The sea slug chapter: siphon and gill plate. |
| `bestiary-fly-compass.png` | campaign-bestiary | The fly compass chapter: a ring of cells with a bump of activity. |
| `bestiary-fly-mb.png` | campaign-bestiary | The fly mushroom body chapter: projection neurons, Kenyon cells, APL and the output neuron. |
| `bestiary-crab.png` | campaign-bestiary | The crab chapter: the stomatogastric ganglion plate. |
| `theory-dynamics.png` | campaign-theory | The phase-plane view of a one-cell Dynamics level: two nullclines, a filled stable fixed point and the cell's live trajectory, the recovery nullcline in slate. |
| `theory-rates.png` | campaign-theory | The rate strips of `r_oscillate`: the excitatory and inhibitory units swinging on a shared scale. |
| `theory-memory.png` | campaign-theory | The pattern grid of `h_complete`: the stored pattern framed in dashed gamboge, the cued cells and the whole pattern lighting up. |
| `theory-tuning.png` | campaign-theory | The `t_decode` view: the stimulus dial, a bar per cell and the decoded-direction arrow in gamboge. |
| `bodies-limb.png` | bodies | The limb view: a joint with flexor and extensor muscles and the spindle readouts. |
