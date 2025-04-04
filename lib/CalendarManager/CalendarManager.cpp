#include "CalendarManager.h"
#include <SPIFFS.h>
#include <vector>

CalendarManager::CalendarManager() : currentMonth(0), currentDay(0) {}

String CalendarManager::getMonthFilePath(int month)
{
    return "/prayer_times_month_" + String(month) + ".json";
}

TodayPrayerTimes CalendarManager::fetchTodayPrayerTimes(int month, int day)
{
    TodayPrayerTimes result;

    String filePath = getMonthFilePath(month);
    File file = SPIFFS.open(filePath, "r");
    if (!file)
    {
        Serial.println("❌ Failed to open file: " + filePath);
        return result;
    }

    String fileContent = file.readString();
    file.close();

    DynamicJsonDocument doc(1024); // Use dynamic allocation here
    DeserializationError error = deserializeJson(doc, fileContent);
    if (error)
    {
        Serial.println("❌ JSON parsing failed");
        return result;
    }

    JsonObject monthData = doc.as<JsonObject>();
    JsonObject dayDataCalendar = monthData["prayerCalender"].as<JsonObject>();
    String dayKey = String(day);

    if (dayDataCalendar[dayKey].is<JsonArray>())
    {
        JsonArray prayerTimes = dayDataCalendar[dayKey].as<JsonArray>();
        for (const auto &time : prayerTimes)
        {
            result.prayerTimes.push_back(time.as<String>());
        }
        // Check if the next day exists in the calendar
        String nextDayKey = String(day + 1);
        if (!dayDataCalendar[nextDayKey].is<JsonArray>())
        {
            result.isLastDayOfMonth = true; // This is the last day of the month if the next day doesn't exist
        }
    }

    for (const auto &time : result.prayerTimes)
    {
        Serial.println("    ⏰ " + time);
    }

    return result;
}
bool CalendarManager::isLaterThan(int hour, int minute, const String &timeStr)
{
    int h = timeStr.substring(0, 2).toInt();
    int m = timeStr.substring(3, 5).toInt();

    if (h > hour)
        return true;
    if (h == hour && m > minute)
        return true;
    return false;
}

String CalendarManager::getNextPrayerTimeForToday(int month, int day, int currentHour, int currentMinute, bool fetchTomorrow)
{
    TodayPrayerTimes todayPrayerTime = fetchTodayPrayerTimes(month, day);
    if (fetchTomorrow)
    {
        Serial.println("🔄 Fetching tomorrow's prayer times");
        return todayPrayerTime.prayerTimes[0]; // Return the first prayer time of the next day
    }
    for (const String &time : todayPrayerTime.prayerTimes)
    {
        if (isLaterThan(currentHour, currentMinute, time))
        {
            return time; // Found the next prayer time
        }
    }
    Serial.println("🌙 All prayers for today have passed.");

    if (todayPrayerTime.isLastDayOfMonth)
    {
        Serial.println("🔄 fetch the next day's prayer times in the next month");
        int nextMonth = (month % 12) + 1;
        int nextDay = 1;
        return getNextPrayerTimeForToday(nextMonth, nextDay, currentHour, currentMinute, true);
    }
    else
    {
        Serial.println("🔄 Fetch the next day");
        int nextMonth = month;
        int nextDay = day + 1;
        return getNextPrayerTimeForToday(nextMonth, nextDay, currentHour, currentMinute, true);
    }
}