# Atlas research, set A: data, statistics and the compass probe

Research for v0.6 "Atlas", iteration A1 (see `docs/ROADMAP-CONNECTOME.md`). The tools under `tools/connectome/` refer to this note.

Conventions
- **[V]**: the claim was checked against the PubMed record (title, abstract) of the cited paper, 2026-11-23. Abstract-level only.
- **[run]**: a number printed by a script in this repository, quoted as printed.
- A claim that none of the eight verified papers supports is not cited. It is listed in the last section.
- The eight verified citations are in `research/atlas-a-citations.json`.

## 1. Data sources

### Fly: hemibrain v1.2

| Item | Value |
|---|---|
| Release | hemibrain v1.2 (neuPrint dataset `hemibrain:v1.2`), "traced adjacencies" export |
| Paper | Scheffer et al. 2020 [V]: electron-microscopy connectome of a large part of the adult fly central brain, with cell types and chemical-synapse circuits |
| File | `exported-traced-adjacencies-v1.2.tar.gz`, 45,872,577 bytes, downloaded 2026-11-22 to a cache outside the repo |
| Tables used | `traced-neurons.csv` (body id, type, instance) and `traced-total-connections.csv` (pair, total synapse count) |
| Whole file | 21739 neurons (traced, non-cropped) |
| Committed | `data/connectome/hemibrain/compass.json`, `mushroom_body.json`, and `PROVENANCE.md`, all written by `tools/connectome/hemibrain.py` |
| Terms | stated by the data's owner for the dataset as a whole; attribution text is in `PROVENANCE.md` |

The compass table holds every neuron of five types (`EPG`, `PEN_a(PEN1)`, `PEN_b(PEN2)`, `PEG`, `Delta7`) and every connection among them, with no threshold. The ring position of each neuron (0 to 15 along the protocerebral bridge) is derived by the script from the glomerulus in the instance name. It is not a field of the data. One check on it: every Delta7 instance lists glomeruli exactly eight positions apart, which is half a ring.

The mushroom-body table holds the right mushroom body only. To keep the file near 1 MB, a connection with fewer than 5 synapses is dropped, except PN to KC, KC to APL and APL to KC, which keep every count. Any statistic over other pairs (KC to KC, KC to MBON, dopamine neuron to KC) describes the strong part of the wiring only. `PROVENANCE.md` lists what else is dropped.

What the central-complex and mushroom-body papers add to the table's use:
- Hulse et al. 2021 [V] give a complete electron-microscopy connectome of the central complex, with network motifs that likely extract head direction and maintain it with attractor dynamics.
- Li et al. 2020 [V] give the mushroom-body connectome, including the MB output neurons and the dopaminergic neurons that modulate the transfer to them.

### Worm: C. elegans hermaphrodite

`tools/connectome/worm_stats.py` reads the hermaphrodite connectome through the `cect` package (version 0.3.5, per the script's docstring), through its `Cook2019HermReader`. The data are the whole-animal connectome of both sexes assembled by Cook and colleagues in 2019; that paper is not among the eight verified papers, so the roadmap's Cook 2019 entry stays unverified (see the last section). Chemical edges are directed. Gap junctions are symmetric. An "edge" is a pair of cells with at least one synapse and a "count" is a number of synapses. Nothing is written to the repository.

Varshney et al. 2011 [V] assembled self-consistent gap-junction and chemical-synapse networks of the hermaphrodite and computed degree distributions, synaptic multiplicities, small-world properties, central neurons and network motifs. They are the reference for the kinds of statistic the worm script reports.

## 2. Statistics

All numbers in this section are from `python3 tools/connectome/atlas_stats.py` (reads the committed JSON tables; deterministic except the shuffle, which is seeded).

### Compass

Neurons by type: Delta7 42, EPG 46, PEG 18, PEN_a(PEN1) 20, PEN_b(PEN2) 22. Connections 9487.

Connected pairs and synapses by type pair [run]:

| Pre | Post | Connected pairs | Synapses |
|---|---|---|---|
| Delta7 | Delta7 | 1719 | 24615 |
| Delta7 | EPG | 987 | 6664 |
| Delta7 | PEG | 393 | 6123 |
| Delta7 | PEN_a(PEN1) | 298 | 2754 |
| Delta7 | PEN_b(PEN2) | 336 | 9800 |
| EPG | Delta7 | 1714 | 17286 |
| EPG | EPG | 750 | 7980 |
| EPG | PEG | 280 | 5765 |
| EPG | PEN_a(PEN1) | 373 | 5128 |
| EPG | PEN_b(PEN2) | 283 | 5157 |
| PEG | Delta7 | 20 | 20 |
| PEG | EPG | 217 | 499 |
| PEG | PEG | 67 | 211 |
| PEG | PEN_a(PEN1) | 42 | 81 |
| PEG | PEN_b(PEN2) | 96 | 1309 |
| PEN_a(PEN1) | Delta7 | 12 | 12 |
| PEN_a(PEN1) | EPG | 431 | 13486 |
| PEN_a(PEN1) | PEG | 49 | 79 |
| PEN_a(PEN1) | PEN_a(PEN1) | 248 | 3177 |
| PEN_a(PEN1) | PEN_b(PEN2) | 263 | 1989 |
| PEN_b(PEN2) | Delta7 | 18 | 19 |
| PEN_b(PEN2) | EPG | 319 | 8417 |
| PEN_b(PEN2) | PEG | 53 | 134 |
| PEN_b(PEN2) | PEN_a(PEN1) | 253 | 1951 |
| PEN_b(PEN2) | PEN_b(PEN2) | 266 | 4270 |

Delta7 is the largest source of synapses (24615 onto itself, 6664 onto EPG). PEG and PEN send few synapses to Delta7 (at most 20 per type pair).

Ring-position offset, post minus pre, in slots of 22.5 degrees, as a synapse-weighted percentage [run]. Same side means both cells are in the same half of the bridge.

| Projection | Side | Largest offsets (percent of synapses) |
|---|---|---|
| EPG to PEN_a | same | 0: 25.2, +1: 13.7, -1: 12.1, +2: 4.9, -2: 4.3 |
| EPG to PEN_a | opposite | -6: 11.3, +6: 11.2, -7: 5.2, +7: 5.3 |
| EPG to PEN_b | same | 0: 28.4, +1: 16.5, -1: 16.4 |
| EPG to PEN_b | opposite | -6: 14.6, +6: 12.5 |
| PEN_a to EPG | same | -1: 14.9, +1: 13.6, -2: 7.3, +2: 6.5, 0: 4.0 |
| PEN_a to EPG | opposite | +6: 12.7, -6: 12.4, -7: 8.1, +7: 7.9 |
| PEN_b to EPG | same | +1: 20.0, -1: 19.3, -2: 4.8, +2: 4.8 |
| PEN_b to EPG | opposite | +6: 15.8, -6: 15.3, +7: 6.6, -7: 6.1 |
| EPG to PEG | same | 0: 78.0 |
| PEG to EPG | same | 0: 39.7 |

Raw glomerulus difference (EPG minus PEN glomerulus number), PEN_a to EPG, same side [run]: the largest counts are -1 (L 1834, R 2005), then -2 (L 881, R 978), then 0 (L 221, R 323). For PEN_b to EPG it is -1 (L 1681, R 1626) then -2 (L 404, R 403).

Delta7 [run]:
- Delta7 to EPG synapses by whether the EPG sits in a glomerulus the Delta7 covers: 4678 in one of the Delta7's own glomeruli, 1986 in another.
- Share of Delta7 to EPG synapses by ring-position distance from the nearest covered position (0 to 8): 0: 70.2, 1: 15.8, 2: 3.2, 3: 6.7, 4: 4.1 (percent).
- Delta7 receives from EPG (17286 synapses) far more than from PEG (20), PEN_a (12) or PEN_b (19).

Mean partners and synapses per neuron [run]:

| Type | In: partners | In: synapses | Out: partners | Out: synapses |
|---|---|---|---|---|
| EPG | 58.8 | 805.3 | 73.9 | 898.2 |
| PEN_a(PEN1) | 60.7 | 654.5 | 50.1 | 937.1 |
| PEN_b(PEN2) | 56.5 | 1023.9 | 41.3 | 672.3 |
| PEG | 46.8 | 684.0 | 24.6 | 117.8 |
| Delta7 | 82.9 | 998.9 | 88.9 | 1189.4 |

### Mushroom body

Neurons by role: APL 1, DAN 159, KC 1927, MBON 43, PN 97. Connections kept 60767 of 473731.

Kenyon cells by type [run]: KCa'b'-ap1 91, KCa'b'-ap2 127, KCa'b'-m 119, KCab-c 252, KCab-m 354, KCab-p 60, KCab-s 223, KCg-d 99, KCg-m 590, KCg-s1 1, KCg-s2 1, KCg-s3 1, KCg-s4 1, KCg-t 8.

Projection-neuron input to Kenyon cells [run]:
- PN to KC connections 10413, synapses 162297.
- KC with no PN input: 179 of 1927.
- PN inputs per KC: mean 5.40, median 5.0, sd 2.96, max 15.
- Histogram (inputs: KCs): 0: 179, 1: 59, 2: 54, 3: 149, 4: 259, 5: 290, 6: 275, 7: 225, 8: 159, 9: 113, 10: 87, 11: 35, 12: 24, 13: 12, 14: 5, 15: 2.

| KC type | n | Mean PN inputs | With no PN input |
|---|---|---|---|
| KCa'b'-ap1 | 91 | 2.24 | 10 |
| KCa'b'-ap2 | 127 | 5.12 | 0 |
| KCa'b'-m | 119 | 5.15 | 0 |
| KCab-c | 252 | 4.51 | 27 |
| KCab-m | 354 | 4.55 | 2 |
| KCab-p | 60 | 0.17 | 51 |
| KCab-s | 223 | 5.97 | 1 |
| KCg-d | 99 | 0.17 | 85 |
| KCg-m | 590 | 8.15 | 0 |
| KCg-s1 | 1 | 0.00 | 1 |
| KCg-s2 | 1 | 0.00 | 1 |
| KCg-s3 | 1 | 0.00 | 1 |
| KCg-s4 | 1 | 1.00 | 0 |
| KCg-t | 8 | 3.75 | 0 |

The mean of 5.40 inputs per KC agrees in size with the claw count the script quotes for KCg cells (about 5 to 6, from the literature, not computed here). The table also shows that the mean differs strongly by type: KCg-m has 8.15, while KCab-p and KCg-d have 0.17 (most of their cells receive no PN synapse in this table).

Test against a degree-preserving shuffle [run]: pairs of PNs from one glomerulus that feed the same KC, real 519 (of 31350 pairs); stub shuffle (200 draws, seed 1): mean 253.9, sd 14.4, z = 18.44. The shuffle keeps each KC's number of PN synapse-partners and each PN's total number of KC partners. Collisions inside a KC are merged, so in-degrees can fall slightly. The real count is far above the shuffle, so the wiring is not random in this sense: same-glomerulus PNs converge on the same KC more often than the shuffle predicts. Caron et al. 2013 [V] found that each Kenyon cell integrates input from an apparently random combination of glomeruli, with no discernible organisation by odour tuning, anatomy or origin, and that Kenyon-cell classes do not preferentially integrate specific combinations. This z-score tests a different property (repeated use of one glomerulus within a cell) from the one Caron et al. examined; the two are not in conflict, and the level `a_random` has to say which question it asks.

Top glomeruli by share of PN to KC synapses [run] (percent):

| KC class | PN to KC synapses | Top five |
|---|---|---|
| KCa'b' | 20058 | DP1m 7.1, DM1 6.8, DP1l 4.8, VL2p 4.1, DM2 3.7 |
| KCab | 57030 | DP1m 7.0, DM1 5.9, DM2 5.6, DM4 5.2, DM6 4.2 |
| KCg | 85209 | DC3 4.3, VL2p 3.9, VM5d 3.8, DC1 3.8, VL2a 3.7 |

APL feedback [run]: KC to APL synapses, mean 52.9 per KC (1927 KCs); APL to KC synapses, mean 49.9 per KC.

| KC class | n | KC to APL mean | APL to KC mean |
|---|---|---|---|
| KCg | 701 | 61.3 | 64.5 |
| KCab | 889 | 44.0 | 35.1 |
| KCa'b' | 337 | 58.7 | 58.9 |

The loop is reciprocal and broad: every KC has synapses with APL in both directions in this table. Lin et al. 2014 [V] showed in the fly that Kenyon cells activate APL and APL inhibits Kenyon cells, that APL is GABAergic, that this negative feedback keeps Kenyon-cell odour responses sparse and decorrelated, and that disrupting it impaired the discrimination of similar odours.

Output side [run]: 35 MBON types (43 neurons); 159 DANs in 31 types. KC to MBON synapses (pairs with at least 5 synapses), top five by MBON type: MBON01 26724, MBON07 25498, MBON14 22538, MBON11 22144, MBON09 15037. DAN to KC synapses (pairs with at least 5) by family: PAM0 6162, PAM1 933, PPL1 12662. KCs per MBON type (at least 5 synapses): MBON11 1479, MBON22 1218, MBON01 920, MBON02 894, MBON07 888, MBON12 882, MBON14 878, MBON18 807.

### Worm: numbers pending

The `cect` package is not importable in the environment that wrote this note (`ModuleNotFoundError: No module named 'cect'`), and nothing was installed. The worm numbers are pending. `tools/connectome/worm_stats.py` computes:

- cell count, number of directed chemical edges, total chemical synapses, gap-junction entries and contacts;
- a split of cells by name into motor-neuron series (DA, DB, DD, VA, VB, VD, AS, VC) and others, and which of the touch cells ALML, ALMR, AVM, PLML, PLMR are present;
- for AVA, AVB, AVD, PVC and AVE (left and right), the number of chemical out-partners and in-partners;
- the ten cells with the highest chemical degree (in plus out partners);
- the number of A-type (DA, VA) and B-type (DB, VB) motor neurons, and the shortest chemical path (in hops) from ALML, AVM and PLML to each;
- the single cells whose removal lengthens the shortest ALM and AVM path to the A-type neurons;
- a census of feed-forward loops in the chemical network, against a shuffle (see the script for the null model used).

To fill this section: create the virtual environment named in the script's docstring on a GitHub runner, run the script, and paste its output here unedited.

## 3. What the tables do not give, and what the compass probe assumes

The tables give who connects to whom and how many synapses. They do not give the following, so `tools/connectome/compass_circuit.py` assumes each as a parameter.

| Missing | Assumption in `compass_circuit.py` |
|---|---|
| Sign of each cell type | EPG, PEN_a, PEN_b, PEG excitatory; Delta7 inhibitory (`sign_*` parameters) |
| Weight of one synapse | `weight = sign * synapse_count * scale`, one scale per presynaptic type (pA per synapse), default 0.3; optionally one per type pair (`w_<pre>_<post>`) |
| Cell model and constants | `iaf_psc_alpha`; `tau_syn_ex` and `tau_syn_in` 5 ms; delay 2 ms |
| Background input | one current `I_e` (default 300 pA), a stand-in for everything the cells receive that the table does not hold; optionally per population (`I_e_<pop>`) |
| Sensory input | cue at ring position k: Poisson input onto the EPG cells at that position (weight 400 pA); two turn sensors as constant currents onto the PEN cells of one half of the bridge each (weight 3 pA per unit) |
| Readout | the 46 EPG cells as a population vector in ring order; the engine places neuron k of 46 at 360 k / 46 degrees but ring position r is at 22.5 r degrees, so the two differ by up to about 10 degrees |

The signs are literature assumptions, not part of the table. This is the weakest point of the probe, and the note cannot yet back it with a verified source (see "Open: needs a source"). What can be said from the verified papers:
- Turner-Evans et al. 2020 [V] combined electron-microscopy reconstruction with RNA profiling of identified cell types in the head-direction system, and report functional support for the roles of individual excitatory or inhibitory circuit elements. The abstract does not name the transmitter or sign of E-PG, P-EN, P-EG or Delta7, so the per-type signs above are not attributed to it here.
- Franconville et al. 2018 [V] tested 70 presynaptic-to-postsynaptic cell-type pairs of the central complex with optogenetic stimulation, calcium imaging and pharmacology, and found the connectivity sparser than anticipated from light microscopy. The sign of a type pair is something a functional measurement gives and a wiring table does not.

The **shift direction** is the second thing the table cannot state. The script assumes that the left half of the bridge pushes the bump towards higher ring positions, so `turn_right` drives the PEN cells of the left half and `turn_left` those of the right half. What the table supports is the geometry: in the raw glomerulus difference above, a PEN cell contacts EPG cells one to two glomeruli away (EPG glomerulus number minus PEN glomerulus number is mostly -1 and -2, for both halves and for PEN_a and PEN_b). With the ring positions of `PROVENANCE.md` (left glomerulus g at 8 - g, right glomerulus g at 7 + g), a lower glomerulus number is a higher ring position on the left and a lower one on the right. So PEN output from the left half lands on higher ring positions and from the right half on lower ones. That is a derivation from the table and the script's ring order, which is itself derived. That a given half moves the bump in the direction of a given turn is a functional fact; the note has no verified source for it. Hulse et al. 2021 [V] describe network motifs that likely update head direction, and Turner-Evans et al. 2020 [V] report motifs hypothesised to update it when the animal turns. Neither abstract states the direction.

Also not in the tables: the ellipsoid-body wedge of each cell (the ring order is derived from the bridge), the transmitter of any cell, synapse sizes, time constants and electrical coupling.

## 4. Feasibility probe of the compass on real wiring (iteration A1)

Question: does the measured compass wiring hold a bump when the circuit is given one sign per cell type and one gain per type?

What was run, and what it found:

| Search | Parameters drawn | Draws | Trials per draw | Result |
|---|---|---|---|---|
| 1 (`compass_search.py`) | one synapse scale per presynaptic cell type, plus background current, synaptic time constants and turn gain | 2,000 | 2 (cue at ring positions 2 and 10), circuit seed 7 | no draw held a bump at both cues; no draw held it at one cue |
| 2 (`pairs=1`) | one scale per type pair (19 pairs) and a background current per population | started, stopped before it finished | | no result |

The second search was added to the script and started. It was stopped before it finished, so it has no result and no partial result is quoted.

**Conclusion.** Whether measured compass wiring holds a bump with per-type signs and gains is open. The first search found nothing; that is a statement about one parameterisation (one scale per presynaptic type), not about the wiring. The second, larger parameterisation has not been run to the end.

**Risk.** This is the main risk of iteration A3 (real wiring holds a bump with the reference's signs and gains on seeds 101 to 105). If no per-type or per-pair assignment works, the compass chapter needs a different design (for example, free parameters that are not per cell type), and A3 cannot be designed until this is known.

**Next step.** Run the second search to completion on a GitHub runner, not a desktop, before A3 is designed. Report the number of draws, which trials passed and the best parameters, then update this section.

## 5. Sources

Only the eight verified papers are cited. Each is quoted in the form its `match` expression in `research/atlas-a-citations.json` expects.

| Cited as | Used for | Supports |
|---|---|---|
| Scheffer et al. 2020 | hemibrain v1.2 | the connectome of the central brain, cell types, chemical-synapse circuits, public data |
| Hulse et al. 2021 | central-complex connectome | motifs for head direction extraction and attractor dynamics |
| Turner-Evans et al. 2020 | ring attractor | the Drosophila head-direction network contains the core components of a ring attractor; motifs that maintain the representation in darkness, update it on turning and tether it to cues; excitatory and inhibitory elements shape activity |
| Franconville et al. 2018 | functional connectome | 70 cell-type pairs tested functionally; a wiring table alone leaves sign to measurement |
| Li et al. 2020 | mushroom-body connectome | MBON, DAN and APL context for the table |
| Caron et al. 2013 | random convergence | each Kenyon cell integrates an apparently random combination of glomeruli |
| Lin et al. 2014 | APL feedback | Kenyon cells activate the GABAergic APL, APL inhibits Kenyon cells, which keeps the code sparse and decorrelated |
| Varshney et al. 2011 | worm wiring | self-consistent hermaphrodite networks; degree, multiplicity, small-world and motif statistics |

The record of each paper in the PubMed database was read on 2026-11-23 (PMIDs 32880371, 34696823, 32916090, 30124430, 33315010, 23615618, 24561998, 21304930). Only abstracts were read.

## Open: needs a source

- Transmitter and sign of E-PG, P-EN, P-EG and Delta7 (the script assumes the first three excitatory and Delta7 inhibitory): the abstracts of the eight papers do not state them; the full text of Turner-Evans et al. 2020 may.
- Which half of the bridge moves the bump in which direction for a given turn: no abstract states it.
- Cook et al. 2019 as the source of the worm connectome read through `cect`: not among the eight verified papers.
- The `cect` package version and the reader named in the worm script (0.3.5, `Cook2019HermReader`) were taken from the script, not checked against the package.
- Worm statistics: pending until `worm_stats.py` runs.
- Result of the second compass search (19 type-pair scales, background current per population): not run to the end.
- That the derived ring order (bridge position as the ring) corresponds to the physiological ring: the table supports only the half-ring spacing of Delta7 glomeruli.
