#ifndef ALADHAN_MANAGER_H
#define ALADHAN_MANAGER_H

#include <Arduino.h>
#include <functional>

class AladhanManager {
public:
  static AladhanManager &getInstance();

  using FetchCallback = std::function<void(bool success, const char *filePath)>;
  using ReverseGeocodeCallback = std::function<void(bool success, const String &cityName)>;

  void asyncFetchMonthlyPrayerTimes(float latitude, float longitude,
                                    int calculationMethod, int month, int year,
                                    FetchCallback callback);

  void asyncReverseGeocode(float latitude, float longitude,
                          ReverseGeocodeCallback callback);

private:
  AladhanManager() {}
  AladhanManager(const AladhanManager &) = delete;
  AladhanManager &operator=(const AladhanManager &) = delete;

  static void fetchTask(void *parameter);
  static void reverseGeocodeTask(void *parameter);
};

#endif // ALADHAN_MANAGER_H

