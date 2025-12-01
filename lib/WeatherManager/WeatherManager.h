#ifndef WEATHER_MANAGER_H
#define WEATHER_MANAGER_H

#include <Arduino.h>
#include <functional>

class WeatherManager {
private:
  WeatherManager() {}
  WeatherManager(const WeatherManager &) = delete;
  WeatherManager &operator=(const WeatherManager &) = delete;

  static void fetchTask(void *parameter);

public:
  static WeatherManager &getInstance();

  using FetchCallback = std::function<void(bool success)>;

  void asyncFetchWeather(float latitude, float longitude, FetchCallback callback);
};

#endif // WEATHER_MANAGER_H


