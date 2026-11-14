# Wren reader typography: implementation spec (updated 13 Nov 2026)

Full research, renders and sources: https://claude.ai/artifact/Rtt7ibuQCFzorNwj3vRs1y
Numbers come from desktop renders (FreeType + HarfBuzz) at true X3/X4 pixels, not from the device. Calibrate on hardware.

## Fonts (all OFL)
1. Literata: default serif. Axes wght 200–900, opsz 7–72 (set opsz to physical pt).
2. Golos Text: sans. Axis wght 400–900. Default font of Large mode.
3. Bitter: slab, "dim light" preset (X3 has no frontlight). Axis wght 100–900; needs wght above 400 on e-ink (see table).
Coverage checked with fontTools: Cyrillic incl. ё і ї є ґ ў, U+0301 stress mark, « » „ “ — … №, ’; English: full Basic Latin + Latin-1 + “ ” ‘ ’ — – … € £ © ™.
Gaps: Golos has no ′ ″ primes and lacks 29 rare Latin Extended-A letters (Esperanto etc.); Bitter has no U+00AD glyph.
Rejected: Noto Serif (CrossPoint default, weak Cyrillic per type.today), Inter, Roboto, Fira, Source Sans, Open Sans, Merriweather, Atkinson Hyperlegible Next (zero Cyrillic glyphs), OpenDyslexic (no measured benefit).

## Modes (preset bundles; size buttons move within the mode's 5 steps)
```json
{
  "ppi": { "X3": 259, "X4": 217 },
  "px": "round(mm * ppi / 25.4)",
  "boldStemFactor": 1.4,
  "minWght": { "Literata": 350, "Golos Text": 400, "Bitter": 350 },
  "modes": {
    "small":    { "xHeightMm": [1.1, 1.2, 1.3, 1.4, 1.5],  "defaultStep": 2, "defaultFont": "Literata",   "lineStepXh": 2.5, "sideMarginMm": 1.2, "paragraph": { "indentEm": 1, "spaceLines": 0 },   "stemRatio": 0.21, "statusXHeightMm": 1.0, "hyphenation": { "zone": 0.75, "minWordLetters": 8, "minLeft": 2, "minRight": 2, "maxConsecutive": 2 } },
    "standard": { "xHeightMm": [1.4, 1.5, 1.7, 1.9, 2.15], "defaultStep": 1, "defaultFont": "Literata",   "lineStepXh": 2.7, "sideMarginMm": 1.6, "paragraph": { "indentEm": 1, "spaceLines": 0 },   "stemRatio": 0.20, "statusXHeightMm": 1.0, "hyphenation": { "zone": 0.75, "minWordLetters": 8, "minLeft": 2, "minRight": 2, "maxConsecutive": 2 } },
    "large":    { "xHeightMm": [2.4, 2.7, 3.1, 3.5, 4.0],  "defaultStep": 1, "defaultFont": "Golos Text", "lineStepXh": 2.9, "sideMarginMm": 1.2, "paragraph": { "indentEm": 0, "spaceLines": 0.5 }, "stemRatio": 0.23, "statusXHeightMm": 2.0, "hyphenation": { "zone": 0.60, "minWordLetters": 8, "minLeft": 2, "minRight": 2, "maxConsecutive": 2 } }
  },
  "hyphenationOverrides": { "en": { "minLeft": 2, "minRight": 3 } }
}
```
- X3 portrait at default step: small 22 lines × 34 chars, standard 18 × 29, large 9 × 18 (ru). X4: 26 × 37, 21 × 31, 11 × 20. English fits 10–15% more chars per line.
- Behaviour: at the last step of a mode, the next press offers the neighbouring mode in one line ("Включить крупный режим?"). Switching mode applies all mode parameters; each mode remembers its own font and last step. Font, alignment and hyphenation are user-overridable in every mode; line spacing and margins only via mode. Large mode also scales the status line and the text-settings menu.
- Line pitch = round(lineStepXh × x-height px), constant on a page; leftover height goes to the top margin. Top margin ~24 px, bottom ~34 px for the status line (scale in large mode).

## Weights per step (regular/bold wght) — starting values
Regular = lightest wght whose vertical stem of "н" in 1-bit reaches round(stemRatio × x-height) px (min 2). Bold stem = round(1.4 × regular stem). Recompute the same way in the CrossPoint font converter, then confirm on device.

| Mode, step | X3 xh · stem | X3 Lit | X3 Golos | X3 Bitter | X4 xh · stem | X4 Lit | X4 Golos | X4 Bitter |
|---|---|---|---|---|---|---|---|---|
| small 1.1 | 11 · 2/3 | 350/475 | 400/550 | 375/575 | 9 · 2/3 | 350/600 | 400/675 | 425/725 |
| small 1.2 | 12 · 3/4 | 425/625 | 500/700 | 525/725 | 10 · 2/3 | 350/525 | 400/600 | 400/650 |
| small 1.3 | 13 · 3/4 | 400/575 | 450/650 | 500/675 | 11 · 2/3 | 350/475 | 400/550 | 375/575 |
| small 1.4 | 14 · 3/4 | 375/525 | 450/625 | 450/625 | 12 · 3/4 | 425/625 | 500/700 | 525/725 |
| small 1.5 | 15 · 3/4 | 350/500 | 400/575 | 425/600 | 13 · 3/4 | 400/575 | 450/650 | 500/675 |
| standard 1.4 | 14 · 3/4 | 375/525 | 450/625 | 450/625 | 12 · 2/3 | 350/450 | 400/500 | 350/525 |
| standard 1.5 | 15 · 3/4 | 350/500 | 400/575 | 425/600 | 13 · 3/4 | 400/575 | 450/650 | 500/675 |
| standard 1.7 | 17 · 3/4 | 350/450 | 400/500 | 400/525 | 15 · 3/4 | 350/500 | 400/575 | 425/600 |
| standard 1.9 | 19 · 4/6 | 400/650 | 450/700 | 475/725 | 16 · 3/4 | 350/450 | 400/525 | 400/550 |
| standard 2.15 | 22 · 4/6 | 350/550 | 400/600 | 400/650 | 18 · 4/6 | 400/650 | 475/725 | 500/775 |
| large 2.4 | 24 · 6/8 | 500/675 | 575/750 | 600/775 | 21 · 5/7 | 450/675 | 525/725 | 550/775 |
| large 2.7 | 28 · 6/8 | 425/600 | 475/650 | 500/675 | 23 · 5/7 | 425/625 | 475/675 | 500/725 |
| large 3.1 | 32 · 7/10 | 425/650 | 500/725 | 525/750 | 26 · 6/8 | 450/625 | 525/700 | 550/725 |
| large 3.5 | 36 · 8/11 | 450/650 | 500/700 | 525/725 | 30 · 7/10 | 450/700 | 525/750 | 550/775 |
| large 4.0 | 41 · 9/13 | 425/675 | 500/725 | 525/750 | 34 · 8/11 | 475/675 | 550/725 | 575/775 |

## Alignment and hyphenation
- Default left-aligned for all languages (CrossPoint default is Justified: change it). Justify stays as an option and forces hyphenation; suggest landscape.
- Hyphenation zone: hyphenate only if the line would otherwise end before `zone` × width; word ≥ minWordLetters; no hyphen on a paragraph's last line or across a page break.
- Dictionaries: hyph-ru, hyph-uk, hyph-en-us. Language from EPUB `lang`, not UI language. Ukrainian apostrophe (’ and ') is part of the word.
- Soft hyphen U+00AD: invisible break opportunity; draw U+002D at the break, never the U+00AD glyph.

## Paragraphs
- small/standard: first-line indent 1 em, no space. ru/uk: indent also after headings. en: no indent after headings or scene breaks.
- large: no indent, 0.5-line space between paragraphs.

## Rendering
- Stems snap to whole pixels in 1-bit (Literata at 15 px x-height: wght 400 and 450 give 3 px, 500 gives 4 px). Hence the per-step weight table.
- 4-gray anti-aliasing default only if page-turn cost is small on the device (measure). Compare hinting modes on device screenshots.
- Converter must read GPOS kern/mark for scripts latn, cyrl AND DFLT: Golos Text keeps its Cyrillic kerning only under DFLT.
- Full refresh: start at every 10 pages in 1-bit, plus on chapter change and after images.

## Text preprocessing by language
- ru/uk: hard nbsp before mid-sentence em dash, after dialogue dash, № 5, 5 кг, initials + surname, т. е., и т. д. Soft: 1–2-letter prepositions/conjunctions at line end are a line-break penalty, not a hard nbsp. Straight quotes → «» outer, „“ inner; " - " → " — "; "..." → "…".
- en: straight quotes and apostrophes → “ ” ‘ ’; "--" → "—"; leave unspaced em dashes alone; nbsp only for numbers + units and Mr./Dr. + surname. Ligatures fi/fl nice-to-have (no collisions without them in Literata 1-bit).

## Features worth building
Per-font reading speed measured passively from page-turn timing; personal "time left in chapter"; font try-on screen (same paragraph, three fonts, same x-height); "Dim light" preset (Bitter, +1 step, denser weight); letter-spacing option that also widens word spacing; size stored in mm so X3 and X4 match physically; status clock updates only on page turn.
