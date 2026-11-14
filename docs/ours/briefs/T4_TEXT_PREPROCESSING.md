# T4 brief: text preprocessing for ru / uk / en (Wren mode only)

Repo: `/Users/daniilhurevych/Wren/firmware` (C++17, PlatformIO, ESP32-C3, ~380 KB RAM, no PSRAM).
Context: Wren is our reader typography layer. When `ReaderRenderSpec::linePitchPx > 0` the book is laid out
in "Wren mode" (T3, already done). Every new behaviour in this task is gated on Wren mode; with
`linePitchPx == 0` output must be byte-identical to today (upstream tests must keep passing unchanged).

## Hard constraints

- No flashing, no `erase_flash`/`write_flash`, no commits, no push.
- Do not edit `src/network/`, `freeink-sdk/`, `lib/hal/`, or generated files (`lib/EpdFont/builtinFonts/*`,
  `*.generated.h`, `lib/I18n/I18nKeys.h`, hyphenation tries).
- No new heap per word on the hot path beyond what `ParsedText` already does. Justify any allocation in a
  short comment. No `std::regex`, no `std::map`.
- Keep edits to upstream files (`ParsedText.*`, `ChapterHtmlSlimParser.*`, `Section.cpp`) small hooks; put the
  logic in new files.
- Do not run long builds in a loop; one build and one test run at the end is enough. The orchestrator re-runs all
  checks.

## Spec (source: `docs/ours/READER_TYPOGRAPHY_SPEC.md`, section "Text preprocessing by language")

ru / uk:
- Straight quotes `"` -> `«` `»` outer, `„` `“` inner (nesting depth tracked per paragraph).
- Apostrophe `'` inside a word -> `’` (U+2019).
- Spaced hyphen or en dash as its own token (` - `, ` – `) -> em dash `—`.
- `...` -> `…`.
- Hard no-break (glue, no line break allowed) in these cases:
  - before a mid-sentence em dash (glue `—` to the previous word);
  - after a dialogue dash (paragraph starts with `—`: glue it to the next word);
  - `№` + number (`№ 5`);
  - number + unit (`5 кг`; a short fixed list: кг г мг т км м см мм л мл с мин ч % °C руб грн коп тыс млн млрд стр гл);
  - initials + surname (`А. С. Пушкин`: a single capital letter + `.` glues to the next token);
  - `т. е.`, `т. д.`, `т. п.`, `и т. д.`, `и т. п.` (glue inside the abbreviation).
- Soft rule: a 1-2 letter word (prepositions/conjunctions such as в к с о у и а я но на по за из от до не ни
  же ли бы; uk: в у з і й та на по за із від до не ні) at the end of a line is a line-break **penalty**, not a
  hard glue. Add the penalty inside the existing DP in `ParsedText::computeLineBreaks`
  (`lib/Epub/Epub/ParsedText.cpp:1111`, cost at ~1185-1200) and the hyphenated path if it has its own cost.
  Penalty size: equal to the cost of a line with 2 em of leftover space, i.e. `(2 * emPx)^2`, where emPx comes
  from `ReaderRenderSpec::fontEmHundredths` (see how T3 uses it for the em indent). Never on the last line of a
  paragraph. It must never make a paragraph fail to lay out (soft only).

en:
- `"` -> `“` `”`, `'` -> `‘` `’`; an apostrophe inside or at the end of a word (`don't`, `dogs'`, `'90s`) is `’`.
- `--` -> `—` (inside a word or as its own token). Leave existing unspaced em dashes alone.
- `...` -> `…`.
- Hard no-break only for number + unit (kg g mg km m cm mm l ml s min h % °C °F lb oz ft in mi) and
  `Mr. Mrs. Ms. Dr. St.` + next word.

Other languages (and unknown/empty language): no preprocessing.

## Where things are

- Language: `ChapterHtmlSlimParser::setWrenTypography(spec, language)` at
  `lib/Epub/Epub/parsers/ChapterHtmlSlimParser.cpp:150` (currently only derives `wrenEnglish` via
  `wren::isEnglishLanguage`, `lib/Epub/Epub/WrenLayout.h`). Add a small language enum there
  (`None, Russian, Ukrainian, English`; ru/uk detection same style as `isEnglishLanguage`, accepting `ru`, `ru-RU`,
  `rus`, `uk`, `uk-UA`, `ukr`).
- Paragraph word storage: `ParsedText` (`lib/Epub/Epub/ParsedText.h:21-70`): `words` (WordStore handles),
  `wordContinues`, `wordNoSpaceBefore`, `wordVisibleOffsetDeltas`, `wordStyles`, ... The parser passes the Wren
  spec with `currentTextBlock->setWrenTypography(wrenSpec)` at `ChapterHtmlSlimParser.cpp:589`; pass the
  language there too.
- Existing hard no-break mechanism: U+00A0 handling in `characterData` (`ChapterHtmlSlimParser.cpp:2495-2530`)
  splits the nbsp into a `" "` token and links neighbours with `continues` flags; `ParsedText.cpp:~1498`
  explains how it survives hyphenation. Reuse that representation for glue (or `wordContinues` alone if it
  renders correctly; verify spacing stays a normal space width in left and justified alignment).
- Existing dash line-breaking behaviour: `test/em_dash_line_breaking/` — read it and keep it passing.
- Visible-text offsets (highlights, clippings, KOReader positions): `characterData` counts source codepoints
  into `visibleTextOffset`; words keep `partWordVisibleOffset`. Replacements change codepoint counts
  (`...` -> `…`, `--` -> `—`). Each word's recorded visible offset must stay the **source** offset so saved
  highlights and sync positions still resolve. Check `test/test_clipping_selection_text.py` and the clipping
  tests; add a test if the risk is real.

## Design

1. New pure module `lib/Epub/Epub/WrenText.{h,cpp}` (namespace `wren`), no renderer dependency, host-testable:
   - per-word rewrite helpers (quotes with a small per-paragraph state struct, apostrophe, `...`, `--`,
     standalone dash), operating on UTF-8 in place or into a caller buffer;
   - predicates: `isGlueAfter(word, lang)` / `isGlueBefore(word, lang)` (unit list, `№`, initials,
     abbreviations, Mr./Dr.), `isShortLineEndWord(word, lang)`.
2. One hook to apply it: either in `ChapterHtmlSlimParser::flushPartWordBuffer`
   (`ChapterHtmlSlimParser.cpp:723`, just before `addWord` at :785) or as a single pass in `ParsedText` before
   line breaking. Pick the one that keeps quote direction correct across inline tags (`<i>"word</i>"`) and
   the code smallest; say which and why in a comment. Glue decisions that need the next word may be done in
   `ParsedText` where the whole paragraph is available.
3. Quote direction: opening when the quote starts a token or follows an opening bracket/dash; closing when it
   ends a token or precedes punctuation. Unbalanced quotes at paragraph end must not leak into the next
   paragraph (reset state per `ParsedText`).
4. Bump `SECTION_FILE_VERSION` in `lib/Epub/Epub/Section.cpp:86` to 77 (cached sections hold processed text).

## Tests (host, `pio run -t unit-tests`)

New `test/wren_text/` (add to `test/CMakeLists.txt` like `test/reader_typography/`), at least:
- ru: `"Он сказал: "привет""` -> `«Он сказал: „привет“»`; `слово - слово` -> `слово — слово` with `—` glued to
  the previous word; dialogue `- Да, - ответил он.`; `№ 5`, `5 кг`, `А. С. Пушкин`, `т. е.`, `и т. д.` glued;
  `...` -> `…`.
- uk: `м'ята` -> `м’ята`; quotes as ru.
- en: `"It's the dogs' '90s," she said--quietly.` -> `“It’s the dogs’ ’90s,” she said—quietly.`; `5 kg`,
  `Dr. Watson` glued; unspaced `a—b` untouched.
- Wren off (`linePitchPx == 0`) or language `de`: text unchanged.
- Line breaking: a ru paragraph where the greedy/DP break would leave `в` at line end moves it to the next line
  when that costs less than the penalty; and the penalty never applies to the last line.
- Add a layout case to `test/chapter_html_slim_parser/WrenTypographyLayoutTest.cpp` showing a glued pair never
  splits across lines.

## Done means

- `pio run -e default` builds.
- `pio run -t unit-tests` passes (all existing tests unchanged plus the new ones).
- Short summary: files touched, where the hook is and why, any spec rule you could not implement and why,
  and anything that touches highlight/sync offsets.
