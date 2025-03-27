#include <Arduino.h>
#include "BLEManager.h"
#include "SPIFFSHelper.h"

void setup() {
    Serial.begin(115200);

    setupSPIFFS();  // Initialize SPIFFS
    setupBLE();     // Initialize BLE

    // Read and print stored JSON from SPIFFS
    String jsonData = readJsonFile();
    Serial.println("📂 Stored JSON in SPIFFS:");
    Serial.println(jsonData);
}

void listenFromBLEAndUpdateJSON() {
    if (isNewBLEDataAvailable()) {  // Check if new data is received
        String jsonData = getReceivedBLEData();
        Serial.println("💾 Saving received JSON to SPIFFS...");
        bool success = writeJsonFile(jsonData);
        Serial.println(success ? "✅ JSON saved successfully!" : "❌ Failed to save JSON.");
        Serial.println(jsonData);

    }
}
void loop() {
    listenFromBLEAndUpdateJSON();
}

