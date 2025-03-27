#include "BLEManager.h"
#include "BLEDevice.h"

#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"
#define CHARACTERISTIC_UUID "abcdefab-cdef-1234-5678-abcdefabcdef"

BLECharacteristic *pCharacteristic = nullptr;
String bleReceivedJson = "";  // Buffer for received JSON
bool newDataAvailable = false; // Flag to indicate new data received

class DataReceivedCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        Serial.println("🔵 onWrite() triggered!");

        std::string rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            bleReceivedJson = String(rxValue.c_str());  // Store JSON for main
            newDataAvailable = true;  // Set flag to notify main loop
            Serial.println("📩 Received JSON stored: " + bleReceivedJson);
        } else {
            Serial.println("⚠️ No data received!");
        }
    }
};

void setupBLE() {
    Serial.println("🔵 Initializing BLE...");
    BLEDevice::init("ESP32_BLE");

    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );

    pCharacteristic->setCallbacks(new DataReceivedCallbacks());
    pService->start();
    BLEDevice::startAdvertising();
    
    Serial.println("✅ BLE ready to receive JSON!");
}

bool isNewBLEDataAvailable() {
    return newDataAvailable;
}

String getReceivedBLEData() {
    newDataAvailable = false;  // Clear flag after reading
    return bleReceivedJson;
}