#!/usr/bin/env bash
# Build the spike GDExtension (gdext/, godot-cpp + spike::engine) and copy it to godot/bin (git-ignored), where spike.gdextension looks.
#   scripts/build-gd.sh            debug build   -> godot/bin/libspike_gd_debug.so
#   scripts/build-gd.sh release    release build -> godot/bin/libspike_gd_release.so
# godot-cpp is cloned on first use into third_party/godot-cpp at the pinned tag GODOT_CPP_TAG (godot-4.5-stable: the newest godot-cpp tag; the
# extension declares compatibility_minimum 4.5 and runs on Godot 4.7). An existing checkout at another tag is switched to the pin. Uses its own build dir (build/gd-<mode>) so it never collides with the engine's `build/`.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
mode=${1:-debug}
case $mode in
  debug) cfg=Debug; gdtarget=template_debug ;;
  release) cfg=Release; gdtarget=template_release ;;
  *) echo "usage: $0 [debug|release]" >&2; exit 2 ;;
esac
GODOT_CPP_TAG=${GODOT_CPP_TAG:-godot-4.5-stable}
if [ ! -f third_party/godot-cpp/CMakeLists.txt ]; then
  git clone --depth 1 --branch "$GODOT_CPP_TAG" https://github.com/godotengine/godot-cpp third_party/godot-cpp
elif [ "$(git -C third_party/godot-cpp describe --tags --exact-match 2>/dev/null || true)" != "$GODOT_CPP_TAG" ]; then
  git -C third_party/godot-cpp fetch -q --depth 1 origin tag "$GODOT_CPP_TAG" && git -C third_party/godot-cpp checkout -q "$GODOT_CPP_TAG"
fi
bdir=build/gd-$mode
mkdir -p build
cmake -S . -B "$bdir" -DCMAKE_BUILD_TYPE=$cfg -DSPIKE_BUILD_TESTS=ON -DSPIKE_BUILD_GDEXT=ON -DGODOTCPP_TARGET=$gdtarget >"$bdir.configure.log" 2>&1 || { cat "$bdir.configure.log"; exit 1; }
cmake --build "$bdir" -j"$(nproc)" --target spike_gd spike-run
mkdir -p godot/bin
cp -f "$bdir/gd-out/libspike_gd.so" "godot/bin/libspike_gd_$mode.so"
cp -f "$bdir/engine/spike-run" godot/bin/spike-run   # CHECK runs it from next to the extension
echo "built godot/bin/libspike_gd_$mode.so and godot/bin/spike-run"
