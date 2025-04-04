#include <Arduino.h>
#include "BLEManager.h"
#include "SPIFFSHelper.h"
#include "WiFiManager.h"
#include <time.h>
#include <CalendarManager.h>
#include <RTCManager.h>
#include <iostream>

#define RAW_JSON_FILE "/data.json"

// Replace with your network credentials
const char *SSID = "Kanout";
const char *PASSWORD = "%0$Vs1zffWAIno#e1*koMx3q3H!zE5zO";
const char *NTP_SERVER = "pool.ntp.org";
const long UTC_OFFSET = 3600; // Adjust based on your timezon

WiFiManager wifiManager(SSID, PASSWORD, NTP_SERVER, UTC_OFFSET);
CalendarManager calendarManager;

void waitUntilFullMinute()
{
  struct tm timeinfo;
  while (true)
  {
    if (getLocalTime(&timeinfo))
    {
      if (timeinfo.tm_sec == 0)
      { // Wait until the second is exactly 00
        Serial.println("✅ It's the full minute! Running task...");
        return;
      }
    }
    delay(10); // Prevent CPU from looping too fast
  }
}
void executeMainTask()
{
  struct tm timeinfo;
  if (getLocalTime(&timeinfo))
  {
    int currentMonth = timeinfo.tm_mon + 1;
    int currentDay = timeinfo.tm_mday;
    int currentSecond = timeinfo.tm_sec;
    int currentMinute = timeinfo.tm_min;
    int currentHour = timeinfo.tm_hour;
    PrayerTimeInfo prayerTimeInfo = calendarManager.getNextPrayerTimeForToday(currentMonth, currentDay, currentHour, currentMinute);
    Serial.println("🔔 Next prayer is at: " + prayerTimeInfo.nextPrayerMinAndHour);
    for (size_t i = 0; i < prayerTimeInfo.prayerTimes.size(); ++i)
    {
      String prayerTime = prayerTimeInfo.prayerTimes[i];
      String isoTime = prayerTimeInfo.prayerTimesISODate[i];
      Serial.println("    ⏰ " + prayerTime);
    }
    RTCManager *rtcManager = RTCManager::getInstance(); // Get the singleton instance
    rtcManager->setTimeToSpecificHourAndMinute(21, 37, 4, 30);
  }
  else
  {
    Serial.println("❌ Failed to get time.");
  }
}

void waitForTimeToSync()
{
  RTCManager *rtcManager = RTCManager::getInstance(); // Get the singleton instance
  while (!rtcManager->isTimeSynced())
  { // Wait for time to sync
    Serial.println("⏳ Waiting for time sync...");
    delay(1000); // Prevent CPU from running too fast
  }
  Serial.println("✅ Time synced!");
  setup(); // Call setup to re-enter deep sleep after the task
}

void setup()
{
  Serial.begin(115200);
  setupSPIFFS();
  setupBLE();
  // if (splitCalendarJson(RAW_JSON_FILE)) {
  //     Serial.println("🎉 Calendar successfully split into 12 files!");
  // } else {
  //     Serial.println("❌ Failed to split calendar!");
  // }
  // wifiManager.connectWiFi();
  // while (!wifiManager.isTimeSynced()) {
  //     delay(1000);  // Wait for time to sync
  //     Serial.println("Waiting for time sync...");
  // }
  struct tm timeinfo;

  RTCManager *rtcManager = RTCManager::getInstance();
  Serial.println("RTCManager instance address: " + String((uintptr_t)rtcManager, HEX));
  if (!getLocalTime(&timeinfo) && !rtcManager->isTimeSynced())
  {
    Serial.println("❌ Time not synced!");
    waitForTimeToSync(); // Set initial time if not synced
  }
  else
  {
    if (getLocalTime(&timeinfo))
    {
      int currentMonth = timeinfo.tm_mon + 1;
      int currentDay = timeinfo.tm_mday;
      int currentSecond = timeinfo.tm_sec;
      int currentMinute = timeinfo.tm_min;
      int currentHour = timeinfo.tm_hour;
      Serial.printf("⏰ Current time: %02d:%02d:%02d - %2d/%2d/%2d \n", currentHour, currentMinute, currentSecond, currentDay, currentMonth, timeinfo.tm_year + 1900);
      executeMainTask(); // Run task immediately on first boot

      // Calculate sleep duration to wake up exactly at next full minute
      int sleepDuration = (60 - currentSecond);
      Serial.printf("💤 Sleeping for %d seconds to align with full minute...\n", sleepDuration);
      esp_sleep_enable_timer_wakeup(sleepDuration * 1000000);
      esp_deep_sleep_start();
    }
    else
    {
      Serial.println("❌ Failed to get time.");
    }
  }
}

void listenFromBLEAndUpdateJSON()
{
  if (isNewBLEDataAvailable())
  { // Check if new data is received
    String jsonData = getReceivedBLEData();
    Serial.println("💾 Saving received JSON to SPIFFS...");
    bool success = writeJsonFile("data.json", jsonData);
    Serial.println(success ? "✅ JSON saved successfully!" : "❌ Failed to save JSON.");
    Serial.println(jsonData);
  }
}

void loop()
{
  listenFromBLEAndUpdateJSON();
  // if (wifiManager.isTimeSynced()) {
  //     // Do something once time is synced, e.g., fetch events from calendar
  //     Serial.println("Time synced. Proceed with further actions.");
  // } else {
  //     // Handle the case where time is not synced
  //     Serial.println("Time not synced yet.");
  // }
  // struct tm timeinfo;
  // if (getLocalTime(&timeinfo))
  // {
  //   int currentMonth = timeinfo.tm_mon + 1; // tm_mon is 0-11, so we add 1 for correct month (1-12)
  //   int currentDay = timeinfo.tm_mday;

  //   Serial.println("📅 Current Date:");
  //   Serial.printf("Month: %d, Day: %d\n", currentMonth, currentDay);

  //   // Your code to fetch today's data or other tasks
  //   // calendarManager.fetchTodayData(currentMonth, currentDay);
  // }
  // else
  // {
  //   Serial.println("❌ Failed to get time");
  // }

  // // After running the task, the device will go to sleep again
  // setup(); // Call setup to re-enter deep sleep after the task
}
