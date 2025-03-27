#include <Arduino.h>
#include "BLEManager.h"
#include "SPIFFSHelper.h"
#define RAW_JSON_FILE "/data.json"

void setup() {
    Serial.begin(115200);

    setupSPIFFS();  // Initialize SPIFFS
    setupBLE();     // Initialize BLE

    if (splitCalendarJson(RAW_JSON_FILE)) {
        Serial.println("🎉 Calendar successfully split into 12 files!");
    } else {
        Serial.println("❌ Failed to split calendar!");
    }
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
}

