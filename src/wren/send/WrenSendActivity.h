#pragma once

#include <DNSServer.h>
#include <qrcode.h>

#include "SendState.h"
#include "activities/Activity.h"
#include "network/CrossPointWebServer.h"
#include "util/ButtonNavigator.h"

// Mirrors CrossPointWebServerActivity @ 622e5de9; keep in sync on upstream merges.
class WrenSendActivity final : public Activity {
 public:
  WrenSendActivity(GfxRenderer& renderer, MappedInputManager& input, bool startInJoinNetwork = false)
      : Activity("WrenSend", renderer, input), startInJoinNetwork(startInJoinNetwork) {}
  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;
  bool skipLoopDelay() override { return webServer && webServer->isRunning(); }
  bool preventAutoSleep() override { return webServer && webServer->isRunning(); }

 private:
  enum class Failure { Hotspot, Server, WifiLost };
  void choose(int row);
  void joinNetwork();
  void startAccessPoint();
  void startServer();
  void cleanupNetwork();
  void fail(Failure reason);
  void updateUploads();
  void scheduleUploadRedraw();
  void prepareQr();
  void drawQr(const QRCode& qr, int x, int y) const;
  void renderChoose();
  void renderFailed();
  void renderRunning();
  void renderReceived(int x, int y, int width, int bottom);
  void footer(bool menu);

  wren::send::State state = wren::send::State::Choose;
  Failure failure = Failure::Hotspot;
  bool startInJoinNetwork = false;
  bool apMode = false;
  bool leaveRequested = false;
  bool fullRefresh = true;
  bool startFrameShown = false;
  int selected = 0;
  int scroll = 0;
  ButtonNavigator navigator{250, 500};
  std::unique_ptr<CrossPointWebServer> webServer;
  std::unique_ptr<DNSServer> dns;
  char ip[16] = {};
  char ssid[96] = {};
  // Version 4 needs 137 bytes per code; both codes live for the network session.
  uint8_t joinQrBytes[137] = {};
  uint8_t openQrBytes[137] = {};
  QRCode joinQr{};
  QRCode openQr{};
  wren::send::HotspotSteps hotspot;
  wren::send::ReceivedList received;
  bool wsInProgress = false;
  char wsName[96] = {};
  size_t wsSize = 0;
  unsigned wsPercent = 0;
  wren::send::UploadRedraw uploadRedraw;
  unsigned long lastCompleteAt = 0;
  unsigned long lastStationPoll = 0;
  unsigned long lastUploadPoll = 0;
  unsigned long lastWifiCheck = 0;
  unsigned long firstDisconnectAt = 0;
  unsigned long lastHandleClientTime = 0;
  int consecutiveDisconnects = 0;
  int wifiBars = 0;
  // Only the loop task touches these; the render task never reads them.
  char httpPath[512] = {};
  char httpName[96] = {};
  size_t httpSize = 0;
  bool httpPending = false;
};
