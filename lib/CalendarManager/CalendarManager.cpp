#include "CalendarManager.h"
#include "AppState.h"
#include "AppStateManager.h"
#include <SPIFFS.h>
#include <vector>

CalendarManager::CalendarManager() : currentMonth(0), currentDay(0) {}

void safeCopyPrayer(char *destination, const std::vector<String> &times,
                    size_t index) {
  if (index < times.size()) {
    strncpy(destination, times[index].c_str(), 6);
    destination[5] = '\0';
  } else {
    strncpy(destination, "00:00", 6); // fallback
    destination[5] = '\0';
  }
}
String CalendarManager::getMonthFilePath(int month, bool isIqama) {
  if (isIqama) {
    return IQAMA_TIME_FILE_NAME + String(month) + ".json";
  }
  return PRAYER_TIME_FILE_NAME + String(month) + ".json";
}
IqamaTimes CalendarManager::fetchTodayIqamaTimes(int month, int day) {
  Serial.println("📅 Fetching Iqama times for month: " + String(month) +
                 ", day: " + String(day));
  // Serial.println("📂 File path: " + getMonthFilePath(month, true));
  IqamaTimes result;

  String filePath = getMonthFilePath(month, true);
  File file = SPIFFS.open(filePath, "r");
  if (!file) {
    Serial.println("❌ Failed to open file: " + filePath);
    return result;
  }

  String fileContent = file.readString();
  file.close();

  DynamicJsonDocument doc(1024); // Use dynamic allocation here
  DeserializationError error = deserializeJson(doc, fileContent);
  if (error) {
    Serial.println("❌ JSON parsing failed");
    return result;
  }

  JsonObject monthData = doc.as<JsonObject>();
  JsonObject dayDataCalendar = monthData["iqamaCalendar"].as<JsonObject>();
  String dayKey = String(day);

  if (dayDataCalendar[dayKey].is<JsonArray>()) {
    JsonArray iqamaTimes = dayDataCalendar[dayKey].as<JsonArray>();
    for (const auto &time : iqamaTimes) {
      result.iqamaTimes.push_back(time.as<String>());
    }
  } else {
    Serial.println("❌ No Iqama times found for this day");
  }

  return result;
}
TodayPrayerTimes CalendarManager::fetchTodayPrayerTimes(int month, int day) {
  TodayPrayerTimes result;

  String filePath = getMonthFilePath(month);
  File file = SPIFFS.open(filePath, "r");
  if (!file) {
    Serial.println("❌ Failed to open file: " + filePath);
    return result;
  }

  String fileContent = file.readString();
  file.close();

  DynamicJsonDocument doc(1024); // Use dynamic allocation here
  DeserializationError error = deserializeJson(doc, fileContent);
  if (error) {
    Serial.println("❌ JSON parsing failed");
    return result;
  }

  JsonObject monthData = doc.as<JsonObject>();
  JsonObject dayDataCalendar = monthData["prayerCalender"].as<JsonObject>();
  String dayKey = String(day);

  if (dayDataCalendar[dayKey].is<JsonArray>()) {
    JsonArray prayerTimes = dayDataCalendar[dayKey].as<JsonArray>();
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) // Assuming local time is available
    {
      int year = timeinfo.tm_year + 1900; // Get the current year
      String isoDate = String(year) + "-" + String(month) + "-" +
                       String(day); // Format the date as YYYY-MM-DD
      for (const auto &time : prayerTimes) {
        result.prayerTimes.push_back(time.as<String>());
        String prayerTime = time.as<String>();
        int hour = prayerTime.substring(0, 2).toInt();
        int minute = prayerTime.substring(3, 5).toInt();
        String timeZoneOffset = "+00:00";
        String isoDateTime =
            isoDate + "T" + prayerTime + ":00" +
            timeZoneOffset; // Format as YYYY-MM-DDTHH:MM:SS+00:00 // Assign the
                            // full ISO 8601 date and time
        result.prayerTimesISODate.push_back(
            isoDateTime); // Add to the new vector
      }
    }
    String nextDayKey = String(day + 1);
    if (!dayDataCalendar[nextDayKey].is<JsonArray>()) {
      result.isLastDayOfMonth = true; // This is the last day of the month if
                                      // the next day doesn't exist
    }
  }

  return result;
}
bool CalendarManager::isLaterThan(int hour, int minute, const String &timeStr) {
  int h = timeStr.substring(0, 2).toInt();
  int m = timeStr.substring(3, 5).toInt();

  if (h > hour)
    return true;
  if (h == hour && m > minute)
    return true;
  return false;
}

TodayAndNextDayPrayerTimes
CalendarManager::fetchTodayAndNextDayPrayerTimes(int month, int day) {

  TodayAndNextDayPrayerTimes times;
  bool hasAllTodayPrayers =
      rtcData.TODAY_FAJR[0] != '\0' && rtcData.TODAY_SUNRISE[0] != '\0' &&
      rtcData.TODAY_DHUHR[0] != '\0' && rtcData.TODAY_ASR[0] != '\0' &&
      rtcData.TODAY_MAGHRIB[0] != '\0' && rtcData.TODAY_ISHA[0] != '\0';

  bool hasAllTodayIqamas = rtcData.TODAY_IQAMA_FAJR[0] != '\0' &&
                           rtcData.TODAY_IQAMA_DHUHR[0] != '\0' &&
                           rtcData.TODAY_IQAMA_ASR[0] != '\0' &&
                           rtcData.TODAY_IQAMA_MAGHRIB[0] != '\0' &&
                           rtcData.TODAY_IQAMA_ISHA[0] != '\0';

  bool hasAllNextDayPrayers =
      rtcData.NEXT_DAY_FAJR[0] != '\0' && rtcData.NEXT_DAY_SUNRISE[0] != '\0' &&
      rtcData.NEXT_DAY_DHUHR[0] != '\0' && rtcData.NEXT_DAY_ASR[0] != '\0' &&
      rtcData.NEXT_DAY_MAGHRIB[0] != '\0' && rtcData.NEXT_DAY_ISHA[0] != '\0';

  bool hasAllNextDayIqamas = rtcData.NEXT_DAY_IQAMA_FAJR[0] != '\0' &&
                             rtcData.NEXT_DAY_IQAMA_DHUHR[0] != '\0' &&
                             rtcData.NEXT_DAY_IQAMA_ASR[0] != '\0' &&
                             rtcData.NEXT_DAY_IQAMA_MAGHRIB[0] != '\0' &&
                             rtcData.NEXT_DAY_IQAMA_ISHA[0] != '\0';

  if (rtcData.day != 0 && rtcData.day == day && rtcData.month != 0 &&
      rtcData.month == month && hasAllTodayPrayers && hasAllTodayIqamas &&
      hasAllNextDayPrayers && hasAllNextDayIqamas) {

    Serial.println("📅 Using cached prayer times from RTC");

    times.todayPrayerTimes = {rtcData.TODAY_FAJR,    rtcData.TODAY_SUNRISE,
                              rtcData.TODAY_DHUHR,   rtcData.TODAY_ASR,
                              rtcData.TODAY_MAGHRIB, rtcData.TODAY_ISHA};

    times.todayIqamaTimes = {rtcData.TODAY_IQAMA_FAJR,
                             rtcData.TODAY_IQAMA_DHUHR, rtcData.TODAY_IQAMA_ASR,
                             rtcData.TODAY_IQAMA_MAGHRIB,
                             rtcData.TODAY_IQAMA_ISHA};

    times.nextDayPrayerTimes = {
        rtcData.NEXT_DAY_FAJR, rtcData.NEXT_DAY_SUNRISE, rtcData.NEXT_DAY_DHUHR,
        rtcData.NEXT_DAY_ASR,  rtcData.NEXT_DAY_MAGHRIB, rtcData.NEXT_DAY_ISHA};

    times.nextDayIqamaTimes = {
        rtcData.NEXT_DAY_IQAMA_FAJR, rtcData.NEXT_DAY_IQAMA_DHUHR,
        rtcData.NEXT_DAY_IQAMA_ASR, rtcData.NEXT_DAY_IQAMA_MAGHRIB,
        rtcData.NEXT_DAY_IQAMA_ISHA};

    return times;
  }

  Serial.println("🔄 Fetching prayer times for month: " + String(month) +
                 ", day: " + String(day));
  TodayPrayerTimes todayPrayerTime = fetchTodayPrayerTimes(month, day);
  IqamaTimes todayIqamaTime = fetchTodayIqamaTimes(month, day);
  if (todayPrayerTime.prayerTimes.empty() ||
      todayIqamaTime.iqamaTimes.empty() ||
      todayPrayerTime.prayerTimes.size() < 6 ||
      todayIqamaTime.iqamaTimes.size() < 5) {
    Serial.println("❌ Failed to fetch today prayer times");
    return times;
  }
  // *** Today prayer times ***//
  safeCopyPrayer(rtcData.TODAY_FAJR, todayPrayerTime.prayerTimes, 0);
  safeCopyPrayer(rtcData.TODAY_SUNRISE, todayPrayerTime.prayerTimes, 1);
  safeCopyPrayer(rtcData.TODAY_DHUHR, todayPrayerTime.prayerTimes, 2);
  safeCopyPrayer(rtcData.TODAY_ASR, todayPrayerTime.prayerTimes, 3);
  safeCopyPrayer(rtcData.TODAY_MAGHRIB, todayPrayerTime.prayerTimes, 4);
  safeCopyPrayer(rtcData.TODAY_ISHA, todayPrayerTime.prayerTimes, 5);

  // *** Today iqama times ***//
  safeCopyPrayer(rtcData.TODAY_IQAMA_FAJR, todayIqamaTime.iqamaTimes, 0);
  safeCopyPrayer(rtcData.TODAY_IQAMA_DHUHR, todayIqamaTime.iqamaTimes, 1);
  safeCopyPrayer(rtcData.TODAY_IQAMA_ASR, todayIqamaTime.iqamaTimes, 2);
  safeCopyPrayer(rtcData.TODAY_IQAMA_MAGHRIB, todayIqamaTime.iqamaTimes, 3);
  safeCopyPrayer(rtcData.TODAY_IQAMA_ISHA, todayIqamaTime.iqamaTimes, 4);
  AppStateManager::save();

  int nextMonth;
  int nextDay;
  if (todayPrayerTime.isLastDayOfMonth) {
    Serial.println("🔄 Next month");
    nextMonth = (month % 12) + 1;
    nextDay = 1;
  } else {
    Serial.println("🔄 Next day");
    nextMonth = month;
    nextDay = day + 1;
  }
  TodayPrayerTimes nextDayPrayerTime =
      fetchTodayPrayerTimes(nextMonth, nextDay);
  IqamaTimes nextDayIqamaTime = fetchTodayIqamaTimes(nextMonth, nextDay);
  if (nextDayPrayerTime.prayerTimes.empty() ||
      nextDayIqamaTime.iqamaTimes.empty() ||
      nextDayPrayerTime.prayerTimes.size() < 6 ||
      nextDayIqamaTime.iqamaTimes.size() < 5) {
    Serial.println("❌ Failed to fetch next day prayer times");
    return times;
  }
  Serial.println("🔄 Fetching prayer times for month: " + String(nextMonth) +
                 ", day: " + String(nextDay));

  // *** Next day prayer times ***//
  safeCopyPrayer(rtcData.NEXT_DAY_FAJR, nextDayPrayerTime.prayerTimes, 0);
  safeCopyPrayer(rtcData.NEXT_DAY_SUNRISE, nextDayPrayerTime.prayerTimes, 1);
  safeCopyPrayer(rtcData.NEXT_DAY_DHUHR, nextDayPrayerTime.prayerTimes, 2);
  safeCopyPrayer(rtcData.NEXT_DAY_ASR, nextDayPrayerTime.prayerTimes, 3);
  safeCopyPrayer(rtcData.NEXT_DAY_MAGHRIB, nextDayPrayerTime.prayerTimes, 4);
  safeCopyPrayer(rtcData.NEXT_DAY_ISHA, nextDayPrayerTime.prayerTimes, 5);

  /// *** Next day iqama times ***//
  safeCopyPrayer(rtcData.NEXT_DAY_IQAMA_FAJR, nextDayIqamaTime.iqamaTimes, 0);
  safeCopyPrayer(rtcData.NEXT_DAY_IQAMA_DHUHR, nextDayIqamaTime.iqamaTimes, 1);
  safeCopyPrayer(rtcData.NEXT_DAY_IQAMA_ASR, nextDayIqamaTime.iqamaTimes, 2);
  safeCopyPrayer(rtcData.NEXT_DAY_IQAMA_MAGHRIB, nextDayIqamaTime.iqamaTimes,
                 3);
  safeCopyPrayer(rtcData.NEXT_DAY_IQAMA_ISHA, nextDayIqamaTime.iqamaTimes, 4);
  rtcData.day = day;
  rtcData.month = month;
  AppStateManager::save();

  times.todayPrayerTimes = {rtcData.TODAY_FAJR,    rtcData.TODAY_SUNRISE,
                            rtcData.TODAY_DHUHR,   rtcData.TODAY_ASR,
                            rtcData.TODAY_MAGHRIB, rtcData.TODAY_ISHA};

  times.todayIqamaTimes = {rtcData.TODAY_IQAMA_FAJR, rtcData.TODAY_IQAMA_DHUHR,
                           rtcData.TODAY_IQAMA_ASR, rtcData.TODAY_IQAMA_MAGHRIB,
                           rtcData.TODAY_IQAMA_ISHA};

  times.nextDayPrayerTimes = {
      rtcData.NEXT_DAY_FAJR, rtcData.NEXT_DAY_SUNRISE, rtcData.NEXT_DAY_DHUHR,
      rtcData.NEXT_DAY_ASR,  rtcData.NEXT_DAY_MAGHRIB, rtcData.NEXT_DAY_ISHA};

  times.nextDayIqamaTimes = {
      rtcData.NEXT_DAY_IQAMA_FAJR, rtcData.NEXT_DAY_IQAMA_DHUHR,
      rtcData.NEXT_DAY_IQAMA_ASR, rtcData.NEXT_DAY_IQAMA_MAGHRIB,
      rtcData.NEXT_DAY_IQAMA_ISHA};

  return times;
}
