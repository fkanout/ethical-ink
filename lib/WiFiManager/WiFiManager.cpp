#include "WiFiManager.h"
#include <time.h>

WiFiManager::WiFiManager(const char* ssid, const char* password, const char* ntpServer, long utcOffset)
    : _ssid(ssid), _password(password), _ntpServer(ntpServer), _utcOffset(utcOffset), _timeClient(_udp, ntpServer, utcOffset) {}

void WiFiManager::connectWiFi() {
    Serial.println("⏳ Connecting to Wi-Fi...");
    WiFi.setAutoReconnect(true);  // Enable auto reconnect
    WiFi.begin(_ssid, _password);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {  // Try for 30 seconds
        delay(2000);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("✅ Connected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        syncTime();
        printCurrentTime();
    } else {
        Serial.println("❌ Failed to connect to WiFi");
    }
}
void WiFiManager::scanWifiNetworks() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();  // Ensure we're not connected before scanning
    delay(100);

    Serial.println("🔍 Scanning for available networks...");

    int numNetworks = WiFi.scanNetworks();
    if (numNetworks == 0) {
        Serial.println("❌ No networks found.");
    } else {
        Serial.printf("✅ %d networks found:\n", numNetworks);
        for (int i = 0; i < numNetworks; i++) {
            Serial.printf("%d: %s (Signal Strength: %d dBm, Encryption: %s)\n",
                          i + 1, 
                          WiFi.SSID(i).c_str(), 
                          WiFi.RSSI(i), 
                          WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Secured");
        }
    }
    WiFi.scanDelete();  // Free memory
}
void WiFiManager::syncTime() {
    Serial.println("⏳ Fetching time from NTP server...");
    _timeClient.begin();
    
    while (!_timeClient.update()) {
        _timeClient.forceUpdate();
    }
    
    time_t epochTime = _timeClient.getEpochTime();
    updateRTC(epochTime);
    timeSynced = true;
    Serial.println("✅ Time synchronized!");
}

void WiFiManager::updateRTC(time_t epochTime) {
    struct timeval now = {epochTime, 0};
    settimeofday(&now, NULL);
}

void WiFiManager::printCurrentTime() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        Serial.printf("📅 Current Time: %04d-%02d-%02d %02d:%02d:%02d\n", 
                      timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    } else {
        Serial.println("❌ Failed to get time");
        timeSynced = false;

    }
}
bool WiFiManager::isTimeSynced() const {
    return timeSynced;
}