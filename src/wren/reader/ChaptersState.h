#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
namespace wren::reader {
inline int chapterPercent(uint64_t before, uint64_t total) {
  return total ? static_cast<int>(std::min<uint64_t>(100, (before * 100 + total / 2) / total)) : 0;
}
inline void chapterNumber(char* out, size_t size, int index) { snprintf(out, size, "%02d", index + 1); }
inline int chapterPageJump(int selected, int delta, int count) {
  return std::max(0, std::min(count - 1, selected + delta));
}
}  // namespace wren::reader
