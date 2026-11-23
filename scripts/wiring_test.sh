#!/usr/bin/env bash
# The Analysis screen's wiring matrix, path finder and null-model bench (godot/tests/test_wiring.gd): matrix image and marks, orders and levels, the path text
# for a chain / a silenced middle / no path, the three null models through the engine, the return to the lab with a cell type selected.
# Needs the godot binary and the built GDExtension (scripts/build-gd.sh). Exit 77 (ctest SKIP) when they are missing.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
GODOT=${GODOT:-godot}
command -v "$GODOT" > /dev/null 2>&1 || { echo "SKIP wiring: godot not found"; exit 77; }
[ -f godot/bin/libspike_gd_debug.so ] || [ -f godot/bin/libspike_gd_release.so ] || { echo "SKIP wiring: the GDExtension is not built (scripts/build-gd.sh)"; exit 77; }
SP_NOAUTOSAVE=1 "$GODOT" --headless --path godot --import > /dev/null 2>&1
out=$(SP_NOAUTOSAVE=1 timeout 600 "$GODOT" --headless --path godot -s tests/test_wiring.gd 2>&1)
echo "$out" | grep -E "^WIRING|SCRIPT ERROR|Parse Error|^ERROR:"
echo "$out" | grep -q "^WIRING OK" || { echo "$out" | tail -30; exit 1; }
