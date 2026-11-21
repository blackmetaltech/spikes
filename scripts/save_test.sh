#!/usr/bin/env bash
# Headless save-layer test (godot/tests/test_save.gd): atomic writes, backups, damaged / truncated / garbage / future progress files, hostile circuit files,
# the Reference overwrite and restart reference-lock repros, a read-only save folder, test hooks never saving, diagnosis persistence, save export / import.
#   scripts/save_test.sh
# Needs only the godot binary (the extension is not loaded: the test touches no engine code). Exit 77 (ctest SKIP) when godot is missing.
# The game's user:// folder is a scratch directory (XDG_DATA_HOME), so the real save is never read or written.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
GODOT=${GODOT:-godot}
command -v "$GODOT" > /dev/null 2>&1 || { echo "SKIP save_io: godot ($GODOT) not found"; exit 77; }
tmp=$(mktemp -d); trap 'chmod -R u+rwx "$tmp" 2> /dev/null; rm -rf "$tmp"' EXIT
export XDG_DATA_HOME="$tmp/data" XDG_CONFIG_HOME="$tmp/config" XDG_CACHE_HOME="$tmp/cache"; mkdir -p "$XDG_DATA_HOME"
env -u SP_NOAUTOSAVE "$GODOT" --headless --path godot --import > "$tmp/import.log" 2>&1
log="$tmp/log"
timeout 300 "$GODOT" --headless --path godot -s tests/test_save.gd > "$log" 2>&1
status=$?
grep -E "^SAVE|SCRIPT ERROR|Parse Error" "$log" | grep -v "Identifier \"Spike\(Sim\|Task\)\" not declared"
if grep -q "^SAVE SKIP" "$log"; then exit 77; fi
if ! grep -q "^SAVE OK" "$log"; then tail -30 "$log"; echo "FAIL save_io: no SAVE OK line (exit $status)"; exit 1; fi
if grep -E "SCRIPT ERROR|Parse Error" "$log" | grep -vq "Identifier \"Spike\(Sim\|Task\)\" not declared"; then echo "FAIL save_io: script errors"; exit 1; fi
[ "$status" = 0 ] || { echo "FAIL save_io: exit status $status"; exit 1; }
echo "save_io: all good"
