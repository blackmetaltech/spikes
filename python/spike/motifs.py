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


def rewire(g: Graph, seed: int, swaps_per_edge: int = 10) -> Graph:
    """Degree-preserving edge-swap randomisation (no self-loops or multi-edges)."""
    e = [(a, b) for a in range(g.n) for b in g.out[a]]
    present = set(e)
    m = len(e)
    if m >= 2:
        rng = SplitMix64(seed)
        for _ in range(m * max(swaps_per_edge, 0)):
            i = rng.next() % m
            j = rng.next() % m
            a, b = e[i]
            c, d = e[j]
            if a == c or b == d or a == d or c == b:
                continue
            if (a, d) in present or (c, b) in present:
                continue
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
