#pragma once
#include <algorithm>
#include <cstddef>

namespace wren::reader {
inline int stepPercent(int value, int delta) { return std::clamp(value + delta, 0, 100); }
template <typename Cumulative>
int percentSpine(int percent, size_t size, int count, Cumulative cumulative) {
  if (!size || count <= 0) return -1;
  percent = std::clamp(percent, 0, 100);
  size_t target = (size / 100) * percent + (size % 100) * percent / 100;
  if (target >= size) target = size - 1;
  for (int i = 0; i < count; ++i)
    if (target <= cumulative(i)) return i;
  return count - 1;
}
}  // namespace wren::reader
