#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

class WiFiManager {
public:
    WiFiManager(const char* ssid, const char* password, const char* ntpServer, long utcOffset);
    void connectWiFi();
    void syncTime();
    void printCurrentTime();
    void scanWifiNetworks();
    bool isTimeSynced() const; 


private:
    bool timeSynced; 
    const char* _ssid;
    const char* _password;
    const char* _ntpServer;
    long _utcOffset;

    WiFiUDP _udp;
    NTPClient _timeClient;

    void updateRTC(time_t epochTime);
};

#endif // WIFI_MANAGER_H