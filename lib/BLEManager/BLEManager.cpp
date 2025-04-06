

#include "BLEManager.h"
#include "BLEDevice.h"
#include "RTCManager.h"
#include "WiFiManager.h"
#include <esp_timer.h>
#include <WiFi.h>
#include <BLE2902.h>
#include "SPIFFSHelper.h"
#include <ArduinoJson.h>

#define SERVICE_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d3f"
#define CHARACTERISTIC_PAYLOAD_MOSQUE_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d40"
#define CHARACTERISTIC_TIME_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d41"
#define CHARACTERISTIC_WIFI_SCAN_UUID "b8b3f5a4-12d5-4d8f-9b6c-8a7f4c1e2d42"

BLECharacteristic *pJsonCharacteristic = nullptr;
BLECharacteristic *pTimeCharacteristic = nullptr;
BLECharacteristic *pWifiScanCharacteristic = nullptr;
BLEServer *pServer = nullptr;

String bleReceivedJson = "";
bool newDataAvailable = false;
bool isConnected = false;

esp_timer_handle_t bleTimeoutTimer;

// ===================== 🔐 Security =====================
class MySecurityCallbacks : public BLESecurityCallbacks
{
public:
    uint32_t onPassKeyRequest() override
    {
        Serial.println("🔐 Passkey requested");
        return 123456; // Hardcoded passkey for simplicity
    }

    void onPassKeyNotify(uint32_t pass_key) override
    {
        Serial.printf("🔐 Passkey Notify: %06u\n", pass_key);
    }

    bool onConfirmPIN(uint32_t pass_key) override
    {
        Serial.printf("🔐 Confirm PIN: %06u\n", pass_key);
        return true; // Always accept the PIN
    }

    bool onSecurityRequest() override
    {
        return true;
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override
    {
        if (cmpl.success)
        {
            Serial.println("✅ Authentication successful");
        }
        else
        {
            Serial.println("❌ Authentication failed");
        }
    }
};

// ===================== 🟡 BLE Callbacks =====================
class NotifyStatusDescriptorCallback : public BLEDescriptorCallbacks
{
    void onWrite(BLEDescriptor *pDesc) override
    {
        uint8_t *value = pDesc->getValue();
        if (value[0] == 0x01)
        {
            Serial.println("📲 Notifications enabled!");
            xTaskCreatePinnedToCore([](void *param)
                                    {
                delay(500);  // Give WiFi time to settle
                WiFiManager::getInstance()->scanWifiNetworks();
                vTaskDelete(NULL); }, "wifi_scan", 4096, NULL, 1, NULL, 1);
        }
    }
};
class JsonReceivedCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pChar) override
    {
        std::string value = pChar->getValue();
        if (!value.empty())
        {
            Serial.println("📩 Received JSON over BLE");
            WiFiManager::getInstance()->deferAsyncConnect(String(value.c_str()));
            Serial.printf("📦 Free heap: %u bytes\n", ESP.getFreeHeap());
        }
    }
};

class TimeReceivedCallbacks : public BLECharacteristicCallbacks
{
    void onWrite(BLECharacteristic *pChar) override
    {
        std::string value = pChar->getValue();
        String iso8601 = String(value.c_str());
        Serial.println("🕒 Received time: " + iso8601);
        RTCManager::getInstance()->setRtcTimeFromISO8601(iso8601);
        RTCManager::getInstance()->printTime();
    }
};

class MyServerCallbacks : public BLEServerCallbacks
{
    void onConnect(BLEServer *pServer) override
    {
        Serial.println("📱 Phone connected");
        isConnected = true;
        // // 👉 Connect Wi-Fi if not already connected
        // if (WiFi.status() != WL_CONNECTED && !wifiAttempted)
        // {
        //     Serial.println("📶 BLE triggered Wi-Fi attempt...");
        //     WiFiManager::getInstance()->autoConnectFromFile();
        //     wifiAttempted = true;
        //     state = CONNECTING_WIFI; // 👈 update FSM state from BLE!
        //     lastAttempt = millis();  // reset retry timer
        // }
    }

    void onDisconnect(BLEServer *pServer) override
    {
        Serial.println("📴 Phone disconnected");
        isConnected = false;
        BLEDevice::startAdvertising();
        // startBLETimeout(); // Restart timeout
    }
};

// ===================== ⏳ Timer Callback =====================
// void BLEManager::bleTimeoutCallback(void *arg)
// {
//     if (!isConnected)
//     {
//         Serial.println("⏹️ No connection after 2 minutes. Stopping BLE.");
//         BLEDevice::getAdvertising()->stop();
//     }
// }

// ===================== 🟢 Public BLE Methods =====================
void BLEManager::setupBLE()
{
    Serial.println("🔵 Initializing BLE...");
    // Suppress noisy internal logs
    esp_log_level_set("BLEDevice", ESP_LOG_WARN);
    BLEDevice::init("Kwaizar");
    // Remove all bonded devices
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num > 0)
    {
        esp_ble_bond_dev_t *bonded_devices = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
        if (esp_ble_get_bond_device_list(&dev_num, bonded_devices) == ESP_OK)
        {
            for (int i = 0; i < dev_num; i++)
            {
                esp_ble_remove_bond_device(bonded_devices[i].bd_addr);
            }
        }
        free(bonded_devices);
    }
    // Security setup
    BLESecurity *pSecurity = new BLESecurity();
    pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    pSecurity->setCapability(ESP_IO_CAP_OUT); // Passkey entry
    pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());

    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    // JSON characteristic for receiving data from phone
    pJsonCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_PAYLOAD_MOSQUE_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pJsonCharacteristic->setCallbacks(new JsonReceivedCallbacks());

    // Time characteristic for receiving time data from phone
    pTimeCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_TIME_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    pTimeCharacteristic->setCallbacks(new TimeReceivedCallbacks());

    // Wi-Fi scan notify characteristic for sending Wi-Fi scan data
    pWifiScanCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_WIFI_SCAN_UUID,
        BLECharacteristic::PROPERTY_NOTIFY);

    BLE2902 *desc = new BLE2902();
    desc->setCallbacks(new NotifyStatusDescriptorCallback());
    pWifiScanCharacteristic->addDescriptor(desc);

    pService->start();
    BLEDevice::startAdvertising();
    Serial.println("📡 BLE is advertising...");

    // // Set up BLE timeout
    // const esp_timer_create_args_t timer_args = {
    //     .callback = &bleTimeoutCallback,
    //     .name = "ble_timeout"};
    // esp_timer_create(&timer_args, &bleTimeoutTimer);
    // startBLETimeout();
}

// void BLEManager::startBLETimeout()
// {
//     esp_timer_stop(bleTimeoutTimer);
//     esp_timer_start_once(bleTimeoutTimer, 2 * 60 * 1000000); // 2 minutes
// }

bool BLEManager::isNewBLEDataAvailable()
{
    return newDataAvailable;
}

String BLEManager::getReceivedBLEData()
{
    newDataAvailable = false;
    return bleReceivedJson;
}

void BLEManager::restartBLE()
{
    if (!isConnected)
    {
        Serial.println("🔁 Restarting BLE...");
        BLEDevice::startAdvertising();
        // startBLETimeout();
    }
}

void BLEManager::sendBLEData(const String &json)
{
    if (isConnected && pWifiScanCharacteristic)
    {
        pWifiScanCharacteristic->setValue(json.c_str());
        pWifiScanCharacteristic->notify();
        Serial.println("📡 Sent Wi-Fi list: " + json);
    }
    else
    {
        Serial.println("⚠️ Not connected or characteristic missing");
    }
}