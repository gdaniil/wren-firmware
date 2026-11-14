#pragma once
#include <memory>
#include <string>

#include "activities/Activity.h"
#include "util/ButtonNavigator.h"
class Epub;
class WrenGoToActivity final : public Activity {
 public:
  WrenGoToActivity(GfxRenderer& renderer, MappedInputManager& input, int initial, const std::string& title,
                   std::shared_ptr<Epub> epub);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool handleHomeGesture() override;

 private:
  void adjust(int delta);
  void updateChapter();
  void cancel();
  int percent;
  std::string title, chapter;
  std::shared_ptr<Epub> epub;
  ButtonNavigator navigator{250, 500};
  bool first = true;
};
