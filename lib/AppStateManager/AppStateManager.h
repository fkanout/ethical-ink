#pragma once
#include <Arduino.h>

struct RTCData {
  uint32_t rtcMagic = 0;

  time_t lastUpdateMillis = 0;

  char FAJR[6] = "00:00";
  char IQAMA_Fajr[6] = "00:00";

  char DHUHR[6] = "00:00";
  char IQAMA_Dhuhr[6] = "00:00";

  char ASR[6] = "00:00";
  char IQAMA_Asr[6] = "00:00";

  char MAGHRIB[6] = "00:00";
  char IQAMA_Maghrib[6] = "00:00";

  char ISHA[6] = "00:00";
  char IQAMA_Isha[6] = "00:00";

  char SUNRISE[6] = "00:00";

  bool isTomorrowFetched = false;

  char nextPrayerMinAndHour[6] = "00:00";

  int day = 0;
  int month = 0;
};

extern RTCData rtcData;

class AppStateManager {
public:
  static void load();
  static void save();
};