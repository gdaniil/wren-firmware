#pragma once

#include <SdCardFontRegistry.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

#include "fontIds.h"

// Reader font size is stored as an actual point size (see CrossPointSettings::
// fontPointSize), not an abstract Small/Medium/Large slot. The selectable sizes
// therefore come from whichever family is active: the built-in or vector set
// below, or the .cpfont files a user installed for an SD family.

// Preserve the four legacy size choices; Wren mode and step control Wren typography.
inline constexpr uint8_t BUILTIN_READER_POINT_SIZES[] = {12, 14, 16, 18};
inline constexpr uint8_t BUILTIN_WREN_XHEIGHTS[] = {12, 14, 17};
inline constexpr int BUILTIN_WREN_FONT_IDS[] = {WREN_LITERATA_12_FONT_ID, WREN_LITERATA_14_FONT_ID,
                                                WREN_LITERATA_17_FONT_ID};

// Selects a built-in size without loading it; returns 0 for other font families.
inline int builtinReaderFontVariant(const int fontId, const float scale) {
  const int* family = BUILTIN_WREN_FONT_IDS;
  const int* end = family + std::size(BUILTIN_WREN_XHEIGHTS);
  const int* base = std::find(family, end, fontId);
  if (base == end) return 0;
  const float target = BUILTIN_WREN_XHEIGHTS[base - family] * scale;
  size_t pick = scale < 1.0f ? 0 : static_cast<size_t>(base - family);
  for (size_t i = 0; i < std::size(BUILTIN_WREN_XHEIGHTS); ++i) {
    const float size = BUILTIN_WREN_XHEIGHTS[i];
    if (scale < 1.0f ? size <= target : std::fabs(size - target) < std::fabs(BUILTIN_WREN_XHEIGHTS[pick] - target)) {
      pick = i;
    }
  }
  return family[pick];
}

// Vector (.ttf/.otf) fonts offer every whole point size from 8 through 22.
inline constexpr uint8_t VECTOR_READER_POINT_SIZES[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22};

// Point sizes selectable for the active reader font, ascending: the vector set
// or installed .cpfont sizes for a known SD family, otherwise the built-in set.
// Never returns empty.
std::vector<uint8_t> readerFontPointSizes(const SdCardFontRegistry* registry, const char* sdFamilyName);

// Closest entry in `sizes` (ascending, `count` > 0) to `pt`; ties resolve to the
// smaller size. Takes a raw range rather than a vector because getReaderFontId()
// runs inside the page render loop and must not allocate.
uint8_t snapToNearestPointSize(const uint8_t* sizes, size_t count, uint8_t pt);

inline uint8_t snapToNearestPointSize(const std::vector<uint8_t>& sizes, const uint8_t pt) {
  return sizes.empty() ? pt : snapToNearestPointSize(sizes.data(), sizes.size(), pt);
}

inline uint8_t builtinWrenXHeight(const uint8_t requested) {
  return snapToNearestPointSize(BUILTIN_WREN_XHEIGHTS, std::size(BUILTIN_WREN_XHEIGHTS), requested);
}

inline int builtinWrenFontId(const uint8_t xHeight) {
  for (size_t i = 0; i < std::size(BUILTIN_WREN_XHEIGHTS); ++i) {
    if (BUILTIN_WREN_XHEIGHTS[i] == xHeight) return BUILTIN_WREN_FONT_IDS[i];
  }
  return WREN_LITERATA_14_FONT_ID;
}
