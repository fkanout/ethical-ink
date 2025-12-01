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
// MAWAQIT API - Commented out, replaced with alternative API
// #include <MAWAQITManager.h>
#include <AladhanManager.h>
#include <RTCManager.h>
#include <SPI.h>
#include <esp_wifi.h>
#include <WeatherManager.h>
#include <AppStateManager.h>

// Pins for E-paper
#define EPD_CS 10
#define EPD_DC 9
#define EPD_RST 8
#define EPD_BUSY 7

// Factory reset button (built-in BOOT button on ESP32)
#define FACTORY_RESET_BUTTON 0 // GPIO0 - BOOT button

// 7.5" b/w (800x480)
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT>
    display(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// ===== حالة الرسم (تعيش في RTC Slow Memory) =====
struct RenderState {
  bool initialized;
  int lastHighlight;
  char times[5][6];       // Fajr..Isha بصيغة "HH:MM"
  char nextPrayerTime[6]; // HH:MM format for calculating countdown in sleep
};
// RTC slow memory state (optional)
RTC_DATA_ATTR RenderState g_renderState;

CalendarManager calendarManager;
unsigned int sleepDuration = 60; // in seconds, default fallback
const char *PRAYER_NAMES[] = {"Fajr", "Sunrise", "Dhuhr",
                              "Asr",  "Maghrib", "Isha"};

const unsigned long mosqueUpdateInterval = 6UL * 60UL * 60UL; // 6 hours
const unsigned long userEventsUpdateInterval = 10UL * 60UL;   // 10 minutes
const unsigned long weatherUpdateInterval = 30UL * 60UL;      // 30 minutes
bool isFetching = false;

AppState state = BOOTING;
AppState lastState = FETAL_ERROR;
// Add this global variable to track BLE advertising state
bool g_bleAdvertising = false;
// Global variables to store BLE-received credentials
String g_receivedSSID = "";
String g_receivedPassword = "";
String g_receivedMosqueUUID = ""; // Legacy, kept for backward compatibility
int g_receivedTimezoneOffset = 0;
// NEW: Location-based prayer times configuration
float g_receivedLatitude = 0.0;
float g_receivedLongitude = 0.0;
int g_receivedCalculationMethod = 4; // Default: Umm Al-Qura (Makkah)
// WiFi connection retry counter
int g_wifiRetryCount = 0;
const int MAX_WIFI_RETRIES = 3; // After 3 failed attempts, fall back to BLE

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

  // Use city name from RTC memory instead of mosque name
  String displayLocationName = String(rtcData.cityName);
  if (displayLocationName.isEmpty()) {
    // Fallback to coordinates if no city name
    displayLocationName = String(rtcData.latitude, 2) + "°, " + String(rtcData.longitude, 2) + "°";
  }
  Serial.printf("📍 Location: %s\n", displayLocationName.c_str());

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
  Serial.println("✨══════•••••••••••••••••••••••••••••••••══════✨");

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

  // Get status information (with awake time counter and cycles)
  StatusInfo statusInfo = StatusBar::getStatusInfo(g_bleAdvertising, rtcData.cumulativeAwakeSeconds, rtcData.wakeCycleCount);

  // Use city name instead of mosque name
  String displayName = displayLocationName;

  // Count displayable characters (Latin script + numbers + punctuation)
  int displayableCount = 0;
  int totalCount = displayName.length();

  for (int i = 0; i < totalCount; i++) {
    unsigned char c = displayName[i];
    // Accept: A-Z, a-z, 0-9, space, comma, period, dash, apostrophe, 
    // extended ASCII (128-255) for accented letters (é, ñ, etc.)
    // and degree symbol (176/°)
    if ((c >= 32 && c <= 126) ||  // Standard ASCII printable
        (c >= 128 && c <= 255)) {  // Extended ASCII (accented letters)
      displayableCount++;
    }
  }

  // If less than 50% of characters are displayable, it's likely non-Latin script
  if (totalCount == 0 || displayableCount < (totalCount * 0.5)) {
    displayName = String(rtcData.latitude, 2) + "°, " + String(rtcData.longitude, 2) + "°";
    Serial.println("📝 Using coordinates as location name (non-Latin script not displayable)");
  } else {
    Serial.printf("📝 Using location name: %s\n", displayName.c_str());
  }

  const char *LOCATION_NAME = displayName.c_str();

  if (!g_renderState.initialized) {
    // First time: Full render
    ui.fullRenderWithStatusBar(L, LOCATION_NAME, countdownStr, PRAYER_NAMES_ROW,
                               prayerTimesRow, iqamaTimesRow, highlightIndex,
                               statusInfo);
  } else if (g_renderState.lastHighlight != highlightIndex) {
    // Prayer changed: Do full refresh
    Serial.println("🔄 Prayer changed - doing full refresh");
    ui.fullRenderWithStatusBar(L, LOCATION_NAME, countdownStr, PRAYER_NAMES_ROW,
                               prayerTimesRow, iqamaTimesRow, highlightIndex,
                               statusInfo);
  } else {
    // Same prayer: Only update countdown and status bar (minimal partial
    // refresh)
    Serial.println("⏱️ Same prayer - only updating countdown");
    ui.partialRenderWithStatusBar(L, LOCATION_NAME, countdownStr,
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
  // Store next prayer time for countdown calculation in sleep handler
  snprintf(g_renderState.nextPrayerTime, sizeof g_renderState.nextPrayerTime,
           "%s", nextPrayerInfo.time.c_str());
}

//-------------------------end main execute-------------------------------------

// ============================================================================
// MAWAQIT API FUNCTIONS - COMMENTED OUT
// Replaced with alternative prayer times API
// ============================================================================

/*
// Helper function to get mosque UUID from config file
String getMosqueUUID() {
  String configJson = readJsonFile("/mosque_config.json");
  if (configJson.isEmpty() || configJson == "{}") {
    Serial.println("⚠️ No mosque config found");
    return String("");
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, configJson);
  if (error) {
    Serial.println("❌ Failed to parse mosque config");
    return String("");
  }

  String uuid = doc["mosque_uuid"].as<String>();
  if (uuid.isEmpty()) {
    Serial.println("⚠️ Empty UUID in config");
    return String("");
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
*/

// ============================================================================
// NEW PRAYER TIMES API - Aladhan Implementation
// ============================================================================

void fetchPrayerTimesFromAladhan() {
  Serial.println("📡 Fetching prayer times from Aladhan API...");

  // Check if location is configured
  if (rtcData.latitude == 0.0 && rtcData.longitude == 0.0) {
    Serial.println("⚠️ No location configured. Please configure via BLE first.");
    state = SLEEPING;
    return;
  }

  Serial.printf("📍 Location: %.6f, %.6f (Method: %d)\n", 
                rtcData.latitude, rtcData.longitude, rtcData.calculationMethod);

  WiFiManager::getInstance().asyncConnectWithSavedCredentials();

  WiFiManager::getInstance().onWifiFailedToConnectCallback([]() {
    Serial.println("❌ Failed to connect to Wi-Fi to fetch prayer times (will retry in 6 hours)");
    // Reset retry count to prevent device restart during periodic tasks
    rtcData.wifiRetryCount = 0;
    AppStateManager::save();
    state = SLEEPING;
  });

  WiFiManager::getInstance().onWifiConnectedCallback([]() {
    Serial.println("✅ Connected to Wi-Fi for Aladhan fetch.");
    
    // Reset retry count on successful connection during periodic tasks
    rtcData.wifiRetryCount = 0;
    AppStateManager::save();

    // Get current date for fetching
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
      Serial.println("❌ Failed to get time for prayer times fetch");
      state = SLEEPING;
      return;
    }

    int currentMonth = timeinfo.tm_mon + 1;
    int currentYear = timeinfo.tm_year + 1900;

    Serial.printf("📅 Fetching prayer times for %d/%d\n", currentMonth, currentYear);

    // Fetch current month
    AladhanManager::getInstance().asyncFetchMonthlyPrayerTimes(
        rtcData.latitude, rtcData.longitude, rtcData.calculationMethod,
        currentMonth, currentYear,
        [currentMonth, currentYear](bool success, const char *path) {
          if (success) {
            Serial.printf("✅ Prayer times for %d/%d fetched successfully\n",
                          currentMonth, currentYear);
            
            // Files are already split and saved by AladhanManager
            // No need to call splitCalendarJson
            
            // Update last fetch time
            rtcData.mosqueLastUpdateMillis =
                RTCManager::getInstance().getEpochTime();
            AppStateManager::save();

            // Fetch next month if we're in the last week
            struct tm timeinfo;
            if (getLocalTime(&timeinfo) && timeinfo.tm_mday >= 24) {
              int nextMonth = currentMonth + 1;
              int nextYear = currentYear;
              if (nextMonth > 12) {
                nextMonth = 1;
                nextYear++;
              }

              Serial.printf("📅 Also fetching next month: %d/%d\n", nextMonth, nextYear);
              
              AladhanManager::getInstance().asyncFetchMonthlyPrayerTimes(
                  rtcData.latitude, rtcData.longitude, rtcData.calculationMethod,
                  nextMonth, nextYear,
                  [nextMonth, nextYear](bool success, const char *path) {
                    if (success) {
                      Serial.printf("✅ Prayer times for %d/%d saved\n",
                                    nextMonth, nextYear);
                    }
                    // Check if weather also needs fetching (chain in same WiFi session)
                    if (shouldFetchBasedOnInterval(rtcData.weatherLastUpdate,
                                                   weatherUpdateInterval, "WEATHER")) {
                      Serial.println("🌤️ Also fetching weather (chaining in same session)...");
                      WeatherManager::getInstance().asyncFetchWeather(
                          rtcData.latitude, rtcData.longitude, [](bool success) {
                            if (success) {
                              Serial.println("✅ Weather fetched successfully");
                              g_renderState.initialized = false;
                              state = RUNNING_MAIN_TASK; // Render screen with new weather
                            } else {
                              Serial.println("⚠️ Failed to fetch weather (will retry later)");
                              state = SLEEPING;
                            }
                          });
                    } else {
                      state = SLEEPING;
                    }
                  });
            } else {
              // No next month needed, check if weather needs fetching
              if (shouldFetchBasedOnInterval(rtcData.weatherLastUpdate,
                                             weatherUpdateInterval, "WEATHER")) {
                Serial.println("🌤️ Also fetching weather (chaining in same session)...");
                WeatherManager::getInstance().asyncFetchWeather(
                    rtcData.latitude, rtcData.longitude, [](bool success) {
                      if (success) {
                        Serial.println("✅ Weather fetched successfully");
                        g_renderState.initialized = false;
                        state = RUNNING_MAIN_TASK; // Render screen with new weather
                      } else {
                        Serial.println("⚠️ Failed to fetch weather (will retry later)");
                        state = SLEEPING;
                      }
                    });
              } else {
                state = SLEEPING;
              }
            }
          } else {
            Serial.println("⚠️ Failed to fetch prayer times from Aladhan");
            // Still check if weather needs fetching even if prayer times failed
            if (shouldFetchBasedOnInterval(rtcData.weatherLastUpdate,
                                           weatherUpdateInterval, "WEATHER")) {
              Serial.println("🌤️ Fetching weather (prayer times fetch failed)...");
              WeatherManager::getInstance().asyncFetchWeather(
                  rtcData.latitude, rtcData.longitude, [](bool success) {
                    if (success) {
                      Serial.println("✅ Weather fetched successfully");
                      g_renderState.initialized = false;
                      state = RUNNING_MAIN_TASK; // Render screen with new weather
                    } else {
                      Serial.println("⚠️ Failed to fetch weather (will retry later)");
                      state = SLEEPING;
                    }
                  });
            } else {
              state = SLEEPING;
            }
          }
        });
  });
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // Set up factory reset button early
  pinMode(FACTORY_RESET_BUTTON, INPUT_PULLUP);
  Serial.printf("\n\n🔧 Factory reset button configured on GPIO%d\n",
                FACTORY_RESET_BUTTON);
  Serial.println("📝 To factory reset: Hold BOOT button, press RST, keep "
                 "holding BOOT for 10 seconds");

  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();
  state = BOOTING;
}

void handleBooting() {
  if (!SPIFFS.begin(true)) {
    Serial.println("❌ Failed to mount SPIFFS");
    state = FETAL_ERROR;
    return;
  }
  Serial.println("✅ SPIFFS mounted successfully");
  AppStateManager::load();
  
  // Don't start tracking awake time on first boot - only after first sleep
  // This ensures we don't count initialization time
  Serial.println("⏰ Initial boot - awake time tracking will start after first sleep");

  // Restore configuration from SPIFFS if available
  if (SPIFFS.exists("/prayer_config.json")) {
    String configJson = readJsonFile("/prayer_config.json");
    if (!configJson.isEmpty() && configJson != "{}") {
      StaticJsonDocument<512> doc;
      DeserializationError err = deserializeJson(doc, configJson);
      if (!err) {
        rtcData.latitude = doc["latitude"] | 0.0;
        rtcData.longitude = doc["longitude"] | 0.0;
        rtcData.calculationMethod = doc["calculation_method"] | 4;
        rtcData.timezoneOffsetSeconds = doc["timezone_offset"] | 0;
        
        // Restore city name if available
        if (doc.containsKey("city_name")) {
          String cityName = doc["city_name"].as<String>();
          if (!cityName.isEmpty()) {
            strncpy(rtcData.cityName, cityName.c_str(), sizeof(rtcData.cityName) - 1);
            rtcData.cityName[sizeof(rtcData.cityName) - 1] = '\0';
          }
        }
        
        AppStateManager::save(); // Save restored values to RTC memory
        
        int hours = rtcData.timezoneOffsetSeconds / 3600;
        int minutes = (abs(rtcData.timezoneOffsetSeconds) % 3600) / 60;
        Serial.println("📦 Configuration restored from SPIFFS:");
        Serial.printf("   Location: %.6f, %.6f\n", rtcData.latitude, rtcData.longitude);
        Serial.printf("   Timezone: UTC%+d:%02d (%d seconds)\n", hours, minutes, rtcData.timezoneOffsetSeconds);
        Serial.printf("   Calculation Method: %d\n", rtcData.calculationMethod);
        if (rtcData.cityName[0] != '\0') {
          Serial.printf("   City: %s\n", rtcData.cityName);
        }
      } else {
        Serial.println("⚠️ Failed to parse prayer_config.json");
      }
    }
  } else {
    Serial.println("ℹ️ No saved configuration found in SPIFFS");
  }

  // Factory reset detection: Hold BOOT button for 10 seconds during boot
  pinMode(FACTORY_RESET_BUTTON, INPUT_PULLUP);
  delay(100); // Give pin time to stabilize

  int buttonState = digitalRead(FACTORY_RESET_BUTTON);
  Serial.printf("🔍 Factory reset button (GPIO%d) state: %s\n",
                FACTORY_RESET_BUTTON,
                buttonState == LOW ? "PRESSED (LOW)" : "NOT PRESSED (HIGH)");

  // Check if button is pressed (LOW when pressed on most ESP32 boards)
  if (buttonState == LOW) {
    Serial.println("🔘 Factory reset button detected - hold for 10 seconds...");

    unsigned long startPress = millis();
    bool stillPressed = true;
    int lastSecond = 0;

    // Wait and check if button stays pressed for 10 seconds
    while (millis() - startPress < 10000) {
      if (digitalRead(FACTORY_RESET_BUTTON) == HIGH) {
        stillPressed = false;
        Serial.println("❌ Button released - factory reset cancelled");
        break;
      }

      // Print countdown every second
      int currentSecond = (millis() - startPress) / 1000;
      if (currentSecond > lastSecond) {
        lastSecond = currentSecond;
        Serial.printf("⏳ Holding... %d/10 seconds\n", currentSecond);
      }

      delay(100);
    }

    if (stillPressed) {
      Serial.println(
          "🔄 Factory reset triggered (button held for 10 seconds)!");
      Serial.println("🗑️ Clearing all saved configuration...");

      // Clear saved data
      rtcData.wifiRetryCount = 0;
      rtcData.bootCount = 0;
      rtcData.mosqueUUID[0] = '\0';
      rtcData.timezoneOffsetSeconds = 0;
      rtcData.latitude = 0.0;
      rtcData.longitude = 0.0;
      rtcData.calculationMethod = 4;
      rtcData.cityName[0] = '\0';
      AppStateManager::save();

      // Delete SPIFFS files
      if (SPIFFS.exists("/wifi.json"))
        SPIFFS.remove("/wifi.json");
      if (SPIFFS.exists("/mosque_config.json"))
        SPIFFS.remove("/mosque_config.json");
      if (SPIFFS.exists("/prayer_config.json"))
        SPIFFS.remove("/prayer_config.json");

      Serial.println("✅ Factory reset complete. Starting BLE setup...");
      state = ADVERTISING_BLE;
      return;
    }
  }

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
    // Try to connect with saved credentials first
    state = CONNECTING_WIFI_WITH_SAVED_CREDENTIALS;
  }
}

void handleConnectingWifiWithSavedCredentials() {
  Serial.println("🔄 Connecting to Wi-Fi with saved credentials...");
  WiFiManager &wifi = WiFiManager::getInstance();
  
  // Set up callbacks BEFORE calling asyncConnectWithSavedCredentials
  wifi.onWifiConnectedCallback([]() {
    Serial.println("✅ Wi-Fi connected");
    BLEManager::getInstance().stopAdvertising();
    g_bleAdvertising = false; // Clear BLE advertising flag
    // Reset retry counter on successful connection
    rtcData.wifiRetryCount = 0;
    AppStateManager::save();
    
    // Check if weather needs to be fetched on boot
    if (shouldFetchBasedOnInterval(rtcData.weatherLastUpdate, 
                                   weatherUpdateInterval, "WEATHER_ON_BOOT")) {
      Serial.println("🌤️ Weather is stale, fetching on boot...");
      WeatherManager::getInstance().asyncFetchWeather(
          rtcData.latitude, rtcData.longitude, [](bool success) {
            if (success) {
              Serial.println("✅ Weather fetched on boot");
              g_renderState.initialized = false; // Force full refresh
            } else {
              Serial.println("⚠️ Failed to fetch weather on boot (will retry later)");
            }
            state = SYNCING_TIME;
          });
    } else {
      Serial.println("ℹ️ Weather is fresh, skipping fetch on boot");
      state = SYNCING_TIME;
    }
  });
  
  wifi.onWifiFailedToConnectCallback([]() {
    Serial.println("❌ Failed to connect to Wi-Fi (callback triggered)");

    // Check if we have any saved credentials at all
    String wifiJsonString = readJsonFile(WIFI_CRED_FILE);
    if (wifiJsonString.isEmpty() || wifiJsonString == "{}") {
      Serial.println("⚠️ No saved WiFi credentials - starting BLE immediately");
      state = ADVERTISING_BLE;
      return;
    }

    // If we have credentials but they failed, use retry logic
    rtcData.wifiRetryCount++;
    AppStateManager::save();

    if (rtcData.wifiRetryCount >= MAX_WIFI_RETRIES) {
      Serial.printf(
          "⚠️ WiFi failed %d times. Starting BLE for reconfiguration...\n",
          rtcData.wifiRetryCount);
      rtcData.wifiRetryCount = 0; // Reset counter
      AppStateManager::save();
      state = ADVERTISING_BLE;
    } else {
      Serial.printf("🔄 WiFi retry %d/%d. Restarting device...\n",
                    rtcData.wifiRetryCount, MAX_WIFI_RETRIES);
      delay(1000);
      ESP.restart();
    }
  });
  
  // Call after callbacks are set up
  wifi.asyncConnectWithSavedCredentials();
}

void handleSyncingTime() {
  Serial.println("🔄 Syncing time...");
  
  // Safety check: Ensure timezone is configured
  if (rtcData.timezoneOffsetSeconds == 0 && (rtcData.latitude == 0.0 || rtcData.longitude == 0.0)) {
    Serial.println("⚠️ No timezone configured - need to reconfigure via BLE");
    state = ADVERTISING_BLE;
    return;
  }
  
  RTCManager &rtc = RTCManager::getInstance();
  if (rtc.syncTimeFromNTPWithOffset(3, 10000, rtcData.timezoneOffsetSeconds)) {
    Serial.println("✅ Time synced successfully");
    // rtc.setTimeToSpecificHourAndMinute(20, 07, 5, 2); // for testing time
    
    // Disconnect WiFi after time sync to prevent beacon timeout
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("📡 Disconnecting WiFi after time sync");
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      delay(100);
    }
    
    state = RUNNING_MAIN_TASK;
  } else {
    Serial.println("❌ Failed to sync time");
    state = ADVERTISING_BLE;
  }
}

void handleAdvertisingBLE() {
  Serial.println("🔔 Advertising BLE...");
  
  // Ensure WiFi is completely stopped before starting BLE
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

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
  
  // Reinitialize WiFi in STA mode with BLE coexistence
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);  // Required for BLE coexistence
  delay(100);
  
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
      
      // Legacy: mosque UUID (kept for backward compatibility but not used)
      String mosqueUuid = doc["mosque_uuid"].as<String>();

      if (ssid.isEmpty() || password.isEmpty()) {
        Serial.println("⚠️ Incomplete Wi-Fi credentials.");
        state = ADVERTISING_BLE;
        return;
      }

      // Validate timezone offset
      if (!doc.containsKey("timezone_offset")) {
        Serial.println("⚠️ Timezone offset is required but not provided.");
        
        GxEPD2Adapter<decltype(display)> epdAdapter(display);
        ScreenUI ui(epdAdapter, 800, 480);
        ui.showInitializationScreenWithError("⚠️ Timezone offset is required but not provided.");
        
        state = ADVERTISING_BLE;
        return;
      }

      // Validate location (latitude and longitude)
      if (!doc.containsKey("latitude") || !doc.containsKey("longitude")) {
        Serial.println("⚠️ Location (latitude/longitude) is required but not provided.");
        
        GxEPD2Adapter<decltype(display)> epdAdapter(display);
        ScreenUI ui(epdAdapter, 800, 480);
        ui.showInitializationScreenWithError("⚠️ Location coordinates are required.");
        
        state = ADVERTISING_BLE;
        return;
      }

      int timezoneOffset = doc["timezone_offset"] | 0;
      float latitude = doc["latitude"] | 0.0;
      float longitude = doc["longitude"] | 0.0;
      int calculationMethod = doc["calculation_method"] | 4; // Default: Umm Al-Qura (Makkah)

      // Basic validation
      if (latitude < -90.0 || latitude > 90.0 || longitude < -180.0 || longitude > 180.0) {
        Serial.println("⚠️ Invalid location coordinates.");
        
        GxEPD2Adapter<decltype(display)> epdAdapter(display);
        ScreenUI ui(epdAdapter, 800, 480);
        ui.showInitializationScreenWithError("⚠️ Invalid location coordinates.");
        
        state = ADVERTISING_BLE;
        return;
      }

      // Store credentials in global variables (defer write operations to avoid
      // stack overflow)
      g_receivedSSID = ssid;
      g_receivedPassword = password;
      g_receivedMosqueUUID = mosqueUuid; // Kept for backward compatibility
      g_receivedTimezoneOffset = timezoneOffset;
      g_receivedLatitude = latitude;
      g_receivedLongitude = longitude;
      g_receivedCalculationMethod = calculationMethod;

      Serial.printf("✅ Received Configuration:\n");
      Serial.printf("   WiFi: %s\n", ssid.c_str());
      Serial.printf("   Location: %.6f, %.6f\n", latitude, longitude);
      Serial.printf("   Timezone: UTC%+d\n", timezoneOffset / 3600);
      Serial.printf("   Calculation Method: %d\n", calculationMethod);

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

    // First, get city name from coordinates using reverse geocoding
    Serial.println("🌍 Getting location name...");
    
    AladhanManager::getInstance().asyncReverseGeocode(
        g_receivedLatitude, g_receivedLongitude,
        [](bool success, const String &cityName) {
          // Save location data to RTC memory
          rtcData.latitude = g_receivedLatitude;
          rtcData.longitude = g_receivedLongitude;
          rtcData.calculationMethod = g_receivedCalculationMethod;
          
          // Save city name to RTC memory
          if (success && !cityName.isEmpty()) {
            strncpy(rtcData.cityName, cityName.c_str(), 
                    sizeof(rtcData.cityName) - 1);
            rtcData.cityName[sizeof(rtcData.cityName) - 1] = '\0';
            Serial.printf("💾 Location saved: %.6f, %.6f - %s (Method: %d)\n", 
                          rtcData.latitude, rtcData.longitude, rtcData.cityName, 
                          rtcData.calculationMethod);
          } else {
            rtcData.cityName[0] = '\0'; // Clear city name
            Serial.printf("💾 Location saved: %.6f, %.6f (Method: %d)\n", 
                          rtcData.latitude, rtcData.longitude, rtcData.calculationMethod);
            Serial.println("⚠️ Failed to get city name, will use coordinates");
          }

          // Save timezone offset to RTC memory
          rtcData.timezoneOffsetSeconds = g_receivedTimezoneOffset;
          int hours = g_receivedTimezoneOffset / 3600;
          int minutes = (abs(g_receivedTimezoneOffset) % 3600) / 60;
          Serial.printf("💾 Timezone offset saved: UTC%+d:%02d (%d seconds)\n", 
                        hours, minutes, g_receivedTimezoneOffset);

          // Legacy: Save mosque UUID if provided (for backward compatibility)
    if (!g_receivedMosqueUUID.isEmpty()) {
      strncpy(rtcData.mosqueUUID, g_receivedMosqueUUID.c_str(),
              sizeof(rtcData.mosqueUUID) - 1);
            rtcData.mosqueUUID[sizeof(rtcData.mosqueUUID) - 1] = '\0';
            Serial.printf("💾 Legacy mosque UUID saved: %s\n", rtcData.mosqueUUID);
          }

          // Persist to RTC memory
      AppStateManager::save();

      // Also save to SPIFFS for backup
          String configJson = "{";
          configJson += "\"latitude\":" + String(rtcData.latitude, 6) + ",";
          configJson += "\"longitude\":" + String(rtcData.longitude, 6) + ",";
          configJson += "\"calculation_method\":" + String(rtcData.calculationMethod) + ",";
          configJson += "\"timezone_offset\":" + String(rtcData.timezoneOffsetSeconds);
          if (rtcData.cityName[0] != '\0') {
            configJson += ",\"city_name\":\"" + String(rtcData.cityName) + "\"";
          }
          configJson += "}";
          writeJsonFile("/prayer_config.json", configJson);
          Serial.println("💾 Configuration saved to SPIFFS");

          // Fetch initial weather AND prayer times while WiFi is connected
          Serial.println("🌤️ Fetching initial weather...");
          WeatherManager::getInstance().asyncFetchWeather(
              rtcData.latitude, rtcData.longitude, [](bool success) {
                if (success) {
                  Serial.println("✅ Initial weather fetched successfully");
                  g_renderState.initialized = false; // Force full refresh
                } else {
                  Serial.println("⚠️ Failed to fetch initial weather (will retry later)");
                }
                
                // Now fetch prayer times (WiFi still connected)
                Serial.println("📡 Fetching initial prayer times...");
                struct tm timeinfo;
                if (!getLocalTime(&timeinfo)) {
                  Serial.println("❌ Failed to get time for initial prayer times fetch");
                  BLEManager::getInstance().stopAdvertising();
                  g_bleAdvertising = false;
                  state = SYNCING_TIME;
                  return;
                }

                int currentMonth = timeinfo.tm_mon + 1;
                int currentYear = timeinfo.tm_year + 1900;

                Serial.printf("📅 Fetching prayer times for %d/%d\n", currentMonth, currentYear);

                AladhanManager::getInstance().asyncFetchMonthlyPrayerTimes(
                    rtcData.latitude, rtcData.longitude, rtcData.calculationMethod,
                    currentMonth, currentYear,
                    [currentMonth, currentYear](bool success, const char *path) {
                      if (success) {
                        Serial.printf("✅ Prayer times for %d/%d fetched successfully\n",
                                      currentMonth, currentYear);
                        rtcData.mosqueLastUpdateMillis = time(nullptr);
                        AppStateManager::save();
                      } else {
                        Serial.println("⚠️ Failed to fetch initial prayer times (will retry later)");
                      }

                      // Done with initial data fetching, keep WiFi connected for time sync
                      BLEManager::getInstance().stopAdvertising();
                      g_bleAdvertising = false;
                      state = SYNCING_TIME;
                    });
              });
        });
  });

  wifi.onWifiFailedToConnectCallback([]() {
    Serial.println("❌ Failed to connect to Wi-Fi");
    Serial.println("🔄 Restarting device...");
    delay(1000);
    ESP.restart();
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
  // Disconnect WiFi to save power and prevent beacon timeout disconnections
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("📡 Disconnecting WiFi before sleep to save power");
    WiFi.disconnect(true);
    delay(100);
  }
  
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
  
  // Calculate awake time for this cycle (only if tracking has started)
  if (rtcData.wakeStartMillis > 0) {
    unsigned long awakeMillis = millis() - rtcData.wakeStartMillis;
    unsigned long awakeSeconds = awakeMillis / 1000;
    
    // Add to cumulative total
    rtcData.cumulativeAwakeSeconds += awakeSeconds;
    
    // Log for debugging
    Serial.printf("⏱️ Awake for %lu seconds this cycle (Total: %lu seconds, Cycles: %lu)\n", 
                  awakeSeconds, rtcData.cumulativeAwakeSeconds, rtcData.wakeCycleCount);
  } else {
    Serial.println("⏱️ First sleep - not counting initialization time");
  }
  
  // Save to RTC memory
  AppStateManager::save();
  
  Serial.flush();
  delay(20); // give UART time to drain

  // Enable wake-up from BOOT button (GPIO0) in addition to timer
  esp_sleep_enable_ext0_wakeup((gpio_num_t)FACTORY_RESET_BUTTON,
                               0); // Wake on LOW (button press)

  // Sleep & wake
  esp_sleep_enable_timer_wakeup((uint64_t)sleepDuration * 1000000ULL);
  esp_light_sleep_start();

  // After wake
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  if (cause == ESP_SLEEP_WAKEUP_TIMER) {
    Serial.println("⏰ Woke from light sleep by timer.");
  } else if (cause == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("🔘 Woke from light sleep by BOOT button!");
    Serial.println("💡 If you want factory reset: Hold BOOT, press RST, keep "
                   "holding 10 sec");
  } else {
    Serial.printf("⏰ Woke from light sleep (cause=%d).\n", (int)cause);
  }

  // Start tracking new wake cycle (or initialize tracking after first sleep)
  if (rtcData.wakeStartMillis == 0) {
    // First wake after initialization - start tracking now
    rtcData.wakeCycleCount = 0;
    rtcData.wakeStartMillis = millis();
    Serial.println("⏰ Wake cycle tracking started (cycle #0)");
  } else {
    // Normal wake - increment cycle
    rtcData.wakeCycleCount++;
    rtcData.wakeStartMillis = millis();
    Serial.printf("⏰ Wake cycle #%lu started\n", rtcData.wakeCycleCount);
  }
  AppStateManager::save();

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

void fetchWeather() {
  Serial.println("🌤️ Fetching weather...");
  
  WiFiManager::getInstance().asyncConnectWithSavedCredentials();

  WiFiManager::getInstance().onWifiFailedToConnectCallback([]() {
    Serial.println("❌ Failed to connect to Wi-Fi to fetch weather (will retry in 1 hour)");
    // Restore retry count to prevent device restart
    rtcData.wifiRetryCount = 0;
    AppStateManager::save();
    state = SLEEPING;
  });

  WiFiManager::getInstance().onWifiConnectedCallback([]() {
    Serial.println("✅ Connected to Wi-Fi for weather fetch.");
    // Restore retry count after successful connection
    rtcData.wifiRetryCount = 0;
    AppStateManager::save();
    
    WeatherManager::getInstance().asyncFetchWeather(
        rtcData.latitude, rtcData.longitude, [](bool success) {
          if (success) {
            Serial.println("✅ Weather fetched successfully");
            g_renderState.initialized = false;
            state = RUNNING_MAIN_TASK; // Render screen with new weather immediately
          } else {
            Serial.println("❌ Failed to fetch weather (will retry in 1 hour)");
            state = SLEEPING;
          }
        });
  });
}

void handlePeriodicTasks() {
  Serial.println("🔄 Running periodic tasks...");
  
  // Check if we need to fetch prayer times (every 6 hours)
  // Note: Weather fetch is chained inside prayer times fetch to reuse WiFi session
  if (shouldFetchBasedOnInterval(rtcData.mosqueLastUpdateMillis,
                                 mosqueUpdateInterval, "PRAYER_TIMES")) {
    fetchPrayerTimesFromAladhan(); // This also checks/fetches weather if needed
    return;
  }
  Serial.println("⚠️ Prayer times do not need to be fetched yet.");

  // Check if we need to fetch weather separately (only if prayer times not fetched)
  if (shouldFetchBasedOnInterval(rtcData.weatherLastUpdate,
                                 weatherUpdateInterval, "WEATHER")) {
    fetchWeather();
    return;
  }
  Serial.println("⚠️ Weather does not need to be fetched yet.");

  // User events fetching (commented out for now)
  // if (shouldFetchBasedOnInterval(rtcData.userEventsUpdateMillis,
  //                                userEventsUpdateInterval, "USER_EVENTS")) {
  //   fetchUserEvents();
  //   return;
  // }

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
  // Debug: Monitor BOOT button state (only print when it changes)
  static int lastButtonState = HIGH;
  int currentButtonState = digitalRead(FACTORY_RESET_BUTTON);
  if (currentButtonState != lastButtonState) {
    lastButtonState = currentButtonState;
    Serial.printf("🔘 BOOT button (GPIO%d): %s\n", FACTORY_RESET_BUTTON,
                  currentButtonState == LOW ? "PRESSED ⬇️" : "RELEASED ⬆️");
  }

  if (state != lastState) {
    Serial.printf("🔁 State changed: %d ➡️ %d\n", lastState, state);
    lastState = state;
    handleAppState();
  }

  vTaskDelay(100 / portTICK_PERIOD_MS);
}
