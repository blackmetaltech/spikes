#!/usr/bin/env python3
"""Hemibrain v1.2 excerpts -> data/connectome/hemibrain/{compass.json, mushroom_body.json, PROVENANCE.md}.

DATA. Scheffer LK et al. (2020) A connectome and analysis of the adult Drosophila central brain. eLife 9:e57443, doi:10.7554/eLife.57443.
Dataset "hemibrain:v1.2" (neuPrint, Janelia FlyEM), public export "exported-traced-adjacencies-v1.2":
  https://storage.googleapis.com/hemibrain/v1.2/exported-traced-adjacencies-v1.2.tar.gz  (about 45 MB)
  traced-neurons.csv            bodyId, type, instance   (all traced, non-cropped neurons)
  traced-total-connections.csv  bodyId_pre, bodyId_post, weight   (weight = total synapse count of the pair)
The archive goes to a cache OUTSIDE the repo: ${XDG_CACHE_HOME:-~/.cache}/spike-connectome/hemibrain-v1.2/ . Only the small tables written here are committed.
Python standard library only. Output is sorted and deterministic.

Usage
  python3 tools/connectome/hemibrain.py            download if needed, extract, write the three files
  python3 tools/connectome/hemibrain.py --check    re-extract from the cache and compare with the committed files (exit 1 on a difference;
                                                   exit 0 with a note if the cache is absent)
  (statistics for research/atlas-a.md: tools/connectome/atlas_stats.py, reads the committed tables)

What the two tables contain: see PROVENANCE.md (written by this script).
"""
import collections, csv, datetime, hashlib, json, os, re, shutil, sys, tarfile, urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "data", "connectome", "hemibrain")
URL = "https://storage.googleapis.com/hemibrain/v1.2/exported-traced-adjacencies-v1.2.tar.gz"
ARCHIVE = "exported-traced-adjacencies-v1.2.tar.gz"
CACHE = os.path.join(os.environ.get("XDG_CACHE_HOME") or os.path.expanduser("~/.cache"), "spike-connectome", "hemibrain-v1.2")
EXTRACT = os.path.join(CACHE, "extracted")
SUB = "exported-traced-adjacencies-v1.2"

# --- the selections -------------------------------------------------------------------------------------------------------------------
COMPASS_TYPES = ["EPG", "PEN_a(PEN1)", "PEN_b(PEN2)", "PEG", "Delta7"]   # exact type strings of the file
# uniglomerular olfactory projection neurons: a glomerulus name of the antennal lobe (D*, V* except VP*, which are thermo/hygrosensory) + _adPN / _lPN / _ilPN;
# instance ends in _R (right hemisphere: they feed the right mushroom body). Multiglomerular (M_*) and VP* types are left out.
PN_RE = re.compile(r"^(DA|DC|DL|DM|DP|VA|VC|VL|VM)\d+[a-z]*_(adPN|lPN|ilPN)$")
KC_RE = re.compile(r"^KC")
MBON_RE = re.compile(r"^MBON")
DAN_RE = re.compile(r"^(PPL1|PAM)\d")
MB_MIN_WEIGHT = 5   # connections below this many synapses are dropped, except between the CORE role pairs (all counts kept): this keeps the file near 1 MB
MB_CORE_PAIRS = {("PN", "KC"), ("KC", "APL"), ("APL", "KC")}   # the fan-in and the feedback loop: the statistics of the chapter need every count


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def ensure_cache(download=True):
    """Return (sha256, download date, archive path) or None when the cache is absent and download is off."""
    arc = os.path.join(CACHE, ARCHIVE)
    rec = os.path.join(CACHE, "download.json")
    if not os.path.exists(arc):
        if not download:
            return None
        os.makedirs(CACHE, exist_ok=True)
        print("downloading", URL, "->", arc, file=sys.stderr)
        urllib.request.urlretrieve(URL, arc + ".part")
        os.replace(arc + ".part", arc)
        if os.path.exists(rec):
            os.remove(rec)
    if os.path.exists(rec):
        r = json.load(open(rec))
    else:
        r = {"sha256": sha256(arc), "date": datetime.date.fromtimestamp(os.path.getmtime(arc)).isoformat(), "size": os.path.getsize(arc)}
        json.dump(r, open(rec, "w"))
    if not os.path.exists(os.path.join(EXTRACT, SUB, "traced-neurons.csv")):
        shutil.rmtree(EXTRACT, ignore_errors=True)
        os.makedirs(EXTRACT)
        with tarfile.open(arc) as t:
            for m in t.getmembers():
                if m.isfile() and os.path.basename(m.name) in ("traced-neurons.csv", "traced-total-connections.csv", "README"):
                    m.name = os.path.join(SUB, os.path.basename(m.name))
                    t.extract(m, EXTRACT)   # names are rewritten to plain files under SUB
    return r["sha256"], r["date"], arc


def read_neurons():
    with open(os.path.join(EXTRACT, SUB, "traced-neurons.csv"), newline="") as f:
        return {int(r["bodyId"]): (r["type"], r["instance"]) for r in csv.DictReader(f)}


def read_connections(keep):
    """All (pre, post, weight) with both ends in `keep` (a set of body ids)."""
    out = []
    with open(os.path.join(EXTRACT, SUB, "traced-total-connections.csv"), newline="") as f:
        rd = csv.reader(f)
        next(rd)
        for a, b, w in rd:
            a = int(a)
            if a in keep:
                b = int(b)
                if b in keep:
                    out.append((a, b, int(w)))
    return out


# --- compass --------------------------------------------------------------------------------------------------------------------------
def parse_pb(instance):
    """'EPG(PB08)_L3' -> ('L', 3). 'PEN_a(PB06a)_R9' -> ('R', 9). 'Delta7(PB15)_L6R3_L' -> sides/glomeruli list and soma side.
    Returns dict(side, glom) for single-glomerulus types and dict(glomeruli=[[side, n], ...], soma=side) for Delta7."""
    m = re.match(r"^[^(]+\([^)]*\)_(.*)$", instance)
    tail = m.group(1)
    mm = re.match(r"^((?:[LR]\d+)+)_([LR])$", tail)
    if mm:
        return {"glomeruli": [[s, int(n)] for s, n in re.findall(r"([LR])(\d+)", mm.group(1))], "soma": mm.group(2)}
    mm = re.match(r"^([LR])(\d+)$", tail)
    return {"side": mm.group(1), "glom": int(mm.group(2))}


def ring_index(side, glom):
    """Position 0..15 on the protocerebral bridge, read left to right: left glomerulus g (L8 .. L1) is at 8 - g, right glomerulus g (R1 .. R8) at 7 + g, taken mod 16
    (so L9 is 15 and R9 is 0: the P-EN cells' extra ninth glomerulus sits next to the eighth of the other side). One step is 22.5 deg when the sixteen E-PG glomeruli
    are taken as the sixteen wedges of the ring. Evidence for this choice that the data themselves give: every Delta7 name lists glomeruli exactly eight positions apart
    (L2R7: 6 and 14; L8R1R9: 0, 8, 0; L1L9R8: 7, 15, 15), that is half a ring. The instances carry no ellipsoid-body wedge, so the order is derived, not read."""
    return (8 - glom) % 16 if side == "L" else (7 + glom) % 16


HEADING_SIGN, HEADING_OFFSET = -1, 3


def heading_index(side, glom):
    """Heading 0..15 (one step 22.5 deg) of a cell in bridge glomerulus (side, glom): right glomerulus g -> 2g, left glomerulus g -> (-2g + 3), both mod 16.
    FITTED, not read from a paper: of the 32 mappings h_R = 2g, h_L = (s*2g + o) mod 16 (s = +-1, o = 0..15), this one has the highest synapse-weighted mean cosine of the heading
    difference over E-PG > E-PG plus E-PG <> PEG connections (tools/connectome/compass_heading.py prints the scan). The two halves of the bridge then each cover the whole ring,
    mirrored and interleaved (right: even headings, left: odd). The mirror direction and the zero of heading are arbitrary."""
    return (2 * glom) % 16 if side == "R" else (HEADING_SIGN * 2 * glom + HEADING_OFFSET) % 16


def extract_compass(neurons):
    ids = sorted(b for b, (t, _) in neurons.items() if t in COMPASS_TYPES)
    ns = []
    for b in ids:
        t, inst = neurons[b]
        n = {"id": b, "type": t, "instance": inst}
        p = parse_pb(inst)
        if "glom" in p:
            n["side"] = p["side"]; n["glomerulus"] = p["glom"]; n["ring"] = ring_index(p["side"], p["glom"]); n["heading"] = heading_index(p["side"], p["glom"])
        else:
            n["soma_side"] = p["soma"]; n["glomeruli"] = p["glomeruli"]
        ns.append(n)
    ns.sort(key=lambda n: (COMPASS_TYPES.index(n["type"]), n.get("heading", -1), n.get("side", ""), n["id"]))
    index = {n["id"]: i for i, n in enumerate(ns)}
    conns = read_connections(set(index))
    conns = sorted((index[a], index[b], w) for a, b, w in conns)
    return {"neurons": ns, "connections": conns}


# --- mushroom body --------------------------------------------------------------------------------------------------------------------
def extract_mb(neurons):
    kc = {b for b, (t, i) in neurons.items() if KC_RE.match(t) and i.endswith("_R")}
    pn = {b for b, (t, i) in neurons.items() if PN_RE.match(t) and i.endswith("_R")}
    apl = {b for b, (t, i) in neurons.items() if t == "APL" and i.endswith("_R")}
    cand = {b for b, (t, i) in neurons.items() if (MBON_RE.match(t) or DAN_RE.match(t)) and i.endswith("_R")}
    # MBONs and dopamine neurons are kept when they have at least one synapse with a Kenyon cell, in either direction
    allconn = read_connections(kc | pn | apl | cand)
    touch = set()
    for a, b, w in allconn:
        if a in kc and b in cand: touch.add(b)
        if b in kc and a in cand: touch.add(a)
    ids = sorted(kc | pn | apl | touch)
    def role(b):
        t = neurons[b][0]
        if b in kc: return "KC"
        if b in pn: return "PN"
        if b in apl: return "APL"
        return "MBON" if MBON_RE.match(t) else "DAN"
    ns = sorted(({"id": b, "role": role(b), "type": neurons[b][0], "instance": neurons[b][1]} for b in ids),
                key=lambda n: (["PN", "KC", "APL", "MBON", "DAN"].index(n["role"]), n["type"], n["id"]))
    index = {n["id"]: i for i, n in enumerate(ns)}
    both = [(a, b, w) for a, b, w in allconn if a in index and b in index]
    conns = sorted((index[a], index[b], w) for a, b, w in both if w >= MB_MIN_WEIGHT or (role(a), role(b)) in MB_CORE_PAIRS)
    kept_by_pair, all_by_pair = collections.Counter(), collections.Counter()
    for a, b, w in both:
        all_by_pair["%s>%s" % (role(a), role(b))] += 1
        if w >= MB_MIN_WEIGHT or (role(a), role(b)) in MB_CORE_PAIRS:
            kept_by_pair["%s>%s" % (role(a), role(b))] += 1
    return {"neurons": ns, "connections": conns, "n_before_threshold": len(both),
            "pairs": {k: [kept_by_pair[k], all_by_pair[k]] for k in sorted(all_by_pair)}}


# --- output ---------------------------------------------------------------------------------------------------------------------------
def dumps(d):
    """Compact but readable: one neuron per line, connections 4000 triples per line-group (still deterministic)."""
    out = ["{"]
    keys = list(d.keys())
    for ki, k in enumerate(keys):
        v = d[k]
        comma = "," if ki < len(keys) - 1 else ""
        if k == "neurons":
            out.append(' "neurons": [')
            out.append(",\n".join("  " + json.dumps(n, separators=(",", ":")) for n in v))
            out.append(" ]" + comma)
        elif k == "connections":
            out.append(' "connections": [')
            rows = [json.dumps(c, separators=(",", ":")) for c in v]
            out.append(",\n".join("  " + ",".join(rows[i:i + 12]) for i in range(0, len(rows), 12)))
            out.append(" ]" + comma)
        else:
            out.append(" " + json.dumps(k) + ": " + json.dumps(v, separators=(",", ":"), sort_keys=True) + comma)
    out.append("}")
    return "\n".join(out) + "\n"


def source(sha, date):
    return {"dataset": "hemibrain:v1.2 (Scheffer et al. 2020, eLife 9:e57443)", "url": URL, "sha256": sha,
            "files": ["traced-neurons.csv", "traced-total-connections.csv"], "script": "tools/connectome/hemibrain.py",
            "license": "CC BY 4.0 (see PROVENANCE.md)"}


def counts(ns, key):
    return dict(sorted(collections.Counter(n[key] for n in ns).items()))


def build(download=True):
    c = ensure_cache(download)
    if c is None:
        return None
    sha, date, _ = c
    neurons = read_neurons()
    comp = extract_compass(neurons)
    mb = extract_mb(neurons)
    ctab = {"source": source(sha, date), "format": "connections are [pre_index, post_index, synapse_count] into neurons; neurons sorted by (type, heading, side, id)",
            "ring_note": "ring = 0..15 position along the protocerebral bridge, left to right (L g: 8-g, R g: 7+g, mod 16), one step 22.5 deg if the 16 glomeruli are the 16 ring positions; derived from the instance names, see ring_index() in tools/connectome/hemibrain.py; it is NOT the ring of headings (see heading_note)",
            "heading_note": "heading = 0..15 (22.5 deg each), fitted to the wiring: right glomerulus g -> 2g, left glomerulus g -> (3 - 2g), mod 16; the scan over 32 mappings is tools/connectome/compass_heading.py; not taken from a paper; mirror direction and zero are arbitrary; ring (position along the bridge) is kept",
            "types": COMPASS_TYPES, "counts": counts(comp["neurons"], "type"), "n_connections": len(comp["connections"]),
            "neurons": comp["neurons"], "connections": comp["connections"]}
    mtab = {"source": source(sha, date), "format": "connections are [pre_index, post_index, synapse_count] into neurons; neurons sorted by (role, type, id); right hemisphere only (instance ends _R)",
            "min_synapses": MB_MIN_WEIGHT, "core_pairs_all_counts": sorted("%s>%s" % p for p in MB_CORE_PAIRS),
            "connections_kept_and_total_by_role_pair": mb["pairs"], "counts_by_role": counts(mb["neurons"], "role"), "counts_by_type": counts(mb["neurons"], "type"),
            "n_connections": len(mb["connections"]), "n_before_threshold": mb["n_before_threshold"], "neurons": mb["neurons"], "connections": mb["connections"]}
    return {"compass.json": dumps(ctab), "mushroom_body.json": dumps(mtab), "PROVENANCE.md": provenance(sha, date, ctab, mtab, neurons)}, (ctab, mtab)


def provenance(sha, date, ctab, mtab, neurons):
    def table(counts_):
        return "\n".join("| %s | %d |" % (k, v) for k, v in counts_.items())
    ce = ctab["counts"]
    mbr = mtab["counts_by_role"]
    sig_c = "python3 tools/connectome/hemibrain.py"
    return f"""# Hemibrain excerpts: provenance

Written by `tools/connectome/hemibrain.py`. Do not edit by hand; re-run the script (`python3 tools/connectome/hemibrain.py`, and `--check` to verify).

## Dataset
- Name and version: hemibrain, release v1.2 (neuPrint dataset `hemibrain:v1.2`; the data are the "traced adjacencies" export).
- Paper: Scheffer LK et al. (2020) A connectome and analysis of the adult Drosophila central brain. eLife 9:e57443, doi:10.7554/eLife.57443.
- URL: {URL}
- File: `{ARCHIVE}`, 45,872,577 bytes. sha256 `{sha}`.
- Downloaded: {date} (to `${{XDG_CACHE_HOME:-~/.cache}}/spike-connectome/hemibrain-v1.2/`, outside the repo; not committed).
- Tables read from the archive: `traced-neurons.csv` (bodyId, type, instance) and `traced-total-connections.csv` (bodyId_pre, bodyId_post, weight). `weight` is the total synapse count of the pair, as the archive's README says ("'weight' corresponds to the total synapse count in the connections between each neuron pair"). The README states that the files came from `neuprint-python` v0.4.12, `fetch_traced_adjacencies`, on `hemibrain:v1.2`.
- Counts in the whole file: {len(neurons)} neurons (traced, non-cropped).

## Terms of use
- Janelia FlyEM's hemibrain page, https://www.janelia.org/project-team/flyem/hemibrain, states: "Hemibrain is licensed under CC-BY." and links to https://creativecommons.org/licenses/by/4.0/ (read 2026-11-22). The sentence does not name a version number; the link is to the 4.0 deed.
- This is the statement of the data's owner. It does not name release v1.2 specifically, and the archive's own README carries no licence text. Treated as: **CC BY 4.0, attribution required; confirmed for the dataset as a whole, not stated per release**.
- Attribution to put on a level card: "Hemibrain connectome v1.2, Scheffer et al. 2020 (eLife 9:e57443), Janelia FlyEM and Google; CC BY 4.0. Simplified for this game."

## Reproduce
```
{sig_c}            # download if needed, write the three files
{sig_c} --check    # compare the committed files with a fresh extraction (needs the cache)
python3 tools/connectome/atlas_stats.py   # the statistics quoted in research/atlas-a.md (reads the committed tables)
```
The script is deterministic: neurons and connections are sorted, nothing random.

## compass.json: the head-direction circuit
- Neurons: all neurons whose `type` is exactly one of: `EPG`, `PEN_a(PEN1)`, `PEN_b(PEN2)`, `PEG`, `Delta7` (the type strings of the file; the file also has `EPGt`, 4 neurons, which are not included).
- Connections: every connection of the file with both ends in this set, with its synapse count. No threshold. {ctab["n_connections"]} connections.
- Each neuron has body id, type, instance name. Instances look like `EPG(PB08)_L3` (type, tract, protocerebral-bridge glomerulus: side L or R and number), `Delta7(PB15)_L6R3_L` (glomeruli it covers, then the soma side). `ring` is the position along the protocerebral bridge from left to right, 0 to 15: left glomerulus g is at 8 - g, right glomerulus g at 7 + g, modulo 16 (E-PG sit in glomeruli 1 to 8 on each side, 16 in all; P-EN cells also sit in glomerulus 9, which wraps to the neighbour of the eighth glomerulus of the other side). This is a position along the bridge and NOT the ring of headings: the E-PG to E-PG wiring has no ring structure in it (synapse-weighted mean cosine of the position difference -0.012). `heading` (0 to 15, 22.5 degrees each) is the ring coordinate: right glomerulus g is heading 2g, left glomerulus g is heading 3 - 2g, modulo 16, so each half of the bridge covers the whole ring, the two halves are mirrored and interleaved. The mapping is FITTED to the wiring, not taken from a paper: of the 32 mappings with right 2g and left s*2g + o, it has the highest mean cosine summed over E-PG to E-PG and E-PG to/from PEG (0.895 and 0.859; the scan is printed by `tools/connectome/compass_heading.py`). The instance carries no ellipsoid-body wedge. Mirror direction and the zero of heading are arbitrary. Neurons are sorted by (type, heading, side, id), so the E-PG cells are in heading order. The committed `compass.json` was rewritten from its own fields by `tools/connectome/add_heading.py` (adds `heading`, re-sorts, renumbers the connections); `hemibrain.py --check` against the raw export was NOT re-run after this change.

| type | neurons |
|---|---|
{table(ce)}

## mushroom_body.json: the right mushroom body
- Right hemisphere only: neurons whose instance ends in `_R`.
- Projection neurons (`PN`): uniglomerular olfactory types, regular expression `{PN_RE.pattern}` (antennal-lobe glomerulus + `_adPN`/`_lPN`/`_ilPN`). Left out: multiglomerular `M_*` types, `VP*` (thermo- and hygrosensory) and `V_ilPN`.
- Kenyon cells (`KC`): every type starting `KC` (KCg-m, KCg-d, KCg-t, KCg-s1..s4, KCab-m, KCab-c, KCab-s, KCab-p, KCa'b'-m, KCa'b'-ap1, KCa'b'-ap2), right side.
- `APL`: the one APL neuron `APL_R`.
- MBONs (`MBON*`) and dopamine neurons (`PPL1*`, `PAM*`): right-side neurons with at least one synapse with a Kenyon cell in either direction.
- Connections: the full set between these neurons has {mtab["n_before_threshold"]} connections ({mtab["n_before_threshold"] - mtab["n_connections"]} more than the file keeps; the Kenyon-cell to Kenyon-cell connections alone are most of it). To keep the file near 1 MB, **a connection with fewer than {MB_MIN_WEIGHT} synapses is dropped**, except between the pairs PN to KC, KC to APL and APL to KC, where every count (1 and up) is kept because the fan-in and feedback statistics need them. The file keeps {mtab["n_connections"]} connections. Kept and total per role pair are in `connections_kept_and_total_by_role_pair` as [kept, total]. Strong (more than {MB_MIN_WEIGHT - 1} synapses) KC to KC, KC to MBON, dopamine neuron to KC and similar connections are all there; the many weak ones are not, so any statistic over those pairs is a statistic of the strong part only.
- Dropped: all other cell types (DPM, other dopamine neurons, left-side neurons); the synapses' positions; the regions (ROI) the synapses lie in (the `traced-roi-connections.csv` file is not used).

| role | neurons |
|---|---|
{table(mbr)}

By type: see `counts_by_type` in `mushroom_body.json`.
"""


def main():
    argv = sys.argv[1:]
    res = build(download="--check" not in argv)
    if res is None:
        print("cache absent: nothing to check (run without --check to download)")
        return 0
    files, _ = res
    if "--check" in argv:
        bad = 0
        for name, text in sorted(files.items()):
            p = os.path.join(OUT, name)
            if not os.path.exists(p) or open(p).read() != text:
                print("DIFFERENT:", p); bad += 1
            else:
                print("ok:", p)
        return 1 if bad else 0
    os.makedirs(OUT, exist_ok=True)
    for name, text in sorted(files.items()):
        with open(os.path.join(OUT, name), "w") as f:
            f.write(text)
        print("wrote", os.path.join(OUT, name), len(text), "bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
