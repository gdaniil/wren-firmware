#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace wren::reader {
inline int bookmarkPercent(float fraction) {
  if (!std::isfinite(fraction)) return 0;
  return static_cast<int>(std::clamp(fraction, 0.0f, 1.0f) * 100.0f + 0.5f);
}

inline void bookmarkMeta(char* out, size_t size, const char* format, float fraction, int tocIndex, const char* name) {
  const int percent = bookmarkPercent(fraction);
  const int written =
      tocIndex >= 0 ? snprintf(out, size, format, percent, tocIndex + 1) : snprintf(out, size, "%d%%", percent);
  if (name && *name && written >= 0 && static_cast<size_t>(written) < size) {
    snprintf(out + written, size - written, " · %s", name);
  }
}

// Return the first whole entry that keeps selected in a viewport of height pixels.
inline int bookmarkScrollStart(const int* heights, int count, int selected, int start, int height) {
  if (!heights || count <= 0) return 0;
  selected = std::clamp(selected, 0, count - 1);
  start = std::clamp(start, 0, selected);
  int used = 0;
  for (int i = start; i <= selected; ++i) used += std::max(0, heights[i]);
  while (start < selected && used > height) used -= std::max(0, heights[start++]);
  return start;
}
}  // namespace wren::reader
