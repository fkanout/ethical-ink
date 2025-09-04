// GxEPD2Adapter.h
#pragma once
#include "IEpaper.h"

// T is your concrete GxEPD2 display type, e.g. GxEPD2_BW<...>
template <class T>
class GxEPD2Adapter : public IEpaper {
public:
  explicit GxEPD2Adapter(T& d) : d_(d) {}
  ~GxEPD2Adapter() override = default;

  void setFullWindow() override { d_.setFullWindow(); }
  void setPartialWindow(int16_t x, int16_t y, int16_t w, int16_t h) override {
    d_.setPartialWindow(x, y, w, h);
  }
  bool firstPage() override { d_.firstPage(); return true; }
  bool nextPage() override { return d_.nextPage(); }

  void fillScreen(uint16_t color) override { d_.fillScreen(color); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    d_.fillRect(x, y, w, h, color);
  }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    d_.drawRect(x, y, w, h, color);
  }

  void setFont(const GFXfont* f) override { d_.setFont(f); }
  void getTextBounds(const char* str, int16_t x, int16_t y,
                     int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) override {
    d_.getTextBounds(str, x, y, x1, y1, w, h);
  }
  void setTextColor(uint16_t color) override { d_.setTextColor(color); }
  void setCursor(int16_t x, int16_t y) override { d_.setCursor(x, y); }
  size_t print(const char* s) override { return d_.print(s); }

  int16_t width()  const override { return d_.width();  }
  int16_t height() const override { return d_.height(); }

  // Add these to GxEPD2Adapter implementations:
void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override {
    d_.drawLine(x0, y0, x1, y1, color);
}
void fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color) override {
    d_.fillCircle(x0, y0, r, color);
}

private:
  T& d_;
};

