// StatusBar.h
#ifndef STATUSBAR_H
#define STATUSBAR_H

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

struct StatusInfo {
  String currentTime;     // "HH:MM"
  String currentDate;     // "Mon DD"
  bool wifiConnected;
  int wifiRssi;          // Signal strength
  bool bleAdvertising;
  bool timeValid;        // Whether RTC time is reliable
};

class StatusBar {
private:
  static const int WIFI_ICON_WIDTH = 48;
  static const int WIFI_ICON_HEIGHT = 32;
  static const int BLE_ICON_WIDTH = 32;
  static const int BLE_ICON_HEIGHT = 48;
  
public:
  static StatusInfo getStatusInfo(bool bleAdvertising = false) {
    StatusInfo status;
    
    // Get current time and date
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      status.timeValid = true;
      // Format time as HH:MM
      char timeStr[6];
      sprintf(timeStr, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
      status.currentTime = String(timeStr);
      
      // Format date as "Mon DD" for compact display
      const char* dayNames[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
      const char* monthNames[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
      char dateStr[12];
      sprintf(dateStr, "%s %02d", monthNames[timeinfo.tm_mon], timeinfo.tm_mday);
      status.currentDate = String(dateStr);
    } else {
      status.timeValid = false;
      status.currentTime = "--:--";
      status.currentDate = "-- --";
    }
    
    // WiFi status
    status.wifiConnected = (WiFi.status() == WL_CONNECTED);
    if (status.wifiConnected) {
      status.wifiRssi = WiFi.RSSI();
    } else {
      status.wifiRssi = -100; // Very weak signal indicator
    }
    
    // BLE status - passed as parameter since BLEManager might not have a global isAdvertising method
    status.bleAdvertising = bleAdvertising;
    
    return status;
  }
  
  // Draw WiFi icon based on signal strength
  template<typename DisplayType>
  static void drawWifiIcon(DisplayType& display, int x, int y, int rssi, bool connected) {
    if (!connected) {
      // Draw crossed out WiFi icon
      drawWifiSignalBars(display, x, y, 0);
      display.drawLine(x, y, x + WIFI_ICON_WIDTH - 1, y + WIFI_ICON_HEIGHT - 1, 0x0000);
      display.drawLine(x, y + WIFI_ICON_HEIGHT - 1, x + WIFI_ICON_WIDTH - 1, y, 0x0000);
      return;
    }
    
    // Determine signal strength (1-4 bars)
    int bars = 1;
    if (rssi > -60) bars = 4;
    else if (rssi > -70) bars = 3;
    else if (rssi > -80) bars = 2;
    
    drawWifiSignalBars(display, x, y, bars);
  }
  
  // Draw Bluetooth icon
  template<typename DisplayType>
  static void drawBluetoothIcon(DisplayType& display, int x, int y, bool advertising) {
    if (!advertising) return; // Don't draw if not advertising
    
    // Simple Bluetooth "B" icon
    display.drawLine(x + 2, y, x + 2, y + BLE_ICON_HEIGHT - 1, 0x0000);
    display.drawLine(x + 2, y, x + 8, y + 4, 0x0000);
    display.drawLine(x + 2, y + 8, x + 8, y + 4, 0x0000);
    display.drawLine(x + 2, y + 8, x + 8, y + 12, 0x0000);
    display.drawLine(x + 2, y + BLE_ICON_HEIGHT - 1, x + 8, y + 12, 0x0000);
    display.drawLine(x + 8, y + 4, x + 8, y + 12, 0x0000);
  }
  
  // Draw complete status bar
  template<typename DisplayType>
  static void drawStatusBar(DisplayType& display, int screenWidth, int screenHeight, 
                           const StatusInfo& status, const GFXfont* font) {
    const int STATUS_BAR_HEIGHT = 25;
    const int MARGIN = 8;
    
    // Set font for status bar
    display.setFont(font);
    display.setTextColor(0x0000); // Black text
    
    // Draw top border line
    display.drawLine(0, STATUS_BAR_HEIGHT, screenWidth, STATUS_BAR_HEIGHT, 0x0000);
    
    // Left side: Date
    display.setCursor(MARGIN, STATUS_BAR_HEIGHT - 6);
    display.print(status.currentDate.c_str());
    
    // Center: Current Time
    int16_t timeX, timeY;
    uint16_t timeW, timeH;
    display.getTextBounds(status.currentTime.c_str(), 0, 0, &timeX, &timeY, &timeW, &timeH);
    int centerX = (screenWidth - timeW) / 2;
    display.setCursor(centerX, STATUS_BAR_HEIGHT - 6);
    display.print(status.currentTime.c_str());
    
    // Right side: Icons
    int iconX = screenWidth - MARGIN;
    
    // WiFi icon (rightmost)
    iconX -= WIFI_ICON_WIDTH;
    drawWifiIcon(display, iconX, STATUS_BAR_HEIGHT - WIFI_ICON_HEIGHT - 2, 
                status.wifiRssi, status.wifiConnected);
    
    // Bluetooth icon (left of WiFi)
    if (status.bleAdvertising) {
      iconX -= (BLE_ICON_WIDTH + 6);
      drawBluetoothIcon(display, iconX, STATUS_BAR_HEIGHT - BLE_ICON_HEIGHT - 2, 
                       status.bleAdvertising);
    }
    
    // Time validity indicator (small filled rectangle if time is not synced)
    if (!status.timeValid) {
      display.fillRect(centerX + timeW + 8, STATUS_BAR_HEIGHT - 14, 4, 4, 0x0000);
    }
  }

private:
  template<typename DisplayType>
  static void drawWifiSignalBars(DisplayType& display, int x, int y, int bars) {
    // Draw WiFi signal bars (simplified representation)
    for (int i = 0; i < 4; i++) {
      int barHeight = ((i + 1) * 2 + 2)*2;
      int barX = x + (i * 5) + 2;
      int barY = y + WIFI_ICON_HEIGHT - barHeight;
      
      if (i < bars) {
        display.fillRect(barX, barY, 3, barHeight, 0x0000);
      } else {
        display.drawRect(barX, barY, 3, barHeight, 0x0000);
      }
    }
  }
};

#endif // STATUSBAR_H