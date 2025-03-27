#include "SPIFFSHelper.h"
#include <ArduinoJson.h>

#define MAIN_JSON_FILE "/calendar.json"

bool setupSPIFFS() {
    if (!SPIFFS.begin(true)) {  // "true" will format SPIFFS if it fails
        Serial.println("⚠️ SPIFFS initialization failed!");
        return false;
    }
    Serial.println("✅ SPIFFS initialized successfully.");
    return true;
}

String readJsonFile(const String &jsonPath) {
    Serial.println("📂 Reading large JSON file from SPIFFS...");

    File file = SPIFFS.open(jsonPath, "r");
    if (!file) {
        Serial.println("❌ Failed to open file!");
        return "{}";  // Return an empty JSON object in case of error
    }

    String jsonData = "";
    const size_t bufferSize = 2048;  // Size of each chunk
    char buffer[bufferSize];

    // Read the file in chunks
    while (file.available()) {
        size_t bytesRead = file.readBytes(buffer, bufferSize);
        jsonData += String(buffer).substring(0, bytesRead);  // Append the chunk to jsonData
    }

    file.close();
    
    Serial.printf("✅ Read %d bytes from file.\n", jsonData.length());
    return jsonData;
}

bool writeJsonFile(const String &filename, const String &jsonData) {
    Serial.printf("💾 Writing JSON to: %s\n", filename.c_str());

    File file = SPIFFS.open(filename, "w");
    if (!file) {
        Serial.println("❌ Failed to open file for writing!");
        return false;
    }

    file.print(jsonData);
    file.close();

    Serial.println("✅ JSON saved successfully.");
    return true;
}

bool splitCalendarJson(const String &rawJsonPath) {
    Serial.println("📂 Loading main JSON file...");
    File file = SPIFFS.open(rawJsonPath, "r");
    if (!file) {
        Serial.println("❌ Failed to open file!");
        return "{}";  // Return an empty JSON object in case of error
    }
    // Read the full JSON from SPIFFS
    // String jsonData = readJsonFile(String(rawJsonPath));
    // Serial.println(jsonData);

    // if (jsonData.length() == 0) {
    //     Serial.println("❌ No data found in main JSON file!");
    //     return false;
    // }

    // Parse the JSON
    DynamicJsonDocument doc(80000);  // 80KB
    DeserializationError error = deserializeJson(doc, file);
    // Print the parsed JSON in a nicely formatted way
    // serializeJsonPretty(doc, Serial);
    // Serial.println();
    if (error) {
        Serial.print("❌ JSON parsing failed: ");
        Serial.println(error.f_str());
        return false;
    }
    if (doc.containsKey("calendar")) {
        Serial.println("'calendar' found!");
    } else {
        Serial.println("⚠️ 'calendar' key not found!");
        return false;
    }

    JsonArray calendar = doc["calendar"].as<JsonArray>();

// Loop through months (0 to 11 since JSON arrays are 0-based)
    for (int month = 0; month < calendar.size(); month++) {
        if (calendar[month].is<JsonObject>()) {
            // Create a new JSON object for the month
            DynamicJsonDocument monthDoc(2048);
            monthDoc["month"] = month + 1;  // Store the month number (1 to 12)
            monthDoc["prayerCalender"] = calendar[month]; // Copy the month's data

            // Convert to a string
            String monthJson;
            serializeJson(monthDoc, monthJson);

            // Save to SPIFFS
            String filename = "/prayer_times_month_" + String(month + 1) + ".json";
            if (writeJsonFile(filename, monthJson)) {
                Serial.printf("✅ Saved: %s\n", filename.c_str());
                Serial.println(monthJson);
            } else {
                Serial.printf("❌ Failed to save: %s\n", filename.c_str());
                return false;
            }
        } else {
            Serial.printf("⚠️ Month %d not found or invalid!\n", month + 1);
        }
    }

    return true;
}