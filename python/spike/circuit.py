"""Build a circuit JSON (docs/CIRCUIT-FORMAT.md) as a PyNEST network identical to the C++ engine's, and run it.

Creation / connection order mirrors engine/src/circuit.h so node ids and RNG streams match:
  1. populations in file order (neuron i = node id i+1), 2. one device per input, 3. one spike_recorder,
  then connections: projections in file order, inputs in file order (delay default = resolution), recorder
  (weight 1, delay = resolution).
  Dopamine plumbing (only when a stdp_dopamine_synapse projection exists) is created after the recorder:
  one volume_transmitter per distinct "dopamine_source" (order of first use), then one poisson_generator and one parrot_neuron;
  connections: each source population -> its volume_transmitter, generator -> parrot, parrot -> every volume_transmitter.
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
    modulation: list = field(default_factory=list)   # validated rows of the "modulation" table
    mod_targets: dict = field(default_factory=dict)  # (pop id, param) -> base value
    mod_levels: dict = field(default_factory=lambda: {"NE": 0.0, "ACh": 0.0, "DA": 0.0})
    da_generator: Any = None

    def population(self, pop_id: str):
        return self.populations[pop_id][2]

    def population_slice(self, pop_id: str) -> range:
        first, n, _ = self.populations[pop_id]
        return range(first, first + n)

    def set_input(self, name: str, value: float):
        import nest

        kind = self.spec["inputs"][[i["name"] for i in self.spec["inputs"]].index(name)]["kind"]
        self.devices[name].set({"amplitude" if kind == "dc" else "rate": float(value)})

    def set_modulator(self, name: str, conc: float):
        """Set an external neuromodulator level (NE, ACh, DA) and apply the modulation table now (the C++ engine does this at the
        next step). Release from "modulator" outputs is an engine feature and is not simulated here; DA drives the volume_transmitter
        Poisson chain at DA_VT_HZ_PER_UNIT Hz per unit."""
        if name not in self.mod_levels:
            raise CircuitError(f"set_modulator: unknown modulator '{name}' (NE, ACh or DA)")
        self.mod_levels[name] = max(0.0, float(conc))
        _apply_modulation(self)

    def modulator(self, name: str) -> float:
        return self.mod_levels[name]


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


_M64 = (1 << 64) - 1


def initial_vm_fraction(seed: int, neuron: int) -> float:
    """U[0,1) draw for a neuron's initial V_m: splitmix64 keyed by (circuit seed, circuit-level neuron index).

    Must match the C++ engine (engine/src/engine.cpp, build step 1) bit for bit.
    """
    z = (int(seed) + (neuron + 1) * 0x9E3779B97F4A7C15) & _M64
    z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & _M64
    z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & _M64
    z ^= z >> 31
    return (z >> 11) * (1.0 / 9007199254740992.0)


MODULATORS = ("NE", "ACh", "DA")
DA_VT_HZ_PER_UNIT = 100.0  # DA concentration 1.0 = 100 Hz of dopamine "spikes" onto every volume_transmitter (engine/src/engine.cpp)
_GAP_MODELS = ("hh_psc_alpha_gap", "hh_cond_beta_gap_traub")


def _num(d: dict, key: str, path: str):
    if key not in d:
        return None
    v = d[key]
    if isinstance(v, bool) or not isinstance(v, (int, float)):
        raise CircuitError(f"{path}.{key}: must be a number")
    return float(v)


def _prepare_projection(spec: dict, pr: dict, k: int) -> dict:
    """Validate a projection and resolve synapse-model defaults exactly like engine/src/circuit.cpp (validate_synapse).

    Returns {"rule", "syn", "common", "gap", "dopamine", "dopamine_pop", "edges"}; error messages carry the same paths.
    """
    path = f"projections[{k}]"
    pops = {p["id"]: p for p in spec["populations"]}
    syn = dict(pr.get("synapse") or {})
    syn.setdefault("synapse_model", "static_synapse")
    model = syn["synapse_model"]
    spath = f"{path}.synapse"
    rule = None
    edges = None
    if "edges" in pr:
        edges = [list(e) for e in pr["edges"]]
    else:
        rule = {key: v for key, v in pr.items() if key not in ("pre", "post", "synapse", "dopamine_source")}
    ds = pr.get("dopamine_source")
    if ds is not None:
        if not isinstance(ds, str):
            raise CircuitError(f"{path}.dopamine_source: must be a population id")
        if ds not in pops:
            raise CircuitError(f"{path}.dopamine_source: unknown population '{ds}'")
        if model != "stdp_dopamine_synapse":
            raise CircuitError(f"{path}.dopamine_source: only valid with synapse_model stdp_dopamine_synapse")
    a, b = pops[pr["pre"]], pops[pr["post"]]
    common: dict = {}

    def weights():
        if edges is not None:
            return [float(e[2]) for e in edges]
        w = syn.get("weight", 1.0)
        return [float(w)] if isinstance(w, (int, float)) and not isinstance(w, bool) else []

    gap = False
    dopamine = False
    if model == "gap_junction":
        gap = True
        if a["model"] not in _GAP_MODELS or b["model"] not in _GAP_MODELS:
            raise CircuitError(
                f"{spath}.synapse_model: gap_junction needs gap-capable neurons on both sides (hh_psc_alpha_gap or hh_cond_beta_gap_traub); "
                f"'{a['id']}' is {a['model']}, '{b['id']}' is {b['model']}"
            )
        if "delay" in syn:
            raise CircuitError(f"{spath}.delay: gap junctions are instantaneous: remove 'delay'")
        for w in weights():
            if not w > 0:
                raise CircuitError(f"{spath}.weight: gap_junction weight is a conductance in nS and must be > 0")
        if rule is not None:
            rn = rule["rule"]
            if rn not in ("one_to_one", "all_to_all"):
                raise CircuitError(f"{path}.rule: gap_junction supports the rules one_to_one and all_to_all (within one population), or edges (got '{rn}')")
            if rule.get("make_symmetric", True) is not True:
                raise CircuitError(f"{path}.make_symmetric: gap junctions must be symmetric: make_symmetric is always true")
            rule.pop("make_symmetric", None)
            if rn == "one_to_one":
                rule["make_symmetric"] = True
            elif pr["pre"] != pr["post"]:
                raise CircuitError(f"{path}.rule: gap_junction all_to_all needs pre and post to be the same population (use one_to_one or edges between two populations)")
            if rn == "all_to_all":
                rule.setdefault("allow_autapses", False)
    elif model in ("tsodyks2_synapse", "quantal_stp_synapse"):
        for key, lo, hi, msg in (("U", 0, 1, "utilisation U must be in [0, 1]"), ("u", 0, 1, "initial utilisation u must be in [0, 1]")):
            v = _num(syn, key, spath)
            if v is not None and not (lo <= v <= hi):
                raise CircuitError(f"{spath}.{key}: {msg}")
        v = _num(syn, "tau_rec", spath)
        if v is not None and not v > 0:
            raise CircuitError(f"{spath}.tau_rec: recovery time constant tau_rec must be > 0 ms")
        v = _num(syn, "tau_fac", spath)
        if v is not None and not v >= 0:
            raise CircuitError(f"{spath}.tau_fac: facilitation time constant tau_fac must be >= 0 ms (0 = depression only)")
        if model == "tsodyks2_synapse":
            v = _num(syn, "x", spath)
            if v is not None and not (0 <= v <= 1):
                raise CircuitError(f"{spath}.x: initial resources x must be in [0, 1]")
        else:
            n = 1
            if "n" in syn:
                if not isinstance(syn["n"], int) or isinstance(syn["n"], bool) or syn["n"] < 1:
                    raise CircuitError(f"{spath}.n: number of release sites n must be an integer >= 1")
                n = syn["n"]
            if "a" in syn:
                if not isinstance(syn["a"], int) or isinstance(syn["a"], bool) or syn["a"] < 0:
                    raise CircuitError(f"{spath}.a: number of available sites a must be an integer >= 0")
                if syn["a"] > n:
                    raise CircuitError(f"{spath}.a: available sites a must not exceed n")
    elif model in ("stdp_synapse", "stdp_dopamine_synapse"):
        dopamine = model == "stdp_dopamine_synapse"
        w = weights()
        for x in w:
            if not x > 0:
                raise CircuitError(
                    f"{spath}.weight: {model} supports excitatory (positive) weights only, got {x:f}. "
                    "Inhibitory plasticity is not available: use static_synapse (or tsodyks2_synapse) for inhibitory connections"
                )
        v = _num(syn, "tau_plus", spath)
        if v is not None and not v > 0:
            raise CircuitError(f"{spath}.tau_plus: tau_plus must be > 0 ms")
        wmax = _num(syn, "Wmax", spath)
        if wmax is not None:
            if not wmax > 0:
                raise CircuitError(f"{spath}.Wmax: Wmax must be > 0")
            for x in w:
                if x > wmax:
                    raise CircuitError(f"{spath}.Wmax: weight {x:f} exceeds Wmax {wmax:f}")
        elif not w and edges is None:
            raise CircuitError(f"{spath}.Wmax: Wmax is required when weight is not a plain number (the default is 2 * weight)")
        elif not w:
            syn["Wmax"] = 1.0  # no edges: nothing is connected
        else:
            syn["Wmax"] = 2.0 * max(w)  # default: 2 * |weight| (edges: the largest edge weight)
        if not dopamine:
            if "volume_transmitter" in syn:
                raise CircuitError(f"{spath}: volume_transmitter is only valid for stdp_dopamine_synapse")
        else:
            v = _num(syn, "Wmin", spath)
            if v is not None and (v < 0 or v > syn["Wmax"]):
                raise CircuitError(f"{spath}.Wmin: Wmin must be in [0, Wmax]")
            for key in ("tau_c", "tau_n"):
                v = _num(syn, key, spath)
                if v is not None and not v > 0:
                    raise CircuitError(f"{spath}.{key}: {key} must be > 0 ms")
            if "volume_transmitter" in syn:
                raise CircuitError(f"{spath}.volume_transmitter: set by the engine from \"dopamine_source\"")
            if ds is None:
                raise CircuitError(f"{path}.dopamine_source: stdp_dopamine_synapse needs a \"dopamine_source\" population (its spikes drive the volume_transmitter)")
            for key in ("A_plus", "A_minus", "Wmin", "Wmax", "b", "tau_c", "tau_n", "tau_plus"):
                if key in syn:
                    common[key] = syn.pop(key)
    return {"rule": rule, "syn": syn, "common": common, "gap": gap, "dopamine": dopamine, "dopamine_pop": ds, "edges": edges}


def _parse_modulation(spec: dict) -> list:
    pops = {p["id"] for p in spec["populations"]}
    rows = []
    for i, m in enumerate(spec.get("modulation", [])):
        path = f"modulation[{i}]"
        if not isinstance(m, dict):
            raise CircuitError(f"{path}: must be an object")
        for key in m:
            if key not in ("modulator", "pop", "param", "gain", "scale_per_unit", "min", "max"):
                raise CircuitError(f"{path}.{key}: unknown key")
        for key in ("modulator", "pop", "param"):
            if key not in m:
                raise CircuitError(f"{path}.{key}: required field is missing")
        if m["modulator"] not in MODULATORS:
            raise CircuitError(f"{path}.modulator: unknown modulator '{m['modulator']}' (NE, ACh or DA)")
        if m["pop"] not in pops:
            raise CircuitError(f"{path}.pop: unknown population '{m['pop']}'")
        if ("gain" in m) == ("scale_per_unit" in m):
            raise CircuitError(f"{path}: exactly one of 'gain' (value += gain * conc) or 'scale_per_unit' (value *= 1 + scale_per_unit * conc) is required")
        scale = "scale_per_unit" in m
        lo, hi = _num(m, "min", path), _num(m, "max", path)
        if lo is not None and hi is not None and lo > hi:
            raise CircuitError(f"{path}: min must be <= max")
        rows.append({"modulator": m["modulator"], "pop": m["pop"], "param": m["param"], "scale": scale,
                     "amount": _num(m, "scale_per_unit" if scale else "gain", path), "min": lo, "max": hi})
    return rows


def _apply_modulation(net: "Network"):
    """Write the modulation table's values for the current modulator levels (rows on one (pop, param) compose in file order)."""
    for (pid, param), base in net.mod_targets.items():
        v = base
        for r in net.modulation:
            if r["pop"] == pid and r["param"] == param:
                c = net.mod_levels[r["modulator"]]
                v = v * (1.0 + r["amount"] * c) if r["scale"] else v + r["amount"] * c
                if r["min"] is not None:
                    v = max(v, r["min"])
                if r["max"] is not None:
                    v = min(v, r["max"])
        net.populations[pid][2].set({param: v})
    if net.da_generator is not None:
        net.da_generator.set({"rate": max(0.0, net.mod_levels["DA"]) * DA_VT_HZ_PER_UNIT})


def _register_nestml():
    """Register the NESTML cells (models/nestml/spike_nestml.py) if the PyNEST loader library is built; skip silently otherwise."""
    import sys

    d = str(Path(__file__).resolve().parents[2] / "models" / "nestml")
    if d not in sys.path:
        sys.path.append(d)
    try:
        import spike_nestml

        spike_nestml.register()
    except (ImportError, FileNotFoundError, OSError, IndexError):
        pass


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
    _register_nestml()  # after ResetKernel and the thread change (both drop registered models)

    # 1. populations
    pops: dict[str, tuple[int, int, Any]] = {}
    first = 0
    all_nodes = None
    for p in spec["populations"]:
        nc = nest.Create(p["model"], int(p["n"]), params=dict(p.get("params") or {}) or None)
        # seeded random initial V_m (docs/CIRCUIT-FORMAT.md): only if params give no V_m and the model has V_m, E_L, V_th
        if "V_m" not in (p.get("params") or {}):
            st = nc[0].get()
            if "V_m" in st and "E_L" in st and "V_th" in st and st["V_th"] > st["E_L"]:
                e_l, v_th = float(st["E_L"]), float(st["V_th"])
                for j in range(int(p["n"])):
                    nc[j].set({"V_m": e_l + initial_vm_fraction(sd, first + j) * (v_th - e_l)})
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

    prepared = [_prepare_projection(spec, pr, k) for k, pr in enumerate(spec.get("projections", []))]
    modulation = _parse_modulation(spec)

    # (4) dopamine plumbing: one volume_transmitter per distinct "dopamine_source" (order of first use), one generator -> parrot chain
    vts: dict[str, Any] = {}
    for pp in prepared:
        if pp["dopamine"] and pp["dopamine_pop"] not in vts:
            vts[pp["dopamine_pop"]] = nest.Create("volume_transmitter")
    da_generator = None
    if vts:
        da_generator = nest.Create("poisson_generator", params={"rate": 0.0})
        relay = nest.Create("parrot_neuron")
        for pid, vt in vts.items():
            nest.Connect(pops[pid][2], vt, conn_spec="all_to_all", syn_spec={"synapse_model": "static_synapse", "weight": 1.0, "delay": res})
        nest.Connect(da_generator, relay, conn_spec="all_to_all", syn_spec={"synapse_model": "static_synapse", "weight": 1.0, "delay": res})
        for vt in vts.values():
            nest.Connect(relay, vt, conn_spec="all_to_all", syn_spec={"synapse_model": "static_synapse", "weight": 1.0, "delay": res})

    # connections: projections
    for k, (pr, pp) in enumerate(zip(spec.get("projections", []), prepared)):
        pre_first, pre_n, pre_nc = pops[pr["pre"]]
        post_first, post_n, post_nc = pops[pr["post"]]
        syn = dict(pp["syn"])
        if pp["dopamine"]:
            model = f"stdp_dopamine_synapse__p{k}"
            nest.CopyModel("stdp_dopamine_synapse", model, {**pp["common"], "volume_transmitter": vts[pp["dopamine_pop"]]})
            syn["synapse_model"] = model
        if pp["edges"] is not None:
            ed = pp["edges"]
            if not ed:
                continue
            src = [base + pre_first + int(e[0]) for e in ed]
            tgt = [base + post_first + int(e[1]) for e in ed]
            s = dict(syn)
            s["weight"] = np.array([float(e[2]) for e in ed])
            if pp["gap"]:  # instantaneous and symmetric: no delay, every edge also post -> pre
                s["weight"] = np.concatenate([s["weight"], s["weight"]])
                src, tgt = src + tgt, tgt + src
            else:
                s["delay"] = np.array([float(e[3]) for e in ed])
            nest.Connect(np.array(src), np.array(tgt), conn_spec="one_to_one", syn_spec=s)
        else:
            nest.Connect(pre_nc, post_nc, conn_spec=pp["rule"], syn_spec=syn)

    # connections: inputs, recorder
    for inp in spec.get("inputs", []):
        tgt = set()
        for sel in inp["targets"]:
            tgt.update(_selector(spec, pops, sel, f"inputs.{inp['name']}.targets"))
        if not tgt:
            continue  # unwired channel: the device exists, nothing is connected
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

    net = Network(spec, res, sd, int(threads), n_neurons, all_nodes, pops, devices, recorder, modulation=modulation, da_generator=da_generator)
    # modulation table: remember each target parameter's base value (after the population's params), then apply the zero-level state
    for r in modulation:
        key = (r["pop"], r["param"])
        if key not in net.mod_targets:
            st = pops[r["pop"]][2][0].get()
            if r["param"] not in st:
                model = next(p["model"] for p in spec["populations"] if p["id"] == r["pop"])
                raise CircuitError(f"modulation.param: model {model} has no parameter '{r['param']}'")
            net.mod_targets[key] = float(st[r["param"]])
    return net


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
