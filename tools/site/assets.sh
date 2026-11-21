#!/usr/bin/env bash
# Static assets of the website that are derived from the game's own files (no game run needed):
#   site/assets/plates/<chapter>.webp, -640.webp   the Bestiary and Theory plates (godot/assets/plates, tools/art/plates.py)
#   site/assets/fonts/                              the game's fonts (godot/assets/fonts, all SIL OFL 1.1; see tools/site/fonts.py)
#   site/assets/paper.webp                          a seamless tile of rag-paper fibre and grain (after godot/assets/shaders/paper.gdshader)
#   site/assets/favicon.svg, icon-180.png   the game icon (godot/assets/brand/icon.svg)
#   site/assets/og.jpg                              the 1200x630 social card, cut from the title screen still (run scripts/site-media.sh first)
# Needs ImageMagick 7 and python3 with fontTools + brotli, numpy and Pillow. Deterministic: rerun after changing a plate, the icon or the fonts.
set -euo pipefail
root=$(cd "$(dirname "$0")/../.." && pwd); cd "$root"
A=site/assets
mkdir -p "$A/plates" "$A/fonts"

# plates: transparent paper, so the page's own paper shows through
for f in godot/assets/plates/*.png; do
  id=$(basename "$f" .png)
  magick "$f" -filter Lanczos -resize 1200x -quality 80 -define webp:alpha-quality=90 -define webp:method=6 "$A/plates/$id.webp"
  magick "$f" -filter Lanczos -resize 640x -quality 78 -define webp:alpha-quality=85 -define webp:method=6 "$A/plates/$id-640.webp"
done

python3 tools/site/fonts.py

python3 tools/site/paper.py

cp godot/assets/brand/icon.svg "$A/favicon.svg"
magick -background none -density 384 godot/assets/brand/icon.svg -resize 180x180 "$A/icon-180.png"

if [ -f "$A/media/title.webp" ]; then
  magick "$A/media/title.webp" -filter Lanczos -resize 1200x -crop 1200x630+0+30 +repage -quality 84 -sampling-factor 4:2:0 -strip "$A/og.jpg"
else
  echo "no site/assets/media/title.webp yet: run scripts/site-media.sh, then this again for og.jpg" >&2
fi
du -sh "$A/plates" "$A/fonts" "$A/paper.webp" "$A/og.jpg" 2>/dev/null || true
