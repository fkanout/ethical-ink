#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>

class WiFiManager
{
public:
    static WiFiManager *getInstance();

    void begin(const char *ssid, const char *password, const char *ntpServer, long utcOffset);
    void connectWiFi();
    void scanWifiNetworks();
    void syncTime();
    void updateRTC(time_t epochTime);
    void printCurrentTime();
    bool isTimeSynced() const;
    void connectAsync(const String &json);
    void deferAsyncConnect(const String &json);
    static void connectTask(void *param);
    void connectNow();
    void scheduleRetry(uint32_t delayMs);
    void updateNTPServer(const String &ntpServer);
    static void handleWiFiEvent(WiFiEvent_t event);
    static void retryTask(void *param);
    void autoConnectFromFile();

private:
    WiFiManager() = default;
    int retryCount = 0;
    const int maxRetries = 3;
    bool isConnecting = false; // prevent duplicate attempts
    // ✅ Use String (manages memory safely)
    String _ssid;
    String _password;
    String _ntpServer;
    long _utcOffset = 0;

    WiFiUDP _udp;
    NTPClient _timeClient = NTPClient(_udp); // still fine here
    bool timeSynced = false;
};
#endif // WIFI_MANAGER_H