#include <Logging.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>
#include <limits>

#include "ParsedText.h"

bool ParsedText::insertWrenAbsorbedOffset(uint32_t offset) {
  absorbedSourceCursor = nullptr;
  auto* owner = &absorbedSourceHead;
  while (*owner && (*owner)->base + (*owner)->deltas[(*owner)->count - 1] < offset) owner = &(*owner)->next;
  if (!*owner) return recordAbsorbedSourceOffset(offset);
  auto* chunk = owner->get();
  if (chunk->begin != 0) {
    std::move(chunk->deltas + chunk->begin, chunk->deltas + chunk->count, chunk->deltas);
    chunk->count -= chunk->begin;
    chunk->begin = 0;
  }
  if (offset < chunk->base && chunk->count < 64 &&
      chunk->base + chunk->deltas[chunk->count - 1] - offset <= UINT16_MAX) {
    const uint32_t shift = chunk->base - offset;
    for (size_t i = 0; i < chunk->count; ++i) chunk->deltas[i] += shift;
    chunk->base = offset;
  }
  if (offset < chunk->base || chunk->count == 64) {
    // Reuse the NFC chunk budget (~140 bytes/64 positions on C3). Splitting a
    // full chunk preserves sorted source coordinates without a paragraph array.
    auto fresh = makeUniqueNoThrow<AbsorbedSourceChunk>();
    if (!fresh) {
      LOG_ERR("PTX", "OOM: Wren source offsets (%u bytes)", static_cast<unsigned>(sizeof(AbsorbedSourceChunk)));
      droppedWords = true;
      return false;
    }
    if (chunk->count == 64) {
      fresh->base = chunk->base;
      fresh->count = 32;
      std::copy(chunk->deltas + 32, chunk->deltas + 64, fresh->deltas);
      chunk->count = 32;
      fresh->next = std::move(chunk->next);
      if (absorbedSourceTail == chunk) absorbedSourceTail = fresh.get();
      chunk->next = std::move(fresh);
      return insertWrenAbsorbedOffset(offset);
    }
    fresh->base = offset;
    fresh->deltas[0] = 0;
    fresh->count = 1;
    fresh->next = std::move(*owner);
    *owner = std::move(fresh);
    return true;
  }
  const uint16_t delta = static_cast<uint16_t>(offset - chunk->base);
  auto* position = std::lower_bound(chunk->deltas + chunk->begin, chunk->deltas + chunk->count, delta);
  if (position != chunk->deltas + chunk->count && *position == delta) return true;
  std::move_backward(position, chunk->deltas + chunk->count, chunk->deltas + chunk->count + 1);
  *position = delta;
  ++chunk->count;
  return true;
}

bool ParsedText::preprocessWrenText() {
  if (wrenSpec.linePitchPx == 0 || wrenLanguage == wren::TextLanguage::None) return true;
  if (wrenProcessedWords >= words.size()) return true;
  size_t longest = 0;
  for (size_t i = wrenProcessedWords; i < words.size(); ++i) longest = std::max(longest, wordAt(i).size());
  // One checked scratch allocation per pass, reused for every word. At most
  // 8*longest+8 bytes: UTF-8 expansion plus original source codepoint boundaries.
  auto scratch = makeUniqueNoThrow<uint32_t[]>(2 * longest + 2);
  if (!scratch) {
    LOG_ERR("PTX", "OOM: Wren scratch (%u bytes)", static_cast<unsigned>((2 * longest + 2) * sizeof(uint32_t)));
    droppedWords = true;
    return false;
  }
  auto* positions = scratch.get();
  char* output = reinterpret_cast<char*>(positions + longest + 1);
  // Paragraph-owned quote state survives soft flushes and inline style tokens.
  // This pass runs before measurement so rewritten glyphs and source anchors agree.
  for (size_t i = wrenProcessedWords; i < words.size(); ++i) {
    const auto input = wordAt(i);
    size_t count = 0;
    for (unsigned char c : input)
      if ((c & 0xc0) != 0x80) ++count;
    const uint32_t start = visibleOffsetAt(i);
    for (size_t c = 0; c <= count; ++c) positions[c] = sourceOffsetAfter(start, c);
    struct Context {
      ParsedText* text;
      uint32_t* positions;
      bool ok;
    } context{this, positions, true};
    const bool attached = i > 0 && (wordContinues[i] || wordNoSpaceBefore[i]);
    const bool nextAttached = i + 1 < words.size() && (wordContinues[i + 1] || wordNoSpaceBefore[i + 1]);
    uint8_t focus = wordFocusBoundary[i];
    const size_t length = wren::rewriteText(
        input, output, wrenLanguage, wrenQuotes, attached ? wren::lastCharacter(wordAt(i - 1)) : 0,
        nextAttached ? wren::firstCharacter(wordAt(i + 1)) : 0,
        [](uint32_t index, void* opaque) {
          auto& ctx = *static_cast<Context*>(opaque);
          ctx.ok = ctx.text->insertWrenAbsorbedOffset(ctx.positions[index]) && ctx.ok;
        },
        &context, &focus);
    if (!context.ok) return false;
    if (input != std::string_view(output, length)) {
      WordStore::StoredWord stored;
      if (!storeWord({output, length}, stored)) return false;
      wordStore.release(words[i]);
      words[i] = stored;
      if (focus != 0 && focus >= length) {
        wordStyles[i] = static_cast<EpdFontFamily::Style>(wordStyles[i] | EpdFontFamily::BOLD);
        focus = 0;
      }
      wordFocusBoundary[i] = focus;
    }
  }
  // Insert the same visible-space token used for NBSP: continuation alone would
  // erase the word gap. Parallel arrays retain ordinary spacing/justification.
  for (size_t i = std::max<size_t>(1, wrenProcessedWords); i < words.size(); ++i) {
    if (wordContinues[i] || wordNoSpaceBefore[i] || wordAt(i - 1) == " " || wordAt(i) == " ") continue;
    const bool dialogue = !paragraphStartEmitted && i == 1;
    bool glue = wren::gluePair(wordAt(i - 1), wordAt(i), wrenLanguage, dialogue);
    if (wordAt(i - 1) == "и" && wordAt(i) == "т.")
      glue = i + 1 < words.size() && (wordAt(i + 1) == "д." || wordAt(i + 1) == "п.");
    if (!glue) continue;
    WordStore::StoredWord space;
    if (!storeWord(" ", space)) return false;
    const uint32_t offset = visibleOffsetAt(i);
    words.insert(words.begin() + i, space);
    wordStyles.insert(wordStyles.begin() + i, wordStyles[i - 1]);
    wordContinues.insert(wordContinues.begin() + i, true);
    wordNoSpaceBefore.insert(wordNoSpaceBefore.begin() + i, false);
    wordContinues[i + 1] = true;
    wordFocusBoundary.insert(wordFocusBoundary.begin() + i, 0);
    wordLinkIds.insert(wordLinkIds.begin() + i, 0);
    wordSizeSlots.insert(wordSizeSlots.begin() + i, wordSizeSlots[i - 1]);
    insertVisibleOffset(i, offset > 0 ? offset - 1 : 0);
    if (i < rubyTexts.size()) rubyTexts.insert(rubyTexts.begin() + i, "");
    ++i;
  }
  wrenProcessedWords = words.size();
  return true;
}
