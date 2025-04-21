#include "WiFiManager.h"
#include "AppState.h"
#include <ArduinoJson.h>

#include <SPIFFSHelper.h>
#include <vector>

WiFiManager::WiFiManager() {
  WiFi.setAutoReconnect(false);
  WiFi.mode(WIFI_STA);
}

WiFiManager &WiFiManager::getInstance() {
  static WiFiManager instance;
  return instance;
}
void WiFiManager::setScanResultCallback(ScanCallback cb) {
  scanResultCallback = cb;
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
    return;
  }
  DynamicJsonDocument doc(256);
  DeserializationError errorParsingWifi = deserializeJson(doc, wifiJsonString);
  if (errorParsingWifi) {
    Serial.println("❌ JSON parse failed");
    return;
  } else {
    String ssid = doc["ssid"].as<String>();
    String password = doc["password"].as<String>();
    Serial.println("📂 Read JSON: " + wifiJsonString);

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
    Serial.printf("🔄 Attempt %d: Connecting to %s\n", attempt, params->ssid);

    WiFi.disconnect();
    vTaskDelay(100 / portTICK_PERIOD_MS);

    WiFi.begin(params->ssid, params->password);

    unsigned long start = millis();
    while ((WiFi.status() != WL_CONNECTED) && (millis() - start < 10000)) {
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
      break;
    } else {
      Serial.println("❌ Connection attempt failed. Retrying...");
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
  }

  if (!connected) {
    Serial.println("❌ Failed to connect after all retries.");
    WiFi.disconnect();
  }

  // Clean up
  WiFiManager::getInstance().connectTaskHandle = nullptr;
  free((void *)params->ssid);
  free((void *)params->password);
  delete params;
  vTaskDelete(NULL);
}