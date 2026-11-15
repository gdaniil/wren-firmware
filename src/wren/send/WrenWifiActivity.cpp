#include "WrenWifiActivity.h"

#include <HalClock.h>
#include <I18n.h>
#include <Memory.h>
#include <TrustedTime.h>
#include <WiFi.h>
#include <esp_mac.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "CrossPointSettings.h"
#include "WifiCredentialStore.h"
#include "components/HeaderBackTapTarget.h"
#include "util/PluginEvents.h"
#include "wren/setup/SetupUi.h"
#include "wren/ui/WrenFonts.h"
#include "wren/ui/WrenKeyboardActivity.h"

namespace fui = freeink::ui;
using wren::wifi::Failure;
using wren::wifi::Row;
namespace {
constexpr unsigned long CONNECTION_TIMEOUT_MS = 15000;
constexpr unsigned long AUTO_TIMEOUT_MS = 7000;
constexpr unsigned long SELECT_HOLD_MS = 600;
constexpr fui::ActionId ACTION_ROW = 1;
}  // namespace

void WrenWifiActivity::onEnter() {
  Activity::onEnter();
  {
    RenderLock lock(*this);
    WIFI_STORE.loadFromFile();
  }
  touchRouter.reset();
  touchRouter.overrideValue = -1;
  interactionsReady = false;
  if (allowAutoConnect && WIFI_STORE.getCredentialCount()) {
    const auto last = WIFI_STORE.getLastConnectedSsid();
    if (!last.empty()) {
      const auto credential = WIFI_STORE.findCredential(last);
      if (credential && tryCredential(*credential)) return;
    }
    startScan(true);
    return;
  }
  startScan();
}

void WrenWifiActivity::onExit() {
  WiFi.scanDelete();
  HeaderBackTapTarget::clear();
  Activity::onExit();
}

void WrenWifiActivity::setState(State next) {
  if (state == next) return;
  RenderLock lock(*this);
  int x = 0, y = 0;
  touchTransitionGuard = mappedInput.isScreenTouchHeld(x, y);
  touchRouter.reset();
  interactionsReady = false;
  state = next;
}

void WrenWifiActivity::startScan(bool automatic) {
  autoConnecting = automatic;
  manualListRequested = false;
  setState(State::Scanning);
  selected = scroll = 0;
  {
    RenderLock lock(*this);
    networks.clear();
  }
  requestUpdate();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  WiFi.scanNetworks(true);
}

void WrenWifiActivity::processScan() {
  const int16_t count = WiFi.scanComplete();
  if (count == WIFI_SCAN_RUNNING) return;
  std::vector<Network> foundNetworks;
  {
    RenderLock lock(*this);
    foundNetworks.swap(networks);
  }
  foundNetworks.clear();
  if (count >= 0) {
    foundNetworks.reserve(count);
    for (int i = 0; i < count; ++i) {
      char ssid[33];
      strlcpy(ssid, WiFi.SSID(i).c_str(), sizeof(ssid));
      if (!ssid[0]) continue;
      const int32_t rssi = WiFi.RSSI(i);
      auto found = std::find_if(foundNetworks.begin(), foundNetworks.end(),
                                [&ssid](const Network& n) { return n.ssid == ssid; });
      if (found == foundNetworks.end()) {
        foundNetworks.push_back({ssid, rssi, WiFi.encryptionType(i) != WIFI_AUTH_OPEN, false, false});
        foundNetworks.back().saved = WIFI_STORE.hasSavedCredential(foundNetworks.back().ssid);
      } else if (rssi > found->rssi) {
        found->rssi = rssi;
        found->encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      }
    }
    std::sort(foundNetworks.begin(), foundNetworks.end(),
              [](const Network& a, const Network& b) { return a.saved != b.saved ? a.saved : a.rssi > b.rssi; });
  }
  {
    RenderLock lock(*this);
    networks.swap(foundNetworks);
  }
  WiFi.scanDelete();
  if (autoConnecting && !manualListRequested && tryNextSaved()) return;
  autoConnecting = false;
  showList();
}

bool WrenWifiActivity::tryCredential(const WifiCredential& credential) {
  if (networks.empty()) strlcpy(firstAttemptedSsid, credential.ssid.c_str(), sizeof(firstAttemptedSsid));
  for (auto& network : networks)
    if (network.ssid == credential.ssid) network.attempted = true;
  {
    RenderLock lock(*this);
    selectedSSID = credential.ssid;
  }
  enteredPassword = credential.password;
  requiresPassword = !credential.password.empty();
  usedSavedPassword = true;
  autoConnecting = true;
  manualListRequested = false;
  connect();
  return true;
}

bool WrenWifiActivity::tryNextSaved() {
  for (const auto& network : networks) {
    if (!network.saved || network.attempted || network.ssid == firstAttemptedSsid) continue;
    const auto credential = WIFI_STORE.findCredential(network.ssid);
    if (credential && tryCredential(*credential)) return true;
  }
  return false;
}

void WrenWifiActivity::autoFailure() {
  WiFi.disconnect();
  if (networks.empty()) {
    startScan(true);
    return;
  }
  if (tryNextSaved()) return;
  autoConnecting = false;
  showList();
}

void WrenWifiActivity::showManualList() {
  WiFi.disconnect();
  autoConnecting = false;
  if (networks.empty()) {
    startScan();
    manualListRequested = true;
    return;
  }
  showList();
  manualListRequested = true;
}

void WrenWifiActivity::showList() {
  setState(State::NetworkList);
  selected = scroll = 0;
  requestUpdate();
}

void WrenWifiActivity::selectNetwork(size_t index) {
  if (index >= networks.size()) return;
  const auto& network = networks[index];
  {
    RenderLock lock(*this);
    selectedSSID = network.ssid;
  }
  requiresPassword = network.encrypted;
  enteredPassword.clear();
  usedSavedPassword = false;
  autoConnecting = false;
  const auto credential = WIFI_STORE.findCredential(selectedSSID);
  if (credential && !credential->password.empty()) {
    enteredPassword = credential->password;
    usedSavedPassword = true;
    connect();
  } else if (requiresPassword) {
    promptPassword();
  } else {
    connect();
  }
}

void WrenWifiActivity::promptHidden() {
  {
    RenderLock lock(*this);
    selectedSSID.clear();
  }
  enteredPassword.clear();
  requiresPassword = true;
  usedSavedPassword = autoConnecting = false;
  auto child =
      makeUniqueNoThrow<WrenKeyboardActivity>(renderer, mappedInput, tr(STR_WREN_SEND_WIFI), tr(STR_WREN_WIFI_HIDDEN),
                                              tr(STR_WREN_WIFI_NETWORK_NAME), "", 32, InputType::Text);
  if (!child) {
    LOG_ERR("WIFI", "OOM: hidden network keyboard");
    showList();
    return;
  }
  setState(State::HiddenEntry);
  startActivityForResult(std::move(child), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      showList();
      return;
    }
    {
      RenderLock lock(*this);
      selectedSSID = std::get<KeyboardResult>(result.data).text;
    }
    if (selectedSSID.empty()) showList();
  });
}

void WrenWifiActivity::promptPassword() {
  auto child = makeUniqueNoThrow<WrenKeyboardActivity>(renderer, mappedInput, tr(STR_WREN_SEND_WIFI), selectedSSID,
                                                       tr(STR_WREN_SEND_PASSWORD), "", 64, InputType::Password);
  if (!child) {
    LOG_ERR("WIFI", "OOM: password keyboard");
    showList();
    return;
  }
  enteredPassword.clear();
  usedSavedPassword = false;
  setState(State::PasswordEntry);
  startActivityForResult(std::move(child), [this](const ActivityResult& result) {
    if (result.isCancelled) {
      showList();
      return;
    }
    enteredPassword = std::get<KeyboardResult>(result.data).text;
  });
}

void WrenWifiActivity::connect() {
  setState(autoConnecting ? State::AutoConnecting : State::Connecting);
  connectionStart = millis();
  connectedIP.clear();
  requestUpdate();
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
  uint8_t mac[6]{};
  const auto macResult = esp_read_mac(mac, ESP_MAC_WIFI_STA);
  if (macResult == ESP_OK) {
    char hostname[sizeof("CrossPoint-Reader-") + 12];
    snprintf(hostname, sizeof(hostname), "CrossPoint-Reader-%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3],
             mac[4], mac[5]);
    WiFi.setHostname(hostname);
  } else {
    LOG_ERR("WIFI", "Failed to read station MAC for hostname (err=%d)", static_cast<int>(macResult));
  }
  if (requiresPassword && !enteredPassword.empty())
    WiFi.begin(selectedSSID.c_str(), enteredPassword.c_str());
  else
    WiFi.begin(selectedSSID.c_str());
}

void WrenWifiActivity::pollConnection() {
  const wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    const IPAddress ip = WiFi.localIP();
    char ipText[16];
    snprintf(ipText, sizeof(ipText), "%d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    {
      RenderLock lock(*this);
      connectedIP = ipText;
      autoConnecting = false;
    }
    if (halClock.isAvailable() && !SETTINGS.clockHasBeenSynced && halClock.syncFromNTP()) {
      SETTINGS.clockHasBeenSynced = 1;
      SETTINGS.saveToFile();
    }
    trustedtime::startSync();
    WIFI_STORE.setLastConnectedSsid(selectedSSID);
    {
      RenderLock lock(*this);
      drainingEvents = true;
    }
    pluginevents::drain(&renderer);
    {
      RenderLock lock(*this);
      drainingEvents = false;
    }
    if (!usedSavedPassword && !enteredPassword.empty()) {
      setState(State::Save);
      selected = scroll = 0;
      requestUpdate();
    } else {
      complete(true);
    }
    return;
  }
  const bool timedOut = millis() - connectionStart > (autoConnecting ? AUTO_TIMEOUT_MS : CONNECTION_TIMEOUT_MS);
  if (status != WL_CONNECT_FAILED && status != WL_NO_SSID_AVAIL && !timedOut) return;
  if (timedOut) WiFi.disconnect();
  if (autoConnecting) {
    autoFailure();
    return;
  }
  failure = wren::wifi::failureCause(status == WL_NO_SSID_AVAIL,
                                     timedOut && status != WL_NO_SSID_AVAIL && status != WL_CONNECT_FAILED);
  setState(State::Failed);
  selected = scroll = 0;
  requestUpdate();
}

void WrenWifiActivity::complete(bool connected) {
  ActivityResult result;
  result.isCancelled = !connected;
  if (connected) result.data = WifiResult{true, selectedSSID, connectedIP};
  setResult(std::move(result));
  finish();
}

void WrenWifiActivity::activateRow(size_t index, bool longPress) {
  if (state == State::NetworkList) {
    const auto kind = wren::wifi::row(index, networks.size());
    if (longPress) {
      if (kind == Row::Network && networks[index].saved) {
        {
          RenderLock lock(*this);
          selectedSSID = networks[index].ssid;
        }
        setState(State::Forget);
        selected = scroll = 0;
        requestUpdate();
      }
      return;
    }
    switch (kind) {
      case Row::Network:
        selectNetwork(index);
        return;
      case Row::ScanAgain:
        startScan();
        return;
      case Row::HiddenNetwork:
        promptHidden();
        return;
    }
  }
  if (longPress || index > 1) return;
  switch (state) {
    case State::Save:
      if (index == 0) WIFI_STORE.addCredential(selectedSSID, enteredPassword);
      complete(true);
      return;
    case State::Forget:
      if (index == 1) {
        WIFI_STORE.removeCredential(selectedSSID);
        for (auto& network : networks)
          if (network.ssid == selectedSSID) network.saved = false;
      }
      showList();
      return;
    case State::Failed:
      if (index == 1) {
        showList();
        return;
      }
      autoConnecting = false;
      if (requiresPassword)
        promptPassword();
      else
        connect();
      return;
    case State::Scanning:
    case State::AutoConnecting:
    case State::Connecting:
    case State::NetworkList:
    case State::HiddenEntry:
    case State::PasswordEntry:
      return;
  }
}

void WrenWifiActivity::loop() {
  using B = MappedInputManager::Button;
  if (state == State::Scanning) {
    if (mappedInput.wasPressed(B::Back)) {
      WiFi.scanDelete();
      complete(false);
      return;
    }
    if (autoConnecting && mappedInput.wasPressed(B::Confirm)) {
      autoConnecting = false;
      manualListRequested = true;
      requestUpdate();
    }
    processScan();
    return;
  }
  if (state == State::AutoConnecting || state == State::Connecting) {
    if (mappedInput.wasPressed(B::Back)) {
      WiFi.disconnect();
      complete(false);
      return;
    }
    if (state == State::AutoConnecting && mappedInput.wasPressed(B::Confirm)) {
      showManualList();
      return;
    }
    pollConnection();
    return;
  }
  if (state == State::HiddenEntry) {
    const auto credential = WIFI_STORE.findCredential(selectedSSID);
    if (credential && !credential->password.empty()) {
      enteredPassword = credential->password;
      usedSavedPassword = true;
      connect();
    } else {
      promptPassword();
    }
    return;
  }
  if (state == State::PasswordEntry) {
    connect();
    return;
  }

  if (manualListRequested) {
    if (!mappedInput.isPressed(B::Confirm)) manualListRequested = false;
    return;
  }
  if (mappedInput.wasPressed(B::Back)) {
    if (state == State::NetworkList)
      complete(false);
    else
      showList();
    return;
  }
  int touchX = 0, touchY = 0;
  if (touchTransitionGuard) {
    if (!mappedInput.isScreenTouchHeld(touchX, touchY)) {
      touchTransitionGuard = false;
      return;
    }
  } else if (state == State::NetworkList && !networks.empty()) {
    const auto swipe = mappedInput.wasSwipe();
    if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
      const size_t count = wren::wifi::rowCount(networks.size());
      const size_t visible = std::max(1, (renderer.getScreenHeight() - 40 - 132) / 72);
      const size_t limit = count > visible ? count - visible : 0;
      if (swipe == MappedInputManager::SwipeDir::Up)
        scroll = std::min(limit, scroll + visible);
      else
        scroll = scroll > visible ? scroll - visible : 0;
      interactionsReady = false;
      touchRouter.reset();
      requestUpdate();
      return;
    }
  }
  if (!touchTransitionGuard && interactionsReady) {
    int tx = 0, ty = 0, rx = 0, ry = 0, hx = 0, hy = 0;
    const bool down = mappedInput.wasScreenTouchDown(tx, ty);
    const bool tapped = mappedInput.wasScreenTapped(rx, ry);
    const bool held = mappedInput.isScreenTouchHeld(hx, hy);
    const auto touch = touchRouter.update(interactions, down, tx, ty, tapped, rx, ry, held, millis());
    if (touch.event) {
      selected = static_cast<size_t>(touch.event.value);
      activateRow(selected, touch.event.longPress);
      return;
    }
    if (touch.activeChanged) requestUpdate();
    if (down || tapped) return;
  }
  const size_t count = state == State::NetworkList ? wren::wifi::rowCount(networks.size()) : 2;
  if (state == State::NetworkList && selected < networks.size() && networks[selected].saved &&
      mappedInput.wasLongPressed(B::Confirm, SELECT_HOLD_MS)) {
    activateRow(selected, true);
    return;
  }
  if (mappedInput.wasReleased(B::Confirm)) {
    activateRow(selected, false);
    return;
  }
  const auto follow = [this, count] {
    const size_t visible = state == State::NetworkList && !networks.empty()
                               ? std::max(1, (renderer.getScreenHeight() - 40 - 132) / 72)
                               : 1;
    scroll = wren::wifi::scrollStart(scroll, selected, visible, count);
    requestUpdate();
  };
  // Same mapping as the Wren send menu: front ▲▼ (Left/Right) move; side buttons move too (no tabs here).
  navigator.onPressAndContinuous({B::Left, B::Up}, [this, count, &follow] {
    selected = wren::wifi::move(selected, count, -1);
    follow();
  });
  navigator.onPressAndContinuous({B::Right, B::Down}, [this, count, &follow] {
    selected = wren::wifi::move(selected, count, 1);
    follow();
  });
}

void WrenWifiActivity::addHit(int y, int height, size_t value) {
  const int bottom = renderer.getScreenHeight() - 40;
  if (y >= bottom || y + height <= 132) return;
  const int top = std::max(y, 132);
  const int end = std::min(y + height, bottom);
  interactions.addInteraction(
      {{0, static_cast<int16_t>(top), static_cast<int16_t>(renderer.getScreenWidth()), static_cast<int16_t>(end - top)},
       ACTION_ROW,
       static_cast<int16_t>(value),
       static_cast<uint16_t>(fui::InputTouch | fui::InputLongPress),
       fui::StateNormal});
}

void WrenWifiActivity::drawList() {
  using namespace wren::ui;
  const int width = renderer.getScreenWidth();
  const int bottom = renderer.getScreenHeight() - 40;
  constexpr int top = 132, height = 72;
  const size_t count = wren::wifi::rowCount(networks.size());
  const size_t visible = std::max(1, (bottom - top) / height);
  for (size_t i = scroll; i < count && i < scroll + visible; ++i) {
    const int y = top + static_cast<int>(i - scroll) * height;
    const bool ink = selected != i;
    renderer.fillRect(0, y, width, height - 2, !ink);
    const auto kind = wren::wifi::row(i, networks.size());
    if (kind == wren::wifi::Row::Network) {
      const auto& network = networks[i];
      drawTextFit(renderer, WREN_GOLOS_24_FONT_ID, 20, y + 8, network.ssid.c_str(), width - 100, ink);
      const char* subtitle = nullptr;
      switch (wren::wifi::subtitle(network.saved, network.encrypted)) {
        case wren::wifi::Subtitle::Saved:
          subtitle = tr(STR_WREN_WIFI_SAVED);
          break;
        case wren::wifi::Subtitle::Secured:
          subtitle = tr(STR_WREN_WIFI_SECURED);
          break;
        case wren::wifi::Subtitle::Open:
          subtitle = tr(STR_WREN_WIFI_OPEN);
          break;
      }
      drawTextFit(renderer, WREN_GOLOS_18_FONT_ID, 20, y + 39, subtitle, width - 100, ink);
      constexpr int heights[] = {6, 10, 14, 18};
      const int bars = wren::wifi::signalBars(network.rssi);
      for (int bar = 0; bar < 4; ++bar) {
        const int x = width - 51 + bar * 8, by = y + 46 - heights[bar];
        if (bar < bars)
          renderer.fillRect(x, by, 5, heights[bar], ink);
        else
          renderer.drawRect(x, by, 5, heights[bar], 1, ink);
      }
    } else {
      const char* label = kind == wren::wifi::Row::ScanAgain ? tr(STR_WREN_WIFI_SCAN_AGAIN) : tr(STR_WREN_WIFI_HIDDEN);
      drawTextFit(renderer, WREN_GOLOS_24_FONT_ID, 20, y + 22, label, width - 90, ink);
      if (kind == wren::wifi::Row::ScanAgain)
        drawIcon(renderer, icons::Refresh, width - 52, y + 20, 1, ink);
      else
        drawIcon(renderer, icons::ChevronRight, width - 26, y + 30, 2, ink);
    }
    renderer.fillRect(0, y + height - 2, width, 2, true);
    addHit(y, height, i);
  }
}

void WrenWifiActivity::drawMessage(const char* headline, const char* message, const char* first, const char* second) {
  using namespace wren::ui;
  const int width = renderer.getScreenWidth();
  const int bottom = renderer.getScreenHeight() - 40;
  drawTextFit(renderer, WREN_GOLOS_40_FONT_ID, 20, 156, headline, width - 40, true, -1);
  const int maxLines = first ? std::max(1, std::min(3, (bottom - 212 - 32 - 88) / 25)) : 3;
  const int lines = drawTextWrapped(renderer, WREN_GOLOS_18_FONT_ID, 20, 212, message, width - 40, maxLines, 25);
  if (!first) return;
  const int top = 212 + lines * 25 + 32;
  renderer.fillRect(0, top, width, 3, true);
  const size_t visible = std::max(1, (bottom - top - 3) / 88);
  scroll = wren::wifi::scrollStart(scroll, selected, visible, 2);
  const char* labels[] = {first, second};
  for (size_t i = scroll; i < 2 && i < scroll + visible; ++i) {
    const int y = top + 3 + static_cast<int>(i - scroll) * 88;
    const bool ink = selected != i;
    renderer.fillRect(0, y, width, 85, !ink);
    drawTextFit(renderer, WREN_GOLOS_32_FONT_ID, 20, y + 24, labels[i], width - 62, ink, -1);
    drawIcon(renderer, icons::ChevronRight, width - 26, y + 35, 2, ink);
    renderer.fillRect(0, y + 85, width, 3, true);
    addHit(y, 88, i);
  }
}

void WrenWifiActivity::render(RenderLock&&) {
  if (drainingEvents) return;
  using namespace wren::ui;
  if (state == State::HiddenEntry || state == State::PasswordEntry) return;
  wren::setup::status(renderer);
  wren::setup::clipContent(renderer);
  interactions.beginPublishCycle();
  interactions.clear();
  const bool networkHeader = state == State::Scanning || state == State::NetworkList;
  HeaderBackTapTarget::set(20, 40, 24, 24);
  drawHeader(renderer, 28, networkHeader ? tr(STR_WREN_TAB_SEND) : tr(STR_WREN_SEND_WIFI),
             networkHeader ? tr(STR_WREN_SEND_WIFI) : selectedSSID.c_str());
  char body[240];
  switch (state) {
    case State::Scanning:
      drawMessage(tr(STR_WREN_WIFI_SEARCHING), tr(STR_WREN_WIFI_SEARCHING_BODY));
      break;
    case State::AutoConnecting:
    case State::Connecting:
      snprintf(body, sizeof(body),
               state == State::AutoConnecting ? tr(STR_WREN_WIFI_AUTO_JOINING) : tr(STR_WREN_WIFI_JOINING),
               selectedSSID.c_str());
      drawMessage(tr(STR_WREN_WIFI_CONNECTING), body);
      break;
    case State::NetworkList:
      if (networks.empty())
        drawMessage(tr(STR_WREN_WIFI_NO_NETWORKS), tr(STR_WREN_WIFI_NO_NETWORKS_BODY), tr(STR_WREN_WIFI_SCAN_AGAIN),
                    tr(STR_WREN_WIFI_HIDDEN));
      else
        drawList();
      break;
    case State::Save:
      drawMessage(tr(STR_WREN_WIFI_CONNECTED), tr(STR_WREN_WIFI_SAVE_BODY), tr(STR_WREN_WIFI_SAVE),
                  tr(STR_WREN_WIFI_DONT_SAVE));
      break;
    case State::Failed:
      drawMessage(tr(STR_WREN_WIFI_FAILED),
                  failure == Failure::NoSsid    ? tr(STR_WREN_WIFI_FAILED_NO_SSID)
                  : failure == Failure::Timeout ? tr(STR_WREN_WIFI_FAILED_TIMEOUT)
                                                : tr(STR_WREN_WIFI_FAILED_PASSWORD),
                  tr(STR_WREN_SEND_TRY_AGAIN), tr(STR_WREN_WIFI_OTHER_NETWORK));
      break;
    case State::Forget:
      snprintf(body, sizeof(body), tr(STR_WREN_WIFI_FORGET_BODY), selectedSSID.c_str());
      drawMessage(tr(STR_WREN_WIFI_FORGET_TITLE), body, tr(STR_WREN_WIFI_KEEP), tr(STR_WREN_WIFI_FORGET));
      break;
    case State::HiddenEntry:
    case State::PasswordEntry:
      break;
  }
  interactions.publish();
  interactionsReady = true;
  wren::setup::unclip(renderer);
  if (state == State::Scanning || state == State::Connecting || state == State::AutoConnecting) {
    const Tab tabs[4] = {{TabKind::Text, tr(STR_WREN_TAB_BACK)},
                         state == State::AutoConnecting ? Tab{TabKind::Text, tr(STR_WREN_TAB_SELECT)} : Tab{},
                         {},
                         {}};
    drawButtonTabs(renderer, tabs, false);
  } else {
    wren::setup::footer(renderer);
  }
  renderer.displayBuffer();
}
