#pragma once

#include <FreeInkUI.h>

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "WifiState.h"
#include "activities/Activity.h"
#include "util/ButtonNavigator.h"

struct WifiCredential;

// Mirrors WifiSelectionActivity @ 622e5de9; keep in sync on upstream merges.
class WrenWifiActivity final : public Activity {
 public:
  explicit WrenWifiActivity(GfxRenderer& renderer, MappedInputManager& input, bool autoConnect = true)
      : Activity("WrenWifi", renderer, input), allowAutoConnect(autoConnect) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool preventAutoSleep() override { return true; }

 private:
  enum class State {
    Scanning,
    AutoConnecting,
    Connecting,
    NetworkList,
    HiddenEntry,
    PasswordEntry,
    Save,
    Failed,
    Forget
  };
  struct Network {
    std::string ssid;
    int32_t rssi = 0;
    bool encrypted = false;
    bool saved = false;
    bool attempted = false;
  };
  const bool allowAutoConnect;
  State state = State::Scanning;
  std::vector<Network> networks;
  char firstAttemptedSsid[33]{};
  bool touchTransitionGuard = false;
  bool drainingEvents = false;
  size_t selected = 0;
  size_t scroll = 0;
  std::string selectedSSID;
  std::string enteredPassword;
  std::string connectedIP;
  bool requiresPassword = false;
  bool usedSavedPassword = false;
  bool autoConnecting = false;
  bool manualListRequested = false;
  unsigned long connectionStart = 0;
  wren::wifi::Failure failure = wren::wifi::Failure::General;
  ButtonNavigator navigator;
  freeink::ui::InteractionBuffer<16> interactions;
  freeink::ui::TouchHoldRouter touchRouter;
  std::atomic<bool> interactionsReady{false};

  void setState(State next);
  void startScan(bool automatic = false);
  void processScan();
  bool tryCredential(const WifiCredential& credential);
  bool tryNextSaved();
  void autoFailure();
  void showManualList();
  void selectNetwork(size_t index);
  void promptHidden();
  void promptPassword();
  void connect();
  void pollConnection();
  void complete(bool connected);
  void activateRow(size_t row, bool longPress);
  void showList();
  void drawList();
  void drawMessage(const char* headline, const char* message, const char* first = nullptr,
                   const char* second = nullptr);
  void addHit(int y, int height, size_t value);
};
