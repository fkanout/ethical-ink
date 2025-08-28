// IEpaper.h
#pragma once
#include <Arduino.h>
#include <gfxfont.h>

class IEpaper {
public:
  virtual ~IEpaper() = default;

  // windowing / page loop
  virtual void setFullWindow() = 0;
  virtual void setPartialWindow(int16_t x, int16_t y, int16_t w, int16_t h) = 0;
  virtual bool firstPage() = 0;     // return true for compatibility
  virtual bool nextPage() = 0;

  // drawing
  virtual void fillScreen(uint16_t color) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;
  virtual void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) = 0;

  // text
  virtual void setFont(const GFXfont* f) = 0;
  virtual void getTextBounds(const char* str, int16_t x, int16_t y,
                             int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) = 0;
  virtual void setTextColor(uint16_t color) = 0;
  virtual void setCursor(int16_t x, int16_t y) = 0;
  virtual size_t print(const char* s) = 0;

  // geometry
  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;
};
