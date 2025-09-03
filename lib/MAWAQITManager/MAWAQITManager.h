#pragma once
#include <Arduino.h>
#include <functional>

class MAWAQITManager {
public:
  // Callback: success flag, and file path to valid data (or nullptr on failure)
  using FetchCallback = std::function<void(bool success, const char *filePath)>;

  static MAWAQITManager &getInstance();
  

  // Set your MAWAQIT API key before calling asyncFetchPrayerTimes
  void setApiKey(const String &key);

  // Launch non-blocking prayer time fetch with retry and validation
  void asyncFetchPrayerTimes(const String &mosqueUUID, FetchCallback callback);
  
  void asyncFetchMosqueInfo(const String &mosqueUUID, FetchCallback callback);

private:
  MAWAQITManager() = default;
  static void fetchTask(void *parameter);

  TaskHandle_t fetchTaskHandle = nullptr;
  String apiKey;

  struct FetchParams {
    String mosqueUUID;
    FetchCallback callback;
  };

  struct InfoFetchParams {
  String mosqueUUID;
  FetchCallback callback;
};
static void fetchInfoTask(void *parameter);
TaskHandle_t fetchInfoTaskHandle = nullptr;
};