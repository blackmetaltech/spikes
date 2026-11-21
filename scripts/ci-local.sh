#!/usr/bin/env bash
# Run what .github/workflows/ci.yml runs, locally, in the same toolchain image.
#   scripts/ci-local.sh [dir]        dir = checkout to test (default: this one; use a clean worktree for a faithful run)
# The image is spike-build:<tag> (tag = scripts/toolchain-tag.sh). It is used if present locally, else pulled from ghcr.io/ixaxaar/spike-build,
# else built from the Dockerfile (cold ~1.5 h; cached afterwards). Set SPIKE_IMAGE to use another image.
# Prints "STEP <name> FAILED" for each failing step and exits non-zero if any failed.
set -uo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
dir=$(cd "${1:-$here}" && pwd)
tag=$("$here/scripts/toolchain-tag.sh")
img=${SPIKE_IMAGE:-spike-build:$tag}
if ! docker image inspect "$img" >/dev/null 2>&1; then
  docker pull "ghcr.io/ixaxaar/spike-build:$tag" 2>/dev/null && docker tag "ghcr.io/ixaxaar/spike-build:$tag" "$img" \
    || docker build -t "$img" "$here" || { echo "STEP image FAILED"; exit 10; }
fi
cd "$dir"
R=(docker run --rm -v "$PWD":/src -w /src "$img")
bad=0
step() { local n=$1; shift; echo "=== $n"; "$@" || { echo "STEP $n FAILED"; bad=1; }; }
step build  "${R[@]}" scripts/build-all.sh --tests --debug --pynest
step ctest  "${R[@]}" bash -c 'cd build && ctest --output-on-failure -j"$(nproc)"'
step pytest "${R[@]}" bash -c 'export SPIKE_RUN=/src/godot/bin/spike-run; python3 -m pytest -q python/tests'
step smoke  "${R[@]}" scripts/smoke.sh
step package "${R[@]}" scripts/package.sh
[ -f build/spike-linux-x86_64.zip ] || { echo "STEP artifact FAILED"; bad=1; }
echo "ci-local: $([ $bad = 0 ] && echo OK || echo FAILED)"; exit $bad
