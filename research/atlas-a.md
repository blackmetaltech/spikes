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

**Superseded in part: the ring coordinate used in this section (bridge position) is wrong; see the coordinate finding in 4c. The results below were made in that coordinate and are kept for the record, not as evidence about the wiring.**

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

## 4c. Directed search and normalisation (A3)

### The coordinate finding

The probe of section 4 and the first runs of this section took the 16 glomeruli of the protocerebral bridge, left to right, as 16 ring positions (`ring` in `compass.json`). That coordinate is wrong for the wiring: it gives the E-PG to E-PG connections no ring structure. `tools/connectome/compass_heading.py` scores a coordinate by the synapse-weighted mean of cos(22.5 degrees x heading difference) over connections (1 = synapses join cells of equal heading, about 0 = unrelated) and scans 32 mappings: right glomerulus g to heading 2g, left glomerulus g to (s*2g + o) mod 16, s = +1 or -1, o = 0 to 15. Delta7 cells have two glomeruli and are left out. Rule: the highest E-PG>E-PG plus E-PG<>PEG. Printed:

```
s   o    EPG>EPG  EPG<>PEG  EPG>PEN      rule
+1  0      0.391     0.780    0.452     1.171
+1  1      0.391     0.780    0.452     1.171
+1  2      0.396     0.777    0.455     1.173
+1  3      0.403     0.771    0.461     1.175
+1  4      0.413     0.765    0.469     1.177
+1  5      0.422     0.758    0.479     1.180
+1  6      0.431     0.751    0.488     1.182
+1  7      0.437     0.747    0.495     1.184
+1  8      0.440     0.745    0.499     1.185
+1  9      0.440     0.745    0.499     1.185
+1  10     0.435     0.748    0.496     1.184
+1  11     0.428     0.754    0.490     1.182
+1  12     0.419     0.760    0.482     1.179
+1  13     0.409     0.767    0.472     1.176
+1  14     0.400     0.774    0.463     1.174
+1  15     0.394     0.778    0.456     1.172
-1  0      0.592     0.775    0.269     1.368
-1  1      0.750     0.812    0.389     1.562
-1  2      0.856     0.841    0.522     1.697
-1  3      0.895     0.859    0.648     1.754
-1  4      0.862     0.861    0.747     1.723
-1  5      0.760     0.849    0.806     1.609
-1  6      0.606     0.823    0.813     1.429
-1  7      0.423     0.789    0.770     1.212
-1  8      0.239     0.750    0.681     0.989
-1  9      0.081     0.713    0.562     0.795
-1  10    -0.025     0.684    0.429     0.659
-1  11    -0.064     0.667    0.303     0.602
-1  12    -0.030     0.664    0.204     0.633
-1  13     0.071     0.676    0.145     0.747
-1  14     0.225     0.702    0.138     0.927
-1  15     0.408     0.736    0.181     1.145
current `ring` (position along the bridge): EPG>EPG -0.012  EPG<>PEG 0.751  EPG>PEN 0.346  rule 0.739
best by the rule: s=-1 o=3 (1.754); next: s=-1 o=4 (1.723); best with s=+1: o=8 (1.185)

```

The mapping chosen is s = -1, o = 3: right glomerulus g is heading 2g, left glomerulus g is heading 3 - 2g (mod 16). Each half of the bridge then covers the whole ring, the two halves are mirrored and interleaved (right: even headings, left: odd). Every mapping with s = +1 scores at most 1.185 on the rule, against 1.754. The mapping is fitted to the wiring, not taken from a paper; its mirror direction and its zero are arbitrary. `compass.json` now has a `heading` field and is sorted by heading (so the E-PG cells are in heading order); `ring` is kept and is the position along the bridge (`PROVENANCE.md`). `hemibrain.py --check` against the raw export was not re-run.

**Turn shifts**, from the same script (for each P-EN cell: the weighted circular mean heading of its E-PG targets minus that of its E-PG inputs, in headings of 22.5 degrees, mean over the cells of the group):

```
turn shifts under the chosen mapping (s=-1, o=3); a positive shift moves the bump to higher heading indices
type         side  cells   mean shift      range
PEN_a(PEN1)  L        10     +0.63 hd  +0.1..+1.1
PEN_a(PEN1)  R        10     -0.58 hd  -0.8..-0.0
PEN_b(PEN2)  L        11     +0.63 hd  +0.1..+1.0
PEN_b(PEN2)  R        11     -0.61 hd  -0.9..-0.0
```

The P-EN cells of the left half of the bridge move the bump towards higher heading (+0.63 headings, about 14 degrees), those of the right half towards lower (-0.6), for both P-EN types. That is the direction the probe already assumed (`turn_right` drives the left cells), so the turn wiring is unchanged; it now rests on this table. What a turn does to the physiological heading is still not read from a paper.

The probe was reworked on the heading: the cue for heading k drives the E-PG cells with heading k-1, k, k+1 (8 to 9 cells, both halves), and the readout bins E-PG spikes by heading.

### Results on the heading coordinate

Runs 45305060631 (searches and evolution), 45309446774 (16-cue checks of 11 settings), 45309843671 (turn-gain sweeps). Printed by `compass_summary2.py`:

```
random searches (stage 1: cues 2 and 10, circuit seed 7)
search          draws  silent    band   satur.  held 2 held 1 cue   stage 2
scale per type   8000   0.757   0.066    0.177       0          7         0
scale per pair   8000   0.788   0.056    0.156       0         12         0

evolution strategy (mu + lambda = 8 + 32, 30 min per run)
run             gens best score first->last held 4 cues  held 8 of 8
opt_n0_12        140 0.516 -> 0.926               no           no
opt_n0_13        184 0.517 -> 0.810               no           no
opt_n0_14         96 0.576 -> 0.956              yes       gen 29
opt_n0_15        153 0.464 -> 0.896               no           no
opt_n2_16        170 0.000 -> 0.812               no           no
opt_n2_17        133 0.419 -> 0.939              yes       gen 62
opt_n2_18        107 0.446 -> 0.946              yes        gen 7
opt_n2_19        145 0.481 -> 0.938               no           no
```

Random search finds no setting that holds at two cues in the heading coordinate either: the shares of silent, in-band and saturated trials are about what they were, and 7 and 12 draws held at one cue. The evolution strategy does: three of eight runs produced individuals that hold at all four cues on circuit seeds 7 and 8, the first one at generation 7, 29 and 62. The best and the individuals holding at 8 of 8 trials of each run were then checked (`compass_final.py`; `tools/connectome/compass_candidates.jsonl` holds their parameters):

```
finalists: held of 16 cues on circuit seeds 101..105; places = distinct late angles (rounded to 10 deg, cue-independent bumps) over the 16 cues at seed 101; drift; turn effect (deg/s; right sign: -20 negative, +20 positive)
 1 opt_n0_14.jsonl best score of the run                score 0.956  held [13, 11, 12, 11, 11] (58 of 80)  failing cues [7, 8, 9, 10, 11, 12]  places [40, 80, 130, 230, 300, 350]  drift -1.54 deg/s (mean |v| 1.57)
 2 opt_n0_14.jsonl best of the individuals held at 8 of 8 trials score 0.956  held [12, 13, 12, 13, 12] (62 of 80)  failing cues [8, 9, 10, 11]  places [40, 80, 130, 300, 350]  drift -0.51 deg/s (mean |v| 0.55)
 3 opt_n2_18.jsonl best score of the run                score 0.946  held [10, 11, 11, 11, 10] (53 of 80)  failing cues [3, 7, 8, 12, 13, 14]  places [0, 40, 110, 230]  drift -0.03 deg/s (mean |v| 0.10)
 4 opt_n2_18.jsonl best of the individuals held at 8 of 8 trials score 0.946  held [11, 11, 11, 11, 10] (54 of 80)  failing cues [3, 7, 8, 12, 13, 14]  places [0, 40, 110, 120, 230, 310]  drift -0.01 deg/s (mean |v| 0.10)
 5 opt_n2_17.jsonl best score of the run                score 0.939  held [11, 12, 12, 12, 12] (59 of 80)  failing cues [1, 2, 9, 13, 14]  places [90, 130, 170, 250, 340]  drift 0.67 deg/s (mean |v| 0.69)
 6 opt_n2_19.jsonl best score of the run                score 0.938  held [11, 12, 12, 12, 12] (59 of 80)  failing cues [0, 12, 13, 14, 15]  places [30, 90, 130, 230]  drift 3.11 deg/s (mean |v| 3.25)
 7 opt_n2_17.jsonl best of the individuals held at 8 of 8 trials score 0.938  held [15, 14, 13, 15, 14] (71 of 80)  failing cues [2, 13, 14]  places [40, 90, 130, 170, 250, 340]  drift -0.03 deg/s (mean |v| 0.05)
 8 opt_n0_12.jsonl best score of the run                score 0.926  held [10, 11, 10, 11, 11] (53 of 80)  failing cues [5, 7, 8, 9, 10, 11]  places [40, 80, 130, 290, 340]  drift -0.00 deg/s (mean |v| 0.06)
 9 opt_n0_15.jsonl best score of the run                score 0.896  held [8, 8, 7, 7, 8] (38 of 80)  failing cues [0, 1, 2, 3, 4, 5, 9, 12, 15]  places [150, 160, 170, 220, 230, 300]  drift 0.42 deg/s (mean |v| 0.51)
10 opt_n2_16.jsonl best score of the run                score 0.812  held [2, 3, 3, 3, 3] (14 of 80)  failing cues [2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]  places [20, 30]  drift 0.01 deg/s (mean |v| 0.20)
11 opt_n0_13.jsonl best score of the run                score 0.810  held [2, 2, 1, 2, 2] (9 of 80)  failing cues [0, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]  places [30]  drift -0.02 deg/s (mean |v| 0.12)
```

None holds at all 16 cues on all of seeds 101 to 105 (the best, no. 7, 71 of 80 trials, failing at cues 2, 13 and 14). The late angles of the finalists sit at a few places 40 to 80 degrees apart (the `places` column), and a cue is held when it lies within 30 degrees of one: the attractor is a set of discrete places, not a continuous ring. Drift is small (no. 7: -0.03 degrees per second, mean absolute 0.05, over 1.5 to 5.5 s at seed 101; the best five are under 1.6).

**Turn.** Turn-gain sweeps (a constant turn current of turn_w pA per unit on the P-EN cells of one half of the bridge, 20 units, 6 s; the effect is the late angle minus the late angle without turn):

```
turn-gain sweeps (compass_turn.py; effect of asym -20 / +20 on the late angle in degrees at cues 2, 6, 10, 14; right sign: negative / positive)
candidate 1 (opt_n0_14.jsonl best score of the run)
turn_w  effect of -20 by cue 2,6,10,14 (deg) effect of +20 by cue 2,6,10,14 (deg) late strength / rate at asym 0, mean
1       [-1, 0, -1, 0]                 [0, 0, 99, 0]                  0.92 / 19.5 Hz
3       [-1, -2, -3, -1]               [2, 0, 99, 1]                  0.92 / 21.8 Hz
10      [4, 1, 1, 2]                   [1, -32, -32, -19]             0.89 / 30.0 Hz
30      [19, 19, 19, 155]              [159, -7, -8, -65]             0.84 / 42.7 Hz
100     [41, 35, 35, 41]               [-95, -101, -100, -95]         0.38 / 69.4 Hz
```

Turning does not work in any setting found. At the evolved gain (1) the bump does not move (effects 0 to 3 degrees); at higher gains it either does not move, jumps by 30 to 100 degrees with signs that do not follow the turn, or, at gain 30 to 100, the activity saturates (rate 40 to 100 Hz) and loses its bump (strength down to 0.06). The evolution strategy had no turn term in its score and turn_w was fixed at 1.

### Conclusion

(a) The earlier failure was mainly the coordinate. The old runs are kept below for the record and are not evidence about the wiring. (That resting place at ring positions 7 to 8 may be the bump moving to one of the discrete places of the heading ring, read in the wrong coordinate; not tested.)

(b) Normalisation was not needed: of the three runs that found holding individuals, two were without it (norm 0) and one with norm 2 (outgoing totals equalised per cell and type pair); the 16-cue results are no better with it (no. 7 uses norm 2; no. 2 with norm 0 holds 62 of 80). The random searches were run without normalisation. The exact assumption of the best setting (no. 7): norm 2, one scale per type pair, one current per population, turn_w 1; the parameters are in `compass_candidates.jsonl`.

(c) The best setting holds at 71 of 80 (cue, seed) trials on seeds 101 to 105, not all 16 cues. It does not meet the exit criterion and no parameter file was saved to `data/connectome/hemibrain/`.

(d) Recommendation: the measured wiring, in the heading coordinate, can hold a bump with fitted gains (drift well under 1 degree per second, at discrete places 40 to 80 degrees apart), but only after a directed search (no random draw of 16,000 did it), and it does not yet turn. A level can say that holding is a property of the gains and the wiring together; it should not offer turning from this wiring until a search that includes the turn (a turn term in the score and a free turn gain) has found a setting. That search is the next step.

Not tested: a search with a turn term and a free turn gain; the evolved settings under a cue of other strength or length; other cell models; signs other than the assumed ones; a continuous attractor (the places are discrete here); `hemibrain.py --check` after the heading change; more than 8 evolution runs of 30 minutes.

### Turning, signs and places (second brief)

Runs 46116624744 (turn-term evolution; hemibrain check; signs and check of the best holding setting), 46120236441 (checks and signs of the two settings that turn), 46120421868 (rotating-trial evolution) and 46123996065 (checks of its settings). Tables are printed by `compass_summary3.py`; tools `compass_opt2.py`, `compass_check.py`, `compass_signs.py`.

**1. Turning.** The evolution strategy now has a turn term. A turn trial: cue, 1 s of settling, then 1 s (1,500 to 2,500 ms) of a 500 Hz spike train of `pulse_w` pA per spike onto the P-EN cells of one half of the bridge, the background current on both halves unchanged; the shift is the bump angle before (1,200-1,500 ms) against after (3,500-4,000 ms); a trial passes if the shift is 2 to 10 headings in the predicted direction (left half towards higher), the bump is the same at 2,700-3,200 and 3,500-4,000 ms (within one heading), and it is still compact and in the rate band. Genes: the 19 pair scales, the 5 currents, the background turn gain and `pulse_w`. The score is half hold (cues 2, 6, 10, 14), half turn (cue 2 up, cue 10 down, cue 10 up, cue 2 down); definitions in the docstring of `compass_opt2.py`. Start: the three settings that held (45 minutes, 12 runs).

```
TURN-TERM EVOLUTION (start: settings 2, 4, 7 of compass_candidates.jsonl, 3 per start; mu 4, lambda 16, 45 min; trials on seed 7 then 8: hold at cues 2, 6, 10, 14 and turn at (cue 2, +), (10, -), (10, +), (2, -))
run      start   gens best first->last  hold       turn       shifts of best (headings)  4+4 on 7  4+4 on 7 and 8
opt2_0   2        209 0.848 -> 0.852        0.95       0.75       [4.46, 3.95, 3.28, -4.43]  no        no
opt2_1   4         97 0.727 -> 0.797        0.84       0.75       [6.23, 4.24, 2.0, 0.01]    no        no
opt2_2   7        218 0.520 -> 0.600        0.93       0.27       [1.99, 0.09, 0.02, 0.08]   no        no
opt2_3   2        212 0.723 -> 0.975        0.95       1.00       [2.01, 6.6, 3.07, 2.27]    yes       yes
opt2_4   4        183 0.578 -> 0.791        0.84       0.74       [6.2, 4.27, 1.95, 0.0]     no        no
opt2_5   7        212 0.471 -> 0.724        0.94       0.51       [2.14, 0.07, 2.02, -2.14]  no        no
opt2_6   2        211 0.726 -> 0.854        0.95       0.75       [2.01, 4.4, 3.19, 0.03]    no        no
opt2_7   4        178 0.676 -> 0.771        0.84       0.70       [6.13, 4.51, 1.62, -0.02]  no        no
opt2_8   7        117 0.492 -> 0.810        0.85       0.77       [7.4, 6.26, 3.84, 0.21]    no        no
opt2_9   2        112 0.603 -> 0.974        0.95       1.00       [2.03, 4.41, 3.07, 4.4]    yes       yes
opt2_10  4        201 0.694 -> 0.771        0.85       0.69       [6.2, 4.64, 1.57, -0.0]    no        no
opt2_11  7        209 0.597 -> 0.684        0.93       0.44       [2.09, -0.03, 1.5, -2.1]   no        no
```

Two of 12 runs (opt2_3 and opt2_9, both from setting 2) found individuals that hold at four cues and turn at four trials on both circuit seeds 7 and 8, with shifts of 2.0 to 6.6 headings. They do not hold up elsewhere. On circuit seeds 101 to 105 (all 16 cues, turn at cues 2, 6, 10, 14 in both directions):

```
SETTINGS THAT HOLD AND TURN ON SEEDS 7 AND 8 (compass_turners.jsonl), checked on seeds 101 to 105:
setting 1:
hold, 16 cues x seeds 101..105: {101: 11, 102: 11, 103: 11, 104: 12, 105: 11} total 56 of 80
turn, 4 cues x 2 directions x seeds 101..105: passes by seed {101: 3, 102: 3, 103: 3, 104: 4, 105: 4} total 17 of 40; passes right half (towards higher): 10 of 20, left half (lower): 7 of 20
turn shifts in the predicted direction, headings: mean 1.37, min -1.99, max 6.66 (n=40)
resting headings of the late bump (nearest heading) over 80 active trials (16 cues x 5 seeds):
heading  trials  cues that end here (cue: seeds)    of which held
2        10      1:5 2:5                            10
4        10      3:5 4:5                            10
6        27      5:5 6:5 7:5 8:5 9:5 11:2           10
10       6       10:5 11:1                          6
13       22      11:2 12:5 13:5 14:5 15:5           15
15       5       0:5                                5
mean |cue heading - resting heading|: 1.06 headings (23.9 deg) over active trials, 0.55 headings (12.5 deg) over held trials; largest 5 headings
distinct resting headings: 6

sign assignments, top four:
a=16 EPG=+ PENa=+ PENb=+ PEG=+ D7=- held 20 of 20
a=18 EPG=+ PENa=- PENb=+ PEG=+ D7=- held 19 of 20
a=24 EPG=+ PENa=+ PENb=+ PEG=- D7=- held 15 of 20
a=26 EPG=+ PENa=- PENb=+ PEG=- D7=- held 14 of 20
with >= 18 of 20: 2 

setting 2:
hold, 16 cues x seeds 101..105: {101: 11, 102: 11, 103: 11, 104: 11, 105: 12} total 56 of 80
turn, 4 cues x 2 directions x seeds 101..105: passes by seed {101: 4, 102: 5, 103: 4, 104: 4, 105: 4} total 21 of 40; passes right half (towards higher): 10 of 20, left half (lower): 11 of 20
turn shifts in the predicted direction, headings: mean 2.24, min -0.04, max 7.51 (n=40)
resting headings of the late bump (nearest heading) over 80 active trials (16 cues x 5 seeds):
heading  trials  cues that end here (cue: seeds)    of which held
2        11      0:1 1:5 2:5                        10
4        10      3:5 4:5                            10
6        27      5:5 6:5 7:5 8:5 9:5 11:2           15
10       6       10:5 11:1                          6
13       26      0:4 11:2 12:5 13:5 14:5 15:5       15
mean |cue heading - resting heading|: 1.18 headings (26.4 deg) over active trials, 0.55 headings (12.5 deg) over held trials; largest 5 headings
distinct resting headings: 5

sign assignments, top four:
a=16 EPG=+ PENa=+ PENb=+ PEG=+ D7=- held 20 of 20
a=24 EPG=+ PENa=+ PENb=+ PEG=- D7=- held 15 of 20
a=18 EPG=+ PENa=- PENb=+ PEG=+ D7=- held 15 of 20
a=26 EPG=+ PENa=- PENb=+ PEG=- D7=- held 15 of 20
with >= 18 of 20: 1
```

That is 56 of 80 hold trials and 17 and 21 of 40 turn trials, against 71 of 80 hold trials for the best setting without a turn term, and the turn shifts at the cues not in the score (6, 14) are mostly zero or in jumps. A second round of evolution started from these two, with a new random set of hold cues, turn trials (any of 16 cues, both directions) and circuit seed 7 or 8 in each generation, then checked on 101 to 105:

```
ROTATING-TRIAL EVOLUTION (start: the two settings above, 6 runs each, 45 min, a new random trial set per generation on seeds 7 or 8), final settings checked on seeds 101 to 105:
run   gens hold of 80   turn of 40 (right half, left half)
1          49           16 of 40 (7, 9)
2          44           20 of 40 (7, 13)
3          41           26 of 40 (14, 12)
4          54           24 of 40 (14, 10)
5          50           20 of 40 (10, 10)
6          38           23 of 40 (9, 14)
7          45           26 of 40 (15, 11)
8          30           20 of 40 (6, 14)
9          55           15 of 40 (5, 10)
10         40           18 of 40 (5, 13)
11         48           18 of 40 (8, 10)
12         45           10 of 40 (7, 3)
```

No setting holds and turns on seeds 101 to 105: the best reach 26 of 40 turn trials with 45 to 54 of 80 hold trials, and none is near 40 of 40 with 80 of 80. **Turning: yes on four trials of circuit seeds 7 and 8 that were in the score, no on seeds 101 to 105.** Not tested: whether more generations, a larger turn trial set or an extra turn gene per P-EN type would close the gap; the hold trials alone are limited by the discrete places (see 3).

**2. Signs.** For the best holding setting (setting 7 of `compass_candidates.jsonl`: norm 2, scale per type pair, no turn term), every gain as fitted, all 32 assignments of the five type signs on cues 2, 6, 10, 14 and circuit seeds 101 to 105:

```
a = bit 1 EPG, 2 PEN_a, 4 PEN_b, 8 PEG, 16 Delta7 (1 = inhibitory); top eight rows, then counts:
a=16 EPG=+ PENa=+ PENb=+ PEG=+ D7=- held 16 of 20
a=18 EPG=+ PENa=- PENb=+ PEG=+ D7=- held 14 of 20
a=20 EPG=+ PENa=+ PENb=- PEG=+ D7=- held 13 of 20
a=22 EPG=+ PENa=- PENb=- PEG=+ D7=- held 12 of 20
a=24 EPG=+ PENa=+ PENb=+ PEG=- D7=- held  5 of 20
a=26 EPG=+ PENa=- PENb=+ PEG=- D7=- held  5 of 20
a=28 EPG=+ PENa=+ PENb=- PEG=- D7=- held  1 of 20
a= 0 EPG=+ PENa=+ PENb=+ PEG=+ D7=+ held  0 of 20
assignments with >= 18 of 20: 0 ; with >= 1: 7 of 32
```

Seven of 32 assignments hold anything; 25 hold 0 of 20. All seven have E-PG excitatory and Delta7 inhibitory. The assumed assignment (Delta7 inhibitory only, a = 16) is the best at 16 of 20, but the best wrong assignment (P-EN_a also inhibitory) holds 14 of 20, so the margin is two trials; no assignment holds 18 or more. Two P-EN-flipped variants (a = 20, 22) hold 13 and 12. What is sharp: P-EG excitatory (flipping it drops 16 to 5 or less), E-PG excitatory and Delta7 inhibitory (any other value holds 0). What is not sharp in this setting: the signs of the two P-EN types. For the two settings that turn the same table has the assumed assignment at 20 of 20 with the best wrong one at 19 and 15 of 20 (setting 1 and 2); it does not separate the P-EN signs there either. A level that asks for exactly one assignment would have to count the P-EN signs as free, or use a setting in which they matter (a turn test would, since P-EN drives the turn; not run).

**3. Places.** Same setting, seeds 101 to 105, all 16 cues:

```
hold, 16 cues x seeds 101..105: {101: 15, 102: 14, 103: 13, 104: 15, 105: 14} total 71 of 80
resting headings of the late bump (nearest heading) over 80 active trials (16 cues x 5 seeds):
heading  trials  cues that end here (cue: seeds)    of which held
2        7       1:5 2:2                            7
4        18      2:3 3:5 4:5 5:5                    15
6        9       6:5 7:4                            9
8        11      7:1 8:5 9:5                        11
11       21      10:5 11:5 12:5 13:5 14:1           15
15       14      0:5 14:4 15:5                      14
mean |cue heading - resting heading|: 0.79 headings (17.7 deg) over active trials, 0.62 headings (13.9 deg) over held trials; largest 3 headings
distinct resting headings: 6
```

The bump rests at six headings, 2, 4, 6, 8, 11 and 15, spaced 2 to 3 headings (45 to 67 degrees) apart, and a cue ends at the nearest of them: the mean absolute error between cue heading and resting heading is 0.79 headings (17.7 degrees) over all active trials and 0.62 headings (13.9 degrees) over the held ones, at most 3 headings. So this wiring holds a heading to about 45 to 67 degrees, not to 22.5; 6 places, not 16. The two settings that turn rest at 5 to 6 headings too (2, 4, 6, 10, 13 and 15, mean error 1.06 and 1.18 headings). A level has to state it as "about six directions".

**Hemibrain check.** `hemibrain.py --check` runs on a runner (the public export is fetched there; the cache is downloaded first with `ensure_cache(True)`, because `--check` itself never downloads):

```
DIFFERENT: /src/data/connectome/hemibrain/PROVENANCE.md
ok: /src/data/connectome/hemibrain/compass.json
ok: /src/data/connectome/hemibrain/mushroom_body.json
exit 1
```

`compass.json` and `mushroom_body.json` equal a fresh extraction byte for byte, so the heading change in `hemibrain.py` and the pass in `add_heading.py` agree. `PROVENANCE.md` differed; its cause was not inspected (the probable one is the download date line, which depends on the cache). `PROVENANCE.md` and the template in `hemibrain.py` have both since gained the paragraph on `compass_setting.json`, so the check is not re-run on them.

**Saved setting.** `data/connectome/hemibrain/compass_setting.json` holds the best holding setting (71 of 80 trials on seeds 101 to 105, `"turns": false`), with a line in `PROVENANCE.md`.

### 4c-old. Directed search and normalisation in the bridge coordinate (kept for the record)

**These runs used the bridge position as the ring and so say nothing about the wiring (see the coordinate finding above).** Question: is the failure of section 4 a drift to a preferred place, does a parameter-free normalisation of the wiring rescue the bump, and does a directed search find a setting that holds? Tools: `compass_circuit.py` (option `norm`), `compass_drift.py`, `compass_opt.py`, `compass_summary.py`. Runs on GitHub runners: 45276642579 (drift test and three random searches with normalisation) and 45279671084 (evolution strategy, 20 jobs). The tables are printed by those scripts.

#### Drift test

The search of section 4 (`pairs=1`, rng seeds 200 to 207, 1,000 draws each) was re-created; per seed the three draws with the strongest late bump inside the rate band (cue at position 10) were kept, 24 draws in all (the brief asked for 20). Each was run at all 16 cue positions for 6,000 ms. Table: the angle of the E-PG population vector in 5,500 to 6,000 ms, in degrees, for cues 0 to 15 (- = silent); then R (length of the mean vector of the angles over the cues: 1 means one place for every cue) and the number of cues whose angle is within 30 degrees of the cue, in the windows cue (300-500 ms), 700-1000, 1500-2000, 3000-3500, 5500-6000; then the mean of the late angles and the mean late strength.

```
id  seed  | late (5500-6000) angle by cue 0..15 | R and follow per window: cue, 700, 1500, 3000, 5500   | late mean-angle  late-strength
0   200   | 163 163 164 164 163 163 163 163 163 164 164 163 163 163 164 164 | 0.14/16 1.00/3 1.00/3 1.00/3 1.00/3 | 163 0.99
1   200   | 174 174 174 175 174 174 174 174 174 174 174 174 174 174 174 174 | 0.05/16 1.00/3 1.00/3 1.00/3 1.00/3 | 174 0.93
2   200   | 183 183 183 183 183 183 184 183 184 183 183 183 184 183 182 182 | 0.23/15 1.00/3 1.00/3 1.00/3 1.00/3 | 183 0.76
3   201   | 168 168 168   - 168 168 168 168 168 168 168 168 168 168 168 168 | 0.00/16 0.94/4 1.00/3 1.00/2 1.00/2 | 168 0.92
4   201   | 181 181 181 182 181 181 180 180 181 182 181 181 181 180 181 180 | 0.09/16 1.00/3 1.00/3 1.00/3 1.00/3 | 181 0.82
5   201   | 193 189 193 198 196 197 193 182 182 184 176 175 187 194 187 192 | 0.19/16 1.00/2 0.99/3 1.00/3 0.99/3 | 189 0.59
6   202   | 169 164 178 170 164 175 179 178 176 179 169 173 176 179 178 172 | 0.22/16 1.00/3 0.99/3 0.99/2 1.00/3 | 174 0.88
7   202   | 184 183 184 180 182 183 180 181 180 182 183 184 181 182 182 180 | 0.06/16 1.00/3 1.00/3 1.00/3 1.00/3 | 182 0.78
8   202   | 180 180 180 180 180 180 181 181 180 180 180 180 180 180 180 181 | 0.06/16 1.00/3 1.00/3 1.00/3 1.00/3 | 180 0.88
9   203   | 190 190 190 189 190 190 190 190 190 191 190 191 190 190 190 190 | 0.11/16 1.00/2 1.00/2 1.00/2 1.00/2 | 190 0.60
10  203   | 168 167 170 169 168 168 167 167 168 166 170 168 167 168 168 167 | 0.01/16 1.00/2 1.00/2 1.00/2 1.00/2 | 168 0.58
11  203   | 194 193 194 194 194 194 193 194 194 193 194 193 193 194 194 193 | 0.24/16 1.00/2 1.00/2 1.00/2 1.00/2 | 194 0.46
12  204   | 167 167 167 167 167 167 167 167 167 167 167 167 167 167 167 167 | 0.09/16 0.99/3 1.00/2 1.00/2 1.00/2 | 167 0.98
13  204   | 168 168 168 168 168 168 168 168 168 168 168 168 168 168 168 168 | 0.01/16 1.00/2 1.00/2 1.00/2 1.00/2 | 168 0.98
14  204   | 174 174 177 174 179 173 174 172 177 176 170 175 178 175 174 174 | 0.04/16 1.00/2 1.00/3 1.00/3 1.00/3 | 175 0.90
15  205   | 167 167 165 166 165 167 167 167 166 167 168 168 166 168 168 167 | 0.05/16 1.00/2 1.00/2 1.00/2 1.00/2 | 167 0.84
16  205   | 179 187 184 187 179 187 189 180 182 182 185 190 184 187 185 184 | 0.19/16 1.00/3 1.00/3 1.00/3 1.00/3 | 184 0.77
17  205   | 180 179 180 178 180 180 179 180 180 180 180 181 180 180 181 180 | 0.00/16 1.00/3 1.00/3 1.00/3 1.00/3 | 180 0.77
18  206   | 167 167 167 167 167 167 167 167 168 167 167 167 167 167 167 167 | 0.11/16 1.00/2 1.00/2 1.00/2 1.00/2 | 167 0.98
19  206   | 171 171 171 172 170 171 170 171   -   - 171 171 171 172 171 170 | 0.01/16 1.00/1 1.00/1 1.00/1 1.00/1 | 171 0.82
20  206   | 188 184 184 183 182 180 186 186 182 183 183 181 181 184 186 183 | 0.14/16 1.00/2 1.00/3 1.00/3 1.00/3 | 183 0.78
21  207   | 178 179 179 179 179 180 178 179 178 179 178 178 179 178 178 180 | 0.07/16 1.00/3 1.00/3 1.00/3 1.00/3 | 179 0.89
22  207   | 167 167 167 168 168 168 167 168 167 168 167 167 168 168 167 167 | 0.00/16 1.00/2 1.00/2 1.00/2 1.00/2 | 168 0.83
23  207   | 183 185 186 181 180 181 176 184 183 183 182 185 182 184 179 182 | 0.06/16 1.00/3 1.00/3 1.00/3 1.00/3 | 182 0.81
```

Means over the draws: cue    R 0.09  follow 16.0; w700   R 1.00  follow 2.5; w1500  R 1.00  follow 2.6; w3000  R 1.00  follow 2.5; w5500  R 1.00  follow 2.5. Late angles binned to the 16 ring positions: 22 of 24 draws use one position for all active cues, 2 use two; the position is 7 (ring angle 157.5 degrees), 8 (180) or, once, 9 (202.5). Moves:

```
drift test, 24 draws x 16 cues: mean move cue window (300-500 ms) to 700-1000 ms: 84.2 deg (largest single 179.8 deg), over 0.45 s between window centres = 187 deg/s; mean move 700-1000 ms to 5500-6000 ms: 3.88 deg
```

So the bump goes to one resting place within a few hundred ms after the cue ends and stays there (about 4 degrees of further movement from 0.7 to 5.5 s). The cue only decides it when it happens to be put at the resting place (2 or 3 of 16 cues). The resting place is ring positions 7 to 8, the two glomeruli next to the midline of the bridge in the derived ring order.

#### Normalisation

`norm=1`: for each ordered pair of cell types, the incoming synapse counts of each post cell are rescaled so that every post cell of that type receives the same total from that pre type (the mean over cells that receive any); `norm=2`: the same for the outgoing totals per pre cell; `norm=3`: the two alternated 30 times. The sum over the pair and the set of connected cells are unchanged. The table of counts has ratios of largest to smallest incoming total per pair up to 29 (outgoing 27); after `norm=1` incoming 1.0 (outgoing 19.5), `norm=2` outgoing 1.0 (incoming 16.5), `norm=3` outgoing 1.0 and incoming up to 6.0.

Random search as in section 4 (`pairs=1`, 4,000 draws per setting, cues 2 and 10, circuit seed 7; draws passing both go to 4 cues x 2 seeds; none did):

```
norm    draws   trials   silent  in band   satur.  band,str>.5 band,err<30  held both  held one
norm1    4000     8000   0.8020   0.0495   0.1485            0         77          0         0
norm2    4000     8000   0.8000   0.0470   0.1530           30         54          0         0
norm3    4000     8000   0.8001   0.0456   0.1542            0         47          0         0
```

Without normalisation the same search found 79.3 per cent silent, 15.0 per cent saturated, 5.7 per cent in band (section 4); normalisation changes none of these shares by more than about one point. No draw held a bump at both cues, with any of the three.

#### Directed search

`compass_opt.py`: (mu + lambda) evolution strategy, mu 8, lambda 32, start from the best 32 of 128 random draws, self-adapted log-normal steps on the 19 pair scales and 5 population currents, 42 minutes per job (102 to 271 generations), five seeds for each of norm 0 to 3. Score as in the docstring (1.0 perfect; rate in band, strength, spikes near the peak, angular error, over cues 2, 6, 10, 14). Individuals with all four cues held on seed 7 are re-run on seed 8; the best three of each run were then checked on circuit seeds 101 to 105 and all 16 cues.

```
evolution runs (5 seeds per norm; (mu + lambda) = 8 + 32; 42 min each). best score by generation, then the check of the best three of each seed
norm  seeds best score gen 0 (min-max)   best score last gen (min-max) gens     max cues held of 16 (one seed), best three of every seed
0         5 0.443 - 0.707             0.770 - 0.811             143-271   3 (mean 1.60)
1         5 0.449 - 0.523             0.586 - 0.820             190-249   3 (mean 0.08)
2         5 0.416 - 0.640             0.784 - 0.818             146-258   3 (mean 1.92)
3         5 0.379 - 0.479             0.549 - 0.809             102-203   2 (mean 0.35)

```

No individual in any of the 20 runs held at all four cues on seed 7 (so none reached the two-seed test). The scores plateau near 0.81 for norm 0 and 2 (and for single runs of norm 1 and 3), which is what a bump that is in band, strong and compact but at the cue for only part of the cues gives. On the 16-cue check the best individuals held 2 or 3 of 16 cues per seed, the cues next to the resting place. Nothing passes on seeds 101 to 105, so no parameter file was saved to `data/connectome/hemibrain/`.

#### Conclusion

(a) Yes, the failure is a drift to a preferred place. For 24 of the best draws of the previous search, every cue ends within about 20 degrees of one angle (163 to 194 degrees), and the move from the cue's position happens within about 0.5 s of the cue (mean 84 degrees, up to 180); afterwards it hardly moves (3.9 degrees mean from 0.7 to 5.5 s). The resting place is at the bridge midline, ring positions 7 to 8.

(b) No. Equalising incoming totals (norm 1), outgoing totals (norm 2) or both (norm 3) leaves the shares of silent, in-band and saturated trials where they were and no draw holds at two cues (12,000 draws). The evolution strategy with norm 2 reaches the same plateau as without, with norm 1 or 3 it is worse in most runs. The assumption that rescales the counts is therefore not enough; whether the resting place is a property of the derived ring order, of the cell counts per position (46 E-PG cells over 16 positions) or of the missing cell-level inputs is not separated here.

(c) The best setting found holds a bump at 2 or 3 of 16 cues; it does not pass on seeds 101 to 105 and is not saved.

(d) Recommendation: do not build the A3 compass level on the claim that the measured wiring holds a heading. Build it on the drift: the player sees that the measured wiring forms one bump and lets it slide to one place, and the level's task is what has to be added to hold it (the open list says what the table lacks). Within the limits below.

Not tested: other cell models or synaptic dynamics (conductance-based, long synaptic time constants beyond 15 ms); signs other than the assumed ones; a different ring order, a per-cell background current, or noise; cue strength and duration; longer evolution runs or a different optimiser; a per-cell gene set (only 19 pair scales and 5 currents were free). The drift test used 24 draws, one per cue and seed, with a 500 ms cue; the drift speed of 187 degrees per second is a mean over the cue-to-window move and a lower bound on the move speed, since the bump may arrive earlier than 700 ms.

## 4d. The compass levels measured in the engine (iteration A3)

Runs 46125376018, 46126304606, 46127746707, 46129197369, 46130470624, 46131476627 and 46132527513 (`research.yml`, `build=full`; the numbers are printed by `engine/tests/tasks_atlas_compass_test` in measurement mode, `ATLAS_MEASURE=signs|places|gain|dx`, on the engine's own tasks `a_signs`, `a_places`, `a_gain`, `dx_a_compass`, not by the python tools). Every table is over circuit seeds 1 to 40 (task seed s gives circuit seed 1000 s + 7, as the game and `spike-run` do) unless it says five or twenty. The wiring and the setting are those of section 4c (`compass_setting.json`); what the levels add is the world, the readout and the criteria below. The 16-cue sweeps (the one with P-EG flipped and the per-cue gain table) ran with a temporary environment hook, `SPIKE_A_CUES`, that replaced the cues of the levels; it is in the history of the branch `w-a3` and not in the merged code, so those two tables cannot be reproduced from the final tests (the sweep of the reference, a_places, can). Where this note says "in this model", it means this circuit of point cells with fitted gains; none of it is a statement about the fly.

**World.** A run is a row of trials of 3,700 ms. Trial: a reset (a negative current of 1 nA on every cell, 0 to 200 ms), a cue (Poisson, 300 Hz, 400 pA per spike, on the E-PG cells of heading k-1, k and k+1, 200 to 700 ms), nothing more. The readout is the rate of the E-PG cells of each of the 16 headings over the last 500 ms of the trial (3,000 to 3,500 ms after the cue starts), and "holds" is the definition of `compass_trial.held` (within 30 degrees of the cue, strength above 0.5, at least 0.7 of the spikes within two headings of the peak, 2 to 60 Hz). The 40 pA of tonic input that the study's two turn sensors gave every P-EN cell is part of their background current. Two things differ from the study and were checked: the cue is a Poisson sensor (independent trains per cell) instead of one shared train, and trials follow each other with a reset instead of each starting on a fresh circuit. With both, the 16-cue sweep below gives the same per-cue pattern as the study's single trials (cue 13 never holds; cues 2 and 14 on some seeds only; 71 of 80 trials there, 563 of 640 here).

**1. a_signs: the eight sign assignments** (E-PG, P-EG, Delta7; the P-EN signs fixed excitatory), five cues in turn, headings 4, 11, 6, 15, 8, pass = the bump holds at all five. Held trials per cue (order 4, 11, 6, 15, 8), of 40 seeds:

```
E-PG P-EG Delta7   passes   held per cue (4, 11, 6, 15, 8)
+    +    -        40/40    40 40 40 40 40      <- the reference
+    +    +         0/40     0  0  0  0  0
+    -    -         0/40    40 24  0 40  1      <- P-EG flipped
+    -    +         0/40     0  0  0  0  0
-    +    -         0/40     0  0  0  0  0
-    +    +         0/40     0  0  0  0  0
-    -    -         0/40     0  0  0  0  0
-    -    +         0/40     0  0  0  0  0
```

The first cue set, headings 4, 8, 11, 15, was not robust: with P-EG flipped it passed on 3 of 40 seeds (held per cue 40, 3, 27, 40). Cue 6 was added because P-EG flipped does not hold there on any seed. The 16-cue sweep with P-EG flipped (E-PG +, Delta7 -; 40 seeds) holds, per cue 0 to 15: 37 0 0 40 40 40 0 0 1 13 0 26 40 40 39 40. The failures of P-EG flipped are almost all `moved` (a compact bump at another heading; 282 of 640 trials, 2 `spread`). Every assignment that makes E-PG or Delta7 wrong spreads over the ring without a bump (mean rate 8 to 353 Hz, strength 0.09 or less) on every trial.

**2. a_places: how many directions?** Reference signs and gains, the 16 cues in turn, a resting place = the heading nearest to the bump angle of an active trial (mean rate 0.3 Hz or more).

```
distinct resting headings per run, 40 seeds:  six on 39, five on 1       (4 to 9 = "about6" on 40 of 40; 1 to 3, 13 or more, none on 0 of 40)
held per cue 0..15, of 40:   40 39 21 40 40 40 40 40 40 39 40 40 40  0 24 40
resting headings (trials, of 640):  2: 60   4: 140   6: 77   8: 83   11: 176   15: 104
where each cue ends:  0->15 (40)  1->2 (39), 4 (1)  2->2 (21), 4 (19)  3,4,5->4  6->6  7->6 (37), 8 (3)  8,9->8  10..13->11  14->11 (16), 15 (24)  15->15
```

The same six places as section 4c (2, 4, 6, 8, 11, 15). The count is the answer; the level states it in words. The bet's candidates and their ranges (all16 13 or more, about6 4 to 9, two 1 to 3, none 0) are chosen to be far apart: the observed count is 5 or 6.

**3. a_gain: one scale.** Sweep of the Delta7 scale (it multiplies every connection out of Delta7; 1.0 is the fitted gain), signs as in the reference, cues 4 and 12, passes of 40 seeds at every step of 0.05 from 0.10 to 1.80 and at 1.03:

```
scale       0.10 0.15 0.20 0.25 | 0.30 0.35 ... 1.80 (every step, 1.03 too)
passes/40      0    0    0    0 |   40   40 ...   40
mean rate Hz 202  151-175  82-103  70 |  58/53  51/46 ... 13/14        (cue 4 / cue 12; 22/21 at 1.0)
```

Below 0.30 the activity spreads (rate 60 Hz and over: `spread`, 80 of 80 trials at each of the four values); from 0.30 up to 1.80 the bump holds on 40 of 40 at all 36 values. **The window is one-sided.** More inhibition never killed the bump: it lowers the rates (5 to 7 Hz at scales 6 and 8) and the bump stays; above a scale of about 2 the result depends on the seed for some cues (cues 4 and 11: 19/20 passes at 2.0, 14 of 20 at 2.5, 11 of 20 at 3.5, 20 of 20 at 6 and 8), so the card's range stops at 1.8. Two things found on the way, kept for the record: (a) with the cues 4 and 11 there was a notch at 0.48 to 0.52 where the pass failed on every seed (the resting place of cue 11 moves to heading 12 and the bump settles more than 30 degrees from the cue); the resting places move with the gain, and across scales 0.3 to 1.8 only cues 4 and 12 hold at every step (5 seeds, 16 cues; cues 3 and 15 from 0.35, cue 5 from 0.6), which is why a_gain uses 4 and 12; (b) none of the other cell types has a window at all. With Delta7 at 1.0 (20 seeds): the E-PG scale holds from 0.30 to 2.0 (0.15 and below fails, 0.25 passes on 13 of 20) and has no upper edge to 2.0; the P-EG scale holds from 0.1 to 4 on every seed; the P-EN scale (both kinds) holds from 0.1 to 4 on every seed. So the Delta7 scale is the choice, with one clean edge, and the level says so (the card, the note and the verdict). This does not agree with the brief's expectation of a bump that dies under too much inhibition: in this model it does not.

**4. dx_a_compass: silence one cell type** (the outputs of one type at weight 0; the two cues of a_gain; mean Hz per cell over 3,200 to 3,700 ms after the first cue starts; 40 seeds, ranges over seeds):

```
silenced   held at both cues   E-PG        P-EN a      P-EN b       P-EG        Delta7
none         40/40              22.2-22.4   4.5-4.8     82.1-82.5    25.6-25.8   90.6-91.4
E-PG          0/40              32.2-32.4   0.0         58.0         0.0         0.0
P-EN a       40/40              21.6-22.0   4.5-4.7     81.5-82.0    25.1-25.6   89.2-90.0
P-EN b       40/40              21.0-21.3   3.8-4.1     79.9-80.6    24.6-24.9   86.8-88.0
P-EG         40/40              19.7-20.0   3.0-3.4     79.2-79.6    23.2-23.7   82.8-83.6
Delta7        0/40              234.2-234.7 91.6-92.2   234.9-235.5  279.2-280.0 339.7-340.3
```

Only two lesions stop the ring holding a bump, E-PG and Delta7, and their signatures are far apart and the same on all 40 seeds: with Delta7 silenced the compass cells fire at 234 Hz and over and the Delta7 cells at 340 Hz; with E-PG silenced the P-EG, Delta7 and P-EN a cells are at 0.0 Hz. Silencing P-EG or either P-EN type is invisible in the holding (the compass cells change by 0 to 2.5 Hz). The culprit is Delta7: its signature (compass cells over-active while the Delta7 cells themselves fire hard) is distinct from every other candidate on all 40 seeds. The case lists all five types as candidates; the three that leave the ring working are ruled out by the recordings, not by the symptom.

**Limits, plainly.** The criteria are those of this model. The wiring holds a bump where it is put at most cues, rests at about six headings and does not turn reliably (4c): there is no turning level. The count of resting places, the sign table and the gain window are results of point cells with fitted gains; the gains were fitted to hold a bump, so "this wiring holds a bump" is a statement about a fit, and the signs are the assumed ones that the fit found to be the only family that holds (4c). The state-space view of the roadmap is not built.

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
- Which half of the bridge moves the bump in which direction for a given turn: the wiring gives it (left half towards higher heading, section 4c); no verified paper statement.
- The discrete places (40 to 80 degrees apart) of the held bump in section 4c: whether the real circuit has a continuous attractor that the fitted gains and the point neurons only approximate, or discrete places: not separated.
- A setting that holds at all 16 cues and turns both ways: none found; the best holds 71 of 80 trials on seeds 101 to 105 (six resting headings), and settings that turn hold 56 of 80 and turn in 17 to 26 of 40 trials (section 4c). More generations, a larger turn trial set, or a separate turn gain per P-EN type are not tried.
- Which sign assignments hold: for the best holding setting only the signs of E-PG, P-EG and Delta7 matter (the assumed ones are the only family that holds); the P-EN signs change the count by 2 to 4 of 20 only. Functional sign data for the P-EN types would be needed, and the table cannot give it.
- The ring order is now answered by the scan of section 4c: the wiring supports the heading mapping right 2g, left 3 - 2g (mod 16), two interleaved mirrored copies. That this is the physiological ring is not claimed; the mirror direction and the absolute zero of heading are arbitrary.
