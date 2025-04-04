#include "BLEManager.h"
#include "BLEDevice.h"
#include <RTCManager.h>

#define SERVICE_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d3f"
#define CHARACTERISTIC_PAYLOAD_MOSQUE_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d40"
#define CHARACTERISTIC_TIME_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d41"
BLECharacteristic *pJsonCharacteristic = nullptr;
BLECharacteristic *pTimeCharacteristic = nullptr;

String bleReceivedJson = "";   // Buffer for received JSON
bool newDataAvailable = false; // Flag to indicate new data received

class JsonReceivedCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pJsonCharacteristic) override
    {
        std::string rxValue = pJsonCharacteristic->getValue();
        if (rxValue.length() > 0)
        {
            bleReceivedJson = String(rxValue.c_str());
            newDataAvailable = true;
            Serial.println("📩 Received JSON: " + bleReceivedJson);
        }
        else
        {
            Serial.println("⚠️ No JSON data received!");
        }
    }
};

class TimeReceivedCallbacks : public BLECharacteristicCallbacks
{

    void onWrite(BLECharacteristic *pCharacteristic)
    {
        std::string receivedData = pCharacteristic->getValue();

        // Convert received data to a String
        String iso8601Time = String(receivedData.c_str());
        Serial.printf("Received ISO 8601 time: %s\n", iso8601Time.c_str());
        RTCManager *rtcManager = RTCManager::getInstance(); // Get the singleton instance
        rtcManager->setRtcTimeFromISO8601(iso8601Time);
        rtcManager->printTime();
    }
};
// void updateESP32Time(long timestamp) {
//     struct timeval tv;
//     tv.tv_sec = timestamp;  // Unix timestamp
//     tv.tv_usec = 0;
//     settimeofday(&tv, NULL);  // Set ESP32 RTC
//     Serial.println("✅ ESP32 RTC Updated!");
// }

void setupBLE()
{
    Serial.println("🔵 Initializing BLE...");
    BLEDevice::init("Kwaizar");

    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // 🔹 JSON Characteristic (Handled by JsonReceivedCallbacks)
    pJsonCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_PAYLOAD_MOSQUE_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pJsonCharacteristic->setCallbacks(new JsonReceivedCallbacks());

    // 🔹 Time Characteristic (Handled by TimeReceivedCallbacks)
    pTimeCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_TIME_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pTimeCharacteristic->setCallbacks(new TimeReceivedCallbacks());

    pService->start();
    BLEDevice::startAdvertising();

    Serial.println("✅ BLE ready to receive JSON & Time updates!");
}

bool isNewBLEDataAvailable()
{
    return newDataAvailable;
}

String getReceivedBLEData()
{
    newDataAvailable = false; // Clear flag after reading
    return bleReceivedJson;
}