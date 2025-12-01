#include "AladhanManager.h"
#include "SPIFFSHelper.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

struct FetchParams {
  float latitude;
  float longitude;
  int calculationMethod;
  int month;
  int year;
  AladhanManager::FetchCallback callback;
};

AladhanManager &AladhanManager::getInstance() {
  static AladhanManager instance;
  return instance;
}

void AladhanManager::asyncFetchMonthlyPrayerTimes(float latitude,
                                                   float longitude,
                                                   int calculationMethod,
                                                   int month, int year,
                                                   FetchCallback callback) {
  FetchParams *params = new FetchParams{latitude, longitude, calculationMethod,
                                        month, year, callback};
  xTaskCreate(fetchTask, "AladhanFetchTask", 16384, params, 1, nullptr);
}

void AladhanManager::fetchTask(void *parameter) {
  FetchParams *params = static_cast<FetchParams *>(parameter);

  // Build Aladhan API URL
  // Format: http://api.aladhan.com/v1/calendar/[year]/[month]?latitude=[lat]&longitude=[lon]&method=[method]
  String url = "http://api.aladhan.com/v1/calendar/";
  url += String(params->year) + "/" + String(params->month);
  url += "?latitude=" + String(params->latitude, 6);
  url += "&longitude=" + String(params->longitude, 6);
  url += "&method=" + String(params->calculationMethod);

  Serial.printf("📡 Fetching from Aladhan API: %s\n", url.c_str());

  bool success = false;
  const char *outputPath = nullptr; // Not used - files are written directly per month
  const int MAX_RETRIES = 2;
  int httpCode = -1;
  String payload = "";

  // Retry loop
  for (int attempt = 1; attempt <= MAX_RETRIES + 1; attempt++) {
    HTTPClient http;
    WiFiClient client;

    if (attempt > 1) {
      Serial.printf("🔄 Retry attempt %d/%d for Aladhan API\n", attempt - 1, MAX_RETRIES);
      delay(2000); // Wait 2 seconds between retries
    }

    http.begin(client, url);
    http.setTimeout(15000); // 15 second timeout
    httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      Serial.printf("✅ Received %d bytes from Aladhan API\n", payload.length());
      http.end();
      break; // Success, exit retry loop
    } else {
      Serial.printf("❌ Aladhan API request failed, code: %d (attempt %d/%d)\n", 
                    httpCode, attempt, MAX_RETRIES + 1);
      http.end();
      
      if (attempt == MAX_RETRIES + 1) {
        // All retries exhausted
        Serial.println("❌ All Aladhan API retry attempts failed");
      }
    }
  }

  if (httpCode == HTTP_CODE_OK) {
    // Parse Aladhan API response
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error && doc["code"] == 200) {
      JsonArray data = doc["data"].as<JsonArray>();

      // Extract just HH:MM from times (Aladhan returns "HH:MM (TZ)")
      auto extractTime = [](const String &timeStr) -> String {
        int spacePos = timeStr.indexOf(' ');
        if (spacePos > 0) {
          return timeStr.substring(0, spacePos);
        }
        return timeStr;
      };

      // Build monthly prayer times and iqama times directly in CalendarManager format
      // Format: { "month": X, "prayerCalender": { "1": [...], "2": [...] }, ... }
      JsonDocument prayerDoc;
      prayerDoc["month"] = params->month;
      JsonObject prayerCalendar = prayerDoc["prayerCalender"].to<JsonObject>();

      JsonDocument iqamaDoc;
      iqamaDoc["month"] = params->month;
      JsonObject iqamaCalendar = iqamaDoc["iqamaCalendar"].to<JsonObject>();

      // Process each day in the month
      int dayNum = 1;
      for (JsonVariant dayData : data) {
        JsonObject timings = dayData["timings"];

        // Prayer times array: [Fajr, Sunrise, Dhuhr, Asr, Maghrib, Isha]
        JsonArray prayerArray = prayerCalendar[String(dayNum)].to<JsonArray>();
        prayerArray.add(extractTime(timings["Fajr"].as<String>()));
        prayerArray.add(extractTime(timings["Sunrise"].as<String>()));
        prayerArray.add(extractTime(timings["Dhuhr"].as<String>()));
        prayerArray.add(extractTime(timings["Asr"].as<String>()));
        prayerArray.add(extractTime(timings["Maghrib"].as<String>()));
        prayerArray.add(extractTime(timings["Isha"].as<String>()));

        // Iqama times array: [Fajr, Dhuhr, Asr, Maghrib, Isha] - no Sunrise
        // Aladhan API doesn't provide iqama times, set to "0" (no delay)
        // User can configure via BLE in future if needed
        JsonArray iqamaArray = iqamaCalendar[String(dayNum)].to<JsonArray>();
        iqamaArray.add("0"); // Fajr - no iqama delay
        iqamaArray.add("0"); // Dhuhr - no iqama delay
        iqamaArray.add("0"); // Asr - no iqama delay
        iqamaArray.add("0"); // Maghrib - no iqama delay
        iqamaArray.add("0"); // Isha - no iqama delay

        dayNum++;
      }

      // Write prayer times to SPIFFS
      String prayerFilename = "/prayer_times_month_" + String(params->month) + ".json";
      String prayerOutput;
      serializeJson(prayerDoc, prayerOutput);
      if (!writeJsonFile(prayerFilename, prayerOutput)) {
        Serial.printf("❌ Failed to save prayer times file\n");
        success = false;
      } else {
        Serial.printf("💾 Saved prayer times to %s\n", prayerFilename.c_str());
      }

      // Write iqama times to SPIFFS
      String iqamaFilename = "/iqama_times_month_" + String(params->month) + ".json";
      String iqamaOutput;
      serializeJson(iqamaDoc, iqamaOutput);
      if (!writeJsonFile(iqamaFilename, iqamaOutput)) {
        Serial.printf("❌ Failed to save iqama times file\n");
        success = false;
      } else {
        Serial.printf("💾 Saved iqama times to %s\n", iqamaFilename.c_str());
        success = true;
      }

    } else {
      Serial.println("❌ Failed to parse Aladhan API response");
      if (payload.length() > 0) {
        Serial.println(payload.substring(0, 200)); // Print first 200 chars for debug
      }
    }
  } else {
    Serial.println("❌ All Aladhan API attempts failed");
  }

  // Call callback with result
  if (params->callback) {
    params->callback(success, success ? outputPath : nullptr);
  }

  delete params;
  vTaskDelete(nullptr);
}

// Reverse geocoding using Nominatim (OpenStreetMap)
struct ReverseGeocodeParams {
  float latitude;
  float longitude;
  AladhanManager::ReverseGeocodeCallback callback;
};

void AladhanManager::asyncReverseGeocode(float latitude, float longitude,
                                         ReverseGeocodeCallback callback) {
  ReverseGeocodeParams *params = new ReverseGeocodeParams{latitude, longitude, callback};
  xTaskCreate(reverseGeocodeTask, "ReverseGeocodeTask", 16384, params, 1, nullptr);
}

void AladhanManager::reverseGeocodeTask(void *parameter) {
  ReverseGeocodeParams *params = static_cast<ReverseGeocodeParams *>(parameter);

  HTTPClient http;
  WiFiClientSecure client;
  client.setInsecure(); // Skip certificate validation for simplicity

  // Build Nominatim API URL
  String url = "https://nominatim.openstreetmap.org/reverse?format=json";
  url += "&lat=" + String(params->latitude, 6);
  url += "&lon=" + String(params->longitude, 6);
  url += "&zoom=10"; // City level
  url += "&addressdetails=1";
  url += "&accept-language=en"; // Force English responses

  Serial.printf("🌍 Reverse geocoding: %.6f, %.6f\n", params->latitude, params->longitude);

  bool success = false;
  String cityName = "";

  http.begin(client, url);
  http.addHeader("User-Agent", "ESP32-PrayerTimes/1.0"); // Required by Nominatim
  http.setTimeout(10000); // 10 second timeout
  
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.printf("✅ Received %d bytes from Nominatim\n", payload.length());

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      JsonObject address = doc["address"];
      
      // Debug: Print what we got
      Serial.println("📋 Nominatim address fields:");
      if (address["city"].is<String>()) {
        Serial.printf("  city: %s\n", address["city"].as<String>().c_str());
      }
      if (address["town"].is<String>()) {
        Serial.printf("  town: %s\n", address["town"].as<String>().c_str());
      }
      if (address["village"].is<String>()) {
        Serial.printf("  village: %s\n", address["village"].as<String>().c_str());
      }
      if (address["county"].is<String>()) {
        Serial.printf("  county: %s\n", address["county"].as<String>().c_str());
      }
      if (address["state"].is<String>()) {
        Serial.printf("  state: %s\n", address["state"].as<String>().c_str());
      }
      if (address["country"].is<String>()) {
        Serial.printf("  country: %s\n", address["country"].as<String>().c_str());
      }
      
      // Try to get city name from various fields (city, town, village, etc.)
      if (address["city"].is<String>()) {
        cityName = address["city"].as<String>();
      } else if (address["town"].is<String>()) {
        cityName = address["town"].as<String>();
      } else if (address["village"].is<String>()) {
        cityName = address["village"].as<String>();
      } else if (address["county"].is<String>()) {
        cityName = address["county"].as<String>();
      } else if (address["state"].is<String>()) {
        cityName = address["state"].as<String>();
      }

      // Get country name
      String country = "";
      if (address["country"].is<String>()) {
        country = address["country"].as<String>();
      }

      // Format: "City, Country" or just "City" or just "Country"
      if (!cityName.isEmpty() && !country.isEmpty()) {
        cityName = cityName + ", " + country;
      } else if (cityName.isEmpty() && !country.isEmpty()) {
        cityName = country;
      }

      if (!cityName.isEmpty()) {
        Serial.printf("✅ Location: %s\n", cityName.c_str());
        success = true;
      } else {
        Serial.println("⚠️ Could not extract city name from response");
      }
    } else {
      Serial.println("❌ Failed to parse Nominatim response");
      Serial.printf("Parse error: %s\n", error.c_str());
    }
  } else {
    Serial.printf("❌ Nominatim request failed, code: %d\n", httpCode);
  }

  http.end();

  // Call callback with result
  if (params->callback) {
    params->callback(success, cityName);
  }

  delete params;
  vTaskDelete(nullptr);
}

