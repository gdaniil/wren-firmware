#pragma once

#include <cstddef>
#include <cstdint>

namespace wren::wifi {
enum class Subtitle { Saved, Secured, Open };
enum class Row { Network, ScanAgain, HiddenNetwork };
enum class Failure { General, NoSsid, Timeout };
constexpr Failure failureCause(bool noSsid, bool timeout) {
  return timeout ? Failure::Timeout : noSsid ? Failure::NoSsid : Failure::General;
}
enum class Prompt { Save, Failed, Forget, NoneFound };

constexpr int signalBars(int32_t rssi) { return rssi >= -50 ? 4 : rssi >= -60 ? 3 : rssi >= -70 ? 2 : 1; }
constexpr Subtitle subtitle(bool saved, bool secured) {
  return saved ? Subtitle::Saved : secured ? Subtitle::Secured : Subtitle::Open;
}
constexpr size_t rowCount(size_t networks) { return networks + 2; }
constexpr Row row(size_t index, size_t networks) {
  return index < networks ? Row::Network : index == networks ? Row::ScanAgain : Row::HiddenNetwork;
}
constexpr bool noNetworks(size_t networks) { return networks == 0; }
constexpr size_t move(size_t selected, size_t count, int direction) {
  return count == 0 ? 0 : direction < 0 ? (selected == 0 ? count - 1 : selected - 1) : (selected + 1) % count;
}
constexpr size_t scrollStart(size_t start, size_t selected, size_t visible, size_t count) {
  if (!visible || count <= visible) return 0;
  if (selected < start) start = selected;
  if (selected >= start + visible) start = selected - visible + 1;
  return start > count - visible ? count - visible : start;
}
constexpr size_t defaultRow(Prompt) { return 0; }
}  // namespace wren::wifi
