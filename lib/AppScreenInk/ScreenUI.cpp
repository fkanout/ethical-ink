// ScreenUI.cpp
#include "ScreenUI.h"
#include <AppStateManager.h>
#include <GxEPD2_BW.h> // for color constants (GxEPD_WHITE/BLACK). If you use others, define 0/1.
#include <WiFi.h>
#include <string.h>

extern RTCData rtcData;

#ifndef GxEPD_WHITE
#define GxEPD_WHITE 0xFFFF
#endif
#ifndef GxEPD_BLACK
#define GxEPD_BLACK 0x0000
#endif

ScreenUI::ScreenUI(IEpaper &epd, int16_t screenW, int16_t screenH)
    : d_(epd), W_(screenW), H_(screenH) {}

ScreenLayout ScreenUI::computeLayout() const {
  ScreenLayout L;
  L.contentStartY = L.statusBarHeight + 10; // 10px margin after status bar

  // Optimized for 800x480 display
  // size boxes of prayers
  L.boxW = W_ * 0.4; // 40% of screen width (320px)
  L.boxH = 25;       // Taller header box
  L.spacing = 20;
  L.boxX = (W_ - L.boxW) / 2;
  L.headerY = L.contentStartY; // Move mosque name up by 10px (removed +10)

  // Countdown section - centered on screen
  // size box of countdown
  L.countdownW = W_ * 0.5; // 50% of screen width (400px)
  L.countdownH = 100;      // Increased from 130 to 160 (added 30px to bottom)
  // Center vertically. (X will be computed where used to avoid changing the
  // header struct.)
  L.countdownY = L.headerY + L.boxH +
                 50; // Reduced from 60 to 50 - push countdown up by 10px

  // Prayer times row - increased height to accommodate iqama times
  L.prayerBoxW = (W_ - 60) / 5; // Divide screen evenly with margins
  L.prayerBoxH = 150;           // Fixed height
  L.prayerSpacing = 10;
  const int count = 5;
  L.rowW = count * L.prayerBoxW + (count - 1) * L.prayerSpacing;
  L.rowStartX = (W_ - L.rowW) / 2;
  L.rowY =
      H_ - L.prayerBoxH - 10; // Position at bottom of screen with 10px margin

  return L;
}

void ScreenUI::fullRenderWithStatusBar(
    const ScreenLayout &L, const char *mosqueName, const char *countdownStr,
    const char *prayerNames[5], const char *prayerTimes[5],
    const char *iqamaTimes[5], int highlightIndex,
    const StatusInfo &statusInfo) {
  d_.setFullWindow();
  d_.firstPage();
  do {
    // STEP 1: Draw prayer boxes with WHITE backgrounds FIRST
    const int16_t totalW = 5 * L.prayerBoxW + 4 * L.prayerSpacing;
    const int16_t startX = (W_ - totalW) / 2;
    
    for (int i = 0; i < 5; i++) {
      const int16_t x = startX + i * (L.prayerBoxW + L.prayerSpacing);
      
      // All boxes: WHITE background + border
      d_.fillRect(x, L.rowY, L.prayerBoxW, L.prayerBoxH, GxEPD_WHITE);
      d_.drawRect(x, L.rowY, L.prayerBoxW, L.prayerBoxH, GxEPD_WHITE);
      
      // If highlighted, add really thick BLACK underline that goes into the box
      if (i == highlightIndex) {
        int16_t underlineY = L.rowY + L.prayerBoxH - 7;  // Start 7px inside the box
        d_.fillRect(x, underlineY, L.prayerBoxW, 20, GxEPD_BLACK);  // 20px thick underline (30% thicker)
      }
    }
    
    // STEP 2: Fill black background (only above prayer boxes)
    d_.fillRect(0, 0, W_, L.rowY, GxEPD_BLACK);

    // STEP 3: Draw status bar with white text
    d_.setTextColor(GxEPD_WHITE);
    StatusBar::drawStatusBar(d_, W_, H_, statusInfo, &Cairo_Bold12pt7b);

    // Header (mosque name) - white text
    d_.setFont(&Cairo_Bold12pt7b);
    int16_t x1_name, y1_name;
    uint16_t w_name, h_name;
    const char *mosqueText = mosqueName ? mosqueName : "Mosque Name";
    d_.getTextBounds(mosqueText, 0, 0, &x1_name, &y1_name, &w_name, &h_name);
    int16_t textX_name = L.boxX + (L.boxW - w_name) / 2 - x1_name;
    int16_t textY_name = L.headerY + (L.boxH - h_name) / 2 - y1_name;
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX_name, textY_name);
    d_.print(mosqueText);


    // "Prayer in" label - white text - positioned just after mosque name
    const int16_t countdownX = (W_ - L.countdownW) / 2;
    const int16_t labelTopY =
        L.countdownY - 40; // 40px space above countdown box - 10px closer

    const char *hlName = (highlightIndex >= 0 && highlightIndex < 5 &&
                          prayerNames[highlightIndex])
                             ? prayerNames[highlightIndex]
                             : "Prayer";
    char labelBuf[32];
    snprintf(labelBuf, sizeof(labelBuf), "%s in", hlName);

    d_.setFont(&Cairo_Bold24pt7b);
    int16_t x1_label, y1_label;
    uint16_t w_label, h_label;
    d_.getTextBounds(labelBuf, 0, 0, &x1_label, &y1_label, &w_label, &h_label);
    int16_t textX_label = (W_ / 2) - w_label / 2 - x1_label;
    int16_t textY_label = labelTopY - y1_label;
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX_label, textY_label);
    d_.print(labelBuf);

    // Centered countdown box - white border and white text
    d_.drawRect(countdownX, L.countdownY, L.countdownW, L.countdownH,
                GxEPD_WHITE);

    // Draw countdown in large font - pushed to top of box
    d_.setFont(&Cairo_Bold60pt7b); // Changed from 70pt to 60pt
    int16_t x1_count, y1_count;
    uint16_t w_count, h_count;
    d_.getTextBounds(countdownStr, 0, 0, &x1_count, &y1_count, &w_count,
                     &h_count);
    int16_t textX_count = countdownX + (L.countdownW - w_count) / 2 - x1_count;
    int16_t textY_count = L.countdownY + (L.countdownH - h_count) / 2 -
                          y1_count; // Centered vertically in the box
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX_count, textY_count);
    d_.print(countdownStr);

    // Weather info - positioned to the right of countdown box
    if (rtcData.currentTemp != 0.0 || rtcData.weatherDesc[0] != '\0') {
      char tempText[16];
      // Format just the number, we'll draw the degree symbol manually
      snprintf(tempText, sizeof(tempText), "%.0f", rtcData.currentTemp);
      
      // Use 24pt font for temperature (smaller, distinct from icon)
      d_.setFont(&Cairo_Bold24pt7b);
      int16_t x1_temp, y1_temp;
      uint16_t w_temp, h_temp;
      d_.getTextBounds(tempText, 0, 0, &x1_temp, &y1_temp, &w_temp, &h_temp);
      
      const int spacing = 30;       // Space between countdown and weather
      const int iconSize = 60;      // Icon size (50pt = ~60px)
      const int iconTempGap = 6;    // Space between icon and temp (reduced to 6px)
      
      // Position to the right of countdown box
      int16_t weatherStartX = countdownX + L.countdownW + spacing;
      
      // Calculate baseline position centered on countdown box
      // Text baseline should be at the vertical center of countdown box
      int16_t baselineY = L.countdownY + (L.countdownH / 2) + (h_temp / 2);
      
      // Position icon lower by 6px and closer to text (independent of text position)
      // Icon is drawn with cursor at y + size, so icon top is at baseline - iconSize
      int16_t iconX = weatherStartX;
      int16_t iconY = baselineY - iconSize + 6;
      
      // Draw weather icon (left side, lowered by 6px)
      drawWeatherIcon(rtcData.weatherDesc, iconX, iconY, iconSize);
      
      // Re-set font to 24pt before printing to ensure correct size
      d_.setFont(&Cairo_Bold24pt7b);
      
      // Draw temperature (right of icon, moved up 5px to align with icon)
      int16_t textX_temp = weatherStartX + iconSize + iconTempGap;
      int16_t textY_temp = baselineY - 5;
      
      d_.setTextColor(GxEPD_WHITE);
      d_.setCursor(textX_temp, textY_temp);
      d_.print(tempText);
      
      // Draw manual degree circle (no 'C' text)
      // Position: top-right corner of the temperature number
      int16_t degreeX = textX_temp + w_temp + x1_temp + 6; 
      int16_t degreeY = textY_temp + y1_temp + 4; // Top corner of the number
      
      // Draw degree circle - 40% thicker (radius 4.2 ≈ 4, draw multiple circles for thickness)
      d_.drawCircle(degreeX, degreeY, 4, GxEPD_WHITE);
      d_.drawCircle(degreeX, degreeY, 5, GxEPD_WHITE); // Outer ring for thickness
    }

    // Get current RTC time to display under countdown
    struct tm timeinfo;
    char currentTimeStr[6] = "00:00";
    if (getLocalTime(&timeinfo)) {
      snprintf(currentTimeStr, sizeof(currentTimeStr), "%02d:%02d",
               timeinfo.tm_hour, timeinfo.tm_min);
    }

    // Draw current RTC time centered vertically on screen
    d_.setFont(&Cairo_Bold40pt7b);
    d_.getTextBounds(currentTimeStr, 0, 0, &x1_count, &y1_count, &w_count,
                     &h_count);
    textX_count =
        (W_ / 2) - (w_count / 2) - x1_count; // Center horizontally on screen
    textY_count =
        (H_ / 2) - 20 -
        y1_count; // Center vertically on screen (slightly above center)
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX_count, textY_count);
    d_.print(currentTimeStr);

    // STEP 4: Draw TEXT inside prayer boxes (boxes already drawn in STEP 1)
    for (int i = 0; i < 5; i++) {
      const int16_t x = startX + i * (L.prayerBoxW + L.prayerSpacing);

      // Calculate Iqama delay
      int delay = 0;
      if (prayerTimes[i] && iqamaTimes[i]) {
        if (strchr(iqamaTimes[i], ':') == nullptr) {
          delay = atoi(iqamaTimes[i]);
        } else {
          int prayerH = 0, prayerM = 0, iqamaH = 0, iqamaM = 0;
          sscanf(prayerTimes[i], "%d:%d", &prayerH, &prayerM);
          sscanf(iqamaTimes[i], "%d:%d", &iqamaH, &iqamaM);
          delay = (iqamaH * 60 + iqamaM) - (prayerH * 60 + prayerM);
        }
      }
      char iqamaDelayStr[8];
      snprintf(iqamaDelayStr, sizeof(iqamaDelayStr), "+%d", delay);

      int16_t section1 = L.rowY + L.prayerBoxH / 3;
      int16_t section2 = L.rowY + (L.prayerBoxH * 2) / 3;

      // Draw text - BLACK text on white background for all boxes
      uint16_t textColor = GxEPD_BLACK;
      
      // Prayer Name
      d_.setFont(&Cairo_Bold18pt7b);
      int16_t x1, y1;
      uint16_t w, h;
      d_.getTextBounds(prayerNames[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (L.prayerBoxW - w) / 2 - x1;
      int16_t nameY = section1 - 5;
      d_.setTextColor(textColor);
      d_.setCursor(nameX, nameY);
      d_.print(prayerNames[i]);

      // Prayer Time
      d_.setFont(&Cairo_Bold24pt7b);
      d_.getTextBounds(prayerTimes[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (L.prayerBoxW - w) / 2 - x1;
      int16_t timeY = section2 - 10;
      d_.setTextColor(textColor);
      d_.setCursor(timeX, timeY);
      d_.print(prayerTimes[i]);

      // Iqama Delay (only show if delay > 0)
      if (delay > 0) {
        d_.setFont(&Cairo_Bold24pt7b);
        d_.getTextBounds(iqamaDelayStr, 0, 0, &x1, &y1, &w, &h);
        int16_t iqamaX = x + (L.prayerBoxW - w) / 2 - x1;
        int16_t iqamaY = L.rowY + L.prayerBoxH - 10;
        d_.setTextColor(textColor);
        d_.setCursor(iqamaX, iqamaY);
        d_.print(iqamaDelayStr);
      }
    }
  } while (d_.nextPage());
}

void ScreenUI::partialRenderWithStatusBar(
    const ScreenLayout &L, const char *mosqueName, const char *countdownStr,
    const char *prayerNames[5], const char *prayerTimes[5],
    const char *iqamaTimes[5], int highlightIndex,
    const StatusInfo &statusInfo) {
  // Update status bar
  redrawStatusBarRegion(statusInfo);

  // Get current RTC time to display under countdown
  struct tm timeinfo;
  char currentTimeStr[6] = "00:00";
  if (getLocalTime(&timeinfo)) {
    snprintf(currentTimeStr, sizeof(currentTimeStr), "%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min);
  }

  // Only update countdown with current time - skip header and prayer
  // boxes since they haven't changed
  redrawCountdownRegion(L, countdownStr, currentTimeStr);
}

void ScreenUI::redrawStatusBarRegion(const StatusInfo &statusInfo) {
  const int statusBarHeight = ScreenLayout::statusBarHeight;
  d_.setPartialWindow(0, 0, W_, statusBarHeight);
  d_.firstPage();
  do {
    d_.fillRect(0, 0, W_, statusBarHeight, GxEPD_BLACK); // Black background
    d_.setTextColor(GxEPD_WHITE);                        // White text
    StatusBar::drawStatusBar(d_, W_, H_, statusInfo, &Cairo_Bold12pt7b);
  } while (d_.nextPage());
}

int ScreenUI::getNextPrayerIndex(const char *times[5], int currentHour,
                                 int currentMin) {
  const int now = currentHour * 60 + currentMin;
  for (int i = 0; i < 5; i++) {
    int h = 0, m = 0;
    if (times[i])
      sscanf(times[i], "%d:%d", &h, &m);
    const int t = h * 60 + m;
    if (t > now)
      return i;
  }
  return 0;
}

void ScreenUI::drawTextBox(const char *text, int16_t x, int16_t y, int16_t wBox,
                           int16_t hBox, const GFXfont *font) {
  d_.setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  d_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  d_.drawRect(x, y, wBox, hBox, GxEPD_BLACK); // this is for the box rounded

  int16_t textX = x + (wBox - w) / 2 - x1;
  int16_t textY = y + (hBox - h) / 2 - y1;

  d_.setTextColor(GxEPD_BLACK);
  d_.setCursor(textX, textY);
  d_.print(text);
}

void ScreenUI::drawTextWithoutBox(const char *text, int16_t x, int16_t y,
                                  int16_t wBox, int16_t hBox,
                                  const GFXfont *font) {
  d_.setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  d_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  // No box drawing - just the text

  int16_t textX = x + (wBox - w) / 2 - x1;
  int16_t textY = y + (hBox - h) / 2 - y1;

  d_.setTextColor(GxEPD_BLACK);
  d_.setCursor(textX, textY);
  d_.print(text);
}

void ScreenUI::drawCenteredText(const char *text, int16_t centerX, int16_t topY,
                                const GFXfont *font) {
  d_.setFont(font);
  int16_t x1, y1;
  uint16_t w, h;
  d_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t textX = centerX - w / 2 - x1;
  int16_t textY = topY - y1;

  d_.setTextColor(GxEPD_BLACK);
  d_.setCursor(textX, textY);
  d_.print(text);
}

// Helper function to calculate Iqama delay
int calculateIqamaDelay(const char *prayerTime, const char *iqamaTime) {
  if (!prayerTime || !iqamaTime)
    return 0;

  // Check if iqamaTime is already a simple offset (e.g., "30" or "+30")
  // If it doesn't contain a colon, it's already the delay in minutes
  if (strchr(iqamaTime, ':') == nullptr) {
    // It's already a minutes offset, just convert to int
    return atoi(iqamaTime);
  }

  // Otherwise, calculate the difference between two absolute times
  int prayerH = 0, prayerM = 0;
  int iqamaH = 0, iqamaM = 0;

  sscanf(prayerTime, "%d:%d", &prayerH, &prayerM);
  sscanf(iqamaTime, "%d:%d", &iqamaH, &iqamaM);

  int prayerMinutes = prayerH * 60 + prayerM;
  int iqamaMinutes = iqamaH * 60 + iqamaM;

  return iqamaMinutes - prayerMinutes;
}

void ScreenUI::drawPrayerTimeBoxes(const char *names[], const char *times[],
                                   const char *iqamaTimes[], int count,
                                   int16_t startY, int16_t boxW, int16_t boxH,
                                   int16_t spacing, int highlightIndex) {
  const int16_t totalW = count * boxW + (count - 1) * spacing;
  const int16_t startX = (W_ - totalW) / 2;

  for (int i = 0; i < count; i++) {
    const int16_t x = startX + i * (boxW + spacing);

    // Calculate Iqama delay
    int delay = calculateIqamaDelay(times[i], iqamaTimes[i]);
    char iqamaDelayStr[8];
    snprintf(iqamaDelayStr, sizeof(iqamaDelayStr), "+%d", delay);

    // Divide box into 3 equal sections for perfect spacing
    int16_t section1 = startY + boxH / 3;
    int16_t section2 = startY + (boxH * 2) / 3;
    int16_t section3 = startY + boxH;

    if (i == highlightIndex) {
      // Highlighted: normal box with thick underline indicator
      d_.drawRect(x, startY, boxW, boxH, GxEPD_WHITE);
      
      // Thick horizontal underline below the box as indicator
      int16_t underlineY = startY + boxH + 3;
      d_.fillRect(x, underlineY, boxW, 5, GxEPD_WHITE);

      // Prayer Name (first third)
      d_.setFont(&Cairo_Bold18pt7b);
      int16_t x1, y1;
      uint16_t w, h;
      d_.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = section1 - 5;
      d_.setTextColor(GxEPD_WHITE); // White text on black background
      d_.setCursor(nameX, nameY);
      d_.print(names[i]);

      // Prayer Time (second third)
      d_.setFont(&Cairo_Bold24pt7b);
      d_.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = section2 - 5;
      d_.setTextColor(GxEPD_WHITE); // White text on black background
      d_.setCursor(timeX, timeY);
      d_.print(times[i]);

      // Iqama delay (third section) - only show if delay > 0
      if (delay > 0) {
        d_.setFont(&Cairo_Bold24pt7b);
        d_.getTextBounds(iqamaDelayStr, 0, 0, &x1, &y1, &w, &h);
        int16_t iqamaX = x + (boxW - w) / 2 - x1;
        int16_t iqamaY = section3 - 15;
        d_.setTextColor(GxEPD_WHITE); // White text on black background
        d_.setCursor(iqamaX, iqamaY);
        d_.print(iqamaDelayStr);
      }
    } else {
      // Not highlighted: normal box with border
      d_.drawRect(x, startY, boxW, boxH, GxEPD_WHITE);

      // Prayer Name (first third)
      d_.setFont(&Cairo_Bold18pt7b);
      int16_t x1, y1;
      uint16_t w, h;
      d_.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = section1 - 5;
      d_.setTextColor(GxEPD_WHITE); // White text on black background
      d_.setCursor(nameX, nameY);
      d_.print(names[i]);

      // Prayer Time (second third)
      d_.setFont(&Cairo_Bold24pt7b);
      d_.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = section2 - 5;
      d_.setTextColor(GxEPD_WHITE); // White text on black background
      d_.setCursor(timeX, timeY);
      d_.print(times[i]);

      // Iqama delay (third section) - only show if delay > 0
      if (delay > 0) {
        d_.setFont(&Cairo_Bold24pt7b);
        d_.getTextBounds(iqamaDelayStr, 0, 0, &x1, &y1, &w, &h);
        int16_t iqamaX = x + (boxW - w) / 2 - x1;
        int16_t iqamaY = section3 - 15;
        d_.setTextColor(GxEPD_WHITE); // White text on black background
        d_.setCursor(iqamaX, iqamaY);
        d_.print(iqamaDelayStr);
      }
    }
  }
}

void ScreenUI::redrawCountdownRegion(const ScreenLayout &L,
                                     const char *countdownStr,
                                     const char *currentTime) {
  const int16_t countdownX = (W_ - L.countdownW) / 2;
  
  // Calculate the width needed for both countdown and center time
  // Start from countdown box left edge, extend to cover center time
  // Center time is centered on screen, so we need to check if it extends beyond countdown box
  int16_t windowX = countdownX;
  int16_t windowW = L.countdownW;
  
  // If we have center time, calculate its bounds and expand window if needed
  if (currentTime) {
    d_.setFont(&Cairo_Bold40pt7b);
    int16_t x1_time, y1_time;
    uint16_t w_time, h_time;
    d_.getTextBounds(currentTime, 0, 0, &x1_time, &y1_time, &w_time, &h_time);
    int16_t timeX = (W_ / 2) - (w_time / 2) - x1_time;
    int16_t timeRight = timeX + w_time + 10; // Add small margin
    
    // Expand window to include time if it extends beyond countdown box
    if (timeX < windowX) {
      windowW += (windowX - timeX);
      windowX = timeX;
    }
    if (timeRight > (countdownX + L.countdownW)) {
      windowW = timeRight - windowX;
    }
  }
  
  // Partial window for countdown and center time ONLY
  // Avoid touching the weather area on the right (starts at countdownX + countdownW + 30)
  const int16_t windowY = L.countdownY;
  const int16_t windowH = (H_ / 2) + 80 - L.countdownY; // Include area for center time
  
  d_.setPartialWindow(windowX, windowY, windowW, windowH);
  d_.firstPage();
  do {
    // Clear the exact window area with black background
    d_.fillRect(windowX, windowY, windowW, windowH, GxEPD_BLACK);

    // Draw countdown box with white border
    d_.drawRect(countdownX, L.countdownY, L.countdownW, L.countdownH,
                GxEPD_WHITE);

    // Draw countdown in large font inside the box
    d_.setFont(&Cairo_Bold60pt7b);
    int16_t x1, y1;
    uint16_t w, h;
    d_.getTextBounds(countdownStr, 0, 0, &x1, &y1, &w, &h);
    int16_t textX = countdownX + (L.countdownW - w) / 2 - x1;
    int16_t textY = L.countdownY + (L.countdownH - h) / 2 - y1;
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX, textY);
    d_.print(countdownStr);

    // Draw current RTC time centered vertically on screen
    if (currentTime) {
      d_.setFont(&Cairo_Bold40pt7b);
      d_.getTextBounds(currentTime, 0, 0, &x1, &y1, &w, &h);
      textX = (W_ / 2) - (w / 2) - x1; // Center horizontally on screen
      textY = (H_ / 2) - 20 -
              y1; // Center vertically on screen (slightly above center)
      d_.setTextColor(GxEPD_WHITE);
      d_.setCursor(textX, textY);
      d_.print(currentTime);
    }
  } while (d_.nextPage());
}

void ScreenUI::redrawPrayerRowRegion(const ScreenLayout &L,
                                     const char *names[5], const char *times[5],
                                     const char *iqamaTimes[5],
                                     int highlightIndex) {
  d_.setPartialWindow(L.rowStartX, L.rowY, L.rowW, L.prayerBoxH);
  d_.firstPage();
  do {
    d_.fillRect(L.rowStartX, L.rowY, L.rowW, L.prayerBoxH,
                GxEPD_BLACK); // Black background
    drawPrayerTimeBoxes(
        const_cast<const char **>(names), const_cast<const char **>(times),
        const_cast<const char **>(iqamaTimes), 5, L.rowY, L.prayerBoxW,
        L.prayerBoxH, L.prayerSpacing, highlightIndex);
  } while (d_.nextPage());
}

void ScreenUI::redrawHeaderRegion(const ScreenLayout &L, const char *mosqueName,
                                  const char *headerLabel) {
  // Header box (mosque name)
  d_.setPartialWindow(L.boxX, L.headerY, L.boxW, L.boxH);
  d_.firstPage();
  do {
    d_.fillRect(L.boxX, L.headerY, L.boxW, L.boxH,
                GxEPD_BLACK); // Black background

    // Draw mosque name in white
    d_.setFont(&Cairo_Bold12pt7b);
    const char *mosqueText = mosqueName ? mosqueName : "Mosque Name";
    int16_t x1, y1;
    uint16_t w, h;
    d_.getTextBounds(mosqueText, 0, 0, &x1, &y1, &w, &h);
    int16_t textX = L.boxX + (L.boxW - w) / 2 - x1;
    int16_t textY = L.headerY + (L.boxH - h) / 2 - y1;
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX, textY);
    d_.print(mosqueText);
  } while (d_.nextPage());

  // Centered label above the countdown (e.g., "Asr in") - using smaller font
  // (18pt instead of 24pt)
  const char *label =
      (headerLabel && headerLabel[0]) ? headerLabel : "Prayer in";
  d_.setFont(&Cairo_Bold24pt7b);
  int16_t x1, y1;
  uint16_t w, h;
  d_.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  const int16_t centerX = W_ / 2;
  const int16_t topY =
      L.countdownY - 40; // 40px space above countdown box - 10px closer
  const int16_t textX = centerX - w / 2 - x1;
  const int16_t textY = topY - y1;

  // Use a reasonable partial window size
  d_.setPartialWindow(textX - 10, topY - 10, w + 20, h + 20);
  d_.firstPage();
  do {
    // Fill with black background first
    d_.fillRect(textX - 10, topY - 10, w + 20, h + 20, GxEPD_BLACK);
    // Then draw white text on top
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX, textY);
    d_.print(label);
  } while (d_.nextPage());
}

void ScreenUI::showInitializationScreen() {
  d_.setFullWindow();
  d_.firstPage();
  do {
    d_.fillScreen(GxEPD_BLACK);
    d_.setTextColor(GxEPD_WHITE);

    // Center the text on screen
    int16_t centerX = W_ / 2; // Center X for 800px wide screen
    int16_t centerY = H_ / 2; // Center Y for 480px high screen

    // First line: "MAWAQIT Frame" - Large font
    d_.setFont(&Cairo_Bold40pt7b);
    d_.setCursor(centerX - 280, centerY);
    d_.print("eTaqweem");

    // Third line: Setup instructions - Smaller font
    d_.setFont(&Cairo_Bold12pt7b);
    d_.setCursor(centerX - 330, centerY + 80);
    d_.print("Enable Bluetooth on your phone and open eTaqweem app to start");

  } while (d_.nextPage());
}

void ScreenUI::showInitializationScreenWithError(const char *errorMsg) {
  d_.setFullWindow();
  d_.firstPage();
  do {
    d_.fillScreen(GxEPD_BLACK);
    d_.setTextColor(GxEPD_WHITE);

    // Center the text on screen
    int16_t centerX = W_ / 2; // Center X for 800px wide screen
    int16_t centerY = H_ / 2; // Center Y for 480px high screen

    // First line: "MAWAQIT Frame" - Large font
    d_.setFont(&Cairo_Bold40pt7b);
    d_.setCursor(centerX - 280, centerY);
    d_.print("eTaqweem ");

    // Third line: Setup instructions - Smaller font
    d_.setFont(&Cairo_Bold12pt7b);
    d_.setCursor(centerX - 330, centerY + 80);
    d_.print("Enable Bluetooth on your phone and open eTaqweem app to start");

    // Error message at footer
    if (errorMsg) {
      d_.setFont(&Cairo_Bold12pt7b);
      int16_t x1, y1;
      uint16_t w, h;
      d_.getTextBounds(errorMsg, 0, 0, &x1, &y1, &w, &h);
      int16_t errorX = centerX - (w / 2) - x1;
      int16_t errorY = H_ - 30; // 30px from bottom
      d_.setCursor(errorX, errorY);
      d_.print(errorMsg);
    }

  } while (d_.nextPage());
}

void ScreenUI::drawWeatherIcon(const char *weatherDesc, int16_t x, int16_t y, int16_t size) {
  // Determine if it's night time based on prayer times (Maghrib to Fajr)
  bool isNight = false;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    // Get current time in minutes since midnight
    int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    
    // Parse Maghrib time (HH:MM)
    int maghribHour = 0, maghribMin = 0;
    if (sscanf(rtcData.TODAY_MAGHRIB, "%d:%d", &maghribHour, &maghribMin) == 2) {
      int maghribMinutes = maghribHour * 60 + maghribMin;
      
      // Parse Fajr time (HH:MM) - could be today or next day
      int fajrHour = 0, fajrMin = 0;
      const char* fajrTime = rtcData.TODAY_FAJR;
      
      // If current time is past midnight, we might need next day's Fajr
      // But since Fajr is always early morning, TODAY_FAJR should be correct
      if (sscanf(fajrTime, "%d:%d", &fajrHour, &fajrMin) == 2) {
        int fajrMinutes = fajrHour * 60 + fajrMin;
        
        // Night time is: after Maghrib OR before Fajr
        // Since Maghrib is in evening and Fajr is in early morning,
        // we need to handle the day boundary
        if (maghribMinutes > fajrMinutes) {
          // Normal case: Maghrib (e.g., 18:00) > Fajr (e.g., 05:00)
          // Night is: >= Maghrib OR < Fajr
          isNight = (currentMinutes >= maghribMinutes || currentMinutes < fajrMinutes);
        } else {
          // Edge case: shouldn't happen, but handle it
          isNight = (currentMinutes >= maghribMinutes && currentMinutes < fajrMinutes);
        }
      }
    }
  }
  
  // Use appropriate font (day or night)
  if (isNight) {
    d_.setFont(&WeatherIconsNight50pt);
  } else {
    d_.setFont(&WeatherIcons50pt);
  }
  d_.setTextColor(GxEPD_WHITE);
  
  // Map weather description to ASCII character
  // Day font: A=sun, B=cloud, C=rain, D=drizzle, E=snow, F=storm, G=fog, H=unknown
  // Night font: I=moon, J=cloudy-night, K=rain-night, L=drizzle-night, M=snow-night, N=storm-night, O=fog-night, P=unknown
  char iconChar = isNight ? 'P' : 'H'; // Default: unknown
  
  if (strcmp(weatherDesc, "Clear") == 0) {
    iconChar = isNight ? 'I' : 'A'; // moon : sun
  } else if (strcmp(weatherDesc, "Cloudy") == 0 || strcmp(weatherDesc, "Overcast") == 0) {
    iconChar = isNight ? 'J' : 'B'; // cloudy-night : cloud
  } else if (strcmp(weatherDesc, "Rain") == 0) {
    iconChar = isNight ? 'K' : 'C'; // rain-night : rain
  } else if (strcmp(weatherDesc, "Drizzle") == 0) {
    iconChar = isNight ? 'L' : 'D'; // drizzle-night : drizzle
  } else if (strcmp(weatherDesc, "Snow") == 0) {
    iconChar = isNight ? 'M' : 'E'; // snow-night : snow
  } else if (strcmp(weatherDesc, "Storm") == 0 || strcmp(weatherDesc, "Thunderstorm") == 0) {
    iconChar = isNight ? 'N' : 'F'; // storm-night : storm
  } else if (strcmp(weatherDesc, "Fog") == 0 || strcmp(weatherDesc, "Mist") == 0) {
    iconChar = isNight ? 'O' : 'G'; // fog-night : fog
  }
  
  // Draw the weather icon
  char iconStr[2] = {iconChar, '\0'};
  d_.setCursor(x, y + size);
  d_.print(iconStr);
}