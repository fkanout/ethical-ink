#ifndef CALENDAR_MANAGER_H
#define CALENDAR_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <vector>

struct TodayPrayerTimes {
  std::vector<String> prayerTimes;
  std::vector<String> prayerTimesISODate;
  bool isLastDayOfMonth = false;
};
struct IqamaTimes {
  std::vector<String> iqamaTimes;
};

struct PrayerTimeInfo {
  std::vector<String> prayerTimes;
  std::vector<String> prayerTimesISODate;
  std::vector<String> iqamaTimes;
  String nextPrayerMinAndHour;
};
class CalendarManager {
public:
  CalendarManager();
  String getMonthFilePath(int month, bool isIqama = false);
  TodayPrayerTimes fetchTodayPrayerTimes(int month, int day);
  IqamaTimes fetchTodayIqamaTimes(int month, int day);
  PrayerTimeInfo getNextPrayerTimeForToday(int month, int day, int currentHour,
                                           int currentMinute,
                                           bool fetchTomorrow = false);

private:
  int currentMonth;
  int currentDay;
  bool isLaterThan(int hour, int minute, const String &timeStr);
};

#endif