// #pragma once
// #include <Arduino.h>
// #include <WiFi.h>
// #include <NTPClient.h>
// #include <WiFiUdp.h>
// #include <ArduinoJson.h>
// #include "SPIFFSHelper.h"

// class WiFiManager
// {
// public:
//     static WiFiManager *getInstance();

//     // Set configuration (does not start connection)
//     void begin(const char *ssid, const char *password, const char *ntpServer,
//     long utcOffset);

//     // Initiates connection using stored credentials.
//     void connectNow();

//     // Parses JSON from BLE and starts connection.
//     void connectAsync(const String &json);

//     // Loads credentials from SPIFFS and initiates connection.
//     void autoConnectFromFile();

//     // Called from BLE tasks (non-blocking).
//     void deferAsyncConnect(const String &json);

//     // Initiates a Wi-Fi scan and sends the list over BLE.
//     void scanWifiNetworks();

//     // Synchronize time using NTP.
//     void syncTime();
//     void printCurrentTime();

//     bool isTimeSynced() const { return timeSynced; }

//     // Retry connection logic.
//     void retryConnection();

//     // Static Wi-Fi event handler.
//     static void handleWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info);
//     // Static task function for periodic retry.
//     static void retryTask(void *param);
//     static void connectTask(void *param);
//     void syncTimeWithTimeout(uint32_t timeoutMs, int maxRetries);

//     const int maxRetries = 3;

// private:
//     WiFiManager();

//     // Stored configuration.
//     String _ssid;
//     String _password;
//     String _ntpServer;
//     long _utcOffset = 0;

//     WiFiUDP _udp;
//     NTPClient _timeClient; // Initialized in begin()
//     bool timeSynced = false;

//     // Retry state.
//     int retryCount = 0;
//     bool isConnecting = false;
//     bool retryTaskRunning = false;
// };