#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>
#include <SPI.h>

// Pins for ESP32-S3
#define EPD_CS   10
#define EPD_DC   9
#define EPD_RST  8
#define EPD_BUSY 7

// 7.5" b/w (800x480)
GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(
  GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)
);

// ---------- helpers ----------
void drawTextBox(const char* text, int16_t x, int16_t y, int16_t wBox, int16_t hBox, const GFXfont* font) {
  display.setFont(font);

  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  display.drawRect(x, y, wBox, hBox, GxEPD_BLACK);

  int16_t textX = x + (wBox - w) / 2 - x1;
  int16_t textY = y + (hBox - h) / 2 - y1;

  display.setTextColor(GxEPD_BLACK);
  display.setCursor(textX, textY);
  display.print(text);
}

void drawCenteredText(const char* text, int16_t centerX, int16_t topY, const GFXfont* font) {
  display.setFont(font);

  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t textX = centerX - w/2 - x1;
  int16_t textY = topY - y1;

  display.setTextColor(GxEPD_BLACK);
  display.setCursor(textX, textY);
  display.print(text);
}

// ---------- draw prayer-time boxes with highlight ----------
void drawPrayerTimeBoxes(const char* names[], const char* times[], int count,
                         int16_t startY, int16_t boxW, int16_t boxH, int16_t spacing,
                         int highlightIndex = -1) {
  // center the whole row
  int16_t totalW = count * boxW + (count - 1) * spacing;
  int16_t startX = (800 - totalW) / 2;

  for (int i = 0; i < count; i++) {
    int16_t x = startX + i * (boxW + spacing);

    if (i == highlightIndex) {
      // Highlighted box (black background, white text)
      display.fillRect(x, startY, boxW, boxH, GxEPD_BLACK);
      display.drawRect(x, startY, boxW, boxH, GxEPD_BLACK);

      display.setFont(&FreeMonoBold9pt7b);
      int16_t x1, y1; uint16_t w, h;
      display.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = startY + 20;
      display.setTextColor(GxEPD_WHITE);
      display.setCursor(nameX, nameY);
      display.print(names[i]);

      display.setFont(&FreeMonoBold18pt7b);
      display.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = startY + boxH - 12;
      display.setCursor(timeX, timeY);
      display.print(times[i]);
    } else {
      // Normal box
      display.drawRect(x, startY, boxW, boxH, GxEPD_BLACK);

      display.setFont(&FreeMonoBold9pt7b);
      int16_t x1, y1; uint16_t w, h;
      display.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
      int16_t nameX = x + (boxW - w) / 2 - x1;
      int16_t nameY = startY + 20;
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(nameX, nameY);
      display.print(names[i]);

      display.setFont(&FreeMonoBold18pt7b);
      display.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
      int16_t timeX = x + (boxW - w) / 2 - x1;
      int16_t timeY = startY + boxH - 12;
      display.setCursor(timeX, timeY);
      display.print(times[i]);
    }
  }
}

// ---------- helper: find next prayer ----------
int getNextPrayerIndex(const char* times[], int count, int currentHour, int currentMin) {
  int now = currentHour * 60 + currentMin; // total minutes

  for (int i = 0; i < count; i++) {
    int h, m;
    sscanf(times[i], "%d:%d", &h, &m);
    int t = h * 60 + m;
    if (t > now) {
      return i; // first prayer after current time
    }
  }
  return 0; // if none left, wrap to Fajr
}

// ---------- main ----------
void setup() {
  Serial.begin(115200);
  delay(100);

  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();

  const char* names[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
  const char* times[5] = {"04:15", "12:40", "16:30", "19:26", "20:30"};

  // Example: assume current time is 11:50
  int currentHour = 13;
  int currentMin  = 50;
  int highlight = getNextPrayerIndex(times, 5, currentHour, currentMin);

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    int16_t currentY = 10;
    int16_t boxW = 200, boxH = 60, spacing = 20;
    int16_t boxX = (800 - boxW) / 2;

    // Mosque name box
    drawTextBox("Mosque Name", boxX, currentY, boxW, boxH, &FreeMonoBold9pt7b);

    // "Prayer in"
    currentY += boxH + spacing;
    drawCenteredText("Prayer in", 400, currentY, &FreeMonoBold9pt7b);

    // HH:MM big box
    currentY += 30;
    drawTextBox("HH:MM", boxX, currentY, 200, 100, &FreeMonoBold24pt7b);

    // bottom row prayer times
    int16_t rowY = 300;
    int16_t prayerBoxW = 140;
    int16_t prayerBoxH = 90;
    int16_t prayerSpacing = 10;
    drawPrayerTimeBoxes(names, times, 5, rowY, prayerBoxW, prayerBoxH, prayerSpacing, highlight);

  } while (display.nextPage());
}

void loop() {
  // ممكن تحدث الشاشة كل فترة حسب الحاجة
}
