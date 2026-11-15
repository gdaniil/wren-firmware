#pragma once

#include <string>

namespace wren::home {
constexpr int ROW_THUMB_HEIGHT = 84;
constexpr int CARD_THUMB_HEIGHT = 196;
int loadBookProgress(const std::string& path);
bool thumbPath(const std::string& path, int height, std::string& out);
}  // namespace wren::home
