#include "WeatherManager.h"
#include "AppStateManager.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

extern RTCData rtcData;

struct FetchParams {
  float latitude;
  float longitude;
  WeatherManager::FetchCallback callback;
};

WeatherManager &WeatherManager::getInstance() {
  static WeatherManager instance;
  return instance;
}

void WeatherManager::asyncFetchWeather(float latitude, float longitude,
                                        FetchCallback callback) {
  FetchParams *params = new FetchParams{latitude, longitude, callback};
  xTaskCreate(fetchTask, "WeatherFetchTask", 8192, params, 1, nullptr);
}

void WeatherManager::fetchTask(void *parameter) {
  FetchParams *params = static_cast<FetchParams *>(parameter);

  // Build Open-Meteo API URL
  // Format: https://api.open-meteo.com/v1/forecast?latitude=X&longitude=Y&current=temperature_2m,weather_code
  String url = "https://api.open-meteo.com/v1/forecast?";
  url += "latitude=" + String(params->latitude, 6);
  url += "&longitude=" + String(params->longitude, 6);
  url += "&current=temperature_2m,weather_code";

  Serial.printf("🌤️ Fetching weather from Open-Meteo: %s\n", url.c_str());

  bool success = false;
  const int MAX_RETRIES = 2;
  int httpCode = -1;
  String payload = "";

  // Retry loop
  for (int attempt = 1; attempt <= MAX_RETRIES + 1; attempt++) {
    HTTPClient http;
    WiFiClientSecure client;
    client.setInsecure(); // Skip certificate validation

    if (attempt > 1) {
      Serial.printf("🔄 Retry attempt %d/%d for Open-Meteo\n", attempt - 1, MAX_RETRIES);
      delay(2000); // Wait 2 seconds between retries
    }

    http.begin(client, url);
    http.setTimeout(15000);
    httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
      payload = http.getString();
      Serial.printf("✅ Received %d bytes from Open-Meteo\n", payload.length());
      http.end();
      break; // Success, exit retry loop
    } else {
      Serial.printf("❌ Open-Meteo request failed, code: %d (attempt %d/%d)\n", 
                    httpCode, attempt, MAX_RETRIES + 1);
      http.end();
      
      if (attempt == MAX_RETRIES + 1) {
        // All retries exhausted
        Serial.println("❌ All Open-Meteo retry attempts failed");
      }
    }
  }

  if (httpCode == HTTP_CODE_OK) {

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);

    if (!error) {
      JsonObject current = doc["current"];
      if (current) {
        float temp = current["temperature_2m"] | 0.0;
        int weatherCode = current["weather_code"] | 0;

        // Map WMO weather codes to descriptions
        const char *description = "Unknown";
        if (weatherCode == 0) {
          description = "Clear";
        } else if (weatherCode >= 1 && weatherCode <= 3) {
          description = "Cloudy";
        } else if (weatherCode >= 45 && weatherCode <= 48) {
          description = "Fog";
        } else if (weatherCode >= 51 && weatherCode <= 67) {
          description = "Rain";
        } else if (weatherCode >= 71 && weatherCode <= 77) {
          description = "Snow";
        } else if (weatherCode >= 80 && weatherCode <= 82) {
          description = "Rain";
        } else if (weatherCode >= 85 && weatherCode <= 86) {
          description = "Snow";
        } else if (weatherCode >= 95 && weatherCode <= 99) {
          description = "Storm";
        }

        // Save to RTC memory
        rtcData.currentTemp = temp;
        strncpy(rtcData.weatherDesc, description, sizeof(rtcData.weatherDesc) - 1);
        rtcData.weatherDesc[sizeof(rtcData.weatherDesc) - 1] = '\0';
        
        // Use epoch time (not millis) for proper interval checking
        time_t now = time(nullptr);
        rtcData.weatherLastUpdate = now;

        AppStateManager::save();

        Serial.printf("🌤️ Weather updated: %.1f°C, %s\n", temp, description);
        success = true;
      } else {
        Serial.println("❌ No 'current' data in Open-Meteo response");
      }
    } else {
      Serial.printf("❌ Failed to parse Open-Meteo JSON: %s\n", error.c_str());
    }
  } else {
    Serial.println("❌ All Open-Meteo attempts failed");
  }

  if (params->callback) {
    params->callback(success);
  }

  delete params;
  vTaskDelete(nullptr);
}

