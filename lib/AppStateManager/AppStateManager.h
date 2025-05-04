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
};

extern RTCData rtcData;

class AppStateManager {
public:
  static void load();
  static void save();
};