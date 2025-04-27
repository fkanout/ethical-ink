#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <functional>
#include <vector>

#define WIFI_MAX_RETRIES 3

struct ScanResult {
  String ssid;
  int rssi;
  bool secured;
};

class WiFiManager {
public:
  using ConnectionCallback = std::function<void(bool success)>;
  using ScanCallback = std::function<void(std::vector<ScanResult>)>;
  using WifiConnectedCallback = std::function<void()>;
  using WifiFailedToConnectCallback = std::function<void()>;
  static WiFiManager &getInstance();
  void setScanResultCallback(ScanCallback cb);
  void onWifiConnectedCallback(WifiConnectedCallback cb);
  void onWifiFailedToConnectCallback(WifiFailedToConnectCallback cb);

  void asyncScanNetworks();
  void asyncConnect(const char *ssid, const char *password);
  void asyncConnectWithSavedCredentials();

private:
  WiFiManager();
  WiFiManager(const WiFiManager &) = delete;
  WiFiManager &operator=(const WiFiManager &) = delete;
  TaskHandle_t connectTaskHandle = nullptr;
  struct ConnectParams {
    const char *ssid;
    const char *password;
  };
  struct ScanParams {
    ScanCallback callback;
  };
  ScanCallback scanResultCallback = nullptr;
  WifiConnectedCallback onWifiConnected = nullptr;
  WifiFailedToConnectCallback onWifiFailedToConnect = nullptr;

  static void scanTask(void *parameter);
  static void connectTask(void *parameter);
};

#endif // WIFI_MANAGER_H