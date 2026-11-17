# Spike 0.3 game design plan: progression, difficulty, learning, fun, time

Status: a proposal for review. Grounded in the design literature below; sources are marked **[searched]** where checked against summaries or primary material this session, and **[memory]** where taken from my own recollection of the book (verify against the text before quoting).

## 0. Where 0.2 stands (the starting point)
- 14 tasks in 5 chapters, each a large build (7–49 neurons), with stars for pass, pass on 5 seeds, and staying under the neuron and energy budget.
- Strengths:
  - A real simulator with real bodies.
  - A strong look.
  - Honest verdicts.
  - A lesion lab.
  - A sandbox.
- Weaknesses, seen through the books below:
  - **Difficulty jumps:** the steps between tasks are big. A novice goes from a 15-neuron reflex to a 47-neuron baroreflex with no smaller exercises in between, and 13 of 14 tasks are only passable *with the brief's recipe*.
  - **Too many concepts at once:** each task introduces several concepts together; nothing isolates them.
  - **Nothing after the pass:** once three stars are earned there is no reason to replay, so there is no skill ceiling.
  - **No audio and little juice:** the only reward moment is the score card.
  - **Overlong runs:** episodes are 10–150 s of simulated time. The long ones (breathing, 150 s) are too long for the build → run → tweak loop.

## 1. What the literature says, and what we take from it

| Source | Idea | What Spike does with it |
|---|---|---|
| Koster, *A Theory of Fun* **[searched]** | Fun is the reward for learning patterns. Boredom comes when there is nothing new to learn: no visible pattern, patterns revealed too fast or too slow, or the game is mastered. | Every level must teach exactly one new pattern and show it clearly. The pace of new patterns is the main dial. Mastery needs a ceiling (section 6), or the game ends at 3 stars. |
| Gee, *What Video Games Have to Teach Us…* **[searched]** | 36 learning principles. Key ones: *regime of competence* (the edge of ability), *explicit information on demand and just in time*, plus **[memory]** *fish tank* (a simplified ecosystem), *sandbox* (low risk), *probe–hypothesize–reprobe*, and skills taught as strategies. | Add **fish-tank micro-puzzles**: 1–4 neuron puzzles that isolate one concept. Information appears just in time: the codex and recipe surface when a verdict needs them. The **predict-then-run** lesion loop is probe–hypothesize–reprobe. |
| Schell, *The Art of Game Design* **[searched; lens names from memory]** | Many lenses on one design. **[memory]** The interest curve (hook, rising peaks, climax), the elemental tetrad (mechanics, story, aesthetics, technology), and lenses on surprise, flow, skill, reward, and endogenous value. | Each chapter gets an interest curve: a hook micro-puzzle, rising tasks, a "boss" scenario, and an epilogue sandbox. A light story layer (section 7). |
| Adams & Dormans, *Game Mechanics: Advanced Game Design* **[searched]** | Internal economies: sources, drains, converters and feedback loops (Machinations). Progression and emergence, which can be combined with feedback loops. LeBlanc: positive feedback magnifies early successes; negative feedback prolongs a game and magnifies late successes. | Spike is emergent (circuits) inside a progression (the campaign). Its economy is ATP as an energy drain, plus neuron and synapse counts. Keep the meta-economy light; no grind. Use negative feedback in difficulty, through hints and a guided mode, so stuck players recover. |
| Sylvester, *Designing Games* **[searched]** | A game is an engine of experience. Elegance, depth and skill ceiling: chess's ceiling is beyond mastery, while shallow games bore once mastered. Iterate by experiment. | Add depth through optimisation metrics and challenge variants. Iterate with playtest telemetry (section 10). |
| Zachtronics (Barth, GDC 2019 "Open-ended puzzle design") **[searched]** | Open-ended puzzles: a toolset plus acceptance criteria, with no predetermined best solution. Players find optimisations the designers did not anticipate. Some players ignore leaderboards and set their own goals. **[memory]** After solving, players see histograms of cost, cycles and area compared with other players. | Spike is already this genre. Add **post-solve histograms** on three axes (neurons, energy, latency or accuracy). That gives open-ended mastery without forcing competition. |
| Csikszentmihalyi, *Flow* **[memory]** | Flow comes when challenge matches skill. Anxiety comes when challenge is above skill, boredom when it is below. | Difficulty tiers and the hint ladder keep players in the channel. Optional challenge variants serve the over-skilled. |
| Hunicke, LeBlanc & Zubek, MDA **[memory]** | Mechanics → dynamics → aesthetics. Designers work forwards, while players experience it backwards. The "8 kinds of fun" include discovery, challenge and expression. | The target aesthetics, in order, are **Discovery** ("so that's how a heart is paced"), **Challenge**, and **Expression** (my own circuit). Every mechanic is judged on which of these it serves. |
| Hodent, *The Gamer's Brain* **[memory]** | Onboarding works within the limits of perception, attention and memory. Teach by doing; keep cognitive load low. Motivation follows self-determination theory: competence, autonomy and relatedness. | The first 10 minutes are pure doing, with progressive disclosure of the user interface. Advanced parameters stay hidden until a level needs them. Autonomy comes from optional paths and open solutions; relatedness from shared circuits and histograms. |
| Swink, *Game Feel* **[memory]** | Real-time control, simulated space and polish (juice) make an interaction feel alive. | Spike sound (section 8), pulses, slow motion on key events, and a celebration on the first pass. |
| Fullerton, *Game Design Workshop* **[memory]** | A playcentric process: prototype, playtest, iterate. Formal and dramatic elements. | Telemetry and persona playtests become release gates (section 10). |

## 2. Design pillars
1. **Real neuroscience, honestly scored.** Unchanged from 0.2.
2. **One new idea per level.** Koster and Gee: the next pattern is always just within reach.
3. **Your circuit, your way.** Zachtronics: open solutions, and mastery measured, not prescribed.
4. **Every failure teaches.** Verdicts point at *where* in the circuit and *why*, in the moment it happens.

## 3. Learning ladder: a concept graph
Each level introduces exactly one concept from this list, reuses at least two earlier ones, and states its concept on its card.

| # | Concept | Fish-tank micro-puzzle (new) | Used by big task |
|---|---|---|---|
| 1 | Threshold and rheobase | make one cell fire on a step | reflex |
| 2 | Excitation (weight, delay) | relay a spike with a 5 ms delay | reflex |
| 3 | Convergence and coincidence | fire only when 3 of 5 inputs arrive together | reflex |
| 4 | Inhibition | silence a cell with an inhibitory neighbour | worm, lamprey |
| 5 | Subtraction (ON minus OFF) | output = A − B | worm |
| 6 | Adaptation | burst then slow down (aeif b, tau_w) | lamprey |
| 7 | Mutual inhibition and half-centre | flip-flop that alternates | lamprey |
| 8 | Coupling and synchrony | two oscillators lock phase | heart_pacemaker |
| 9 | Intrinsic pacemaking | pick the cell that fires on its own | heart_pacemaker |
| 10 | Neuromodulation | slow a pacemaker with ACh | heart_autonomic |
| 11 | Recruitment (size principle) | graded force from a pool | stretch_reflex |
| 12 | Reciprocal inhibition | antagonist relaxes | stretch_reflex |
| 13 | Recurrent inhibition | gain control (Renshaw) | renshaw |
| 14 | Negative feedback control | hold a value against a push | posture, baroreflex |
| 15 | Feed-forward plus feedback | anticipate a cue, correct the error | heart_autonomic |
| 16 | Timed bursts (pattern generator) | three bursts in order | reach |
| 17 | Rhythm plus sensory gating | a burster sped up by CO₂ | breathing |
| 18 | Travelling waves and polarity | ascending excitation, descending inhibition | peristalsis |
| 19 | Divergent command | one cue fans out to many effectors | fight_or_flight |

That gives about 19 micro-puzzles plus 14 big tasks, so about 33 required levels in 0.3. Each micro-puzzle is 1–4 neurons, takes 2–5 minutes, and uses a 2–5 s episode.

## 4. Progression structure (macro)
- **Spine and branches.** Each chapter has its micro-puzzles and big tasks on a mandatory spine, plus optional branches (challenge variants, diagnosis cases).
- **Unlock rule:**
  - A chapter opens when its *concepts* are mastered: the spine tasks are passed, not 50% of anything. This replaces the 0.2 "half the previous chapter" rule.
  - Optional branches never gate the spine.
  - Two chapters are always open at once, for autonomy (self-determination theory).
- **Interest curve per chapter** (Schell):
  1. A *hook* micro-puzzle with an instant result.
  2. Rising micro-puzzles and tasks.
  3. A *boss* scenario that combines everything with perturbations: fight-or-flight for Autonomic, and a new "sprint" for Spinal.
  4. An *epilogue*: a sandbox prefilled with that chapter's parts and a codex reveal.
- **Chapter order:** Invertebrates → Spinal → Heart → Breathing → Autonomic, with the boss tasks capping each chapter.

## 5. Difficulty
Difficulty has three independent axes, so it never means just "more neurons":
1. **Structural:** the number of concepts combined and the number of populations.
2. **Tolerance:** how tight the pass bands are, how many seeds must pass, and how much noise there is.
3. **Perturbation:** how strong and frequent the pushes, CO₂ steps, haemorrhages and so on are.

**Modes,** chosen per save and switchable at any time with no penalty except star labels:

| Mode | Recipe in brief | Hints | Seeds to pass | Noise | Bands |
|---|---|---|---|---|---|
| Guided | visible | free, 3 tiers | 1 | low | wide |
| Standard (default) | hidden behind "show recipe" (costs the efficiency star) | 3 tiers, unlocked after 2 failed runs | 1 (5 for 2 stars) | normal | normal |
| Researcher | none | none | 5 required | physiological | tight |

- **Hint ladder.** This is negative feedback that keeps players in flow.
  - **Tier 1** names the concept involved: "this needs inhibition between the two sides".
  - **Tier 2** points at the place: it highlights the population or link the verdict is about.
  - **Tier 3** gives a partial recipe.
  - Hint use is recorded, not punished.
- **No dynamic difficulty adjustment inside a level.** Puzzles must be stable to be fair; difficulty adapts only through hints and mode.
- **Challenge variants** are optional and add depth. Examples:
  - "with half the neurons";
  - "with the vagus already cut" (a lesioned body);
  - "at 2× noise";
  - "using only Izhikevich cells".

## 6. Mastery and replay (skill ceiling)
- **Post-solve histograms.** These are Zachtronics-style, on three metrics per task: **neurons**, **energy** (spikes/s, as ATP), and a task-specific quality (latency, error or synchrony).
  - The distribution is generated offline from developer and AI solutions plus players' opt-in uploads (later, in 0.4 Share).
  - Your point is marked on each histogram.
  - This replaces the third star's single neuron budget with "beat the median on any axis".
- **Personal goals:** a "pin a target" button for self-set goals, for players who ignore histograms.
- **Diagnosis cases** reverse the puzzle. You get a *working* circuit with a hidden lesion, plus the patient's symptoms; you probe it with the scopes and lesion lab, then name the lesion. This is Gee's probe–hypothesize–reprobe loop, uses the lesion lab as play, and fits the clinical theme. Examples:
  - orthostatic hypotension → a sympathetic lesion;
  - Horner's syndrome → a lesion of the superior cervical ganglion pathway;
  - clonus → loss of Renshaw or descending inhibition.

## 7. Fun element and fiction
- **Target aesthetics:** Discovery > Challenge > Expression (MDA).
- **Light fiction:** you are a new member of a physiology lab. A lab notebook records each solved level with a short, true, sourced fact and a "what real neuroscientists found" card, with its citation. Chapters are "rotations" (spinal clinic, cardiology and so on). No heavy story.
- **Surprise moments** (Schell's surprise lens), built from emergent behaviour that already happens:
  - a circuit that oscillates when you didn't intend it;
  - a lesion that does something unexpected.

  The game detects and celebrates these: "You built a CPG by accident!"
- **Expression:**
  - the sandbox exports a share code, an image of the circuit and its raster;
  - circuit names;
  - a gallery of your circuits.

## 8. Feel and juice
- **Sound:**
  - each spike is a short click, pitched by population and rate-limited;
  - bursts form chords;
  - the heart beats audibly;
  - the lungs breathe.

  This is the biggest missing piece; neuroscientists literally listen to spikes. Audio uses an offline generator, in the same style as gigawatt and Mugshot.
- **Moments:**
  - slow motion and a camera push on the first pass;
  - pulse trails on the critical path;
  - the verdict highlights the guilty population in the editor.
- **Micro-feedback while editing:**
  - a live preview of the selected neuron's f–I point;
  - while a weight slider is dragged, the expected first-spike latency.

## 9. Game time and pacing
- **Run length** (wall time at default speed):
  - micro-puzzles ≤ 10 s;
  - tasks ≤ 45 s, with the default speed auto-chosen and long episodes compressed by "skip to next event";
  - **Instant check**: a headless score in under 2 s, using `spike-run`-style headless scoring, so the build → check loop is quick.
- **Time on task:**
  - micro-puzzle 2–5 min;
  - big task 10–25 min for a novice;
  - a chapter is about 2–3 h;
  - **critical path** about 12–15 h;
  - with variants, diagnosis and optimisation, 30–50 h.
- **Session design:** a natural stopping point every 20–40 min (a chapter node). Autosave, and resume straight into the lab.

## 10. Measuring it (playtest gates for 0.3)
**Local telemetry:** opt-in, stored on the device, exportable. It records:
- time per level;
- run count;
- hint tier used;
- quits mid-level;
- the edits made before the first pass.

**Gates** (AI persona playtests plus the novice harness, then human playtests):
- **Micro-puzzles:** every one solvable by the novice harness without recipe or hints in ≤ 8 runs.
- **Spine tasks:** every one solvable in Standard mode with tier-1/2 hints only.
- **Long runs:** no level has a median wall time per run above 45 s.
- **Unfinished levels:** the persona critique reports no level where a novice is stuck with no next action.
- **Physiology:** every concept card has a correct, cited fact. The physiologist persona signs this off.

## 11. 0.3 scope ("Learning curve") and work split
| Area | Work |
|---|---|
| Content | 19 micro-puzzles (tasks with small bodies and 2–5 s episodes), 2 boss scenarios (spinal sprint, autonomic exam), 6 diagnosis cases |
| Progression | Spine and branches map, concept unlock rule, two open chapters, interest curve per chapter, lab-notebook codex |
| Difficulty | Guided, Standard and Researcher modes, hint ladder (concept → highlight → partial recipe), challenge variants |
| Mastery | 3-metric histograms (seeded from offline solution sweeps), pin-a-target, surprise detection |
| Feel | Spike, heart and breath audio (generator plus GameAudio port), pass celebration, verdict highlight, live f–I preview |
| Pacing | Instant check, skip to next event, default-speed tuning |
| Measure | Local telemetry, novice harness extended to micro-puzzles, persona critique gates |
| Deferred from 0.2 | Weight-spread control (fixes reach), Renshaw control measured on the player's circuit, per-task sensor weights, plastic synapses in the editor |

The suggested order is:
1. Concept graph and micro-puzzles.
2. Hint ladder and modes.
3. Audio and feel.
4. Histograms.
5. Diagnosis.

Content and the engine can run in parallel with the UI.

## Sources
- Koster: [Theory of Fun, 10 years later (Koster)](https://www.raphkoster.com/wp-content/uploads/2026/07/Theory-of-Fun-10-Years-Later.pdf), [Game studies wiki summary](https://game-studies.fandom.com/wiki/A_Theory_of_Fun_for_Game_Design?oldid=4074)
- Gee: [Wikipedia](https://en.wikipedia.org/wiki/What_Video_Games_Have_to_Teach_Us_About_Learning_and_Literacy), [the 36 principles (GMU)](https://mason.gmu.edu/~lsmithg/jamespaulgee2), [Draper's notes](https://www.psy.gla.ac.uk/~steve/best/gee.html)
- Schell: [O'Reilly listing](https://oreilly.com/library/view/the-art-of/9781466598645), [Schell GDC09 slides](https://web.cs.wpi.edu/~rich/courses/imgd4000-d09/lectures/schell-GDC09.pdf)
- Adams & Dormans: [Peachpit article](https://peachpit.com/articles/article.aspx?p=1925649), [sample chapter](https://ptgmedia.pearsoncmg.com/imprint_downloads/peachpit/peachpit/samplechapters/0321820274/0321820274_gamemechanics_ch05section.pdf), [Dormans, integrating emergence and progression (DiGRA)](https://dl.digra.org/index.php/dl/article/download/593/593/590)
- Sylvester: [O'Reilly](https://www.oreilly.com/library/view/designing-games/9781449338015/), [GBGames review](https://www.gbgames.com/2014/05/07/book-review-designing-games-by-tynan-sylvester/)
- Zachtronics: [Open-ended puzzle design, GDC 2019](https://gdcvault.com/play/1025715/Open-Ended-Puzzle-Design-at), [Game Developer: Zachtronics' approach](https://gamedeveloper.com/design/video-zachtronics-approach-to-open-ended-puzzle-design), [Opus Magnum and perfect solutions (PC Gamer)](https://www.pcgamer.com/au/perfectly-solving-opus-magnums-puzzles-is-impossible-but-thats-ok)
- From memory, not checked this session: Csikszentmihalyi (*Flow*), the MDA paper, Hodent (*The Gamer's Brain*), Swink (*Game Feel*), Fullerton (*Game Design Workshop*).
