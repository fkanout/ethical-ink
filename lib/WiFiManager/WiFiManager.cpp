#include "WiFiManager.h"
#include <BLEManager.h>
#include "SPIFFSHelper.h"
#include <ArduinoJson.h>
#include <RTCManager.h>

// Singleton instance getter
WiFiManager *WiFiManager::getInstance()
{
    static WiFiManager instance;
    return &instance;
}
void WiFiManager::begin(const char *ssid, const char *password, const char *ntpServer, long utcOffset)
{
    Serial.println("📦 WiFiManager.begin() called");

    _ssid = String(ssid);
    _password = String(password);
    _ntpServer = String(ntpServer);
    _utcOffset = utcOffset;

    _timeClient = NTPClient(_udp, _ntpServer.c_str(), _utcOffset);
}

void WiFiManager::deferAsyncConnect(const String &json)
{
    // Pass the String via heap
    String *data = new String(json);
    xTaskCreatePinnedToCore(
        WiFiManager::connectTask,
        "wifi_task",
        12288,
        data,
        1,
        NULL,
        1);
}
void WiFiManager::handleWiFiEvent(WiFiEvent_t event)
{
    switch (event)
    {
    case SYSTEM_EVENT_STA_CONNECTED:
        Serial.println("📶 Wi-Fi connected to AP");
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        Serial.println("📡 Got IP");
        Serial.println(WiFi.localIP());
        WiFiManager::getInstance()->syncTime();
        BLEManager::sendBLEData("{\"wifiStatus\":\"connected\"}");
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Serial.println("❌ Wi-Fi disconnected");
        BLEManager::sendBLEData("{\"wifiStatus\":\"disconnected\"}");
        break;
    default:
        break;
    }
}

void WiFiManager::connectTask(void *param)
{
    String *jsonPtr = static_cast<String *>(param);
    String json = *jsonPtr;
    delete jsonPtr; // Free heap

    WiFiManager::getInstance()->connectAsync(json);
    vTaskDelete(NULL); // Clean up task
}
void WiFiManager::scheduleRetry(uint32_t delayMs)
{
    // Pass delay as pointer
    uint32_t *delayCopy = new uint32_t(delayMs);

    xTaskCreatePinnedToCore(
        WiFiManager::retryTask,
        "wifi_retry",
        4096,
        delayCopy,
        1,
        NULL,
        1);
}

void WiFiManager::retryTask(void *param)
{
    uint32_t delayMs = *(uint32_t *)param;
    delete (uint32_t *)param;

    vTaskDelay(delayMs / portTICK_PERIOD_MS);
    WiFiManager::getInstance()->connectNow();
    vTaskDelete(NULL);
}
void WiFiManager::connectNow()
{
    retryCount = 0;
    isConnecting = true;

    Serial.printf("🔁 Connecting to Wi-Fi (attempt %d/%d)...\n", retryCount + 1, maxRetries);

    WiFi.disconnect(true);
    delay(100);

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.begin(_ssid.c_str(), _password.c_str());

    static bool eventHooked = false;
    if (!eventHooked)
    {
        WiFi.onEvent(WiFiManager::handleWiFiEvent); // Register WiFi event handler
        eventHooked = true;
    }
}

void WiFiManager::connectAsync(const String &json)
{
    Serial.println("⚙️ connectAsync running");
    Serial.println("📨 JSON: " + json);

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        Serial.println("❌ JSON parse failed");
        return;
    }

    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();
    writeJsonFile("/wifi.json", json);
    Serial.println("💾 Credentials saved to /wifi.json");
    begin(ssid.c_str(), password.c_str(), "pool.ntp.org", 3600);
    connectNow();

    Serial.printf("📦 Free heap: %u bytes\n", ESP.getFreeHeap());
}

void WiFiManager::autoConnectFromFile()
{
    Serial.println("📂 Attempting to load Wi-Fi credentials from /wifi.json");

    String json = readJsonFile("/wifi.json");
    if (json.isEmpty() || json == "{}")
    {
        Serial.println("⚠️ No valid Wi-Fi credentials found.");
        return;
    }

    DynamicJsonDocument doc(256);
    DeserializationError error = deserializeJson(doc, json);
    if (error)
    {
        Serial.println("❌ Failed to parse /wifi.json");
        return;
    }

    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();

    if (ssid.isEmpty() || password.isEmpty())
    {
        Serial.println("⚠️ Incomplete Wi-Fi credentials.");
        return;
    }

    Serial.println("✅ Loaded saved Wi-Fi credentials:");
    Serial.println("📶 SSID: " + ssid);
    Serial.println("🔑 Password: " + password);

    begin(ssid.c_str(), password.c_str(), "pool.ntp.org", 3600);
    connectNow();
}
void WiFiManager::connectWiFi()
{
    Serial.println("⏳ Connecting to Wi-Fi...");
    WiFi.setAutoReconnect(true);
    WiFi.begin(_ssid, _password, 0, NULL, true);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30)
    {
        delay(2000);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("✅ Connected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        syncTime();
        printCurrentTime();
    }
    else
    {
        Serial.println("❌ Failed to connect to WiFi");
    }
}

void WiFiManager::scanWifiNetworks()
{
    Serial.println("🔍 Starting WiFi scan...");

    WiFi.mode(WIFI_STA);
    int numNetworks = WiFi.scanNetworks();

    if (numNetworks == WIFI_SCAN_FAILED)
    {
        Serial.println("❌ Scan failed. Try again.");
        return;
    }

    if (numNetworks == 0)
    {
        Serial.println("❌ No networks found.");
        BLEManager::sendBLEData("[]");
        return;
    }

    Serial.printf("✅ %d networks found:\n", numNetworks);

    String json = "[";
    for (int i = 0; i < numNetworks; i++)
    {
        String ssid = WiFi.SSID(i);
        int rssi = WiFi.RSSI(i);
        bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;

        Serial.printf("%d: %s (Signal: %d dBm, Encryption: %s)\n",
                      i + 1, ssid.c_str(), rssi, secured ? "Secured" : "Open");

        json += "{";
        json += "\"ssid\":\"" + ssid + "\",";
        json += "\"rssi\":" + String(rssi) + ",";
        json += "\"secured\":" + String(secured ? "true" : "false");
        json += "}";

        if (i < numNetworks - 1)
            json += ",";
    }
    json += "]";

    BLEManager::sendBLEData(json);
    WiFi.scanDelete();
}

void WiFiManager::syncTime()
{
    Serial.println("⏳ Fetching time from NTP server...");
    _timeClient.begin();

    while (!_timeClient.update())
    {
        _timeClient.forceUpdate();
    }

    time_t epochTime = _timeClient.getEpochTime();
    RTCManager::getInstance()->updateRTC(epochTime);
    Serial.println("✅ Time fetched from NTP server: " + _timeClient.getFormattedTime());
}

void WiFiManager::printCurrentTime()
{
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        Serial.printf("📅 Current Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
    else
    {
        Serial.println("❌ Failed to get time");
        timeSynced = false;
    }
}

bool WiFiManager::isTimeSynced() const
{
    return timeSynced;
}
