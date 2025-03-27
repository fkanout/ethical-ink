#include "SPIFFSHelper.h"

#define JSON_FILE "/data.json"  // File path in SPIFFS

bool setupSPIFFS() {
    if (!SPIFFS.begin(true)) {  // "true" will format SPIFFS if it fails
        Serial.println("⚠️ SPIFFS initialization failed!");
        return false;
    }
    Serial.println("✅ SPIFFS initialized successfully.");
    return true;
}

String readJsonFile() {
    Serial.println("📂 Reading JSON data from SPIFFS...");

    File file = SPIFFS.open(JSON_FILE, "r");
    if (!file || file.size() == 0) {
        Serial.println("❌ Failed to open file or file is empty!");
        return "{}";  // Return an empty JSON object
    }

    String jsonData = file.readString();
    file.close();

    Serial.printf("✅ Read %d bytes from file.\n", jsonData.length());
    return jsonData;
}

bool writeJsonFile(const String &jsonData) {
    Serial.println("💾 Writing JSON data to SPIFFS...");

    File file = SPIFFS.open(JSON_FILE, "w");
    if (!file) {
        Serial.println("❌ Failed to open file for writing!");
        return false;
    }

    file.print(jsonData);
    file.close();

    Serial.println("✅ JSON data successfully written to file.");
    return true;
}