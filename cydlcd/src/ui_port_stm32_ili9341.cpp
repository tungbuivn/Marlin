/**
 * ui_port_stm32_ili9341.cpp -- PORT PHAN CUNG CHO BOARD STM32 (F411/F407) + ILI9341 + ENCODER.
 *
 * Board nay: LCD **ILI9341** 240x320 (man NGANG 320x240) noi SPI + encoder quay (A/B/SW)
 *   - Ve: driver TU VIET tft_ili9341.cpp (khong dung thu vien ngoai: F411 chi co 128 KB RAM nen
 *     khong the giu framebuffer, UI chi ve lai vung thay doi).
 *   - Nhap: encoder qua ngat (encoder.cpp).
 * Board CYD (ST7789 + cam ung) nam o file RIENG: ui_port_cyd_st7789.cpp.
 */
#include "ui_lvgl.h"

#if CYDLCD_USE_LVGL && !defined(CYDLCD_BOARD_CYD)
#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>          // snprintf (man hinh ket qua hieu chuan)
  #include "tft_ili9341.h"
  #include "encoder.h"

  static ILI9341 s_lcd;

  #if (TFT_ROTATION != 0) && (TFT_ROTATION != 1) && (TFT_ROTATION != 2) && (TFT_ROTATION != 3)
    #error "TFT_ROTATION phai la 0..3"
  #endif

// ===========================================================================
//  Clock
// ===========================================================================
uint32_t ui_port_millis(void) { return millis(); }
//  STM32: day buffer LVGL ra ILI9341 (SPI + chan trong cydlcd_config.h)
// ===========================================================================

static inline void lcd_cs(const bool low)  { digitalWrite(TFT_PIN_CS, low ? LOW : HIGH); }
static inline void lcd_dc(const bool data) { digitalWrite(TFT_PIN_DC, data ? HIGH : LOW); }

static void lcd_cmd(const uint8_t c) {
  lcd_dc(false); lcd_cs(true);
  SPI.transfer(c);
  lcd_cs(false);
}

static void lcd_data16(const uint16_t d) {
  lcd_dc(true); lcd_cs(true);
  SPI.transfer((uint8_t)(d >> 8));
  SPI.transfer((uint8_t)d);
  lcd_cs(false);
}

// Cua so ve + lenh "memory write" (0x2C) giong ILI9341::setWindow()
static void lcd_window(const int32_t x0, const int32_t y0, const int32_t x1, const int32_t y1) {
  lcd_cmd(0x2A); lcd_data16((uint16_t)x0); lcd_data16((uint16_t)x1);
  lcd_cmd(0x2B); lcd_data16((uint16_t)y0); lcd_data16((uint16_t)y1);
  lcd_cmd(0x2C);
}

// ILI9341 nhan 16-bit mau theo kieu big-endian (byte cao truoc), con LVGL giu
// little-endian trong RAM -> dao 2 byte cua tung pixel khi day.
static void lcd_push_pixels(const lv_color_t *px, uint32_t n) {
  const uint8_t *p = (const uint8_t *)px;
  uint8_t buf[64];                                     // 32 pixel = 64 byte
  lcd_dc(true); lcd_cs(true);
  while (n) {
    const uint32_t chunk = (n > 32U) ? 32U : n;
    for (uint32_t i = 0; i < chunk; i++) {
      buf[i * 2U]      = p[1];
      buf[i * 2U + 1U] = p[0];
      p += 2;
    }
    SPI.transfer(buf, (size_t)(chunk * 2U));
    n -= chunk;
  }
  lcd_cs(false);
}

void ui_port_display_init(void) {
  s_lcd.begin();                                       // init + TFT_ROTATION + bat den nen
  // Driver goc mo SPI transaction va khong dong -> cau hinh lai cho chac chan
  SPI.beginTransaction(SPISettings(TFT_SPI_SPEED, MSBFIRST, SPI_MODE0));
  s_lcd.fillScreen(TFT_BLACK);
}

// ===========================================================================
//  Flush callback cua LVGL (STM32: day thang ra ILI9341 qua SPI)
// ===========================================================================
void ui_port_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *px) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  if (w > 0 && h > 0) {
    lcd_window(area->x1, area->y1, area->x2, area->y2);
    lcd_push_pixels(px, (uint32_t)w * (uint32_t)h);
  }
  lv_disp_flush_ready(disp_drv);
}

bool ui_port_input_is_encoder(void) { return true; }

// STM32: khong co touch -> khong co hieu chuan cam ung
bool ui_port_touch_cal_available(void) { return false; }
bool ui_port_touch_cal_is_saved(void) { return false; }
void ui_port_touch_calibrate(void) { }

#define ENC_SW_DEBOUNCE_MS 25U
static bool     s_sw_stable = false;
static uint32_t s_sw_ms     = 0;

void ui_port_input_init(void) {
  pinMode(ENC_PIN_SW, INPUT_PULLUP);                   // nut bam cua encoder
  s_sw_stable = (digitalRead(ENC_PIN_SW) == LOW);
  s_sw_ms = millis();
  enc_begin();                                         // 2 chan A/B (ngat + bang trang thai)
}

// Callback LVGL doc input (encoder)
void ui_port_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  (void)indev_drv;
  data->continue_reading = false;
  // Nut bam: debounce 25ms (encoder.h chi lo 2 chan quay)
  const uint32_t now = millis();
  const bool     raw = (digitalRead(ENC_PIN_SW) == LOW);
  if (raw != s_sw_stable && (now - s_sw_ms) > ENC_SW_DEBOUNCE_MS) {
    s_sw_ms     = now;
    s_sw_stable = raw;
  }
  // Tra ve so detent tich luy tu lan doc truoc (enc_detents() da tu xoa bo dem).
  data->enc_diff = enc_detents();
  data->key      = LV_KEY_ENTER;
  data->state    = s_sw_stable ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

#endif  // CYDLCD_USE_LVGL && !CYDLCD_BOARD_CYD
