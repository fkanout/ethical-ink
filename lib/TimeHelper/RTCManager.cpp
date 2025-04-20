#include "RTCManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiUdp.h>

RTCManager &RTCManager::getInstance() {
  static RTCManager instance;
  return instance;
}

RTC_DATA_ATTR bool RTCManager::timeSynced = false;
RTC_DATA_ATTR bool RTCManager::manualTimeSet = false;

void RTCManager::setRtcTimeFromISO8601(const String &iso8601Time) {
  struct tm tm;
  if (strptime(iso8601Time.c_str(), "%Y-%m-%dT%H:%M:%S", &tm) != NULL) {
    time_t t = mktime(&tm);
    struct timeval tv = {t, 0};
    settimeofday(&tv, NULL);
    Serial.println("✅ ESP32 RTC Updated!");
    timeSynced = true;
  } else {
    Serial.println("❌ Failed to parse ISO 8601 time string.");
  }
}

void RTCManager::printTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to get time");
    return;
  }

  Serial.printf("⏰ Current RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

bool RTCManager::isTimeSynced() { return timeSynced; }

void RTCManager::updateRTC(time_t epochTime) {
  struct timeval now = {epochTime, 0};
  settimeofday(&now, nullptr);
  timeSynced = true;
  Serial.println("✅ RTC updated with epoch time");
}

void RTCManager::setTimeToSpecificHourAndMinute(int newHour, int newMinute,
                                                int newMonth, int newDay) {
  if (manualTimeSet) {
    Serial.println("❌ Manual time setting is already set.");
    return;
  }
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    timeinfo.tm_hour = newHour;
    timeinfo.tm_min = newMinute;
    if (newMonth > 0 && newDay > 0) {
      timeinfo.tm_mon = newMonth - 1;
      timeinfo.tm_mday = newDay;
    }
    time_t newTime = mktime(&timeinfo);
    struct timeval tv = {newTime, 0};
    settimeofday(&tv, NULL);
    Serial.printf("✏️ ESP32 RTC Updated with new time %02d:%02d - %2d/%2d\n",
                  newHour, newMinute, newMonth, newDay);
    manualTimeSet = true;
  } else {
    Serial.println("❌ Failed to get current time for RTC update.");
    manualTimeSet = false;
  }
}

bool RTCManager::syncTimeFromNTPWithOffset(int maxRetries, uint32_t timeoutMs) {
  WiFiClientSecure client;
  client.setInsecure();
  int offsetSeconds = 0;
  bool gotOffset = false;

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.printf("🌐 Attempt %d to get timezone offset from ipwho.is\n",
                  attempt);

    if (client.connect("ipwho.is", 443)) {
      client.print("GET / HTTP/1.1\r\nHost: ipwho.is\r\nUser-Agent: "
                   "RTCManager/1.0\r\nConnection: close\r\nAccept: "
                   "application/json\r\n\r\n");

      while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r")
          break;
      }

      String response;
      while (client.available())
        response += client.readString();

      int jsonStart = response.indexOf('{');
      if (jsonStart > 0)
        response = response.substring(jsonStart);

      DynamicJsonDocument doc(2048);
      if (deserializeJson(doc, response) == DeserializationError::Ok &&
          doc["success"]) {
        offsetSeconds = doc["timezone"]["offset"] | 0;
        gotOffset = true;
        Serial.printf("✅ Timezone offset: %d seconds\n", offsetSeconds);
        break;
      } else {
        Serial.println("❌ Failed to parse ipwho.is response");
      }
    } else {
      Serial.println("❌ Connection to ipwho.is failed");
    }

    delay(1000);
  }

  if (!gotOffset) {
    return false;
  }

  if (offsetSeconds == 0) {
    Serial.println("⚠️ Using default UTC offset (0)");
  }

  // Step 2: Try to sync time from NTP server using offset
  WiFiUDP udp;
  NTPClient timeClient(udp, "pool.ntp.org", offsetSeconds);
  timeClient.begin();

  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    Serial.printf("⏳ Attempt %d to sync NTP time\n", attempt);
    if (timeClient.update()) {
      Serial.println("✅ NTP time sync complete");
      time_t now = timeClient.getEpochTime();
      updateRTC(now);
      timeClient.end();

      return true;
    }
    delay(timeoutMs);
  }

  Serial.println("❌ Failed to sync time from NTP");
  timeClient.end();
  return false;
}
