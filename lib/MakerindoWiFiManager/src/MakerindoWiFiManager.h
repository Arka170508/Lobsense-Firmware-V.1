#pragma once

#include <Arduino.h>
#include <Preferences.h>
#include <WebServer.h>

class MakerindoWiFiManager {
 public:
  explicit MakerindoWiFiManager(uint16_t serverPort = 80);

  bool begin(const char* apSsid, const char* apPassword,
             const char* preferencesNamespace = "wifi-config",
             uint32_t connectionTimeoutMs = 20000);
  void loop();

  bool isConnected() const;
  String localIP() const;
  String accessPointIP() const;

 private:
  void registerRoutes();
  bool startPortal();
  void stopPortal();
  void handleRoot();
  void handleNetworkScan();
  void handleConnect();
  void sendJsonError(int statusCode, const String& message);
  bool connectSavedNetwork();
  static String jsonEscape(const String& value);

  WebServer server_;
  Preferences preferences_;
  String apSsid_;
  String apPassword_;
  uint32_t connectionTimeoutMs_ = 20000;
  uint32_t lastAccessPointAttemptAt_ = 0;
  bool preferencesReady_ = false;
  bool portalRunning_ = false;
  bool started_ = false;
};
