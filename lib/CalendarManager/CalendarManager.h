#ifndef CALENDAR_MANAGER_H
#define CALENDAR_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <ArduinoJson.h>

class CalendarManager {
public:
    CalendarManager();
    bool fetchTodayData(int month, int day);
    String getMonthFilePath(int month);

private:
    int currentMonth;
    int currentDay;
};

#endif