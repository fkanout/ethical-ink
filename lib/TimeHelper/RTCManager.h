#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H
#include <Arduino.h>

class RTCManager
{
private:
    static RTCManager *instance;
    static RTC_DATA_ATTR bool manualTimeSet;
    static RTC_DATA_ATTR bool timeSynced;
    RTCManager() {}

public:
    static RTCManager *getInstance();
    void setRtcTimeFromISO8601(const String &iso8601Time);
    void updateRTC(time_t epochTime);
    void printTime();
    bool isTimeSynced();
    void setTimeToSpecificHourAndMinute(int newHour, int newMinute, int newMonth = 0, int newDay = 0);
};
#endif