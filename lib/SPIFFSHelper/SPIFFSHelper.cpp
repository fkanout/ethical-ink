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
  Serial.printf("\r💾 Writing JSON to: %s", filename.c_str());

  File file = SPIFFS.open(filename, "w");
  if (!file) {
    Serial.printf("\r❌ Failed to open file for writing: %s\n",
                  filename.c_str());
    return false;
  }

  file.print(jsonData);
  file.close();

  Serial.printf("\r✅ JSON saved successfully: %s\n", filename.c_str());
  return true;
}

bool splitCalendarJson(const String &rawJsonPath, const bool &isIqama) {
  Serial.println("📂 Loading main JSON file...");
  File file = SPIFFS.open(rawJsonPath, "r");
  if (!file) {
    Serial.println("❌ Failed to open file!");
    return false;
  }

  size_t fileSize = file.size();
  Serial.printf("📦 File size: %d bytes\n", fileSize);

  // Calculate required capacity: file size + ~30% overhead for ArduinoJson
  size_t capacity = fileSize + (fileSize / 3);
  Serial.printf("📊 JSON capacity: %d bytes\n", capacity);

  DynamicJsonDocument doc(capacity);
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  Serial.printf("📈 Memory used: %d bytes\n", doc.memoryUsage());

  if (error) {
    Serial.print("❌ JSON parsing failed: ");
    Serial.println(error.f_str());
    Serial.printf("⚠️ Error code: %d\n", error.code());
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
        Serial.printf("\r✅ Saved: %s", filename.c_str());
      } else {
        Serial.printf("\r❌ Failed to save: %s",
                      filename.c_str()); // Also same line
        return false;
      }
    } else {
      Serial.printf("\r⚠️ Month %d not found or invalid!", month + 1);
    }
  }
  Serial.println();

  return true;
}