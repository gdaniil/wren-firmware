#pragma once

namespace wren {
inline bool isEnglishLanguage(const char* language) {
  if (!language) return false;
  const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c; };
  if (lower(language[0]) != 'e' || lower(language[1]) != 'n') return false;
  return language[2] == '\0' || language[2] == '-' || language[2] == '_' ||
         (lower(language[2]) == 'g' && (language[3] == '\0' || language[3] == '-' || language[3] == '_'));
}
}  // namespace wren
