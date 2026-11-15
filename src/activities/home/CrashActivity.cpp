#include "CrashActivity.h"

#include <GfxRenderer.h>
#include <HalSystem.h>
#include <I18n.h>

#include "wren/system/WrenSystemScreen.h"

void CrashActivity::onEnter() {
  Activity::onEnter();
  panicMessage = HalSystem::getPanicInfo(false);
  requestUpdateAndWait();
}

void CrashActivity::loop() {
  int x = 0;
  int y = 0;
  if (mappedInput.wasPressed(MappedInputManager::Button::Back) ||
      mappedInput.wasPressed(MappedInputManager::Button::Confirm) || mappedInput.wasScreenTapped(x, y)) {
    finish();
  }
}

void CrashActivity::render(RenderLock&&) {
  wren::system::drawStateScreen(renderer, wren::ui::icons::WrenConfused, tr(STR_WREN_CRASH_TITLE),
                                tr(STR_WREN_CRASH_BODY), tr(STR_WREN_SYS_HOME), 64, panicMessage.c_str());
  renderer.displayBuffer();
}
