#include "CalendarManager.h"
#include <SPIFFS.h> 

CalendarManager::CalendarManager() : currentMonth(0), currentDay(0) {}

String CalendarManager::getMonthFilePath(int month) {
    return "/prayer_times_month_" + String(month) + ".json";
}

bool CalendarManager::fetchTodayData(int month, int day) {
    String filePath = getMonthFilePath(month);
    File file = SPIFFS.open(filePath, "r");
    if (!file) {
        Serial.println("❌ Failed to open file: " + filePath);
        return false;
    }

    String fileContent = file.readString();
    file.close();

    // Create a DynamicJsonDocument to hold the JSON data
    DynamicJsonDocument doc(2048);
    DeserializationError error = deserializeJson(doc, fileContent);
    if (error) {
        Serial.println("❌ JSON parsing failed");
        return false;
    }
    JsonObject monthData = doc.as<JsonObject>(); 
    JsonObject dayDataCalender = monthData["prayerCalender"].as<JsonObject>();
    String dayKey = String(day);
    Serial.printf("🔑 Looking for key: '%s'\n", dayKey.c_str());    
    if (dayDataCalender[dayKey].is<JsonArray>()) {
        JsonArray prayerTimes = dayDataCalender[dayKey].as<JsonArray>();

        Serial.printf("📅 Prayer times for day %d:\n", dayKey);
        for (size_t i = 0; i < prayerTimes.size(); i++) {
            Serial.printf("  ⏰ %s\n", prayerTimes[i].as<const char*>());
        }
    } else {
        Serial.printf("⚠️ No prayer times found for day %d!\n", dayKey);
    }   
    return true;
   
}