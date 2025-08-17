#include "EventsManager.h"

void EventsManager::setAccessToken(const String &token) { accessToken = token; }

void EventsManager::asyncFetchEvents(FetchCallback callback) {
  if (fetchTaskHandle != nullptr) {
    Serial.println("🛑 Stopping existing event fetch task...");
    vTaskDelete(fetchTaskHandle);
    fetchTaskHandle = nullptr;
  }

  FetchParams *params = new FetchParams{callback};
  xTaskCreate(fetchTask, "EventsFetchTask", 8192, params, 1, &fetchTaskHandle);
}

void EventsManager::fetchTask(void *parameter) {
  FetchParams *params = static_cast<FetchParams *>(parameter);
  bool success = false;

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Not connected to Wi-Fi. Aborting events fetch.");
    if (params->callback) {
      params->callback(false, nullptr);
    }
    delete params;
    vTaskDelete(nullptr);
    return;
  }

  // Build ISO8601 time range: now to now + 7 days
  time_t now = time(nullptr);
  char timeMin[40];
  char timeMax[40];

  strftime(timeMin, sizeof(timeMin), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));

  time_t oneWeekLater = now + 7 * 24 * 60 * 60;
  strftime(timeMax, sizeof(timeMax), "%Y-%m-%dT%H:%M:%SZ",
           gmtime(&oneWeekLater));

  String url =
      "https://www.googleapis.com/calendar/v3/calendars/primary/events?";
  url += "timeMin=" + String(timeMin);
  url += "&timeMax=" + String(timeMax);
  url += "&maxResults=20&orderBy=startTime&singleEvents=true";

  Serial.printf("🌐 Fetching events from: %s\n", url.c_str());

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10000);

  HTTPClient https;
  if (!https.begin(client, url)) {
    Serial.println("❌ HTTPS.begin() failed");
  } else {
    https.addHeader("Authorization",
                    String("Bearer ") + getInstance().accessToken);
    https.setTimeout(10000);

    int httpCode = https.GET();
    if (httpCode != HTTP_CODE_OK) {
      Serial.printf("❌ HTTP GET failed: %d\n", httpCode);
    } else {
      String json = https.getString();
      Serial.printf("📦 Received %d bytes\n", json.length());

      StaticJsonDocument<16384> sourceDoc;
      DeserializationError error = deserializeJson(sourceDoc, json);
      if (error) {
        Serial.printf("❌ JSON validation failed: %s\n", error.c_str());
      } else {
        JsonArray items = sourceDoc["items"].as<JsonArray>();
        StaticJsonDocument<8192> filteredDoc;
        JsonArray filtered = filteredDoc.createNestedArray("events");

        for (JsonObject event : items) {
          const char *type = event["eventType"] | "default";
          if (strcmp(type, "default") != 0)
            continue;

          JsonObject dst = filtered.createNestedObject();
          dst["summary"] = event["summary"] | "(no title)";
          dst["start"] = event["start"]["dateTime"] | event["start"]["date"];
          dst["creator"] = event["creator"]["displayName"] |
                           event["creator"]["email"] | "(unknown)";
          JsonArray attendees = event["attendees"].as<JsonArray>();
          for (JsonObject attendee : attendees) {
            if (attendee["self"] == true) {
              dst["responseStatus"] = attendee["responseStatus"];
              break;
            }
          }
        }

        File file = SPIFFS.open(EVENTS_JSON_PATH, FILE_WRITE);
        if (!file) {
          Serial.println("❌ Failed to open file for writing");
        } else {
          serializeJson(filteredDoc, file);
          file.close();
          Serial.printf("✅ Filtered events saved to %s\n", EVENTS_JSON_PATH);
          success = true;
        }
      }
    }
    https.end();
  }

  if (params->callback) {
    params->callback(success, success ? EVENTS_JSON_PATH : nullptr);
  }

  delete params;
  getInstance().fetchTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

void EventsManager::listEventsForNextWeek() {
  File file = SPIFFS.open(EVENTS_JSON_PATH, FILE_READ);
  if (!file) {
    Serial.println("❌ Cannot open events.json");
    return;
  }

  StaticJsonDocument<8192> doc;
  DeserializationError err = deserializeJson(doc, file);
  file.close();

  if (err) {
    Serial.printf("❌ JSON parse error: %s\n", err.c_str());
    return;
  }

  JsonArray events = doc["events"].as<JsonArray>();
  if (!events || events.size() == 0) {
    Serial.println("📭 No filtered events this week");
    return;
  }

  Serial.println("📅 Events for the next 7 days:");
  for (JsonObject event : events) {
    const char *summary = event["summary"] | "(no title)";
    const char *start = event["start"];
    const char *creator = event["creator"];
    const char *status = event["responseStatus"] | "unknown";
    Serial.printf("🔹 %s at %s | Creator: %s | Status: %s\n", summary, start,
                  creator, status);
  }
}
