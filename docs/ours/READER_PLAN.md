# Reader typography: work plan

Source of truth for values: `READER_TYPOGRAPHY_SPEC.md` (13 Nov 2026). This file is the breakdown into tasks.
Written 14 Nov 2026 on `ours` @ `c416e993`.

## Architecture decisions

1. **Reader fonts live on the SD card as `.cpfont`, not in flash.** Three families x ~22 pixel sizes x 4 styles
   does not fit the app partition. The `.cpfont` pipeline already exists (`lib/EpdFont/scripts/fontconvert_sdcard.py`,
   variable-font instancing via fontTools, `SdCardFontRegistry`). Built-in Literata is the fallback when
   the SD fonts are missing.
2. **A `.cpfont` "size" means x-height in pixels, not points.** Files are `WrenLiterata_15.cpfont` etc. The registry
   parses `<Family>_<n>.cpfont` with a `uint8_t` size, so no format change. Weight is a function of
   (font, x-height px) only: the spec table agrees across X3 and X4 for equal px. One family serves both devices.
   Sizes needed (union of X3 and X4 columns): 9 10 11 12 13 14 15 16 17 18 19 21 22 23 24 26 28 30 32 34 36 41.
3. **Typography model is our own pure C++ module** (`src/wren/typography/`), host-tested, no heap. It turns
   (device ppi, mode, step, font) into pixel values. Settings store mode, per-mode step and per-mode font.
4. **Engine changes in `lib/Epub` are unavoidable** (line pitch in px, em indent, hyphenation zone, language rules,
   text preprocessing). This breaks the REPO_MAP "never touch lib/Epub" rule on purpose. Keep each hook small,
   gated by new `ReaderRenderSpec` fields whose defaults reproduce upstream behaviour, so upstream merges stay
   cheap and upstream tests keep passing. Bump `LAYOUT_SIGNATURE_VERSION` and the section-cache version.
5. **API compatibility:** new keys may appear in `/api/settings` (additive). No change to `src/network/`.

## Tasks

| # | Task | Depends on | Touches | Codex model |
|---|---|---|---|---|
| T1 | Font build tool: x-height-targeted `.cpfont` for Literata, Golos Text, Bitter with per-size wght (regular/bold), opsz, kerning under latn/cyrl/DFLT, coverage report, stem check | none | new `scripts/wren/`, read-only use of `lib/EpdFont/scripts` | Terra high |
| T2 | Typography model + settings: modes table, mm to px, steps, line pitch, margins, hyphenation and paragraph params, persisted mode/step/font; host tests | none | new `src/wren/typography/`, small edits `CrossPointSettings.{h,cpp}`, `SettingsList` | Terra medium |
| T3 | Engine hooks: line pitch px (constant per page, leftover to top margin), indent in em, 0.5-line paragraph space, hyphenation zone / min word / min left-right / max consecutive / not on last line / not across page; language from EPUB `lang`; Ukrainian apostrophe in words; soft hyphen draws U+002D; default left-aligned, justify forces hyphenation | T2 | `lib/Epub` (Section, ParsedText, ChapterHtmlSlimParser, hyphenation), `ReaderRenderSpec.h` | Sol high |
| T4 | Text preprocessing ru/uk/en: quotes, dashes, ellipsis, nbsp rules, short-word line-end penalty | T3 (same files) | new pure module + one call site in parser | Terra high |
| T5 | Reader UX: size buttons move within mode steps, last-step offer of the next mode, mode switch applies everything, per-mode font/step memory, large mode scales status line and settings menu, font try-on screen | T2, T3, T1 fonts on SD | `src/activities/reader/*`, new activities, i18n strings | Terra high |
| T6 | Refresh policy: full refresh every 10 pages in 1-bit, on chapter change, after images; status clock updates only on page turn | T5 | reader activity | Terra medium |
| T7 | Device calibration (Daniil, needs X3): weights per step from screenshots, 4-gray AA cost, hinting modes, line counts vs spec | T1-T6, hardware | `HARDWARE_LOG.md` | none |

Later (spec "features worth building"): reading speed per font, time left in chapter, dim-light preset (Bitter,
+1 step, denser weight), letter-spacing option that widens word spacing.

## Order

T1 and T2 run in parallel now (no shared files). T3 after T2 lands. T4 after T3. T6 after T4 (no longer waits
for T5). T7 when the X3 is in hand.

**T5 is deferred (Daniil, 14 Nov 2026): a separate piece of work later, not part of this run.** Until then the
mode/step/font settings exist but have no reader UI.

## Status (14 Nov 2026)

- T2 done and checked: `src/wren/typography/`, settings keys `wrenMode`, `wrenStep[3]`, `wrenFont[3]`, 917/917 host
  tests, three envs build, `pio check` clean. Not in the settings UI yet.
- T1 built 66 `.cpfont` (27.3 MiB) with `scripts/wren/build_fonts.py`; report in `build/wren-fonts/REPORT.md`.
  Golos Cyrillic kerning comes through DFLT. U+0301 placement passes on host renders. 44/156 stem checks miss the
  target with the spec weights (the converter hints differently from the research renders), so weights are being
  recalibrated against the converter, per the spec's own instruction.
- Firmware weights and em sizes now come from `WrenFontMetrics.generated.h`, emitted by the font builder from
  its measured instances and `weights.json`. GPOS mark anchors are not supported by `.cpfont`; the renderer's
  heuristic places combining marks.
- Regenerate firmware metrics after a measured font build; `--metrics-only` uses the completed build
  measurements without rebuilding `.cpfont` files.

- T3 done and checked (14 Nov 2026): Wren turns on automatically when the `Wren<Font>` family for the current
  mode is on the SD card at the needed x-height; otherwise layout is upstream. Fixed pitch, leftover to the top,
  em indent from `WrenFontMetrics.generated.h` (written by `scripts/wren/build_fonts.py`), hyphenation zone / min
  word / left-right / max consecutive / not across page, left-aligned unless the user picked alignment, justify
  forces hyphenation. Section file v76, layout signature v3. 940/940 host tests, three envs, `pio check` clean.
  Flash +5.4 KB, static RAM unchanged.
- Design consequences of the fixed pitch, to review: headings render at body size (only one size is loaded);
  CSS top margins, drop caps and widow/orphan carry are off in Wren mode; tables stack; large-mode paragraph
  space rounds 0.5 line up to 1 full line; ruby takes whole extra lines. No way to turn Wren off except removing
  the fonts until T5 adds UI.

- T4 done and checked (14 Nov 2026): `lib/Epub/Epub/WrenText.{h,cpp}` (rules) and `WrenParsedText.cpp` (one
  paragraph pass in `ParsedText` before measurement, quote state per paragraph). ru/uk/en quotes, apostrophe,
  dashes, ellipsis; glue via the existing NBSP space token; short-word line-end preference limited to a list of
  prepositions/conjunctions/particles (pronouns like "он" may end a line). Source offsets inside rewritten words
  go through the NFC absorbed-position list, so highlights and sync positions stay on source text. Wren layout
  always uses the greedy hyphenated path (zone > 0), so there the short-word rule is a move-to-next-line
  preference; the DP penalty applies only without a zone. Section file v77, layout signature v4. Terra stopped
  twice; Sol high finished. 959/959 host tests, three envs, `pio check` clean, Flash +7 KB vs T3.

- T6 done and checked (14 Nov 2026), by hand (about 10 lines, no Codex): full refresh on chapter change
  (`lastRenderedSpineIndex` in `EpubReaderActivity`), default refresh frequency 10 pages (was 15; saved settings
  keep their value). Already upstream: full refresh after image pages, and the status clock is drawn only with
  a page (no clock timer in the reader). 959/959 host tests, three envs, `pio check` clean.

## Unverified

- Everything about X4 / X4 Classic / X4 Pro (217 ppi assumed for all 4.3" 800x480 panels).
- Whether the converter's 150 DPI rasterisation and FreeType hinting reproduce the spec's stem widths: T1 measures it.
- SD read cost of switching sizes on C3 (font cache, heap): measure in T5.
