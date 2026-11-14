#pragma once

namespace wren::ui::layout {
constexpr int ROW_HEIGHT = 54;
constexpr int tabX(int width, int index) {
  constexpr int wide[] = {38, 154, 268, 384};
  constexpr int narrow[] = {25, 130, 245, 350};
  return index >= 0 && index < 4 ? (width >= 528 ? wide[index] : narrow[index]) : 0;
}
constexpr int rowCount(int top, int bottom) { return bottom > top ? (bottom - top) / ROW_HEIGHT : 0; }
constexpr int truncationWidth(int left, int right) { return right > left ? right - left : 0; }
}  // namespace wren::ui::layout
