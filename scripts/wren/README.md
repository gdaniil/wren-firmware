# Wren SD-card fonts

From the repository root:

```sh
source .venv/bin/activate
pip install -r scripts/wren/requirements.txt
python scripts/wren/build_fonts.py --weights measured
```

Outputs are ignored under `build/wren-fonts/{Literata,Golos,Bitter}/`.
Do not copy those folders by hand: the reader expects
`/fonts/WrenLiterata/`, `/fonts/WrenGolos/` and `/fonts/WrenBitter/`.
Install them with one of:

```sh
python scripts/wren/install_fonts.py --sd /Volumes/<card>        # mounted SD card, fast
python scripts/wren/install_fonts.py --device crosspoint.local   # over Wi-Fi, Send tab on
```

Without the selected SD font size, the reader uses built-in Literata at the
nearest 12, 14 or 17 pixel x-height. Each folder includes
the source OFL license. `WrenLiterata_15.cpfont` has a 15-pixel monochrome
x-height; the number is not a point size.

The default `--weights measured` reads the modes and minimum weights from the
specification. For each x-height it takes the largest regular stem target among
all modes using that size. Bold targets the larger of rounded 1.4 times the
regular target and the achieved regular stem plus one pixel, and its weight
must strictly exceed the regular weight. It scans
weights in 5-unit steps from the font's specified minimum, recalibrating exact
x-height and optical size for every candidate. It chooses the lightest exact
match. If no exact match exists, it chooses the nearest achieved stem above
the target, then the lightest weight attaining it, and flags that choice.
A target with no achievable stem at or above it stops the build with an error.

`--weights table` retains the original specified weights, taking the larger
regular and bold weights separately when device/mode rows conflict. Both modes
report the measured alternatives. The specification table itself is unchanged.
The selected build weights and achieved stems are exported to
`scripts/wren/weights.json`, keyed by family and pixel x-height.

Sources are pinned to a Google Fonts revision in `download_fonts.py`, cached
under `.cache/wren-fonts/`, and identified by SHA256 in the report. Cached
static instances include their source hash and axes in a JSON sidecar.
Delete that cache to download and instantiate again.

The wrapper imports the existing SD converter. During each rasterisation it
temporarily adapts FreeType's sizing call to pixel em units at 72 DPI and sets
the variable axes. It restores both hooks even on failure. Static fontTools
instances provide kerning with the same axes; bitmaps use the variable font
so its native variation hinting is preserved. No converter source is edited.

The em search uses a 1/64-pixel grid and selects the midpoint of the interval
whose rendered `x` has the requested height. Optical size follows
`clamp(em / 259 * 72)`. The BW decoder treats every nonzero converter 2-bit
sample as ink, matching `lib/GfxRenderer/GlyphBitmap.h`.

Verification is part of every build: the builder parses every emitted style
and checks its x-height, checks Golos kerning against DFLT-only extraction,
and writes coverage, mode-specific stem measurements, and accent previews to
`build/wren-fonts/REPORT.md`. `measurements.json` contains all chosen axes and
all stem checks, including checks that pass. Stem width is the median left
stroke width in rows with two separated strokes, excluding the top/bottom 20%
of ink rows. This rejects crossbar rows that the original fixed bands counted
as stems. Left/right strokes can snap differently, so the left remains the
measurement reference. The original 44-row mismatch table is preserved in
`table-mismatches-reference.md` and appended to the generated report.
Accent previews reproduce the reader's
bitmap-centering and vertical-gap heuristic without GPOS mark positioning.

Run the host regression checks with:

```sh
source .venv/bin/activate
python -m unittest discover -s scripts/wren
```

These are host checks. Inspect the PNGs and calibrate on the device before
using the weights as final hardware typography values.

Build the built-in Literata faces with `python scripts/wren/build_builtin_literata.py`.
This uses the pinned Literata sources, `weights.json`, and the same x-height
calibration as the SD build. It writes headers and content-derived font IDs.
