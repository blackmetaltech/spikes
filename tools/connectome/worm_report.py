#!/usr/bin/env python3
"""Numbers for the worm chapter ("Reading wiring"), from the committed tables only (no cect needed):
  data/connectome/worm/touch.json   the touch-withdrawal excerpt, cell level
  data/connectome/worm/whole.json   every neuron, chemical connections, electrical connections
Plain Python, one process:  python3 tools/connectome/worm_report.py [shuffles]     (default 100 shuffles per null model, seed 1)
Printed tables are pasted into research/atlas-a.md.
"""
import collections, json, os, random, statistics, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
D = os.path.join(ROOT, "data", "connectome", "worm")
touch = json.load(open(os.path.join(D, "touch.json")))
whole = json.load(open(os.path.join(D, "whole.json")))
NSHUF = int(sys.argv[1]) if len(sys.argv) > 1 else 100

CLASSES = ["ALM", "AVM", "PLM", "AVA", "AVB", "AVD", "AVE", "PVC", "DA", "VA", "DB", "VB"]
INTER = ["AVA", "AVB", "AVD", "AVE", "PVC"]
ANT, POST, A, B = ["ALM", "AVM"], ["PLM"], ["DA", "VA"], ["DB", "VB"]
THRESH = [1, 2, 3, 5, 10]

# ---------------------------------------------------------------- a. class-level matrices of touch.json
tn = touch["neurons"]
cls_of = [n["class"] for n in tn]
assert sorted(set(cls_of)) == sorted(CLASSES)
chem = collections.Counter(); elec = collections.Counter()
for i, j, n, kind in touch["connections"]:
    if kind == "chemical":
        chem[(cls_of[i], cls_of[j])] += n
    else:  # symmetric, stored once: expand to both ordered pairs (a self-coupling of one cell counts once)
        elec[(cls_of[i], cls_of[j])] += n
        if i != j:
            elec[(cls_of[j], cls_of[i])] += n


def table(m, title):
    print(title + "\n")
    print("| pre \\ post | " + " | ".join(CLASSES) + " |")
    print("|---|" + "---:|" * len(CLASSES))
    for a in CLASSES:
        print("| %s | " % a + " | ".join(str(m[(a, b)]) if m[(a, b)] else "." for b in CLASSES) + " |")
    print()


print("### a. Class-level matrices of touch.json (rows presynaptic, columns postsynaptic; '.' = none)\n")
print("Cells: %d. Chemical connections between cells: %d (%d synapses). Electrical couplings: %d (count total %d, each coupling listed once)." % (
    len(tn), touch["counts"]["chemical"], sum(chem.values()), touch["counts"]["electrical"], sum(n for i, j, n, k in touch["connections"] if k == "electrical")))
print("Class cell counts: " + ", ".join("%s %d" % (c, cls_of.count(c)) for c in CLASSES) + "\n")
table(chem, "Chemical synapse counts summed over cell pairs:")
table(elec, "Electrical counts (the matrix is symmetric; a coupling inside a class, such as AVAL-AVAR, appears twice in its diagonal cell):")

# ---------------------------------------------------------------- b. paths at thresholds
def graph(thr, with_elec):
    tot = collections.Counter(chem)
    if with_elec:
        for k, v in elec.items():
            tot[k] += v
    adj = collections.defaultdict(set)
    for (a, b), v in tot.items():
        if v >= thr and a != b:
            adj[a].add(b)
    return adj


def bfs(adj, srcs, banned=()):
    d = {s: 0 for s in srcs if s not in banned}
    q = collections.deque(d)
    while q:
        u = q.popleft()
        for v in adj[u]:
            if v not in d and v not in banned:
                d[v] = d[u] + 1
                q.append(v)
    return d


def reverse(adj):
    r = collections.defaultdict(set)
    for a, bs in adj.items():
        for b in bs:
            r[b].add(a)
    return r


def analyse(adj, srcs, tgts):
    ds = bfs(adj, srcs)
    hops = min([ds[t] for t in tgts if t in ds], default=None)
    if hops is None:
        return None, [], {c: True for c in INTER}
    dt = bfs(reverse(adj), tgts)
    on = [c for c in INTER if c in ds and c in dt and ds[c] + dt[c] == hops]
    cut = {}
    for c in INTER:
        d2 = bfs(adj, srcs, banned={c})
        cut[c] = not any(t in d2 for t in tgts)
    return hops, on, cut


print("### b. Shortest class-level paths at synapse thresholds\n")
print("A class-level edge is kept when its summed count (over the cell pairs of the two classes, self-connections within a cell excluded) is at least t. 'hops' = fewest edges from any source class to any target class. 'on a shortest path' = interneuron classes with distance from source plus distance to target equal to the hop count. 'cut by' = interneuron classes whose removal alone leaves no path.\n")
for with_elec, lab in ((False, "chemical only"), (True, "chemical plus electrical")):
    print("**%s**\n" % lab)
    print("| t | edges | anterior touch (ALM, AVM) to A-type: hops | interneurons on a shortest path | cut by | posterior touch (PLM) to B-type: hops | interneurons on a shortest path | cut by | ALM, AVM to B: hops | PLM to A: hops |")
    print("|---:|---:|---|---|---|---|---|---|---|---|")
    for t in THRESH:
        adj = graph(t, with_elec)
        ne = sum(len(v) for v in adj.values())
        h1, on1, c1 = analyse(adj, ANT, A)
        h2, on2, c2 = analyse(adj, POST, B)
        h3 = analyse(adj, ANT, B)[0]
        h4 = analyse(adj, POST, A)[0]
        f = lambda h: "none" if h is None else str(h)
        g = lambda xs: ", ".join(xs) if xs else "none"
        cut = lambda c, h: "no path" if h is None else g([k for k, v in c.items() if v])
        print("| %d | %d | %s | %s | %s | %s | %s | %s | %s | %s |" % (t, ne, f(h1), g(on1), cut(c1, h1), f(h2), g(on2), cut(c2, h2), f(h3), f(h4)))
    print()

# ---------------------------------------------------------------- c. motifs on whole.json
names = [n["name"] for n in whole["neurons"]]
cat = [n["category"] for n in whole["neurons"]]
N = len(names)
edges = sorted({(i, j) for i, j, w in whole["chemical"] if i != j})
selfl = sum(1 for i, j, w in whole["chemical"] if i == j)
M = len(edges)


def census(es):
    """Induced 030T (feed-forward loop) and 030C (3-cycle) counts of a directed graph without self-loops."""
    out = collections.defaultdict(set)
    for a, b in es:
        out[a].add(b)
    ffl = cyc = 0
    for a, b in es:
        for c in out[b]:
            if c == a:
                continue
            ab, bc = True, True
            if c in out[a]:   # a>b, b>c, a>c: transitive if there is no edge back on any pair
                if a not in out[b] and b not in out[c] and a not in out[c]:
                    ffl += 1
            elif a in out[c] and a not in out[b] and b not in out[c] and c not in out[a]:   # a>b, b>c, c>a, no reciprocal
                cyc += 1
    return ffl, cyc // 3


def swap_null(es, rng, same_block):
    E = list(es); S = set(E); m = len(E); ok = 0
    for _ in range(10 * m):
        x, y = rng.randrange(m), rng.randrange(m)
        (a, b), (c, d) = E[x], E[y]
        if x == y or a == c or b == d or a == d or c == b or (a, d) in S or (c, b) in S:
            continue
        if same_block and not (cat[b] == cat[d] or cat[a] == cat[c]):
            continue
        S.discard((a, b)); S.discard((c, d)); S.add((a, d)); S.add((c, b)); E[x], E[y] = (a, d), (c, b); ok += 1
    return E, ok


def random_null(m, rng):
    s = set()
    while len(s) < m:
        a, b = rng.randrange(N), rng.randrange(N)
        if a != b:
            s.add((a, b))
    return s


def blocks(es):
    return collections.Counter((cat[a], cat[b]) for a, b in es)


print("### c. Motifs in the whole chemical network (cell level, binarised)\n")
real = census(edges)
print("Nodes %d (of which %d have at least one chemical edge), directed edges %d. %d self-connections (a cell onto itself) are left out of the graph and of every null model." % (
    N, len({x for e in edges for x in e}), M, selfl))
print("Motifs are induced triads, as in the standard triad census: 030T = a>b, b>c, a>c and no other edge among the three; 030C = a>b>c>a and no other edge. (An undirected count that ignores reciprocal edges, as `worm_stats.py` did, is larger.)")
print("Categories (from the data set's own lists, stored in whole.json): " + ", ".join("%s %d" % kv for kv in sorted(collections.Counter(cat).items())) + ". The 7 neurons in none of the sensory, interneuron and motor lists form a fourth block, 'other'.")
print("Observed: 030T (feed-forward loop) %d, 030C (3-cycle) %d.\n" % real)
print("Null models, %d shuffles each, seed 1 (one generator, models run in the order i, ii, iii):" % NSHUF)
rng = random.Random(1)
res = {}
real_blocks = blocks(edges)
accepted = {}
for name in ("i", "ii", "iii"):
    vals = []; acc = []
    for _ in range(NSHUF):
        if name == "i":
            es = random_null(M, rng)
        else:
            es, ok = swap_null(edges, rng, name == "iii"); acc.append(ok)
            assert blocks(es) == real_blocks if name == "iii" else True
            assert collections.Counter(a for a, b in es) == collections.Counter(a for a, b in edges)
            assert collections.Counter(b for a, b in es) == collections.Counter(b for a, b in edges)
        vals.append(census(es))
    res[name] = vals
    accepted[name] = statistics.mean(acc) if acc else None
labels = {"i": "(i) same number of edges placed at random", "ii": "(ii) degree-preserving edge swaps (in- and out-degree of every cell kept)",
          "iii": "(iii) degree-preserving swaps that also keep the block counts (pre category to post category)"}
print()
print("| null model | motif | observed | null mean | null sd | z |")
print("|---|---|---:|---:|---:|---:|")
for name in ("i", "ii", "iii"):
    for k, mot in enumerate(("030T feed-forward loop", "030C 3-cycle")):
        v = [x[k] for x in res[name]]
        mu, sd = statistics.mean(v), statistics.stdev(v)
        z = (real[k] - mu) / sd if sd > 0 else float("nan")
        print("| %s | %s | %d | %.2f | %.2f | %.2f |" % (labels[name], mot, real[k], mu, sd, z))
print("\nSwap attempts per shuffle: %d (10 x edges); mean accepted swaps: (ii) %.0f, (iii) %.0f. sd is the sample standard deviation over the %d shuffles.\n" % (10 * M, accepted["ii"], accepted["iii"], NSHUF))
print("Block counts of the observed network (rows pre category, columns post category); model (iii) keeps every one of them:\n")
cats = ["sensory", "interneuron", "motor", "other"]
print("| pre \\ post | " + " | ".join(cats) + " |\n|---|" + "---:|" * 4)
for a in cats:
    print("| %s | " % a + " | ".join(str(real_blocks[(a, b)]) for b in cats) + " |")
print()

# ---------------------------------------------------------------- d. hubs
print("### d. Ten cells of highest total chemical degree\n")
outd = collections.Counter(a for a, b in edges); ind = collections.Counter(b for a, b in edges)
syn_out = collections.Counter(); syn_in = collections.Counter()
for i, j, w in whole["chemical"]:
    if i != j:
        syn_out[i] += w; syn_in[j] += w
deg = {i: outd[i] + ind[i] for i in range(N)}
top = sorted(range(N), key=lambda i: (-deg[i], names[i]))[:10]
print("Degree = number of distinct chemical partners out plus in (self-connections excluded); ties broken by name.\n")
print("| rank | cell | class | category | degree | out | in | synapses out | synapses in |\n|---:|---|---|---|---:|---:|---:|---:|---:|")
for r, i in enumerate(top, 1):
    print("| %d | %s | %s | %s | %d | %d | %d | %d | %d |" % (r, names[i], whole["neurons"][i]["class"], cat[i], deg[i], outd[i], ind[i], syn_out[i], syn_in[i]))
