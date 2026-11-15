#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include "SetupState.h"

namespace wren::files {
enum class IconKind { Folder, Book, Image };
constexpr bool directory(std::string_view name) { return !name.empty() && name.back() == '/'; }
constexpr unsigned char lower(unsigned char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; }
inline bool less(std::string_view a, std::string_view b) {
  if (directory(a) != directory(b)) return directory(a);
  for (size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    if (lower(a[i]) != lower(b[i])) return lower(a[i]) < lower(b[i]);
  }
  return a.size() < b.size();
}
inline IconKind iconKind(std::string_view name) {
  if (directory(name)) return IconKind::Folder;
  if (name.size() >= 4) {
    auto ext = name.substr(name.size() - 4);
    if (ext[0] == '.' && lower(ext[1]) == 'b' && lower(ext[2]) == 'm' && lower(ext[3]) == 'p') return IconKind::Image;
  }
  return IconKind::Book;
}
inline void formatSize(char* out, size_t capacity, uint64_t bytes) {
  if (bytes < 104858)
    snprintf(out, capacity, "%llu kb", static_cast<unsigned long long>((bytes + 512) / 1024));
  else {
    const uint64_t tenths = (bytes / 1048576) * 10 + ((bytes % 1048576) * 10 + 524288) / 1048576;
    snprintf(out, capacity, "%llu.%llu mb", static_cast<unsigned long long>(tenths / 10),
             static_cast<unsigned long long>(tenths % 10));
  }
}
// Return a suffix on a UTF-8 boundary; the caller draws an ellipsis before it.
inline std::string_view pathTail(std::string_view path, size_t maxBytes) {
  size_t start = path.size() > maxBytes ? path.size() - maxBytes : 0;
  while (start < path.size() && (static_cast<unsigned char>(path[start]) & 0xc0) == 0x80) ++start;
  return path.substr(start);
}
inline int scrollStart(int start, int selected, int visible, int count) {
  return wren::setup::scrollStart(start, selected, visible, count);
}
inline int clampSelection(int selected, int count) { return count <= 0 ? 0 : std::clamp(selected, 0, count - 1); }
}  // namespace wren::files
