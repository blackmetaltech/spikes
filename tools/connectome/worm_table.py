#!/usr/bin/env python3
"""C. elegans hermaphrodite connectome -> data/connectome/worm/{touch.json, whole.json, PROVENANCE.md}.

DATA. Cook SJ et al. (2019) Whole-animal connectomes of both Caenorhabditis elegans sexes. Nature 571:63-71, doi:10.1038/s41586-019-1352-7
(hermaphrodite adjacency matrices, SI 5, corrected July 2020), read through the OpenWorm ConnectomeToolbox package `cect`, reader `Cook2019Herm`
(same reader as worm.py and worm_stats.py). Needs cect; run it with the interpreter that has it:
  ~/.cache/spike-connectome/venv-cect/bin/python tools/connectome/worm_table.py            write the three files
  ~/.cache/spike-connectome/venv-cect/bin/python tools/connectome/worm_table.py --check    regenerate in memory, compare with the committed files (exit 1 on a difference)
Standard library plus cect. Output is sorted and deterministic. Statistics for research/atlas-a.md: tools/connectome/worm_report.py (reads the committed tables only).
"""
import collections, hashlib, json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "data", "connectome", "worm")
DATE = "2026-11-23"

SENSORS = ["ALML", "ALMR", "AVM", "PLML", "PLMR"]
COMMANDS = ["AVAL", "AVAR", "AVBL", "AVBR", "AVDL", "AVDR", "AVEL", "AVER", "PVCL", "PVCR"]
MOTOR_A = ["DA%d" % i for i in range(1, 10)] + ["VA%d" % i for i in range(1, 13)]
MOTOR_B = ["DB%d" % i for i in range(1, 8)] + ["VB%d" % i for i in range(1, 12)]


def load():
    import cect
    from cect.readers.Cook2019HermReader import get_instance
    import cect.Cells as C
    ds = get_instance()
    cells, conns = ds.read_data()
    return cect, C, ds, cells, conns


def neuron_class(name, names):
    """Class of a neuron: trailing digits dropped (DA1 -> DA); then a trailing L or R dropped when the other side exists (AVAL -> AVA)."""
    base = re.sub(r"\d+$", "", name)
    if base != name:
        return base
    if name[-1] in "LR" and name[:-1] + ("R" if name[-1] == "L" else "L") in names:
        return name[:-1]
    return name


def category(name, C):
    if name in set(C.SENSORY_NEURONS_COOK):
        return "sensory"
    if name in set(C.INTERNEURONS_COOK):
        return "interneuron"
    if name in set(C.MOTORNEURONS_COOK):
        return "motor"
    return "other"


def build():
    cect, C, ds, cells, conns = load()
    names = set(cells)
    sens, inter, mot = set(C.SENSORY_NEURONS_COOK), set(C.INTERNEURONS_COOK), set(C.MOTORNEURONS_COOK)
    assert not (sens & inter) and not (sens & mot) and not (inter & mot)
    chem, gap = {}, {}
    for c in conns:
        assert float(c.number) == int(c.number) and c.number > 0
        d = chem if c.syntype == "Chemical" else gap
        k = (c.pre_cell, c.post_cell)
        assert k not in d
        d[k] = int(c.number)
    assert all(gap.get((b, a)) == n for (a, b), n in gap.items()), "gap junction table not symmetric"
    # electrical: symmetric, each coupling kept once (name order); self-couplings (a == b) kept
    elec = {(a, b): n for (a, b), n in gap.items() if a <= b}

    def cat(n):
        return "sensory" if n in sens else "interneuron" if n in inter else "motor" if n in mot else "other"

    # ---- whole.json
    wn = sorted(names)
    widx = {n: i for i, n in enumerate(wn)}
    whole = {
        "source": {"dataset": "Cook et al. 2019, C. elegans hermaphrodite (Nature 571:63-71)", "package": "cect " + cect.__version__, "reader": "Cook2019Herm",
                   "script": "tools/connectome/worm_table.py", "provenance": "data/connectome/worm/PROVENANCE.md"},
        "format": "neurons sorted by name; chemical and electrical entries are [pre_index, post_index, synapse_count] into neurons. Chemical connections are directed. "
                  "Electrical couplings are symmetric and listed once, with name(pre) <= name(post) in sort order; self-couplings are kept. Self-connections are kept in both lists.",
        "category_note": "category from the data set's own lists in cect.Cells: SENSORY_NEURONS_COOK, INTERNEURONS_COOK, MOTORNEURONS_COOK; 'other' = in none of them. class = trailing digits dropped, then a trailing L or R dropped when the other side exists (derived by neuron_class() in the script, not a field of the data).",
        "counts": {"neurons": len(wn), "chemical": len(chem), "electrical": len(elec),
                   "by_category": dict(sorted(collections.Counter(cat(n) for n in wn).items()))},
        "neurons": [{"name": n, "class": neuron_class(n, names), "category": cat(n)} for n in wn],
        "chemical": [[widx[a], widx[b], v] for (a, b), v in sorted(chem.items(), key=lambda kv: (widx[kv[0][0]], widx[kv[0][1]]))],
        "electrical": [[widx[a], widx[b], v] for (a, b), v in sorted(elec.items(), key=lambda kv: (widx[kv[0][0]], widx[kv[0][1]]))],
    }

    # ---- touch.json
    role = {}
    for n in SENSORS: role[n] = "touch"
    for n in COMMANDS: role[n] = "command"
    for n in MOTOR_A: role[n] = "motor_A"
    for n in MOTOR_B: role[n] = "motor_B"
    for n in role:
        assert n in names, n
    order = SENSORS + COMMANDS + MOTOR_A + MOTOR_B
    tidx = {n: i for i, n in enumerate(order)}
    tconn = [[tidx[a], tidx[b], v, "chemical"] for (a, b), v in chem.items() if a in tidx and b in tidx]
    tconn += [[tidx[a], tidx[b], v, "electrical"] for (a, b), v in gap.items() if a in tidx and b in tidx and tidx[a] <= tidx[b]]
    tconn.sort(key=lambda r: (r[3], r[0], r[1]))
    nc = sum(1 for r in tconn if r[3] == "chemical")
    touch = {
        "source": whole["source"],
        "format": "connections are [pre_index, post_index, synapse_count, kind] into neurons; kind is 'chemical' (directed) or 'electrical' (symmetric, listed once with pre_index <= post_index; self-couplings kept). Left and right cells are separate.",
        "roles": {"touch": "touch receptors", "command": "command interneurons (AVA, AVB, AVD, AVE, PVC)", "motor_A": "A-type motor neurons DA1-9, VA1-12", "motor_B": "B-type motor neurons DB1-7, VB1-11"},
        "counts": {"neurons": len(order), "chemical": nc, "electrical": len(tconn) - nc,
                   "by_role": dict(sorted(collections.Counter(role[n] for n in order).items()))},
        "neurons": [{"name": n, "class": neuron_class(n, names), "role": role[n], "category": cat(n)} for n in order],
        "connections": tconn,
    }
    # data file (for provenance)
    path = getattr(ds, "filename", None)
    if path and not os.path.isabs(path):
        path = os.path.join(os.path.dirname(cect.readers.Cook2019HermReader.__file__), path)
    try:
        import glob
        xl = glob.glob(os.path.join(os.path.dirname(cect.__file__), "data", "SI 5 Connectome adjacency matrices, corrected July 2020.xlsx"))[0]
        h = hashlib.sha256(open(xl, "rb").read()).hexdigest(); size = os.path.getsize(xl)
    except Exception:
        xl, h, size = None, "unknown", 0
    info = {"cect": cect.__version__, "xlsx": os.path.basename(xl) if xl else "unknown", "sha": h, "size": size, "n_conns": len(conns),
            "n_chem": len(chem), "n_gap_entries": len(gap), "n_elec": len(elec),
            "chem_self": sum(1 for (a, b) in chem if a == b), "elec_self": sum(1 for (a, b) in elec if a == b),
            "synapses": sum(chem.values()), "other": sorted(n for n in wn if cat(n) == "other"), "cats": whole["counts"]["by_category"],
            "t_chem_syn": sum(r[2] for r in tconn if r[3] == "chemical"), "t_elec_syn": sum(r[2] for r in tconn if r[3] == "electrical")}
    return touch, whole, provenance(touch, whole, info)


def provenance(touch, whole, i):
    tc = touch["counts"]
    sig = "~/.cache/spike-connectome/venv-cect/bin/python tools/connectome/worm_table.py"
    return f"""# Worm connectome excerpts: provenance

Written by `tools/connectome/worm_table.py`. Do not edit by hand; re-run the script (`{sig}`, and `--check` to verify).

## Dataset
- Name: C. elegans hermaphrodite whole-animal connectome, Cook et al. (2019).
- Paper: Cook SJ et al. (2019) Whole-animal connectomes of both Caenorhabditis elegans sexes. Nature 571:63-71, doi:10.1038/s41586-019-1352-7.
- Data file: `{i["xlsx"]}` ({i["size"]:,} bytes, sha256 `{i["sha"]}`), the hermaphrodite sheets "hermaphrodite chemical" and "herm gap jn symmetric", bundled inside the `cect` package.
- Reader: OpenWorm ConnectomeToolbox, PyPI package `cect` version {i["cect"]} (MIT licence per `tools/connectome/worm.py`), `cect.readers.Cook2019HermReader.get_instance().read_data()`. The data keep the terms of the original publication (as `tools/connectome/worm.py` states).
- Date written: {DATE}. Interpreter: `~/.cache/spike-connectome/venv-cect/bin/python`; nothing is downloaded by the script.
- What the reader returns: {i["n_conns"]} connection records between {whole["counts"]["neurons"]} neurons (the 302 neurons of the hermaphrodite as listed in the data; muscles and other non-neuronal targets are not in this reader's output): {i["n_chem"]} directed chemical pairs ({i["synapses"]} synapses in all, {i["chem_self"]} of the pairs are self-connections) and {i["n_gap_entries"]} gap-junction records. The gap-junction sheet is symmetric (every pair is listed in both directions with the same count, checked by the script); it holds {i["n_elec"]} distinct couplings ({i["elec_self"]} are self-couplings). Counts are whole numbers and are the numbers of the data; nothing is rescaled.

## touch.json: the touch-withdrawal excerpt
- Neurons ({tc["neurons"]}), left and right cells separate: touch receptors ALML, ALMR, AVM, PLML, PLMR (role `touch`); command interneurons AVAL/R, AVBL/R, AVDL/R, AVEL/R, PVCL/R (`command`); A-type motor neurons DA1-9 and VA1-12 (`motor_A`, 21); B-type motor neurons DB1-7 and VB1-11 (`motor_B`, 18).
- Connections: every chemical and every electrical connection of the data with both ends in this set, with its count. No threshold, nothing added. {tc["chemical"]} chemical ({i["t_chem_syn"]} synapses) and {tc["electrical"]} electrical ({i["t_elec_syn"]} in the counts, symmetric couplings listed once).
- Each neuron has name, `class` (trailing digits dropped, then a trailing L or R dropped when the other side exists: ALML -> ALM, DA3 -> DA, AVAL -> AVA; derived by the script, not a field of the data), `role` and `category` (the data set's own lists).
- Dropped: every cell outside the set, and all connections that have one end outside it.

## whole.json: every neuron
- Neurons: all {whole["counts"]["neurons"]} neurons of the reader's output, with name, class (same rule as above) and category. Category comes from the lists in `cect.Cells` (`SENSORY_NEURONS_COOK`, `INTERNEURONS_COOK`, `MOTORNEURONS_COOK`): {i["cats"]}. The {len(i["other"])} neurons in none of the three lists are `other`: {", ".join(i["other"])}.
- `chemical`: every directed chemical connection with its count ({whole["counts"]["chemical"]}). `electrical`: every gap-junction coupling once ({whole["counts"]["electrical"]}). Self-connections are kept; statistics that ignore them must say so.
- Format: connections are [pre_index, post_index, count] into `neurons`, one per line to keep the file small and its diffs readable.
- Dropped: nothing within the reader's output. Not in it: muscles, other non-neuronal cells, synapse positions and the neurotransmitter and sign (the reader's `synclass` field is not kept).

## Reproduce
```
{sig}            # write the three files
{sig} --check    # regenerate in memory and compare with the committed files
python3 tools/connectome/worm_report.py   # the numbers quoted in research/atlas-a.md (reads the committed tables only)
```
The script is deterministic: neurons and connections are sorted, nothing random.
"""


def dump_touch(t):
    lines = ["{"]
    keys = [k for k in t if k not in ("neurons", "connections")]
    for k in keys:
        lines.append(" %s: %s," % (json.dumps(k), json.dumps(t[k])))
    lines.append(' "neurons": [\n' + ",\n".join("  " + json.dumps(n) for n in t["neurons"]) + "\n ],")
    lines.append(' "connections": [\n' + ",\n".join("  " + json.dumps(c) for c in t["connections"]) + "\n ]")
    lines.append("}")
    return "\n".join(lines) + "\n"


def dump_whole(w):
    lines = ["{"]
    for k in w:
        if k in ("neurons", "chemical", "electrical"):
            continue
        lines.append(" %s: %s," % (json.dumps(k), json.dumps(w[k])))
    for k in ("neurons", "chemical", "electrical"):
        lines.append(' %s: [\n%s\n ]%s' % (json.dumps(k), ",\n".join("  " + json.dumps(x) for x in w[k]), "," if k != "electrical" else ""))
    lines.append("}")
    return "\n".join(lines) + "\n"


def main(argv):
    touch, whole, prov = build()
    files = {"touch.json": dump_touch(touch), "whole.json": dump_whole(whole), "PROVENANCE.md": prov}
    for k in ("touch.json", "whole.json"):
        json.loads(files[k])
    if "--check" in argv:
        bad = 0
        for fn, text in files.items():
            p = os.path.join(OUT, fn)
            same = os.path.exists(p) and open(p).read() == text
            print("%-14s %s" % (fn, "same" if same else "DIFFERS"))
            bad += not same
        sys.exit(1 if bad else 0)
    os.makedirs(OUT, exist_ok=True)
    for fn, text in files.items():
        with open(os.path.join(OUT, fn), "w") as f:
            f.write(text)
        print("wrote %s (%d bytes)" % (os.path.relpath(os.path.join(OUT, fn), ROOT), len(text.encode())))


if __name__ == "__main__":
    main(sys.argv[1:])
