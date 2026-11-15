#!/usr/bin/env bash
# Build the spike-gd GDExtension and copy it into godot/bin (git-ignored), where spike.gdextension looks for it.
#   scripts/build-gd.sh            debug build   -> godot/bin/libspike_gd_debug.so
#   scripts/build-gd.sh release    release build -> godot/bin/libspike_gd_release.so
# Linux only for now (see godot/spike.gdextension for the other platforms to add).
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
mode=${1:-debug}
case $mode in
  debug) cargo build -p spike-gd ;;
  release) cargo build -p spike-gd --release ;;
  *) echo "usage: $0 [debug|release]" >&2; exit 2 ;;
esac
mkdir -p godot/bin
cp -f "target/$mode/libspike_gd.so" "godot/bin/libspike_gd_$mode.so"
echo "built godot/bin/libspike_gd_$mode.so"
