

#include "AppStateManager.h"
#include "EventsManager.h"
#include "SPIFFSHelper.h"
#include "WiFiManager.h"
#include <AppState.h>
#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEManager.h>
#include <CalendarManager.h>
#include <MAWAQITManager.h>
#include <RTCManager.h>
#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <SPI.h>

// Pins for E-paper
#define EPD_CS   10
#define EPD_DC   9
#define EPD_RST  8
#define EPD_BUSY 7

// 7.5" b/w (800x480)
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(
  GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);




CalendarManager calendarManager;
unsigned int sleepDuration = 60; // in seconds, default fallback
const char *PRAYER_NAMES[] = {"Fajr", "Sunrise", "Dhuhr",
                              "Asr",  "Maghrib", "Isha"};

const unsigned long mosqueUpdateInterval = 6UL * 60UL * 60UL; // 6 hours
const unsigned long userEventsUpdateInterval = 10UL * 60UL;   // 10 minutes
bool isFetching = false;

AppState state = BOOTING;
AppState lastState = FETAL_ERROR;

// Add these global variables to track changes
String lastCountdownStr = "";
int lastHighlightIndex = -1;
bool forceFullRefresh = false;
int partialUpdateCount = 0;
const int MAX_PARTIAL_UPDATES = 10; // Force full refresh after N partial updates

// ---------- helpers ----------
void drawTextBox(const char* text, int16_t x, int16_t y, int16_t wBox, int16_t hBox, const GFXfont* font) {
  display.setFont(font);

  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  display.drawRect(x, y, wBox, hBox, GxEPD_BLACK);

  int16_t textX = x + (wBox - w) / 2 - x1;
  int16_t textY = y + (hBox - h) / 2 - y1;

  display.setTextColor(GxEPD_BLACK);
  display.setCursor(textX, textY);
  display.print(text);
}

void drawCenteredText(const char* text, int16_t centerX, int16_t topY, const GFXfont* font) {
  display.setFont(font);

  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t textX = centerX - w/2 - x1;
  int16_t textY = topY - y1;

  display.setTextColor(GxEPD_BLACK);
  display.setCursor(textX, textY);
  display.print(text);
}

// ---------- draw prayer-time boxes with highlight ----------
void drawPrayerTimeBoxes(const char* names[], const char* times[], int count,
                         int16_t startY, int16_t boxW, int16_t boxH, int16_t spacing,
                         int highlightIndex = -1) {
  // center the whole row
  int16_t totalW = count * boxW + (count - 1) * spacing;
  int16_t startX = (800 - totalW) / 2;

  for (int i = 0; i < count; i++) {
    int16_t x = startX + i * (boxW + spacing);

    if (i == highlightIndex) {
      // Highlighted box (black background, white text)
      display.fillRect(x, startY, boxW, boxH, GxEPD_BLACK);
      display.drawRect(x, startY, boxW, boxH, GxEPD_BLACK);

      display.setFont(&FreeMonoBold9pt7b);
      int16_t x1, y1; uint16_t w, h;
      display.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = startY + 20;
      display.setTextColor(GxEPD_WHITE);
      display.setCursor(nameX, nameY);
      display.print(names[i]);

      display.setFont(&FreeMonoBold18pt7b);
      display.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = startY + boxH - 12;
      display.setCursor(timeX, timeY);
      display.print(times[i]);
    } else {
      // Normal box
      display.drawRect(x, startY, boxW, boxH, GxEPD_BLACK);

      display.setFont(&FreeMonoBold9pt7b);
      int16_t x1, y1; uint16_t w, h;
      display.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = startY + 20;
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(nameX, nameY);
      display.print(names[i]);

      display.setFont(&FreeMonoBold18pt7b);
      display.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = startY + boxH - 12;
      display.setCursor(timeX, timeY);
      display.print(times[i]);
    }
  }
}

// ---------- helper: find next prayer ----------
int getNextPrayerIndex(const char* times[], int count, int currentHour, int currentMin) {
  int now = currentHour * 60 + currentMin; // total minutes

  for (int i = 0; i < count; i++) {
    int h, m;
    sscanf(times[i], "%d:%d", &h, &m);
    int t = h * 60 + m;
    if (t > now) {
      return i; // first prayer after current time
    }
  }
  return 0; // if none left, wrap to Fajr
}

//---------------------------------------helpers for screen---------------------------------


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

// Helper functions for display updates
void performFullRefresh(const char* countdownStr, const char* prayerNames[], 
                       const char* prayerTimes[], int highlightIndex) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    int16_t currentY = 10;
    int16_t boxW = 200, boxH = 60, spacing = 20;
    int16_t boxX = (800 - boxW) / 2;

    // Mosque name box
    drawTextBox("Mosque Name", boxX, currentY, boxW, boxH, &FreeMonoBold9pt7b);

    // "Prayer in"
    currentY += boxH + spacing;
    drawCenteredText("Prayer in", 400, currentY, &FreeMonoBold9pt7b);

    // HH:MM big box with countdown
    currentY += 30;
    drawTextBox(countdownStr, boxX, currentY, 200, 100, &FreeMonoBold24pt7b);

    // Bottom row prayer times
    int16_t rowY = 300;
    int16_t prayerBoxW = 140;
    int16_t prayerBoxH = 90;
    int16_t prayerSpacing = 10;
    drawPrayerTimeBoxes(prayerNames, prayerTimes, 5, rowY, prayerBoxW, prayerBoxH, 
                       prayerSpacing, highlightIndex);
  } while (display.nextPage());
}

void performPartialCountdownUpdate(const char* countdownStr) {
  // Define the countdown box area
  int16_t boxX = (800 - 200) / 2;
  int16_t boxY = 140; // Adjust based on your layout (currentY + 30 from your original code)
  int16_t boxW = 200;
  int16_t boxH = 100;
  
  // Set partial window for countdown area
  display.setPartialWindow(boxX, boxY, boxW, boxH);
  
  display.firstPage();
  do {
    // Clear the countdown area
    display.fillRect(boxX, boxY, boxW, boxH, GxEPD_WHITE);
    // Redraw just the countdown box
    drawTextBox(countdownStr, boxX, boxY, boxW, boxH, &FreeMonoBold24pt7b);
  } while (display.nextPage());
}

// Add this function to force a full refresh when needed
void forceNextFullRefresh() {
  forceFullRefresh = true;
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
  Serial.printf("🕒 %02d:%02d:%02d  📅 %02d/%02d/%04d\n", timeinfo.tm_hour,
                timeinfo.tm_min, currentSecond, timeinfo.tm_mday,
                timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

  TodayAndNextDayPrayerTimes todayAndNextDayPrayerTimes =
      calendarManager.fetchTodayAndNextDayPrayerTimes(timeinfo.tm_mon + 1,
                                                      timeinfo.tm_mday);

  std::vector<String> todayPrayer = todayAndNextDayPrayerTimes.todayPrayerTimes;
  std::vector<String> todayIqama = todayAndNextDayPrayerTimes.todayIqamaTimes;
  std::vector<String> nextDayPrayer = todayAndNextDayPrayerTimes.nextDayPrayerTimes;
  std::vector<String> nextDayIqama = todayAndNextDayPrayerTimes.nextDayIqamaTimes;

  bool isShowNextDayPrayers = false;
  struct NextPrayerInfo {
    String time;
    String name;
  } nextPrayerInfo;

  if (todayPrayer.empty() || todayIqama.empty() || nextDayPrayer.empty() || nextDayIqama.empty()) {
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
    FAJR = nextDayPrayer[0]; SUNRISE = nextDayPrayer[1]; DHUHR = nextDayPrayer[2];
    ASR = nextDayPrayer[3]; MAGHRIB = nextDayPrayer[4]; ISHA = nextDayPrayer[5];
    IQAMA_Fajr = nextDayIqama[0]; IQAMA_Dhuhr = nextDayIqama[1]; IQAMA_Asr = nextDayIqama[2];
    IQAMA_Maghrib = nextDayIqama[3]; IQAMA_Isha = nextDayIqama[4];
  } else {
    FAJR = todayPrayer[0]; SUNRISE = todayPrayer[1]; DHUHR = todayPrayer[2];
    ASR = todayPrayer[3]; MAGHRIB = todayPrayer[4]; ISHA = todayPrayer[5];
    IQAMA_Fajr = todayIqama[0]; IQAMA_Dhuhr = todayIqama[1]; IQAMA_Asr = todayIqama[2];
    IQAMA_Maghrib = todayIqama[3]; IQAMA_Isha = todayIqama[4];
  }

  Countdown countdown = calculateCountdownToNextPrayer(nextPrayerInfo.time, timeinfo);
  char countdownStr[16];
  sprintf(countdownStr, "%02d:%02d", countdown.hours, countdown.minutes);

  String title = String("✨══════•••••• Prayer times for ") + (isShowNextDayPrayers ? "tomorrow" : "today") + " ••••••══════✨";

  Serial.println(title.c_str());
  Serial.printf("  ⏰ %s  %s\n", FAJR.c_str(), IQAMA_Fajr.c_str());
  Serial.printf("  🌅 %s\n", SUNRISE.c_str());
  Serial.printf("  ⏰ %s  %s\n", DHUHR.c_str(), IQAMA_Dhuhr.c_str());
  Serial.printf("  ⏰ %s  %s\n", ASR.c_str(), IQAMA_Asr.c_str());
  Serial.printf("  ⏰ %s  %s\n", MAGHRIB.c_str(), IQAMA_Maghrib.c_str());
  Serial.printf("  ⏰ %s  %s\n", ISHA.c_str(), IQAMA_Isha.c_str());
  Serial.printf("  ⏳%s in %02d:%02d\n", nextPrayerInfo.name.c_str(), countdown.hours, countdown.minutes);
  Serial.println("✨══════•••••••••••••••••••••••••••••••••••══════✨");

  // ---------- Optimized E-paper display update ----------
  const char* prayerNames[5] = {"Fajr","Dhuhr","Asr","Maghrib","Isha"};
  const char* prayerTimes[5] = {FAJR.c_str(), DHUHR.c_str(), ASR.c_str(), MAGHRIB.c_str(), ISHA.c_str()};
  int currentHighlightIndex = getNextPrayerIndex(prayerTimes, 5, timeinfo.tm_hour, timeinfo.tm_min);
  
  // Check what needs updating
  bool countdownChanged = (String(countdownStr) != lastCountdownStr);
  bool highlightChanged = (currentHighlightIndex != lastHighlightIndex);
  bool needsFullRefresh = forceFullRefresh || (partialUpdateCount >= MAX_PARTIAL_UPDATES);
  
  Serial.printf("📊 Display Update Status:\n");
  Serial.printf("   Countdown: %s -> %s (Changed: %s)\n", 
                lastCountdownStr.c_str(), countdownStr, countdownChanged ? "YES" : "NO");
  Serial.printf("   Highlight: %d -> %d (Changed: %s)\n", 
                lastHighlightIndex, currentHighlightIndex, highlightChanged ? "YES" : "NO");
  Serial.printf("   Partial Updates: %d/%d\n", partialUpdateCount, MAX_PARTIAL_UPDATES);
  Serial.printf("   Force Full Refresh: %s\n", forceFullRefresh ? "YES" : "NO");
  
  if (!countdownChanged && !highlightChanged && !needsFullRefresh) {
    Serial.println("⏭️ No display changes needed, skipping screen update");
    unsigned long duration = millis() - startTime;
    Serial.printf("⏱️ Main task completed in %lu ms (no display update)\n", duration);
    return;
  }

  // Determine update type and perform update
  if (needsFullRefresh || highlightChanged) {
    Serial.println("🔄 Performing FULL screen refresh");
    Serial.printf("   Reason: %s%s%s\n", 
                  needsFullRefresh ? "MaxPartialReached/Forced " : "",
                  highlightChanged ? "HighlightChanged " : "",
                  forceFullRefresh ? "ForceRefresh " : "");
    
    performFullRefresh(countdownStr, prayerNames, prayerTimes, currentHighlightIndex);
    partialUpdateCount = 0;
    forceFullRefresh = false;
    Serial.println("✅ Full refresh completed");
    
  } else if (countdownChanged) {
    Serial.println("⚡ Performing PARTIAL update (countdown only)");
    
    // Check if partial updates are supported
    if (display.epd2.hasFastPartialUpdate) {
      performPartialCountdownUpdate(countdownStr);
      partialUpdateCount++;
      Serial.printf("✅ Partial update completed (%d/%d)\n", partialUpdateCount, MAX_PARTIAL_UPDATES);
    } else {
      Serial.println("⚠️ Partial updates not supported, falling back to full refresh");
      performFullRefresh(countdownStr, prayerNames, prayerTimes, currentHighlightIndex);
      partialUpdateCount = 0;
    }
  }

  // Update tracking variables
  lastCountdownStr = String(countdownStr);
  lastHighlightIndex = currentHighlightIndex;

  // Sleep duration until next update
  unsigned long duration = millis() - startTime;
  Serial.printf("⏱️ Main task completed in %lu ms\n", duration);
}

// Additional helper function to force full refresh when prayer data changes
// Call this from your prayer data fetch functions
void onPrayerDataUpdated() {
  Serial.println("📅 Prayer data updated - forcing next full refresh");
  forceNextFullRefresh();
  // Reset tracking to ensure full refresh
  lastCountdownStr = "";
  lastHighlightIndex = -1;
  partialUpdateCount = 0;
}
//-------------------------end main execute-------------------------------------
void fetchPrayerTimesIfDue() {
  Serial.println("📡 Fetching prayer times from MAWAQIT...");
  WiFiManager::getInstance().asyncConnectWithSavedCredentials();

  WiFiManager::getInstance().onWifiFailedToConnectCallback([]() {
    Serial.println(
        "❌ Failed to connect to Wi-Fi to fetch prayer times if due");
    // Track this state to show in the UI and stop keeping the device awake each
    // time

    state = SLEEPING;
  });
  WiFiManager::getInstance().onWifiConnectedCallback([]() {
    Serial.println("✅ Connected to Wi-Fi for MAWAQIT fetch.");
    MAWAQITManager::getInstance().setApiKey(
        "86ed48fd-691e-4370-a9bf-ae74f788ed54");
    MAWAQITManager::getInstance().asyncFetchPrayerTimes(
        "f9a51508-05b7-4324-a7e8-4acbc2893c02",
        [](bool success, const char *path) {
          if (success) {
            Serial.printf("📂 Valid prayer times file ready at: %s\n", path);
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
    // rtc.setTimeToSpecificHourAndMinute(22, 0, 5, 2);
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
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int currentSecond = timeinfo.tm_sec;
    sleepDuration = 60 - currentSecond;
    if (sleepDuration == 60) {
      sleepDuration = 0; // edge case
    }
  } else {
    Serial.println(
        "⚠️ Failed to get time for sleep alignment. Using 60s fallback.");
    sleepDuration = 60;
  }

  Serial.printf("💤 Sleeping for %d seconds to align with full minute...\n",
                sleepDuration);
  esp_sleep_enable_timer_wakeup(sleepDuration * 1000000ULL);
  esp_deep_sleep_start();
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
  if (shouldFetchBasedOnInterval(rtcData.userEventsUpdateMillis,
                                 userEventsUpdateInterval, "USER_EVENTS")) {
    fetchUserEvents();
    return;
  }
  Serial.println("⚠️ User events do not need to be fetched.");
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