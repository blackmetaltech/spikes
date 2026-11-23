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

import math
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
    proj_nc: dict = field(default_factory=dict)       # projection number -> (pre NodeCollection, post NodeCollection, synapse model)
    proj_targets: dict = field(default_factory=dict)  # (projection number, param) -> (connections, base values per connection)
    sg_times: dict = field(default_factory=dict)      # spike_generator input name -> every spike time queued so far (ms, ascending)

    def population(self, pop_id: str):
        return self.populations[pop_id][2]

    def population_slice(self, pop_id: str) -> range:
        first, n, _ = self.populations[pop_id]
        return range(first, first + n)

    def set_input(self, name: str, value: float):
        import nest

        kind = self.spec["inputs"][[i["name"] for i in self.spec["inputs"]].index(name)]["kind"]
        if kind == "spike_generator":
            raise CircuitError(f"set_input: '{name}' is a spike_generator: use queue_spikes()")
        self.devices[name].set({"amplitude" if kind == "dc" else "rate": float(value)})  # sinusoidal_poisson: the mean rate

    def set_input_param(self, name: str, key: str, value: float):
        """Change "rate", "amplitude" (Hz), "freq_hz" (Hz) or "phase" (degrees) of a sinusoidal_poisson input (like Engine::set_input_param)."""
        kind = self.spec["inputs"][[i["name"] for i in self.spec["inputs"]].index(name)]["kind"]
        if kind != "sinusoidal_poisson":
            raise CircuitError(f"set_input_param: '{name}' is not a sinusoidal_poisson input")
        if key not in _SINE_KEYS:
            raise CircuitError(f"set_input_param: unknown parameter '{key}' (rate, amplitude, freq_hz, phase)")
        self.devices[name].set({_SINE_KEYS[key]: float(value)})

    def queue_spikes(self, name: str, times_ms):
        """Append spikes (absolute model time in ms, a multiple of the resolution, strictly after the time reached) to a spike_generator
        input (like Engine::queue_spikes). Raises CircuitError, queueing nothing, for a time in the past or off the grid."""
        kind = self.spec["inputs"][[i["name"] for i in self.spec["inputs"]].index(name)]["kind"]
        if kind != "spike_generator":
            raise CircuitError(f"queue_spikes: '{name}' is not a spike_generator input")
        add = []
        for t in times_ms:
            t = float(t)
            steps = t / self.resolution_ms
            if abs(steps - round(steps)) > 1e-6:
                raise CircuitError(f"queue_spikes: spike time {t:f} ms is not a multiple of the resolution ({self.resolution_ms:f} ms)")
            if not t > self.time_ms + 0.5 * self.resolution_ms:
                raise CircuitError(f"queue_spikes: spike time {t:f} ms is not after the model time reached ({self.time_ms:f} ms)")
            add.append(round(steps) * self.resolution_ms)
        times = sorted(self.sg_times.get(name, []) + add)
        self.sg_times[name] = times
        self.devices[name].set({"spike_times": [t for t in times if t > self.time_ms + 0.5 * self.resolution_ms]})

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


# Load-time budgets (mirror the engine, engine/src/circuit.h: kMaxNeurons, kMaxConnections, kMaxDelayMs, kMaxRateHz, kMaxDcPa, kMaxJsonDepth, kMaxSeed, ...). A circuit over
# one is refused with the engine's message before anything is built; engine/tests/tasks_rules_test.cpp keeps MAX_NEURONS equal to the engine's.
MAX_NEURONS = 20000
MAX_CONNECTIONS = 2.0e6
MAX_DELAY_MS = 1000.0
MAX_RATE_HZ = 1.0e5
MAX_DC_PA = 1.0e5
MAX_NESTML_DC_PA = 2000.0        # I_e of a NESTML cell
MAX_NESTML_INPUT_PA = 1000.0     # a dc input's weight x value onto a NESTML cell
MAX_JSON_DEPTH = 32
MAX_SEED = 4294967295            # NEST's RNG seed range is [1, 2^32 - 1]; a missing seed is 1, as in the engine (NEST refuses 0)
_LIMIT_NESTML = ("butera_burster", "stg_cell", "sa_node_pacemaker", "icc_slow_wave")


def _depth_ok(v, depth=1) -> bool:
    if isinstance(v, (dict, list)):
        if depth > MAX_JSON_DEPTH:
            return False
        return all(_depth_ok(x, depth + 1) for x in (v.values() if isinstance(v, dict) else v))
    return True


def _is_int(v) -> bool:
    return isinstance(v, int) and not isinstance(v, bool) or (isinstance(v, float) and v.is_integer())


def check_seed(seed, path: str = "seed") -> int:
    """The seed as an int: 1 .. 2**32 - 1 like the engine (it reads a missing seed as 1, never 0: NEST refuses 0)."""
    if isinstance(seed, bool) or not _is_int(seed) or not 1 <= seed <= MAX_SEED:
        raise CircuitError(f"{path}: must be an integer from 1 to {MAX_SEED}")
    return int(seed)


def check_limits(spec: dict) -> None:
    """Raise CircuitError for a circuit the engine would refuse at load for size or range (engine/src/circuit.cpp): the neuron total, an estimated connection
    count, delays, rates, constant currents, the resolution and the seed. Structure (unknown models, weights, plasticity ...) is checked where the circuit is built."""
    res = spec.get("resolution_ms", 0.1)
    if isinstance(res, bool) or not isinstance(res, (int, float)) or not 0.001 <= res <= 10.0:
        raise CircuitError("resolution_ms: must be between 0.001 and 10 ms")
    if "seed" in spec:
        check_seed(spec["seed"])
    pops = spec.get("populations")
    if not isinstance(pops, list) or not pops:
        raise CircuitError("populations: must be a non-empty array")
    sizes: dict[str, int] = {}
    model_of: dict[str, str] = {}
    total = 0
    for i, p in enumerate(pops):
        path = f"populations[{i}]"
        n = p.get("n")
        if isinstance(n, bool) or not _is_int(n) or n < 1:
            raise CircuitError(f"{path}.n: must be an integer >= 1")
        if n > MAX_NEURONS or total + n > MAX_NEURONS:
            raise CircuitError(f"{path}.n: this circuit has more than {MAX_NEURONS} neurons in total (the limit that keeps the simulation responsive)")
        total += int(n)
        sizes[p["id"]] = int(n)
        model_of[p["id"]] = p["model"]
        ie = (p.get("params") or {}).get("I_e")
        cap = MAX_NESTML_DC_PA if p["model"] in _LIMIT_NESTML else MAX_DC_PA
        if isinstance(ie, (int, float)) and not abs(ie) <= cap:
            raise CircuitError(f"{path}.params.I_e: a constant current of {ie:f} pA is above the allowed {int(cap)} pA")
    conns = 0.0
    for k, pr in enumerate(spec.get("projections", [])):
        path = f"projections[{k}]"
        syn = pr.get("synapse") or {}
        d = syn.get("delay")
        if isinstance(d, (int, float)) and not d <= MAX_DELAY_MS:
            raise CircuitError(f"{path}.synapse.delay: delay {d:f} ms is above the allowed {int(MAX_DELAY_MS)} ms")
        npre, npost = sizes.get(str(pr.get("pre")).split(".")[0], 0), sizes.get(str(pr.get("post")).split(".")[0], 0)
        est = float(npre * npost)
        if "edges" in pr:
            est = float(len(pr["edges"]))
            for j, e in enumerate(pr["edges"]):
                if isinstance(e, (list, tuple)) and len(e) == 4 and isinstance(e[3], (int, float)) and not e[3] <= MAX_DELAY_MS:
                    raise CircuitError(f"{path}.edges[{j}]: delay {e[3]:f} ms is above the allowed {int(MAX_DELAY_MS)} ms")
        else:
            rule = pr.get("rule")
            r = rule.get("rule") if isinstance(rule, dict) else rule
            num = lambda key: max(0.0, float(rule.get(key, 0.0))) if isinstance(rule, dict) and isinstance(rule.get(key, 0.0), (int, float)) else 0.0
            if r == "one_to_one":
                est = float(min(npre, npost))
            elif r == "fixed_indegree":
                est = num("indegree") * npost
            elif r == "fixed_outdegree":
                est = num("outdegree") * npre
            elif r == "fixed_total_number":
                est = num("N")
            elif r == "pairwise_bernoulli":
                p_ = float(rule.get("p", 1.0)) if isinstance(rule, dict) and isinstance(rule.get("p", 1.0), (int, float)) else 1.0
                est = min(est, max(0.0, p_) * npre * npost)
        if syn.get("synapse_model") == "gap_junction":
            est *= 2
        conns += est
        if not est <= MAX_CONNECTIONS or not conns <= MAX_CONNECTIONS:
            raise CircuitError(f"{path}: this circuit would make about {int(min(conns, 1e18))} connections; the limit is {int(MAX_CONNECTIONS)}")
    for i, inp in enumerate(spec.get("inputs", [])):
        path = f"inputs[{i}]"
        d = inp.get("delay")
        if isinstance(d, (int, float)) and d > MAX_DELAY_MS:
            raise CircuitError(f"{path}.delay: delay {d:f} ms is above the allowed {int(MAX_DELAY_MS)} ms")
        kind = inp.get("kind")
        val = inp.get("value", 0.0)
        if kind == "poisson" and isinstance(val, (int, float)) and val > MAX_RATE_HZ:
            raise CircuitError(f"{path}.value: rate {val:f} Hz is above the allowed {int(MAX_RATE_HZ)} Hz")
        if kind == "sinusoidal_poisson":
            for key in ("rate", "amplitude"):
                if isinstance(inp.get(key), (int, float)) and inp[key] > MAX_RATE_HZ:
                    raise CircuitError(f"{path}.{key}: rate above the allowed {int(MAX_RATE_HZ)} Hz")
        if kind == "dc" and isinstance(val, (int, float)):
            w = inp.get("weight", 1.0)
            for t in inp.get("targets", []):
                tid = t if isinstance(t, str) else t.get("pop")
                cap = MAX_NESTML_INPUT_PA if model_of.get(tid) in _LIMIT_NESTML else MAX_DC_PA
                if isinstance(w, (int, float)) and abs(w * val) > cap:
                    raise CircuitError(f"{path}.value: a constant current of {w * val:f} pA onto '{tid}' is above the allowed {int(cap)} pA")


def _is_rate_model(model: str) -> bool:
    """Same test as the engine's is_rate_model: NEST rate models (lin_rate_ipn, tanh_rate_ipn, ...) and siegert_neuron."""
    return model == "siegert_neuron" or "rate" in model


def check_supported(spec: dict) -> None:
    """The Theory wing's rate inputs and populations (input kinds `rate_drive` and `tuned_poisson`, rate models and `siegert_neuron`, their projections) are not
    supported by this package yet. Say so plainly, instead of failing inside NEST: such a circuit runs in the C++ engine (spike-run, the game)."""
    for i, inp in enumerate(spec.get("inputs", [])):
        if inp.get("kind") in ("rate_drive", "tuned_poisson"):
            raise CircuitError(f"inputs[{i}].kind: '{inp['kind']}' (a Theory-wing input) is not supported by the Python package yet; run this circuit with the C++ engine (spike-run or the game)")
    for i, p in enumerate(spec.get("populations", [])):
        if _is_rate_model(str(p.get("model", ""))):
            raise CircuitError(f"populations[{i}].model: the rate model '{p['model']}' (a Theory-wing population) is not supported by the Python package yet; run this circuit with the C++ engine (spike-run or the game)")


def load_circuit(circuit) -> dict:
    """Accept a JSON string, a dict, or a path to a .json file. Text and files are refused when nested deeper than the engine allows (32 levels)."""
    if isinstance(circuit, dict):
        return circuit
    if isinstance(circuit, os.PathLike) or (isinstance(circuit, str) and not circuit.lstrip().startswith("{") and os.path.exists(circuit)):
        spec = json.loads(Path(circuit).read_text())
    else:
        spec = json.loads(circuit)
    if not _depth_ok(spec):
        raise CircuitError(f"$: JSON is nested deeper than {MAX_JSON_DEPTH} levels")
    return spec


_FREE_PARAMS = {"sign": "sign", "scale": "range"}   # the parameters a group may declare free (engine/src/circuit.cpp kFreeParams); a later one is one more row here
_GROUP_KEYS = {"id", "label", "pops", "sign", "scale", "free", "ranges", "off"}


def apply_groups(spec: dict) -> dict:
    """The circuit with its cell-type groups applied (docs/CIRCUIT-FORMAT.md "Locked parts and groups", 0.6): the weight of every connection of every projection whose
    presynaptic population is in a group becomes |stored weight| x scale x sign (`weight_sd` is multiplied by scale), exactly as the C++ engine does at load. Returns a
    new dict without "groups" (so applying it twice cannot apply the groups twice) and without the "locked" flags; a spec without groups and flags comes back unchanged
    in content. `locked` is not enforced here: the package runs any circuit (the lock is the game's and the levels' concern: engine/src/tasks/registry.cpp)."""
    import copy

    out = copy.deepcopy(spec)
    for p in out.get("populations", []):
        if isinstance(p, dict) and "locked" in p:
            if not isinstance(p["locked"], bool):
                raise CircuitError("populations[%d].locked: must be true or false" % out["populations"].index(p))
            del p["locked"]
    groups = out.pop("groups", None)
    for k, pr in enumerate(out.get("projections", [])):
        if isinstance(pr, dict) and "locked" in pr:
            if not isinstance(pr["locked"], bool):
                raise CircuitError(f"projections[{k}].locked: must be true or false")
            if pr["locked"] and "edges" not in pr:
                raise CircuitError(f"projections[{k}]: a locked projection lists its edges (the wiring is data, not a rule that draws it)")
            del pr["locked"]
    if groups is None:
        return out
    if not isinstance(groups, list):
        raise CircuitError("groups: must be an array")
    pop_ids = [p["id"] for p in out.get("populations", [])]
    of_pop: dict[str, tuple[float, int]] = {}   # population id -> (scale, sign)
    seen: set[str] = set()
    for i, g in enumerate(groups):
        path = f"groups[{i}]"
        if not isinstance(g, dict):
            raise CircuitError(f"{path}: must be an object")
        for key in g:
            if key not in _GROUP_KEYS:
                raise CircuitError(f"{path}.{key}: unknown key")
        gid = g.get("id")
        if not isinstance(gid, str) or not gid or gid in seen:
            raise CircuitError(f"{path}.id: must be non-empty and unique among groups")
        seen.add(gid)
        pops = g.get("pops")
        if not isinstance(pops, list) or not pops:
            raise CircuitError(f"{path}.pops: must be a non-empty array of population ids")
        sign = g.get("sign", 1)
        if isinstance(sign, bool) or not isinstance(sign, (int, float)) or sign not in (1, -1):
            raise CircuitError(f"{path}.sign: must be +1 or -1")
        off = g.get("off", False)
        if not isinstance(off, bool):
            raise CircuitError(f"{path}.off: must be true or false")
        scale = g.get("scale", 1.0)
        if isinstance(scale, bool) or not isinstance(scale, (int, float)) or not ((0.0 <= scale if off else 0.0 < scale) and scale <= 1e6):
            raise CircuitError(f"{path}.scale: must be a number from 0 up" if off else f"{path}.scale: must be a positive number")
        free = g.get("free", [])
        ranges = g.get("ranges", {})
        if not isinstance(free, list) or not isinstance(ranges, dict):
            raise CircuitError(f"{path}.free: must be an array of parameter names (and ranges an object)")
        for name in free:
            if name not in _FREE_PARAMS:
                raise CircuitError(f"{path}.free: unknown free parameter '{name}' (this package knows sign and scale)")
            if _FREE_PARAMS[name] == "range":
                rg = ranges.get(name)
                if not (isinstance(rg, list) and len(rg) == 2 and all(isinstance(x, (int, float)) and not isinstance(x, bool) for x in rg) and (0.0 <= rg[0] if off else 0.0 < rg[0]) and rg[0] <= rg[1]):
                    raise CircuitError(f"{path}.ranges: a free '{name}' needs a range [lo, hi] with {'0 <= lo' if off else '0 < lo'} <= hi")
        for name in ranges:
            if name not in free:
                raise CircuitError(f"{path}.ranges: '{name}' has a range but is not in the free list")
        for pid in pops:
            if pid not in pop_ids:
                raise CircuitError(f"{path}.pops: unknown population '{pid}'")
            if pid in of_pop:
                raise CircuitError(f"{path}.pops: population '{pid}' is already in a group (a population is in at most one cell type)")
            of_pop[pid] = (float(scale), 1 if sign > 0 else -1)
    for k, pr in enumerate(out.get("projections", [])):
        pre = str(pr.get("pre")).split(".")[0]
        if pre not in of_pop:
            continue
        scale, sign = of_pop[pre]
        factor = scale * sign
        if "edges" in pr:
            pr["edges"] = [[e[0], e[1], abs(float(e[2])) * factor, *e[3:]] for e in pr["edges"]]
        else:
            syn = pr.setdefault("synapse", {})
            w = syn.get("weight", 1.0)
            if isinstance(w, bool) or not isinstance(w, (int, float)):
                raise CircuitError(f"projections[{k}].synapse.weight: a projection out of a cell-type group needs a plain number as its weight")
            syn["weight"] = abs(float(w)) * factor
        if isinstance(pr.get("weight_sd"), (int, float)):
            pr["weight_sd"] = pr["weight_sd"] * scale
    return out


_SINE_KEYS = {"rate": "rate", "amplitude": "amplitude", "freq_hz": "frequency", "phase": "phase"}
INPUT_KINDS = ("dc", "poisson", "spike_generator", "sinusoidal_poisson")


def _parse_input(inp: dict, i: int, res: float):
    """Validate an input like engine/src/circuit.cpp and return (NEST model, params, spike_times). Error paths are the engine's."""
    path = f"inputs[{i}]"
    kind = inp.get("kind")
    if kind not in INPUT_KINDS:
        raise CircuitError(f"{path}.kind: unsupported input kind '{kind}' (supported: dc, poisson, spike_generator, sinusoidal_poisson)")
    sg, sp = kind == "spike_generator", kind == "sinusoidal_poisson"
    if not sg and "spike_times" in inp:
        raise CircuitError(f'{path}.spike_times: only valid for kind "spike_generator"')
    for k in ("rate", "amplitude", "freq_hz", "phase"):
        if not sp and k in inp:
            raise CircuitError(f'{path}.{k}: only valid for kind "sinusoidal_poisson"')
    if (sg or sp) and "value" in inp:
        raise CircuitError(f"{path}.value: a {kind} input has no value (spike_generator: give spike_times; sinusoidal_poisson: rate, amplitude, freq_hz, phase)")
    if sg:
        times = []
        st = inp.get("spike_times", [])
        if not isinstance(st, list):
            raise CircuitError(f"{path}.spike_times: must be an array of times in ms")
        for k, t in enumerate(st):
            tp = f"{path}.spike_times[{k}]"
            if isinstance(t, bool) or not isinstance(t, (int, float)):
                raise CircuitError(f"{tp}: must be a number (ms)")
            t = float(t)
            if not t > 0.0:
                raise CircuitError(f"{tp}: spike time must be > 0 ms")
            steps = t / res
            if abs(steps - round(steps)) > 1e-6:
                raise CircuitError(f"{tp}: spike time {t:f} ms is not a multiple of resolution_ms ({res:f})")
            if times and t < times[-1]:
                raise CircuitError(f"{tp}: spike times must be sorted in ascending order")
            times.append(round(steps) * res)
        return "spike_generator", ({"spike_times": times} if times else {}), times
    if sp:
        rate, amp = _num(inp, "rate", path) or 0.0, _num(inp, "amplitude", path) or 0.0
        freq, phase = _num(inp, "freq_hz", path) or 0.0, _num(inp, "phase", path) or 0.0
        if not rate >= 0:
            raise CircuitError(f"{path}.rate: mean rate must be >= 0 Hz")
        if not amp >= 0:
            raise CircuitError(f"{path}.amplitude: amplitude must be >= 0 Hz")
        if not (freq >= 0 and freq * res / 1000.0 < 0.5):
            raise CircuitError(f"{path}.freq_hz: frequency must be >= 0 and below the Nyquist limit of the resolution ({500.0 / res:f} Hz)")
        return "sinusoidal_poisson_generator", {"rate": rate, "amplitude": amp, "frequency": freq, "phase": phase}, []
    key = "amplitude" if kind == "dc" else "rate"
    return ("dc_generator" if kind == "dc" else "poisson_generator"), {key: float(inp.get("value", 0.0))}, []


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


def edge_spread_normal(seed: int, proj: int, edge: int) -> float:
    """Standard-normal draw for the weight spread (projection keys weight_sd / weight_cv): splitmix64 keyed by
    (circuit seed, projection index, edge index), two uniforms, Box-Muller. Mirrors engine/src/circuit.cpp spread_normal."""

    def u(i: int) -> float:
        key = ((((proj + 1) << 32) | edge) * 2 + i + 1) & _M64
        z = (int(seed) + key * 0x9E3779B97F4A7C15) & _M64
        z = ((z ^ (z >> 30)) * 0xBF58476D1CE4E5B9) & _M64
        z = ((z ^ (z >> 27)) * 0x94D049BB133111EB) & _M64
        z ^= z >> 31
        return (z >> 11) * (1.0 / 9007199254740992.0)

    u1 = 1.0 - u(0)
    u2 = u(1)
    return math.sqrt(-2.0 * math.log(u1)) * math.cos(2.0 * math.pi * u2)


def spread_weight(w: float, kind: int, amount: float, seed: int, proj: int, edge: int) -> float:
    """w + sd*z (kind 1) or w*(1+cv*z) (kind 2), sign kept, |w'| >= 1e-3 |w|. Mirrors spread_weight in engine/src/circuit.cpp."""
    z = edge_spread_normal(seed, proj, edge)
    w2 = w + amount * z if kind == 1 else w * (1.0 + amount * z)
    floor_mag = 1e-3 * abs(w)
    if w > 0:
        w2 = max(w2, floor_mag)
    elif w < 0:
        w2 = min(w2, -floor_mag)
    return w2


def _spread_edges(pr: dict, edges: list, seed: int, k: int, path: str) -> list:
    """Apply weight_sd (w + sd*z) or weight_cv (w*(1+cv*z)) to every edge weight, keeping the sign (|w'| >= 1e-3 |w|)."""
    if "weight_sd" in pr and "weight_cv" in pr:
        raise CircuitError(f"{path}: give weight_sd or weight_cv, not both")
    key = "weight_sd" if "weight_sd" in pr else "weight_cv" if "weight_cv" in pr else None
    if key is None:
        return edges
    amount = pr[key]
    if isinstance(amount, bool) or not isinstance(amount, (int, float)) or not amount >= 0:
        raise CircuitError(f"{path}.{key}: must be a number >= 0")
    out = []
    for i, e in enumerate(edges):
        w = float(e[2])
        z = edge_spread_normal(seed, k, i)
        w2 = w + amount * z if key == "weight_sd" else w * (1.0 + amount * z)
        floor_mag = 1e-3 * abs(w)
        if w > 0:
            w2 = max(w2, floor_mag)
        elif w < 0:
            w2 = min(w2, -floor_mag)
        out.append([e[0], e[1], w2, *e[3:]])
    return out


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


# Weight ceilings (mirror weight_limit() in engine/src/circuit.cpp; docs/CIRCUIT-FORMAT.md "Weight limits")
MAX_CONDUCTANCE_NS = 500.0
MAX_CURRENT_PA = 10000.0


def weight_limit(neuron_model: str):
    """(limit, unit) of |weight| onto `neuron_model`: nS for "_cond_" models, pA for "_psc_" / "_curr_" models, else (0.0, None)."""
    if neuron_model in ("butera_burster", "stg_cell", "sa_node_pacemaker", "icc_slow_wave"):
        return 2000.0, "nS"  # NESTML cells (engine/src/circuit.cpp kMaxNestmlWeightNs)
    if "_cond_" in neuron_model:
        return MAX_CONDUCTANCE_NS, "nS"
    if "_psc_" in neuron_model or "_curr_" in neuron_model:
        return MAX_CURRENT_PA, "pA"
    return 0.0, None


def _prepare_projection(spec: dict, pr: dict, k: int, seed: int = 0) -> dict:
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
    spread = None
    if "edges" in pr:
        edges = _spread_edges(pr, [list(e) for e in pr["edges"]], seed, k, path)
    else:
        rule = {key: v for key, v in pr.items() if key not in ("pre", "post", "synapse", "dopamine_source", "weight_sd", "weight_cv")}
        if "weight_sd" in pr and "weight_cv" in pr:
            raise CircuitError(f"{path}: give weight_sd or weight_cv, not both")
        for key, kind in (("weight_sd", 1), ("weight_cv", 2)):
            if key in pr:
                amount = pr[key]
                if isinstance(amount, bool) or not isinstance(amount, (int, float)) or not amount >= 0:
                    raise CircuitError(f"{path}.{key}: must be a number >= 0")
                if model == "gap_junction" or model.startswith("stdp"):
                    raise CircuitError(f"{path}.{key}: weight spread on a rule projection is not supported for {model} (use an 'edges' projection)")
                spread = (kind, float(amount))
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

    if model != "gap_junction":
        lim, unit = weight_limit(b["model"])
        if lim > 0:

            def check_w(w, where):
                if not math.isfinite(w) or abs(w) > lim:
                    raise CircuitError(
                        f"{where}: weight {w:f} is outside the allowed range |w| <= {int(lim)} {unit} for {b['model']} "
                        f"(reference circuits use at most 60 nS / 1500 pA)"
                    )

            if edges is not None:
                for i, e in enumerate(edges):
                    check_w(float(e[2]), f"{path}.edges[{i}]")
            elif isinstance(syn.get("weight"), (int, float)) and not isinstance(syn.get("weight"), bool):
                check_w(float(syn["weight"]), f"{spath}.weight")

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
    elif model in ("tsodyks2_synapse", "tsodyks_synapse", "quantal_stp_synapse"):
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
        if model in ("tsodyks2_synapse", "tsodyks_synapse"):
            v = _num(syn, "x", spath)
            if v is not None and not (0 <= v <= 1):
                raise CircuitError(f"{spath}.x: initial resources x must be in [0, 1]")
            if model == "tsodyks_synapse":
                v = _num(syn, "y", spath)
                if v is not None and not (0 <= v <= 1):
                    raise CircuitError(f"{spath}.y: initial active fraction y must be in [0, 1]")
                v = _num(syn, "tau_psc", spath)
                if v is not None and not v > 0:
                    raise CircuitError(f"{spath}.tau_psc: postsynaptic current time constant tau_psc must be > 0 ms")
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
    return {"rule": rule, "syn": syn, "common": common, "gap": gap, "dopamine": dopamine, "dopamine_pop": ds, "edges": edges, "spread": spread}


def _parse_modulation(spec: dict) -> list:
    pops = {p["id"] for p in spec["populations"]}
    projs = spec.get("projections", [])
    rows = []
    for i, m in enumerate(spec.get("modulation", [])):
        path = f"modulation[{i}]"
        if not isinstance(m, dict):
            raise CircuitError(f"{path}: must be an object")
        for key in m:
            if key not in ("modulator", "pop", "projection", "param", "gain", "scale_per_unit", "min", "max"):
                raise CircuitError(f"{path}.{key}: unknown key")
        for key in ("modulator", "param"):
            if key not in m:
                raise CircuitError(f"{path}.{key}: required field is missing")
        if m["modulator"] not in MODULATORS:
            raise CircuitError(f"{path}.modulator: unknown modulator '{m['modulator']}' (NE, ACh or DA)")
        if ("pop" in m) == ("projection" in m):
            raise CircuitError(f"{path}: exactly one of 'pop' (a neuron parameter) or 'projection' (a synapse parameter) is required")
        proj = None
        if "pop" in m:
            if m["pop"] not in pops:
                raise CircuitError(f"{path}.pop: unknown population '{m['pop']}'")
        else:
            proj = m["projection"]
            if isinstance(proj, bool) or not isinstance(proj, int) or not 0 <= proj < len(projs):
                raise CircuitError(f"{path}.projection: must be a projection number (0-based, file order; this circuit has {len(projs)})")
            sm = (projs[proj].get("synapse") or {}).get("synapse_model", "static_synapse")
            if sm == "gap_junction":
                raise CircuitError(f"{path}.projection: gap_junction projections cannot be modulated")
            if m["param"] == "weight" and sm.startswith("stdp"):
                raise CircuitError(f"{path}.param: the weight of a plastic {sm} projection is owned by the plasticity rule and cannot be modulated")
            if m["param"] in ("delay", "synapse_model", "port", "target", "source"):
                raise CircuitError(f"{path}.param: '{m['param']}' cannot be modulated")
        if ("gain" in m) == ("scale_per_unit" in m):
            raise CircuitError(f"{path}: exactly one of 'gain' (value += gain * conc) or 'scale_per_unit' (value *= 1 + scale_per_unit * conc) is required")
        scale = "scale_per_unit" in m
        lo, hi = _num(m, "min", path), _num(m, "max", path)
        if lo is not None and hi is not None and lo > hi:
            raise CircuitError(f"{path}: min must be <= max")
        rows.append({"modulator": m["modulator"], "pop": m.get("pop"), "projection": proj, "param": m["param"], "scale": scale,
                     "amount": _num(m, "scale_per_unit" if scale else "gain", path), "min": lo, "max": hi})
    return rows


def _law(v: float, r: dict, conc: float) -> float:
    v = v * (1.0 + r["amount"] * conc) if r["scale"] else v + r["amount"] * conc
    if r["min"] is not None:
        v = max(v, r["min"])
    if r["max"] is not None:
        v = min(v, r["max"])
    return v


def _apply_modulation(net: "Network"):
    """Write the modulation table's values for the current modulator levels (rows on one (pop, param) or (projection, param) compose in file order)."""
    for (pid, param), base in net.mod_targets.items():
        v = base
        for r in net.modulation:
            if r["projection"] is None and r["pop"] == pid and r["param"] == param:
                v = _law(v, r, net.mod_levels[r["modulator"]])
        net.populations[pid][2].set({param: v})
    for (k, param), (conns, base) in net.proj_targets.items():
        rows = [r for r in net.modulation if r["projection"] == k and r["param"] == param]
        post_model = next(p["model"] for p in net.spec["populations"] if p["id"] == net.spec["projections"][k]["post"])
        lim = weight_limit(post_model)[0] if param == "weight" else 0.0
        vals = []
        for b in base:
            v = b
            for r in rows:
                v = _law(v, r, net.mod_levels[r["modulator"]])
            vals.append(max(-lim, min(lim, v)) if lim > 0 else v)
        conns.set({param: vals})
    if net.da_generator is not None:
        net.da_generator.set({"rate": max(0.0, net.mod_levels["DA"]) * DA_VT_HZ_PER_UNIT})


NESTML_MODELS = ("sa_node_pacemaker", "butera_burster", "icc_slow_wave")

# Seeded random initial V_m: per-model range override (engine/src/circuit.cpp vm_init_range); default range is [E_L, V_th].
VM_INIT_RANGE = {"sa_node_pacemaker": (-74.0, -45.0)}

# Default modulation (docs/CIRCUIT-FORMAT.md): a model with an `ne` / `ach` parameter follows the NE / ACh level (gain 1, floor 0)
# unless the circuit's "modulation" table has a row for that (population, parameter).
DEFAULT_MODULATION = (("NE", "ne"), ("ACh", "ach"))


def vm_init_range(model: str, e_l: float, v_th: float):
    """(lo, hi) of the random initial V_m for a model, or None when it has no range."""
    if model in VM_INIT_RANGE:
        return VM_INIT_RANGE[model]
    return (e_l, v_th) if v_th > e_l else None


def _register_nestml(spec: dict | None = None):
    """Register the NESTML cells (models/nestml/spike_nestml.py) if the PyNEST loader library is built.

    When the circuit (`spec`) uses a NESTML model that cannot be registered this raises CircuitError with the reason and the build
    step; circuits without NESTML models are unaffected (registration is skipped quietly)."""
    import sys

    d = str(Path(__file__).resolve().parents[2] / "models" / "nestml")
    if d not in sys.path:
        sys.path.append(d)
    failure = None
    try:
        import spike_nestml

        spike_nestml.register()
    except (ImportError, FileNotFoundError, OSError, IndexError, AttributeError) as e:
        failure = f"{type(e).__name__}: {e}"
    used = sorted({p["model"] for p in (spec or {}).get("populations", []) if p["model"] in NESTML_MODELS})
    if used:
        import nest

        missing = [m for m in used if m not in nest.node_models]
        if missing:
            raise CircuitError(
                f"populations: the circuit uses NESTML model(s) {', '.join(missing)}, but the NESTML models are not registered with PyNEST"
                + (f" ({failure})" if failure else "")
                + ". Build the PyNEST loader library with scripts/build-nestml.sh --pynest (or set SPIKE_NESTML_LIB to libspike_nestml_py.so)."
            )


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
    spec = apply_groups(spec)   # 0.6: cell-type groups (sign and scale) are applied to the weights at load, as in the C++ engine
    res = float(spec.get("resolution_ms", 0.1))
    check_limits(spec)
    check_supported(spec)
    sd = check_seed(spec["seed"] if "seed" in spec else 1) if seed is None else check_seed(seed, "seed")   # a missing seed is 1, as in the engine

    nest.ResetKernel()
    nest.verbosity = nest.VerbosityLevel.INFO if verbose else nest.VerbosityLevel.ERROR
    nest.resolution = res
    nest.rng_seed = sd
    nest.local_num_threads = int(threads)
    nest.print_time = False
    _register_nestml(spec)  # after ResetKernel and the thread change (both drop registered models)

    # 1. populations
    pops: dict[str, tuple[int, int, Any]] = {}
    first = 0
    all_nodes = None
    for p in spec["populations"]:
        nc = nest.Create(p["model"], int(p["n"]), params=dict(p.get("params") or {}) or None)
        # seeded random initial V_m (docs/CIRCUIT-FORMAT.md): only if params give no V_m and the model has V_m, E_L, V_th
        if "V_m" not in (p.get("params") or {}):
            st = nc[0].get()
            rng = vm_init_range(p["model"], float(st["E_L"]), float(st["V_th"])) if "V_m" in st and "E_L" in st and "V_th" in st else None
            if rng is not None:
                lo, hi = rng
                for j in range(int(p["n"])):
                    nc[j].set({"V_m": lo + initial_vm_fraction(sd, first + j) * (hi - lo)})
        pops[p["id"]] = (first, int(p["n"]), nc)
        first += int(p["n"])
        all_nodes = nc if all_nodes is None else all_nodes + nc
    n_neurons = first

    # 2. devices, 3. recorder
    devices = {}
    sg_times: dict[str, list] = {}
    for i, inp in enumerate(spec.get("inputs", [])):
        model, params, times = _parse_input(inp, i, res)
        devices[inp["name"]] = nest.Create(model, params=params or None)
        if inp["kind"] == "spike_generator":
            sg_times[inp["name"]] = list(times)
    recorder = nest.Create("spike_recorder", params={"record_to": "memory"})

    base = all_nodes.tolist()[0]

    def nodes(idx: list[int]):
        return nest.NodeCollection([base + i for i in idx])

    prepared = [_prepare_projection(spec, pr, k, sd) for k, pr in enumerate(spec.get("projections", []))]
    modulation = _parse_modulation(spec)
    for pid, (_, _, pnc) in pops.items():  # default modulation (after the circuit's own rows, like the engine)
        st0 = pnc[0].get()
        for mod, par in DEFAULT_MODULATION:
            if par in st0 and not any(r["projection"] is None and r["pop"] == pid and r["param"] == par for r in modulation):
                modulation.append({"modulator": mod, "pop": pid, "projection": None, "param": par, "scale": False, "amount": 1.0, "min": 0.0, "max": None})

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
    proj_nc: dict[int, tuple] = {}
    for k, (pr, pp) in enumerate(zip(spec.get("projections", []), prepared)):
        pre_first, pre_n, pre_nc = pops[pr["pre"]]
        post_first, post_n, post_nc = pops[pr["post"]]
        syn = dict(pp["syn"])
        if pp["dopamine"]:
            model = f"stdp_dopamine_synapse__p{k}"
            nest.CopyModel("stdp_dopamine_synapse", model, {**pp["common"], "volume_transmitter": vts[pp["dopamine_pop"]]})
            syn["synapse_model"] = model
        elif any(j != k and q["pre"] == pr["pre"] and q["post"] == pr["post"] and prepared[j]["syn"]["synapse_model"] == syn["synapse_model"]
                 for j, q in enumerate(spec["projections"])):
            # another projection joins the same populations with the same synapse model: a private model copy tells their connections apart (as in the engine)
            model = f"{syn['synapse_model']}__p{k}"
            nest.CopyModel(syn["synapse_model"], model)
            syn["synapse_model"] = model
        proj_nc[k] = (pre_nc, post_nc, syn["synapse_model"])
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
            if pp["spread"] is not None:  # per connection, canonical (source, target, port) order (same as the engine)
                kind, amount = pp["spread"]
                conns = nest.GetConnections(pre_nc, post_nc, syn["synapse_model"])
                if len(conns):
                    st = conns.get(["source", "target", "port", "weight"])
                    keys = list(zip(st["source"], st["target"], st["port"]))
                    order = sorted(range(len(keys)), key=lambda j: keys[j])
                    new = [0.0] * len(keys)
                    for rank, j in enumerate(order):
                        new[j] = spread_weight(float(st["weight"][j]), kind, amount, sd, k, rank)
                    conns.set(weight=new)

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

    net = Network(spec, res, sd, int(threads), n_neurons, all_nodes, pops, devices, recorder, modulation=modulation, da_generator=da_generator,
                  proj_nc=proj_nc, sg_times=sg_times)
    # modulation table: remember each target parameter's base value (after the population's params), then apply the zero-level state
    for r in modulation:
        if r["projection"] is not None:  # synapse row: each connection's own base value
            key = (r["projection"], r["param"])
            if key not in net.proj_targets:
                pre_nc, post_nc, model = proj_nc[r["projection"]]
                conns = nest.GetConnections(pre_nc, post_nc, model)
                if len(conns):
                    try:
                        st = conns.get(r["param"])
                    except Exception as e:  # PyNEST raises for an unknown synapse parameter
                        raise CircuitError(f"modulation.param: synapse model {model} has no parameter '{r['param']}'") from e
                    base = [float(x) for x in (st if isinstance(st, (list, tuple, np.ndarray)) else [st])]
                    net.proj_targets[key] = (conns, base)
            continue
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
