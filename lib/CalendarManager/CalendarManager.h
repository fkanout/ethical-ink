#ifndef CALENDAR_MANAGER_H
#define CALENDAR_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <vector>

struct TodayPrayerTimes
{
    std::vector<String> prayerTimes;
    bool isLastDayOfMonth = false;
};
class CalendarManager
{
public:
    CalendarManager();
    String getMonthFilePath(int month);
    TodayPrayerTimes fetchTodayPrayerTimes(int month, int day);
    String getNextPrayerTimeForToday(int month, int day, int currentHour, int currentMinute, bool fetchTomorrow = false);

private:
    int currentMonth;
    int currentDay;
    bool isLaterThan(int hour, int minute, const String &timeStr);
};

#endif