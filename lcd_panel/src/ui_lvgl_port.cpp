/**
 * ui_lvgl_port.cpp -- lop phu thuoc PHAN CUNG cho UI LVGL (tach khoi ui_lvgl.cpp).
 *
 *  - ui_port_display_init(): init LCD
 *      STM32 (F407/F411): dung driver co san trong repo `tft_ili9341.h` (ILI9341 qua SPI).
 *      ESP32 "CYD"      : dung TFT_eSPI (cau hinh o tools/tft_setup/User_Setup.h).
 *  - ui_port_flush_cb(): LVGL goi khi co vung can ve -> day pixel ra LCD.
 *  - ui_port_input_init() / ui_port_read_cb():
 *      STM32: encoder quay (encoder.h: enc_begin/enc_detents) + nut bam cua encoder.
 *      CYD  : cam ung XPT2046 (SPI mem - xem ghi chu o duoi).
 *  - ui_port_millis(): dong ho ms cho LVGL tick.
 *
 * GHI CHU KY THUAT (vi sao co vai doan "tu viet"):
 *  1) `ILI9341::setWindow()/pushColor()` la PRIVATE va driver goc khong co ham day 1 vung
 *     pixel tu buffer (pushColor chi day 1 mau lap lai), nen o day phai tu viet phan day
 *     buffer ra ILI9341. Phan init/rotation/den nen van dung `ILI9341::begin()` co san.
 *  2) CYD: TFT_eSPI chi ho tro XPT2046 khi touch nam CHUNG bus SPI voi LCD (chi khac CS),
 *     con CYD di day rieng (T_CLK 25 / T_DIN 32 / T_DO 39 / T_CS 33) -> `tft.getTouch()`
 *     khong doc duoc cam ung. Vi vay phan doc cam ung o day tu bit-bang SPI.
 *     Muon thu `tft.getTouch()`: xem PANEL_CYD_TOUCH_USE_TFTESPI ben duoi.
 */
#include "ui_lvgl.h"

#if PANEL_USE_LVGL

#include <Arduino.h>
#include <SPI.h>

// ===========================================================================
//  Chon driver LCD theo board
// ===========================================================================
#if defined(PANEL_BOARD_CYD)

  #include <TFT_eSPI.h>
  static TFT_eSPI s_lcd;

  // Canh bao som neu tools/tft_setup/User_Setup.h khong duoc TFT_eSPI nap
  #ifndef PANEL_CYD_SETUP_LOADED
    #warning "TFT_eSPI khong doc tools/tft_setup/User_Setup.h -- them -DUSER_SETUP_LOADED=1 -include tools/tft_setup/User_Setup.h vao build_flags"
  #endif

  #if (UI_LCD_HOR_RES != TFT_WIDTH) || (UI_LCD_VER_RES != TFT_HEIGHT)
    #error "UI_LCD_* trong ui_lvgl.h khong khop TFT_WIDTH/TFT_HEIGHT cua TFT_eSPI"
  #endif

#else   // ---------------- STM32 (F407 / F411) ----------------

  #include "tft_ili9341.h"
  #include "encoder.h"

  static ILI9341 s_lcd;

  #if (TFT_ROTATION != 0) && (TFT_ROTATION != 2)
    #error "UI LVGL chi ho tro man portrait 240x320 (TFT_ROTATION 0 hoac 2)"
  #endif

#endif

// ===========================================================================
//  Clock
// ===========================================================================
uint32_t ui_port_millis(void) { return millis(); }

// ===========================================================================
//  STM32: day buffer LVGL ra ILI9341 (SPI + chan trong panel_config.h)
// ===========================================================================
#if !defined(PANEL_BOARD_CYD)

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

#endif  // !PANEL_BOARD_CYD

// ===========================================================================
//  CYD: XPT2046 (SPI mem) + flush bang TFT_eSPI
// ===========================================================================
#if defined(PANEL_BOARD_CYD)

  // ---------------------------------------------------------------------------
  //  Hieu chuan cam ung: chinh 4 macro nay sau khi thu tren board that.
  //    RAW_MIN/MAX: gia tri tho 12-bit khi cham vao 2 dau truc (thuong ~200..3800)
  //    SWAP_XY / INVERT_*: doi truc cho khop huong man hinh (rotation 0)
  // ---------------------------------------------------------------------------
  #ifndef PANEL_CYD_TOUCH_RAW_MIN
    #define PANEL_CYD_TOUCH_RAW_MIN 200
  #endif
  #ifndef PANEL_CYD_TOUCH_RAW_MAX
    #define PANEL_CYD_TOUCH_RAW_MAX 3800
  #endif
  #ifndef PANEL_CYD_TOUCH_SWAP_XY
    #define PANEL_CYD_TOUCH_SWAP_XY 1
  #endif
  #ifndef PANEL_CYD_TOUCH_INVERT_X
    #define PANEL_CYD_TOUCH_INVERT_X 0
  #endif
  #ifndef PANEL_CYD_TOUCH_INVERT_Y
    #define PANEL_CYD_TOUCH_INVERT_Y 0
  #endif

  // Dat 1 neu ban da patch TFT_eSPI de touch chay tren bus rieng
  //#define PANEL_CYD_TOUCH_USE_TFTESPI 1

  #ifndef PANEL_CYD_TOUCH_CS
    #define PANEL_CYD_TOUCH_CS 33
  #endif
  #ifndef PANEL_CYD_TOUCH_IRQ
    #define PANEL_CYD_TOUCH_IRQ 36
  #endif
  #ifndef PANEL_CYD_TOUCH_DIN
    #define PANEL_CYD_TOUCH_DIN 32
  #endif
  #ifndef PANEL_CYD_TOUCH_DO
    #define PANEL_CYD_TOUCH_DO 39
  #endif
  #ifndef PANEL_CYD_TOUCH_CLK
    #define PANEL_CYD_TOUCH_CLK 25
  #endif

  #ifdef PANEL_CYD_TOUCH_USE_TFTESPI

  static bool cyd_touch_read(lv_coord_t *x, lv_coord_t *y) {
    uint16_t tx = 0, ty = 0;
    if (!s_lcd.getTouch(&tx, &ty)) return false;
    *x = (lv_coord_t)tx;
    *y = (lv_coord_t)ty;
    return true;
  }

  #else   // ---- SPI mem (mac dinh, dung cho CYD nguyen ban) ----

  static inline void tsp_dly(void) { delayMicroseconds(1); }

  // 1 byte SPI mem: MSB truoc, doc MISO o suon len
  static uint8_t tsp_xfer(uint8_t b) {
    uint8_t r = 0;
    for (uint8_t i = 0; i < 8; i++) {
      digitalWrite(PANEL_CYD_TOUCH_DIN, (b & 0x80U) ? HIGH : LOW);
      b = (uint8_t)(b << 1);
      digitalWrite(PANEL_CYD_TOUCH_CLK, HIGH);
      tsp_dly();
      r = (uint8_t)(r << 1);
      if (digitalRead(PANEL_CYD_TOUCH_DO)) r = (uint8_t)(r | 1U);
      digitalWrite(PANEL_CYD_TOUCH_CLK, LOW);
      tsp_dly();
    }
    return r;
  }

  // Doc 1 kenh 12-bit: 0x90 = X (XP), 0xD0 = Y (YP)
  static uint16_t xpt_read12(const uint8_t cmd) {
    digitalWrite(PANEL_CYD_TOUCH_CS, LOW);
    tsp_dly();
    tsp_xfer(cmd);
    const uint8_t hi = tsp_xfer(0x00);
    const uint8_t lo = tsp_xfer(0x00);
    tsp_dly();
    digitalWrite(PANEL_CYD_TOUCH_CS, HIGH);
    return (uint16_t)((((uint16_t)hi << 8) | (uint16_t)lo) >> 3) & 0x0FFFU;
  }

  static lv_coord_t tsp_map(const uint16_t raw, const lv_coord_t out_max, const bool invert) {
    uint16_t v = raw;
    if (v < (uint16_t)PANEL_CYD_TOUCH_RAW_MIN) v = (uint16_t)PANEL_CYD_TOUCH_RAW_MIN;
    if (v > (uint16_t)PANEL_CYD_TOUCH_RAW_MAX) v = (uint16_t)PANEL_CYD_TOUCH_RAW_MAX;
    const uint32_t num = (uint32_t)(v - (uint16_t)PANEL_CYD_TOUCH_RAW_MIN) * (uint32_t)(out_max - 1);
    const uint32_t den = (uint32_t)((uint16_t)PANEL_CYD_TOUCH_RAW_MAX - (uint16_t)PANEL_CYD_TOUCH_RAW_MIN);
    lv_coord_t r = (lv_coord_t)(num / den);
    if (invert) r = (lv_coord_t)(out_max - 1 - r);
    return r;
  }

  static bool cyd_touch_read(lv_coord_t *x, lv_coord_t *y) {
    // T_IRQ muc thap khi co cham (tren CYD co dien tro keo len ngoai)
    if (digitalRead(PANEL_CYD_TOUCH_IRQ) != LOW) return false;

    const uint16_t raw_x = xpt_read12(0x90);      // XP
    const uint16_t raw_y = xpt_read12(0xD0);      // YP
    // Gia tri tho ngoai dai -> nhieu (nhieu khi vua cham/vua nha)
    if (raw_x < 100 || raw_x > 4000 || raw_y < 100 || raw_y > 4000) return false;
    #if PANEL_CYD_TOUCH_SWAP_XY
      *x = tsp_map(raw_y, UI_LCD_HOR_RES, PANEL_CYD_TOUCH_INVERT_X);
      *y = tsp_map(raw_x, UI_LCD_VER_RES, PANEL_CYD_TOUCH_INVERT_Y);
    #else
      *x = tsp_map(raw_x, UI_LCD_HOR_RES, PANEL_CYD_TOUCH_INVERT_X);
      *y = tsp_map(raw_y, UI_LCD_VER_RES, PANEL_CYD_TOUCH_INVERT_Y);
    #endif
    return true;
  }

  #endif  // PANEL_CYD_TOUCH_USE_TFTESPI

  void ui_port_display_init(void) {
    s_lcd.init();
    s_lcd.setRotation(0);                              // portrait 240x320
    s_lcd.fillScreen(TFT_BLACK);
  }

#endif  // PANEL_BOARD_CYD

// ===========================================================================
//  Flush callback cua LVGL
// ===========================================================================
void ui_port_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *px) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  if (w > 0 && h > 0) {
  #if defined(PANEL_BOARD_CYD)
    // pushColors(..., swap = true): TFT_eSPI doi 2 byte -> dung endianness cua ILI9341
    s_lcd.startWrite();
    s_lcd.setAddrWindow(area->x1, area->y1, w, h);
    s_lcd.pushColors(reinterpret_cast<uint16_t *>(px), (uint32_t)w * (uint32_t)h, true);
    s_lcd.endWrite();
  #else
    lcd_window(area->x1, area->y1, area->x2, area->y2);
    lcd_push_pixels(px, (uint32_t)w * (uint32_t)h);
  #endif
  }
  lv_disp_flush_ready(disp_drv);
}

// ===========================================================================
//  Input
// ===========================================================================
#if defined(PANEL_BOARD_CYD)

// CYD khong co encoder -> cam ung
bool ui_port_input_is_encoder(void) { return false; }

void ui_port_input_init(void) {
#ifndef PANEL_CYD_TOUCH_USE_TFTESPI
  pinMode(PANEL_CYD_TOUCH_CS, OUTPUT);
  digitalWrite(PANEL_CYD_TOUCH_CS, HIGH);
  pinMode(PANEL_CYD_TOUCH_CLK, OUTPUT);
  digitalWrite(PANEL_CYD_TOUCH_CLK, LOW);
  pinMode(PANEL_CYD_TOUCH_DIN, OUTPUT);
  pinMode(PANEL_CYD_TOUCH_DO, INPUT);                  // GPIO39 chi doc
  pinMode(PANEL_CYD_TOUCH_IRQ, INPUT);                 // GPIO36 chi doc, co keo len ngoai
#endif
}

#else   // ---------------- STM32: encoder ----------------

bool ui_port_input_is_encoder(void) { return true; }

#define ENC_SW_DEBOUNCE_MS 25U
static bool     s_sw_stable = false;
static uint32_t s_sw_ms     = 0;

void ui_port_input_init(void) {
  pinMode(ENC_PIN_SW, INPUT_PULLUP);                   // nut bam cua encoder
  s_sw_stable = (digitalRead(ENC_PIN_SW) == LOW);
  s_sw_ms = millis();
  enc_begin();                                         // 2 chan A/B (ngat + bang trang thai)
}

#endif

// Callback LVGL doc input (encoder hoac touch)
void ui_port_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  (void)indev_drv;
  data->continue_reading = false;

#if defined(PANEL_BOARD_CYD)
  lv_coord_t x = 0, y = 0;
  data->point.x = 0;
  data->point.y = 0;
  if (cyd_touch_read(&x, &y)) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
#else
  // Nut bam: debounce 25ms (encoder.h chi lo 2 chan quay)
  const uint32_t now = millis();
  const bool     raw = (digitalRead(ENC_PIN_SW) == LOW);
  if (raw != s_sw_stable && (now - s_sw_ms) > ENC_SW_DEBOUNCE_MS) {
    s_sw_ms     = now;
    s_sw_stable = raw;
  }

  // Tra ve so detent tich luy tu lan doc truoc (enc_detents() da tu xoa bo dem).
  // LVGL 8.3: type = ENCODER thi mac dinh data->key = LV_KEY_ENTER, o day set lai
  // cho tuong minh.
  data->enc_diff = enc_detents();
  data->key      = LV_KEY_ENTER;
  data->state    = s_sw_stable ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
#endif
}

#endif  // PANEL_USE_LVGL
