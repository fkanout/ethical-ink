// ScreenUI.h
#pragma once
#include <Arduino.h>
#include <gfxfont.h>
#include "IEpaper.h"
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>

struct ScreenLayout {
  int16_t boxX, boxW, boxH, spacing;
  int16_t headerY;
  int16_t countdownY;
  int16_t countdownW, countdownH;
  int16_t rowY, prayerBoxW, prayerBoxH, prayerSpacing, rowStartX, rowW;
};

struct RenderStatePersist {
  bool initialized = false;
  int lastHighlight = -1;
  char times[5][6] = {{0}};
};

class ScreenUI {
public:
  // pass screen size (800x480) so we don’t depend on a specific panel
  ScreenUI(IEpaper& epd, int16_t screenW, int16_t screenH);

  ScreenLayout computeLayout() const;

  void fullRender(const ScreenLayout& L,
                  const char* mosqueName,
                  const char* countdownStr,
                  const char* prayerNames[5],
                  const char* prayerTimes[5],
                  int highlightIndex);

  void partialRender(const ScreenLayout& L,
                     const char* mosqueName,
                     const char* countdownStr,
                     const char* prayerNames[5],
                     const char* prayerTimes[5],
                     int highlightIndex);

  static int getNextPrayerIndex(const char* times[5], int currentHour, int currentMin);

private:
  IEpaper& d_;
  int16_t W_, H_;

  void drawTextBox(const char* text, int16_t x, int16_t y, int16_t wBox, int16_t hBox, const GFXfont* font);
  void drawCenteredText(const char* text, int16_t centerX, int16_t topY, const GFXfont* font);
  void drawPrayerTimeBoxes(const char* names[], const char* times[], int count,
                           int16_t startY, int16_t boxW, int16_t boxH, int16_t spacing,
                           int highlightIndex);

  void redrawCountdownRegion(const ScreenLayout& L, const char* countdownStr);
  void redrawPrayerRowRegion(const ScreenLayout& L, const char* names[5], const char* times[5], int highlightIndex);
  void redrawHeaderRegion(const ScreenLayout& L, const char* mosqueName);
};
