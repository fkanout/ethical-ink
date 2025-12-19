#include "RTCManager.h"
#include <Arduino.h>
#include <ArduinoJson.h>

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
time_t RTCManager::getEpochTime() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    time_t epochTime = mktime(&timeinfo);
    return epochTime;
  } else {
    Serial.println("❌ Failed to get current epoch time");
    return 0;
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

bool RTCManager::syncTimeFromNTPWithOffset(int maxRetries, uint32_t timeoutMs, int timezoneOffsetSeconds) {
  if (timezoneOffsetSeconds == 0) {
    Serial.println("⏰ Syncing time from NTP (UTC)");
  } else {
    int hours = timezoneOffsetSeconds / 3600;
    int minutes = (abs(timezoneOffsetSeconds) % 3600) / 60;
    Serial.printf("⏰ Syncing time from NTP (UTC%+d:%02d)\n", hours, minutes);
  }

  configTime(timezoneOffsetSeconds, 0, "pool.ntp.org", "time.nist.gov");

  Serial.println("⏳ Waiting for NTP time sync...");
  
  int attempts = 0;
  const int maxAttempts = maxRetries * 10;
  struct tm timeinfo;
  
  while (!getLocalTime(&timeinfo) && attempts < maxAttempts) {
    delay(100);
    attempts++;
    if (attempts % 10 == 0) {
      Serial.printf("⏳ Waiting... (%d/%d)\n", attempts / 10, maxRetries);
      }
    }

  if (attempts >= maxAttempts) {
    Serial.println("❌ Failed to sync time from NTP (timeout)");
    return false;
    }

  Serial.println("✅ NTP time sync complete");
  timeSynced = true;
  
  Serial.printf("⏰ Synced Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

      return true;
}
