#include "BLEManager.h"
#include "BLEDevice.h"
#include "RTCManager.h"
#include "SPIFFSHelper.h"
#include "WiFiManager.h"
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <WiFi.h>
#include <esp_timer.h>

#define SERVICE_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d3f"
#define CHARACTERISTIC_PAYLOAD_MOSQUE_UUID                                     \
  "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d40"
#define CHARACTERISTIC_TIME_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d41"
#define CHARACTERISTIC_WIFI_SCAN_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d42"
BLEManager::NotificationToggleCallback BLEManager::notificationCallback =
    nullptr;

BLEManager::JsonReceivedCallback BLEManager::jsonCallback = nullptr;

BLECharacteristic *pJsonCharacteristic = nullptr;
BLECharacteristic *pTimeCharacteristic = nullptr;
BLECharacteristic *pWifiScanCharacteristic = nullptr;
BLEServer *pServer = nullptr;

String bleReceivedJson = "";
bool newDataAvailable = false;
bool isConnected = false;

// ===================== 🔁 Singleton =====================
BLEManager &BLEManager::getInstance() {
  static BLEManager instance;
  return instance;
}
void BLEManager::setNotificationEnabledCallback(NotificationToggleCallback cb) {
  notificationCallback = cb;
}

void BLEManager::invokeNotificationCallback() {
  if (notificationCallback) {
    notificationCallback();
  }
}
void BLEManager::setJsonReceivedCallback(JsonReceivedCallback cb) {
  jsonCallback = cb;
}

void BLEManager::invokeJsonReceivedCallback(const String &json) {
  if (jsonCallback) {
    jsonCallback(json);
  }
}
// ===================== 🟡 BLE Callbacks =====================
class NotifyStatusDescriptorCallback : public BLEDescriptorCallbacks {
  void onWrite(BLEDescriptor *pDesc) override {
    uint8_t *value = pDesc->getValue();
    if (value[0] == 0x01) {
      Serial.println("📲 Notifications enabled!");
      BLEManager::invokeNotificationCallback();
    }
  }
};

class JsonReceivedCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    std::string value = pChar->getValue();
    if (!value.empty()) {
      BLEManager::invokeJsonReceivedCallback(String(value.c_str()));
      Serial.printf("📦 Free heap: %u bytes\n", ESP.getFreeHeap());
    }
  }
};

class TimeReceivedCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pChar) override {
    std::string value = pChar->getValue();
    String iso8601 = String(value.c_str());
    Serial.println("🕒 Received time: " + iso8601);
  }
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    Serial.println("📱 Phone connected");
    isConnected = true;
  }

  void onDisconnect(BLEServer *pServer) override {
    Serial.println("📴 Phone disconnected");
    isConnected = false;
    BLEDevice::startAdvertising();
  }
};

// ===================== 🟢 Public BLE Methods =====================
void BLEManager::setupBLE() {
  Serial.println("🔵 Initializing BLE...");
  esp_log_level_set("BLEDevice", ESP_LOG_WARN);
  BLEDevice::init("Kwaizar");

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pJsonCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_PAYLOAD_MOSQUE_UUID, BLECharacteristic::PROPERTY_WRITE);
  pJsonCharacteristic->setCallbacks(new JsonReceivedCallbacks());

  pTimeCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_TIME_UUID, BLECharacteristic::PROPERTY_WRITE);
  pTimeCharacteristic->setCallbacks(new TimeReceivedCallbacks());

  pWifiScanCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_WIFI_SCAN_UUID, BLECharacteristic::PROPERTY_NOTIFY);

  BLE2902 *desc = new BLE2902();
  desc->setCallbacks(new NotifyStatusDescriptorCallback());
  pWifiScanCharacteristic->addDescriptor(desc);

  pService->start();
  BLEDevice::startAdvertising();
  Serial.println("📡 BLE is advertising...");
}

bool BLEManager::isNewBLEDataAvailable() { return newDataAvailable; }

void BLEManager::stopAdvertising() {
  BLEDevice::stopAdvertising();
  Serial.println("🔕 BLE advertising stopped");
}

void BLEManager::startAdvertising() {
  BLEDevice::startAdvertising();
  Serial.println("🔔 BLE advertising restarted");
}

String BLEManager::getReceivedBLEData() {
  newDataAvailable = false;
  return bleReceivedJson;
}

void BLEManager::restartBLE() {
  if (!isConnected) {
    Serial.println("🔁 Restarting BLE...");
    BLEDevice::startAdvertising();
  }
}

void BLEManager::sendBLEData(const String &json) {
  if (isConnected && pWifiScanCharacteristic) {
    pWifiScanCharacteristic->setValue(json.c_str());
    pWifiScanCharacteristic->notify();
    Serial.println("📡 Sent Wi-Fi list: " + json);
  } else {
    Serial.println("⚠️ Not connected or characteristic missing");
  }
}
