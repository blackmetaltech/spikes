# Writing wiki pages

The wiki at https://ixaxaar.github.io/spikes/wiki/ is built from the Markdown files in this folder by `tools/site/build.py` (Python standard library only). The Pages workflow (`.github/workflows/pages.yml`) runs `--check` and deploys `site/` on every push to master that touches `site/`, `tools/site/` or the workflow; locally:

```sh
python3 tools/site/build.py --check     # renders site/wiki/*.html, search.json and site/sitemap.xml; fails on broken links
python3 -m http.server -d site 8000     # then open http://localhost:8000/
```

Files in capitals (this README, `IMAGES.md`) and files starting with `_` are notes, not pages. Anything else here that is not Markdown (for example `img/*.webp`) is copied to `site/wiki/` as it is. `site/wiki/`, `search.json` and `site/sitemap.xml` are generated and not tracked by git.

## Front matter

```
---
title: The Bestiary          # the page title (the h1); a leading "# Title" line in the body is dropped
order: 31                    # sidebar position; sections are ordered by their smallest page order
section: The campaign        # sidebar group, numbered I., II., ... in the order above
summary: One sentence.       # under the title, on the index, in search results and the meta description
---
```

`index.md` and `see-it-in-action.md` belong to the site; the comment `<!-- pages -->` in `index.md` expands to the list of every page by section.

## What the Markdown supports

- Headings `##` to `####`, each with an anchor (`## Title {#custom-id}` sets the id). Pages with three or more get an "On this page" column on wide screens.
- Paragraphs, **bold**, *italic*, `code`, ~~strike~~, links, `<kbd>`, `<sub>`, `<sup>`. Underscores inside words are never emphasis, so V_m, tau_syn_ex and E_L stay as written: write maths plainly (`tau_m dV/dt = -(V - E_L) + R I`) or in a code span.
- Links to other pages as `glossary.md` or `glossary.md#adaptation`; links to repository files as relative paths (`../../docs/WHAT-IS-REAL.md`) become GitHub links. `--check` fails on a missing page, anchor or image.
- Lists (nested by indenting two or more spaces), block quotes, rules (`---`), tables with `:---:` alignment, fenced code blocks with a language.
- An image alone in a paragraph becomes a figure; the caption is the title if given, otherwise the alt text:
  `![What it shows](img/bodies-limb.png "The caption, numbered Fig. N automatically.")`.
  A missing `.png` falls back to a `.webp` of the same name (the captures are WebP to keep the site small).
- A clip: the same syntax with a `.webm` or `.mp4` source, e.g. `![The leech heartbeat](../assets/media/leech.webm "Caption")`. It gets its MP4 fallback, poster and GIF from the files next to it, and plays only while on screen (never with reduced motion).
- Callouts, in any of these forms (`note`, `tip`, `warning`; `important`, `caution` and `danger` are drawn as warnings):

  ```
  > [!NOTE]
  > Text.

  !!! warning "Optional title"
      Indented text.

  :::tip Optional title
  Text.
  :::
  ```

- Raw HTML blocks (a line starting with `<div`, `<figure>`, `<table>`, `<details>` and so on) pass through. `<div class="gallery">` ... `</div>` with figures inside lays them out in a grid.

## Images

`IMAGES.md` lists the captures the pages use; `scripts/site-media.sh` makes them (the `wiki` items) from the exported game into `img/` as WebP (`ONLY="wiki:first-hour-lab" scripts/site-media.sh` remakes one; it needs the exported game, `xvfb-run`, ffmpeg and ImageMagick). Site-wide media (clips, screenshots) are in `../assets/media/` and the chapter plates in `../assets/plates/`; both may be used from a page with a relative path.
