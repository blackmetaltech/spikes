#!/usr/bin/env bash
# Build everything from a clean checkout, in order: NEST (static, pinned) -> NESTML models (pinned) -> engine + spike-run + release GDExtension.
#   scripts/build-all.sh            NEST, NESTML, release extension, spike-run  -> godot/bin/{libspike_gd_release.so,spike-run}
#   scripts/build-all.sh --tests    additionally configure and build the whole engine tree (build/: tests, benchmarks, spike-catalog) so `ctest` can run
#   scripts/build-all.sh --debug    additionally build the debug extension (used by scripts/smoke.sh)
#   scripts/build-all.sh --pynest   additionally build PyNEST (third_party/nest-install) + the NESTML PyNEST module, for python/ parity tests
# Each step is incremental; the whole thing runs in the Dockerfile image (docker run --rm -v "$PWD":/src spike-build scripts/build-all.sh).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
tests=0; debug=0; pynest=0
for a in "$@"; do case "$a" in --tests) tests=1 ;; --debug) debug=1 ;; --pynest) pynest=1 ;; *) echo "usage: $0 [--tests] [--debug] [--pynest]" >&2; exit 2 ;; esac; done
step() { echo; echo "=== $*"; }
step "NEST (static libs)"; scripts/build-nest.sh
[ "$pynest" = 1 ] && { step "NEST + PyNEST"; scripts/build-nest.sh --pynest; }
step "NESTML models"; scripts/build-nestml.sh
[ "$pynest" = 1 ] && { step "NESTML PyNEST module"; scripts/build-nestml.sh --pynest; }
step "GDExtension (release) + spike-run"; scripts/build-gd.sh release
[ "$debug" = 1 ] && { step "GDExtension (debug)"; scripts/build-gd.sh debug; }
if [ "$tests" = 1 ]; then
  step "engine tree (tests, spike-catalog)"
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSPIKE_BUILD_TESTS=ON -DSPIKE_BUILD_GDEXT=OFF >/dev/null
  cmake --build build -j"$(nproc)"
fi
echo; echo "build-all: done"; ls -la godot/bin
