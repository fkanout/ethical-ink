

#include "SPIFFSHelper.h"
#include "WiFiManager.h"
#include <AppState.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEManager.h>
#include <CalendarManager.h>
#include <MAWAQITManager.h>
#include <RTCManager.h>

CalendarManager calendarManager;
unsigned int sleepDuration = 60; // in seconds, default fallback
RTC_DATA_ATTR unsigned long lastUpdateMillis =
    0; // stores last successful MAWAQIT fetch (UTC time)
const unsigned long updateInterval = 6UL * 60UL * 60UL * 1000UL; // 6 hours
bool isFetching = false;
AppState state = BOOTING;
AppState lastState = FETAL_ERROR;
Countdown calculateCountdownToNextPrayer(const String &nextPrayer,
                                         const struct tm &now) {
  int currentSeconds = now.tm_hour * 3600 + now.tm_min * 60 + now.tm_sec;

  int prayerHour = nextPrayer.substring(0, 2).toInt();
  int prayerMin = nextPrayer.substring(3, 5).toInt();
  int prayerSeconds = prayerHour * 3600 + prayerMin * 60;

  int diffSeconds = prayerSeconds - currentSeconds;
  if (diffSeconds < 0) {
    diffSeconds += 24 * 3600; // Next day
  }

  // ✅ UX Adjustment: subtract a full minute if we're *exactly* on a new minute
  // (because display won't update until a minute later)
  if (now.tm_sec == 0) {
    diffSeconds -= 60;
  }

  // Prevent negative values (in case diff was exactly 0)
  if (diffSeconds < 0) {
    diffSeconds = 0;
  }

  Countdown result;
  result.hours = diffSeconds / 3600;
  result.minutes = (diffSeconds % 3600) / 60;
  return result;
}

void executeMainTask() {

  setCpuFrequencyMhz(80);
  Serial.printf("⚙️ CPU now running at: %d MHz\n", getCpuFrequencyMhz());
  unsigned long startTime = millis();

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to get time.");
    return;
  }

  int currentSecond = timeinfo.tm_sec;
  PrayerTimeInfo prayerTimeInfo = calendarManager.getNextPrayerTimeForToday(
      timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min);

  Serial.printf("🕒 %02d:%02d:%02d  📅 %02d/%02d/%04d\n", timeinfo.tm_hour,
                timeinfo.tm_min, currentSecond, timeinfo.tm_mday,
                timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

  if (prayerTimeInfo.prayerTimes.empty() || prayerTimeInfo.iqamaTimes.empty() ||
      prayerTimeInfo.prayerTimes.size() < 6) {
    Serial.println("🐞 Issue with prayer times.");
    return;
  }

  Countdown countdown = calculateCountdownToNextPrayer(
      prayerTimeInfo.nextPrayerMinAndHour, timeinfo);

  String sunrise = prayerTimeInfo.prayerTimes[1];
  prayerTimeInfo.prayerTimes.erase(prayerTimeInfo.prayerTimes.begin() + 1);

  Serial.println("---------------------------");
  for (size_t i = 0; i < prayerTimeInfo.prayerTimes.size(); ++i) {
    const String &prayer = prayerTimeInfo.prayerTimes[i];
    const String &iqama = prayerTimeInfo.iqamaTimes[i];
    Serial.printf("  ⏰ %s  %s\n", prayer.c_str(), iqama.c_str());
  }
  Serial.printf("⏳ Next prayer in %02d:%02d\n", countdown.hours,
                countdown.minutes);
  Serial.println("🔔 Next prayer: " + prayerTimeInfo.nextPrayerMinAndHour);
  Serial.println("🌅 Sunrise: " + sunrise);
  Serial.println("---------------------------");

  sleepDuration = 60 - currentSecond;
  unsigned long duration = millis() - startTime;
  Serial.printf("⏱️ Main task completed in %lu ms\n", duration);
  state = SLEEPING;
}
void fetchPrayerTimesIfDue() {
  // if (isFetching) {
  //   Serial.println("⏳ Still waiting for MAWAQIT fetch to complete...");
  //   return;
  // }

  // isFetching = true;
  // Serial.println("📡 Fetching prayer times from MAWAQIT...");
  // WiFiManager::getInstance().asyncConnectWithSavedCredentials([](bool
  // success) {
  //   if (!success) {
  //     Serial.println("❌ Failed to connect to Wi-Fi for MAWAQIT fetch.");
  //     isFetching = false;
  //     Serial.println("Resume main task, will retry later...");
  //     executeMainTask();
  //     return;
  //   }
  //   Serial.println("✅ Connected to Wi-Fi for MAWAQIT fetch.");
  //   MAWAQITManager::getInstance().setApiKey(
  //       "86ed48fd-691e-4370-a9bf-ae74f788ed54");
  //   MAWAQITManager::getInstance().asyncFetchPrayerTimes(
  //       "f9a51508-05b7-4324-a7e8-4acbc2893c02",
  //       [](bool success, const char *path) {
  //         isFetching = false;
  //         lastUpdateMillis = millis();

  //         if (success) {
  //           Serial.printf("📂 Valid prayer times file ready at: %s\n",
  //           path); splitCalendarJson(MOSQUE_FILE);
  //           splitCalendarJson(MOSQUE_FILE, true);
  //         } else {
  //           Serial.println("⚠️ Failed to fetch valid data after retries.");
  //         }
  //         // ✅ Once done, resume main task (like sleep)
  //         executeMainTask();
  //       });
  // });
}

void setup() {
  Serial.begin(115200);
  delay(300);
  state = BOOTING;
}

void handleBooting() {
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Failed to mount SPIFFS");
    state = FETAL_ERROR;
  }
  Serial.println("✅ SPIFFS mounted successfully");
  state = CHECKING_TIME;
}

void handleCheckingTime() {
  Serial.println("⏱️ Checking time...");
  RTCManager &rtc = RTCManager::getInstance();
  if (rtc.isTimeSynced()) {
    Serial.println("✅ Time synced");
    state = RUNNING_MAIN_TASK;
  } else {
    Serial.println("❌ Time not synced");
    state = CONNECTING_WIFI_WITH_SAVED_CREDENTIALS;
  }
}

void handleConnectingWifiWithSavedCredentials() {
  Serial.println("🔄 Connecting to Wi-Fi...");
  WiFiManager &wifi = WiFiManager::getInstance();
  wifi.asyncConnectWithSavedCredentials();
  wifi.onWifiConnectedCallback([]() {
    Serial.println("✅ Wi-Fi connected");
    BLEManager::getInstance().stopAdvertising();
    state = SYNCING_TIME;
  });
  wifi.onWifiFailedToConnectCallback([]() {
    Serial.println("❌ Failed to connect to Wi-Fi");
    state = ADVERTISING_BLE;
  });
}

void handleSyncingTime() {
  Serial.println("🔄 Syncing time...");
  RTCManager &rtc = RTCManager::getInstance();
  if (rtc.syncTimeFromNTPWithOffset(3, 10000)) {
    Serial.println("✅ Time synced successfully");
    state = RUNNING_MAIN_TASK;
  } else {
    Serial.println("❌ Failed to sync time");
    state = ADVERTISING_BLE;
  }
}

void handleAdvertisingBLE() {
  Serial.println("🔔 Advertising BLE...");
  BLEManager &ble = BLEManager::getInstance();
  ble.setupBLE();
  ble.onNotificationEnabled([]() {
    Serial.println("🔔 BLE notification enabled");
    state = WAITING_FOR_WIFI_SCAN;
  });
}

void handleWaitingForWifiScan() {
  Serial.println("🔄 Waiting for Wi-Fi scan...");
  WiFiManager &wifi = WiFiManager::getInstance();
  wifi.asyncScanNetworks();
  wifi.setScanResultCallback([](const std::vector<ScanResult> &results) {
    Serial.println("📋 Wi-Fi scan results:");
    String json = "[";
    for (size_t i = 0; i < results.size(); ++i) {
      const auto &net = results[i];
      String displaySSID = net.ssid.substring(0, 25);
      const char *security = net.secured ? "secured" : "open";
      Serial.printf("   📶 %-25s %5ddBm  %s\n", displaySSID.c_str(), net.rssi,
                    security);
      json += "{";
      json += "\"ssid\":\"" + displaySSID + "\",";
      json += "\"rssi\":" + String(net.rssi) + ",";
      json += "\"secured\":" + String(net.secured ? "true" : "false");
      json += "}";
      if (i < results.size() - 1) {
        json += ",";
      }
    }
    json += "]";
    BLEManager::getInstance().onNotificationEnabled(
        []() { handleWaitingForWifiScan(); });
    BLEManager::getInstance().sendBLEData(json);
    BLEManager::getInstance().onJsonReceived([](const String &json) {
      Serial.println("📩 Received JSON over BLE: " + json);
      WiFiManager &wifi = WiFiManager::getInstance();
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, json);
      if (err) {
        Serial.println("❌ Invalid JSON format");
        state = ADVERTISING_BLE;
        return;
      }
      String ssid = doc["ssid"].as<String>();
      String password = doc["password"].as<String>();
      if (ssid.isEmpty() || password.isEmpty()) {
        Serial.println("⚠️ Incomplete Wi-Fi credentials.");
        state = ADVERTISING_BLE;
        return;
      }
      wifi.asyncConnect(ssid.c_str(), password.c_str());
      state = CONNECTING_WIFI;
    });
  });
}

void handleUseWifiInput() {
  Serial.println("🔄 Waiting for user Wi-Fi input...");
  BLEManager &ble = BLEManager::getInstance();
}
void handleConnectingWifi() {
  Serial.println("🔄 Connecting to Wi-Fi...");
  WiFiManager &wifi = WiFiManager::getInstance();
  wifi.onWifiConnectedCallback([]() {
    Serial.println("✅ Wi-Fi connected");
    BLEManager::getInstance().stopAdvertising();
    state = SYNCING_TIME;
  });
  wifi.onWifiFailedToConnectCallback([]() {
    Serial.println("❌ Failed to connect to Wi-Fi");
    state = ADVERTISING_BLE;
  });
}

void handleSleeping() {
  Serial.printf("💤 Sleeping for %d seconds to align with full minute...\n",
                sleepDuration);
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  esp_deep_sleep_start();
}

void handleMainTaskState() {
  Serial.println("⚙️ Running main task...");
  executeMainTask();
  state = SLEEPING;
}

void handleAppState() {
  switch (state) {
  case BOOTING:
    handleBooting();
    break;
  case CHECKING_TIME:
    handleCheckingTime();
    break;
  case CONNECTING_WIFI:
    handleConnectingWifi();
    break;
  case CONNECTING_WIFI_WITH_SAVED_CREDENTIALS:
    handleConnectingWifiWithSavedCredentials();
    break;
  case SYNCING_TIME:
    handleSyncingTime();
    break;
  case ADVERTISING_BLE:
    handleAdvertisingBLE();
    break;
  case WAITING_FOR_WIFI_SCAN:
    handleWaitingForWifiScan();
    break;
  case RUNNING_MAIN_TASK:
    handleMainTaskState();
    break;
  case SLEEPING:
    handleSleeping();
    break;
  case FETAL_ERROR:
    break;
  }
}

void loop() {
  if (state != lastState) {
    Serial.printf("🔁 State changed: %d ➡️ %d\n", lastState, state);
    lastState = state;
    handleAppState();
  }

  vTaskDelay(100 / portTICK_PERIOD_MS);
}