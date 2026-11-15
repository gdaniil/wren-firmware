#include "BootActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "wren/WrenVersion.h"
#include "wren/ui/WrenChrome.h"
#include "wren/ui/WrenFonts.h"

void BootActivity::onEnter() {
  Activity::onEnter();
  using namespace wren::ui;
  const int width = renderer.getScreenWidth();
  const int height = renderer.getScreenHeight();
  const int groupHeight = 192 + 24 + 56 + 12 + 19;
  int y = (height - groupHeight) / 2;
  renderer.clearScreen();
  drawIcon(renderer, icons::WrenHello, (width - 192) / 2, y, 6);
  y += 192 + 24;
  const char* hello = tr(STR_WREN_BOOT_HELLO);
  drawTextFit(renderer, WREN_GOLOS_56_HEADLINE_FONT_ID,
              std::max(0, (width - textWidth(renderer, WREN_GOLOS_56_HEADLINE_FONT_ID, hello, -2)) / 2), y, hello,
              width, true, -2);
  y += 56 + 12;
  char version[48];
  snprintf(version, sizeof(version), tr(STR_WREN_FW_VERSION), WREN_VERSION);
  drawLabel(renderer, (width - labelWidth(renderer, version)) / 2, y, version);
  renderer.displayBuffer();
}
