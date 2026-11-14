#pragma once
#include <algorithm>

namespace wren::reader {
enum class SettingsRow { Font, Size, Alignment, Orientation };
inline int cycleSetting(int value, int delta, int count) { return (value + (delta % count) + count) % count; }
inline int extraSpace(int remaining, int gaps, int index) {
  return gaps > 0 ? std::max(0, remaining) / gaps + (index < std::max(0, remaining) % gaps ? 1 : 0) : 0;
}
}  // namespace wren::reader
