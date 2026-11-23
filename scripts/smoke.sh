#!/usr/bin/env bash
# Headless smoke test of the Godot game: build the GDExtension, import the project, run the SP_SMOKE app test, assert on its output.
#   scripts/smoke.sh
# Boots every screen in every layout, checks the font floor (>= 14 px on screen), loads the GDExtension, runs every task for 1 s on its reference
# circuit, opens the lab on each task and edits a circuit in it, then the v0.2 checks: campaign map and chapters, parts bin and catalogue reader,
# a real lesion re-run, the body views on synthetic tasks (SP_MOCK), the lesion lab and the sandbox; then, per real task, the empty bench the lab
# builds (channel kinds equal the reference's), a minimal circuit that moves the task's modulators, the real body view (no "Waiting for" placeholder),
# and finally every reference circuit scored headless with `spike-run --task <id>` (it must pass on at least one of seeds 1, 2, 3).
# Exit status is non-zero if any step fails; nothing is masked.
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
log=$(mktemp); trap 'rm -f "$log"' EXIT
fail=0
scripts/build-gd.sh debug || { echo "FAIL: building the GDExtension (scripts/build-gd.sh)"; exit 1; }
cmake --build build/gd-debug --target spike-run -j"$(nproc)" > "$log" 2>&1 || { cat "$log"; echo "FAIL: building spike-run"; exit 1; }
godot --headless --path godot --import > "$log" 2>&1
rc=$?
if [ "$rc" != 0 ] || grep -E "SCRIPT ERROR|Parse Error" "$log"; then cat "$log"; echo "FAIL: project import (exit $rc)"; exit 1; fi
SP_SMOKE=1 SP_NOAUTOSAVE=1 timeout 900 godot --headless --path godot 2>&1 | tee "$log"
status=${PIPESTATUS[0]}
grep -q "SMOKE OK" "$log" || { echo "FAIL: no SMOKE OK line"; fail=1; }
grep -q "^EXTENSION OK" "$log" || { echo "FAIL: GDExtension did not load / the tasks did not run"; fail=1; }
grep -q "^V02 OK" "$log" || { echo "FAIL: the v0.2 checks (campaign, parts bin, lesion lab, body views, sandbox) did not finish"; fail=1; }
grep -q "^TASKS OK" "$log" || { echo "FAIL: the per-task checks (skeleton kinds, modulators, body views) did not finish"; fail=1; }
grep -q "^FONTS below 14 px: 0" "$log" || { echo "FAIL: text below the readable minimum"; fail=1; }
if grep -E "SCRIPT ERROR|Parse Error|SMOKE FAIL|^ERROR:" "$log"; then echo "FAIL: errors in the log"; fail=1; fi
[ "$status" = 0 ] || { echo "FAIL: exit status $status"; fail=1; }
# every reference passes its task, headless, the way CI scores a circuit (a seed that fails is retried on the next: references have narrow plateaus)
run=build/gd-debug/engine/spike-run
tasks=$("$run" --list-tasks) || { echo "FAIL: spike-run --list-tasks"; fail=1; tasks=""; }
for id in $tasks; do
  ok=0
  extra=()
  # diagnosis cases are decided by the submitted answer (set_variant), not by the circuit. The answer is NOT read from the C++ source: nothing
  # submitted must fail, then every candidate the case offers (spike-run --list-variants) is submitted: exactly one must pass. The hints no longer
  # name candidates (that leaked the answer), so the tier-3 hint must not contain any candidate id either.
  # a case is a task of level kind "diagnosis" (the dx_* cases and the prediction bets a_motif and a_predict), not a task whose id starts with dx_
  if [ "$("$run" --level-kind "$id" 2>/dev/null)" = "diagnosis" ]; then
    if SPIKE_CIRCUITS_DIR="$root/circuits" "$run" --task "$id" --seed 1 --quick --require any > /dev/null 2>&1; then
      echo "FAIL: $id passes with nothing submitted"; fail=1; continue
    fi
    cands=$("$run" --list-variants "$id" 2>/dev/null)
    ncand=$(printf '%s\n' "$cands" | grep -c .)
    if [ "$ncand" -lt 3 ]; then echo "FAIL: $id offers $ncand candidates (at least three expected)"; fail=1; continue; fi
    hint3=$(SPIKE_CIRCUITS_DIR="$root/circuits" "$run" --task "$id" --seed 1 --quick 2>/dev/null | python3 -c '
import json, sys
d = json.load(sys.stdin)
print(" ".join(h["text"] for h in d["runs"][0]["hints"]))' 2>/dev/null)
    npass=0
    for c in $cands; do
      case "$hint3" in *"$c"*) echo "FAIL: $id: a hint names the candidate '$c'"; fail=1 ;; esac
      SPIKE_CIRCUITS_DIR="$root/circuits" "$run" --task "$id" --seed 1 --quick --variant "$c" --require any > /dev/null 2>&1 && npass=$((npass + 1))
    done
    if [ "$npass" = 1 ]; then echo "REFERENCE $id: exactly one of its $ncand candidates passes; nothing submitted fails; no hint names a candidate"; else echo "FAIL: $id: $npass of its $ncand candidates pass (exactly one must)"; fail=1; fi
    continue
  fi
  for seed in 1 2 3; do
    out=$(SPIKE_CIRCUITS_DIR="$root/circuits" "$run" --task "$id" --seed "$seed" "${extra[@]}" --require any 2>&1) && { ok=1; break; }
  done
  if [ "$ok" = 1 ]; then echo "REFERENCE $id passes (seed $seed)"; else echo "$out" | head -3; echo "FAIL: the reference circuit of $id does not pass on seeds 1-3"; fail=1; fi
done
# --- audio smoke (item H): every sound loads, every GameAudio API is called, the spike limiter is checked ---
godot --headless --path godot -s tests/test_audio.gd > "$log" 2>&1
grep -E "^AUDIO|SCRIPT ERROR|Parse Error" "$log"
grep -q "^AUDIO OK" "$log" && ! grep -qE "SCRIPT ERROR|Parse Error" "$log" || { echo "FAIL: audio smoke (godot/tests/test_audio.gd)"; fail=1; }
# --- end audio smoke ---
[ "$fail" = 0 ] && echo "smoke.sh: all good" || exit 1
