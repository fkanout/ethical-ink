// ScreenUI.cpp
#include "ScreenUI.h"
#include <GxEPD2_BW.h> // for color constants (GxEPD_WHITE/BLACK). If you use others, define 0/1.

#ifndef GxEPD_WHITE
#define GxEPD_WHITE 0xFFFF
#endif
#ifndef GxEPD_BLACK
#define GxEPD_BLACK 0x0000
#endif

ScreenUI::ScreenUI(IEpaper& epd, int16_t screenW, int16_t screenH)
: d_(epd), W_(screenW), H_(screenH) {}

ScreenLayout ScreenUI::computeLayout() const {
  ScreenLayout L;

  // Optimized for 800x480 display
  L.boxW = W_ * 0.2;        // 40% of screen width (320px)
  L.boxH = 70;              // Taller header box
  L.spacing = 25;
  L.boxX = (W_ - L.boxW) / 2;
  L.headerY = 20;           // More top margin

  // Countdown section - centered on screen
  L.countdownW = W_ * 0.3;      // 50% of screen width (400px)
  L.countdownH = 100;           // Taller for better font di splay
  // Center vertically. (X will be computed where used to avoid changing the header struct.)
  L.countdownY = (H_ - L.countdownH) / 2;

  // Prayer times row
  L.rowY = L.countdownY + L.countdownH + 40;      // Spacing below the centered countdown
  L.prayerBoxW = (W_ - 60) / 5;                   // Divide screen evenly with margins
  L.prayerBoxH = H_ - L.rowY - 30;                // Use remaining height
  L.prayerSpacing = 10;
  const int count = 5;
  L.rowW = count * L.prayerBoxW + (count - 1) * L.prayerSpacing;
  L.rowStartX = (W_ - L.rowW) / 2;

  return L;
}

void ScreenUI::fullRender(const ScreenLayout& L,
                          const char* mosqueName,
                          const char* countdownStr,
                          const char* prayerNames[5],
                          const char* prayerTimes[5],
                          int highlightIndex) {
  d_.setFullWindow();
  d_.firstPage();
  do {
    d_.fillScreen(GxEPD_WHITE);

    // Header (mosque name)
    drawTextBox(mosqueName ? mosqueName : "Mosque Name",
                L.boxX, L.headerY, L.boxW, L.boxH, &Cairo_Bold9pt7b);

    // "Prayer in" label just above the centered countdown
    const int16_t countdownX = (W_ - L.countdownW) / 2;
    const int16_t labelTopY   = L.countdownY - 70; // adjust spacing above countdown
    drawCenteredText("Prayer in", W_/2, labelTopY, &Cairo_Bold24pt7b);

    // Centered countdown box
    drawTextBox(countdownStr, countdownX, L.countdownY, L.countdownW, L.countdownH, &Cairo_Bold40pt7b);

    // Prayer time boxes row
    drawPrayerTimeBoxes(const_cast<const char**>(prayerNames),
                        const_cast<const char**>(prayerTimes),
                        5, L.rowY, L.prayerBoxW, L.prayerBoxH, L.prayerSpacing, highlightIndex);
  } while (d_.nextPage());
}

void ScreenUI::partialRender(const ScreenLayout& L,
                             const char* mosqueName,
                             const char* countdownStr,
                             const char* prayerNames[5],
                             const char* prayerTimes[5],
                             int highlightIndex) {
  redrawHeaderRegion(L, mosqueName);
  redrawCountdownRegion(L, countdownStr);
  redrawPrayerRowRegion(L, prayerNames, prayerTimes, highlightIndex);
}

int ScreenUI::getNextPrayerIndex(const char* times[5], int currentHour, int currentMin) {
  const int now = currentHour * 60 + currentMin;
  for (int i = 0; i < 5; i++) {
    int h = 0, m = 0;
    if (times[i]) sscanf(times[i], "%d:%d", &h, &m);
    const int t = h * 60 + m;
    if (t > now) return i;
  }
  return 0;
}

void ScreenUI::drawTextBox(const char* text, int16_t x, int16_t y, int16_t wBox, int16_t hBox, const GFXfont* font) {
  d_.setFont(font);
  int16_t x1, y1; uint16_t w, h;
  d_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  d_.drawRect(x, y, wBox, hBox, GxEPD_BLACK);   // this is for the box rounded 

  int16_t textX = x + (wBox - w) / 2 - x1;
  int16_t textY = y + (hBox - h) / 2 - y1;

  d_.setTextColor(GxEPD_BLACK);
  d_.setCursor(textX, textY);
  d_.print(text);
}

void ScreenUI::drawCenteredText(const char* text, int16_t centerX, int16_t topY, const GFXfont* font) {
  d_.setFont(font);
  int16_t x1, y1; uint16_t w, h;
  d_.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t textX = centerX - w/2 - x1;
  int16_t textY = topY - y1;

  d_.setTextColor(GxEPD_BLACK);
  d_.setCursor(textX, textY);
  d_.print(text);
}

void ScreenUI::drawPrayerTimeBoxes(const char* names[], const char* times[], int count,
                                   int16_t startY, int16_t boxW, int16_t boxH, int16_t spacing,
                                   int highlightIndex) {
  const int16_t totalW = count * boxW + (count - 1) * spacing;
  const int16_t startX = (W_ - totalW) / 2;

  for (int i = 0; i < count; i++) {
    const int16_t x = startX + i * (boxW + spacing);

    if (i == highlightIndex) {
      d_.fillRect(x, startY, boxW, boxH, GxEPD_BLACK);
      d_.drawRect(x, startY, boxW, boxH, GxEPD_BLACK);

      // Name (medium)
      d_.setFont(&Cairo_Bold9pt7b);
      int16_t x1, y1; uint16_t w, h;
      d_.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = startY + 25;
      d_.setTextColor(GxEPD_WHITE);
      d_.setCursor(nameX, nameY);
      d_.print(names[i]);

      // Time (large)
      d_.setFont(&Cairo_Bold24pt7b);
      d_.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = startY + boxH - 20;
      d_.setCursor(timeX, timeY);
      d_.print(times[i]);
    } else {
      d_.drawRect(x, startY, boxW, boxH, GxEPD_BLACK);

      d_.setFont(&Cairo_Bold9pt7b);
      int16_t x1, y1; uint16_t w, h;
      d_.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = startY + 25;
      d_.setTextColor(GxEPD_BLACK);
      d_.setCursor(nameX, nameY);
      d_.print(names[i]);

      d_.setFont(&Cairo_Bold24pt7b);
      d_.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = startY + boxH - 20;
      d_.setCursor(timeX, timeY);
      d_.print(times[i]);
    }
  }
}

void ScreenUI::redrawCountdownRegion(const ScreenLayout& L, const char* countdownStr) {
  const int16_t countdownX = (W_ - L.countdownW) / 2;
  d_.setPartialWindow(countdownX, L.countdownY, L.countdownW, L.countdownH);
  d_.firstPage();
  do {
    d_.fillRect(countdownX, L.countdownY, L.countdownW, L.countdownH, GxEPD_WHITE);
    drawTextBox(countdownStr, countdownX, L.countdownY, L.countdownW, L.countdownH, &Cairo_Bold40pt7b);
  } while (d_.nextPage());
}

void ScreenUI::redrawPrayerRowRegion(const ScreenLayout& L,
                                     const char* names[5], const char* times[5],
                                     int highlightIndex) {
  d_.setPartialWindow(L.rowStartX, L.rowY, L.rowW, L.prayerBoxH);
  d_.firstPage();
  do {
    d_.fillRect(L.rowStartX, L.rowY, L.rowW, L.prayerBoxH, GxEPD_WHITE);
    drawPrayerTimeBoxes(const_cast<const char**>(names),
                        const_cast<const char**>(times),
                        5, L.rowY, L.prayerBoxW, L.prayerBoxH, L.prayerSpacing, highlightIndex);
  } while (d_.nextPage());
}

void ScreenUI::redrawHeaderRegion(const ScreenLayout& L, const char* mosqueName) {
  // Header box
  d_.setPartialWindow(L.boxX, L.headerY, L.boxW, L.boxH);
  d_.firstPage();
  do {
    d_.fillRect(L.boxX, L.headerY, L.boxW, L.boxH, GxEPD_WHITE);
    drawTextBox(mosqueName ? mosqueName : "Mosque Name",
                L.boxX, L.headerY, L.boxW, L.boxH, &Cairo_Bold9pt7b);
  } while (d_.nextPage());

  // "Prayer in" label above the centered countdown
  const char* label = "Prayer in";
  d_.setFont(&Cairo_Bold9pt7b);
  int16_t x1, y1; uint16_t w, h;
  d_.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  const int16_t centerX = W_/2;
  const int16_t topY = L.countdownY - 70; // keep consistent with fullRender
  const int16_t textX = centerX - w/2 - x1;
  const int16_t textY = topY - y1;

  d_.setPartialWindow(textX - 2, topY - 2, w + 4, h + 4);
  d_.firstPage();
  do {
    d_.fillRect(textX - 2, topY - 2, w + 4, h + 4, GxEPD_WHITE);
    drawCenteredText(label, centerX, topY, &Cairo_Bold24pt7b);
  } while (d_.nextPage());
}
