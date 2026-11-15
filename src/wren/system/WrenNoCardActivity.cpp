#include "WrenNoCardActivity.h"

#include <Arduino.h>

#include "WrenSystemScreen.h"

void WrenNoCardActivity::onEnter() {
  Activity::onEnter();
  requestUpdateAndWait();
}

void WrenNoCardActivity::loop() {
  if (mappedInput.wasPressed(MappedInputManager::Button::Confirm)) ESP.restart();
}

void WrenNoCardActivity::render(RenderLock&&) {
  wren::system::drawNoCardScreen(renderer);
  renderer.displayBuffer();
}
