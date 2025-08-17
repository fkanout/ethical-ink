#include <GxEPD2_BW.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold24pt7b.h>  // Larger font for HH:MM
#include <SPI.h>

// Pins for ESP32-S3
#define EPD_CS   10
#define EPD_DC   9
#define EPD_RST  8
#define EPD_BUSY 7

GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));

// Draw a text box
void drawTextBox(const char* text, int16_t x, int16_t y, int16_t width, int16_t height, const GFXfont* font) {
  display.setFont(font);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  display.drawRect(x, y, width, height, GxEPD_BLACK);

  int16_t textX = x + (width - w) / 2 - x1;
  int16_t textY = y + (height - h) / 2 - y1;

  display.setTextColor(GxEPD_BLACK);
  display.setCursor(textX, textY);
  display.print(text);
}

// Draw centered text
void drawCenteredText(const char* text, int16_t centerX, int16_t topY, const GFXfont* font) {
  display.setFont(font);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);

  int16_t textX = centerX - w / 2 - x1;
  int16_t textY = topY - y1;

  display.setTextColor(GxEPD_BLACK);
  display.setCursor(textX, textY);
  display.print(text);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  display.init(115200);  
  display.setRotation(0); // 0 = portrait, 1 = landscape
  display.setFullWindow();

  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE); // Clear screen

    int16_t currentY = 10; // top margin
    int16_t boxWidth = 200;
    int16_t boxHeight = 60;
    int16_t spacing = 20;
    int16_t boxX = (800 - boxWidth) / 2;

    // ===== First box =====
    drawTextBox("Mosque Name", boxX, currentY, boxWidth, boxHeight, &FreeMonoBold9pt7b);

    // ===== Text below first box =====
    currentY += boxHeight + spacing;
    drawCenteredText("Prayer in", 400, currentY, &FreeMonoBold9pt7b);

    // ===== Second box (HH:MM) with bigger font =====
    currentY += 20 + 10; // leave space after text
    drawTextBox("HH:MM", boxX, currentY, 200, 100, &FreeMonoBold24pt7b); // Bigger font

  } while (display.nextPage());

  Serial.println("Drawing complete.");
}

void loop() {
  // Nothing
}
