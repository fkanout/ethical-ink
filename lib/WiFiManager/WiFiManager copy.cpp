// #include "WiFiManager.h"
// #include "BLEManager.h" // For sending BLE notifications
// #include <RTCManager.h>

// // Singleton instance getter.
// WiFiManager *WiFiManager::getInstance()
// {
//     static WiFiManager instance;
//     return &instance;
// }

// WiFiManager::WiFiManager() : _timeClient(_udp) {}

// // Stores configuration without initiating connection.
// void WiFiManager::begin(const char *ssid, const char *password, const char
// *ntpServer, long utcOffset)
// {
//     Serial.println("📦 WiFiManager.begin() called");

//     _ssid = String(ssid);
//     _password = String(password);
//     _ntpServer = String(ntpServer);
//     _utcOffset = utcOffset;
//     _timeClient = NTPClient(_udp, _ntpServer.c_str(), _utcOffset);
// }

// // Initiates connection using stored credentials.
// void WiFiManager::connectNow()
// {
//     retryCount = 0;
//     isConnecting = true;
//     Serial.printf("🔁 Connecting to Wi-Fi (attempt %d/%d)...\n", retryCount +
//     1, maxRetries); WiFi.disconnect(true); delay(100); WiFi.mode(WIFI_STA);
//     WiFi.setAutoReconnect(false);
//     WiFi.begin(_ssid.c_str(), _password.c_str());

//     // Register event handler once.
//     static bool eventHooked = false;
//     if (!eventHooked)
//     {
//         WiFi.onEvent(WiFiManager::handleWiFiEvent);
//         eventHooked = true;
//     }
// }

// // Called by the BLE task context to parse JSON and connect.
// void WiFiManager::connectAsync(const String &json)
// {
//     Serial.println("⚙️ connectAsync running");
//     Serial.println("📨 JSON: " + json);

//     DynamicJsonDocument doc(256);
//     DeserializationError err = deserializeJson(doc, json);
//     if (err)
//     {
//         Serial.println("❌ JSON parse failed");
//         return;
//     }

//     String ssid = doc["ssid"].as<String>();
//     String password = doc["password"].as<String>();
//     ssid.trim();
//     password.trim();

//     // Save cleaned credentials to SPIFFS.
//     DynamicJsonDocument outDoc(256);
//     outDoc["ssid"] = ssid;
//     outDoc["password"] = password;
//     String cleanedJson;
//     serializeJson(outDoc, cleanedJson);
//     writeJsonFile("/wifi.json", cleanedJson);
//     Serial.println("💾 Credentials saved to /wifi.json");

//     // Update configuration and connect.
//     begin(ssid.c_str(), password.c_str(), "pool.ntp.org", 3600);
//     connectNow();
//     Serial.printf("📦 Free heap: %u bytes\n", ESP.getFreeHeap());
// }

// // Called from BLE tasks to offload processing.
// void WiFiManager::deferAsyncConnect(const String &json)
// {
//     String *data = new String(json);
//     xTaskCreatePinnedToCore(connectTask, "wifi_task", 12288, data, 1, NULL,
//     1);
// }

// // Task to process BLE-provided JSON (non-blocking).
// void WiFiManager::connectTask(void *param)
// {
//     String *jsonPtr = static_cast<String *>(param);
//     String json = *jsonPtr;
//     delete jsonPtr;
//     WiFiManager::getInstance()->connectAsync(json);
//     vTaskDelete(NULL);
// }

// // Loads stored credentials from SPIFFS and initiates connection.
// void WiFiManager::autoConnectFromFile()
// {
//     Serial.println("📂 Attempting to load Wi-Fi credentials from
//     /wifi.json"); String json = readJsonFile("/wifi.json"); if
//     (json.isEmpty() || json == "{}")
//     {
//         Serial.println("⚠️ No valid Wi-Fi credentials found.");
//         return;
//     }
//     DynamicJsonDocument doc(256);
//     DeserializationError err = deserializeJson(doc, json);
//     if (err)
//     {
//         Serial.println("❌ Failed to parse /wifi.json");
//         return;
//     }

//     String ssid = doc["ssid"].as<String>();
//     String password = doc["password"].as<String>();
//     ssid.trim();
//     password.trim();

//     if (ssid.isEmpty() || password.isEmpty())
//     {
//         Serial.println("⚠️ Incomplete Wi-Fi credentials.");
//         return;
//     }

//     Serial.println("✅ Loaded saved Wi-Fi credentials:");
//     Serial.println("📶 SSID: " + ssid);
//     Serial.println("🔑 Password: " + password);

//     begin(ssid.c_str(), password.c_str(), "pool.ntp.org", 3600);
//     connectNow();
// }

// // Scans for available networks and sends the list over BLE.
// void WiFiManager::scanWifiNetworks()
// {
//     Serial.println("🔍 Starting WiFi scan...");
//     WiFi.mode(WIFI_STA);
//     int numNetworks = WiFi.scanNetworks();
//     if (numNetworks == WIFI_SCAN_FAILED)
//     {
//         Serial.println("❌ Scan failed. Try again.");
//         return;
//     }
//     if (numNetworks == 0)
//     {
//         Serial.println("❌ No networks found.");
//         BLEManager::sendBLEData("[]");
//         return;
//     }

//     Serial.printf("✅ %d networks found:\n", numNetworks);
//     String json = "[";
//     for (int i = 0; i < numNetworks; i++)
//     {
//         String netSSID = WiFi.SSID(i);
//         int rssi = WiFi.RSSI(i);
//         bool secured = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
//         json += "{";
//         json += "\"ssid\":\"" + netSSID + "\",";
//         json += "\"rssi\":" + String(rssi) + ",";
//         json += "\"secured\":" + String(secured ? "true" : "false");
//         json += "}";
//         if (i < numNetworks - 1)
//             json += ",";
//     }
//     json += "]";
//     BLEManager::sendBLEData(json);
//     WiFi.scanDelete();
// }

// void WiFiManager::syncTimeWithTimeout(uint32_t timeoutMs, int maxRetries)
// {
//     Serial.println("⏳ Fetching time from NTP server...");
//     _timeClient.begin(); // Initialize the NTP client
//     uint32_t startTime = millis();
//     int retries = 0;

//     // Attempt to update until the client successfully updates or we time
//     out. while (!_timeClient.update())
//     {
//         // Check if the current attempt has exceeded the timeout.
//         if (millis() - startTime > timeoutMs)
//         {
//             if (retries < maxRetries)
//             {
//                 retries++;
//                 Serial.printf("⏳ NTP sync timeout, retrying (%d/%d)...\n",
//                 retries, maxRetries);
//                 // Reset timer for the next attempt.
//                 startTime = millis();
//             }
//             else
//             {
//                 Serial.println("❌ NTP sync timed out after maximum
//                 retries.");
//             }
//         }
//         _timeClient.forceUpdate(); // Force update (blocking call)
//         delay(500);                // Short delay between retries to avoid
//         overwhelming the NTP server
//     }

//     // When successful, update the RTC with the fetched time.
//     time_t epochTime = _timeClient.getEpochTime();
//     RTCManager::getInstance()->updateRTC(epochTime);
//     timeSynced = true;
//     Serial.println("✅ Time fetched from NTP: " +
//     _timeClient.getFormattedTime());
// }

// // Prints the current system time.
// void WiFiManager::printCurrentTime()
// {
//     struct tm timeinfo;
//     if (getLocalTime(&timeinfo))
//     {
//         Serial.printf("📅 Current Time: %04d-%02d-%02d %02d:%02d:%02d\n",
//                       timeinfo.tm_year + 1900, timeinfo.tm_mon + 1,
//                       timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min,
//                       timeinfo.tm_sec);
//     }
//     else
//     {
//         Serial.println("❌ Failed to get time");
//         timeSynced = false;
//     }
// }

// // Retry connection if disconnected.
// void WiFiManager::retryConnection()
// {
//     if (retryCount < maxRetries)
//     {
//         retryCount++;
//         Serial.printf("🔁 Retrying Wi-Fi connection (attempt %d/%d)...\n",
//         retryCount + 1, maxRetries); WiFi.disconnect(true); delay(100);
//         WiFi.begin(_ssid.c_str(), _password.c_str());
//     }
//     else
//     {
//         isConnecting = false;
//         Serial.println("🛑 Max Wi-Fi retries reached.");
//         BLEManager::sendBLEData("{\"wifiStatus\":\"failed\"}");
//     }
// }

// // Static Wi-Fi event handler.
// void WiFiManager::handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info)
// {
//     {
//         WiFiManager *wm = WiFiManager::getInstance();
//         switch (event)
//         {
//         case SYSTEM_EVENT_STA_CONNECTED:
//             Serial.println("📶 Wi-Fi connected to AP");
//             break;
//         case SYSTEM_EVENT_STA_GOT_IP:
//             Serial.println("📡 Got IP");
//             Serial.println(WiFi.localIP());
//             wm->syncTimeWithTimeout(10000, 5); // Sync time with timeout
//             // Save working credentials.
//             {
//                 String json = "{\"ssid\":\"" + wm->_ssid +
//                 "\",\"password\":\"" + wm->_password + "\"}";
//                 writeJsonFile("/wifi.json", json);
//                 Serial.println("💾 Credentials saved to /wifi.json");
//             }
//             BLEManager::sendBLEData("{\"wifiStatus\":\"connected\"}");
//             break;
//         case SYSTEM_EVENT_STA_DISCONNECTED:
//             Serial.println("❌ Wi-Fi disconnected");
//             Serial.printf("🔄 Disconnection reason: %d\n",
//             info.wifi_sta_disconnected.reason);

//             if (info.wifi_sta_disconnected.reason == WIFI_REASON_AUTH_EXPIRE)
//             {
//                 Serial.println("🔄 AUTH_EXPIRE received — forcing
//                 reauthentication..."); WiFi.disconnect(true); delay(100); if
//                 (!wm->retryTaskRunning)
//                 {
//                     wm->retryTaskRunning = true;
//                     xTaskCreatePinnedToCore(WiFiManager::retryTask,
//                     "wifi_retry", 4096, NULL, 1, NULL, 1);
//                 }
//             }
//             break;
//         default:
//             break;
//         }
//     }
// }

// // Static retry task function (Option B).
// void WiFiManager::retryTask(void *param)
// {
//     WiFiManager *wm = WiFiManager::getInstance();
//     // Loop and check Wi-Fi status every 10 seconds.
//     while (wm->isConnecting && WiFi.status() != WL_CONNECTED &&
//     wm->retryCount < wm->maxRetries)
//     {
//         Serial.println("🔁 Periodic retry task running...");
//         vTaskDelay(10000 / portTICK_PERIOD_MS);
//         wm->retryConnection();
//     }
//     wm->retryTaskRunning = false; // Reset flag when done.
//     vTaskDelete(NULL);
// }