#!/usr/bin/env bash
# Headless test of Plate 0's first-run logic (godot/tests/test_tutorial.gd): a fresh save starts the tutorial at step 1, a skipped or finished one never restarts,
# the mode picker is owed until a mode was chosen, the step text has no digits. Needs only the godot binary. Exit 77 (ctest SKIP) when godot is missing.
#   scripts/tutorial_test.sh
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
GODOT=${GODOT:-godot}
command -v "$GODOT" > /dev/null 2>&1 || { echo "SKIP tutorial: godot ($GODOT) not found"; exit 77; }
tmp=$(mktemp -d); trap 'chmod -R u+rwx "$tmp" 2> /dev/null; rm -rf "$tmp"' EXIT
export XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config" XDG_CACHE_HOME="$tmp/cache"; mkdir -p "$XDG_DATA_HOME"
env -u SP_NOAUTOSAVE "$GODOT" --headless --path godot --import > "$tmp/import.log" 2>&1
log="$tmp/log"
timeout 300 "$GODOT" --headless --path godot -s tests/test_tutorial.gd > "$log" 2>&1
status=$?
grep -E "^TUTORIAL|SCRIPT ERROR|Parse Error" "$log" | grep -v "Identifier \"Spike\(Sim\|Task\)\" not declared"
if grep -q "^TUTORIAL SKIP" "$log"; then exit 77; fi
if ! grep -q "^TUTORIAL OK" "$log"; then tail -30 "$log"; echo "FAIL tutorial: no TUTORIAL OK line (exit $status)"; exit 1; fi
if grep -E "SCRIPT ERROR|Parse Error" "$log" | grep -vq "Identifier \"Spike\(Sim\|Task\)\" not declared"; then echo "FAIL tutorial: script errors"; exit 1; fi
[ "$status" = 0 ] || { echo "FAIL tutorial: exit status $status"; exit 1; }
echo "tutorial: all good"

