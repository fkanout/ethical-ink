#pragma once
#include <Arduino.h>

struct RTCData {
  uint32_t rtcMagic = 0;

  time_t mosqueLastUpdateMillis = 0;
  time_t userEventsUpdateMillis = 0;

  char TODAY_FAJR[6] = "00:00";
  char TODAY_IQAMA_FAJR[6] = "00:00";
  char NEXT_DAY_FAJR[6] = "00:00";
  char NEXT_DAY_IQAMA_FAJR[6] = "00:00";

  char TODAY_DHUHR[6] = "00:00";
  char TODAY_IQAMA_DHUHR[6] = "00:00";
  char NEXT_DAY_DHUHR[6] = "00:00";
  char NEXT_DAY_IQAMA_DHUHR[6] = "00:00";

  char TODAY_ASR[6] = "00:00";
  char TODAY_IQAMA_ASR[6] = "00:00";
  char NEXT_DAY_ASR[6] = "00:00";
  char NEXT_DAY_IQAMA_ASR[6] = "00:00";

  char TODAY_MAGHRIB[6] = "00:00";
  char TODAY_IQAMA_MAGHRIB[6] = "00:00";
  char NEXT_DAY_MAGHRIB[6] = "00:00";
  char NEXT_DAY_IQAMA_MAGHRIB[6] = "00:00";

  char TODAY_ISHA[6] = "00:00";
  char TODAY_IQAMA_ISHA[6] = "00:00";
  char NEXT_DAY_ISHA[6] = "00:00";
  char NEXT_DAY_IQAMA_ISHA[6] = "00:00";

  char TODAY_SUNRISE[6] = "00:00";
  char NEXT_DAY_SUNRISE[6] = "00:00";

  int day = 0;
  int month = 0;

  char mosqueUUID[40] =
      ""; // Store mosque UUID (36 chars + null terminator + padding) - DEPRECATED, use lat/lon
  
  // NEW: Location-based prayer times configuration
  float latitude = 0.0;  // Location latitude for prayer times calculation
  float longitude = 0.0; // Location longitude for prayer times calculation
  int calculationMethod = 4; // Calculation method (4 = Umm Al-Qura, Makkah)
  char cityName[50] = ""; // City name from Aladhan API
  
  int timezoneOffsetSeconds = 0; // Timezone offset in seconds (e.g., 3600 for UTC+1)
  int wifiRetryCount = 0; // Track WiFi connection failures
  int bootCount = 0;      // Track rapid reboots for factory reset detection
  unsigned long lastBootMillis =
      0; // Last boot time in milliseconds (from millis())
};

extern RTCData rtcData;

class AppStateManager {
public:
  static void load();
  static void save();
};
