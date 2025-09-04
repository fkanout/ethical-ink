#include "MAWAQITManager.h"
#include <AppState.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>

#define TEMP_JSON_PATH "/temp.json"
#define MAX_RETRIES 3

MAWAQITManager &MAWAQITManager::getInstance() {
  static MAWAQITManager instance;
  return instance;
}

void MAWAQITManager::setApiKey(const String &key) { apiKey = key; }

void MAWAQITManager::asyncFetchPrayerTimes(const String &mosqueUUID,
                                           FetchCallback callback) {
  if (fetchTaskHandle != nullptr) {
    Serial.println("🛑 Stopping existing MAWAQIT fetch task...");
    vTaskDelete(fetchTaskHandle);
    fetchTaskHandle = nullptr;
  }
  FetchParams *params = new FetchParams{mosqueUUID, callback};
  xTaskCreate(fetchTask, "MawaqitFetchTask", 8192, params, 1, &fetchTaskHandle);
}

void MAWAQITManager::fetchTask(void *parameter) {
  FetchParams *params = static_cast<FetchParams *>(parameter);
  bool success = false;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Not connected to Wi-Fi. Aborting fetch.");
    if (params->callback) {
      params->callback(false, nullptr);
    }
    delete params;
    vTaskDelete(nullptr);
    return;
  }
  String url = "https://mawaqit.net/api/3.0/mosque/" + params->mosqueUUID +
               "/times?calendar";
  Serial.printf("🌐 Fetching from: %s\n", url.c_str());
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);
  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("❌ HTTPS.begin() failed");
  } else {
    https.addHeader("Api-Access-Token", MAWAQITManager::getInstance().apiKey);
    https.setTimeout(10000); // 10 seconds timeout
    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK) {
      Serial.printf("❌ HTTP GET failed: %d\n", httpCode);
    } else {
      String json = https.getString();
      Serial.printf("📦 Received %d bytes\n", json.length());
      // Validate the JSON (lightweight structure check)
      StaticJsonDocument<512> doc;
      DeserializationError error = deserializeJson(doc, json);
      if (error) {
        Serial.printf("❌ JSON validation failed: %s\n", error.c_str());
      } else {
        File file = SPIFFS.open(MOSQUE_FILE, FILE_WRITE);
        if (!file) {
          Serial.println("❌ Failed to open file for writing");
        } else {
          file.print(json);
          file.close();
          Serial.printf("✅ JSON saved to %s\n", MOSQUE_FILE);
          success = true;
        }
      }
    }
    https.end();
  }
  if (params->callback) {
    params->callback(success, success ? MOSQUE_FILE : nullptr);
  }
  delete params;
  MAWAQITManager::getInstance().fetchTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

// NEW FUNCTION: Async fetch mosque info
void MAWAQITManager::asyncFetchMosqueInfo(const String &mosqueUUID,
                                          FetchCallback callback) {
  if (fetchInfoTaskHandle != nullptr) {
    Serial.println("🛑 Stopping existing MAWAQIT info fetch task...");
    vTaskDelete(fetchInfoTaskHandle);
    fetchInfoTaskHandle = nullptr;
  }
  
  InfoFetchParams *params = new InfoFetchParams{mosqueUUID, callback};
  xTaskCreate(fetchInfoTask, "MawaqitInfoTask", 8192, params, 1, &fetchInfoTaskHandle);
}

void MAWAQITManager::fetchInfoTask(void *parameter) {
  InfoFetchParams *params = static_cast<InfoFetchParams *>(parameter);
  bool success = false;
  String mosqueName = "";

  // Check Wi-Fi connection first
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Not connected to Wi-Fi. Aborting info fetch.");
    if (params->callback) {
      params->callback(false, nullptr);
    }
    delete params;
    vTaskDelete(nullptr);
    return;
  }

  // API endpoint
  String url = "https://mawaqit.net/api/3.0/mosque/" + params->mosqueUUID + "/info?calendar";
  Serial.printf("🌐 Fetching mosque info from: %s\n", url.c_str());

  WiFiClientSecure client;
  client.setInsecure();               // Disable certificate check
  //client.setBufferSizes(512, 512);    // Reduce RAM usage
  client.setTimeout(10000);           // 10s timeout

  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("❌ HTTPS.begin() failed for mosque info");
  } else {
    https.addHeader("Api-Access-Token", MAWAQITManager::getInstance().apiKey);
    https.setTimeout(10000);

    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK) {
      Serial.printf("❌ HTTP GET failed for mosque info: %d\n", httpCode);
    } else {
      String payload = https.getString();
      Serial.printf("📦 Received mosque info: %d bytes\n", payload.length());

      // Extract "name" value manually from JSON
      int nameIndex = payload.indexOf("\"name\"");
      if (nameIndex != -1) {
        int colonIndex = payload.indexOf(":", nameIndex);
        int quoteStart = payload.indexOf("\"", colonIndex + 1);
        int quoteEnd   = payload.indexOf("\"", quoteStart + 1);

        if (quoteStart != -1 && quoteEnd != -1) {
          mosqueName = payload.substring(quoteStart + 1, quoteEnd);
          Serial.printf("🕌 Mosque Name: %s\n", mosqueName.c_str());
          success = true;
        } else {
          Serial.println("⚠️ Could not extract mosque name");
        }
      } else {
        Serial.println("⚠️ 'name' field not found in JSON");
      }
    }

    https.end();
    client.stop();
  }

  // Return mosque name (instead of file path) via callback
  if (params->callback) {
    params->callback(success, success ? mosqueName.c_str() : nullptr);
  }

  delete params;
  MAWAQITManager::getInstance().fetchInfoTaskHandle = nullptr;
  vTaskDelete(nullptr);
}
