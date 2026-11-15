#pragma once

#include <atomic>
#include <string>

class GfxRenderer;
class MappedInputManager;

class EndOfBookOptions {
 public:
  enum class Action { None, Redraw, GoHome, Restart, Delete, LastPage };

  EndOfBookOptions(GfxRenderer& renderer, std::string title, std::string author);
  Action handleMenuInput(const MappedInputManager& input);
  void moveSelection(bool forward);
  void render(GfxRenderer& renderer, const MappedInputManager& input);
  const std::string& displayTitle() const { return title; }

 private:
  Action selectedAction(int selected) const;
  GfxRenderer& renderer;
  std::string title;
  std::string subtitle;
  std::atomic<int> rowTop;
  std::atomic<int> selector{0};
};
