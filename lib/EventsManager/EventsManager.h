#ifndef EVENTS_MANAGER_H
#define EVENTS_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <HTTPClient.h>
#include <SPIFFS.h>
#include <WiFiClientSecure.h>

#define EVENTS_JSON_PATH "/events.json"
#define MAX_EVENT_FETCH_RETRIES 3

class EventsManager {
public:
  using FetchCallback = std::function<void(bool success, const char *path)>;

  static EventsManager &getInstance() {
    static EventsManager instance;
    return instance;
  }

  void setAccessToken(const String &token);
  void asyncFetchEvents(FetchCallback callback);
  void listEventsForNextWeek();

private:
  EventsManager() = default;
  EventsManager(const EventsManager &) = delete;
  EventsManager &operator=(const EventsManager &) = delete;

  static void fetchTask(void *parameter);
  String accessToken;
  TaskHandle_t fetchTaskHandle = nullptr;

  struct FetchParams {
    FetchCallback callback;
  };
};

#endif // EVENTS_MANAGER_H
