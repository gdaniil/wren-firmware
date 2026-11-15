#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace wren::home {

constexpr uint16_t NO_BOOK = UINT16_MAX;

// Skipped RecentDesc rows are sorted and unique. This maps the unpinned
// portion of the list to a sort row without reading the index for each skip.
inline uint16_t sortRowFor(uint16_t row, const uint16_t* skipped, size_t skippedCount, uint16_t indexCount) {
  uint32_t result = row;
  for (size_t i = 0; i < skippedCount; ++i) {
    if (skipped[i] <= result) ++result;
  }
  return result < indexCount ? static_cast<uint16_t>(result) : NO_BOOK;
}

enum class ScreenState { Scanning, Empty, Library, Search };

inline ScreenState screenState(bool scanning, uint16_t bookCount, bool searching) {
  if (scanning) return ScreenState::Scanning;
  if (searching) return ScreenState::Search;
  return bookCount == 0 ? ScreenState::Empty : ScreenState::Library;
}

inline int moveFocus(int focus, int delta, int itemCount) {
  if (itemCount <= 0) return -1;
  return std::max(0, std::min(itemCount - 1, focus + delta));
}

inline int scrollStart(int start, int focusedRow, int visibleRows, int rowCount) {
  if (visibleRows <= 0 || rowCount <= 0) return 0;
  start = std::max(0, std::min(start, std::max(0, rowCount - visibleRows)));
  if (focusedRow < 0) return start;  // The continue card has focus.
  focusedRow = std::min(focusedRow, rowCount - 1);
  if (focusedRow < start) return focusedRow;
  if (focusedRow >= start + visibleRows) return focusedRow - visibleRows + 1;
  return start;
}

inline uint32_t titleHash(const char* title) {
  uint32_t hash = 2166136261u;
  while (*title) {
    hash = (hash ^ static_cast<uint8_t>(*title++)) * 16777619u;
  }
  return hash;
}

inline unsigned coverPattern(const char* title) { return titleHash(title) % 5; }
inline bool useSmallTabFont(int labelWidth, int cellWidth) { return labelWidth > cellWidth - 8; }

}  // namespace wren::home
