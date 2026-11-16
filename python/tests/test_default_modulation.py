"""Per-model default modulation, the per-model initial V_m range and the loud NESTML registration failure (docs/CIRCUIT-FORMAT.md)."""
import pytest

from conftest import requires_nest

pytestmark = requires_nest


def _sa(modulation=None, n=1, params=None):
    c = {
        "schema_version": 1, "seed": 2, "resolution_ms": 0.1,
        "populations": [{"id": "SA", "model": "sa_node_pacemaker", "n": n, "params": params or {}}],
        "inputs": [], "outputs": [{"name": "beat", "kind": "rate", "neurons": ["SA"]}],
    }
    if modulation is not None:
        c["modulation"] = modulation
    return c


def _need_sa():
    import nest

    import spike

    try:
        spike.build(_sa())
    except spike.circuit.CircuitError as e:
        import os

        if os.environ.get("SPIKE_REQUIRE_NEST", "") not in ("", "0"):
            pytest.fail(str(e))
        pytest.skip("NESTML PyNEST library not built")
    return nest


def test_default_modulation_follows_global_levels():
    import spike

    nest = _need_sa()
    net = spike.build(_sa())  # no modulation table at all
    net.set_modulator("ACh", 0.5)
    net.set_modulator("NE", 0.25)
    st = net.population("SA").get()
    assert st["ach"] == pytest.approx(0.5) and st["ne"] == pytest.approx(0.25)
    net.set_modulator("ACh", 0.0)
    assert net.population("SA").get("ach") == 0.0
    # the implicit rows are floored at 0 and gain 1
    assert [(r["modulator"], r["param"], r["amount"], r["min"]) for r in net.modulation] == [("NE", "ne", 1.0, 0.0), ("ACh", "ach", 1.0, 0.0)]


def test_table_overrides_per_parameter():
    import spike

    _need_sa()
    net = spike.build(_sa([{"modulator": "ACh", "pop": "SA", "param": "ach", "gain": 0.5}]))
    net.set_modulator("ACh", 1.0)
    net.set_modulator("NE", 1.0)
    st = net.population("SA").get()
    assert st["ach"] == pytest.approx(0.5)  # the circuit's row wins
    assert st["ne"] == pytest.approx(1.0)  # ne keeps the default
    assert len(net.modulation) == 2


def test_models_without_ne_ach_are_untouched():
    import spike

    net = spike.build({"schema_version": 1, "seed": 1, "populations": [{"id": "A", "model": "iaf_psc_alpha", "n": 1}]})
    assert net.modulation == []


def test_sa_initial_vm_range():
    import spike

    _need_sa()
    net = spike.build(_sa(n=12))
    vm = list(net.neurons.get("V_m"))
    assert len(set(vm)) == 12 and all(-74.0 <= v < -45.0 for v in vm)
    net = spike.build(_sa(n=3, params={"V_m": -60.0}))
    assert set(net.neurons.get("V_m")) == {-60.0}


def test_missing_nestml_is_an_error(monkeypatch):
    import spike

    nest = spike.circuit._import_nest()
    import spike_nestml

    if "sa_node_pacemaker" not in nest.node_models:  # registration state is dropped by ResetKernel in build(); make register() fail
        pass
    monkeypatch.setattr(spike_nestml, "register", lambda: (_ for _ in ()).throw(FileNotFoundError("libspike_nestml_py.so not found")))
    with pytest.raises(spike.circuit.CircuitError, match="NESTML.*not registered.*build-nestml.sh --pynest"):
        spike.build(_sa())
    # a circuit without NESTML models builds fine
    spike.build({"schema_version": 1, "seed": 1, "populations": [{"id": "A", "model": "iaf_psc_alpha", "n": 1}]})
