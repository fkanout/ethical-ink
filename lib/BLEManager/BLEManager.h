#pragma once

#include <Arduino.h>
#include <BLECharacteristic.h>
#include <BLEServer.h>
#include <functional>

class BLEManager {
public:
  static BLEManager &getInstance();
  using NotificationToggleCallback = std::function<void(void)>;
  using JsonReceivedCallback = std::function<void(const String &json)>;
  void setupBLE();
  void stopAdvertising();
  void startAdvertising();
  void restartBLE();
  void sendBLEData(const String &json);
  bool isNewBLEDataAvailable();
  String getReceivedBLEData();

  void setNotificationEnabledCallback(NotificationToggleCallback cb);
  void setJsonReceivedCallback(JsonReceivedCallback cb);

  static void invokeNotificationCallback();
  static void invokeJsonReceivedCallback(const String &json);

private:
  BLEManager() = default;
  BLEManager(const BLEManager &) = delete;
  BLEManager &operator=(const BLEManager &) = delete;
  static NotificationToggleCallback notificationCallback;
  static JsonReceivedCallback jsonCallback;
};
