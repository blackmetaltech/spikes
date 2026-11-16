"""3-node motif census and degree-preserving null models. Pure Python, same results as spike::analysis in C++.

Motif ids follow Milo et al. 2002 / mfinder (6, 12, 14, 36, 38, 46, 74, 78, 98, 102, 108, 110, 238): the minimum over
node orderings of the row-major 9-bit adjacency matrix. `index` 0..12 is the position in census vectors.
"""
from __future__ import annotations

import itertools
import math
from dataclasses import dataclass, field
from typing import Iterable, Sequence

MASK64 = (1 << 64) - 1


@dataclass(frozen=True)
class Motif:
    index: int
    milo_id: int
    man: str  # Holland-Leinhardt triad code, as in networkx.triads_census
    name: str
    edges: int
    has_mutual: bool


MOTIFS: tuple[Motif, ...] = tuple(
    Motif(i, *row)
    for i, row in enumerate(
        [
            (6, "021D", "fan_out", 2, False),
            (12, "021C", "chain", 2, False),
            (14, "111U", "mutual_pair_driving", 3, True),
            (36, "021U", "fan_in", 2, False),
            (38, "030T", "feed_forward_loop", 3, False),
            (46, "120U", "mutual_pair_co_driving", 4, True),
            (74, "111D", "mutual_pair_fed", 3, True),
            (78, "201", "two_mutual_pairs", 4, True),
            (98, "030C", "feedback_loop", 3, False),
            (102, "120C", "mutual_pair_in_loop", 4, True),
            (108, "120D", "mutual_pair_co_driven", 4, True),
            (110, "210", "mutual_pair_double", 5, True),
            (238, "300", "fully_connected", 6, True),
        ]
    )
)
BY_MILO_ID = {m.milo_id: m for m in MOTIFS}
BY_MAN = {m.man: m for m in MOTIFS}
N_CLASSES = len(MOTIFS)

_PAIRS = ((0, 1), (0, 2), (1, 0), (1, 2), (2, 0), (2, 1))


def _build_table() -> list[int]:
    table = []
    for code in range(64):
        edges = [_PAIRS[b] for b in range(6) if code >> b & 1]
        links = {frozenset(e) for e in edges}
        if len(links) < 2:
            table.append(-1)
            continue
        best = 1 << 9
        for p in itertools.permutations(range(3)):
            c = 0
            for i, j in edges:
                c |= 1 << (8 - (3 * p[i] + p[j]))
            best = min(best, c)
        table.append(BY_MILO_ID[best].index)
    return table


_TABLE = _build_table()


class Graph:
    """Simple directed graph: self-loops dropped, multi-edges merged. `out[a]` is a sorted list."""

    def __init__(self, n: int, out: list[list[int]]):
        self.n = n
        self.out = out
        self._sets = [set(o) for o in out]

    def has_edge(self, a: int, b: int) -> bool:
        return 0 <= a < self.n and b in self._sets[a]

    def edge_count(self) -> int:
        return sum(len(o) for o in self.out)


def make_graph(edges: Iterable[Sequence[float]], n: int | None = None, sign: str = "any") -> Graph:
    """edges: (pre, post[, weight]). sign: 'any' | 'E' (weight > 0) | 'I' (weight < 0)."""
    edges = [tuple(e) for e in edges]
    if n is None:
        n = max((max(int(e[0]), int(e[1])) + 1 for e in edges), default=0)
    out: list[set[int]] = [set() for _ in range(n)]
    for e in edges:
        a, b = int(e[0]), int(e[1])
        w = float(e[2]) if len(e) > 2 else 1.0
        if a == b or not (0 <= a < n and 0 <= b < n):
            continue
        if sign == "E" and not w > 0:
            continue
        if sign == "I" and not w < 0:
            continue
        out[a].add(b)
    return Graph(n, [sorted(s) for s in out])


def census(g_or_edges, n: int | None = None, sign: str = "any") -> list[int]:
    """Counts of induced connected 3-node subgraphs per class (index order of MOTIFS)."""
    g = g_or_edges if isinstance(g_or_edges, Graph) else make_graph(g_or_edges, n, sign)
    nb: list[set[int]] = [set() for _ in range(g.n)]
    for a in range(g.n):
        for b in g.out[a]:
            nb[a].add(b)
            nb[b].add(a)
    counts = [0] * N_CLASSES
    for v in range(g.n):
        N = sorted(nb[v])
        for i in range(len(N)):
            u = N[i]
            for j in range(i + 1, len(N)):
                w = N[j]
                if (g.has_edge(u, w) or g.has_edge(w, u)) and v > u:
                    continue  # triangle: counted once, at its smallest node
                node = (v, u, w)
                code = 0
                for b, (x, y) in enumerate(_PAIRS):
                    if g.has_edge(node[x], node[y]):
                        code |= 1 << b
                counts[_TABLE[code]] += 1
    return counts


class SplitMix64:
    """Same generator as the C++ side, so nulls are identical for a given seed."""

    def __init__(self, seed: int):
        self.s = seed & MASK64

    def next(self) -> int:
        self.s = (self.s + 0x9E3779B97F4A7C15) & MASK64
        z = self.s
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & MASK64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & MASK64
        return z ^ (z >> 31)


def rewire(g: Graph, seed: int, swaps_per_edge: int = 10, stats: dict | None = None) -> Graph:
    """Degree-preserving edge-swap randomisation (no self-loops or multi-edges). `stats`, if given, accumulates
    {"attempts", "accepted"} swap counts (the C++ RewireStats)."""
    e = [(a, b) for a in range(g.n) for b in g.out[a]]
    present = set(e)
    m = len(e)
    if m >= 2:
        rng = SplitMix64(seed)
        for _ in range(m * max(swaps_per_edge, 0)):
            i = rng.next() % m
            j = rng.next() % m
            if stats is not None:
                stats["attempts"] = stats.get("attempts", 0) + 1
            a, b = e[i]
            c, d = e[j]
            if a == c or b == d or a == d or c == b:
                continue
            if (a, d) in present or (c, b) in present:
                continue
            if stats is not None:
                stats["accepted"] = stats.get("accepted", 0) + 1
            present.discard((a, b))
            present.discard((c, d))
            present.add((a, d))
            present.add((c, b))
            e[i] = (a, d)
            e[j] = (c, b)
    out: list[list[int]] = [[] for _ in range(g.n)]
    for a, b in e:
        out[a].append(b)
    return Graph(g.n, [sorted(o) for o in out])


@dataclass
class MotifResult:
    counts: list[int]
    null_mean: list[float]
    null_sd: list[float]
    z: list[float]
    null_samples: int = 0

    def table(self):
        """Rows of (index, milo_id, man, name, count, null_mean, null_sd, z)."""
        return [
            (m.index, m.milo_id, m.man, m.name, self.counts[m.index], self.null_mean[m.index], self.null_sd[m.index], self.z[m.index])
            for m in MOTIFS
        ]

    def by_name(self) -> dict[str, dict]:
        return {r[3]: dict(count=r[4], null_mean=r[5], null_sd=r[6], z=r[7]) for r in self.table()}


def zscores(g_or_edges, n: int | None = None, sign: str = "any", n_null: int = 100, seed: int = 1, swaps_per_edge: int = 10) -> MotifResult:
    """Census plus z-scores against `n_null` rewired graphs (null k uses seed + k)."""
    g = g_or_edges if isinstance(g_or_edges, Graph) else make_graph(g_or_edges, n, sign)
    counts = census(g)
    nulls = [census(rewire(g, seed + k, swaps_per_edge)) for k in range(n_null)]
    mean, sd, z = [], [], []
    for c in range(N_CLASSES):
        xs = [float(x[c]) for x in nulls]
        mu = sum(xs) / n_null if n_null else 0.0
        var = 0.0
        for x in xs:
            var += (x - mu) * (x - mu)
        s = math.sqrt(var / (n_null - 1)) if n_null > 1 else 0.0
        diff = counts[c] - mu
        if s > 0:
            zc = diff / s
        elif diff == 0 or n_null == 0:
            zc = 0.0
        else:
            zc = math.inf if diff > 0 else -math.inf
        mean.append(mu)
        sd.append(s)
        z.append(zc)
    return MotifResult(counts, mean, sd, z, n_null)


def edges_from_nest(net) -> list[tuple[int, int, float]]:
    """Connection graph of a built PyNEST network (`spike.build`): (pre, post, weight) between circuit neurons."""
    import nest

    first = net.neurons.tolist()[0]
    n = net.n_neurons
    conns = nest.GetConnections()
    if len(conns) == 0:
        return []
    src = conns.get("source")
    tgt = conns.get("target")
    w = conns.get("weight")
    if not isinstance(src, list):
        src, tgt, w = [src], [tgt], [w]
    out = []
    for s, t, wt in zip(src, tgt, w):
        a, b = s - first, t - first
        if 0 <= a < n and 0 <= b < n:
            out.append((a, b, float(wt)))
    return out


# ---------------------------------------------------------------- 4-node census (199 connected directed classes)

MIN_NULL_SD = 0.5
SIGNIFICANT_Z = 2.0
MIN_SWAP_ACCEPTANCE = 0.2

_P4 = ((0, 1), (0, 2), (0, 3), (1, 0), (1, 2), (1, 3), (2, 0), (2, 1), (2, 3), (3, 0), (3, 1), (3, 2))


def _build_table4():
    perms = list(itertools.permutations(range(4)))
    canon = []
    for code in range(4096):
        edges = [_P4[b] for b in range(12) if code >> b & 1]
        parent = list(range(4))

        def find(x):
            while parent[x] != x:
                x = parent[x]
            return x

        for a, b in edges:
            parent[find(a)] = find(b)
        if len({find(i) for i in range(4)}) != 1:
            canon.append(-1)
            continue
        canon.append(min(sum(1 << (15 - (4 * p[a] + p[b])) for a, b in edges) for p in perms))
    ids = sorted({c for c in canon if c >= 0})
    index = {c: i for i, c in enumerate(ids)}
    return ids, [index.get(c, -1) for c in canon]


IDS4, _TABLE4 = _build_table4()
N_CLASSES4 = len(IDS4)  # 199
assert N_CLASSES4 == 199


@dataclass(frozen=True)
class Motif4:
    index: int
    id: int
    edges: int
    mutual_pairs: int


def _info4(i: int, cid: int) -> Motif4:
    bit = lambda a, b: cid >> (15 - (4 * a + b)) & 1
    edges = sum(bit(a, b) for a in range(4) for b in range(4) if a != b)
    mutual = sum(1 for a in range(4) for b in range(a + 1, 4) if bit(a, b) and bit(b, a))
    return Motif4(i, cid, edges, mutual)


MOTIFS4: tuple[Motif4, ...] = tuple(_info4(i, c) for i, c in enumerate(IDS4))
BY_ID4 = {m.id: m for m in MOTIFS4}


def census4(g_or_edges, n: int | None = None, sign: str = "any") -> list[int]:
    """Counts of induced weakly connected 4-node subgraphs per class (ESU enumeration, Wernicke 2006)."""
    g = g_or_edges if isinstance(g_or_edges, Graph) else make_graph(g_or_edges, n, sign)
    nb: list[list[int]] = [[] for _ in range(g.n)]
    for a in range(g.n):
        for b in g.out[a]:
            nb[a].append(b)
            nb[b].append(a)
    nb = [sorted(set(x)) for x in nb]
    cover = [0] * g.n
    counts = [0] * N_CLASSES4
    sub = [0, 0, 0, 0]
    sets = g._sets

    def classify():
        code = 0
        for b, (x, y) in enumerate(_P4):
            if sub[y] in sets[sub[x]]:
                code |= 1 << b
        counts[_TABLE4[code]] += 1

    def add(x, d):
        cover[x] += d
        for y in nb[x]:
            cover[y] += d

    def extend(k, ext, v):
        if k == 3:
            for w in ext:
                sub[3] = w
                classify()
            return
        ext = list(ext)
        while ext:
            w = ext.pop()
            ext2 = ext + [u for u in nb[w] if u > v and cover[u] == 0]
            sub[k] = w
            add(w, 1)
            extend(k + 1, ext2, v)
            add(w, -1)

    for v in range(g.n):
        sub[0] = v
        add(v, 1)
        extend(1, [u for u in nb[v] if u > v], v)
        add(v, -1)
    return counts


@dataclass
class MotifResult4:
    counts: list[int]
    null_mean: list[float]
    null_sd: list[float]
    z: list[float]
    null_samples: int = 0
    node_count: int = 0
    edge_count: int = 0
    swap_attempts: int = 0
    swap_accepted: int = 0
    swap_acceptance: float = 0.0
    effective_swaps_per_edge: float = 0.0
    degenerate: bool = False
    significant: list[int] = field(default_factory=list)


def zscores4(g_or_edges, n: int | None = None, sign: str = "any", n_null: int = 100, seed: int = 1, swaps_per_edge: int = 10) -> MotifResult4:
    """4-node census plus z-scores against `n_null` rewired graphs (null k uses seed + k), with the same significance rule as 3 nodes."""
    g = g_or_edges if isinstance(g_or_edges, Graph) else make_graph(g_or_edges, n, sign)
    counts = census4(g)
    st: dict = {}
    nulls = [census4(rewire(g, seed + k, swaps_per_edge, st)) for k in range(n_null)]
    att, acc = st.get("attempts", 0), st.get("accepted", 0)
    m = g.edge_count()
    r = MotifResult4(counts, [], [], [], n_null, g.n, m, att, acc)
    r.swap_acceptance = acc / att if att else 0.0
    r.effective_swaps_per_edge = acc / n_null / m if n_null > 0 and m > 0 else 0.0
    r.degenerate = n_null > 0 and r.swap_acceptance < MIN_SWAP_ACCEPTANCE
    for c in range(N_CLASSES4):
        xs = [float(x[c]) for x in nulls]
        mu = sum(xs) / n_null if n_null else 0.0
        var = 0.0
        for x in xs:
            var += (x - mu) * (x - mu)
        s = math.sqrt(var / (n_null - 1)) if n_null > 1 else 0.0
        diff = counts[c] - mu
        if s > 0:
            zc = diff / s
        elif diff == 0 or n_null == 0:
            zc = 0.0
        else:
            zc = math.inf if diff > 0 else -math.inf
        r.null_mean.append(mu)
        r.null_sd.append(s)
        r.z.append(zc)
        r.significant.append(int(math.isfinite(zc) and abs(zc) > SIGNIFICANT_Z and s >= MIN_NULL_SD and not r.degenerate))
    return r
