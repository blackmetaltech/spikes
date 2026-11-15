#!/usr/bin/env bash
# Headless smoke test of the Godot game: build the GDExtension, import the project, run the SP_SMOKE app test, assert on its output.
#   scripts/smoke.sh
# Boots every screen in every layout, checks the font floor (>= 14 px on screen), loads the GDExtension, creates a SpikeSim and steps it.
# Exit status is non-zero if any step fails; nothing is masked.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
log=$(mktemp); trap 'rm -f "$log"' EXIT
fail=0
cargo build -p spike-gd || { echo "FAIL: cargo build -p spike-gd"; exit 1; }
mkdir -p godot/bin && cp -f target/debug/libspike_gd.so godot/bin/libspike_gd_debug.so || { echo "FAIL: copying the extension"; exit 1; }
godot --headless --path godot --import > "$log" 2>&1
rc=$?
if [ "$rc" != 0 ] || grep -E "SCRIPT ERROR|Parse Error" "$log"; then cat "$log"; echo "FAIL: project import (exit $rc)"; exit 1; fi
SP_SMOKE=1 SP_NOAUTOSAVE=1 timeout 180 godot --headless --path godot 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
grep -q "SMOKE OK" "$log" || { echo "FAIL: no SMOKE OK line"; fail=1; }
grep -q "^EXTENSION OK" "$log" || { echo "FAIL: GDExtension did not load / SpikeSim did not run"; fail=1; }
grep -q "^FONTS below 14 px: 0" "$log" || { echo "FAIL: text below the readable minimum"; fail=1; }
if grep -E "SCRIPT ERROR|Parse Error|SMOKE FAIL|^ERROR:" "$log"; then echo "FAIL: errors in the log"; fail=1; fi
[ "$status" = 0 ] || { echo "FAIL: exit status $status"; fail=1; }
[ "$fail" = 0 ] && echo "smoke.sh: all good" || exit 1
