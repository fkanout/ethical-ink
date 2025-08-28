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
  L.boxW = 200; L.boxH = 60; L.spacing = 20;
  L.boxX = (W_ - L.boxW) / 2;
  L.headerY = 10;
  L.countdownY = L.headerY + L.boxH + L.spacing + 30;
  L.countdownW = 200; L.countdownH = 100;

  L.rowY = 300; L.prayerBoxW = 140; L.prayerBoxH = 90; L.prayerSpacing = 10;
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

    int16_t currentY = L.headerY;
    drawTextBox(mosqueName ? mosqueName : "Mosque Name",
                L.boxX, currentY, L.boxW, L.boxH, &FreeMonoBold9pt7b);

    currentY += L.boxH + L.spacing;
    drawCenteredText("Prayer in", W_/2, currentY, &FreeMonoBold9pt7b);

    currentY += 30;
    drawTextBox(countdownStr, L.boxX, L.countdownY, L.countdownW, L.countdownH, &FreeMonoBold24pt7b);

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

  d_.drawRect(x, y, wBox, hBox, GxEPD_BLACK);

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

      d_.setFont(&FreeMonoBold9pt7b);
      int16_t x1, y1; uint16_t w, h;
      d_.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = startY + 20;
      d_.setTextColor(GxEPD_WHITE);
      d_.setCursor(nameX, nameY);
      d_.print(names[i]);

      d_.setFont(&FreeMonoBold18pt7b);
      d_.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = startY + boxH - 12;
      d_.setCursor(timeX, timeY);
      d_.print(times[i]);
    } else {
      d_.drawRect(x, startY, boxW, boxH, GxEPD_BLACK);

      d_.setFont(&FreeMonoBold9pt7b);
      int16_t x1, y1; uint16_t w, h;
      d_.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = startY + 20;
      d_.setTextColor(GxEPD_BLACK);
      d_.setCursor(nameX, nameY);
      d_.print(names[i]);

      d_.setFont(&FreeMonoBold18pt7b);
      d_.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = startY + boxH - 12;
      d_.setCursor(timeX, timeY);
      d_.print(times[i]);
    }
  }
}

void ScreenUI::redrawCountdownRegion(const ScreenLayout& L, const char* countdownStr) {
  d_.setPartialWindow(L.boxX, L.countdownY, L.countdownW, L.countdownH);
  d_.firstPage();
  do {
    d_.fillRect(L.boxX, L.countdownY, L.countdownW, L.countdownH, GxEPD_WHITE);
    drawTextBox(countdownStr, L.boxX, L.countdownY, L.countdownW, L.countdownH, &FreeMonoBold24pt7b);
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
  d_.setPartialWindow(L.boxX, L.headerY, L.boxW, L.boxH);
  d_.firstPage();
  do {
    d_.fillRect(L.boxX, L.headerY, L.boxW, L.boxH, GxEPD_WHITE);
    drawTextBox(mosqueName ? mosqueName : "Mosque Name",
                L.boxX, L.headerY, L.boxW, L.boxH, &FreeMonoBold9pt7b);
  } while (d_.nextPage());

  const char* label = "Prayer in";
  d_.setFont(&FreeMonoBold9pt7b);
  int16_t x1, y1; uint16_t w, h;
  d_.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  int16_t topY = L.headerY + L.boxH + L.spacing;
  int16_t centerX = W_/2;
  int16_t textX = centerX - w/2 - x1;
  int16_t textY = topY - y1;

  d_.setPartialWindow(textX - 2, topY - 2, w + 4, h + 4);
  d_.firstPage();
  do {
    d_.fillRect(textX - 2, topY - 2, w + 4, h + 4, GxEPD_WHITE);
    drawCenteredText(label, centerX, topY, &FreeMonoBold9pt7b);
  } while (d_.nextPage());
}
