#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>   // bigger font for times
#include <Fonts/FreeMonoBold24pt7b.h>   // bigger font for HH:MM box
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

// ---------- generic helpers ----------
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

// ---------- new: draw 5 prayer-time boxes ----------
void drawPrayerTimeBoxes(const char* names[], const char* times[], int count,
                         int16_t startY, int16_t boxW, int16_t boxH, int16_t spacing) {
  // center the whole row
  int16_t totalW = count * boxW + (count - 1) * spacing;
  int16_t startX = (800 - totalW) / 2;

  for (int i = 0; i < count; i++) {
    int16_t x = startX + i * (boxW + spacing);

    // box outline
    display.drawRect(x, startY, boxW, boxH, GxEPD_BLACK);

    // prayer name (top, small font)
    display.setFont(&FreeMonoBold9pt7b);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(names[i], 0, 0, &x1, &y1, &w, &h);
    int16_t nameX = x + (boxW - w) / 2 - x1;
    int16_t nameY = startY + 20; // top area
    display.setCursor(nameX, nameY);
    display.print(names[i]);

    // time (bigger font) centered lower
    display.setFont(&FreeMonoBold18pt7b);
    display.getTextBounds(times[i], 0, 0, &x1, &y1, &w, &h);
    int16_t timeX = x + (boxW - w) / 2 - x1;
    int16_t timeY = startY + boxH - 12; // a bit above bottom
    display.setCursor(timeX, timeY);
    display.print(times[i]);
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  display.init(115200);
  display.setRotation(0); // keep as you had it
  display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    // --- top section ---
    int16_t currentY = 10;
    int16_t boxW = 200, boxH = 60, spacing = 20;
    int16_t boxX = (800 - boxW) / 2;

    // 1) Mosque name box
    drawTextBox("Mosque Name", boxX, currentY, boxW, boxH, &FreeMonoBold9pt7b);

    // 2) "Prayer in" text (restored)
    currentY += boxH + spacing;
    drawCenteredText("Prayer in", 400, currentY, &FreeMonoBold9pt7b);

    // 3) HH:MM big box
    currentY += 20 + 10; // some extra space after the line
    drawTextBox("HH:MM", boxX, currentY, 200, 100, &FreeMonoBold24pt7b);

    // --- bottom section: 5 boxes for prayer times ---
    const char* names[5] = {"Fajr", "Dhuhr", "Asr", "Maghrib", "Isha"};
    const char* times[5] = {"04:15", "12:40", "16:30", "19:26", "18:30"};

    // optional Arabic header (note: default fonts may not render Arabic)
    // drawCenteredText("اوقات الصلاة", 400, 315, &FreeMonoBold9pt7b);

    // place row near bottom of 480px screen
    int16_t rowY = 300;            // adjust if you need higher/lower
    int16_t prayerBoxW = 140;      // width of each of the 5 boxes
    int16_t prayerBoxH = 90;       // height
    int16_t prayerSpacing = 10;    // gap between boxes
    drawPrayerTimeBoxes(names, times, 5, rowY, prayerBoxW, prayerBoxH, prayerSpacing);

  } while (display.nextPage());
}

void loop() {}
