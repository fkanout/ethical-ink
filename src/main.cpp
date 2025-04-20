

#include "SPIFFSHelper.h"
#include "WiFiManager.h"
#include <AppState.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEManager.h>
#include <CalendarManager.h>
#include <RTCManager.h>

CalendarManager calendarManager;

bool bootstrap() {
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Failed to mount SPIFFS");
    return false;
  }
  Serial.println("✅ SPIFFS mounted successfully");
  Serial.println("✅ Bootstrap successful");
  return true;
}

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

  int sleepDuration = 60 - currentSecond;
  Serial.printf("💤 Sleeping for %d seconds to align with full minute...\n",
                sleepDuration);
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  esp_deep_sleep_start();
}

void onWifiNetworksFound(const std::vector<ScanResult> &results) {
  Serial.println("📋 Wi-Fi networks:");
  String json = "[";
  for (size_t i = 0; i < results.size(); ++i) {
    const auto &net = results[i];
    String displaySSID = net.ssid.substring(0, 25);
    const char *security = net.secured ? "secured" : "open";

    // ✅ Print nicely formatted output
    Serial.printf("   📶 %-25s %5ddBm  %s\n", displaySSID.c_str(), net.rssi,
                  security);

    // ✅ Add to JSON
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

  // ✅ Send it over BLE
  BLEManager::getInstance().sendBLEData(json);
}
void onBLENotificationEnabled() {
  Serial.println("🔔 BLE notification enabled — main.cpp was notified!");
  WiFiManager &wifi = WiFiManager::getInstance();
  wifi.asyncScanNetworks(onWifiNetworksFound);
}
void onWiFiConnected(bool success) {
  if (success) {
    Serial.println("🎉 Wi‑Fi connected — syncing time...");

    RTCManager &rtc = RTCManager::getInstance();
    bool timeIsSynced = rtc.syncTimeFromNTPWithOffset(3, 10000);
    if (timeIsSynced) {
      Serial.println("✅ Time synced successfully");
    } else {
      Serial.println("❌ Failed to sync time");
    }
    RTCManager::getInstance().printTime();
    executeMainTask();
  } else {
    Serial.println("😓 Failed to connect to Wi‑Fi.");
  }
}
void onJsonReceivedCallback(const String &json) {
  Serial.println("📩 Received JSON over BLE: " + json);
  WiFiManager &wifi = WiFiManager::getInstance();

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, json);
  if (err) {
    Serial.println("❌ Invalid JSON format");
    return;
  }

  if (doc.containsKey("ssid")) {
    String ssid = doc["ssid"];
    String password = doc["password"];
    wifi.asyncConnect(ssid.c_str(), password.c_str(), onWiFiConnected);
  }
}
void setup() {
  Serial.begin(115200);
  delay(500);

  if (!bootstrap()) {
    Serial.println("❌ Bootstrap failed");
    return;
  }

  // splitCalendarJson(MOSQUE_FILE, true);

  RTCManager &rtc = RTCManager::getInstance();
  if (!rtc.isTimeSynced()) {
    Serial.println("❌ Time not synced");
    WiFiManager &wifi = WiFiManager::getInstance();
    String wifiJsonString = readJsonFile(WIFI_CRED_FILE);
    if (wifiJsonString.isEmpty() || wifiJsonString == "{}") {
      Serial.println("⚠️ No valid Wi-Fi credentials found.");
      BLEManager &ble = BLEManager::getInstance();
      ble.setupBLE();
      ble.startAdvertising();
      ble.setNotificationEnabledCallback(onBLENotificationEnabled);
      ble.setJsonReceivedCallback(onJsonReceivedCallback);
      return;
    }
    DynamicJsonDocument doc(256);
    DeserializationError errorParsingWifi =
        deserializeJson(doc, wifiJsonString);
    if (errorParsingWifi) {
      Serial.println("❌ JSON parse failed");
      wifi.asyncScanNetworks();
      return;
    } else {
      String ssid = doc["ssid"].as<String>();
      String password = doc["password"].as<String>();
      Serial.println("📂 Read JSON: " + wifiJsonString);
      wifi.asyncConnect(ssid.c_str(), password.c_str(), onWiFiConnected);
      return;
    }
  }

  executeMainTask();
}

void loop() { vTaskDelay(1000 / portTICK_PERIOD_MS); }