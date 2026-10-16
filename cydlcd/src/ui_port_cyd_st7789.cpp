/**
 * ui_port_cyd_st7789.cpp -- PORT PHAN CUNG CHO MAN HINH CYD (ESP32-2432S028R).
 *
 * Board CYD: LCD **ST7789** 240x320 (man NGANG 320x240) + cam ung **XPT2046** cam chan rieng.
 *   - Ve: TFT_eSPI (driver ST7789) - cau hinh chan o tools/tft_setup_cyd/User_Setup.h.
 *   - Cam ung: bit-bang SPI mem (T_CLK 25 / T_DIN 32 / T_DO 39 / T_CS 33 / T_IRQ 36) + hieu chuan
 *     luu vao NVS.
 * Board STM32 (ILI9341 + encoder) nam o file RIENG: ui_port_stm32_ili9341.cpp.
 */
#include "ui_lvgl.h"

#if CYDLCD_USE_LVGL && defined(CYDLCD_BOARD_CYD)
#include <Arduino.h>
#include <SPI.h>
#include <stdio.h>          // snprintf (man hinh ket qua hieu chuan)
#include "ui_lang.h"        // T() - da ngon ngu (chu tren man HIEU CHUAN cam ung)
#include "ui_colortest.h"   // bang mau cho lenh `UI COLORTEST` (ui_port_colortest_raw)
  #include <TFT_eSPI.h>
  #include <Preferences.h>          // luu ket qua hieu chuan cam ung vao NVS (khong mat khi reset)
  static TFT_eSPI s_lcd;

  // Canh bao som neu tools/tft_setup_cyd/User_Setup.h khong duoc TFT_eSPI nap
  #ifndef CYDLCD_CYD_SETUP_LOADED
    #warning "TFT_eSPI khong doc tools/tft_setup_cyd/User_Setup.h -- them -DUSER_SETUP_LOADED=1 -include tools/tft_setup_cyd/User_Setup.h vao build_flags"
  #endif

  // UI la man NGANG 320x240 (ui_lvgl.h) con man hinh CYD la 240x320 -> cho phep ca 2 chieu
  #if !((UI_LCD_HOR_RES == TFT_WIDTH  && UI_LCD_VER_RES == TFT_HEIGHT) || \
        (UI_LCD_HOR_RES == TFT_HEIGHT && UI_LCD_VER_RES == TFT_WIDTH))
    #error "UI_LCD_* trong ui_lvgl.h khong khop TFT_WIDTH/TFT_HEIGHT cua TFT_eSPI"   // i18n-ok (khong phai chu hien thi)
  #endif

// ===========================================================================
//  Clock
// ===========================================================================
uint32_t ui_port_millis(void) { return millis(); }
// ===========================================================================
//  CYD: XPT2046 (SPI mem) + flush bang TFT_eSPI
// ===========================================================================

  // ---------------------------------------------------------------------------
  //  Hieu chuan cam ung: chinh cac macro nay sau khi thu tren board that
  //  (dat chung trong cydlcd_config.h cung duoc — o day chi lay gia tri mac dinh).
  //    XMIN/XMAX, YMIN/YMAX: gia tri tho 12-bit tai 2 dau MOI TRUC (thuong khac nhau!)
  //    SWAP_XY / INVERT_*: doi truc cho khop huong man hinh (rotation 0)
  //  Bat -DCYDLCD_CYD_DEBUG=1 de in gia tri tho + toa do pixel ra Serial 115200 khi cham.
  // ---------------------------------------------------------------------------
  #ifndef CYDLCD_CYD_TOUCH_XMIN
    #define CYDLCD_CYD_TOUCH_XMIN 200
  #endif
  #ifndef CYDLCD_CYD_TOUCH_XMAX
    #define CYDLCD_CYD_TOUCH_XMAX 3800
  #endif
  #ifndef CYDLCD_CYD_TOUCH_YMIN
    #define CYDLCD_CYD_TOUCH_YMIN 200
  #endif
  #ifndef CYDLCD_CYD_TOUCH_YMAX
    #define CYDLCD_CYD_TOUCH_YMAX 3800
  #endif
  #ifndef CYDLCD_CYD_TOUCH_SWAP_XY
    #define CYDLCD_CYD_TOUCH_SWAP_XY 1
  #endif
  #ifndef CYDLCD_CYD_TOUCH_INVERT_X
    #define CYDLCD_CYD_TOUCH_INVERT_X 0
  #endif
  #ifndef CYDLCD_CYD_TOUCH_INVERT_Y
    #define CYDLCD_CYD_TOUCH_INVERT_Y 0
  #endif

  // Dat 1 neu ban da patch TFT_eSPI de touch chay tren bus rieng
  //#define CYDLCD_CYD_TOUCH_USE_TFTESPI 1

  #if defined(CYDLCD_CYD_DEBUG)
    // -------------------------------------------------------------------------
    //  Hieu chuan KHONG can dong bo thoi gian:
    //  Ghi nho 5 CUM cham gan nhat (1 cum = 1 lan cham; cach nhau > 1 s la cum moi) va
    //  in lai moi giay -> cham 5 diem theo thu tu bat ky luc nao, sau do doc serial luc
    //  nao cung con du lieu (chi mat khi reset board).
    //  Thu tu cham: 1=goc TREN-TRAI, 2=goc TREN-PHAI, 3=goc DUOI-PHAI, 4=goc DUOI-TRAI, 5=tam.
    //  Dong in ra:  [CAL] 1:raw=3722,3628(n7) 2:raw=... (raw_x,raw_y trung binh cua cum)
    // -------------------------------------------------------------------------
    #define CAL_CLUSTERS 5
    typedef struct { uint32_t sx, sy; uint16_t n; uint32_t t; } cal_pt_t;
    static cal_pt_t s_cal[CAL_CLUSTERS];
    static uint8_t  s_cal_n = 0;
    static uint32_t s_cal_print_ms = 0;

    static void cal_add(const uint16_t rx, const uint16_t ry) {
      const uint32_t now = millis();
      if (s_cal_n && (now - s_cal[s_cal_n - 1].t) < 1000U) {      // van la cung 1 lan cham
        cal_pt_t *p = &s_cal[s_cal_n - 1];
        p->sx += rx; p->sy += ry; p->n++; p->t = now;
      }
      else {                                                      // cum moi
        if (s_cal_n >= CAL_CLUSTERS) {                            // giu 5 cum MOI NHAT
          for (uint8_t i = 1; i < CAL_CLUSTERS; i++) s_cal[i - 1] = s_cal[i];
          s_cal_n = CAL_CLUSTERS - 1;
        }
        cal_pt_t *p = &s_cal[s_cal_n++];
        p->sx = rx; p->sy = ry; p->n = 1; p->t = now;
      }
    }

    static void cal_print_1hz(void) {
      if (!s_cal_n) return;
      const uint32_t now = millis();
      if (now - s_cal_print_ms < 1000U) return;
      s_cal_print_ms = now;
      Serial.print("[CAL]");
      for (uint8_t i = 0; i < s_cal_n; i++)
        Serial.printf(" %u:raw=%u,%u(n%u)", (unsigned)(i + 1),
                      (unsigned)(s_cal[i].sx / s_cal[i].n), (unsigned)(s_cal[i].sy / s_cal[i].n),
                      (unsigned)s_cal[i].n);
      Serial.println();
    }
  #endif  // CYDLCD_CYD_DEBUG

  #ifndef CYDLCD_CYD_TOUCH_CS
    #define CYDLCD_CYD_TOUCH_CS 33
  #endif
  #ifndef CYDLCD_CYD_TOUCH_IRQ
    #define CYDLCD_CYD_TOUCH_IRQ 36
  #endif
  #ifndef CYDLCD_CYD_TOUCH_DIN
    #define CYDLCD_CYD_TOUCH_DIN 32
  #endif
  #ifndef CYDLCD_CYD_TOUCH_DO
    #define CYDLCD_CYD_TOUCH_DO 39
  #endif
  #ifndef CYDLCD_CYD_TOUCH_CLK
    #define CYDLCD_CYD_TOUCH_CLK 25
  #endif

  // ---------------------------------------------------------------------------
  //  Trang thai hieu chuan cam ung (RUNTIME, khong phai macro):
  //  gia tri mac dinh lay tu cydlcd_config.h; neu da hieu chuan bang tay (menu Calibrate)
  //  thi lay tu NVS. Nho vay hieu chuan lai duoc ma khong phai build lai firmware.
  // ---------------------------------------------------------------------------
  static uint16_t s_t_xmin = CYDLCD_CYD_TOUCH_XMIN, s_t_xmax = CYDLCD_CYD_TOUCH_XMAX;
  static uint16_t s_t_ymin = CYDLCD_CYD_TOUCH_YMIN, s_t_ymax = CYDLCD_CYD_TOUCH_YMAX;
  static uint8_t  s_t_swap = (CYDLCD_CYD_TOUCH_SWAP_XY  ? 1 : 0);
  static uint8_t  s_t_invx = (CYDLCD_CYD_TOUCH_INVERT_X ? 1 : 0);
  static uint8_t  s_t_invy = (CYDLCD_CYD_TOUCH_INVERT_Y ? 1 : 0);
  static bool     s_t_from_nvs = false;

  #define TOUCH_NVS_NS "cydtouch"

  static void touch_cal_load(void) {
    Preferences p;
    if (!p.begin(TOUCH_NVS_NS, true)) return;               // read-only
    s_t_xmin = p.getUShort("xmin", s_t_xmin);
    s_t_xmax = p.getUShort("xmax", s_t_xmax);
    s_t_ymin = p.getUShort("ymin", s_t_ymin);
    s_t_ymax = p.getUShort("ymax", s_t_ymax);
    s_t_swap = p.getUChar("swap", s_t_swap);
    s_t_invx = p.getUChar("invx", s_t_invx);
    s_t_invy = p.getUChar("invy", s_t_invy);
    s_t_from_nvs = true;
    p.end();
    Serial.printf("[TOUCHCAL] nap tu NVS: swap=%u invX=%u invY=%u X=%u..%u Y=%u..%u\n",
                  (unsigned)s_t_swap, (unsigned)s_t_invx, (unsigned)s_t_invy,
                  (unsigned)s_t_xmin, (unsigned)s_t_xmax, (unsigned)s_t_ymin, (unsigned)s_t_ymax);
  }

  static void touch_cal_save(void) {
    Preferences p;
    if (!p.begin(TOUCH_NVS_NS, false)) return;
    p.putUShort("xmin", s_t_xmin); p.putUShort("xmax", s_t_xmax);
    p.putUShort("ymin", s_t_ymin); p.putUShort("ymax", s_t_ymax);
    p.putUChar("swap", s_t_swap);  p.putUChar("invx", s_t_invx); p.putUChar("invy", s_t_invy);
    p.end();
    s_t_from_nvs = true;
  }

  #ifdef CYDLCD_CYD_TOUCH_USE_TFTESPI

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
      digitalWrite(CYDLCD_CYD_TOUCH_DIN, (b & 0x80U) ? HIGH : LOW);
      b = (uint8_t)(b << 1);
      digitalWrite(CYDLCD_CYD_TOUCH_CLK, HIGH);
      tsp_dly();
      r = (uint8_t)(r << 1);
      if (digitalRead(CYDLCD_CYD_TOUCH_DO)) r = (uint8_t)(r | 1U);
      digitalWrite(CYDLCD_CYD_TOUCH_CLK, LOW);
      tsp_dly();
    }
    return r;
  }

  // Doc 1 kenh 12-bit: 0x90 = X (XP), 0xD0 = Y (YP)
  static uint16_t xpt_read12(const uint8_t cmd) {
    digitalWrite(CYDLCD_CYD_TOUCH_CS, LOW);
    tsp_dly();
    tsp_xfer(cmd);
    const uint8_t hi = tsp_xfer(0x00);
    const uint8_t lo = tsp_xfer(0x00);
    tsp_dly();
    digitalWrite(CYDLCD_CYD_TOUCH_CS, HIGH);
    return (uint16_t)((((uint16_t)hi << 8) | (uint16_t)lo) >> 3) & 0x0FFFU;
  }

  static lv_coord_t tsp_map(const uint16_t raw, const lv_coord_t out_max, const bool invert,
                            const uint16_t rmin, const uint16_t rmax) {
    uint16_t v = raw;
    if (v < rmin) v = rmin;
    if (v > rmax) v = rmax;
    const uint32_t den = (uint32_t)(rmax - rmin);
    lv_coord_t r = den ? (lv_coord_t)(((uint32_t)(v - rmin) * (uint32_t)(out_max - 1)) / den) : 0;
    if (invert) r = (lv_coord_t)(out_max - 1 - r);
    return r;
  }

  static bool cyd_touch_read(lv_coord_t *x, lv_coord_t *y) {
    // T_IRQ muc thap khi co cham (tren CYD co dien tro keo len ngoai)
    if (digitalRead(CYDLCD_CYD_TOUCH_IRQ) != LOW) return false;

    const uint16_t raw_x = xpt_read12(0x90);      // XP
    const uint16_t raw_y = xpt_read12(0xD0);      // YP
    // Gia tri tho ngoai dai -> nhieu (nhieu khi vua cham/vua nha)
    if (raw_x < 100 || raw_x > 4000 || raw_y < 100 || raw_y > 4000) return false;
    #if defined(CYDLCD_CYD_DEBUG)
      cal_add(raw_x, raw_y);                        // nho lai de hieu chuan (xem cal_print_1hz)
    #endif
    // Dung gia tri hieu chuan RUNTIME (NVS neu da hieu chuan, khong thi lay tu cydlcd_config.h)
    if (s_t_swap) {
      *x = tsp_map(raw_y, UI_LCD_HOR_RES, s_t_invx, s_t_ymin, s_t_ymax);
      *y = tsp_map(raw_x, UI_LCD_VER_RES, s_t_invy, s_t_xmin, s_t_xmax);
    }
    else {
      *x = tsp_map(raw_x, UI_LCD_HOR_RES, s_t_invx, s_t_xmin, s_t_xmax);
      *y = tsp_map(raw_y, UI_LCD_VER_RES, s_t_invy, s_t_ymin, s_t_ymax);
    }
    #if defined(CYDLCD_CYD_DEBUG)
      static uint32_t dbg_last = 0;
      if (millis() - dbg_last > 150) {
        dbg_last = millis();
        Serial.printf("[TOUCH] raw=%u,%u -> px=%d,%d | swap=%u invX=%u invY=%u X=%u..%u Y=%u..%u\n",
                      (unsigned)raw_x, (unsigned)raw_y, (int)*x, (int)*y,
                      (unsigned)s_t_swap, (unsigned)s_t_invx, (unsigned)s_t_invy,
                      (unsigned)s_t_xmin, (unsigned)s_t_xmax,
                      (unsigned)s_t_ymin, (unsigned)s_t_ymax);
      }
    #endif
    return true;
  }

  // ===========================================================================
  //  HIEU CHUAN 4 GOC (khong can biet truoc goc co nhan hay khong)
  //   1) Man hinh hien TEN GOC can hieu chuan o giua + huong dan ngan.
  //   2) Bat dau nhan cam ung -> CA MAN HINH DOI SANG XANH (tin hieu duoc chap nhan).
  //   3) Nguoi dung keo ngon tay/but SAT VAO GOC do; luc NHAC TAY chinh la luc mat cam ung
  //      -> lay CUC TRI (min/max) ghi duoc trong ca qua trinh keo lam toa do goc do.
  //   4) Lam lan luot: TREN-TRAI, TREN-PHAI, DUOI-PHAI, DUOI-TRAI.
  //   5) Tu 4 goc suy ra swap/invert/bien roi LUU vao NVS -> reset khong mat.
  //  Vao bang: menu "Calibrate touch", hoac GIU TAY tren man hinh luc cap dien (thoat hiem
  //  khi hieu chuan cu qua te, khong cham duoc vao menu).
  // ===========================================================================
  static bool cal_touched(void) { return digitalRead(CYDLCD_CYD_TOUCH_IRQ) == LOW; }

  static bool cal_read_raw(uint16_t *rx, uint16_t *ry) {
    const uint16_t a = xpt_read12(0x90);          // XP
    const uint16_t b = xpt_read12(0xD0);          // YP
    if (a < 100 || a > 4000 || b < 100 || b > 4000) return false;
    *rx = a; *ry = b;
    return true;
  }

  static void cal_draw(const uint16_t bg, const char *l1, const char *l2, const char *l3) {
    s_lcd.fillScreen(bg);
    s_lcd.setTextDatum(MC_DATUM);
    s_lcd.setTextColor(TFT_WHITE, bg);
    s_lcd.setTextFont(4);
    s_lcd.drawString(l1, UI_LCD_HOR_RES / 2, UI_LCD_VER_RES / 2 - 34);
    s_lcd.setTextFont(2);
    if (l2) s_lcd.drawString(l2, UI_LCD_HOR_RES / 2, UI_LCD_VER_RES / 2 + 6);
    if (l3) s_lcd.drawString(l3, UI_LCD_HOR_RES / 2, UI_LCD_VER_RES / 2 + 28);
    s_lcd.setTextDatum(TL_DATUM);
  }

  static int cal_abs(const int v) { return v < 0 ? -v : v; }
  static uint16_t cal_lo(const uint16_t a, const uint16_t b) { return a < b ? a : b; }
  static uint16_t cal_hi(const uint16_t a, const uint16_t b) { return a > b ? a : b; }

  void ui_port_touch_calibrate(void) {
    static const char *const NAME[4] = {"TOP-LEFT", "TOP-RIGHT", "BOTTOM-RIGHT", "BOTTOM-LEFT"};
    uint16_t cx[4] = {0}, cy[4] = {0};

    Serial.println("[TOUCHCAL] bat dau hieu chuan 4 goc");
    for (uint8_t i = 0; i < 4; i++) {
      bool ok = false;
      for (uint8_t attempt = 0; attempt < 3 && !ok; attempt++) {
        uint32_t t0 = millis();
        while (cal_touched() && (millis() - t0) < 5000U) delay(5);         // doi nhac tay
        cal_draw(TFT_BLACK, NAME[i], T(UI_T_CAL_CORNER_1), T(UI_T_CAL_CORNER_2));
        t0 = millis();
        while (!cal_touched() && (millis() - t0) < 60000U) delay(5);       // doi bat dau cham
        if (!cal_touched()) { cal_draw(TFT_RED, NAME[i], T(UI_T_CAL_NO_TOUCH), T(UI_T_CAL_RETRY)); delay(1500); continue; }

        cal_draw(TFT_GREEN, NAME[i], T(UI_T_CAL_READING), T(UI_T_CAL_DRAG_HOLD));   // <- ca man hinh XANH
        const bool want_max_x = (i == 1 || i == 2);                        // TR, BR: raw_x lon nhat
        const bool want_max_y = (i == 2 || i == 3);                        // BR, BL: raw_y lon nhat
        uint16_t ex = 0, ey = 0, n = 0, px = 0, py = 0;
        bool first = true;
        while (cal_touched()) {
          uint16_t rx, ry;
          if (cal_read_raw(&rx, &ry)) {
            // Mau nhay qua xa mau truoc (>500) = luc nhac tay -> bo qua
            const bool jump = (!first && (cal_abs((int)rx - (int)px) > 500 || cal_abs((int)ry - (int)py) > 500));
            if (!jump) {
              if (first) { ex = rx; ey = ry; first = false; }
              else {
                if (want_max_x ? (rx > ex) : (rx < ex)) ex = rx;
                if (want_max_y ? (ry > ey) : (ry < ey)) ey = ry;
              }
              n++; px = rx; py = ry;
            }
          }
          delay(4);
        }
        if (n >= 5) { cx[i] = ex; cy[i] = ey; ok = true;
                      Serial.printf("[TOUCHCAL] %s raw=%u,%u (n=%u)\n", NAME[i],
                                    (unsigned)ex, (unsigned)ey, (unsigned)n); }
        else { cal_draw(TFT_RED, NAME[i], T(UI_T_CAL_TOO_SHORT), T(UI_T_CAL_TOUCH_AGAIN)); delay(1500); }
      }
      if (!ok) {
        cal_draw(TFT_RED, "LOI", T(UI_T_CAL_FAIL_L1), T(UI_T_CAL_FAIL_L2));
        Serial.println("[TOUCHCAL] that bai");
        delay(2500);
        return;
      }
    }

    // ---- Suy ra swap / invert / bien tu 4 goc ----
    // raw_x lech nhieu giua TRAI-PHAI => raw_x la truc NGANG (khong swap); nguoc lai thi swap.
    const uint32_t lr = (uint32_t)cal_abs((int)cx[0] - (int)cx[1]) + (uint32_t)cal_abs((int)cx[3] - (int)cx[2]);
    const uint32_t tb = (uint32_t)cal_abs((int)cx[0] - (int)cx[3]) + (uint32_t)cal_abs((int)cx[1] - (int)cx[2]);
    const bool swap = (tb > lr);

    uint16_t xlo, xhi, ylo, yhi;
    uint8_t  invx, invy;
    if (!swap) {
      xlo = cal_lo(cal_lo(cx[0], cx[3]), cal_lo(cx[1], cx[2]));     // raw_x: trai / phai
      xhi = cal_hi(cal_hi(cx[0], cx[3]), cal_hi(cx[1], cx[2]));
      ylo = cal_lo(cal_lo(cy[0], cy[1]), cal_lo(cy[2], cy[3]));     // raw_y: tren / duoi
      yhi = cal_hi(cal_hi(cy[0], cy[1]), cal_hi(cy[2], cy[3]));
      invx = (cx[0] > cx[1]) ? 1 : 0;                               // trai > phai -> dao truc X
      invy = (cy[0] > cy[2]) ? 1 : 0;                               // tren  > duoi -> dao truc Y
    }
    else {
      xlo = cal_lo(cal_lo(cy[0], cy[3]), cal_lo(cy[1], cy[2]));     // truc X lay tu raw_y
      xhi = cal_hi(cal_hi(cy[0], cy[3]), cal_hi(cy[1], cy[2]));
      ylo = cal_lo(cal_lo(cx[0], cx[1]), cal_lo(cx[2], cx[3]));     // truc Y lay tu raw_x
      yhi = cal_hi(cal_hi(cx[0], cx[1]), cal_hi(cx[2], cx[3]));
      invx = (cy[0] > cy[1]) ? 1 : 0;
      invy = (cx[0] > cx[2]) ? 1 : 0;
    }

    s_t_xmin = xlo; s_t_xmax = xhi; s_t_ymin = ylo; s_t_ymax = yhi;
    s_t_swap = swap ? 1 : 0; s_t_invx = invx; s_t_invy = invy;
    touch_cal_save();
    Serial.printf("[TOUCHCAL] luu: swap=%u invX=%u invY=%u X=%u..%u Y=%u..%u\n",
                  (unsigned)s_t_swap, (unsigned)s_t_invx, (unsigned)s_t_invy,
                  (unsigned)s_t_xmin, (unsigned)s_t_xmax, (unsigned)s_t_ymin, (unsigned)s_t_ymax);

    // ---- Man hinh ket qua (de doi chieu / chup lai) ----
    char b1[26], b2[26], b3[26], b4[26];
    snprintf(b1, sizeof(b1), "swap=%u invX=%u invY=%u", (unsigned)s_t_swap, (unsigned)s_t_invx, (unsigned)s_t_invy);
    snprintf(b2, sizeof(b2), "X %u..%u  Y %u..%u", (unsigned)s_t_xmin, (unsigned)s_t_xmax,
             (unsigned)s_t_ymin, (unsigned)s_t_ymax);
    snprintf(b3, sizeof(b3), "TL %u,%u  TR %u,%u", (unsigned)cx[0], (unsigned)cy[0], (unsigned)cx[1], (unsigned)cy[1]);
    snprintf(b4, sizeof(b4), "BR %u,%u  BL %u,%u", (unsigned)cx[2], (unsigned)cy[2], (unsigned)cx[3], (unsigned)cy[3]);
    s_lcd.fillScreen(TFT_BLACK);
    s_lcd.setTextDatum(MC_DATUM);
    s_lcd.setTextColor(TFT_GREEN, TFT_BLACK);
    s_lcd.setTextFont(4);
    s_lcd.drawString(T(UI_T_CAL_SAVED), UI_LCD_HOR_RES / 2, 34);
    s_lcd.setTextFont(2);
    s_lcd.setTextColor(TFT_WHITE, TFT_BLACK);
    s_lcd.drawString(b1, UI_LCD_HOR_RES / 2, 92);
    s_lcd.drawString(b2, UI_LCD_HOR_RES / 2, 118);
    s_lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    s_lcd.drawString(b3, UI_LCD_HOR_RES / 2, 152);
    s_lcd.drawString(b4, UI_LCD_HOR_RES / 2, 178);
    s_lcd.drawString(T(UI_T_CAL_CONTINUE), UI_LCD_HOR_RES / 2, 214);
    s_lcd.setTextDatum(TL_DATUM);
    delay(4000);
  }

  #endif  // CYDLCD_CYD_TOUCH_USE_TFTESPI

  void ui_port_display_init(void) {
    s_lcd.init();
    s_lcd.setRotation(1);                              // 1 = LANDSCAPE 320x240
    // DEN NEN: firmware tu bat (User_Setup.h dat TFT_BL -1 nen TFT_eSPI khong dung toi chan nay).
    // Xem CYDLCD_BL_PIN trong cydlcd_config.h: 21 = CYD (ST7789), 27 = nhieu ban ST7789 khac, -1 = den nen
    // da dau thang 3V3 (khong can dieu khien). Tach ra khoi TFT_eSPI de chan den nen co the dung
    // lam CHAN CHON MUX SPI the SD (luc do phai dat CYDLCD_BL_PIN -1 + dau den nen thang 3V3).
    #if CYDLCD_BL_PIN >= 0
      pinMode(CYDLCD_BL_PIN, OUTPUT);
      digitalWrite(CYDLCD_BL_PIN, CYDLCD_BL_ON_LEVEL);
    #endif
    s_lcd.fillScreen(TFT_BLACK);

    // ---------------------------------------------------------------------------
    // DAO MAU CUA PANEL (2026-10-16)
    // VI SAO CAN: TFT_eSPI 2.5.43 (TFT_Drivers/ST7789_Init.h:102) gui `ST7789_INVON` **VO DIEU KIEN** -
    // no KHONG doc `TFT_INVERSION_ON`/`TFT_INVERSION_OFF` trong User_Setup.h (nen dong
    // `#define TFT_INVERSION_ON` dang co chi la "cho vui", khong co tac dung voi driver ST7789).
    // Hau qua: panel nao KHONG can dao mau thi ca man hinh bi "am ban" (trang<->den nguoc, mau nguoc).
    // Cach tat that: goi `invertDisplay(false)` SAU init (gui INVOFF) - bat binh thuong chi can
    //     -DCYDLCD_CYD_INVERT_OFF=1
    // trong build_flags cua env (xem cydlcd/README.md muc "Kiem tra duong mau").
    // ---------------------------------------------------------------------------
    #if defined(CYDLCD_CYD_INVERT_OFF)
      s_lcd.invertDisplay(false);
    #endif
    #if defined(CYDLCD_CYD_DEBUG)
      // Bat lai bang -DCYDLCD_CYD_DEBUG=1 trong env cydlcd_esp32_cyd khi can chan doan:
      // muon biet den nen di chan nao thi doi CYDLCD_BL_PIN giua 21 / 27, va thu fillScreen(TFT_RED)
      // de tach loi "den nen" khoi loi "driver/SPI".
      Serial.printf("[CYD] TFT_eSPI init xong: width=%d height=%d | den nen: BL_PIN=%d | the SD: %s\n",
                    s_lcd.width(), s_lcd.height(), (int)CYDLCD_BL_PIN,
                    #if CYDLCD_SD_UART
                      "qua UART (cydlcd la may chu the)"
                    #elif CYDLCD_SD_SHARED
                      "dung chung qua MUX"
                    #else
                      "khong ro (kiem CYDLCD_SD_UART/CYDLCD_SD_SHARED)"
                    #endif
                    );
    #endif
  }


// ===========================================================================
//  KIEM TRA DUONG MAU (lenh `UI COLORTEST`) - xem src/ui_colortest.h
//
//  To TOAN MAN HINH 8 mau bang THANG driver (fillScreen), KHONG qua LVGL: nhu vay neu mau sai thi
//  chac chan sai o lop panel/TFT_eSPI (thu tu kenh `TFT_RGB_ORDER`, dao mau/inversion), khong phai o UI.
//  Sau khi xong: ep LVGL ve lai toan man (man hinh vua bi ghi de ngoai y LVGL).
// ===========================================================================
void ui_port_colortest_raw(void) {
  Serial.printf("[COLORTEST] BAT DAU: %u mau toan man hinh, moi mau 700 ms\n", (unsigned)UI_COLORTEST_N);
  Serial.println("[COLORTEST] Doi chieu: DO->xanh duong = sai thu tu kenh (TFT_RGB_ORDER TFT_BGR); "
                 "trang<->den nguoc = panel bi dao mau (bat -DCYDLCD_CYD_INVERT_OFF=1)");
  for (uint8_t i = 0; i < UI_COLORTEST_N; i++) {
    const uint16_t c = ui_colortest_rgb565(ui_colortest_bars[i].rgb888);
    s_lcd.fillScreen(c);
    Serial.printf("[COLORTEST] %u/%u: %-7s #%06lX -> RGB565 0x%04X (man hinh phai la mau nay)\n",
                  (unsigned)(i + 1), (unsigned)UI_COLORTEST_N, ui_colortest_bars[i].name,
                  (unsigned long)ui_colortest_bars[i].rgb888, (unsigned)c);
    delay(700);
  }
  s_lcd.fillScreen(ui_colortest_rgb565(ui_colortest_bars[5].rgb888));   // tra ve mau nen cua UI (BG)
  lv_obj_invalidate(lv_scr_act());     // man hinh bi ghi de ngoai LVGL -> bat LVGL ve lai
  Serial.println("[COLORTEST] xong (da tra ve man hinh binh thuong)");
}

// ===========================================================================
//  Flush callback cua LVGL (CYD: TFT_eSPI)
// ===========================================================================
// ---- DO HIEU NANG (xem ui_port_perf_report) ----
// Dem so lan flush + TONG DIEN TICH da day ra SPI: neu "px" lon (nhieu man hinh/giay) thi LVGL dang ve lai
// qua nhieu (moi vong lap ve lai ca man), con neu px nho ma van cham thi do SPI hoac do cho khac.
static uint32_t s_pf_flush_n = 0, s_pf_flush_px = 0, s_pf_flush_us = 0, s_pf_flush_max_us = 0;
static uint32_t s_pf_read_n = 0, s_pf_read_us = 0, s_pf_read_max_us = 0;

void ui_port_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *px) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  if (w > 0 && h > 0) {
    const uint32_t t0 = micros();
    // pushColors(..., swap = true): TFT_eSPI doi 2 byte -> dung endianness cua ST7789
    s_lcd.startWrite();
    s_lcd.setAddrWindow(area->x1, area->y1, w, h);
    s_lcd.pushColors(reinterpret_cast<uint16_t *>(px), (uint32_t)w * (uint32_t)h, true);
    s_lcd.endWrite();
    const uint32_t dt = micros() - t0;
    s_pf_flush_n++;
    s_pf_flush_px += (uint32_t)w * (uint32_t)h;
    s_pf_flush_us += dt;
    if (dt > s_pf_flush_max_us) s_pf_flush_max_us = dt;
  }
  lv_disp_flush_ready(disp_drv);
}

// In + XOA bo dem hieu nang cua phan cung (SPI + cam ung). `UI PERF` goi ham nay 2 lan cach nhau N giay:
// lan 1 de xoa, lan 2 de doc so cua khoang do.
void ui_port_perf_report(void) {
  const uint32_t px = s_pf_flush_px, n = s_pf_flush_n;
  Serial.printf("[PERF] SPI: %lu lan flush, %lu px (%lu%% cua 1 man) | tb %lu px/lan | %lu us tong, "
                "%lu us/lan, max %lu us\n",
                (unsigned long)n, (unsigned long)px, (unsigned long)(px * 100UL / (UI_LCD_HOR_RES * UI_LCD_VER_RES)),
                (unsigned long)(n ? px / n : 0), (unsigned long)s_pf_flush_us,
                (unsigned long)(n ? s_pf_flush_us / n : 0), (unsigned long)s_pf_flush_max_us);
  Serial.printf("[PERF] cam ung: %lu lan doc, %lu us tong, tb %lu us/lan, max %lu us\n",
                (unsigned long)s_pf_read_n, (unsigned long)s_pf_read_us,
                (unsigned long)(s_pf_read_n ? s_pf_read_us / s_pf_read_n : 0),
                (unsigned long)s_pf_read_max_us);
  s_pf_flush_n = s_pf_flush_px = s_pf_flush_us = s_pf_flush_max_us = 0;
  s_pf_read_n = s_pf_read_us = s_pf_read_max_us = 0;
}

// ===========================================================================
//  Input
// ===========================================================================
//  Input (cam ung XPT2046)
// ===========================================================================

// CYD khong co encoder -> cam ung
bool ui_port_input_is_encoder(void) { return false; }

// CYD co hieu chuan cam ung 4 goc (menu "Calibrate touch" hoac giu tay luc cap dien)
bool ui_port_touch_cal_available(void) { return true; }
bool ui_port_touch_cal_is_saved(void) { return s_t_from_nvs; }

#ifdef CYDLCD_CYD_TOUCH_USE_TFTESPI
// Neu dung tft.getTouch() cua TFT_eSPI (da patch) thi khong co gia tri raw -> khong hieu chuan
void ui_port_touch_calibrate(void) { }
#endif

void ui_port_input_init(void) {
#ifndef CYDLCD_CYD_TOUCH_USE_TFTESPI
  pinMode(CYDLCD_CYD_TOUCH_CS, OUTPUT);
  digitalWrite(CYDLCD_CYD_TOUCH_CS, HIGH);
  pinMode(CYDLCD_CYD_TOUCH_CLK, OUTPUT);
  digitalWrite(CYDLCD_CYD_TOUCH_CLK, LOW);
  pinMode(CYDLCD_CYD_TOUCH_DIN, OUTPUT);
  pinMode(CYDLCD_CYD_TOUCH_DO, INPUT);                  // GPIO39 chi doc
  pinMode(CYDLCD_CYD_TOUCH_IRQ, INPUT);                 // GPIO36 chi doc, co keo len ngoai
  // NGAT TOUCH_IRQ (thuc day task `ui` ngay khi cham): DA THU va PHAI BO - ttachInterrupt() keo them
  // ~488 B DRAM tinh cua core Arduino (bang chan ngat), ma dram0 cua ESP32 da KIN (linker bao tran ngay).
  // Chap nhan tre <= 33 ms (1 nhip task ui) - khong cam nhan duoc tren man 240x320. Neu sau nay co PSRAM/
  // giam LVGL heap thi gan lai 1 dong: attachInterrupt(digitalPinToInterrupt(CYDLCD_CYD_TOUCH_IRQ), ui_task_wake_isr, FALLING);
  touch_cal_load();                                    // nap hieu chuan da luu (neu co)
  delay(50);
  // GIU TAY tren man hinh luc cap dien -> vao thang che do hieu chuan.
  // Day la "thoat hiem": khi hieu chuan cu qua te thi khong the cham trung menu nua.
  if (digitalRead(CYDLCD_CYD_TOUCH_IRQ) == LOW) ui_port_touch_calibrate();
#endif
}

// Callback LVGL doc input (cam ung XPT2046)
void ui_port_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  (void)indev_drv;
  data->continue_reading = false;
  #if defined(CYDLCD_CYD_DEBUG)
    cal_print_1hz();                     // in lai 5 cum cham gan nhat (de hieu chuan cam ung)
  #endif
  lv_coord_t x = 0, y = 0;
  data->point.x = 0;
  data->point.y = 0;
  // Do thoi gian doc cam ung (bit-bang SPI mem): LVGL goi ham nay moi LV_INDEV_DEF_READ_PERIOD (20 ms).
  const uint32_t t0 = micros();
  const bool touched = cyd_touch_read(&x, &y);
  const uint32_t dt = micros() - t0;
  s_pf_read_n++; s_pf_read_us += dt;
  if (dt > s_pf_read_max_us) s_pf_read_max_us = dt;
  if (touched) {
    data->point.x = x;
    data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  }
  else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

#endif  // CYDLCD_USE_LVGL && CYDLCD_BOARD_CYD
