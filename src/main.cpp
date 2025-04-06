#include <Arduino.h>
#include "BLEManager.h"
#include "SPIFFSHelper.h"
#include "WiFiManager.h"
#include <CalendarManager.h>
#include <RTCManager.h>
#include <AppState.h>
#define RAW_JSON_FILE "/data.json"

AppState state = BOOTING;
unsigned long lastAttempt = 0;
bool bleStarted = false;
bool wifiAttempted = false;
CalendarManager calendarManager;

void executeMainTask()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("❌ Failed to get time.");
    return;
  }

  int currentSecond = timeinfo.tm_sec;
  PrayerTimeInfo prayerTimeInfo = calendarManager.getNextPrayerTimeForToday(
      timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);

  Serial.printf("🕒 %02d:%02d:%02d  📅 %02d/%02d/%04d\n",
                timeinfo.tm_hour, timeinfo.tm_min, currentSecond,
                timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

  Serial.println("🔔 Next prayer: " + prayerTimeInfo.nextPrayerMinAndHour);
  for (const String &prayer : prayerTimeInfo.prayerTimes)
  {
    Serial.println("    ⏰ " + prayer);
  }

  int sleepDuration = 60 - currentSecond;
  Serial.printf("💤 Sleeping for %d seconds to align with full minute...\n", sleepDuration);
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  esp_deep_sleep_start();
}

void setup()
{
  Serial.begin(115200);
  setCpuFrequencyMhz(80); // Lower CPU clock to 80 MHz
  Serial.printf("CPU Frequency: %d MHz\n", getCpuFrequencyMhz());

  setupSPIFFS();
  BLEManager::setupBLE();
  RTCManager::getInstance(); // Prepare RTC
  setenv("TZ", "CET-1", 1);  // Constant UTC+1, no DST
  tzset();
}

void loop()
{
  RTCManager *rtc = RTCManager::getInstance();

  switch (state)
  {
  case BOOTING:
    Serial.println("🚀 Booting...");
    state = CHECKING_TIME;
    break;

  case CHECKING_TIME:
    struct tm timeinfo;
    if (getLocalTime(&timeinfo) && rtc->isTimeSynced())
    {
      Serial.println("✅ Time already synced — skipping Wi-Fi.");
      state = RUNNING_MAIN_TASK;
    }
    else
    {
      Serial.println("❌ Time not synced — enabling Wi-Fi.");
      WiFiManager::getInstance()->autoConnectFromFile();
      wifiAttempted = true;
      state = CONNECTING_WIFI;
      lastAttempt = millis();
    }
    break;

  case CONNECTING_WIFI:
    if (!bleStarted)
    {
      BLEManager::restartBLE();
      Serial.println("📲 BLE advertising restarted");
      bleStarted = true;
    }
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("📡 Wi-Fi connected.");
      state = WAITING_FOR_TIME_SYNC;
    }
    else if (millis() - lastAttempt > 10000)
    {
      Serial.println("❌ Wi-Fi not connected. Retrying...");
      lastAttempt = millis();
    }
    break;

  case WAITING_FOR_TIME_SYNC:
    if (rtc->isTimeSynced())
    {
      Serial.println("✅ Time synced via Wi-Fi.");
      state = RUNNING_MAIN_TASK;
    }
    break;

  case RUNNING_MAIN_TASK:
    executeMainTask();
    state = SLEEPING;
    break;

  case SLEEPING:
    break; // Will never reach here; device goes to deep sleep
  }

  delay(200);
}