#include "CalendarManager.h"
#include "AppState.h"
#include <SPIFFS.h>
#include <vector>
CalendarManager::CalendarManager() : currentMonth(0), currentDay(0) {}

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

PrayerTimeInfo CalendarManager::getNextPrayerTimeForToday(int month, int day,
                                                          int currentHour,
                                                          int currentMinute,
                                                          bool fetchTomorrow) {

  TodayPrayerTimes todayPrayerTime = fetchTodayPrayerTimes(month, day);
  IqamaTimes todayIqamaTime = fetchTodayIqamaTimes(month, day);
  PrayerTimeInfo result;
  result.day = day;
  result.month = month;
  result.fetchTomorrow = fetchTomorrow;
  result.prayerTimes = todayPrayerTime.prayerTimes;
  result.prayerTimesISODate =
      todayPrayerTime.prayerTimesISODate;        // Default to 0
  result.iqamaTimes = todayIqamaTime.iqamaTimes; // Default to 0
  if (fetchTomorrow) {
    result.nextPrayerMinAndHour = todayPrayerTime.prayerTimes[0];
    return result;
  }
  for (const String &time : todayPrayerTime.prayerTimes) {
    if (isLaterThan(currentHour, currentMinute, time)) {
      result.nextPrayerMinAndHour = time;
      return result;
    }
  }

  Serial.println("🌙 All prayers for today have passed - Fetch tomorrow ");

  if (todayPrayerTime.isLastDayOfMonth) {
    Serial.println("🔄 Next month");
    int nextMonth = (month % 12) + 1;
    int nextDay = 1;
    return getNextPrayerTimeForToday(nextMonth, nextDay, currentHour,
                                     currentMinute, true);
  } else {
    Serial.println("🔄 Next day");
    int nextMonth = month;
    int nextDay = day + 1;
    return getNextPrayerTimeForToday(nextMonth, nextDay, currentHour,
                                     currentMinute, true);
  }

  return result;
}
