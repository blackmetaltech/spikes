#!/usr/bin/env python3
"""Scale benchmark for the Atlas size cap: random circuits with explicit `edges` projections, run through spike-run.

Each circuit: N iaf_psc_alpha cells, 80 % excitatory (E) and 20 % inhibitory (I), every cell has IN_DEGREE = 50 inputs (40 from E, 10 from I,
random sources, no self-connection, delay 1.5 ms, listed as `edges`), and a Poisson drive (RATE_HZ to every cell) that holds the cells at a few Hz.
Reported per size: connections, load time (wall time of a 1 ms run: process start, NEST start, circuit build), seconds of wall clock per simulated
second (from a SIM_MS run minus the load time), and the mean firing rate (spikes counted by spike-run). Best of REPEATS runs (the smallest time).
The machine's load average, CPU model and the thread count are printed with the table.

Usage: python3 tools/bench/scale.py [--bin build/engine/spike-run] [--sizes 500,3000,10000] [--threads 1] [--sim-ms 2000] [--keep dir]
"""
import argparse, json, os, platform, random, re, subprocess, sys, tempfile, time

IN_DEGREE, FRAC_E, RATE_HZ, DRIVE_PA, W_E, W_I, DELAY, REPEATS = 50, 0.8, 3000.0, 18.5, 25.0, -100.0, 1.5, 3


def make(n, seed=11):
    rnd = random.Random(seed)
    ne = int(n * FRAC_E); ni = n - ne
    k_e, k_i = int(IN_DEGREE * FRAC_E), IN_DEGREE - int(IN_DEGREE * FRAC_E)
    def edges(npre, npost, k, w, same):
        out = []
        for post in range(npost):
            src = set()
            while len(src) < k:
                s = rnd.randrange(npre)
                if not (same and s == post):
                    src.add(s)
            out += [[s, post, w, DELAY] for s in sorted(src)]
        return out
    pops = [{"id": "E", "model": "iaf_psc_alpha", "n": ne, "params": {}, "role": "neuron"},
            {"id": "I", "model": "iaf_psc_alpha", "n": ni, "params": {}, "role": "neuron"}]
    proj = []
    for pre, post, npre, npost, k, w in (("E", "E", ne, ne, k_e, W_E), ("E", "I", ne, ni, k_e, W_E), ("I", "E", ni, ne, k_i, W_I), ("I", "I", ni, ni, k_i, W_I)):
        proj.append({"pre": pre, "post": post, "edges": edges(npre, npost, k, w, pre == post), "synapse": {"synapse_model": "static_synapse"}})
    return {"schema_version": 1, "name": "bench %d" % n, "resolution_ms": 0.1, "seed": seed, "populations": pops, "projections": proj,
            "inputs": [{"name": "drive", "kind": "poisson", "targets": ["E", "I"], "weight": DRIVE_PA, "value": RATE_HZ}],
            "outputs": [], "record": {"spikes": "all"}, "meta": {"author": "tools/bench/scale.py", "notes": "random benchmark circuit"}}


def run(binp, path, ms, threads):
    t = time.perf_counter()
    p = subprocess.run([binp, path, str(ms), "--threads", str(threads)], capture_output=True, text=True)
    dt = time.perf_counter() - t
    if p.returncode != 0:
        raise SystemExit("spike-run failed: " + p.stderr[-500:])
    m = re.search(r"spikes=(\d+)", p.stdout + p.stderr)
    return dt, int(m.group(1)) if m else -1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", default=os.path.join(os.path.dirname(__file__), "..", "..", "build", "engine", "spike-run"))
    ap.add_argument("--sizes", default="500,3000,10000")
    ap.add_argument("--threads", type=int, default=1)
    ap.add_argument("--sim-ms", type=int, default=2000)
    ap.add_argument("--keep")
    a = ap.parse_args()
    cpu = next((l.split(":", 1)[1].strip() for l in open("/proc/cpuinfo") if l.startswith("model name")), platform.processor())
    print("machine: %s; %d logical CPUs; load average at start %s; engine threads %d" % (cpu, os.cpu_count(), " ".join("%.2f" % x for x in os.getloadavg()), a.threads))
    rows = []
    with tempfile.TemporaryDirectory() as td:
        for n in [int(x) for x in a.sizes.split(",")]:
            c = make(n)
            ncon = sum(len(p["edges"]) for p in c["projections"])
            path = os.path.join(a.keep or td, "bench_%d.json" % n)
            os.makedirs(os.path.dirname(path), exist_ok=True)
            json.dump(c, open(path, "w"))
            loads = [run(a.bin, path, 1, a.threads)[0] for _ in range(REPEATS)]
            sims = [run(a.bin, path, a.sim_ms, a.threads) for _ in range(REPEATS)]
            load = min(loads); best = min(s[0] for s in sims)
            spk = sims[0][1]
            per = (best - load) / (a.sim_ms / 1000.0)
            rows.append((n, ncon, load, per, spk / n / (a.sim_ms / 1000.0)))
            print("  %6d cells %8d connections: load %.2f s, %.2f s per simulated second (real-time factor %.2f), mean rate %.1f Hz; load loads %s, sim totals %s" %
                  (n, ncon, load, per, 1.0 / per if per > 0 else float("inf"), rows[-1][4], ["%.2f" % x for x in loads], ["%.2f" % s[0] for s in sims]), flush=True)
    print("\n| cells | connections | load (s) | wall seconds per simulated second | real-time factor | mean rate (Hz) |\n|---|---|---|---|---|---|")
    for n, ncon, load, per, hz in rows:
        print("| %d | %d | %.2f | %.2f | %.2f | %.1f |" % (n, ncon, load, per, 1.0 / per if per > 0 else float("inf"), hz))
    print("\nload average at end", " ".join("%.2f" % x for x in os.getloadavg()))


if __name__ == "__main__":
    main()
