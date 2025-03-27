#include <Arduino.h>
#include "BLEManager.h"
#include "SPIFFSHelper.h"
#include "WiFiManager.h"

#include <time.h>
#define RAW_JSON_FILE "/data.json"


// Replace with your network credentials
const char* SSID = "Kanout";
const char* PASSWORD = "%0$Vs1zffWAIno#e1*koMx3q3H!zE5zO";
const char* NTP_SERVER = "pool.ntp.org";
const long UTC_OFFSET = 3600;  // Adjust based on your timezon
WiFiManager wifiManager(SSID, PASSWORD, NTP_SERVER, UTC_OFFSET);

void setInitialTime() {
    struct tm timeinfo = {};
    timeinfo.tm_year = 2025 - 1900;  // Year 2025
    timeinfo.tm_mon = 3 - 1;         // March (months are 0-based)
    timeinfo.tm_mday = 27;           // Day 27
    timeinfo.tm_hour = 16;
    timeinfo.tm_min = 35;
    timeinfo.tm_sec = 0;

    struct timeval now = {mktime(&timeinfo), 0};
    settimeofday(&now, NULL);
}
void printTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        Serial.println("❌ Failed to get time");
        return;
    }

    Serial.printf("⏰ Current RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}


void setup() {
    Serial.begin(115200);
    setupSPIFFS();  
    setupBLE(); 
    // if (splitCalendarJson(RAW_JSON_FILE)) {
    //     Serial.println("🎉 Calendar successfully split into 12 files!");
    // } else {
    //     Serial.println("❌ Failed to split calendar!");
    // }
    wifiManager.connectWiFi();
    while (!wifiManager.isTimeSynced()) {
        delay(1000);  // Wait for time to sync
        Serial.println("Waiting for time sync...");
    }

    Serial.println("✅ Time synced, proceed with calendar fetch...");


}

void listenFromBLEAndUpdateJSON() {
    if (isNewBLEDataAvailable()) {  // Check if new data is received
        String jsonData = getReceivedBLEData();
        Serial.println("💾 Saving received JSON to SPIFFS...");
        bool success = writeJsonFile("data.json",jsonData);
        Serial.println(success ? "✅ JSON saved successfully!" : "❌ Failed to save JSON.");
        Serial.println(jsonData);

    }
}
void loop() {
    listenFromBLEAndUpdateJSON();
    // if (wifiManager.isTimeSynced()) {
    //     // Do something once time is synced, e.g., fetch events from calendar
    //     Serial.println("Time synced. Proceed with further actions.");
    // } else {
    //     // Handle the case where time is not synced
    //     Serial.println("Time not synced yet.");
    // }
}

