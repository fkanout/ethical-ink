#ifndef RTC_MANAGER_H
#define RTC_MANAGER_H

#include <Arduino.h>

class RTCManager {
private:
  // Private constructor to enforce singleton
  RTCManager() {}

  // Prevent copying
  RTCManager(const RTCManager &) = delete;
  RTCManager &operator=(const RTCManager &) = delete;

  // RTC retained flags
  static RTC_DATA_ATTR bool manualTimeSet;
  static RTC_DATA_ATTR bool timeSynced;

public:
  // Singleton accessor
  static RTCManager &getInstance();

  // Functionality
  void setRtcTimeFromISO8601(const String &iso8601Time);
  void updateRTC(time_t epochTime);
  void printTime();
  bool isTimeSynced();
  void setTimeToSpecificHourAndMinute(int newHour, int newMinute,
                                      int newMonth = 0, int newDay = 0);
  bool syncTimeFromNTPWithOffset(int maxRetries = 3,
                                 uint32_t timeoutMs = 10000,
                                 int timezoneOffsetSeconds = 0);
  time_t getEpochTime();
};

#endif // RTC_MANAGER_H