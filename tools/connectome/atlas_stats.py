#!/usr/bin/env python3
"""Statistics quoted in research/atlas-a.md, computed from the committed hemibrain tables (data/connectome/hemibrain/*.json). No cache needed.

Usage: python3 tools/connectome/atlas_stats.py [compass|mb|all]
"""
import collections, json, os, random, statistics, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.normpath(os.path.join(HERE, "..", "..", "data", "connectome", "hemibrain"))


def load(name):
    return json.load(open(os.path.join(DATA, name)))


def compass():
    d = load("compass.json")
    ns, cs = d["neurons"], d["connections"]
    ty = [n["type"] for n in ns]
    print("COMPASS  neurons by type", d["counts"], " connections", len(cs))
    # connection table by type pair: number of neuron pairs, total synapses
    pair = collections.defaultdict(lambda: [0, 0])
    for a, b, w in cs:
        p = pair[(ty[a], ty[b])]
        p[0] += 1; p[1] += w
    print("type pair: connected pairs, synapses")
    for k, v in sorted(pair.items()):
        print("  %-12s > %-12s %5d %6d" % (k[0], k[1], v[0], v[1]))
    # offsets around the ring. Glomerulus numbers of the two sides; PEN glomerulus 9 has the phase of 1 (ring = (g-1) mod 8)
    def off(src_t, dst_t):
        h = collections.defaultdict(float)
        tot = 0.0
        for a, b, w in cs:
            if ty[a] == src_t and ty[b] == dst_t:
                A, B = ns[a], ns[b]
                same = A["side"] == B["side"]
                o = (B["ring"] - A["ring"]) % 8
                h[(("same" if same else "opposite") + " side", o if o <= 4 else o - 8)] += w
                tot += w
        return h, tot
    for s, t in [("EPG", "PEN_a(PEN1)"), ("EPG", "PEN_b(PEN2)"), ("PEN_a(PEN1)", "EPG"), ("PEN_b(PEN2)", "EPG"), ("EPG", "PEG"), ("PEG", "EPG")]:
        h, tot = off(s, t)
        print("offset in ring slots (45 deg each), synapse-weighted, %s > %s (total %d synapses; slot = post.ring - pre.ring mod 8, in -3..4)" % (s, t, tot))
        for side in ("same side", "opposite side"):
            row = {o: h.get((side, o), 0) for o in range(-3, 5)}
            share = {o: round(100 * v / tot, 1) for o, v in row.items() if v}
            print("   %-13s %s" % (side, share))
    # the glomerulus offset (raw numbers), same hemisphere, for the shift direction: PEN glomerulus - EPG glomerulus in the EPG > PEN direction (PB)
    for pt in ("PEN_a(PEN1)", "PEN_b(PEN2)"):
        h = collections.Counter()
        for a, b, w in cs:
            if ty[a] == "EPG" and ty[b] == pt and ns[a]["side"] == ns[b]["side"]:
                h[(ns[a]["side"], ns[b]["glomerulus"] - ns[a]["glomerulus"])] += w
        print("raw glomerulus difference (PEN - EPG), EPG > %s, same side, synapses:" % pt, dict(sorted(h.items())))
        h = collections.Counter()
        for a, b, w in cs:
            if ty[a] == pt and ty[b] == "EPG" and ns[a]["side"] == ns[b]["side"]:
                h[(ns[a]["side"], ns[b]["glomerulus"] - ns[a]["glomerulus"])] += w
        print("raw glomerulus difference (EPG - PEN), %s > EPG, same side, synapses:" % pt, dict(sorted(h.items())))
    # Delta7: connections
    for t in ("Delta7",):
        for other in sorted(set(ty)):
            o = sum(w for a, b, w in cs if ty[a] == t and ty[b] == other)
            i = sum(w for a, b, w in cs if ty[b] == t and ty[a] == other)
            print("  Delta7 > %-12s %5d synapses ; %-12s > Delta7 %5d" % (other, o, other, i))
    # Delta7 specificity: do Delta7 neurons contact EPG in the glomeruli they cover? (synapse share by glomerulus phase distance from the Delta7's covered glomeruli)
    gl = collections.Counter(); allw = collections.Counter()
    for a, b, w in cs:
        if ty[a] == "Delta7" and ty[b] == "EPG":
            cov = {(s, g) for s, g in ns[a]["glomeruli"]}
            allw["Delta7>EPG"] += w
            gl["EPG glomerulus is one of the Delta7's own" if (ns[b]["side"], ns[b]["glomerulus"]) in cov else "other glomerulus"] += w
    print("Delta7 > EPG synapses by whether the EPG sits in a glomerulus the Delta7 covers:", dict(gl))
    # phase distance between pre-Delta7's covered ring slots and the EPG
    dist = collections.Counter()
    for a, b, w in cs:
        if ty[a] == "Delta7" and ty[b] == "EPG":
            slots = {(g - 1) % 8 for s, g in ns[a]["glomeruli"]}
            dd = min(min((ns[b]["ring"] - s) % 8, (s - ns[b]["ring"]) % 8) for s in slots)
            dist[dd] += w
    tot = sum(dist.values())
    print("Delta7 > EPG synapse share by ring-slot distance from the nearest slot the Delta7 covers (0..4):", {k: round(100 * v / tot, 1) for k, v in sorted(dist.items())})
    # per EPG: number of Delta7 inputs and PEN inputs, per type in-degree and out-degree (neurons and synapses)
    for t in d["types"]:
        idx = [i for i, x in enumerate(ty) if x == t]
        s = set(idx)
        for dr in ("in", "out"):
            deg = collections.Counter(); syn = collections.Counter()
            for a, b, w in cs:
                if dr == "in" and b in s: deg[b] += 1; syn[b] += w
                if dr == "out" and a in s: deg[a] += 1; syn[a] += w
            print("  %-12s %s: partners per neuron mean %.1f, synapses per neuron mean %.1f" % (t, dr, sum(deg.values()) / len(idx), sum(syn.values()) / len(idx)))


def mb(seed=1, n_shuffle=200):
    d = load("mushroom_body.json")
    ns, cs = d["neurons"], d["connections"]
    role = [n["role"] for n in ns]
    ty = [n["type"] for n in ns]
    print("MUSHROOM BODY  by role", d["counts_by_role"], " connections kept", len(cs), "of", d["n_before_threshold"])
    kcs = [i for i, r in enumerate(role) if r == "KC"]
    pns = [i for i, r in enumerate(role) if r == "PN"]
    kc_sub = collections.Counter(ty[i] for i in kcs)
    print("KC by type", dict(sorted(kc_sub.items())))
    glom = lambda t: t.rsplit("_", 1)[0]   # antennal-lobe glomerulus of a PN type
    # PN > KC edges (all counts kept)
    pk = [(a, b, w) for a, b, w in cs if role[a] == "PN" and role[b] == "KC"]
    print("PN>KC connections", len(pk), "synapses", sum(w for _, _, w in pk))
    inn = collections.defaultdict(set); insyn = collections.defaultdict(int)
    for a, b, w in pk:
        inn[b].add(a); insyn[b] += w
    kc_with = [len(inn.get(k, ())) for k in kcs]
    print("KC with no PN input", sum(1 for x in kc_with if x == 0), "of", len(kcs))
    h = collections.Counter(kc_with)
    print("PN inputs per KC: mean %.2f  median %.1f  sd %.2f  max %d" % (statistics.mean(kc_with), statistics.median(kc_with), statistics.pstdev(kc_with), max(kc_with)))
    print("   histogram (inputs: KCs):", dict(sorted(h.items())))
    print("   claw-count reference (Caron 2013: about 5-6 per KC for KCg, from the literature, not computed here)")
    for t in sorted(kc_sub):
        v = [len(inn.get(k, ())) for k in kcs if ty[k] == t]
        print("   %-12s n=%4d  mean PN inputs %.2f  (no input: %d)" % (t, len(v), statistics.mean(v), sum(1 for x in v if x == 0)))
    # fraction of KC input pairs that come from the same glomerulus (a KC listening twice to one glomerulus), against a shuffle
    def same_glom_frac(inp):
        pairs = tot = 0
        for k, S in inp.items():
            g = collections.Counter(glom(ty[p]) for p in S)
            # pairs of distinct PN neurons from the same glomerulus
            pairs += sum(c * (c - 1) // 2 for c in g.values())
            n = len(S); tot += n * (n - 1) // 2
        return pairs, tot
    real = same_glom_frac(inn)
    # null 1: each KC keeps its PN in-degree, sources drawn from all PNs with the real out-degree of each PN (configuration model by stub shuffling)
    rnd = random.Random(seed)
    outdeg = collections.Counter(a for a, b, w in pk)
    stubs = [p for p, c in outdeg.items() for _ in range(c)]
    indeg = {k: len(inn.get(k, ())) for k in kcs}
    null = []
    for _ in range(n_shuffle):
        rnd.shuffle(stubs)
        it = iter(stubs); sh = {}
        ok = True
        for k in kcs:
            S = set()
            for _ in range(indeg[k]):
                S.add(next(it))
            sh[k] = S
        null.append(same_glom_frac(sh)[0])
    m, sd = statistics.mean(null), statistics.pstdev(null)
    print("PN pairs from one glomerulus into the same KC: real %d (of %d pairs); degree-preserving stub shuffle (%d draws, seed %d): mean %.1f sd %.1f, z = %.2f" % (real[0], real[1], n_shuffle, seed, m, sd, (real[0] - m) / sd if sd else float('nan')))
    print("   (the stub shuffle keeps each KC's number of PN synapse-partners and each PN's total number of KC partners; collisions inside a KC are merged, so in-degrees can fall slightly)")
    # input-class bias: share of PN>KC synapses by PN glomerulus, per KC family (KCg vs KCab vs KCa'b')
    fam = lambda t: "KCg" if t.startswith("KCg") else ("KCab" if t.startswith("KCab") else "KCa'b'")
    share = collections.defaultdict(collections.Counter)
    for a, b, w in pk:
        share[fam(ty[b])][glom(ty[a])] += w
    for f, c in sorted(share.items()):
        tot = sum(c.values())
        print("   %s: PN>KC synapses %d; top glomeruli: %s" % (f, tot, [(g, round(100 * v / tot, 1)) for g, v in c.most_common(5)]))
    # APL
    apl = [i for i, r in enumerate(role) if r == "APL"][0]
    k2a = {a: w for a, b, w in cs if b == apl and role[a] == "KC"}
    a2k = {b: w for a, b, w in cs if a == apl and role[b] == "KC"}
    print("APL: KC>APL synapses mean %.1f (KCs: %d); APL>KC synapses mean %.1f (KCs: %d)" % (statistics.mean(k2a.values()), len(k2a), statistics.mean(a2k.values()), len(a2k)))
    for f in ("KCg", "KCab", "KCa'b'"):
        a = [k2a[k] for k in kcs if fam(ty[k]) == f and k in k2a]
        b = [a2k[k] for k in kcs if fam(ty[k]) == f and k in a2k]
        print("   %-7s KC>APL mean %.1f, APL>KC mean %.1f (n=%d)" % (f, statistics.mean(a), statistics.mean(b), len(a)))
    # MBON, DAN
    mbon = [i for i, r in enumerate(role) if r == "MBON"]
    dan = [i for i, r in enumerate(role) if r == "DAN"]
    print("MBON types", len({ty[i] for i in mbon}), "neurons", len(mbon), "; DAN neurons", len(dan), "types", len({ty[i] for i in dan}))
    kc_syn = collections.Counter()
    for a, b, w in cs:
        if role[a] == "KC" and role[b] == "MBON": kc_syn[ty[b]] += w
    print("KC>MBON synapses (>=5 per pair) by MBON type, top 10:", kc_syn.most_common(10))
    kd = collections.Counter()
    for a, b, w in cs:
        if role[a] == "DAN" and role[b] == "KC": kd[ty[a][:4]] += w
    print("DAN>KC synapses (>=5 per pair) by family:", dict(kd))
    # KC out-degree to MBON: how many KCs per MBON type (>=5 synapses)
    kcs_per = collections.defaultdict(set)
    for a, b, w in cs:
        if role[a] == "KC" and role[b] == "MBON": kcs_per[ty[b]].add(a)
    print("KCs per MBON type (pairs with >=5 synapses): ", {k: len(v) for k, v in sorted(kcs_per.items(), key=lambda x: -len(x[1]))[:8]})


if __name__ == "__main__":
    w = sys.argv[1] if len(sys.argv) > 1 else "all"
    if w in ("compass", "all"): compass()
    if w in ("mb", "all"): mb()
