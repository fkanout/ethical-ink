#include "WiFiManager.h"
#include "AppState.h"
#include <ArduinoJson.h>
#include <esp_wifi.h>

#include <SPIFFSHelper.h>
#include <vector>

WiFiManager::WiFiManager() {
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
  
  // Improve WiFi stability
  WiFi.persistent(true);  // Store credentials in flash
  WiFi.setAutoConnect(false);  // Manual control
  
  // MUST enable modem sleep for WiFi/BLE coexistence
  // WIFI_PS_MIN_MODEM is required when BLE is active
  esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  
  // Boost WiFi TX power to maximum for better range/stability
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // Maximum power (was default ~17dBm)
}

WiFiManager &WiFiManager::getInstance() {
  static WiFiManager instance;
  return instance;
}
void WiFiManager::setScanResultCallback(ScanCallback cb) {
  scanResultCallback = cb;
}
void WiFiManager::onWifiConnectedCallback(WifiConnectedCallback cb) {
  onWifiConnected = cb;
}
void WiFiManager::onWifiFailedToConnectCallback(
    WifiFailedToConnectCallback cb) {
  onWifiFailedToConnect = cb;
}
void WiFiManager::asyncScanNetworks() {
  if (connectTaskHandle != nullptr) {
    Serial.println("🛑 Stopping ongoing Wi-Fi connection task...");
    vTaskDelete(connectTaskHandle);
    connectTaskHandle = nullptr;
  }
  xTaskCreate(scanTask, "ScanTask", 8192, nullptr, 1, nullptr);
}

void WiFiManager::asyncConnect(const char *ssid, const char *password) {
  if (!ssid || strlen(ssid) == 0) {
    Serial.println("❌ asyncConnect: SSID is empty — aborting connection.");
    return;
  }
  ConnectParams *params = new ConnectParams;
  params->ssid = strdup(ssid);         // deep copy
  params->password = strdup(password); // deep copy
  xTaskCreate(connectTask, "ConnectTask", 8192, params, 1, &connectTaskHandle);
}

void WiFiManager::asyncConnectWithSavedCredentials() {
  String wifiJsonString = readJsonFile(WIFI_CRED_FILE);
  if (wifiJsonString.isEmpty() || wifiJsonString == "{}") {
    Serial.println("⚠️ No valid Wi-Fi credentials found - "
                   "asyncConnectWithSavedCredentials");
    // Trigger failure callback to allow fallback to BLE
    if (WiFiManager::getInstance().onWifiFailedToConnect) {
      WiFiManager::getInstance().onWifiFailedToConnect();
    }
    return;
  }
  DynamicJsonDocument doc(256);
  DeserializationError errorParsingWifi = deserializeJson(doc, wifiJsonString);
  if (errorParsingWifi) {
    Serial.println("❌ JSON parse failed");
    // Trigger failure callback
    if (WiFiManager::getInstance().onWifiFailedToConnect) {
      WiFiManager::getInstance().onWifiFailedToConnect();
    }
    return;
  } else {
    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();
    if (ssid.isEmpty() || password.isEmpty()) {
      Serial.println("⚠️ Incomplete Wi-Fi credentials.");
      if (WiFiManager::getInstance().onWifiFailedToConnect) {
        WiFiManager::getInstance().onWifiFailedToConnect();
      }
      return;
    }

    ConnectParams *params = new ConnectParams;
    params->ssid = strdup(ssid.c_str());
    params->password = strdup(password.c_str());
    xTaskCreate(connectTask, "ConnectTask", 8192, params, 1,
                &connectTaskHandle);
  }
}

void WiFiManager::scanTask(void *parameter) {
  (void)parameter;
  std::vector<ScanResult> results;

  Serial.println("🔍 Scanning WiFi networks...");
  int numNetworks = WiFi.scanNetworks();
  if (numNetworks <= 0) {
    Serial.println("❌ No WiFi networks found.");
  } else {
    Serial.printf("✅ Found %d networks:\n", numNetworks);
    for (int i = 0; i < numNetworks; ++i) {
      ScanResult result = {.ssid = WiFi.SSID(i),
                           .rssi = WiFi.RSSI(i),
                           .secured =
                               (WiFi.encryptionType(i) != WIFI_AUTH_OPEN)};
      results.push_back(result);
    }
    if (WiFiManager::getInstance().scanResultCallback) {
      WiFiManager::getInstance().scanResultCallback(results);
    }
  }

  WiFi.scanDelete();
  vTaskDelete(NULL);
}

void WiFiManager::connectTask(void *parameter) {
  ConnectParams *params = static_cast<ConnectParams *>(parameter);
  bool connected = false;

  for (int attempt = 1; attempt <= WIFI_MAX_RETRIES; attempt++) {
    Serial.printf("🔄 Attempt %d/%d: Connecting to %s\n", attempt, WIFI_MAX_RETRIES, params->ssid);

    // Properly disconnect and wait
    WiFi.disconnect(true);  // true = turn off WiFi
    vTaskDelay(500 / portTICK_PERIOD_MS);  // Wait longer for complete disconnect
    
    // Reconnect
    WiFi.mode(WIFI_STA);  // Ensure station mode
    WiFi.begin(params->ssid, params->password);

    // Wait for connection with timeout
    unsigned long start = millis();
    while ((WiFi.status() != WL_CONNECTED) && (millis() - start < 15000)) {  // Increased to 15 seconds
      vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ Connected to Wi‑Fi!");
      Serial.print("📡 IP Address: ");
      Serial.println(WiFi.localIP());
      connected = true;
      String json = "{\"ssid\":\"" + String(params->ssid) +
                    "\",\"password\":\"" + String(params->password) + "\"}";
      writeJsonFile(WIFI_CRED_FILE, json);
      Serial.printf("💾 Wifi Credentials saved to %s\n", WIFI_CRED_FILE);
      if (WiFiManager::getInstance().onWifiConnected) {
        WiFiManager::getInstance().onWifiConnected();
      }
      break;
    } else {
      Serial.printf("❌ Connection attempt %d failed. ", attempt);
      if (attempt < WIFI_MAX_RETRIES) {
        Serial.println("Retrying...");
        vTaskDelay(2000 / portTICK_PERIOD_MS);  // Wait 2 seconds before retry
      } else {
        Serial.println("No more retries.");
      }
    }
  }

  if (!connected) {
    Serial.println("❌ Failed to connect after all retries.");
    WiFi.disconnect();
    if (WiFiManager::getInstance().onWifiFailedToConnect) {
      WiFiManager::getInstance().onWifiFailedToConnect();
    }
    // Don't restart device - let the callback handle the failure
    // Restarting would lose RTC memory and cause boot loops
  }

  // Clean up
  WiFiManager::getInstance().connectTaskHandle = nullptr;
  free((void *)params->ssid);
  free((void *)params->password);
  delete params;
  vTaskDelete(NULL);
}