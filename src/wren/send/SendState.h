#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace wren::send {
enum class State { Choose, JoinPick, ApStarting, Running, Failed };
enum class HotspotStep { Join, Open };
enum class MenuRow { JoinNetwork, CreateHotspot, CalibreWireless, UsbDrive };

constexpr int menuRowCount(bool usb) { return usb ? 4 : 3; }
constexpr MenuRow menuRow(int index) {
  constexpr MenuRow rows[] = {MenuRow::JoinNetwork, MenuRow::CreateHotspot, MenuRow::CalibreWireless,
                              MenuRow::UsbDrive};
  return rows[index];
}
inline void formatSsid(char (&out)[10], const uint8_t (&mac)[6]) {
  std::snprintf(out, sizeof(out), "wren-%02x%02x", static_cast<unsigned>(mac[4]), static_cast<unsigned>(mac[5]));
}
struct HotspotSteps {
  HotspotStep step = HotspotStep::Join;
  unsigned zeroPolls = 0;
  bool poll(unsigned stations, bool received, bool uploading) {
    const auto before = step;
    if (stations || uploading) {
      step = HotspotStep::Open;
      zeroPolls = 0;
    } else if (step == HotspotStep::Open && !received) {
      if (++zeroPolls >= 2) {
        step = HotspotStep::Join;
        zeroPolls = 0;
      }
    } else {
      zeroPolls = 0;
    }
    return step != before;
  }
};
struct ReceivedFile {
  char name[96] = {};
  uint64_t size = 0;
};
struct ReceivedList {
  ReceivedFile files[3] = {};
  size_t count = 0;
  void add(const char* name, uint64_t size) {
    files[2] = files[1];
    files[1] = files[0];
    std::snprintf(files[0].name, sizeof(files[0].name), "%s", name ? name : "");
    files[0].size = size;
    ++count;
  }
  size_t visibleCount() const { return count < 3 ? count : 3; }
};
constexpr unsigned percent(uint64_t received, uint64_t total) {
  if (!total) return 0;
  if (received >= total) return 100;
  // Find the greatest percentage whose rounded-up byte threshold was reached.
  unsigned low = 0, high = 100;
  while (low < high) {
    const unsigned mid = (low + high + 1) / 2;
    const uint64_t threshold = (total / 100) * mid + ((total % 100) * mid + 99) / 100;
    if (received >= threshold)
      low = mid;
    else
      high = mid - 1;
  }
  return low;
}
constexpr unsigned progressBand(unsigned value) { return value >= 75 ? 3 : value >= 50 ? 2 : value >= 25 ? 1 : 0; }
struct UploadRedraw {
  bool pending = false;
  unsigned drawnBand = 0;
  uint32_t last = 0;
  bool update(bool active, bool event, unsigned progress, uint32_t now) {
    if (event || (active && progressBand(progress) > drawnBand)) pending = true;
    if (!pending || (active && static_cast<uint32_t>(now - last) < 2000)) return false;
    last = now;
    drawnBand = active ? progressBand(progress) : 0;
    pending = false;
    return true;
  }
};
constexpr int scrollStart(int start, int selected, int visible, int count) {
  if (visible <= 0 || count <= visible) return 0;
  if (selected < start) start = selected;
  if (selected >= start + visible) start = selected - visible + 1;
  if (start > count - visible) start = count - visible;
  return start < 0 ? 0 : start;
}
}  // namespace wren::send
