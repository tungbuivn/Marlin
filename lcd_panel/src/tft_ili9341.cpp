/**
 * lcd_panel -- ILI9341 (2.4" SPI TFT) driver, no external library.
 * See tft_ili9341.h for the why (no framebuffer, no lib deps).
 */
#include "tft_ili9341.h"
#include "font8x8.h"
#include <SPI.h>

// ---------------------------------------------------------------------------
// Low level SPI
// ---------------------------------------------------------------------------
static inline void cs_low()  { digitalWrite(TFT_PIN_CS, LOW); }
static inline void cs_high() { digitalWrite(TFT_PIN_CS, HIGH); }

void ILI9341::cmd(uint8_t c) {
  digitalWrite(TFT_PIN_DC, LOW);
  cs_low();
  SPI.transfer(c);
  cs_high();
}

void ILI9341::data(uint8_t d) {
  digitalWrite(TFT_PIN_DC, HIGH);
  cs_low();
  SPI.transfer(d);
  cs_high();
}

void ILI9341::data16(uint16_t d) {
  digitalWrite(TFT_PIN_DC, HIGH);
  cs_low();
  SPI.transfer((uint8_t)(d >> 8));
  SPI.transfer((uint8_t)d);
  cs_high();
}

void ILI9341::begin() {
  pinMode(TFT_PIN_CS, OUTPUT);
  pinMode(TFT_PIN_DC, OUTPUT);
  pinMode(TFT_PIN_RST, OUTPUT);
  cs_high();

  #if TFT_PIN_BL >= 0
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);
  #endif

  SPI.setMISO(TFT_PIN_MISO);
  SPI.setMOSI(TFT_PIN_MOSI);
  SPI.setSCLK(TFT_PIN_SCK);
  SPI.begin();
  SPI.beginTransaction(SPISettings(TFT_SPI_SPEED, MSBFIRST, SPI_MODE0));

  // Hardware reset
  digitalWrite(TFT_PIN_RST, HIGH); delay(5);
  digitalWrite(TFT_PIN_RST, LOW);  delay(20);
  digitalWrite(TFT_PIN_RST, HIGH); delay(150);

  cmd(0x01); delay(120);            // software reset
  cmd(0x28);                        // display off

  cmd(0xCF); data(0x00); data(0xC1); data(0x30);   // power control B
  cmd(0xED); data(0x64); data(0x03); data(0x12); data(0x81);
  cmd(0xE8); data(0x85); data(0x00); data(0x78);
  cmd(0xCB); data(0x39); data(0x2C); data(0x00); data(0x34); data(0x02);
  cmd(0xF7); data(0x20);
  cmd(0xEA); data(0x00); data(0x00);
  cmd(0xC0); data(0x23);                            // power control 1
  cmd(0xC1); data(0x10);                            // power control 2
  cmd(0xC5); data(0x3E); data(0x28);                // VCOM 1
  cmd(0xC7); data(0x86);                            // VCOM 2
  #if   TFT_ROTATION == 0
    cmd(0x36); data(0x48);                          // portrait 240x320
  #elif TFT_ROTATION == 1
    cmd(0x36); data(0x28);                          // landscape 320x240
  #elif TFT_ROTATION == 2
    cmd(0x36); data(0x88);                          // portrait, flipped
  #else
    cmd(0x36); data(0xE8);                          // landscape, flipped
  #endif
  cmd(0x3A); data(0x55);                            // 16 bit/pixel
  cmd(0xB1); data(0x00); data(0x18);                // frame rate
  cmd(0xB6); data(0x08); data(0x82); data(0x27);    // display function
  cmd(0xF2); data(0x00);                            // 3G gamma off
  cmd(0x26); data(0x01);                            // gamma curve
  cmd(0xE0); data(0x0F); data(0x31); data(0x2B); data(0x0C); data(0x0E); data(0x08); data(0x4E);
             data(0xF1); data(0x37); data(0x07); data(0x10); data(0x03); data(0x0E); data(0x09); data(0x00);
  cmd(0xE1); data(0x00); data(0x0E); data(0x14); data(0x03); data(0x11); data(0x07); data(0x31);
             data(0xC1); data(0x48); data(0x08); data(0x0F); data(0x0C); data(0x31); data(0x36); data(0x0F);
  cmd(0x11); delay(120);                            // sleep out
  cmd(0x29);                                        // display on

  const uint8_t r = TFT_ROTATION & 3;
  _w = (r == 1 || r == 3) ? 320 : 240;
  _h = (r == 1 || r == 3) ? 240 : 320;
  fillScreen(TFT_BLACK);
}

void ILI9341::setWindow(int16_t x, int16_t y, int16_t w, int16_t h) {
  cmd(0x2A); data16(x); data16(x + w - 1);          // column address set
  cmd(0x2B); data16(y); data16(y + h - 1);          // page address set
  cmd(0x2C);                                        // memory write
}

void ILI9341::pushColor(uint16_t color, uint32_t count) {
  uint8_t buf[64];
  for (uint8_t i = 0; i < sizeof(buf); i += 2) {
    buf[i]     = (uint8_t)(color >> 8);
    buf[i + 1] = (uint8_t)color;
  }
  digitalWrite(TFT_PIN_DC, HIGH);
  cs_low();
  while (count) {
    const uint32_t n = count > (sizeof(buf) / 2) ? (sizeof(buf) / 2) : count;
    SPI.transfer(buf, n * 2);
    count -= n;
  }
  cs_high();
}

void ILI9341::fillScreen(uint16_t color) { fillRect(0, 0, _w, _h, color); }

void ILI9341::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  if (x >= _w || y >= _h || w <= 0 || h <= 0) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > _w) w = _w - x;
  if (y + h > _h) h = _h - y;
  setWindow(x, y, w, h);
  pushColor(color, uint32_t(w) * uint32_t(h));
}

void ILI9341::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) { fillRect(x, y, w, 1, color); }
void ILI9341::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) { fillRect(x, y, 1, h, color); }

void ILI9341::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
  drawFastHLine(x, y, w, color);
  drawFastHLine(x, y + h - 1, w, color);
  drawFastVLine(x, y, h, color);
  drawFastVLine(x + w - 1, y, h, color);
}

void ILI9341::progressBar(int16_t x, int16_t y, int16_t w, int16_t h, int16_t pct, uint16_t fg, uint16_t bg, uint16_t frame) {
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  const int16_t fill = int16_t((int32_t)(w - 4) * pct / 100);
  fillRect(x, y, w, h, frame);
  fillRect(x + 2, y + 2, w - 4, h - 4, bg);
  if (fill > 0) fillRect(x + 2, y + 2, fill, h - 4, fg);
}

void ILI9341::glyph(char c, int16_t x, int16_t y, uint16_t fg, uint16_t bg, uint8_t size) {
  if (c < 32 || c > 126) c = '?';
  const uint8_t *g = font8x8[c - 32];
  // One window for the whole 8x8 cell: rows are pushed as fg/bg pixels
  setWindow(x, y, 8 * size, 8 * size);
  digitalWrite(TFT_PIN_DC, HIGH);
  cs_low();
  for (uint8_t row = 0; row < 8; row++) {
    const uint8_t bits = pgm_read_byte(&g[row]);
    for (uint8_t sy = 0; sy < size; sy++) {
      for (uint8_t col = 0; col < 8; col++) {
        const uint16_t px = (bits & (1 << col)) ? fg : bg;
        for (uint8_t sx = 0; sx < size; sx++) {
          SPI.transfer((uint8_t)(px >> 8));
          SPI.transfer((uint8_t)px);
        }
      }
    }
  }
  cs_high();
}

void ILI9341::print(const char *s, int16_t x, int16_t y, uint16_t fg, uint16_t bg, uint8_t size) {
  if (!size) size = 1;
  for (; *s; s++, x += 8 * size)
    glyph(*s, x, y, fg, bg, size);
}
