#include "SPIFFSHelper.h"
#include <ArduinoJson.h>

#define MAIN_JSON_FILE "/calendar.json"

bool setupSPIFFS() {
  if (!SPIFFS.begin(true)) { // "true" will format SPIFFS if it fails
    Serial.println("⚠️ SPIFFS initialization failed!");
    return false;
  }
  Serial.println("✅ SPIFFS initialized successfully.");
  return true;
}
void deleteFile(const char *path) {
  if (SPIFFS.exists(path)) {
    if (SPIFFS.remove(path)) {
      Serial.printf("🗑️ Deleted file: %s\n", path);
    } else {
      Serial.printf("❌ Failed to delete file: %s\n", path);
    }
  } else {
    Serial.printf("⚠️ File does not exist: %s\n", path);
  }
}

String readJsonFile(const String &jsonPath) {
  Serial.println("📂 Reading large JSON file from SPIFFS...");

  File file = SPIFFS.open(jsonPath, "r");
  if (!file) {
    Serial.println("❌ Failed to open file!");
    return "{}"; // Return an empty JSON object in case of error
  }

  String jsonData = "";
  const size_t bufferSize = 2048; // Size of each chunk
  char buffer[bufferSize];

  // Read the file in chunks
  while (file.available()) {
    size_t bytesRead = file.readBytes(buffer, bufferSize);
    jsonData +=
        String(buffer).substring(0, bytesRead); // Append the chunk to jsonData
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

bool splitCalendarJson(const String &rawJsonPath, const bool &isIqama) {
  Serial.println("📂 Loading main JSON file...");
  File file = SPIFFS.open(rawJsonPath, "r");
  if (!file) {
    Serial.println("❌ Failed to open file!");
    return "{}";
  }
  DynamicJsonDocument doc(80000); // 80KB
  DeserializationError error = deserializeJson(doc, file);
  if (error) {
    Serial.print("❌ JSON parsing failed: ");
    Serial.println(error.f_str());
    return false;
  }
  String mainKey = isIqama ? "iqamaCalendar" : "calendar";
  String jsonKey = isIqama ? "iqamaCalendar" : "prayerCalender";
  String fileName = isIqama ? "/iqama_times_month_" : "/prayer_times_month_";
  if (doc.containsKey(mainKey)) {
    Serial.println(mainKey + " found!");
  } else {
    Serial.println("⚠️" + mainKey + " key not found!");
    return false;
  }

  JsonArray calendar = doc[mainKey].as<JsonArray>();

  for (int month = 0; month < calendar.size(); month++) {
    if (calendar[month].is<JsonObject>()) {

      DynamicJsonDocument monthDoc(2048);
      monthDoc["month"] = month + 1;
      monthDoc[jsonKey] = calendar[month];
      String monthJson;
      serializeJson(monthDoc, monthJson);

      String filename = fileName + String(month + 1) + ".json";
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