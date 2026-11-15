#!/usr/bin/env bash
# Build the spike GDExtension (gdext/, godot-cpp + spike::engine) and copy it to godot/bin (git-ignored), where spike.gdextension looks.
#   scripts/build-gd.sh            debug build   -> godot/bin/libspike_gd_debug.so
#   scripts/build-gd.sh release    release build -> godot/bin/libspike_gd_release.so
# godot-cpp is cloned on first use into third_party/godot-cpp (tag godot-4.5-stable: the newest godot-cpp tag; the extension declares
# compatibility_minimum 4.5 and runs on Godot 4.7). Uses its own build dir (build/gd-<mode>) so it never collides with the engine's `build/`.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
mode=${1:-debug}
case $mode in
  debug) cfg=Debug ;;
  release) cfg=Release ;;
  *) echo "usage: $0 [debug|release]" >&2; exit 2 ;;
esac
if [ ! -f third_party/godot-cpp/CMakeLists.txt ]; then
  git clone --depth 1 --branch godot-4.5-stable https://github.com/godotengine/godot-cpp third_party/godot-cpp
fi
bdir=build/gd-$mode
mkdir -p build
cmake -S . -B "$bdir" -DCMAKE_BUILD_TYPE=$cfg -DSPIKE_BUILD_TESTS=ON -DSPIKE_BUILD_GDEXT=ON >"$bdir.configure.log" 2>&1 || { cat "$bdir.configure.log"; exit 1; }
cmake --build "$bdir" -j"$(nproc)" --target spike_gd
mkdir -p godot/bin
cp -f "$bdir/gd-out/libspike_gd.so" "godot/bin/libspike_gd_$mode.so"
echo "built godot/bin/libspike_gd_$mode.so"
