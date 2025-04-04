#ifndef CALENDAR_MANAGER_H
#define CALENDAR_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <vector>

struct TodayPrayerTimes
{
    std::vector<String> prayerTimes;
    std::vector<String> prayerTimesISODate;
    bool isLastDayOfMonth = false;
};
struct PrayerTimeInfo
{
    std::vector<String> prayerTimes;
    std::vector<String> prayerTimesISODate;
    String nextPrayerMinAndHour;
};
class CalendarManager
{
public:
    CalendarManager();
    String getMonthFilePath(int month);
    TodayPrayerTimes fetchTodayPrayerTimes(int month, int day);
    PrayerTimeInfo getNextPrayerTimeForToday(int month, int day, int currentHour, int currentMinute, bool fetchTomorrow = false);

private:
    int currentMonth;
    int currentDay;
    bool isLaterThan(int hour, int minute, const String &timeStr);
};

#endif