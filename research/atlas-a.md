# Atlas research, set A: data, statistics and the compass probe

Research for v0.6 "Atlas", iteration A1 (see `docs/ROADMAP-CONNECTOME.md`). The tools under `tools/connectome/` refer to this note.

Conventions
- **[V]**: the claim was checked against the PubMed record (title, abstract) of the cited paper, 2026-11-23. Abstract-level only.
- **[run]**: a number printed by a script in this repository, quoted as printed.
- A claim that none of the eleven verified papers supports is not cited. It is listed in the last section.
- The eleven verified citations are in `research/atlas-a-citations.json`.

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

The data are the whole-animal connectome of both sexes of Cook et al. 2019 [V], hermaphrodite adjacency matrices (SI 5, corrected July 2020), read through the `cect` package, version 0.3.5, with its `Cook2019Herm` reader. `tools/connectome/worm_table.py` writes two committed tables from it, `data/connectome/worm/touch.json` (the touch-withdrawal excerpt, 54 cells, every connection among them) and `data/connectome/worm/whole.json` (all 302 neurons the reader returns, every chemical and electrical connection); `data/connectome/worm/PROVENANCE.md` records the file, its hash and what was kept. `tools/connectome/worm_report.py` reads only those tables. Chemical connections are directed, gap junctions are symmetric, an edge is a pair of cells with at least one synapse and a count is a number of synapses. The older `tools/connectome/worm_stats.py` reads `cect` directly and is superseded by the report for these numbers.

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

### Worm: wiring tables and numbers

Printed by `python3 tools/connectome/worm_report.py` (about 8 seconds, pure Python, reads `touch.json` and `whole.json`), pasted as printed [run]. Class names are derived by a rule in `worm_table.py` (trailing digits dropped, then a trailing L or R when the other side exists), not a field of the data. Reading the tables: a sensor-to-motor connection is a direct one-hop edge in the class matrix (AVM onto DA, VA and VB; PLM onto VA), so the one-hop rows at low thresholds are not a mistake of the path search.

#### a. Class-level matrices of touch.json (rows presynaptic, columns postsynaptic; '.' = none)

Cells: 54. Chemical connections between cells: 308 (1540 synapses). Electrical couplings: 161 (count total 846, each coupling listed once).
Class cell counts: ALM 2, AVM 1, PLM 2, AVA 2, AVB 2, AVD 2, AVE 2, PVC 2, DA 9, VA 12, DB 7, VB 11

Chemical synapse counts summed over cell pairs:

| pre \ post | ALM | AVM | PLM | AVA | AVB | AVD | AVE | PVC | DA | VA | DB | VB |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| ALM | 1 | . | . | . | . | 3 | 3 | 25 | . | . | . | . |
| AVM | . | . | . | . | 22 | 2 | . | 27 | 2 | 1 | . | 1 |
| PLM | . | . | . | 17 | . | 18 | . | . | . | 6 | . | . |
| AVA | . | . | . | 19 | 9 | 14 | 8 | 77 | 137 | 145 | 10 | . |
| AVB | . | . | . | 47 | 8 | 7 | 4 | 2 | 6 | 8 | . | 3 |
| AVD | . | . | . | 167 | 1 | 8 | 7 | 3 | 42 | 16 | 2 | . |
| AVE | 1 | . | . | 67 | . | 13 | 3 | 3 | 44 | 26 | 3 | . |
| PVC | . | . | . | 27 | 69 | 40 | 21 | 16 | 8 | . | 39 | 20 |
| DA | . | . | . | . | . | . | . | . | 8 | 2 | 12 | . |
| VA | . | . | . | 6 | . | 1 | 1 | 7 | 27 | 14 | 10 | 40 |
| DB | . | . | . | . | . | . | . | . | 66 | . | . | . |
| VB | . | . | . | . | . | . | 1 | 1 | . | 63 | . | 3 |

Electrical counts (the matrix is symmetric; a coupling inside a class, such as AVAL-AVAR, appears twice in its diagonal cell):

| pre \ post | ALM | AVM | PLM | AVA | AVB | AVD | AVE | PVC | DA | VA | DB | VB |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| ALM | . | 23 | . | . | . | 2 | . | . | . | . | . | . |
| AVM | 23 | . | . | . | . | 8 | . | . | . | . | . | . |
| PLM | . | . | . | 6 | . | 5 | . | 12 | . | . | . | . |
| AVA | . | . | 6 | 36 | 3 | 33 | 4 | 52 | 81 | 113 | . | 2 |
| AVB | . | . | . | 3 | 36 | . | . | . | . | . | 69 | 87 |
| AVD | 2 | 8 | 5 | 33 | . | 12 | . | 2 | . | 1 | . | . |
| AVE | . | . | . | 4 | . | . | 10 | . | 1 | . | . | . |
| PVC | . | . | 12 | 52 | . | 2 | . | 134 | 25 | 32 | 10 | 9 |
| DA | . | . | . | 81 | . | . | 1 | 25 | 40 | 27 | 1 | 1 |
| VA | . | . | . | 113 | . | 1 | . | 32 | 27 | 6 | 1 | 14 |
| DB | . | . | . | . | 69 | . | . | 10 | 1 | 1 | 70 | 9 |
| VB | . | . | . | 2 | 87 | . | . | 9 | 1 | 14 | 9 | 82 |

#### b. Shortest class-level paths at synapse thresholds

A class-level edge is kept when its summed count (over the cell pairs of the two classes, self-connections within a cell excluded) is at least t. 'hops' = fewest edges from any source class to any target class. 'on a shortest path' = interneuron classes with distance from source plus distance to target equal to the hop count. 'cut by' = interneuron classes whose removal alone leaves no path.

**chemical only**

| t | edges | anterior touch (ALM, AVM) to A-type: hops | interneurons on a shortest path | cut by | posterior touch (PLM) to B-type: hops | interneurons on a shortest path | cut by | ALM, AVM to B: hops | PLM to A: hops |
|---:|---:|---|---|---|---|---|---|---|---|
| 1 | 60 | 1 | none | none | 2 | AVA, AVD | none | 1 | 1 |
| 2 | 52 | 1 | none | none | 2 | AVA, AVD | none | 2 | 1 |
| 3 | 47 | 2 | AVB, AVD, AVE, PVC | none | 2 | AVA | none | 2 | 1 |
| 5 | 40 | 2 | AVB, PVC | none | 2 | AVA | none | 2 | 1 |
| 10 | 30 | 3 | AVA, AVB, AVD, AVE, PVC | none | 2 | AVA | none | 2 | 2 |

**chemical plus electrical**

| t | edges | anterior touch (ALM, AVM) to A-type: hops | interneurons on a shortest path | cut by | posterior touch (PLM) to B-type: hops | interneurons on a shortest path | cut by | ALM, AVM to B: hops | PLM to A: hops |
|---:|---:|---|---|---|---|---|---|---|---|
| 1 | 83 | 1 | none | none | 2 | AVA, AVD, PVC | none | 1 | 1 |
| 2 | 73 | 1 | none | none | 2 | AVA, AVD, PVC | none | 2 | 1 |
| 3 | 66 | 2 | AVB, AVD, AVE, PVC | none | 2 | AVA, PVC | none | 2 | 1 |
| 5 | 62 | 2 | AVB, AVD, PVC | none | 2 | AVA, PVC | none | 2 | 1 |
| 10 | 50 | 2 | AVD, PVC | none | 2 | AVA, PVC | none | 2 | 2 |

#### c. Motifs in the whole chemical network (cell level, binarised)

Nodes 302 (of which 302 have at least one chemical edge), directed edges 3671. 38 self-connections (a cell onto itself) are left out of the graph and of every null model.
Motifs are induced triads, as in the standard triad census: 030T = a>b, b>c, a>c and no other edge among the three; 030C = a>b>c>a and no other edge. (An undirected count that ignores reciprocal edges, as `worm_stats.py` did, is larger.)
Categories (from the data set's own lists, stored in whole.json): interneuron 89, motor 123, other 7, sensory 83. The 7 neurons in none of the sensory, interneuron and motor lists form a fourth block, 'other'.
Observed: 030T (feed-forward loop) 2281, 030C (3-cycle) 184.

Null models, 100 shuffles each, seed 1 (one generator, models run in the order i, ii, iii):

| null model | motif | observed | null mean | null sd | z |
|---|---|---:|---:|---:|---:|
| (i) same number of edges placed at random | 030T feed-forward loop | 2281 | 1592.76 | 42.94 | 16.03 |
| (i) same number of edges placed at random | 030C 3-cycle | 184 | 532.66 | 23.29 | -14.97 |
| (ii) degree-preserving edge swaps (in- and out-degree of every cell kept) | 030T feed-forward loop | 2281 | 3296.88 | 97.94 | -10.37 |
| (ii) degree-preserving edge swaps (in- and out-degree of every cell kept) | 030C 3-cycle | 184 | 837.64 | 37.79 | -17.30 |
| (iii) degree-preserving swaps that also keep the block counts (pre category to post category) | 030T feed-forward loop | 2281 | 3376.47 | 117.91 | -9.29 |
| (iii) degree-preserving swaps that also keep the block counts (pre category to post category) | 030C 3-cycle | 184 | 826.71 | 43.41 | -14.80 |

Swap attempts per shuffle: 36710 (10 x edges); mean accepted swaps: (ii) 30285, (iii) 16583. sd is the sample standard deviation over the 100 shuffles.

Block counts of the observed network (rows pre category, columns post category); model (iii) keeps every one of them:

| pre \ post | sensory | interneuron | motor | other |
|---|---:|---:|---:|---:|
| sensory | 335 | 550 | 262 | 0 |
| interneuron | 288 | 819 | 490 | 28 |
| motor | 84 | 209 | 579 | 5 |
| other | 0 | 9 | 9 | 4 |

#### d. Ten cells of highest total chemical degree

Degree = number of distinct chemical partners out plus in (self-connections excluded); ties broken by name.

| rank | cell | class | category | degree | out | in | synapses out | synapses in |
|---:|---|---|---|---:|---:|---:|---:|---:|
| 1 | AVAL | AVA | interneuron | 108 | 43 | 65 | 258 | 667 |
| 2 | AVAR | AVA | interneuron | 107 | 48 | 59 | 292 | 685 |
| 3 | AVDR | AVD | interneuron | 76 | 30 | 46 | 155 | 237 |
| 4 | DVA | DVA | sensory | 74 | 44 | 30 | 158 | 146 |
| 5 | PVCR | PVC | interneuron | 71 | 38 | 33 | 172 | 262 |
| 6 | AVEL | AVE | interneuron | 70 | 24 | 46 | 124 | 304 |
| 7 | AVER | AVE | interneuron | 69 | 25 | 44 | 133 | 295 |
| 8 | AVBL | AVB | interneuron | 68 | 23 | 45 | 65 | 338 |
| 9 | PVCL | PVC | interneuron | 68 | 35 | 33 | 157 | 283 |
| 10 | AVDL | AVD | interneuron | 64 | 25 | 39 | 139 | 264 |

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

Question: does the measured compass wiring hold a bump where a cue put it, when the circuit is given one sign per cell type and gains per type or per type pair?

Answer so far: no. Two random searches found no setting that does. The wiring does form a compact bump, but the bump does not stay where the cue put it.

| Search | Parameters drawn | Draws | Trials per draw | Result |
|---|---|---|---|---|
| 1 (`compass_search.py`, desktop, 2026-11-22) | one synapse scale per presynaptic cell type, background current, synaptic time constants, turn gain | 2,000 | 2 (cue at ring positions 2 and 10), circuit seed 7 | no draw held a bump at both cues; none at one cue |
| 2 (`pairs=1`, GitHub runners, 2026-11-23, seeds 200 to 207) | one scale per type pair (19 pairs), a background current per population, and the rest as in search 1 | 8,000 | the same 2 | no draw held a bump at both cues; none at one cue |

"Held" is the test of `compass_trial.held` on the window 3,000 to 3,500 ms: bump within 30 degrees of the cue, strength above 0.5, at least 0.7 of the spikes near the peak, mean rate between 2 and 60 Hz.

What the 16,000 trials of search 2 did, from the `.stage1` records:

| Late state | Share of trials |
|---|---|
| silent (mean rate below 2 Hz) | 0.7927 |
| saturated (above 60 Hz) | 0.1502 |
| in the rate band (2 to 60 Hz) | 0.0571 (913 trials) |

Of the 913 trials in the rate band, 92 had a bump of strength above 0.5 and 161 were within 30 degrees of the cue, but only 2 had strength above 0.3 and were within 30 degrees. No draw had both of its trials in the band with strength above 0.3 and within 45 degrees. The six strongest late bumps (strength 0.98, every spike near the peak) sat about 60 degrees from a cue at position 10 and about 120 degrees from a cue at position 2, which is what one fixed resting place of the bump would give. This points to a bump that forms and then moves to a place the wiring prefers. It was not tested directly (no trial recorded the late angle for more than two cues).

**Conclusion.** With leaky integrate-and-fire cells, weights proportional to synapse counts and gains per type or per type pair, random search does not find a setting in which the measured wiring holds a bump at the cued place. This is a statement about these parameterisations and a random search of 10,000 draws, not a proof that none exists.

**Risk for A3.** The exit criterion of A3 (real wiring holds a bump with the reference's signs and gains on seeds 101 to 105) is not met by anything found so far. Before A3 is designed, one of these has to be decided and tested on runners: a directed search (an optimiser on a graded score, instead of random draws); a free parameter that is not per type (for example a per-cell background current that evens out the ring); or a level whose stated lesson is the drift itself (the measured wiring forms a bump and does not hold it without tuning).

## 4b. Feasibility probe of the worm touch circuit (iteration A2)

Scripts: `tools/connectome/worm_circuit.py`, `worm_trial.py`, `worm_search.py`, `worm_ablate.py`. All runs were made on GitHub runners on 2026-11-23. The circuit is `data/connectome/worm/touch.json` with nothing pruned, leaky integrate-and-fire cells and weights proportional to synapse counts. Touch is a 150 Hz train to each sensor of the touched end for 400 ms.

**Search 1: does the wiring tell head touch from tail touch?** 12,000 draws (seeds 300 to 307) over one sign per interneuron class, an optional sign rule between the backward and forward groups, the weight scales, the background current, the synaptic time constants and a minimum synapse count. "Selective" means head touch gives the A-type motor neurons at least 5 Hz and twice the B-type rate, and tail touch the reverse.

| Outcome | Draws |
|---|---|
| selective both ways | 16 of 12,000 |
| head criterion met | 2,104 |
| tail criterion met | 319 |

All 16 two-way draws have the chemical output of AVA inhibitory, of AVD excitatory and of PVC inhibitory, with electrical coupling on. No source in this note supports an inhibitory AVA output, so the setting is not used. With every class excitatory and no sign rule (1,652 draws), 769 met the head criterion and none the tail criterion, and in those the head and the tail trial gave the same rates: any touch ignites the whole circuit, and the A-type cells win because they receive more synapses. The second stage of this search (other seeds, ablations) failed on a bug in the touch spike times, fixed afterwards.

**Search 2: the circuit with every synapse excitatory, head touch only** (`worm_ablate.py`, 900 draws, seeds 400 to 405). A draw is usable when head touch gives the A-type cells at least 5 Hz, the baseline is below 1 Hz and the activity stops after the touch.

| Outcome | Draws |
|---|---|
| runaway (activity does not stop) | 831 |
| silent | 52 |
| usable | 17 |

In the 17 usable draws the B-type cells stay at or near 0 Hz. Each was then run on circuit seeds 7, 8 and 9 with interneuron classes silenced.

| Class | Silencing it abolishes the response (of 17) | It alone carries the response (of 17) |
|---|---|---|
| AVA | 17 | 0 |
| PVC | 17 | 0 |
| AVD | 13 | 0 |
| AVB | 6 | 0 |
| AVE | 0 | 0 |

Six draws form a cluster in which AVA, AVB, AVD and PVC are each needed and AVE is not: chemical scale 14.6 to 17.7 pA per synapse, background current 106 to 193 pA, excitatory time constant 2 ms, minimum count 3 or 5, no electrical coupling.

**What this means for the levels.**
- The wiring diagram offers two-hop routes from the head sensors to the A-type motor neurons through AVB, AVD, AVE or PVC (at 3 synapses or more). In the model no single interneuron class carries the response alone. A route on paper is not a pathway that works.
- AVA, the cell of highest degree, is on none of those shortest routes and is needed in every usable draw.
- The model's result for PVC (needed for the backward response) is a property of this model: excitatory synapses only, no electrical coupling. It is not a claim about the animal. Chalfie et al. 1985 is the source for what ablations do in the animal; its record was read, its full text was not, so no ablation result from it is quoted here.
- The usable region is narrow (17 of 900). A level needs one fixed parameter point, checked on held-out seeds.

**Motifs with the engine's census** (`python/spike/motifs.py`, induced three-cell classes, 302 cells, 3,671 directed pairs without self-connections, 50 shuffles, seed 1):

| Class | Count | z, same number of edges | z, same degrees |
|---|---|---|---|
| feed-forward loop (030T) | 2,281 | +18.25 | -8.56 |
| feedback loop (030C) | 184 | -16.30 | -14.57 |

The feed-forward loop is more frequent than in a random graph with the same number of edges and less frequent than in graphs with the same degrees. Milo et al. 2002 report the feed-forward loop as a motif of the worm's wiring; their data set and null model differ from these (their record was read, not their methods), so the difference is noted and not explained.

### The model point of the levels (iteration A2, step 1)

Script `tools/connectome/worm_point.py`, run on GitHub runners on 2026-11-23 (4 shards; run 45223533836). Circuit as in search 2 (every synapse excitatory, weight = synapse count x scale, no electrical coupling, tau_syn_ex 2 ms) and the same head touch (150 Hz Poisson trains to ALML, ALMR and AVM for 400 ms from 500 ms), 1,500 ms per trial. Grid: chemical scale 14.6, 15.5, 16.5, 17.7 pA per synapse; background current 110, 130, 150, 170, 190 pA; synapse floor 3 and 5 (40 points). Circuit seeds 1, 2, 3, 101 to 105 (8 seeds); the touch trains differ with the seed. A point must satisfy, on all eight seeds: intact, backward (A-type) motor neurons at least 5 Hz and forward (B-type) below 1 Hz over [500, 1200) ms and both below 1 Hz before the touch ([100, 500)) and at the end ([1000, 1500)); each of AVA, AVB, AVD and PVC silenced, A-type below 1 Hz; AVE silenced, A-type at least 5 Hz.

| Outcome | Points |
|---|---|
| the intact criterion fails on some seed (stopped at the first failing seed: seed 1 for 31 points, seed 3 for 3) | 34 |
| intact holds on all eight seeds, but silencing AVB does not abolish the response | 4 |
| holds on all eight seeds | 2: (15.5, 170, floor 3) and (16.5, 150, floor 3) |

No point with the synapse floor 5 holds. The point **chemical scale 15.5 pA per synapse, I_e 170 pA, tau_syn_ex 2 ms, synapse floor 3** is the model point of the levels. Rates in Hz per cell, one value per seed (1, 2, 3, 101, 102, 103, 104, 105):

| Condition | A-type | B-type |
|---|---|---|
| intact | 18.03, 19.86, 18.84, 18.23, 18.78, 19.39, 18.16, 18.64 | 0 on all |
| baseline and after the touch | A and B at 0 on all | |
| AVA silenced | 0 on all | 0 on all |
| AVB silenced | 0, 0, 0, 0, 0.07, 0, 0, 0 | 0 on all |
| AVD silenced | 0.07, 0.27, 0, 0.14, 0.14, 0, 0.07, 0.14 | 0 on all |
| PVC silenced | 0 on all | 0 on all |
| AVE silenced | 15.1, 17.28, 15.65, 15.78, 16.12, 16.05, 15.58, 16.53 | 0 on all |
| route AVB alone (the other four silenced) | 0 on all | 0 on all |
| route AVD alone | 0 on all | 0 on all |
| route AVE alone | 0 on all | 0 on all |

With the synapse floor 3 the shortest class-level routes from the head touch cells (ALM, AVM) to the backward motor neurons (DA, VA) are two hops long and pass through AVB, AVD or AVE (AVM to AVB, ALM to AVD and AVE; PVC is three hops away). **Each route kept alone gives silent motor neurons on all eight seeds**: in this model no shortest route carries the response, and AVA, which is on none of them, is needed. The other point that holds, (16.5, 150, 3), gives the same ordering with smaller margins (intact A-type 12.4 to 22.7 Hz, AVE silenced 9.7 to 20.0 Hz).

**The four lesions of `dx_a_worm`** (patient: every cell and connection, the outputs of one class at weight 0; the level's own Poisson touch, circuit seeds 1000 x seed + 7 for seeds 1 to 3 and 101 to 105; `tasks_atlas_worm_test` prints every row). Touch cells 58 to 87 Hz in all. AVB (Hz per cell, minimum to maximum over the eight seeds): AVA silenced 21.4 to 31.4, AVB silenced 20.7 to 35.7, AVD silenced 34.3 to 47.9, PVC silenced 0.7 to 5.7. With PVC silenced AVA and AVD are at 0 and the motor neurons at 0 on all eight seeds; PVC fires 20.0 to 27.9. So one scope on AVB separates PVC from the other three, which is the case's answer. With AVB silenced the backward motor neurons are at 0 on seven seeds and at 2.9 Hz (AVA 24.3, AVD 12.1) on seed 101: the lesion does not abolish the response on every seed. (An earlier run of `tools/connectome/worm_dx.py`, with explicit touch spike trains and circuit seeds 1 to 3 and 101 to 105 used directly, gave the same ordering: AVB 18.6 to 32.1 with AVB silenced, 0.7 to 3.6 with PVC silenced.)

**What the levels claim.** `a_path` scores the route, not the response (the response of a route kept alone is the lesson), `a_predict` asks which class can be silenced (AVE, measured on the eight seeds), and `dx_a_worm` asks which of AVA, AVB, AVD, PVC is silenced. All of it is a statement about this model (every synapse excitatory, no electrical coupling), not about the animal; the notebook entries cite Chalfie et al. 1985 only for what its record says: two pathways for anterior touch (AVD and AVB), one for posterior touch (PVC), found with wiring reconstructions and laser killing of cells.

**Sixty seeds through the tasks** (`spike-run`, circuit seeds 1 to 60, 2026-11-23). Rate of the backward (A-type) motor neurons during the response, with one class silenced by `a_predict`:

| Class silenced | A-type rate over 60 seeds (Hz) | Counts as "still backs up" (5 Hz or more) on |
|---|---|---|
| AVA | 0.00 to 0.00 | 0 of 60 |
| PVC | 0.00 to 0.00 | 0 of 60 |
| AVD | 0.00 to 0.34 | 0 of 60 |
| AVB | 0.00 to 5.78 | 1 of 60 (seed 20) |
| AVE | 7.14 to 18.03 | 60 of 60 |

The B-type rate is 0 in every run. The outcome for AVB is not clear-cut and the gap between AVB (up to 5.78 Hz) and AVE (down to 7.14 Hz) is too narrow for a threshold between them, so `a_predict` offers four classes and leaves AVB out, and says so. The reference of `a_path` passes on 60 of 60 seeds.

## 5. Sources

Only the eleven verified papers are cited. Each is quoted in the form its `match` expression in `research/atlas-a-citations.json` expects.

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
| Cook et al. 2019 | worm connectome | whole-animal connectomes of both sexes, quantitative connectivity matrices from sensory input to end-organ output; the data read through `cect` |
| Chalfie et al. 1985 | touch circuit | six touch receptors, five pairs of interneurons and 69 motor neurons; two anterior-touch pathways (AVD and AVB) and one posterior-touch pathway (PVC); gap-junction coupling of the anterior touch cells |
| Milo et al. 2002 | network motifs | motifs defined as patterns occurring significantly more often than in randomised networks; found also in the synaptic network of C. elegans |

The record of each paper in the PubMed database was read on 2026-11-23 (PMIDs 32880371, 34696823, 32916090, 30124430, 33315010, 23615618, 24561998, 21304930, 31270481, 3981252, 12399590). Only abstracts were read.

## Open: needs a source

- Transmitter and sign of E-PG, P-EN, P-EG and Delta7 (the script assumes the first three excitatory and Delta7 inhibitory): the abstracts of the verified papers do not state them; the full text of Turner-Evans et al. 2020 may.
- Which half of the bridge moves the bump in which direction for a given turn: no abstract states it.
- Whether the late bump of the compass probe rests at one fixed place for every cue (section 4): not tested directly.
- That the derived ring order (bridge position as the ring) corresponds to the physiological ring: the table supports only the half-ring spacing of Delta7 glomeruli.
