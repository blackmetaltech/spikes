---
title: Contributing and the honesty contract
section: Under the hood
order: 86
summary: How to report a bug, what a good contribution looks like, and the rules every level and every claim in Spike must follow.
---

# Contributing and the honesty contract

Spike is a game that teaches by being right. That only works if the puzzles cannot be passed by tricks and the biology it quotes is real. This page says how to help, and which rules a contribution has to meet.

## Report a bug or a confusing level

Open an issue at [github.com/ixaxaar/spikes/issues](https://github.com/ixaxaar/spikes/issues). The project asks for two kinds of report: what broke, and what confused you. Confusion is a bug. If a brief left you guessing or a verdict misled you, say so.

A useful report has:

1. The level id or name, and the mode (Guided, Standard or Researcher). See [modes](modes.html).
2. The circuit file. Save it from the lab (More, then Save, once that opens) and attach it. See [a circuit is data](circuit-is-data.html).
3. What you expected, and what you got.
4. The verdict text, if it was a Check or a Test x5.
5. Your platform and the game version shown on the title screen.
6. The game log, `logs/spike.log` in the game's data folder (Settings, Game tab, Open log folder). It keeps the last three starts, and records damaged or newer save files the game noticed.

If you want to help the maintainers learn where players get stuck, Settings has an opt-in play log (Game tab, "Record my play"). It is off by default and keeps data on your own device only: time per level, runs, hint tiers, quits, and edits before your first pass. You can export it as JSON and attach it to a report. Nothing is ever sent for you.

## Before you send code

1. Build and run the tests (see [building from source](building-from-source.html)).
2. Read [architecture](architecture.html) and, for a level, [make it yours](make-it-yours.html).
3. If your change touches the UI, update the interaction graph (below).
4. Keep your change small, and say in the description what it changes for a player.

Pushes that only touch Markdown, `docs/`, `research/` or the website (`site/`, `tools/site/`) do not run CI. The website and wiki have their own workflow, which fails on a broken link, anchor or image; check a wiki change locally with `python3 tools/site/build.py --check`. See [building from source](building-from-source.html#continuous-integration).

## The honesty contract

These five rules came out of a harsh review of version 0.3 (`docs/REVIEW-HARSH.md`) and are recorded in `docs/REQ-0.4.md`. Every level must follow them, and tests enforce most of them.

### 1. Mechanism, not just output

A level checks that the circuit uses the mechanism the level teaches, through the circuit's structure, and the verdict says which part is missing. For example, a level that teaches convergence checks that several sensory cells really converge onto the same target, and a circuit without that fails whatever its weights. A cheap trick that happens to hit the numbers does not pass.

### 2. No numbers in verdicts or hints

Verdicts describe what the circuit did and which way to change it ("too weak to reach threshold"), never the answer ("375 pA"). In the current code this applies to verdicts and to all three hint tiers of every level, pass or fail. A test scans every task for digits. Only the recipe, which Guided mode shows in the brief and Standard shows behind a Show recipe button, carries numbers. See [stars, par and the hint ladder](stars-and-hints.html).

> **Note:** The original requirements text says "tier-1/2 hints". The 0.5 playtest fixes extended the rule to tier 3 as well, and the changelog says so. The code is what counts.

### 3. A table of circuits that must fail

Every level has rows in `engine/tests/cheap_rows_*.inc` (six files, `a` to `f`, each its own ctest entry): an empty bench, a single cell, a tonic cell with all weights 0, the reference with every weight 0 or extreme, the reference with the taught mechanism cut out, and any cheap circuit a reviewer found. Each must fail on seeds 1 and 2 (in the quick episode that Check uses), and say something. A broken harness is caught because the reference circuit must pass the same harness. Diagnosis cases test that every wrong and unknown answer fails. See [make it yours](make-it-yours.html).

### 4. Honest novice tests

A scripted novice (`tasks_novice_test`) builds the circuits a new player tries from the brief, in the order they try them, and uses only the brief, the verdicts and the lab's own defaults. It may not use recipes, tier-3 hints or the reference. Every run must either pass or say, in words, which group never fires. Levels it cannot solve are reported, not hidden. A second test, `tasks_honest_test`, checks the other direction: valid circuits that are not the reference must not be rejected, and a quick pass must hold in the full episode.

### 5. Real data, honest labels

- Every citation is checked against PubMed or Crossref, with a DOI stored next to it.
- Wrong citations are fixed or removed. `docs/CITATIONS.md` lists the corrections that were made.
- Facts that do not fit their level are rewritten.
- Generated histograms are labelled "generated variants of the reference, not players".
- There is a What is real page in the game and in the docs: [what is real](what-is-real.html).

## What the labels mean

`docs/WHAT-IS-REAL.md` sorts every part of the game under five labels. A contribution must say which one applies.

| Label | Meaning |
|---|---|
| NEST | The equations are NEST's own. |
| NESTML re-fit | A custom cell with structure from a paper and hand-adjusted numbers. Not validated. |
| Tuned in silico | A number picked by sweeping until the level passed. A property of the game, not a measurement. |
| Illustrative | A coefficient picked so a body looks plausible. |
| Generated | Produced by a script (the histograms). |

Do not call a tuned number a measurement. Do not call a body model clinical.

## Citations

The registry is `catalog/citations.json`. Every reference the game shows (notebook notes, concept cards, lesion hints, diagnosis explanations) or the docs make must be in it.

```sh
python3 tools/citations/check.py               # offline: registry sanity, missing references, known wrong strings
python3 tools/citations/check.py --online      # ask Crossref about every DOI
python3 tools/citations/check.py --online --fill   # look up DOIs for entries that lack one
python3 tools/citations/check.py --write-md    # regenerate docs/CITATIONS.md
```

Each entry has a status. **Verified** means the record was read from PubMed. **Verified-locus** means PubMed has a record at exactly that journal, year, volume, first page and first author. **Unverified** means it could not be checked (books and older papers); they are kept because the work is real and classic, but are marked. The registry at the last count held 194 entries, of which 34 are unverified. The citation file is generated; do not edit it by hand.

## The interaction-graph rule

`godot/tests/interaction_graph.json` lists every screen, tab, panel, modal and toast, and every control, with the actions that move between them. A test starts the real game headless, presses each control with a real mouse click, and fails if a control is covered, dead, or not listed. So:

- Any UI change updates the JSON in the same commit.
- Run `tools/graph/render.py` to regenerate the diagram in `docs/INTERACTION-GRAPH.md`, and commit it.
- Run `scripts/ui_graph_test.sh`.
- Name new controls (`Kit.btn` names them from their label; `Kit.ident` names anything else).

This is also the source for [controls](controls.html).

## The look

The visual identity is "Cajal Plate" (`docs/IDENTITY.md`): India ink and Golgi stain on aged paper, sienna for excitation and Prussian blue for inhibition, with a second cue so colour is never the only signal. Type: Fraunces for display, Atkinson Hyperlegible for text and IBM Plex Mono for readouts, all open licences and vendored. Keep text at 14 px or larger on screen; the smoke test fails below that. Respect the Reduced motion setting.

## Licences and credits

The Credits screen (title screen, or Settings) lists the engine, fonts and sources, including the connectome data behind the worm touch level (OpenWorm's ConnectomeToolbox, with the terms of the original paper). Read it before you add a data source.

## Related pages

[Make it yours](make-it-yours.html), [building from source](building-from-source.html), [what is real](what-is-real.html), [tips and common mistakes](tips.html), [the changelog](changelog.html).
