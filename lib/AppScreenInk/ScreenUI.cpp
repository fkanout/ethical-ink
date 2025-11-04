// ScreenUI.cpp
#include "ScreenUI.h"
#include <GxEPD2_BW.h> // for color constants (GxEPD_WHITE/BLACK). If you use others, define 0/1.
#include <WiFi.h>

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
  L.headerY = L.contentStartY + 10; // Start after status bar

  // Countdown section - centered on screen
  // size box of countdown
  L.countdownW = W_ * 0.5; // 50% of screen width (400px)
  L.countdownH = 130;      // Taller for better font display
  // Center vertically. (X will be computed where used to avoid changing the
  // header struct.)
  L.countdownY = L.headerY + L.boxH + 100; // space under the mosque name

  // Prayer times row - increased height to accommodate iqama times
  L.rowY =
      L.countdownY + L.countdownH + 30; // Spacing below the centered countdown
  L.prayerBoxW = (W_ - 60) / 5;         // Divide screen evenly with margins
  L.prayerBoxH =
      H_ - L.rowY - 10; // Use remaining height (should be taller now)
  L.prayerSpacing = 10;
  const int count = 5;
  L.rowW = count * L.prayerBoxW + (count - 1) * L.prayerSpacing;
  L.rowStartX = (W_ - L.rowW) / 2;

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
    d_.fillScreen(GxEPD_BLACK); // Black background

    // Draw status bar first with white text
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

    // "Prayer in" label - white text
    const int16_t countdownX = (W_ - L.countdownW) / 2;
    const int16_t labelTopY = L.countdownY - 50;

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
    d_.setFont(&Cairo_Bold70pt7b);
    int16_t x1_count, y1_count;
    uint16_t w_count, h_count;
    d_.getTextBounds(countdownStr, 0, 0, &x1_count, &y1_count, &w_count,
                     &h_count);
    d_.drawRect(countdownX, L.countdownY, L.countdownW, L.countdownH,
                GxEPD_WHITE);
    int16_t textX_count = countdownX + (L.countdownW - w_count) / 2 - x1_count;
    int16_t textY_count =
        L.countdownY + (L.countdownH - h_count) / 2 - y1_count;
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX_count, textY_count);
    d_.print(countdownStr);

    // Prayer time boxes row with iqama times - custom drawing for reversed
    // colors
    const int16_t totalW = 5 * L.prayerBoxW + 4 * L.prayerSpacing;
    const int16_t startX = (W_ - totalW) / 2;

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

      if (i == highlightIndex) {
        // Highlighted: white background, black text
        d_.fillRect(x, L.rowY, L.prayerBoxW, L.prayerBoxH, GxEPD_WHITE);
        d_.drawRect(x, L.rowY, L.prayerBoxW, L.prayerBoxH, GxEPD_WHITE);

        // Prayer Name
        d_.setFont(&Cairo_Bold18pt7b);
        int16_t x1, y1;
        uint16_t w, h;
        d_.getTextBounds(prayerNames[i], 0, 0, &x1, &y1, &w, &h);
        int16_t nameX = x + (L.prayerBoxW - w) / 2 - x1;
        int16_t nameY = section1 - 5;
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(nameX, nameY);
        d_.print(prayerNames[i]);

        // Prayer Time
        d_.setFont(&Cairo_Bold24pt7b);
        d_.getTextBounds(prayerTimes[i], 0, 0, &x1, &y1, &w, &h);
        int16_t timeX = x + (L.prayerBoxW - w) / 2 - x1;
        int16_t timeY = section2 - 10;
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(timeX, timeY);
        d_.print(prayerTimes[i]);

        // Iqama Delay
        d_.setFont(&Cairo_Bold18pt7b);
        d_.getTextBounds(iqamaDelayStr, 0, 0, &x1, &y1, &w, &h);
        int16_t iqamaX = x + (L.prayerBoxW - w) / 2 - x1;
        int16_t iqamaY = L.rowY + L.prayerBoxH - 10;
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(iqamaX, iqamaY);
        d_.print(iqamaDelayStr);
      } else {
        // Not highlighted: white border, white text
        d_.drawRect(x, L.rowY, L.prayerBoxW, L.prayerBoxH, GxEPD_WHITE);

        // Prayer Name
        d_.setFont(&Cairo_Bold18pt7b);
        int16_t x1, y1;
        uint16_t w, h;
        d_.getTextBounds(prayerNames[i], 0, 0, &x1, &y1, &w, &h);
        int16_t nameX = x + (L.prayerBoxW - w) / 2 - x1;
        int16_t nameY = section1 - 5;
        d_.setTextColor(GxEPD_WHITE);
        d_.setCursor(nameX, nameY);
        d_.print(prayerNames[i]);

        // Prayer Time
        d_.setFont(&Cairo_Bold24pt7b);
        d_.getTextBounds(prayerTimes[i], 0, 0, &x1, &y1, &w, &h);
        int16_t timeX = x + (L.prayerBoxW - w) / 2 - x1;
        int16_t timeY = section2 - 10;
        d_.setTextColor(GxEPD_WHITE);
        d_.setCursor(timeX, timeY);
        d_.print(prayerTimes[i]);

        // Iqama Delay
        d_.setFont(&Cairo_Bold18pt7b);
        d_.getTextBounds(iqamaDelayStr, 0, 0, &x1, &y1, &w, &h);
        int16_t iqamaX = x + (L.prayerBoxW - w) / 2 - x1;
        int16_t iqamaY = L.rowY + L.prayerBoxH - 10;
        d_.setTextColor(GxEPD_WHITE);
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

  // Only update countdown - skip header and prayer boxes since they haven't
  // changed
  redrawCountdownRegion(L, countdownStr);
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
      d_.fillRect(x, startY, boxW, boxH,
                  GxEPD_WHITE); // White background for highlight
      d_.drawRect(x, startY, boxW, boxH, GxEPD_WHITE);

      // Prayer Name (first third)
      d_.setFont(&Cairo_Bold18pt7b);
      int16_t x1, y1;
      uint16_t w, h;
      d_.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = section1 - 5;
      d_.setTextColor(GxEPD_BLACK); // Black text on white background
      d_.setCursor(nameX, nameY);
      d_.print(names[i]);

      // Prayer Time (second third)
      d_.setFont(&Cairo_Bold24pt7b);
      d_.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = section2 - 5;
      d_.setTextColor(GxEPD_BLACK); // Black text on white background
      d_.setCursor(timeX, timeY);
      d_.print(times[i]);

      // Iqama delay (third section)
      d_.setFont(&Cairo_Bold18pt7b);
      d_.getTextBounds(iqamaDelayStr, 0, 0, &x1, &y1, &w, &h);
      int16_t iqamaX = x + (boxW - w) / 2 - x1;
      int16_t iqamaY = section3 - 15;
      d_.setTextColor(GxEPD_BLACK); // Black text on white background
      d_.setCursor(iqamaX, iqamaY);
      d_.print(iqamaDelayStr);
    } else {
      d_.drawRect(x, startY, boxW, boxH, GxEPD_WHITE); // White border

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

      // Iqama delay (third section)
      d_.setFont(&Cairo_Bold18pt7b);
      d_.getTextBounds(iqamaDelayStr, 0, 0, &x1, &y1, &w, &h);
      int16_t iqamaX = x + (boxW - w) / 2 - x1;
      int16_t iqamaY = section3 - 15;
      d_.setTextColor(GxEPD_WHITE); // White text on black background
      d_.setCursor(iqamaX, iqamaY);
      d_.print(iqamaDelayStr);
    }
  }
}

void ScreenUI::redrawCountdownRegion(const ScreenLayout &L,
                                     const char *countdownStr) {
  const int16_t countdownX = (W_ - L.countdownW) / 2;
  d_.setPartialWindow(countdownX, L.countdownY, L.countdownW, L.countdownH);
  d_.firstPage();
  do {
    d_.fillRect(countdownX, L.countdownY, L.countdownW, L.countdownH,
                GxEPD_BLACK); // Black background

    // Draw white border
    d_.drawRect(countdownX, L.countdownY, L.countdownW, L.countdownH,
                GxEPD_WHITE);

    // Draw white text
    d_.setFont(&Cairo_Bold70pt7b);
    int16_t x1, y1;
    uint16_t w, h;
    d_.getTextBounds(countdownStr, 0, 0, &x1, &y1, &w, &h);
    int16_t textX = countdownX + (L.countdownW - w) / 2 - x1;
    int16_t textY = L.countdownY + (L.countdownH - h) / 2 - y1;
    d_.setTextColor(GxEPD_WHITE);
    d_.setCursor(textX, textY);
    d_.print(countdownStr);
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
  const int16_t topY = L.countdownY - 50;
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
    d_.print("MAWAQIT Frame");

    // Third line: Setup instructions - Smaller font
    d_.setFont(&Cairo_Bold12pt7b);
    d_.setCursor(centerX - 330, centerY + 80);
    d_.print("Enable Bluetooth on your phone and open MAWAQIT app to start");

  } while (d_.nextPage());
}
