#include <GxEPD2_BW.h>

#include <Fonts/FreeMonoBold9pt7b.h>

#include <SPI.h>



// Pins for ESP32-S3 (غيّرها إذا لزم الأمر)

#define EPD_CS   10

#define EPD_DC   9

#define EPD_RST  8

#define EPD_BUSY 7



// GDEH075T7 V2 - 7.5" b/w screen

GxEPD2_BW<GxEPD2_750_T7, GxEPD2_750_T7::HEIGHT> display(GxEPD2_750_T7(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));



void setup() {

  Serial.begin(115200);

  delay(100);

  

  display.init(115200);  // SPI speed

  display.setRotation(1);

  display.setFont(&FreeMonoBold9pt7b);

  display.setTextColor(GxEPD_BLACK);

  display.setFullWindow();



  display.firstPage();

  do {

    display.fillScreen(GxEPD_WHITE);

    display.setCursor(20, 100);

    display.println("Hello, World!");

    display.drawRect(10, 120, 100, 50, GxEPD_BLACK);

    display.fillCircle(150, 150, 30, GxEPD_BLACK);

  } while (display.nextPage());



  Serial.println("Done.");

}



void loop() {}