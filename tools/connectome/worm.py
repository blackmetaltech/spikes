#!/usr/bin/env python3
"""Worm touch circuit -> circuits/worm_touch.json (the reference circuit of the "worm_touch" level).

DATA. OpenWorm ConnectomeToolbox, the PyPI package `cect` (MIT licence; github.com/openworm/ConnectomeToolbox), dataset reader `Cook2019Herm`:
Cook SJ et al. (2019) Whole-animal connectomes of both Caenorhabditis elegans sexes. Nature 571:63-71, doi:10.1038/s41586-019-1352-7 (hermaphrodite
adjacency matrices, SI 5, corrected July 2020). `cect` bundles the data file; the data keep the terms of the original publication. If `cect` is not
importable the script installs it into tools/connectome/.venv (pip, from pypi.org) and runs itself again there. If that fails (no network) it falls
back to the counts typed in FALLBACK below (copied from one run of this script with cect 0.3.5; not re-checked against the paper) and says so in the
circuit's meta.

WHAT IT BUILDS (about 13 named cells + 4 motor-neuron classes, 17 model cells; weights = synapse count x SCALE_PA)
  touch cells   ALML ALMR (anterior), AVM (anterior), PLML PLMR (posterior)           <- the sensors 'touch_head' (ALM, AVM) and 'touch_tail' (PLM)
  interneurons  AVDL AVDR, AVAL AVAR (backward command), PVCL PVCR, AVBL AVBR (forward command)
  motor classes MA = DA + VA (backward crawl), MB = DB + VB (forward crawl), one model cell per class
Edges that are in the circuit (counts from the data; left/right cells are separate model cells, so every cell pair keeps its own count):
  ALM > AVD (chemical), AVM > AVD (chemical), PLM > PVC (electrical, see below), AVD > AVA (chemical), PVC > AVB (chemical),
  AVA <> AVA and AVB <> AVB (the left-right partner: chemical + electrical), AVA > MA (chemical), AVB > MB (electrical)
SIMPLIFICATIONS (all of them are recorded in the circuit's meta.simplifications and are said on the level and on the What-is-real page)
  1. Spiking integrate-and-fire cells. The real cells of this circuit signal mostly with graded potentials, not spikes.
  2. Every chemical synapse is excitatory (the data do not give the sign). Electrical couplings (gap junctions) are modelled as excitatory chemical links
     with weight = count x SCALE_PA: spikes cannot be coupled electrically in this engine's cell models.
  3. Only the connections of the touch pathway as drawn by Chalfie et al. (1985) are kept. Strong connections of the data that are NOT modelled are
     listed at the end of a run (for example ALM > PVC, AVM > PVC, AVM > AVB, AVA > PVC, AVB > AVA, PVC > AVA, PVC > B-type motor neurons, PLM > AVA/AVD).
  4. The mutual inhibition of the two command groups (AVA/AVD against AVB/PVC) is NOT a connection of the data (AVA<>AVB chemical contacts exist, their
     sign is not known): it is added by hand, every AVA cell inhibits every AVB cell and the other way round, weight INHIBITION_PA, not scaled by counts.
  5. Weights are proportional to counts with one global scale. The touch inputs of the command cells have few synapses (2-8), so the excitability of
     each class (V_th) is tuned (CLASS_PARAMS) so that one touch spike moves it; this is a tuned value, not a measurement.
  6. Motor neuron classes are collapsed to one cell each (DA, VA, DB, VB are separate cells; MA = {DA, VA}, MB = {DB, VB}).

Usage:  python3 tools/connectome/worm.py [--check]     (--check: print the extracted counts and exit without writing)
"""
import json, os, re, subprocess, sys, collections

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "circuits", "worm_touch.json")
VENV = os.path.join(HERE, ".venv")

SCALE_PA = 25.0        # pA per synapse (one global scale)
INHIBITION_PA = -2000.0
DELAY = 2.0

# per-class excitability (tuned by a random search over these values until the level was passed on 5 seeds in all three modes): V_th in mV (iaf_psc_alpha, rest and reset -70 mV)
# and the refractory period of the command cells (10 ms keeps a held command at a plausible rate)
CLASS_PARAMS = {
    "ALM": {}, "AVM": {}, "PLM": {},
    "AVD": {"V_th": -69.4},
    "AVA": {"V_th": -63.4, "t_ref": 10.0},
    "PVC": {"V_th": -68.6},
    "AVB": {"V_th": -64.0, "t_ref": 10.0},
    "MA": {"V_th": -63.8},
    "MB": {"V_th": -60.8},
}

# typed from one run with cect 0.3.5 (Cook2019Herm), cells L/R as listed; (pre, post, kind, count)
FALLBACK = [
    ("ALML", "AVDR", "chem", 3), ("AVM", "AVDL", "chem", 1), ("AVM", "AVDR", "chem", 1),
    ("PLML", "PVCL", "gap", 4), ("PLMR", "PVCR", "gap", 8),
    ("AVDL", "AVAL", "chem", 37), ("AVDL", "AVAR", "chem", 37), ("AVDR", "AVAL", "chem", 41), ("AVDR", "AVAR", "chem", 52),
    ("PVCL", "AVBL", "chem", 7), ("PVCL", "AVBR", "chem", 27), ("PVCR", "AVBL", "chem", 21), ("PVCR", "AVBR", "chem", 14),
    ("AVAL", "AVAR", "chem", 12), ("AVAR", "AVAL", "chem", 7), ("AVAL", "AVAR", "gap", 18), ("AVAR", "AVAL", "gap", 18),
    ("AVBL", "AVBR", "chem", 6), ("AVBR", "AVBL", "chem", 2), ("AVBL", "AVBR", "gap", 18), ("AVBR", "AVBL", "gap", 18),
]
FALLBACK_MOTOR = [  # AVB > DB, VB electrical (class sums over the individual motor neurons); AVA > DA, VA chemical are added in main()
    ("AVBL", "DB", "gap", 33), ("AVBL", "VB", "gap", 45), ("AVBR", "DB", "gap", 36), ("AVBR", "VB", "gap", 42),
]

CELLS = {  # model population -> (cells in order, n)
    "ALM": ["ALML", "ALMR"], "AVM": ["AVM"], "PLM": ["PLML", "PLMR"], "AVD": ["AVDL", "AVDR"], "AVA": ["AVAL", "AVAR"],
    "PVC": ["PVCL", "PVCR"], "AVB": ["AVBL", "AVBR"], "MA": ["DA", "VA"], "MB": ["DB", "VB"],
}


def cell_pop(name):
    for p, cs in CELLS.items():
        if name in cs:
            return p, cs.index(name)
    raise KeyError(name)


def load_counts():
    """Returns (edges {(pre, post, kind): count}, source dict). Motor-neuron members are summed per class (DA1..DA9 -> DA, ...)."""
    try:
        import cect
        from cect.readers.Cook2019HermReader import get_instance
    except ImportError:
        if os.path.isdir(VENV) or "--no-venv" not in sys.argv:
            py = os.path.join(VENV, "bin", "python")
            try:
                if not os.path.exists(py):
                    subprocess.check_call([sys.executable, "-m", "venv", VENV])
                subprocess.check_call([py, "-m", "pip", "install", "--quiet", "cect"])
                os.execv(py, [py, os.path.abspath(__file__), "--no-venv"] + [a for a in sys.argv[1:] if a != "--no-venv"])
            except Exception as ex:  # no network, no venv module ...
                print("cect could not be installed (%s): using the hand-entered counts" % ex, file=sys.stderr)
        return None
    ds = get_instance()
    cells, conns = ds.read_data()
    def cls(n):
        return n[:2] if re.fullmatch(r"[DV][AB]\d+", n) else n
    edges = collections.defaultdict(float)
    for c in conns:
        kind = "chem" if c.syntype == "Chemical" else "gap"
        edges[(cls(c.pre_cell), cls(c.post_cell), kind)] += float(c.number)
    src = {"package": "cect", "version": cect.__version__, "reader": "Cook2019Herm", "licence": "cect: MIT; data: Cook et al. 2019 (Nature 571:63-71)"}
    return edges, src


def counts_table(edges):
    """The edges the circuit uses: [(pre cell, post cell, kind, count)]."""
    g = lambda a, b, k: edges.get((a, b, k), 0.0)
    out = []
    for a, b in (("ALML", "AVDR"), ("ALML", "AVDL"), ("ALMR", "AVDL"), ("ALMR", "AVDR"), ("AVM", "AVDL"), ("AVM", "AVDR")):
        out.append((a, b, "chem", g(a, b, "chem")))
    for a, b in (("PLML", "PVCL"), ("PLMR", "PVCR")):
        out.append((a, b, "gap", g(a, b, "gap")))
    for a in ("AVDL", "AVDR"):
        for b in ("AVAL", "AVAR"):
            out.append((a, b, "chem", g(a, b, "chem")))
    for a in ("PVCL", "PVCR"):
        for b in ("AVBL", "AVBR"):
            out.append((a, b, "chem", g(a, b, "chem")))
    for a, b in (("AVAL", "AVAR"), ("AVAR", "AVAL"), ("AVBL", "AVBR"), ("AVBR", "AVBL")):
        out.append((a, b, "chem", g(a, b, "chem")))
        out.append((a, b, "gap", g(a, b, "gap")))
    for a in ("AVAL", "AVAR"):
        for m in ("DA", "VA"):
            out.append((a, m, "chem", g(a, m, "chem")))
    for a in ("AVBL", "AVBR"):
        for m in ("DB", "VB"):
            out.append((a, m, "gap", g(a, m, "gap")))
    return [e for e in out if e[3] > 0]


def build(rows, src):
    """rows: [(pre cell, post cell, kind, count)] -> circuit dict."""
    pops = []
    layout = {"ALM": (130, 70), "AVM": (130, 150), "PLM": (130, 330), "AVD": (330, 70), "AVA": (330, 170), "PVC": (330, 330), "AVB": (330, 250), "MA": (530, 110), "MB": (530, 290)}
    for pid, cs in CELLS.items():
        x, y = layout[pid]
        pops.append({"id": pid, "model": "iaf_psc_alpha", "n": len(cs), "params": dict(CLASS_PARAMS[pid]), "role": "neuron", "pos": [[x + 30 * k, y] for k in range(len(cs))]})
    groups = collections.OrderedDict()
    for a, b, kind, n in rows:
        (pa, ia), (pb, ib) = cell_pop(a), cell_pop(b)
        groups.setdefault((pa, pb), {})
        groups[(pa, pb)][(ia, ib)] = groups[(pa, pb)].get((ia, ib), 0.0) + n
    order = [("ALM", "AVD"), ("AVM", "AVD"), ("PLM", "PVC"), ("AVD", "AVA"), ("AVA", "AVA"), ("PVC", "AVB"), ("AVB", "AVB"), ("AVA", "MA"), ("AVB", "MB")]
    projs = []
    for key in order:
        ed = groups.get(key)
        if not ed:
            continue
        edges = [[i, j, round(n * SCALE_PA, 3), DELAY] for (i, j), n in sorted(ed.items())]
        projs.append({"pre": key[0], "post": key[1], "edges": edges, "synapse": {"synapse_model": "static_synapse"}})
    for pre, post in (("AVA", "AVB"), ("AVB", "AVA")):
        projs.append({"pre": pre, "post": post, "rule": "all_to_all", "synapse": {"synapse_model": "static_synapse", "weight": INHIBITION_PA, "delay": DELAY}})
    inputs = [
        {"name": "touch_head", "kind": "poisson", "targets": [{"pop": "ALM"}, {"pop": "AVM"}], "weight": 1000.0, "value": 0.0},
        {"name": "touch_tail", "kind": "poisson", "targets": [{"pop": "PLM"}], "weight": 1000.0, "value": 0.0},
    ]
    outputs = [{"name": "reverse", "kind": "rate", "neurons": [{"pop": "MA"}]}, {"name": "forward", "kind": "rate", "neurons": [{"pop": "MB"}]}]
    meta = {
        "author": "spike (tools/connectome/worm.py)", "task": "worm_touch", "notes": "docs/circuits/bestiary_a.md",
        "dataset": src,
        "source": "Cook SJ et al. (2019) Nature 571:63-71, doi:10.1038/s41586-019-1352-7, hermaphrodite adjacency matrices (SI 5, corrected July 2020), read through the OpenWorm ConnectomeToolbox (cect, MIT)",
        "weights": "pre-post pair weight = synapse count x %g pA (one global scale); electrical couplings are modelled as excitatory links with the same rule; the AVA/AVB mutual inhibition is hand-set (%g pA) and is not in the data" % (SCALE_PA, INHIBITION_PA),
        "simplifications": [
            "spiking integrate-and-fire cells stand in for cells that signal mostly with graded potentials",
            "all chemical synapses excitatory (the data have no sign); gap junctions as excitatory links",
            "only the touch pathway of Chalfie et al. 1985 is kept; ALM>PVC, AVM>PVC, AVM>AVB, AVA>PVC, AVB>AVA, PVC>AVA and PVC>B-type motor neurons are in the data and left out",
            "AVA/AVB mutual inhibition added by hand (not a connection of the data)",
            "per-class excitability (V_th) tuned so that few-synapse touch inputs drive the command cells",
            "bilateral cells L/R kept as separate model cells; motor classes collapsed to DA, VA (MA) and DB, VB (MB), one cell each",
        ],
    }
    return {"schema_version": 1, "name": "Worm touch circuit (Cook 2019 counts)", "resolution_ms": 0.1, "seed": 7, "populations": pops, "projections": projs,
            "inputs": inputs, "outputs": outputs, "record": {"spikes": "all"}, "meta": meta}


NOT_MODELLED = [("ALM", "PVC"), ("AVM", "PVC"), ("AVM", "AVB"), ("AVA", "PVC"), ("AVB", "AVA"), ("PVC", "AVA"), ("PVC", "DB"), ("PVC", "VB"), ("PLM", "AVA"), ("PLM", "AVD")]


def main():
    got = load_counts()
    if got is None:
        rows = [e for e in FALLBACK if e[3] > 0]
        # AVA > MA chemical, class sums over the individual motor neurons
        rows += [("AVAL", "DA", "chem", 67.0), ("AVAL", "VA", "chem", 79.0), ("AVAR", "DA", "chem", 70.0), ("AVAR", "VA", "chem", 66.0)]
        rows += [e for e in FALLBACK_MOTOR if e[3] > 0]
        src = {"package": "none", "reader": "hand-entered", "licence": "values typed from a cect 0.3.5 run (Cook2019Herm); not re-checked against the paper"}
        print("WARNING: cect not available: hand-entered counts used", file=sys.stderr)
    else:
        edges, src = got
        rows = counts_table(edges)
        side = collections.defaultdict(float)
        for (a, b, k), n in edges.items():
            aa = re.sub(r"[LR]$", "", a) if a[:3] in ("ALM", "AVM", "PLM", "AVD", "AVA", "PVC", "AVB") else a
            bb = re.sub(r"[LR]$", "", b) if b[:3] in ("ALM", "AVM", "PLM", "AVD", "AVA", "PVC", "AVB") else b
            side[(aa, bb, k)] += n
        print("not modelled (counts from the data, summed over the left and right cells):")
        for a, b in NOT_MODELLED:
            print("  %-4s > %-4s chem %4d  gap %4d" % (a, b, side[(a, b, "chem")], side[(a, b, "gap")]))
    print("dataset:", src)
    for r in rows:
        print("  %-5s > %-5s %-4s %5.1f" % r)
    if "--check" in sys.argv:
        return
    c = build(rows, src)
    with open(OUT, "w") as f:
        json.dump(c, f, indent=1)
        f.write("\n")
    print("wrote", os.path.relpath(OUT, ROOT))


if __name__ == "__main__":
    main()
