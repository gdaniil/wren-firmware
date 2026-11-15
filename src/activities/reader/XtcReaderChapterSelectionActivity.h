#pragma once
#include <Xtc.h>

#include <memory>
#include <string>

#include "activities/Activity.h"
#include "components/UiAppHost.h"
#include "util/ButtonNavigator.h"

class XtcReaderChapterSelectionActivity final : public Activity, protected UiAppHost {
 public:
  XtcReaderChapterSelectionActivity(GfxRenderer&, MappedInputManager&, const std::shared_ptr<Xtc>&, uint32_t);
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static void buildHits(UiScreen& screen, void* context);
  int visible() const;
  void move(int index);
  void activate(int index);
  std::shared_ptr<Xtc> xtc;
  uint32_t currentPage;
  std::string title;
  int selected = 0;
  int start = 0;
  ButtonNavigator navigator;
};
