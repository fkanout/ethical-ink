#include "AppStateManager.h"

// ✅ Here you really allocate the RTC variable
RTC_DATA_ATTR RTCData rtcData;

#define RTC_MAGIC_VALID 0xBEEF1234

void AppStateManager::load() {
  if (rtcData.rtcMagic != RTC_MAGIC_VALID) {
    Serial.println("🧹 RTC memory invalid, reinitializing...");
    memset(&rtcData, 0, sizeof(rtcData));

    strncpy(rtcData.FAJR, "00:00", sizeof(rtcData.FAJR));
    strncpy(rtcData.IQAMA_Fajr, "00:00", sizeof(rtcData.IQAMA_Fajr));
    strncpy(rtcData.DHUHR, "00:00", sizeof(rtcData.DHUHR));
    strncpy(rtcData.IQAMA_Dhuhr, "00:00", sizeof(rtcData.IQAMA_Dhuhr));
    strncpy(rtcData.ASR, "00:00", sizeof(rtcData.ASR));
    strncpy(rtcData.IQAMA_Asr, "00:00", sizeof(rtcData.IQAMA_Asr));
    strncpy(rtcData.MAGHRIB, "00:00", sizeof(rtcData.MAGHRIB));
    strncpy(rtcData.IQAMA_Maghrib, "00:00", sizeof(rtcData.IQAMA_Maghrib));
    strncpy(rtcData.ISHA, "00:00", sizeof(rtcData.ISHA));
    strncpy(rtcData.IQAMA_Isha, "00:00", sizeof(rtcData.IQAMA_Isha));
    strncpy(rtcData.SUNRISE, "00:00", sizeof(rtcData.SUNRISE));
    strncpy(rtcData.nextPrayerMinAndHour, "00:00",
            sizeof(rtcData.nextPrayerMinAndHour));

    rtcData.isTomorrowFetched = false;
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