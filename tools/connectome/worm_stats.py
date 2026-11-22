#!/usr/bin/env python3
"""Statistics of the C. elegans hermaphrodite connectome (Cook et al. 2019, read through `cect` 0.3.5) for research/atlas-a.md.
Needs the cect package: run with  ~/.cache/spike-connectome/venv-cect/bin/python tools/connectome/worm_stats.py
(python3 -m venv ~/.cache/spike-connectome/venv-cect && ~/.cache/spike-connectome/venv-cect/bin/pip install cect). Nothing is written to the repo.
Chemical edges are directed; gap junctions are symmetric. 'Edge' = a pair of cells with at least one synapse; 'count' = number of synapses.
"""
import collections, random, re, statistics, sys
from cect.readers.Cook2019HermReader import get_instance

cells, conns = get_instance().read_data()
chem = collections.defaultdict(float); gap = collections.defaultdict(float)
for c in conns:
    (chem if c.syntype == "Chemical" else gap)[(c.pre_cell, c.post_cell)] += float(c.number)
names = sorted(cells)
print("cells", len(names), "chemical edges (directed pairs)", len(chem), "synapses", int(sum(chem.values())), "| gap-junction entries (both directions listed)", len(gap), "contacts", int(sum(gap.values())))
motor_re = re.compile(r"^(DA|DB|DD|VA|VB|VD|AS|DB|VC)\d+$")
cls = collections.Counter(("motor (D/V/AS/VC series)" if motor_re.match(n) else "other") for n in names)
print("by name:", dict(cls))
touch = ["ALML", "ALMR", "AVM", "PLML", "PLMR"]
print("touch cells present:", [t for t in touch if t in names])
for n in ("AVAL", "AVAR", "AVBL", "AVBR", "AVDL", "AVDR", "PVCL", "PVCR", "AVEL", "AVER"):
    o = sum(1 for (a, b) in chem if a == n); i = sum(1 for (a, b) in chem if b == n)
    print("  %-5s chemical out-partners %3d in-partners %3d" % (n, o, i))
# hubs by total chemical degree
deg = collections.Counter()
for (a, b) in chem: deg[a] += 1; deg[b] += 1
print("top 10 by chemical degree (in + out partners):", deg.most_common(10))
# shortest chemical path from touch cells to A-type (backward) and B-type (forward) motor neurons
adj = collections.defaultdict(set)
for (a, b) in chem: adj[a].add(b)
def bfs(src, banned=()):
    d = {src: 0}; q = collections.deque([src])
    while q:
        u = q.popleft()
        for v in adj[u]:
            if v in banned or v in d: continue
            d[v] = d[u] + 1; q.append(v)
    return d
A = [n for n in names if re.match(r"^(DA|VA)\d+$", n)]; B = [n for n in names if re.match(r"^(DB|VB)\d+$", n)]
print("A-type motor neurons (DA, VA)", len(A), "B-type (DB, VB)", len(B))
for src in ("ALML", "AVM", "PLML"):
    d = bfs(src)
    print("  %-5s: to A-type min hops %s, to B-type min hops %s" % (src, min(d.get(a, 99) for a in A), min(d.get(b, 99) for b in B)))
# single-cell ablation: which one removal lengthens or cuts ALM > A-type within the shortest length
base = {s: min(bfs(s).get(a, 99) for a in A) for s in ("ALML", "ALMR", "AVM")}
print("shortest hops to A-type, touch cell:", base)
cut = []
for x in names:
    if x in touch: continue
    r = {s: min(bfs(s, banned={x}).get(a, 99) for a in A) for s in ("ALML", "ALMR", "AVM")}
    if any(r[s] > base[s] for s in r): cut.append((x, r))
print("single cells whose removal lengthens the shortest ALM/AVM > A-type chemical path (cell, new hops):", cut[:20], "(%d in all)" % len(cut))
# feed-forward loops (030T) in the chemical network
def ffl(edges):
    out = collections.defaultdict(set)
    for (a, b) in edges: 
        if a != b: out[a].add(b)
    n = 0
    for a in list(out):
        for b in out[a]:
            for c in out.get(b, ()):
                if c in out[a] and c != a: n += 1
    return n
E_ = list(chem)
real = ffl(E_)
def swap(edges, nswap, rng):
    E = list(edges); S = set(E); k = 0
    while k < nswap:
        i, j = rng.randrange(len(E)), rng.randrange(len(E))
        (a, b), (c, d) = E[i], E[j]
        if a == d or c == b or (a, d) in S or (c, b) in S or a == c or b == d: k += 1; continue
        S.discard((a, b)); S.discard((c, d)); S.add((a, d)); S.add((c, b)); E[i], E[j] = (a, d), (c, b); k += 1
    return E
rng = random.Random(1)
nul = [ffl(swap(E_, 10 * len(E_), rng)) for _ in range(100)]
print("feed-forward loops (a>b, b>c, a>c): real %d; degree-preserving edge swap (100 draws, 10 x edges swaps each, seed 1): mean %.0f sd %.1f, z = %.1f" % (real, statistics.mean(nul), statistics.pstdev(nul), (real - statistics.mean(nul)) / statistics.pstdev(nul)))
# Erdos-Renyi null (same number of edges between the same cells)
nodes = sorted({x for e in E_ for x in e}); m = len(E_)
er = []
for _ in range(100):
    s = set()
    while len(s) < m:
        a, b = rng.choice(nodes), rng.choice(nodes)
        if a != b: s.add((a, b))
    er.append(ffl(s))
print("  Erdos-Renyi null (same edge count, 100 draws): mean %.0f sd %.1f, z = %.1f" % (statistics.mean(er), statistics.pstdev(er), (real - statistics.mean(er)) / statistics.pstdev(er)))
