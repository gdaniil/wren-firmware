# Web flasher pixel verify (Figma aaFFyK5TYIRuoBfopyP9WH)

**Verdict: 0 geometric defects found, but NOT confirmed pixel perfect. The side-by-side visual comparison could not be done.**
The Figma MCP hit the Starter plan tool-call limit after 5 `get_metadata` calls. `get_screenshot`, `get_design_context`
and metadata for frames 31:93 (unsupported) and 46:534 (installing) were refused. So: no Figma screenshots, no Figma
font/size/weight/line-height/letter-spacing/stroke values at all (metadata gives only x/y/w/h). Those properties are
checked only against the previous report's claims, which are not independent evidence.

## Defects

| state | element | property | figma | ours | severity |
|---|---|---|---|---|---|
| none found | | | | | |

## Unverified (needs a Figma screenshot / design context once the limit resets)

| state | element | what could not be checked |
|---|---|---|
| all | hero, h1, body, buttons, h2 | font family/size/weight/line-height/letter-spacing vs Figma (ours: Literata 300 72/40/20, IBM Plex Mono 400 20, line-heights 107/59/30/normal, letter-spacing normal) |
| all | left column right border, feature image border, radio border, button border | stroke widths (ours 4, 4, 2, 3px) |
| all | colors | ink #151515, paper #f3f1ea, bg #f1eee5 vs Figma fills |
| unsupported 31:93 | whole frame | no Figma data fetched; ours: text y687 h52, button 360x62 at y763 (3px border, 8px gap), chevron 6x12 at x368 |
| installing 46:534 | whole frame | no Figma data fetched; ours: text y718 h52, progress 360x26 at y794, percent y844 |
| all | glyph rendering, baseline, wrapping, side-by-side visuals | no screenshot comparison possible; I only viewed our own pane screenshots (connect, installing) |

## Checked and equal (our `getBoundingClientRect` vs Figma metadata geometry, tolerance 1px)

- connect 41:214 (1512x900): left column 600 wide; logo 81x96 at (30,30); h1 y134 h107 (Figma 104+30, 107); tagline y249 h52 (219+30, 52);
  connect button 360x62 at y808 (Figma 732+76, bottom 870 both). Our text sits at y666 because of the extra hint line (intentional).
- features column: x630 start, 852 content, text blocks 440 wide at x836 (206+630), h2 y30 h59, p y97 h52 (30+67), image 328x492 at x892 y173 (262, 143), block height 635, step 699.
- choose 43:261: radio rows at x30/178/302 (Figma 0/108/216 plus differing label widths from different wording), row h26, box 16x17 at y+4.5, box-text gap 8, row gap 24, radios to button 32, button 360x62 y808 (Figma 666+142), bottom 870.
- done 50:592: y530 / 580 / 630 / 706 / 808 equal to Figma 530/50/100/176/278 offsets; progress 360x26, border 3 + pad 4 gives 346x12 fill at inset 7 (Figma 7,7,346x12); fonts button 360x62, label x46 y824 (16,16 inset), icon 30x30 at x344 (314+30), y824.
- phone 43:445 at 402 wide: column 523 high; logo (10,50); h1 y154; tagline y269; installer (10,401) h68, message inset 8; content starts y573 (523+50); blocks 1 and 2 at 573 and 1272 (Figma 50, 749), image x37 (27+10), h2 382 wide.
- Horizontal scroll: none at 402, 375, 1024x768, 1512 in all 7 states (scrollWidth == clientWidth). At 1024 the choose row stays one line.
- Fonts: `document.fonts` shows Literata 300 and IBM Plex Mono 400 both loaded.
- Console: only 404s (firmware.json, favicon.ico, wren-fonts.zip, all absent locally) and Chrome beforeunload notices from state-switching. No script errors.

## Notes (not defects)

- Phone blocks 3 and 4: Figma frames are 661 high (3-line paragraph, image y169); ours stay 635 because our copy wraps to 2 lines. Wording-driven, so subsequent blocks differ by 26px per block against Figma (block 3 at 1971 equal, later ones shift). Copy is intentionally different.
- Installing: Figma mock bar fill is a static sample; ours follows the value.
- Phone footer (y4100) has no Figma counterpart (intentional).
- The h1 box is 322 wide (shrink to content inside a 400 max), Figma 400. Same wrap and position, no visual effect.

## What a re-run needs

Reset the Figma MCP quota, then: screenshot 41:214, 43:261, 46:534, 50:592, 31:93, 43:445 at 1512 wide and compare against our pane;
fetch `get_design_context` for text styles and strokes (hero h1, tagline, button label, radio, progress, divider, feature image border).
