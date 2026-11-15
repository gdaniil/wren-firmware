#pragma once

namespace wren::setup {
enum class RootRow { Reader, Display, System, Files, Count };
enum class DisplayRow { SleepScreen, QuickResume, Battery, SunlightFix, Count };
enum class SystemRow { SleepAfter, Wifi, Language, Titles, ClearCache, Rebuild, Count };
constexpr int displayRowCount(bool touch) { return touch ? 3 : 4; }
constexpr int optionIndex(int value, const int* options, int count) {
  for (int i = 0; i < count; ++i)
    if (options[i] == value) return i;
  return -1;
}
constexpr int sleepScreenIndex(int value) {
  constexpr int options[] = {0, 3};  // wren (DARK), cover
  return optionIndex(value, options, 2);
}
constexpr int batteryIndex(int value) { return value == 0 ? 0 : 1; }
constexpr int sleepAfterIndex(int value) {
  constexpr int options[] = {5, 10, 30};
  return optionIndex(value, options, 3);
}
constexpr int languageIndex(int value, int english, int ukrainian) {
  const int options[] = {english, ukrainian};
  return optionIndex(value, options, 2);
}
constexpr int scrollStart(int start, int selected, int visible, int count) {
  if (visible <= 0 || count <= visible) return 0;
  if (selected < start) start = selected;
  if (selected >= start + visible) start = selected - visible + 1;
  if (start > count - visible) start = count - visible;
  return start < 0 ? 0 : start;
}
}  // namespace wren::setup
