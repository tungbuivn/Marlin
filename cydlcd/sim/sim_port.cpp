/**
 * sim_port.cpp -- hien thuc HOP DONG `ui_port_*` (khai bao o src/ui_lvgl.h) cho ban GIA LAP tren PC.
 *
 * Thay cho 2 file port that:
 *   src/ui_port_cyd_st7789.cpp     (board CYD: TFT_eSPI + XPT2046)
 *   src/ui_port_stm32_ili9341.cpp  (board STM32: tft_ili9341 + encoder)
 * => ban gia lap KHONG build 2 file do (xem tools/run_sim.ps1: danh sach file nguon co dinh).
 *
 * Man hinh gia = 1 mang framebuffer RGB565 320x240 trong RAM:
 *   - `ui_port_flush_cb()` copy vung LVGL vua ve vao framebuffer (khong SPI, khong DMA);
 *   - `sim_dump_bmp()` ghi framebuffer ra file .bmp 24-bit de mo bang trinh xem anh tren PC.
 * Cam ung gia = hang doi "cham" (`sim_tap`): moi lan cham duoc GIU (PRESS) vai vong doc indev roi
 * THA (RELEASE), dung nhu nguoi dung bam -> di dung duong su kien that cua LVGL.
 *
 * Dong ho AO: `sim_millis()` - tang trong `sim_advance()`. Nho vay kich ban chay NHANH hon thuc te
 * (khong phai cho that) ma LVGL van thay thoi gian troi dung nhu tren board.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <Arduino.h>            // shim cua ban gia lap (sim/shim/Arduino.h): SimSerial + millis/delay

#include "lvgl.h"
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"        // UI_LCD_HOR_RES/VER_RES
#include "cydlcd_config.h"
#include "sim.h"

#if CYDLCD_USE_LVGL

// ===========================================================================
//  Framebuffer + dong ho ao
// ===========================================================================
#define SIM_W UI_LCD_HOR_RES
#define SIM_H UI_LCD_VER_RES

static uint16_t s_fb[SIM_W * SIM_H];
static uint32_t s_ms = 0;
static uint32_t s_flush_n = 0, s_flush_px = 0;

uint32_t sim_millis(void) { return s_ms; }

// ---- Serial gia + thoi gian Arduino (xem shim/Arduino.h) ----
SimSerial Serial;
uint32_t millis(void) { return s_ms; }
uint32_t micros(void) { return s_ms * 1000u; }
void     delay(uint32_t ms) { sim_advance(ms); }
void     delayMicroseconds(uint32_t us) { (void)us; }
void     yield(void) { }

// ===========================================================================
//  HOP DONG PORT (ui_lvgl.h)
// ===========================================================================
uint32_t ui_port_millis(void) { return s_ms; }

// Board gia lap: khong co man hinh that -> khong init gi. (Viec dang ky `lv_disp_drv_t` nam trong
// ui_lvgl_begin() cua src/ui_lvgl.cpp, khong phai o port.)
void ui_port_display_init(void) { }

// Gia lap cam ung (LV_INDEV_TYPE_POINTER) -> `ui_port_input_is_encoder()` = false.
void ui_port_input_init(void) { }
bool ui_port_input_is_encoder(void) { return false; }

// Khong co NVS/phan cung de hieu chuan -> bao "khong ho tro" (man MENU se an dong "Calibrate touch").
void ui_port_touch_calibrate(void) { }
bool ui_port_touch_cal_available(void) { return false; }
bool ui_port_touch_cal_is_saved(void) { return false; }

// Copy vung vua ve vao framebuffer. LV_COLOR_DEPTH = 16 (xem src/lv_conf.h) nen moi pixel la RGB565.
void ui_port_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *px) {
  const int32_t w = area->x2 - area->x1 + 1;
  for (int32_t y = area->y1; y <= area->y2; y++) {
    if (y < 0 || y >= SIM_H) { px += w; continue; }
    for (int32_t x = area->x1; x <= area->x2; x++) {
      if (x >= 0 && x < SIM_W) s_fb[y * SIM_W + x] = px->full;
      px++;
    }
  }
  s_flush_n++;
  s_flush_px += (uint32_t)(w * (area->y2 - area->y1 + 1));
  lv_disp_flush_ready(disp_drv);
}

// ---- cam ung gia: hang doi cham ----
#define SIM_TAP_MAX 32
static int16_t s_tap[SIM_TAP_MAX][2];
static int     s_tap_head = 0, s_tap_tail = 0;   // [head..tail) = cac lan cham dang cho
static int     s_tap_phase = 0;                  // 0 = dang THA, >0 = dem nguoc so vong GIU
#define SIM_TAP_HOLD 5                           // so vong doc giu tay (~50 ms voi buoc 10 ms)

void sim_tap(int x, int y) {
  const int nxt = (s_tap_tail + 1) % SIM_TAP_MAX;
  if (nxt == s_tap_head) return;                 // hang doi day -> bo (kich ban khong bao gio day)
  s_tap[s_tap_tail][0] = (int16_t)x;
  s_tap[s_tap_tail][1] = (int16_t)y;
  s_tap_tail = nxt;
}

int sim_tap_pending(void) { return (s_tap_tail - s_tap_head + SIM_TAP_MAX) % SIM_TAP_MAX; }

void ui_port_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data) {
  (void)indev_drv;
  static int16_t last_x = SIM_W / 2, last_y = SIM_H / 2;
  if (s_tap_phase > 0) {                          // dang GIU
    s_tap_phase--;
  }
  else if (s_tap_head != s_tap_tail) {            // lay lan cham ke tiep
    last_x = s_tap[s_tap_head][0];
    last_y = s_tap[s_tap_head][1];
    s_tap_head = (s_tap_head + 1) % SIM_TAP_MAX;
    s_tap_phase = SIM_TAP_HOLD;
  }
  data->point.x = last_x;
  data->point.y = last_y;
  data->state = (s_tap_phase > 0) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void ui_port_perf_report(void) {
  printf("[SIM] flush: %lu lan, %lu pixel (khong co SPI that)\n",
         (unsigned long)s_flush_n, (unsigned long)s_flush_px);
  s_flush_n = s_flush_px = 0;
}

// ===========================================================================
//  Vong lap thoi gian ao
// ===========================================================================
extern "C" void sim_step(uint32_t dt_ms);

void sim_advance(uint32_t ms) {
  // Buoc 10 ms: LVGL doc indev moi 30 ms (LV_INDEV_DEF_READ_PERIOD) -> 1 lan cham duoc thay it nhat
  // 3 lan doc (du de sinh PRESSED -> RELEASED nhu bam that).
  const uint32_t step = 10;
  uint32_t left = ms;
  while (left) {
    const uint32_t d = (left < step) ? left : step;
    s_ms += d;
    lv_tick_inc(d);
    ui_lvgl_task();
    left -= d;
  }
}

void sim_settle(uint32_t ms) { sim_advance(ms); }

// ===========================================================================
//  Framebuffer -> BMP 24-bit (khong can thu vien anh nao)
// ===========================================================================
void sim_fb_clear(uint16_t rgb565) {
  for (int i = 0; i < SIM_W * SIM_H; i++) s_fb[i] = rgb565;
}

// ===========================================================================
//  PHAN TICH ANH (kiem tra tu dong, khong can nhin)
//
// VI SAO CAN: loi nguoi dung bao "o truc X = 285.98 thi so 2 DE len nhan X" la loi BO CUC - chi nhin
// moi thay. Ban gia lap khong the "nhin" thay nguoi, nen o day doc chinh framebuffer:
//   - dem so pixel "co muc" (khac mau nen) -> biet trang co ve gi khong (trang TRANG = loi);
//   - tach dong chu thanh cac "run" theo cot -> do KHOANG TRONG giua nhan truc va so.
// Neu nhan va so chong nhau thi 2 run DINH LIEN (hoac chi con 1 run) -> bao FAIL.
// ===========================================================================
// So pixel khac mau nen trong 1 vung (mau nen lay o goc duoi-trai - vung chac chan khong co chu).
int sim_ink_count(int x0, int y0, int x1, int y1, uint16_t bg) {
  int n = 0;
  for (int y = y0; y <= y1 && y < SIM_H; y++)
    for (int x = x0; x <= x1 && x < SIM_W; x++)
      if (s_fb[y * SIM_W + x] != bg) n++;
  return n;
}

// Mau nen cua man hinh: lay mau o (2, SIM_H-2) (goc duoi-trai, khong co vat nao).
uint16_t sim_bg_color(void) { return s_fb[(SIM_H - 2) * SIM_W + 2]; }

// Tach cac "run" cot co muc trong dai y0..y1, x0..x1. Tra ve so run (toi da max_runs).
// `min_gap`: so cot TRONG toi thieu de coi la 2 run khac nhau (nho hon thi gop lai).
int sim_ink_runs(int x0, int y0, int x1, int y1, int min_gap, int *starts, int *ends, int max_runs) {
  const uint16_t bg = sim_bg_color();
  int runs = 0, rs = -1, gap = 0;
  for (int x = x0; x <= x1 && x < SIM_W; x++) {
    bool ink = false;
    for (int y = y0; y <= y1 && y < SIM_H && !ink; y++)
      if (s_fb[y * SIM_W + x] != bg) ink = true;
    if (ink) {
      if (rs < 0) rs = x;
      gap = 0;
    }
    else if (rs >= 0) {
      if (++gap >= min_gap) {
        if (runs < max_runs) { starts[runs] = rs; ends[runs] = x - gap; }
        runs++;
        rs = -1; gap = 0;
      }
    }
  }
  if (rs >= 0) { if (runs < max_runs) { starts[runs] = rs; ends[runs] = x1; } runs++; }
  return runs;
}

bool sim_dump_bmp(const char *path) {  FILE *f = fopen(path, "wb");
  if (!f) { printf("[SIM] KHONG mo duoc file anh '%s'\n", path); return false; }

  const int row_raw = SIM_W * 3;
  const int pad = (4 - (row_raw % 4)) % 4;         // BMP: moi hang phai chia het cho 4 byte
  const int row = row_raw + pad;
  const uint32_t img = (uint32_t)row * SIM_H;
  const uint32_t file_size = 54 + img;

  uint8_t hdr[54];
  memset(hdr, 0, sizeof(hdr));
  hdr[0] = 'B'; hdr[1] = 'M';
  memcpy(&hdr[2], &file_size, 4);
  const uint32_t off = 54;  memcpy(&hdr[10], &off, 4);
  const uint32_t hdr_size = 40; memcpy(&hdr[14], &hdr_size, 4);
  const int32_t  w = SIM_W, h = SIM_H;
  memcpy(&hdr[18], &w, 4);
  memcpy(&hdr[22], &h, 4);
  const uint16_t planes = 1, bpp = 24;
  memcpy(&hdr[26], &planes, 2);
  memcpy(&hdr[28], &bpp, 2);
  memcpy(&hdr[34], &img, 4);
  fwrite(hdr, 1, sizeof(hdr), f);

  uint8_t *line = (uint8_t *)malloc((size_t)row);
  if (!line) { fclose(f); return false; }
  for (int y = SIM_H - 1; y >= 0; y--) {           // BMP luu tu duoi len
    for (int x = 0; x < SIM_W; x++) {
      const uint16_t p = s_fb[y * SIM_W + x];
      const uint8_t r5 = (p >> 11) & 0x1F, g6 = (p >> 5) & 0x3F, b5 = p & 0x1F;
      line[x * 3 + 0] = (uint8_t)((b5 << 3) | (b5 >> 2));
      line[x * 3 + 1] = (uint8_t)((g6 << 2) | (g6 >> 4));
      line[x * 3 + 2] = (uint8_t)((r5 << 3) | (r5 >> 2));
    }
    memset(line + row_raw, 0, (size_t)pad);
    fwrite(line, 1, (size_t)row, f);
  }
  free(line);
  fclose(f);
  return true;
}

#endif  // CYDLCD_USE_LVGL
