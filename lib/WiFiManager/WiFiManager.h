#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <functional>
#include <vector>

#define WIFI_MAX_RETRIES 5

struct ScanResult {
  String ssid;
  int rssi;
  bool secured;
};

class WiFiManager {
public:
  using ConnectionCallback = std::function<void(bool success)>;
  using ScanCallback = std::function<void(std::vector<ScanResult>)>;
  static WiFiManager &getInstance();

  void asyncScanNetworks(ScanCallback callback = nullptr);
  void asyncConnect(const char *ssid, const char *password,
                    ConnectionCallback callback = nullptr);
  void asyncConnectWithSavedCredentials(ConnectionCallback callback = nullptr);

private:
  WiFiManager();
  WiFiManager(const WiFiManager &) = delete;
  WiFiManager &operator=(const WiFiManager &) = delete;
  TaskHandle_t connectTaskHandle = nullptr;
  struct ConnectParams {
    const char *ssid;
    const char *password;
    ConnectionCallback callback;
  };
  struct ScanParams {
    ScanCallback callback;
  };
  static void scanTask(void *parameter);
  static void connectTask(void *parameter);
};

#endif // WIFI_MANAGER_H