"""Build a circuit JSON (docs/CIRCUIT-FORMAT.md) as a PyNEST network identical to the C++ engine's, and run it.

Creation / connection order mirrors engine/src/circuit.h so node ids and RNG streams match:
  1. populations in file order (neuron i = node id i+1), 2. one device per input, 3. one spike_recorder,
  then connections: projections in file order, inputs in file order (delay default = resolution), recorder
  (weight 1, delay = resolution).
"""
from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import numpy as np


class CircuitError(ValueError):
    pass


@dataclass
class Spikes:
    """Spikes sorted by time then neuron: circuit-level neuron index and time in ms."""

    neuron: np.ndarray
    t_ms: np.ndarray

    def __len__(self):
        return len(self.neuron)

    def pairs(self) -> list[tuple[int, float]]:
        return list(zip(self.neuron.tolist(), self.t_ms.tolist()))

    def steps(self, resolution_ms: float) -> list[tuple[int, int]]:
        """(step, neuron) pairs: exact comparison key (times are multiples of the resolution)."""
        return sorted(zip(np.rint(self.t_ms / resolution_ms).astype(int).tolist(), self.neuron.tolist()))


@dataclass
class Network:
    spec: dict
    resolution_ms: float
    seed: int
    threads: int
    n_neurons: int
    neurons: Any            # nest.NodeCollection of all circuit neurons
    populations: dict       # id -> (first_index, n, NodeCollection)
    devices: dict           # input name -> NodeCollection
    recorder: Any
    time_ms: float = 0.0
    _read: int = 0

    def population(self, pop_id: str):
        return self.populations[pop_id][2]

    def population_slice(self, pop_id: str) -> range:
        first, n, _ = self.populations[pop_id]
        return range(first, first + n)

    def set_input(self, name: str, value: float):
        import nest

        kind = self.spec["inputs"][[i["name"] for i in self.spec["inputs"]].index(name)]["kind"]
        self.devices[name].set({"amplitude" if kind == "dc" else "rate": float(value)})


def load_circuit(circuit) -> dict:
    """Accept a JSON string, a dict, or a path to a .json file."""
    if isinstance(circuit, dict):
        return circuit
    if isinstance(circuit, os.PathLike) or (isinstance(circuit, str) and not circuit.lstrip().startswith("{") and os.path.exists(circuit)):
        return json.loads(Path(circuit).read_text())
    return json.loads(circuit)


def _selector(spec, pops, sel, what) -> list[int]:
    pid, idx = (sel, None) if isinstance(sel, str) else (sel["pop"], sel.get("idx"))
    if pid not in pops:
        raise CircuitError(f"{what}: unknown population '{pid}'")
    first, n, _ = pops[pid]
    return [first + i for i in (range(n) if idx is None else idx)]


def _import_nest():
    try:
        import nest  # noqa: F401
    except ImportError as e:  # pragma: no cover
        raise ImportError(
            "PyNEST is not importable. Put the NEST build's site-packages on PYTHONPATH, e.g. "
            "third_party/nest-install/lib/python3.*/site-packages (see scripts/build-nest.sh)."
        ) from e
    import nest

    return nest


def build(circuit, threads: int = 1, seed: int | None = None, verbose: bool = False) -> Network:
    """Build the circuit in PyNEST (resets the NEST kernel). `seed` overrides the circuit's seed."""
    nest = _import_nest()
    spec = load_circuit(circuit)
    if spec.get("schema_version") != 1:
        raise CircuitError("schema_version: unsupported version (expected 1)")
    res = float(spec.get("resolution_ms", 0.1))
    sd = int(spec.get("seed", 0)) if seed is None else int(seed)

    nest.ResetKernel()
    nest.verbosity = nest.VerbosityLevel.INFO if verbose else nest.VerbosityLevel.ERROR
    nest.resolution = res
    nest.rng_seed = sd
    nest.local_num_threads = int(threads)
    nest.print_time = False

    # 1. populations
    pops: dict[str, tuple[int, int, Any]] = {}
    first = 0
    all_nodes = None
    for p in spec["populations"]:
        nc = nest.Create(p["model"], int(p["n"]), params=dict(p.get("params") or {}) or None)
        pops[p["id"]] = (first, int(p["n"]), nc)
        first += int(p["n"])
        all_nodes = nc if all_nodes is None else all_nodes + nc
    n_neurons = first

    # 2. devices, 3. recorder
    devices = {}
    for inp in spec.get("inputs", []):
        model = {"dc": "dc_generator", "poisson": "poisson_generator"}[inp["kind"]]
        key = "amplitude" if inp["kind"] == "dc" else "rate"
        devices[inp["name"]] = nest.Create(model, params={key: float(inp.get("value", 0.0))})
    recorder = nest.Create("spike_recorder", params={"record_to": "memory"})

    base = all_nodes.tolist()[0]

    def nodes(idx: list[int]):
        return nest.NodeCollection([base + i for i in idx])

    # connections: projections
    for k, pr in enumerate(spec.get("projections", [])):
        pre_first, pre_n, pre_nc = pops[pr["pre"]]
        post_first, post_n, post_nc = pops[pr["post"]]
        syn = dict(pr.get("synapse") or {})
        syn.setdefault("synapse_model", "static_synapse")
        if "edges" in pr:
            ed = pr["edges"]
            if not ed:
                continue
            src = [base + pre_first + int(e[0]) for e in ed]
            tgt = [base + post_first + int(e[1]) for e in ed]
            s = dict(syn)
            s["weight"] = np.array([float(e[2]) for e in ed])
            s["delay"] = np.array([float(e[3]) for e in ed])
            nest.Connect(np.array(src), np.array(tgt), conn_spec="one_to_one", syn_spec=s)
        else:
            rule = {key: v for key, v in pr.items() if key not in ("pre", "post", "synapse")}
            nest.Connect(pre_nc, post_nc, conn_spec=rule, syn_spec=syn)

    # connections: inputs, recorder
    for inp in spec.get("inputs", []):
        tgt = set()
        for sel in inp["targets"]:
            tgt.update(_selector(spec, pops, sel, f"inputs.{inp['name']}.targets"))
        delay = float(inp.get("delay", -1.0))
        nest.Connect(
            devices[inp["name"]],
            nodes(sorted(tgt)),
            conn_spec="all_to_all",
            syn_spec={"weight": float(inp.get("weight", 1.0)), "delay": delay if delay > 0 else res},
        )
    rec = spec.get("record", {}).get("spikes", "all")
    if rec == "all":
        rec_idx = list(range(n_neurons))
    elif rec == "none":
        rec_idx = []
    else:
        s = set()
        for sel in rec:
            s.update(_selector(spec, pops, sel, "record.spikes"))
        rec_idx = sorted(s)
    if rec_idx:
        nest.Connect(nodes(rec_idx), recorder, conn_spec="all_to_all", syn_spec={"weight": 1.0, "delay": res})

    return Network(spec, res, sd, int(threads), n_neurons, all_nodes, pops, devices, recorder)


def _events(net: Network) -> Spikes:
    ev = net.recorder.get("events")
    base = net.neurons.tolist()[0]
    nr = np.asarray(ev["senders"], dtype=np.int64) - base
    t = np.asarray(ev["times"], dtype=np.float64)
    order = np.lexsort((nr, t))
    return Spikes(nr[order], t[order])


def run(net: Network, ms: float, drain: bool = False) -> Spikes:
    """Simulate `ms` of model time and return all spikes so far (or only the new ones with drain=True)."""
    nest = _import_nest()
    nest.Simulate(float(ms))
    net.time_ms += float(ms)
    s = _events(net)
    if drain:
        k = net._read
        net._read = len(s)
        # events are time-ordered per call; the already-read prefix is the earliest spikes
        return Spikes(s.neuron[k:], s.t_ms[k:])
    return s
