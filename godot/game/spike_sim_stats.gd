class_name SpikeSimStats
extends RefCounted
## Thin wrapper over the static SpikeSim.spike_stats (C++ analysis: rates, ISI CV, Golomb chi); a GDScript static method cannot name an extension class directly at parse time.
static func stats(pairs: PackedFloat64Array, n: int, t0: float, t1: float) -> Dictionary:
	if not ClassDB.class_exists("SpikeSim"): return {}
	var inst = ClassDB.instantiate("SpikeSim")
	return inst.call("spike_stats", pairs, n, t0, t1)
