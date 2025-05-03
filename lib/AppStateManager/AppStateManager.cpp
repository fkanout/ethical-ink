#include "AppStateManager.h"

// ✅ Here you really allocate the RTC variable
RTC_DATA_ATTR RTCData rtcData;

#define RTC_MAGIC_VALID 0xBEEF1234

void AppStateManager::load() {
  if (rtcData.rtcMagic != RTC_MAGIC_VALID) {
    Serial.println("🧹 RTC memory invalid, reinitializing...");
    memset(&rtcData, 0, sizeof(rtcData));

    strncpy(rtcData.TODAY_FAJR, "00:00", sizeof(rtcData.TODAY_FAJR));
    strncpy(rtcData.TODAY_IQAMA_FAJR, "00:00",
            sizeof(rtcData.TODAY_IQAMA_FAJR));
    strncpy(rtcData.NEXT_DAY_FAJR, "00:00", sizeof(rtcData.NEXT_DAY_FAJR));
    strncpy(rtcData.NEXT_DAY_IQAMA_FAJR, "00:00",
            sizeof(rtcData.NEXT_DAY_IQAMA_FAJR));

    strncpy(rtcData.TODAY_DHUHR, "00:00", sizeof(rtcData.TODAY_DHUHR));
    strncpy(rtcData.TODAY_IQAMA_DHUHR, "00:00",
            sizeof(rtcData.TODAY_IQAMA_DHUHR));
    strncpy(rtcData.NEXT_DAY_DHUHR, "00:00", sizeof(rtcData.NEXT_DAY_DHUHR));
    strncpy(rtcData.NEXT_DAY_IQAMA_DHUHR, "00:00",
            sizeof(rtcData.NEXT_DAY_IQAMA_DHUHR));

    strncpy(rtcData.TODAY_ASR, "00:00", sizeof(rtcData.TODAY_ASR));
    strncpy(rtcData.TODAY_IQAMA_ASR, "00:00", sizeof(rtcData.TODAY_IQAMA_ASR));
    strncpy(rtcData.NEXT_DAY_ASR, "00:00", sizeof(rtcData.NEXT_DAY_ASR));
    strncpy(rtcData.NEXT_DAY_IQAMA_ASR, "00:00",
            sizeof(rtcData.NEXT_DAY_IQAMA_ASR));

    strncpy(rtcData.TODAY_MAGHRIB, "00:00", sizeof(rtcData.TODAY_MAGHRIB));
    strncpy(rtcData.TODAY_IQAMA_MAGHRIB, "00:00",
            sizeof(rtcData.TODAY_IQAMA_MAGHRIB));
    strncpy(rtcData.NEXT_DAY_MAGHRIB, "00:00",
            sizeof(rtcData.NEXT_DAY_MAGHRIB));
    strncpy(rtcData.NEXT_DAY_IQAMA_MAGHRIB, "00:00",
            sizeof(rtcData.NEXT_DAY_IQAMA_MAGHRIB));

    strncpy(rtcData.TODAY_ISHA, "00:00", sizeof(rtcData.TODAY_ISHA));
    strncpy(rtcData.TODAY_IQAMA_ISHA, "00:00",
            sizeof(rtcData.TODAY_IQAMA_ISHA));
    strncpy(rtcData.NEXT_DAY_ISHA, "00:00", sizeof(rtcData.NEXT_DAY_ISHA));
    strncpy(rtcData.NEXT_DAY_IQAMA_ISHA, "00:00",
            sizeof(rtcData.NEXT_DAY_IQAMA_ISHA));

    strncpy(rtcData.TODAY_SUNRISE, "00:00", sizeof(rtcData.TODAY_SUNRISE));
    strncpy(rtcData.NEXT_DAY_SUNRISE, "00:00",
            sizeof(rtcData.NEXT_DAY_SUNRISE));

    rtcData.lastUpdateMillis = 0;
    rtcData.day = 0;
    rtcData.month = 0;
    rtcData.rtcMagic = RTC_MAGIC_VALID;
  } else {
    Serial.println("✅ RTC memory valid, loaded");
  }
}

void AppStateManager::save() {
  rtcData.rtcMagic = RTC_MAGIC_VALID;
  Serial.println("✅ RTC memory saved");
}