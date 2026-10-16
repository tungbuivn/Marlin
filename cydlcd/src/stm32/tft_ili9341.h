/**
 * cydlcd -- ILI9341 (2.4" SPI TFT) driver, no external library.
 *
 * Part of the standalone display firmware: this MCU renders the screen, Marlin on the
 * mainboard only answers G-code (see cydlcd/README.md).
 *
 * The driver writes straight to the MAN HINH CYD (no framebuffer: 240x320x2 = 150 KB would not
 * fit even the F411's 128 KB), so the UI only repaints the areas whose value changed.
 */
#pragma once

#include <Arduino.h>
#include "cydlcd_config.h"

#define TFT_BLACK   0x0000
#define TFT_WHITE   0xFFFF
#define TFT_GREY    0x8410
#define TFT_DKGREY  0x39E7
#define TFT_RED     0xF800
#define TFT_GREEN   0x07E0
#define TFT_BLUE    0x001F
#define TFT_YELLOW  0xFFE0
#define TFT_ORANGE  0xFD20
#define TFT_CYAN    0x07FF

class ILI9341 {
public:
  void begin();

  uint16_t width() const  { return _w; }
  uint16_t height() const { return _h; }

  void fillScreen(uint16_t color);
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
  void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);

  // 8x8 font scaled by `size` (1 or 2), with its own background colour
  void print(const char *s, int16_t x, int16_t y, uint16_t fg, uint16_t bg, uint8_t size = 1);
  uint16_t textWidth(const char *s, uint8_t size = 1) const { return strlen(s) * 8 * size; }

  // Horizontal bar, `pct` 0..100
  void progressBar(int16_t x, int16_t y, int16_t w, int16_t h, int16_t pct, uint16_t fg, uint16_t bg, uint16_t frame);

private:
  uint16_t _w = 240, _h = 320;

  void cmd(uint8_t c);
  void data(uint8_t d);
  void data16(uint16_t d);
  void setWindow(int16_t x, int16_t y, int16_t w, int16_t h);
  void pushColor(uint16_t color, uint32_t count);
  void glyph(char c, int16_t x, int16_t y, uint16_t fg, uint16_t bg, uint8_t size);
};
