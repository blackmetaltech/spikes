#!/usr/bin/env bash
# Retake the documentation screenshots -> docs/screenshots/<screen>-<WxH>.png  (sizes: desktop 1920x1080, phone 844x390 landscape)
# Without a display (CI, containers) or with SP_SHOTS_GL=1 it renders through xvfb-run and the OpenGL renderer.
# ONLY="title lab" limits the screens; SS=2 renders at double size and downsamples with Lanczos.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd); cd "$root"
scripts/build-gd.sh >/dev/null
cd godot
godot --headless --path . --import >/dev/null 2>&1
out=$root/docs/screenshots; mkdir -p "$out"
SS=${SS:-1}
tmp=$(mktemp -d); trap 'rm -rf "$tmp" override.cfg' EXIT
export SP_NOAUTOSAVE=1 SP_NODISPLAY=1 SP_NOTHROTTLE=1 SP_BUDGET_US=${SP_BUDGET_US:-200000}
im=$(command -v magick || command -v convert || true)
run_godot() {
  if [ -z "${DISPLAY:-}" ] || [ "${SP_SHOTS_GL:-0}" = 1 ]; then
    env "$@" xvfb-run -a -s "-screen 0 $((1920*SS))x$((1080*SS))x24" godot --path . --rendering-driver opengl3 >/dev/null 2>&1 || true
  else
    env "$@" godot --path . >/dev/null 2>&1 || true
  fi
}
shot() {   # shot <name> <WxH> <env assignments...>
  local name=$1 size=$2; shift 2
  local w=${size%x*} h=${size#*x}
  printf '[display]\nwindow/size/mode=0\nwindow/size/window_width_override=%s\nwindow/size/window_height_override=%s\n' $((w*SS)) $((h*SS)) > override.cfg
  local png=$tmp/$name-$size.png
  run_godot SP_SHOT_AT="${SP_SHOT_AT:-1.5}" "$@" SP_SHOT="$png"
  [ -f "$png" ] || { echo "MISSING $name-$size"; return 1; }
  if [ "$SS" != 1 ] && [ -n "$im" ]; then "$im" "$png" -filter Lanczos -resize "${w}x${h}!" "$out/$name-$size.png"; else cp "$png" "$out/$name-$size.png"; fi
  echo "wrote docs/screenshots/$name-$size.png"
}
ONLY=${ONLY:-"title tasks lab-lamprey lab-worm lab-reflex lab-edit lab-world lab-scopes analysis settings credits"}
PHONE_ONLY=" lab-world lab-scopes "
for size in 1920x1080 844x390; do
  for s in $ONLY; do
    [[ "$PHONE_ONLY" == *" $s "* && "$size" != 844x390 ]] && continue
    case $s in
      title) shot title $size SP_SCREEN=title ;;
      settings) shot settings $size SP_SCREEN=settings SP_TAB=Display ;;
      tasks) shot tasks $size SP_SCREEN=tasks ;;
      lab-lamprey) shot lab-lamprey $size SP_SCREEN=lab SP_TASK=lamprey SP_REF=1 SP_SPEED=1 SP_SIM_UNTIL=6000 SP_SHOT_AT=3 ;;
      lab-worm) shot lab-worm $size SP_SCREEN=lab SP_TASK=worm SP_REF=1 SP_SPEED=20 SP_SIM_UNTIL=40000 SP_SHOT_AT=3 ;;
      lab-reflex) shot lab-reflex $size SP_SCREEN=lab SP_TASK=reflex SP_REF=1 SP_SPEED=1 SP_SIM_UNTIL=5200 SP_SHOT_AT=3 ;;
      lab-edit) shot lab-edit $size SP_SCREEN=lab SP_TASK=lamprey SP_REF=1 SP_SPEED=1 SP_SEL=proj:3 SP_SIM_UNTIL=6000 SP_SHOT_AT=3 ;;
      lab-world) shot lab-world $size SP_SCREEN=lab SP_TASK=worm SP_REF=1 SP_SPEED=20 SP_SIM_UNTIL=40000 SP_LABTAB=world SP_SHOT_AT=3 ;;
      lab-scopes) shot lab-scopes $size SP_SCREEN=lab SP_TASK=lamprey SP_REF=1 SP_SPEED=1 SP_SIM_UNTIL=6000 SP_LABTAB=scopes_only SP_SHOT_AT=3 ;;
      analysis) shot analysis $size SP_SCREEN=analysis SP_TASK=lamprey SP_REF=1 SP_SPEED=1 SP_SHOT_AT=7 ;;
      credits) shot credits $size SP_SCREEN=credits ;;
      *) echo "unknown screen $s" >&2; exit 2 ;;
    esac
  done
done
