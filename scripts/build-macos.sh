#!/usr/bin/env bash
# Build the macOS (Apple Silicon by default) pieces: NEST (static, pinned) -> NESTML sources -> godot-cpp -> release extension + spike-run.
#   scripts/build-macos.sh [all|nest|nestml|gd]     (default all; the stages are separate so CI can cache the NEST tree after "nest")
# Output: godot/bin/libspike_gd_release.dylib and godot/bin/spike-run
# Pins are read from the Linux scripts so each pin lives in one place: NEST_REF (build-nest.sh), NESTML_VERSION (build-nestml.sh), GODOT_CPP_TAG (build-gd.sh).
# Needs Homebrew: cmake ninja gsl python3 (the NEST configure wants Cython at build time; it is installed into the NESTML venv). Env: SPIKE_ARCH (default arm64).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
stage=${1:-all}
ARCH=${SPIKE_ARCH:-$(uname -m)}
pin() { sed -n "s/^$2=\"\${$2:-\([^}]*\)}\".*/\1/p" "scripts/$1" | head -1; }
NEST_REF=$(pin build-nest.sh NEST_REF); NESTML_VERSION=$(pin build-nestml.sh NESTML_VERSION)
GODOT_CPP_TAG=$(sed -n 's/^GODOT_CPP_TAG=\${GODOT_CPP_TAG:-\([^}]*\)}.*/\1/p' scripts/build-gd.sh | head -1)
[ -n "$NEST_REF" ] && [ -n "$NESTML_VERSION" ] && [ -n "$GODOT_CPP_TAG" ] || { echo "could not read the pins from the Linux scripts" >&2; exit 1; }
echo "pins: NEST $NEST_REF, NESTML $NESTML_VERSION, godot-cpp $GODOT_CPP_TAG, arch $ARCH"
JOBS=$(sysctl -n hw.ncpu)
export MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-12.0}
GSL_PREFIX=$(brew --prefix gsl)
# a stable Python for the venv (the brew default can be a pre-release with no wheels for NESTML's dependencies)
[ -d "$(brew --prefix python@3.12 2>/dev/null)/bin" ] && export PATH="$(brew --prefix python@3.12)/bin:$PATH"
OSX=(-DCMAKE_OSX_ARCHITECTURES="$ARCH")   # the deployment target comes from MACOSX_DEPLOYMENT_TARGET (NEST rejects it as a -D option)
SRC=$root/third_party/nest-simulator; BUILD=$root/third_party/nest-build
VENV=$root/third_party/nestml-venv

venv() {  # NESTML venv (pinned) via the Linux script, plus Cython/numpy for the NEST configure
  export ROOT="$root" SPIKE_NO_PREBUILT=1
  scripts/build-nestml.sh --venv-only
  "$VENV/bin/pip" install -q cython numpy
}

do_nest() {
  local stamp="$NEST_REF $ARCH deploy=$MACOSX_DEPLOYMENT_TARGET $(shasum "$0" | cut -d' ' -f1)"
  if [ "$(cat "$BUILD/.spike-macos-stamp" 2>/dev/null)" = "$stamp" ]; then echo "NEST: up to date ($NEST_REF)"; return; fi
  venv
  if [ ! -d "$SRC/.git" ]; then git clone -q https://github.com/nest/nest-simulator.git "$SRC"; fi
  git -C "$SRC" fetch -q --depth 1 origin "$NEST_REF" 2>/dev/null || true
  git -C "$SRC" checkout -q "$NEST_REF"
  mkdir -p "$BUILD"
  ( cd "$BUILD"
    PATH="$VENV/bin:$PATH" cmake -G Ninja "$SRC" "${OSX[@]}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$GSL_PREFIX" \
      -DPython_EXECUTABLE="$VENV/bin/python" -DPython3_EXECUTABLE="$VENV/bin/python" \
      -Dwith-mpi=OFF -Dwith-openmp=OFF -Dwith-boost=OFF -Dwith-ltdl=OFF -Dwith-readline=OFF -Dwith-gsl=ON
    ninja -j"$JOBS" models nestkernel nestutil )
  echo "$stamp" > "$BUILD/.spike-macos-stamp"
  echo "NEST static libs:"; find "$BUILD" -name '*.a'
}

do_nestml() { export ROOT="$root" SPIKE_NO_PREBUILT=1; scripts/build-nestml.sh; }

do_gd() {
  if [ ! -f third_party/godot-cpp/CMakeLists.txt ]; then
    git clone --depth 1 --branch "$GODOT_CPP_TAG" https://github.com/godotengine/godot-cpp third_party/godot-cpp
  fi
  local b=build/gd-release-macos
  cmake -S . -B "$b" -G Ninja "${OSX[@]}" -DCMAKE_BUILD_TYPE=Release -DSPIKE_BUILD_TESTS=OFF -DSPIKE_BUILD_GDEXT=ON \
    -DGODOTCPP_TARGET=template_release -DCMAKE_PREFIX_PATH="$GSL_PREFIX" -DGODOTCPP_ARCH="$ARCH"
  cmake --build "$b" -j"$JOBS" --target spike_gd spike-run
  mkdir -p godot/bin
  cp -f "$b/gd-out/libspike_gd.dylib" godot/bin/libspike_gd_release.dylib
  cp -f "$b/engine/spike-run" godot/bin/spike-run
  file godot/bin/libspike_gd_release.dylib godot/bin/spike-run
  echo "built godot/bin/libspike_gd_release.dylib and godot/bin/spike-run"
}

case $stage in
  nest) do_nest ;;
  nestml) do_nestml ;;
  gd) do_gd ;;
  all) do_nest; do_nestml; do_gd ;;
  *) echo "usage: $0 [all|nest|nestml|gd]" >&2; exit 2 ;;
esac
