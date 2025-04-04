#include "RTCManager.h"
#include <Arduino.h>

RTCManager *RTCManager::instance = nullptr;
RTCManager *RTCManager::getInstance()
{
    if (!instance)
    {
        instance = new RTCManager(); // Only create a new instance if one doesn't exist.
    }
    return instance;
}

bool timeSynced = false;                              // Flag to indicate if time is synced
RTC_DATA_ATTR bool RTCManager::manualTimeSet = false; // Define static variable with RTC_DATA_ATTR
void RTCManager::setRtcTimeFromISO8601(const String &iso8601Time)
{
    struct tm tm;
    if (strptime(iso8601Time.c_str(), "%Y-%m-%dT%H:%M:%S", &tm) != NULL)
    {
        time_t t = mktime(&tm);
        struct timeval tv = {t, 0}; // seconds, microseconds
        settimeofday(&tv, NULL);    // Set the ESP32 RTC
        Serial.println("✅ ESP32 RTC Updated!");
        timeSynced = true; // Set the flag to indicate time is synced
    }
}
void RTCManager::printTime()
{
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo))
    {
        Serial.println("❌ Failed to get time");
        return;
    }

    Serial.printf("⏰ Current RTC Time: %04d-%02d-%02d %02d:%02d:%02d\n",
                  timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                  timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}
bool RTCManager::isTimeSynced()
{
    return timeSynced;
}

void RTCManager::setTimeToSpecificHourAndMinute(int newHour, int newMinute, int newMonth, int newDay)
{
    if (manualTimeSet)
    {
        Serial.println("❌ Manual time setting is already set.");
        return;
    }
    struct tm timeinfo;
    if (getLocalTime(&timeinfo))
    {
        // Retain the current date (year, month, day), but update the time
        timeinfo.tm_hour = newHour;
        timeinfo.tm_min = newMinute;

        if (newMonth > 0 && newDay > 0)
        {
            timeinfo.tm_mon = newMonth - 1; // tm_mon is 0-based
            timeinfo.tm_mday = newDay;
        }
        time_t newTime = mktime(&timeinfo);

        // Set the new time back to the RTC
        struct timeval tv = {newTime, 0}; // seconds, microseconds
        settimeofday(&tv, NULL);          // Set ESP32 RTC

        Serial.printf("✏️ ESP32 RTC Updated with new time %02d:%02d - %2d/%2d\n", newHour, newMinute, newMonth, newDay);
        manualTimeSet = true;
    }
    else
    {
        Serial.println("❌ Failed to get current time for RTC update.");
        manualTimeSet = false;
    }
}
