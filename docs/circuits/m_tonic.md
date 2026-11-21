# m_tonic: tonic drive (bias current)

Level: micro-puzzle, Invertebrates chapter, after `m_adaptation`. Concept `tonic_drive` (new), uses `inhibition`.

World: 4 s. The dc sensor `cue` is 0 except on [1.7 s, 2.7 s) where it is 2.0 (times the sensor weight). Motor `spikes` reads the cell or cells wired to it.

Pass: rest rate in [0.6 s, 1.6 s] between 5 and 60 spikes/s (Standard; Guided from 3.5, Researcher from 6); the rate in [1.95 s, 2.7 s] at most a quarter of the rest rate (Guided 0.35); the rate in [3.1 s, 4 s] at least 0.6 of the rest rate. Structure: a cell on the way to `spikes` has I_e > 0, and the cue reaches `spikes` through a net inhibitory path.

Reference (`circuits/m_tonic.json`, made by `circuits/tools/gen_tonic.py`): N2 (iaf_psc_alpha) driven by `cue` at 300 pA/unit, N2 > N1 at -1000 pA, N1 (iaf_psc_alpha, I_e 400 pA) read on `spikes`. The plain cell's rheobase is about 375 pA, so 400 pA gives about 35 spikes/s; 450 pA already gives 50.

The lab default (a plain cell, no bias) is silent and the verdict says why. Cheap rows: `engine/tests/cheap_rows_a.inc` (bias removed, bias far over or under threshold, link cut, link set excitatory, link far too weak, bias on the wrong cell). Citation: Raman IM, Bean BP (1999) J Neurosci 19:1663-1674, doi:10.1523/JNEUROSCI.19-05-01663.1999 (catalog/citations.json, verified against the PubMed record).
