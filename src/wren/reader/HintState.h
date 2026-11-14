#pragma once

#include <cstdint>

namespace wren::reader {
class HintState {
 public:
  bool visible() const { return shown; }
  bool reveal(uint32_t now) {
    if (shown) return false;
    shown = true;
    lastInput = now;
    return true;
  }
  void input(uint32_t now) {
    if (shown) lastInput = now;
  }
  bool expire(uint32_t now) {
    if (!shown || static_cast<uint32_t>(now - lastInput) < 3000) return false;
    shown = false;
    return true;
  }
  void hide() { shown = false; }

 private:
  bool shown = false;
  uint32_t lastInput = 0;
};
}  // namespace wren::reader
