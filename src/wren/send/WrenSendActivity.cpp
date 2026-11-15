#include "WrenSendActivity.h"

#include <ESPmDNS.h>
#include <FontCacheManager.h>
#include <HalGPIO.h>
#include <HalStorage.h>
#include <Memory.h>
#include <WiFi.h>

#include <algorithm>
#include <cstring>

#include "SilentRestart.h"
#include "activities/network/CalibreConnectActivity.h"
#include "util/PluginEvents.h"
#include "util/TaskWatchdog.h"
#include "wren/send/WrenWifiActivity.h"
#include "wren/setup/SetupUi.h"
#include "wren/ui/WrenFonts.h"

namespace {
#if FREEINK_CAP_USB_MSC
constexpr bool HAS_USB = true;
#else
constexpr bool HAS_USB = false;
#endif
constexpr unsigned long WIFI_ABANDON_MS = 5UL * 60UL * 1000UL;
int barsForRssi(int rssi, int bars) {
  constexpr int rise[] = {-85, -75, -65, -55};
  constexpr int fall[] = {-88, -78, -68, -58};
  bars = std::clamp(bars, 0, 4);
  while (bars < 4 && rssi >= rise[bars]) ++bars;
  while (bars > 0 && rssi < fall[bars - 1]) --bars;
  return bars;
}
void restartMdns() {
  MDNS.end();
  if (!MDNS.begin("crosspoint")) LOG_ERR("SEND", "mDNS failed to start");
}
}  // namespace

void WrenSendActivity::onEnter() {
  Activity::onEnter();
  if (auto* fcm = renderer.getFontCacheManager()) fcm->releaseSdFontCaches();
  requestUpdate();
  if (startInJoinNetwork) joinNetwork();
}
void WrenSendActivity::cleanupNetwork() {
  if (dns) {
    dns->stop();
    dns.reset();
  }
  MDNS.end();
  if (webServer) {
    webServer->stop();
    webServer.reset();
  }
  if (WiFi.getMode() != WIFI_MODE_NULL) {
    if (apMode)
      WiFi.softAPdisconnect(true);
    else
      WiFi.disconnect(false);
    WiFi.mode(WIFI_OFF);
  }
}
void WrenSendActivity::onExit() {
  Activity::onExit();
  const bool wifiActive = WiFi.getMode() != WIFI_MODE_NULL;
  cleanupNetwork();
  pluginevents::refreshSubscriptions();
  if (wifiActive) {
    delay(30);
    silentRestart();
  }
}
void WrenSendActivity::joinNetwork() {
  if (!startInJoinNetwork) silentRestartToJoinNetwork();
  apMode = false;
  WiFi.mode(WIFI_STA);
  state = wren::send::State::JoinPick;
  // The child is retained until selection returns and owns the Wi-Fi selection UI.
  auto child = makeUniqueNoThrow<WrenWifiActivity>(renderer, mappedInput);
  if (!child) {
    LOG_ERR("SEND", "OOM: Wi-Fi selection activity");
    fail(Failure::Server);
    return;
  }
  const auto orientation = renderer.getOrientation();
  startActivityForResult(std::move(child), [this, orientation](const ActivityResult& result) {
    {
      RenderLock lock;
      renderer.setOrientation(orientation);
      if (result.isCancelled) {
        state = wren::send::State::Choose;
        return;
      }
      const auto& wifi = std::get<WifiResult>(result.data);
      snprintf(ip, sizeof(ip), "%s", wifi.ip.c_str());
      snprintf(ssid, sizeof(ssid), "%s", wifi.ssid.c_str());
    }
    restartMdns();
    startServer();
  });
}
void WrenSendActivity::choose(int row) {
  using namespace wren::send;
  switch (menuRow(row)) {
    case MenuRow::JoinNetwork:
      joinNetwork();
      return;
    case MenuRow::CreateHotspot:
      cleanupNetwork();
      apMode = true;
      state = State::ApStarting;
      startFrameShown = false;
      requestUpdate();
      return;
    case MenuRow::CalibreWireless: {
      // The parent remains on the activity stack until Calibre returns.
      auto child = makeUniqueNoThrow<CalibreConnectActivity>(renderer, mappedInput);
      if (!child) {
        LOG_ERR("SEND", "OOM: Calibre activity");
        return;
      }
      const auto orientation = renderer.getOrientation();
      startActivityForResult(std::move(child), [this, orientation](const ActivityResult&) {
        RenderLock lock;
        renderer.setOrientation(orientation);
        state = State::Choose;
        cleanRefresh = true;
      });
      return;
    }
    case MenuRow::UsbDrive:
#if FREEINK_CAP_USB_MSC
      activityManager.goToUsbDrive();
#endif
      return;
  }
}
void WrenSendActivity::startAccessPoint() {
  WiFi.mode(WIFI_AP);
  delay(100);
  uint8_t mac[6]{};
  WiFi.softAPmacAddress(mac);
  char name[10];
  wren::send::formatSsid(name, mac);
  {
    RenderLock lock;
    snprintf(ssid, sizeof(ssid), "%s", name);
  }
  if (!WiFi.softAP(name, nullptr, 1, false, 4)) {
    LOG_ERR("SEND", "AP start failed");
    fail(Failure::Hotspot);
    return;
  }
  delay(100);
  const IPAddress address = WiFi.softAPIP();
  {
    RenderLock lock;
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", address[0], address[1], address[2], address[3]);
  }
  restartMdns();
  // DNS is allocated once for the AP session and released on failure or exit.
  dns = makeUniqueNoThrow<DNSServer>();
  if (!dns) {
    LOG_ERR("SEND", "OOM: DNS server");
    fail(Failure::Hotspot);
    return;
  }
  dns->setErrorReplyCode(DNSReplyCode::NoError);
  if (!dns->start(53, "*", address)) {
    LOG_ERR("SEND", "DNS start failed");
    fail(Failure::Hotspot);
    return;
  }
  startServer();
}
void WrenSendActivity::startServer() {
  {
    RenderLock lock;
    if (auto* fcm = renderer.getFontCacheManager()) fcm->releaseSdFontCaches();
  }
  // One server owns all transfer services until retry or exit.
  webServer = makeUniqueNoThrow<CrossPointWebServer>();
  if (!webServer) {
    LOG_ERR("SEND", "OOM: transfer server");
    fail(apMode ? Failure::Hotspot : Failure::Server);
    return;
  }
  webServer->setUploadCancelCheck([this] {
    // This callback is also used by font uploads; only an open ordinary upload is recorded.
    const auto& upload = webServer->upload;
    if (upload.file && !upload.fileName.isEmpty()) {
      const int n = snprintf(httpPath, sizeof(httpPath), "%s%s%s", upload.path.c_str(),
                             upload.path.endsWith("/") ? "" : "/", upload.fileName.c_str());
      if (n > 0 && static_cast<size_t>(n) < sizeof(httpPath)) {
        snprintf(httpName, sizeof(httpName), "%s", upload.fileName.c_str());
        httpSize = upload.size;
        httpPending = true;
      }
    }
    mappedInput.update(true);
    if (gpio.rawInputActive()) {
      delay(6);
      mappedInput.update(true);
    }
    leaveRequested = leaveRequested || mappedInput.isPressed(MappedInputManager::Button::Back) ||
                     mappedInput.wasPressed(MappedInputManager::Button::Back) || mappedInput.wasHomeGesture();
    return leaveRequested;
  });
  webServer->begin();
  if (!webServer->isRunning()) {
    LOG_ERR("SEND", "Transfer server failed");
    fail(apMode ? Failure::Hotspot : Failure::Server);
    return;
  }
  lastCompleteAt = webServer->getWsUploadStatus().lastCompleteAt;
  const int bars = apMode ? 0 : barsForRssi(WiFi.RSSI(), 0);
  {
    RenderLock lock;
    prepareQr();
    wifiBars = bars;
    state = wren::send::State::Running;
  }
  if (!apMode) {
    {
      RenderLock lock;
      drainingEvents = true;
    }
    pluginevents::drain(&renderer);
    {
      RenderLock lock;
      drainingEvents = false;
    }
  }
  requestUpdate();
}
void WrenSendActivity::prepareQr() {
  char payload[64];
  snprintf(payload, sizeof(payload), "WIFI:T:nopass;S:%s;;", ssid);
  qrcode_initText(&joinQr, joinQrBytes, 4, ECC_LOW, payload);
  snprintf(payload, sizeof(payload), "http://%s/", ip);
  qrcode_initText(&openQr, openQrBytes, 4, ECC_LOW, payload);
}
void WrenSendActivity::fail(Failure reason) {
  cleanupNetwork();
  failure = reason;
  state = wren::send::State::Failed;
  selected = scroll = 0;
  consecutiveDisconnects = 0;
  requestUpdate();
}
void WrenSendActivity::scheduleUploadRedraw() {
  if (uploadRedraw.update(wsInProgress, true, wsPercent, millis())) requestUpdate();
}
void WrenSendActivity::updateUploads() {
  // The server API returns owned strings; their copies live only for this poll.
  const auto status = webServer->getWsUploadStatus();
  RenderLock lock;
  bool changed = false;
  if (status.lastCompleteAt && status.lastCompleteAt != lastCompleteAt) {
    lastCompleteAt = status.lastCompleteAt;
    received.add(status.lastCompleteName.c_str(), status.lastCompleteSize);
    changed = true;
  }
  const bool started = status.inProgress && !wsInProgress;
  const bool ended = !status.inProgress && wsInProgress;
  wsInProgress = status.inProgress;
  if (wsInProgress) {
    snprintf(wsName, sizeof(wsName), "%s", status.filename.c_str());
    wsSize = status.total;
    wsPercent = wren::send::percent(status.received, status.total);
    if (apMode && hotspot.poll(0, received.count != 0, true)) changed = true;
  }
  if (changed && apMode && hotspot.poll(0, true, true)) changed = true;
  if (uploadRedraw.update(wsInProgress, started || ended || changed, wsPercent, millis())) requestUpdate();
}
void WrenSendActivity::loop() {
  using B = MappedInputManager::Button;
  using namespace wren::send;
  if (state == State::ApStarting) {
    if (mappedInput.wasReleased(B::Back) || mappedInput.wasHomeGesture()) {
      activityManager.goHome();
      return;
    }
    if (!startFrameShown) {
      requestUpdateAndWait();
      startFrameShown = true;
      return;
    }
    startAccessPoint();
    return;
  }
  if (state == State::Choose || state == State::Failed) {
    const bool menu = state == State::Choose;
    const int count = menu ? menuRowCount(HAS_USB) : 2;
    int rowTop = 187;
    int touchX = 0, touchY = 0;
    const bool touchInput = mappedInput.wasScreenTouchDown(touchX, touchY) ||
                            mappedInput.wasScreenTapped(touchX, touchY) ||
                            mappedInput.wasSwipe() != MappedInputManager::SwipeDir::None;
    if (!menu && touchInput) {
      const char* failureText = I18N.get(failure == Failure::Hotspot    ? StrId::STR_WREN_SEND_HOTSPOT_FAILED
                                         : failure == Failure::WifiLost ? StrId::STR_WREN_SEND_WIFI_LOST
                                                                        : StrId::STR_WREN_SEND_TRANSFER_FAILED);
      RenderLock lock;
      rowTop =
          212 +
          wren::ui::wrappedLineCount(renderer, WREN_GOLOS_18_FONT_ID, failureText, renderer.getScreenWidth() - 40, 3) *
              25 +
          35;
    }
    const int rowHeight = menu ? 104 : 88;
    const int contentBottom = renderer.getScreenHeight() - (menu ? 72 : 40);
    const int visible = std::max(1, (contentBottom - rowTop) / rowHeight);
    if (wren::setup::swipeSelection(mappedInput, selected, count, visible)) requestUpdate();
    int touchedRow = 0;
    const auto touch = mappedInput.rowTouch(touchedRow, rowTop, rowHeight, std::min(count - scroll, visible));
    if (touch != MappedInputManager::RowTouch::None) {
      selected = scroll + touchedRow;
      requestUpdate();
    }
    int tx = 0, ty = 0;
    if (menu && mappedInput.wasScreenTapped(tx, ty) && ty >= 40 && ty < 96 && tx >= 100 &&
        tx < renderer.getScreenWidth() - 20) {
      const int tab = (tx - 100) * 3 / (renderer.getScreenWidth() - 120);
      if (tab == 0) activityManager.goHome();
      if (tab == 2) activityManager.goToSettings();
      return;
    }
    if (mappedInput.wasReleased(B::Back) || mappedInput.wasBackGesture()) {
      if (menu) {
        activityManager.goHome();
      } else {
        state = State::Choose;
        selected = scroll = 0;
        requestUpdate();
      }
      return;
    }
    if (mappedInput.wasReleased(B::Confirm) || touch == MappedInputManager::RowTouch::Tap) {
      if (menu)
        choose(selected);
      else if (selected == 1) {
        state = State::Choose;
        selected = scroll = 0;
        requestUpdate();
      } else if (apMode) {
        state = State::ApStarting;
        startFrameShown = false;
        requestUpdate();
      } else
        joinNetwork();
      return;
    }
    if (menu && mappedInput.wasPressed(B::Up)) {
      activityManager.goHome();
      return;
    }
    if (menu && mappedInput.wasPressed(B::Down)) {
      activityManager.goToSettings();
      return;
    }
    navigator.onPressAndContinuous({B::Left}, [this, count] {
      selected = (selected + count - 1) % count;
      requestUpdate();
    });
    navigator.onPressAndContinuous({B::Right}, [this, count] {
      selected = (selected + 1) % count;
      requestUpdate();
    });
    return;
  }
  if (state != State::Running || !webServer) return;
  if (apMode && dns) dns->processNextRequest();
  const unsigned long now = millis();
  if (apMode && now - lastStationPoll >= 1000) {
    lastStationPoll = now;
    const int stations = WiFi.softAPgetStationNum();
    RenderLock lock;
    if (hotspot.poll(stations, received.count != 0, wsInProgress)) scheduleUploadRedraw();
  }
  if (!apMode && now - lastWifiCheck >= 2000) {
    lastWifiCheck = now;
    const bool connected = WiFi.status() == WL_CONNECTED;
    const int rssi = connected ? WiFi.RSSI() : 0;
    bool repaint = false;
    if (!connected) {
      if (!consecutiveDisconnects++) {
        firstDisconnectAt = now;
        repaint = true;
      }
      if (now - firstDisconnectAt >= WIFI_ABANDON_MS) {
        fail(Failure::WifiLost);
        return;
      }
    } else {
      if (consecutiveDisconnects) repaint = true;
      consecutiveDisconnects = 0;
      const int bars = barsForRssi(rssi, wifiBars);
      if (bars != wifiBars) {
        wifiBars = bars;
        repaint = true;
      }
    }
    if (repaint) scheduleUploadRedraw();
  }
  const unsigned long gap = millis() - lastHandleClientTime;
  if (lastHandleClientTime && gap > 100) LOG_DBG("SEND", "handleClient gap: %lu ms", gap);
  resetTaskWatchdogIfSubscribed();
  for (int i = 0; i < 500 && webServer->isRunning(); ++i) {
    webServer->handleClient();
    if (httpPending) {
      httpPending = false;
      if (Storage.exists(httpPath)) {
        RenderLock lock;
        received.add(httpName, httpSize);
        if (apMode) hotspot.poll(0, true, true);
        scheduleUploadRedraw();
      }
    }
    if (leaveRequested) {
      onGoHome();
      return;
    }
    if ((i & 31) == 31) resetTaskWatchdogIfSubscribed();
    if ((i & 63) == 63) {
      yield();
      mappedInput.update(true);
      if (mappedInput.wasReleased(B::Back) || mappedInput.wasHomeGesture()) {
        onGoHome();
        return;
      }
    }
  }
  lastHandleClientTime = millis();
  // getWsUploadStatus() copies strings: poll it a few times a second, not on every loop pass.
  if (lastHandleClientTime - lastUploadPoll >= 250) {
    lastUploadPoll = lastHandleClientTime;
    updateUploads();
  }
  if (mappedInput.wasReleased(B::Back) || mappedInput.wasHomeGesture()) onGoHome();
}
void WrenSendActivity::footer(bool menu) {
  using namespace wren::ui;
  if (menu) {
    wren::setup::footer(renderer, state == wren::send::State::Choose);
    return;
  }
  const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_SEND_STOP)}, {}, {}, {}};
  drawButtonTabs(renderer, tabs, false);
}
void WrenSendActivity::drawQr(const QRCode& qr, int x, int y) const {
  QRCode copy = qr;
  if (!copy.size) return;
  const int scale = 198 / copy.size;
  x += (198 - scale * copy.size) / 2;
  y += (198 - scale * copy.size) / 2;
  for (int row = 0; row < copy.size; ++row)
    for (int col = 0; col < copy.size; ++col)
      if (qrcode_getModule(&copy, col, row)) renderer.fillRect(x + col * scale, y + row * scale, scale, scale, true);
}
void WrenSendActivity::renderChoose() {
  using namespace wren::ui;
  drawTabHeader(renderer, 28, 1);
  drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, 20, 124, tr(STR_WREN_SEND_CONNECT_HOW), renderer.getScreenWidth() - 40,
              true, -1);
  renderer.fillRect(0, 184, renderer.getScreenWidth(), 3, true);
  constexpr int top = 187, height = 104;
  const int count = wren::send::menuRowCount(HAS_USB);
  const int bottom = renderer.getScreenHeight() - 72;
  const int visible = std::max(1, (bottom - top) / height);
  scroll = wren::send::scrollStart(scroll, selected, visible, count);
  const StrId labels[] = {StrId::STR_WREN_SEND_JOIN_NETWORK, StrId::STR_WREN_SEND_CREATE_HOTSPOT,
                          StrId::STR_WREN_SEND_CALIBRE, StrId::STR_WREN_SEND_USB};
  for (int i = scroll; i < count && i < scroll + visible; ++i) {
    const int y = top + (i - scroll) * height;
    const bool ink = selected != i;
    renderer.fillRect(0, y, renderer.getScreenWidth(), height - 3, !ink);
    drawTextFit(renderer, WREN_GOLOS_32_FONT_ID, 20, y + 31, I18N.get(labels[i]), renderer.getScreenWidth() - 62, ink,
                -1);
    drawIcon(renderer, icons::ChevronRight, renderer.getScreenWidth() - 26, y + 43, 2, ink);
    renderer.fillRect(0, y + height - 3, renderer.getScreenWidth(), 3, true);
  }
  drawLabel(renderer, 20, renderer.getScreenHeight() - 68, tr(STR_WREN_SEND_ONLY_24));
}
void WrenSendActivity::renderFailed() {
  using namespace wren::ui;
  drawHeader(renderer, 28, tr(STR_WREN_TAB_SEND),
             I18N.get(apMode ? StrId::STR_WREN_SEND_HOTSPOT : StrId::STR_WREN_SEND_WIFI));
  drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, 20, 156, tr(STR_WREN_SEND_FAILED), renderer.getScreenWidth() - 40, true,
              -1);
  const char* message = I18N.get(failure == Failure::Hotspot    ? StrId::STR_WREN_SEND_HOTSPOT_FAILED
                                 : failure == Failure::WifiLost ? StrId::STR_WREN_SEND_WIFI_LOST
                                                                : StrId::STR_WREN_SEND_TRANSFER_FAILED);
  const int lines =
      drawTextWrapped(renderer, WREN_GOLOS_18_FONT_ID, 20, 212, message, renderer.getScreenWidth() - 40, 3, 25);
  const int top = 212 + lines * 25 + 32;
  renderer.fillRect(0, top, renderer.getScreenWidth(), 3, true);
  const int visible = std::max(1, (renderer.getScreenHeight() - 40 - top - 3) / 88);
  scroll = wren::send::scrollStart(scroll, selected, visible, 2);
  const StrId labels[] = {StrId::STR_WREN_SEND_TRY_AGAIN, StrId::STR_WREN_SEND_OTHER_WAYS};
  for (int i = scroll; i < 2 && i < scroll + visible; ++i) {
    const int y = top + 3 + (i - scroll) * 88;
    const bool ink = selected != i;
    renderer.fillRect(0, y, renderer.getScreenWidth(), 85, !ink);
    drawTextFit(renderer, WREN_GOLOS_32_FONT_ID, 20, y + 24, I18N.get(labels[i]), renderer.getScreenWidth() - 62, ink,
                -1);
    drawIcon(renderer, icons::ChevronRight, renderer.getScreenWidth() - 26, y + 35, 2, ink);
    renderer.fillRect(0, y + 85, renderer.getScreenWidth(), 3, true);
  }
}
void WrenSendActivity::renderReceived(int x, int y, int width, int bottom) {
  using namespace wren::ui;
  if (y + 33 > bottom) return;
  drawLabel(renderer, x, y, tr(STR_WREN_SEND_RECEIVED));
  char count[16];
  snprintf(count, sizeof(count), "%u", static_cast<unsigned>(received.count));
  drawLabel(renderer, x + width - labelWidth(renderer, count), y, count);
  renderer.fillRect(x, y + 29, width, 3, true);
  y += 33;
  if (!wsInProgress && received.count == 0) {
    if (y + 25 <= bottom)
      drawTextWrapped(renderer, WREN_GOLOS_18_FONT_ID, x, y + 12, tr(STR_WREN_SEND_EMPTY), width,
                      std::max(1, (bottom - y - 12) / 25), 25);
    return;
  }
  const int rows = static_cast<int>(received.visibleCount()) + (wsInProgress ? 1 : 0);
  for (int i = 0; i < rows && y + 64 <= bottom; ++i, y += 64) {
    const bool incoming = wsInProgress && i == 0;
    const auto& item = received.files[incoming ? 0 : i - (wsInProgress ? 1 : 0)];
    const char* name = incoming ? wsName : item.name;
    const size_t size = incoming ? wsSize : item.size;
    const int rightWidth = incoming ? 200 : 70;
    drawTextFit(renderer, WREN_GOLOS_24_FONT_ID, x, y + 4, name, std::max(0, width - rightWidth - 10), true, -1);
    const char* ext = strrchr(name, '.');
    char meta[64];
    const unsigned long tenths = static_cast<unsigned long>((static_cast<uint64_t>(size) * 10 + 524288) / 1048576);
    snprintf(meta, sizeof(meta), "%.16s · %lu.%lu mb", ext && ext[1] ? ext + 1 : "", tenths / 10, tenths % 10);
    drawTextFit(renderer, WREN_MONO_16_FONT_ID, x, y + 36, meta, std::max(0, width - rightWidth - 10), true, 0);
    if (incoming) {
      char number[8];
      snprintf(number, sizeof(number), "%u%%", wsPercent);
      drawProgressBar(renderer, x + width - 200, y + 24, 140, wsPercent / 100.0f);
      drawLabel(renderer, x + width - labelWidth(renderer, number), y + 18, number);
    } else {
      drawLabel(renderer, x + width - labelWidth(renderer, tr(STR_WREN_SEND_DONE)), y + 18, tr(STR_WREN_SEND_DONE));
    }
    renderer.fillRect(x, y + 61, width, 3, true);
  }
}
void WrenSendActivity::renderRunning() {
  using namespace wren::ui;
  const bool joining = apMode && hotspot.step == wren::send::HotspotStep::Join;
  const bool landscape = renderer.getScreenWidth() > renderer.getScreenHeight();
  const int width = renderer.getScreenWidth();
  const int bottom = renderer.getScreenHeight() - 40;
  drawHeader(renderer, 28, tr(STR_WREN_TAB_SEND),
             I18N.get(apMode ? StrId::STR_WREN_SEND_HOTSPOT : StrId::STR_WREN_SEND_WIFI));
  drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, 20, 156,
              I18N.get(joining  ? StrId::STR_WREN_SEND_JOIN_WREN
                       : apMode ? StrId::STR_WREN_SEND_PHONE_JOINED
                                : StrId::STR_WREN_SEND_READY),
              width - 40, true, -1);
  drawTextFit(renderer, WREN_GOLOS_18_FONT_ID, 20, 210,
              I18N.get(joining  ? StrId::STR_WREN_SEND_JOIN_TEXT
                       : apMode ? StrId::STR_WREN_SEND_OPEN_TEXT
                                : StrId::STR_WREN_SEND_READY_TEXT),
              width - 40);
  const int qrY = 250;
  drawQr(joining ? joinQr : openQr, 20, qrY);
  const int fieldX = 238;
  const int fieldWidth = width - fieldX - 20;
  const int fy = qrY + (landscape ? 0 : 28);
  const int secondX = landscape ? fieldX + fieldWidth / 2 : fieldX;
  const int secondWidth = landscape ? fieldWidth - fieldWidth / 2 : fieldWidth;
  const int secondY = landscape ? fy : fy + 78;
  drawLabel(renderer, fieldX, fy, I18N.get(joining ? StrId::STR_WREN_SEND_NETWORK : StrId::STR_WREN_SEND_ADDRESS));
  drawTextFit(renderer, WREN_GOLOS_32_FONT_ID, fieldX, fy + 22, joining ? ssid : ip,
              landscape ? fieldWidth / 2 - 12 : fieldWidth, true, -1);
  drawLabel(renderer, secondX, secondY,
            I18N.get(joining ? StrId::STR_WREN_SEND_PASSWORD : StrId::STR_WREN_SEND_NETWORK));
  drawTextFit(
      renderer, joining || apMode ? WREN_GOLOS_32_FONT_ID : WREN_GOLOS_24_FONT_ID, secondX, secondY + 22,
      joining ? tr(STR_WREN_SEND_NONE) : (!apMode && consecutiveDisconnects ? tr(STR_WREN_SEND_RECONNECTING) : ssid),
      secondWidth, true, -1);
  if (!apMode && !consecutiveDisconnects) {
    constexpr int heights[] = {6, 10, 14, 18};
    for (int i = 0; i < 4; ++i) {
      const int x = secondX + i * 8, y = secondY + 75 - heights[i];
      if (i < wifiBars)
        renderer.fillRect(x, y, 5, heights[i], true);
      else
        renderer.drawRect(x, y, 5, heights[i], true);
    }
  }
  if (joining) {
    const int x = landscape ? fieldX : 20;
    const int y = landscape ? qrY + 90 : qrY + 220;
    const int w = landscape ? fieldWidth : width - 40;
    const int boxHeight = std::min(100, bottom - y);
    if (boxHeight >= 70) {
      renderer.drawRect(x, y, w, boxHeight, 3, true);
      drawLabel(renderer, x + 14, y + 10, tr(STR_WREN_SEND_NEXT));
      drawTextWrapped(renderer, WREN_GOLOS_18_FONT_ID, x + 14, y + 30, tr(STR_WREN_SEND_NEXT_TEXT), w - 28,
                      std::max(1, (boxHeight - 38) / 24), 24);
    }
  } else if (landscape) {
    renderReceived(fieldX, qrY + 90, fieldWidth, bottom);
  } else {
    renderReceived(20, qrY + 220, width - 40, bottom);
  }
}
void WrenSendActivity::render(RenderLock&&) {
  if (drainingEvents) return;
  using namespace wren::ui;
  using wren::send::State;
  if (state == State::JoinPick) return;
  wren::setup::status(renderer);
  wren::setup::clipContent(renderer);
  switch (state) {
    case State::Choose:
      renderChoose();
      break;
    case State::ApStarting:
      drawHeader(renderer, 28, tr(STR_WREN_TAB_SEND), tr(STR_WREN_SEND_HOTSPOT));
      drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, 20, 156, tr(STR_WREN_SEND_ONE_MOMENT),
                  renderer.getScreenWidth() - 40, true, -1);
      drawTextWrapped(renderer, WREN_GOLOS_18_FONT_ID, 20, 210, tr(STR_WREN_SEND_STARTING_TEXT),
                      renderer.getScreenWidth() - 40, 3, 25);
      break;
    case State::Running:
      renderRunning();
      break;
    case State::Failed:
      renderFailed();
      break;
    case State::JoinPick:
      break;
  }
  wren::setup::unclip(renderer);
  footer(state == State::Choose || state == State::Failed);
  renderer.displayBuffer(cleanRefresh ? HalDisplay::HALF_REFRESH : HalDisplay::FAST_REFRESH);
  cleanRefresh = false;
}
