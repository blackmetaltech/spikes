# Worm connectome excerpts: provenance

Written by `tools/connectome/worm_table.py`. Do not edit by hand; re-run the script (`~/.cache/spike-connectome/venv-cect/bin/python tools/connectome/worm_table.py`, and `--check` to verify).

## Dataset
- Name: C. elegans hermaphrodite whole-animal connectome, Cook et al. (2019).
- Paper: Cook SJ et al. (2019) Whole-animal connectomes of both Caenorhabditis elegans sexes. Nature 571:63-71, doi:10.1038/s41586-019-1352-7.
- Data file: `SI 5 Connectome adjacency matrices, corrected July 2020.xlsx` (4,188,190 bytes, sha256 `1f4fdbf84746b69b49a8da0816f52787860ce349b638dce37924ba80f90c70c9`), the hermaphrodite sheets "hermaphrodite chemical" and "herm gap jn symmetric", bundled inside the `cect` package.
- Reader: OpenWorm ConnectomeToolbox, PyPI package `cect` version 0.3.5 (MIT licence per `tools/connectome/worm.py`), `cect.readers.Cook2019HermReader.get_instance().read_data()`. The data keep the terms of the original publication (as `tools/connectome/worm.py` states).
- Date written: 2026-11-23. Interpreter: `~/.cache/spike-connectome/venv-cect/bin/python`; nothing is downloaded by the script.
- What the reader returns: 5905 connection records between 302 neurons (the 302 neurons of the hermaphrodite as listed in the data; muscles and other non-neuronal targets are not in this reader's output): 3709 directed chemical pairs (20965 synapses in all, 38 of the pairs are self-connections) and 2196 gap-junction records. The gap-junction sheet is symmetric (every pair is listed in both directions with the same count, checked by the script); it holds 1105 distinct couplings (14 are self-couplings). Counts are whole numbers and are the numbers of the data; nothing is rescaled.

## touch.json: the touch-withdrawal excerpt
- Neurons (54), left and right cells separate: touch receptors ALML, ALMR, AVM, PLML, PLMR (role `touch`); command interneurons AVAL/R, AVBL/R, AVDL/R, AVEL/R, PVCL/R (`command`); A-type motor neurons DA1-9 and VA1-12 (`motor_A`, 21); B-type motor neurons DB1-7 and VB1-11 (`motor_B`, 18).
- Connections: every chemical and every electrical connection of the data with both ends in this set, with its count. No threshold, nothing added. 308 chemical (1540 synapses) and 161 electrical (846 in the counts, symmetric couplings listed once).
- Each neuron has name, `class` (trailing digits dropped, then a trailing L or R dropped when the other side exists: ALML -> ALM, DA3 -> DA, AVAL -> AVA; derived by the script, not a field of the data), `role` and `category` (the data set's own lists).
- Dropped: every cell outside the set, and all connections that have one end outside it.

## whole.json: every neuron
- Neurons: all 302 neurons of the reader's output, with name, class (same rule as above) and category. Category comes from the lists in `cect.Cells` (`SENSORY_NEURONS_COOK`, `INTERNEURONS_COOK`, `MOTORNEURONS_COOK`): {'interneuron': 89, 'motor': 123, 'other': 7, 'sensory': 83}. The 7 neurons in none of the three lists are `other`: CANL, CANR, MCL, MCR, MI, NSML, NSMR.
- `chemical`: every directed chemical connection with its count (3709). `electrical`: every gap-junction coupling once (1105). Self-connections are kept; statistics that ignore them must say so.
- Format: connections are [pre_index, post_index, count] into `neurons`, one per line to keep the file small and its diffs readable.
- Dropped: nothing within the reader's output. Not in it: muscles, other non-neuronal cells, synapse positions and the neurotransmitter and sign (the reader's `synclass` field is not kept).

## Reproduce
```
~/.cache/spike-connectome/venv-cect/bin/python tools/connectome/worm_table.py            # write the three files
~/.cache/spike-connectome/venv-cect/bin/python tools/connectome/worm_table.py --check    # regenerate in memory and compare with the committed files
python3 tools/connectome/worm_report.py   # the numbers quoted in research/atlas-a.md (reads the committed tables only)
```
The script is deterministic: neurons and connections are sorted, nothing random.
