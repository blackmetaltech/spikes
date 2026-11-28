# Hemibrain excerpts: provenance

Written by `tools/connectome/hemibrain.py`. Do not edit by hand; re-run the script (`python3 tools/connectome/hemibrain.py`, and `--check` to verify).

## Dataset
- Name and version: hemibrain, release v1.2 (neuPrint dataset `hemibrain:v1.2`; the data are the "traced adjacencies" export).
- Paper: Scheffer LK et al. (2020) A connectome and analysis of the adult Drosophila central brain. eLife 9:e57443, doi:10.7554/eLife.57443.
- URL: https://storage.googleapis.com/hemibrain/v1.2/exported-traced-adjacencies-v1.2.tar.gz
- File: `exported-traced-adjacencies-v1.2.tar.gz`, 45,872,577 bytes. sha256 `07d8946eb0c4e3a5cb23d5769c9817847494f9fcadbc0ca239eed7bbd5555cf7`.
- Downloaded: 2026-11-22 (to `${XDG_CACHE_HOME:-~/.cache}/spike-connectome/hemibrain-v1.2/`, outside the repo; not committed).
- Tables read from the archive: `traced-neurons.csv` (bodyId, type, instance) and `traced-total-connections.csv` (bodyId_pre, bodyId_post, weight). `weight` is the total synapse count of the pair, as the archive's README says ("'weight' corresponds to the total synapse count in the connections between each neuron pair"). The README states that the files came from `neuprint-python` v0.4.12, `fetch_traced_adjacencies`, on `hemibrain:v1.2`.
- Counts in the whole file: 21739 neurons (traced, non-cropped).

## Terms of use
- Janelia FlyEM's hemibrain page, https://www.janelia.org/project-team/flyem/hemibrain, states: "Hemibrain is licensed under CC-BY." and links to https://creativecommons.org/licenses/by/4.0/ (read 2026-11-22). The sentence does not name a version number; the link is to the 4.0 deed.
- This is the statement of the data's owner. It does not name release v1.2 specifically, and the archive's own README carries no licence text. Treated as: **CC BY 4.0, attribution required; confirmed for the dataset as a whole, not stated per release**.
- Attribution to put on a level card: "Hemibrain connectome v1.2, Scheffer et al. 2020 (eLife 9:e57443), Janelia FlyEM and Google; CC BY 4.0. Simplified for this game."

## Reproduce
```
python3 tools/connectome/hemibrain.py            # download if needed, write the three files
python3 tools/connectome/hemibrain.py --check    # compare the committed files with a fresh extraction (needs the cache)
python3 tools/connectome/atlas_stats.py   # the statistics quoted in research/atlas-a.md (reads the committed tables)
```
The script is deterministic: neurons and connections are sorted, nothing random.

## compass.json: the head-direction circuit
- Neurons: all neurons whose `type` is exactly one of: `EPG`, `PEN_a(PEN1)`, `PEN_b(PEN2)`, `PEG`, `Delta7` (the type strings of the file; the file also has `EPGt`, 4 neurons, which are not included).
- Connections: every connection of the file with both ends in this set, with its synapse count. No threshold. 9487 connections.
- Each neuron has body id, type, instance name. Instances look like `EPG(PB08)_L3` (type, tract, protocerebral-bridge glomerulus: side L or R and number), `Delta7(PB15)_L6R3_L` (glomeruli it covers, then the soma side). `ring` is the position along the protocerebral bridge from left to right, 0 to 15: left glomerulus g is at 8 - g, right glomerulus g at 7 + g, modulo 16 (E-PG sit in glomeruli 1 to 8 on each side, 16 in all; P-EN cells also sit in glomerulus 9, which wraps to the neighbour of the eighth glomerulus of the other side). This is a position along the bridge and NOT the ring of headings: the E-PG to E-PG wiring has no ring structure in it (synapse-weighted mean cosine of the position difference -0.012). `heading` (0 to 15, 22.5 degrees each) is the ring coordinate: right glomerulus g is heading 2g, left glomerulus g is heading 3 - 2g, modulo 16, so each half of the bridge covers the whole ring, the two halves are mirrored and interleaved. The mapping is FITTED to the wiring, not taken from a paper: of the 32 mappings with right 2g and left s*2g + o, it has the highest mean cosine summed over E-PG to E-PG and E-PG to/from PEG (0.895 and 0.859; the scan is printed by `tools/connectome/compass_heading.py`). The instance carries no ellipsoid-body wedge. Mirror direction and the zero of heading are arbitrary. Neurons are sorted by (type, heading, side, id), so the E-PG cells are in heading order. The committed `compass.json` was rewritten from its own fields by `tools/connectome/add_heading.py` (adds `heading`, re-sorts, renumbers the connections); `hemibrain.py --check` against the raw export was NOT re-run after this change.

| type | neurons |
|---|---|
| Delta7 | 42 |
| EPG | 46 |
| PEG | 18 |
| PEN_a(PEN1) | 20 |
| PEN_b(PEN2) | 22 |

## mushroom_body.json: the right mushroom body
- Right hemisphere only: neurons whose instance ends in `_R`.
- Projection neurons (`PN`): uniglomerular olfactory types, regular expression `^(DA|DC|DL|DM|DP|VA|VC|VL|VM)\d+[a-z]*_(adPN|lPN|ilPN)$` (antennal-lobe glomerulus + `_adPN`/`_lPN`/`_ilPN`). Left out: multiglomerular `M_*` types, `VP*` (thermo- and hygrosensory) and `V_ilPN`.
- Kenyon cells (`KC`): every type starting `KC` (KCg-m, KCg-d, KCg-t, KCg-s1..s4, KCab-m, KCab-c, KCab-s, KCab-p, KCa'b'-m, KCa'b'-ap1, KCa'b'-ap2), right side.
- `APL`: the one APL neuron `APL_R`.
- MBONs (`MBON*`) and dopamine neurons (`PPL1*`, `PAM*`): right-side neurons with at least one synapse with a Kenyon cell in either direction.
- Connections: the full set between these neurons has 473731 connections (412964 more than the file keeps; the Kenyon-cell to Kenyon-cell connections alone are most of it). To keep the file near 1 MB, **a connection with fewer than 5 synapses is dropped**, except between the pairs PN to KC, KC to APL and APL to KC, where every count (1 and up) is kept because the fan-in and feedback statistics need them. The file keeps 60767 connections. Kept and total per role pair are in `connections_kept_and_total_by_role_pair` as [kept, total]. Strong (more than 4 synapses) KC to KC, KC to MBON, dopamine neuron to KC and similar connections are all there; the many weak ones are not, so any statistic over those pairs is a statistic of the strong part only.
- Dropped: all other cell types (DPM, other dopamine neurons, left-side neurons); the synapses' positions; the regions (ROI) the synapses lie in (the `traced-roi-connections.csv` file is not used).

| role | neurons |
|---|---|
| APL | 1 |
| DAN | 159 |
| KC | 1927 |
| MBON | 43 |
| PN | 97 |

By type: see `counts_by_type` in `mushroom_body.json`.

## compass_setting.json: the best holding setting of the compass probe
Not hemibrain data. The parameters (one scale per type pair, one background current per population, norm 2) with which `tools/connectome/compass_circuit.py` makes the measured compass wiring hold a bump, found by `tools/connectome/compass_opt.py` and checked by `compass_check.py` and `compass_signs.py` on GitHub runners (2026-11-28): 71 of 80 trials (16 cues x circuit seeds 101 to 105) held; `"turns": false`, because no setting found holds and turns on those seeds (research/atlas-a.md section 4c). It uses the assumed signs (Delta7 inhibitory, the rest excitatory) and the fitted `heading` coordinate above.
