"""Test setup: put PyNEST (from the repo's NEST build, or /tmp/nest-install) and the spike package on sys.path."""
import glob
import os
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
sys.path.insert(0, str(HERE.parent))


def _pynest_dirs():
    env = os.environ.get("SPIKE_PYNEST")
    if env:
        yield env
    yield from sorted(glob.glob(str(REPO / "third_party/nest-install/lib/python3*/site-packages")))
    yield from sorted(glob.glob("/tmp/nest-install/lib/python3*/site-packages"))


for d in _pynest_dirs():
    if os.path.isdir(os.path.join(d, "nest")):
        if d not in sys.path:
            sys.path.append(d)
        break


def have_nest():
    try:
        import nest  # noqa: F401
        return True
    except Exception:
        return False


def find_exe(name, env):
    if os.environ.get(env):
        return os.environ[env]
    for pat in (f"build*/**/{name}", f"build*/{name}", f"cmake-build*/**/{name}"):
        for p in sorted(glob.glob(str(REPO / pat), recursive=True)):
            if os.path.isfile(p) and os.access(p, os.X_OK):
                return p
    return None


@pytest.fixture(scope="session")
def circuits_dir():
    return HERE / "circuits"


@pytest.fixture(scope="session")
def spike_run_exe():
    return find_exe("spike-run", "SPIKE_RUN")


@pytest.fixture(scope="session")
def analysis_test_exe():
    return find_exe("analysis_test", "SPIKE_ANALYSIS_TEST")


_REQUIRE = os.environ.get("SPIKE_REQUIRE_NEST", "") not in ("", "0")


def pytest_configure(config):
    config.addinivalue_line("markers", "requires_nest_mark: needs PyNEST (skips, or fails with SPIKE_REQUIRE_NEST=1)")


def _need_nest():
    if not have_nest():
        if _REQUIRE:
            pytest.fail("SPIKE_REQUIRE_NEST=1 but PyNEST is not importable (run scripts/build-nest.sh --pynest or set SPIKE_PYNEST)")
        pytest.skip("PyNEST not importable (set SPIKE_PYNEST to the NEST site-packages)")


@pytest.fixture(autouse=True)
def _nest_gate(request):
    if request.node.get_closest_marker("requires_nest_mark"):
        _need_nest()


requires_nest = pytest.mark.requires_nest_mark
