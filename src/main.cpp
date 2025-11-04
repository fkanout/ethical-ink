#include "AppStateManager.h"
#include "EventsManager.h"
#include "GxEPD2Adapter.h"
#include "SPIFFSHelper.h"
#include "ScreenUI.h"
#include "StatusBar.h"
#include "WiFiManager.h"
#include <AppState.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEManager.h>
#include <CalendarManager.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <GxEPD2_BW.h>
#include <MAWAQITManager.h>
#include <RTCManager.h>
#include <SPI.h>

// Pins for E-paper
#define EPD_CS 10
#define EPD_DC 9
#define EPD_RST 8
#define EPD_BUSY 7

// 7.5" b/w (800x480)
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT>
    display(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// ===== حالة الرسم (تعيش في RTC Slow Memory) =====
struct RenderState {
  bool initialized;
  int lastHighlight;
  char times[5][6]; // Fajr..Isha بصيغة "HH:MM"
};
// RTC slow memory state (optional)
RTC_DATA_ATTR RenderState g_renderState;

CalendarManager calendarManager;
unsigned int sleepDuration = 60; // in seconds, default fallback
const char *PRAYER_NAMES[] = {"Fajr", "Sunrise", "Dhuhr",
                              "Asr",  "Maghrib", "Isha"};

const unsigned long mosqueUpdateInterval = 6UL * 60UL * 60UL; // 6 hours
const unsigned long userEventsUpdateInterval = 10UL * 60UL;   // 10 minutes
bool isFetching = false;

AppState state = BOOTING;
AppState lastState = FETAL_ERROR;
String mosqueName = "Mosque Name"; // Default fallback name
// Add this global variable to track BLE advertising state
bool g_bleAdvertising = false;
// Global variables to store BLE-received credentials
String g_receivedSSID = "";
String g_receivedPassword = "";
String g_receivedMosqueUUID = "";

//--------------------------------------------------------------------------
bool shouldFetchBasedOnInterval(unsigned long lastUpdateSeconds,
                                unsigned long intervalSeconds,
                                const char *label = nullptr) {
  time_t now = RTCManager::getInstance().getEpochTime();
  if (now < 100000) {
    Serial.println("⚠️ Skipping fetch: RTC not synced yet.");
    return false;
  }

  // Align to the nearest minute
  unsigned long nowAligned = now - (now % 60);
  unsigned long lastAligned = lastUpdateSeconds - (lastUpdateSeconds % 60);

  if (lastUpdateSeconds == 0) {
    if (label)
      Serial.printf("🆕 [%s] No previous update. Should fetch.\n", label);
    return true;
  }

  unsigned long elapsed = nowAligned - lastAligned;

  if (elapsed >= intervalSeconds) {
    if (label) {
      Serial.printf("⏱️ [%s] It's been %lu seconds. Fetching now...\n", label,
                    elapsed);
    }
    return true;
  } else {
    if (label) {
      unsigned long remaining =
          intervalSeconds > elapsed ? intervalSeconds - elapsed : 0;
      unsigned long elapsedH = elapsed / 3600;
      unsigned long elapsedM = (elapsed % 3600) / 60;
      unsigned long remainingH = remaining / 3600;
      unsigned long remainingM = (remaining % 3600) / 60;
      Serial.printf(
          "🕰️ [%s] Only %luh %lum elapsed. Next fetch in %luh %lum.\n", label,
          elapsedH, elapsedM, remainingH, remainingM);
    }
    return false;
  }
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

bool isLaterThan(int hour, int minute, const String &timeStr) {
  int h = timeStr.substring(0, 2).toInt();
  int m = timeStr.substring(3, 5).toInt();

  if (h > hour)
    return true;
  if (h == hour && m > minute)
    return true;
  return false;
}

void executeMainTask() {
  setCpuFrequencyMhz(80);
  Serial.printf("⚙️ CPU now running at: %d MHz\n", getCpuFrequencyMhz());
  unsigned long startTime = millis();

  // Load mosque name from saved info file if available
  if (SPIFFS.exists("/mosque_info.json")) {
    File file = SPIFFS.open("/mosque_info.json", FILE_READ);
    if (file) {
      String json = file.readString();
      file.close();

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, json);
      if (!error && doc["name"].is<String>()) {
        mosqueName = doc["name"].as<String>();
        Serial.printf("📖 Loaded mosque name: %s\n", mosqueName.c_str());
      }
    }
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("❌ Failed to get time.");
    return;
  }

  int currentSecond = timeinfo.tm_sec;
  Serial.printf("🕒 %02d:%02d:%02d  📅 %02d/%02d/%04d\n", timeinfo.tm_hour,
                timeinfo.tm_min, currentSecond, timeinfo.tm_mday,
                timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

  TodayAndNextDayPrayerTimes todayAndNextDayPrayerTimes =
      calendarManager.fetchTodayAndNextDayPrayerTimes(timeinfo.tm_mon + 1,
                                                      timeinfo.tm_mday);

  std::vector<String> todayPrayer = todayAndNextDayPrayerTimes.todayPrayerTimes;
  std::vector<String> todayIqama = todayAndNextDayPrayerTimes.todayIqamaTimes;
  std::vector<String> nextDayPrayer =
      todayAndNextDayPrayerTimes.nextDayPrayerTimes;
  std::vector<String> nextDayIqama =
      todayAndNextDayPrayerTimes.nextDayIqamaTimes;

  bool isShowNextDayPrayers = false;
  struct NextPrayerInfo {
    String time;
    String name;
  } nextPrayerInfo;

  if (todayPrayer.empty() || todayIqama.empty() || nextDayPrayer.empty() ||
      nextDayIqama.empty()) {
    Serial.println("❌ Missing prayer times or iqama times!");
    return;
  }

  for (size_t i = 0; i < todayPrayer.size(); ++i) {
    if (isLaterThan(timeinfo.tm_hour, timeinfo.tm_min, todayPrayer[i])) {
      nextPrayerInfo.time = todayPrayer[i];
      nextPrayerInfo.name = PRAYER_NAMES[i];
      break;
    }
  }

  if (nextPrayerInfo.time.isEmpty() || nextPrayerInfo.name.isEmpty()) {
    nextPrayerInfo.time = nextDayPrayer[0];
    nextPrayerInfo.name = PRAYER_NAMES[0];
    isShowNextDayPrayers = true;
  }

  if (nextPrayerInfo.time.isEmpty() || nextPrayerInfo.name.isEmpty()) {
    Serial.println("❌ No next prayer found!");
    return;
  }

  String FAJR, SUNRISE, DHUHR, ASR, MAGHRIB, ISHA;
  String IQAMA_Fajr, IQAMA_Dhuhr, IQAMA_Asr, IQAMA_Maghrib, IQAMA_Isha;

  if (isShowNextDayPrayers) {
    FAJR = nextDayPrayer[0];
    SUNRISE = nextDayPrayer[1];
    DHUHR = nextDayPrayer[2];
    ASR = nextDayPrayer[3];
    MAGHRIB = nextDayPrayer[4];
    ISHA = nextDayPrayer[5];
    IQAMA_Fajr = nextDayIqama[0];
    IQAMA_Dhuhr = nextDayIqama[1];
    IQAMA_Asr = nextDayIqama[2];
    IQAMA_Maghrib = nextDayIqama[3];
    IQAMA_Isha = nextDayIqama[4];
  } else {
    FAJR = todayPrayer[0];
    SUNRISE = todayPrayer[1];
    DHUHR = todayPrayer[2];
    ASR = todayPrayer[3];
    MAGHRIB = todayPrayer[4];
    ISHA = todayPrayer[5];
    IQAMA_Fajr = todayIqama[0];
    IQAMA_Dhuhr = todayIqama[1];
    IQAMA_Asr = todayIqama[2];
    IQAMA_Maghrib = todayIqama[3];
    IQAMA_Isha = todayIqama[4];
  }

  Countdown countdown =
      calculateCountdownToNextPrayer(nextPrayerInfo.time, timeinfo);
  char countdownStr[16];
  sprintf(countdownStr, "%02d:%02d", countdown.hours, countdown.minutes);

  String title = String("✨══════•••••• Prayer times for ") +
                 (isShowNextDayPrayers ? "tomorrow" : "today") +
                 " ••••••══════✨";

  Serial.println(title.c_str());
  Serial.printf("  ⏰ %s  %s\n", FAJR.c_str(), IQAMA_Fajr.c_str());
  Serial.printf("  🌅 %s\n", SUNRISE.c_str());
  Serial.printf("  ⏰ %s  %s\n", DHUHR.c_str(), IQAMA_Dhuhr.c_str());
  Serial.printf("  ⏰ %s  %s\n", ASR.c_str(), IQAMA_Asr.c_str());
  Serial.printf("  ⏰ %s  %s\n", MAGHRIB.c_str(), IQAMA_Maghrib.c_str());
  Serial.printf("  ⏰ %s  %s\n", ISHA.c_str(), IQAMA_Isha.c_str());
  Serial.printf("  ⏳%s in %02d:%02d\n", nextPrayerInfo.name.c_str(),
                countdown.hours, countdown.minutes);
  Serial.println("✨══════•••••••••••••••••••••••••••••••••••══════✨");

  // ---------- E-paper display with status bar ----------
  const char *PRAYER_NAMES_ROW[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
  const char *prayerTimesRow[5] = {FAJR.c_str(), DHUHR.c_str(), ASR.c_str(),
                                   MAGHRIB.c_str(), ISHA.c_str()};

  // NEW: Create iqama times array (Note: No Sunrise iqama, so we skip index 1)
  const char *iqamaTimesRow[5] = {IQAMA_Fajr.c_str(), IQAMA_Dhuhr.c_str(),
                                  IQAMA_Asr.c_str(), IQAMA_Maghrib.c_str(),
                                  IQAMA_Isha.c_str()};

  GxEPD2Adapter<decltype(display)> epdAdapter(display);
  ScreenUI ui(epdAdapter, /*screenW*/ 800, /*screenH*/ 480);
  ScreenLayout L = ui.computeLayout();

  int highlightIndex = ScreenUI::getNextPrayerIndex(
      prayerTimesRow, timeinfo.tm_hour, timeinfo.tm_min);

  // Get status information
  StatusInfo statusInfo = StatusBar::getStatusInfo(g_bleAdvertising);

  // Use the fetched mosque name, fallback to static name if it contains mostly
  // non-ASCII (Arabic) characters
  String displayName = mosqueName;

  // Count displayable vs non-displayable characters
  int asciiCount = 0;
  int totalCount = displayName.length();

  for (int i = 0; i < totalCount; i++) {
    unsigned char c = displayName[i];
    // Check if character is ASCII letter or number
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
        (c >= '0' && c <= '9')) {
      asciiCount++;
    }
  }

  // If less than 20% of characters are ASCII letters/numbers, use fallback
  if (totalCount == 0 || asciiCount < (totalCount * 0.2)) {
    displayName = "HAMA City - Syria";
    Serial.println("📝 Using fallback mosque name (Arabic not displayable)");
  }

  const char *MOSQUE_NAME = displayName.c_str();

  if (!g_renderState.initialized) {
    // UPDATED: Pass iqama times to fullRender
    ui.fullRenderWithStatusBar(L, MOSQUE_NAME, countdownStr, PRAYER_NAMES_ROW,
                               prayerTimesRow, iqamaTimesRow, highlightIndex,
                               statusInfo);
  } else {
    // UPDATED: Pass iqama times to partialRender
    ui.partialRenderWithStatusBar(L, MOSQUE_NAME, countdownStr,
                                  PRAYER_NAMES_ROW, prayerTimesRow,
                                  iqamaTimesRow, highlightIndex, statusInfo);
  }

  // persist (optional)
  g_renderState.initialized = true;
  g_renderState.lastHighlight = highlightIndex;
  for (int i = 0; i < 5; ++i) {
    snprintf(g_renderState.times[i], sizeof g_renderState.times[i], "%s",
             prayerTimesRow[i]);
  }
}

//-------------------------end main execute-------------------------------------

// Helper function to get mosque UUID from config file
String getMosqueUUID() {
  const char *DEFAULT_UUID = "f9a51508-05b7-4324-a7e8-4acbc2893c02";

  String configJson = readJsonFile("/mosque_config.json");
  if (configJson.isEmpty() || configJson == "{}") {
    Serial.println("⚠️ No mosque config found, using default UUID");
    return String(DEFAULT_UUID);
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, configJson);
  if (error) {
    Serial.println("❌ Failed to parse mosque config, using default UUID");
    return String(DEFAULT_UUID);
  }

  String uuid = doc["mosque_uuid"].as<String>();
  if (uuid.isEmpty()) {
    Serial.println("⚠️ Empty UUID in config, using default");
    return String(DEFAULT_UUID);
  }

  Serial.printf("✅ Using mosque UUID: %s\n", uuid.c_str());
  return uuid;
}

void fetchPrayerTimesIfDue() {
  Serial.println("📡 Fetching prayer times and mosque info from MAWAQIT...");

  // Check if mosque UUID is set
  if (strlen(rtcData.mosqueUUID) == 0) {
    Serial.println(
        "⚠️ No mosque UUID configured. Please configure via BLE first.");
    state = SLEEPING;
    return;
  }

  Serial.printf("🕌 Using mosque UUID: %s", rtcData.mosqueUUID);

  WiFiManager::getInstance().asyncConnectWithSavedCredentials();

  WiFiManager::getInstance().onWifiFailedToConnectCallback([]() {
    Serial.println(
        "❌ Failed to connect to Wi-Fi to fetch prayer times if due");
    state = SLEEPING;
  });

  WiFiManager::getInstance().onWifiConnectedCallback([]() {
    Serial.println("✅ Connected to Wi-Fi for MAWAQIT fetch.");
    MAWAQITManager::getInstance().setApiKey(
        "86ed48fd-691e-4370-a9bf-ae74f788ed54");

    // Get mosque UUID from config file
    String mosqueUUID = getMosqueUUID();

    // First fetch mosque info to get the name
    MAWAQITManager::getInstance().asyncFetchMosqueInfo(
        mosqueUUID, [mosqueUUID](bool success, const char *name) {
          if (success && name) {
            mosqueName = String(name);
            Serial.printf("🕌 Updated mosque name: %s\n", mosqueName.c_str());
          } else {
            Serial.println(
                "⚠️ Failed to fetch mosque info. Using default name.");
          }

          // Now fetch prayer times using the same UUID
          MAWAQITManager::getInstance().asyncFetchPrayerTimes(
              mosqueUUID, [](bool success, const char *path) {
                if (success) {
                  Serial.printf("📂 Valid prayer times file ready at: %s\n",
                                path);
                  splitCalendarJson(MOSQUE_FILE);
                  splitCalendarJson(MOSQUE_FILE, true);
                  rtcData.mosqueLastUpdateMillis =
                      RTCManager::getInstance().getEpochTime();
                  AppStateManager::save();
                } else {
                  Serial.println("⚠️ Failed to fetch valid data after retries.");
                }
                state = SLEEPING;
              });
        });
  });
}

void setup() {
  Serial.begin(115200);
  delay(300);
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();
  state = BOOTING;
}

void handleBooting() {
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Failed to mount SPIFFS");
    state = FETAL_ERROR;
  }
  Serial.println("✅ SPIFFS mounted successfully");
  AppStateManager::load();
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
    // state = CONNECTING_WIFI_WITH_SAVED_CREDENTIALS;
    state = ADVERTISING_BLE; // Use BLE for WiFi setup (comment this to use
                             // saved credentials)
  }
}

void handleConnectingWifiWithSavedCredentials() {
  Serial.println("🔄 Connecting to Wi-Fi...");
  WiFiManager &wifi = WiFiManager::getInstance();
  wifi.asyncConnectWithSavedCredentials();
  wifi.onWifiConnectedCallback([]() {
    Serial.println("✅ Wi-Fi connected");
    BLEManager::getInstance().stopAdvertising();
    g_bleAdvertising = false; // Clear BLE advertising flag
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
    // rtc.setTimeToSpecificHourAndMinute(20, 07, 5, 2); // for testing time
    state = RUNNING_MAIN_TASK;
  } else {
    Serial.println("❌ Failed to sync time");
    state = ADVERTISING_BLE;
  }
}

void handleAdvertisingBLE() {
  Serial.println("🔔 Advertising BLE...");

  // Display initialization message on e-ink screen
  GxEPD2Adapter<decltype(display)> epdAdapter(display);
  ScreenUI ui(epdAdapter, /*screenW*/ 800, /*screenH*/ 480);
  ui.showInitializationScreen();

  BLEManager &ble = BLEManager::getInstance();
  ble.setupBLE();
  g_bleAdvertising = true; // Set BLE advertising flag
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
      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, json);
      if (err) {
        Serial.println("❌ Invalid JSON format");
        state = ADVERTISING_BLE;
        return;
      }

      String ssid = doc["ssid"].as<String>();
      String password = doc["password"].as<String>();
      String mosqueUuid = doc["mosque_uuid"].as<String>();

      if (ssid.isEmpty() || password.isEmpty()) {
        Serial.println("⚠️ Incomplete Wi-Fi credentials.");
        state = ADVERTISING_BLE;
        return;
      }

      // Store credentials in global variables (defer write operations to avoid
      // stack overflow)
      g_receivedSSID = ssid;
      g_receivedPassword = password;
      g_receivedMosqueUUID = mosqueUuid;

      Serial.printf("✅ Received WiFi: %s, Mosque UUID: %s\n", ssid.c_str(),
                    mosqueUuid.isEmpty() ? "not provided" : mosqueUuid.c_str());

      // Transition to connecting state
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

  // Use the globally stored credentials
  wifi.asyncConnect(g_receivedSSID.c_str(), g_receivedPassword.c_str());

  wifi.onWifiConnectedCallback([]() {
    Serial.println("✅ Wi-Fi connected");

    // Now it's safe to write to SPIFFS and RTC (we're in the main loop context
    // after connection)
    if (!g_receivedMosqueUUID.isEmpty()) {
      // Save to RTC memory (primary storage)
      strncpy(rtcData.mosqueUUID, g_receivedMosqueUUID.c_str(),
              sizeof(rtcData.mosqueUUID) - 1);
      rtcData.mosqueUUID[sizeof(rtcData.mosqueUUID) - 1] =
          '\0'; // Ensure null termination
      AppStateManager::save();
      Serial.printf("💾 Mosque UUID saved to RTC: %s\n", rtcData.mosqueUUID);

      // Also save to SPIFFS for backup
      String mosqueConfigJson =
          "{\"mosque_uuid\":\"" + g_receivedMosqueUUID + "\"}";
      writeJsonFile("/mosque_config.json", mosqueConfigJson);
      Serial.printf("💾 Mosque UUID saved to SPIFFS: %s\n",
                    g_receivedMosqueUUID.c_str());
    }

    BLEManager::getInstance().stopAdvertising();
    g_bleAdvertising = false; // Clear BLE advertising flag
    state = SYNCING_TIME;
  });

  wifi.onWifiFailedToConnectCallback([]() {
    Serial.println("❌ Failed to connect to Wi-Fi");
    state = ADVERTISING_BLE;
  });
}

// void handleSleeping() {
//   struct tm timeinfo;
//   if (getLocalTime(&timeinfo)) {
//     int currentSecond = timeinfo.tm_sec;
//     sleepDuration = 60 - currentSecond;
//     if (sleepDuration == 60) {
//       sleepDuration = 0; // edge case
//     }
//   } else {
//     Serial.println(
//         "⚠️ Failed to get time for sleep alignment. Using 60s fallback.");
//     sleepDuration = 60;
//   }

//   Serial.printf("💤 Sleeping for %d seconds to align with full minute...\n",
//                 sleepDuration);
//   esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
//   esp_deep_sleep_start();
// }

void handleSleeping() {
  // Compute seconds to next full minute
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int currentSecond = timeinfo.tm_sec;
    sleepDuration = 60 - currentSecond;
    if (sleepDuration == 60)
      sleepDuration = 0; // edge case
  } else {
    Serial.println(
        "⚠️ Failed to get time for sleep alignment. Using 60s fallback.");
    sleepDuration = 60;
  }

  // Announce BEFORE sleeping
  Serial.printf("💤 Entering LIGHT SLEEP for %d seconds to align with next "
                "full minute...\n",
                sleepDuration);
  Serial.flush();
  delay(20); // give UART time to drain

  // Sleep & wake
  esp_sleep_enable_timer_wakeup((uint64_t)sleepDuration * 1000000ULL);
  esp_light_sleep_start();

  // After wake
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("⏰ Woke from light sleep by timer.");
  } else {
    Serial.printf("⏰ Woke from light sleep (cause=%d).\n", (int)cause);
  }

  // Continue with main task next
  state = RUNNING_MAIN_TASK;
}

void handleMainTaskState() {
  Serial.println("⚙️ Running main task...");
  executeMainTask();
  state = RUNNING_PERIODIC_TASKS;
}

void fetchUserEvents() {
  Serial.println("📡 Fetching user events...");
  WiFiManager::getInstance().asyncConnectWithSavedCredentials();

  WiFiManager::getInstance().onWifiFailedToConnectCallback([]() {
    Serial.println("❌ Failed to connect to Wi-Fi to fetch user events");
    // Track this state to show in the UI and stop keeping the device awake each
    // time
    state = SLEEPING;
  });
  WiFiManager::getInstance().onWifiConnectedCallback([]() {
    Serial.println("✅ Connected to Wi-Fi to fetch user events.");
    String accessToken =
        "ya29.a0AZYkNZgcd6d9N5GzdATE3EZ9ssp2J1nYDHgHrKywEEYy7O3-6kJetSSBuifYF_"
        "CTpLwa9SOJEpoKFk_"
        "hGmRwvYfeqLj1PpyhvbHRy4aTiZtTUjUkDDutqHrRKefaxj4BOr6yuoSx-"
        "xizcEoiNphHQNz0ci6EbuCtjG6frLaPaCgYKATkSARMSFQHGX2MiKjV9ydIfVHvXVcOt6P"
        "l8VA0175";
    EventsManager::getInstance().setAccessToken(accessToken);
    EventsManager::getInstance().asyncFetchEvents(
        [](bool success, const char *path) {
          if (success) {
            rtcData.userEventsUpdateMillis =
                RTCManager::getInstance().getEpochTime();
            AppStateManager::save();
          } else {
            Serial.println("⚠️ Calendar fetch failed");
          }
          state = SLEEPING;
        });
  });
}

void handlePeriodicTasks() {
  Serial.println("🔄 Running periodic tasks...");
  // if (shouldFetchBasedOnInterval(rtcData.userEventsUpdateMillis,
  //                                userEventsUpdateInterval, "USER_EVENTS")) {
  //  // fetchUserEvents();
  //   return;
  // }
  // Serial.println("⚠️ User events do not need to be fetched.");
  if (shouldFetchBasedOnInterval(rtcData.mosqueLastUpdateMillis,
                                 mosqueUpdateInterval, "MOSQUE_DATA")) {
    fetchPrayerTimesIfDue();
    return;
  }
  Serial.println("⚠️ Mosque data does not need to be fetched.");

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
  case RUNNING_PERIODIC_TASKS:
    handlePeriodicTasks();
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
