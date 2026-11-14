#pragma once
#include <algorithm>
#include <cstdint>

namespace wren::reader {
inline bool landscape(uint8_t orientation) { return orientation == 1 || orientation == 3; }
inline uint8_t toggleOrientation(uint8_t orientation) { return landscape(orientation) ? 0 : 1; }
inline int scrollStart(int selected, int start, int visible) {
  visible = std::max(1, visible);
  if (selected < start) return selected;
  if (selected >= start + visible) return selected - visible + 1;
  return start;
}
inline bool within(uint32_t now, uint32_t start, uint32_t duration) { return now - start < duration; }
}  // namespace wren::reader
