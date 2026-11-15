class_name DemoCircuit
extends RefCounted
## A tiny placeholder circuit for the lab screen and the smoke test: six excitatory and two inhibitory neurons in a ring with feedback inhibition.
## Replaced by real circuits (spike-core JSON, versioned schema) when the editor lands. Weights are in the stub engine's units (nS-ish).
const KINDS := ["E", "E", "E", "E", "E", "E", "I", "I"]

static func json() -> String:
	var neurons := []
	for i in KINDS.size(): neurons.append({"id": i, "kind": "lif", "type": KINDS[i]})
	var syn := []
	for i in 6: syn.append({"pre": i, "post": (i + 1) % 6, "weight": 1.6, "delay_ms": 2.0})
	for i in [1, 4]:
		syn.append({"pre": i, "post": 6 if i == 1 else 7, "weight": 2.0, "delay_ms": 1.0})
	syn.append({"pre": 6, "post": 3, "weight": -2.5, "delay_ms": 1.5}); syn.append({"pre": 7, "post": 0, "weight": -2.5, "delay_ms": 1.5})
	return JSON.stringify({"version": 0, "neurons": neurons, "synapses": syn, "inputs": [{"channel": 0, "target": 0}, {"channel": 1, "target": 3}]})
