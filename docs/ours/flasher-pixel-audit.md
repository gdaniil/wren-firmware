# Web flasher pixel audit (Figma aaFFyK5TYIRuoBfopyP9WH)

Files changed: `web-flasher/styles.css` only. Measured with `getBoundingClientRect` at 1512x900, 402x900,
375x812, 1024x768 against frames 31:93, 41:214, 43:261, 46:534, 50:592, 43:445.

## Fixed

| element | property | figma | before | after |
|---|---|---|---|---|
| h1 "hey, wren" | line-height / box height | normal = 107px | 1.3 = 93.6px | 107px |
| all Literata h2 (40px) | line-height / box height | 59px | 52px | 59px |
| feature block | height, step | 635px, step 699px | 628px, drift 7px per block | 635px, step 699px (last block y=2826 matches) |
| hero tagline | y position | 219 inside hero | 211 | 219 |
| Literata 20px (buttons, outline buttons) | line-height | 30px | 26px | 30px |
| IBM Plex Mono body | line-height | normal (26px at 20px) | 1.3 (same value) | normal |
| outline button (get chrome, fonts.zip) | padding / height | 62px (3px border inside 16px inset) | 16px 13px = 68px | 13px = 62px |
| outline button | gap text-icon | 8px | none | 8px |
| radio box | height | 17px | 16px | 17px |
| hero h1, tagline | box width | 400px | 536px | max 400px (same wrap) |
| installer text (p) | box width | 400px | 420px | max 400px (same wrap; block stays 420) |
| phone (<960) column | side padding | 10px | 16px | 10px |
| phone features | side padding | 10px | 16px | 10px |
| phone h1 | font-size | 72px | 56px under 420px | 72px |
| phone message | inset | 8px around text (text x=18) | 0 | 8px (text x=18) |
| 960-1099px, choose | radio row gap | n/a | wrapped to 2 rows at 1024 | gap 20px, one row |

## Verified equal (no change)

| element | property | value |
|---|---|---|
| left column | width / border | 600px at 1512, 4px right border, padding 30 |
| right column | width / padding / bg | 912px, 30px, #f1eee5 |
| paper / ink / button text | colors | #f3f0e7 base (paper.png), #151515, #f3f1ea |
| logo | size | 81x96 at 30,30 |
| hero gap | h1/logo/tagline | 8px |
| h1, h2, body fonts | family/weight/size | Literata 300 72/40/20, IBM Plex Mono 400 20 |
| letter-spacing | all | normal |
| installer | gap, button size | 24px (32px in choose), 360x62, padding 14/16/18 |
| installer bottom | anchor | bottom 870 at 900 (space-between) |
| progress | size, border, inset | 360x26, 3px border, 4px padding, 12px bar |
| done state | y offsets | 530, 580, 630, 706, 808 (Figma 530, 580, 630, 706, 808) |
| radio | box, border, gap | 16px, 2px, 8px label gap, 24px between rows |
| feature | text box, image | 440px wide, 8px gap, image 328x492 4px border, 24px gap, 64px between |
| phone | column / installer / content | column 523, installer at y=401, hero at y=50, features start 573, images x=37 |
| scroll | 1024, 375, 402 | no horizontal scroll in any of 7 states |

## Not matched, and why

- Hint lines (16px, "use a data cable...", "edge works too", status line): no Figma equivalent, wording kept, so the
  connect and unsupported blocks are 21-42px taller than Figma.
- Choose state labels ("x3 or x4", "x4 pro", "x4 classic") differ from Figma ("X3/X4", "X4pro", "X4classic"); wording kept.
  Row width is 420 by design.
- Fractions: Figma measures list integer heights; Chrome gives the same integers now (hero p 52, h1 107, h2 59).
- Installing progress fill: Figma mock bar is 53px at "22%", real fill follows the value (22% would be 76px). Not a bug.
- Phone: Figma frame has no footer; ours adds footer (y 4100). Phone installer text box is 366px (Figma 366 inside the 8px inset).
- Paper background is paper.png, not the WebGPU shader (by rule). Grain not compared.
- Not visually screenshot-compared (the browser zoom tool failed on the pane's coordinate frame); checks are geometric
  and computed-style only. Panel text rendering and wrap of copy that differs from Figma wording could not be compared.
