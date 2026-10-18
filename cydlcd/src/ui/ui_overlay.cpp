/**
 * ui_overlay.cpp -- Lop phu tien do upload + khoa input (xem ui_overlay.h de biet vi sao tach 2 nua).
 *
 * CHUYEN TU cyd_sd.cpp (2026-10-15, B3): truoc day lop phu nay tao/ve NGAY trong duong upload (task `net`
 * / vong lap chinh) => khi tach task `ui` thi se co 2 task cung goi LVGL (hong man hinh). Nay:
 *   - `ov_*` / `ui_lock()`: chi ghi trang thai (co khoa nho de chuoi khong bi doc nua chung);
 *   - `ui_overlay_update()`: task `ui` doc trang thai roi tao/cap nhat/huy widget.
 */
#include "ui_overlay.h"

#if CYDLCD_USE_LVGL

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <lvgl.h>

#include "ui_lvgl_int.h"        // UI_LCD_HOR_RES/VER_RES, ma mau MR_*

// Mau lop phu: lay DUNG gia tri cua ui_lvgl_int.h (MR_BG/MR_PROGBG/MR_PROGBAR/MR_FRAME/MR_RATE100)
#define OV_BG   0x1E156E
#define OV_BAR  0x2A2080
#define OV_FILL 0x00E5FF
#define OV_TXT  0xE6E1E5
#define OV_OK   0x81C784
#define OV_ERR  0xFF8A80

// ---- Trang thai chia se giua task `net` (ghi) va task `ui` (doc) ----
static volatile bool    s_vis  = false;      // lop phu dang BAT
static volatile bool    s_lock = false;      // input dang bi KHOA
// Chuoi trang thai: 32 byte la du ("100% 4418/4418KB 146.2KB/s" = 26; ten trang thai ngan nhat ~26).
// GIU NHO la co y: DRAM tinh (dram0) cua ESP32 da kin - them ~100 B la linker bao tran ngay.
static char             s_state[32] = "";
static char             s_info[32]  = "";
static volatile bool    s_ok   = true;
static volatile uint8_t s_pct  = 0;
static volatile uint32_t s_rate_ms = 0, s_rate_base = 0, s_rate_x10 = 0, s_ov_ms = 0;

#if defined(CYDLCD_BOARD_CYD)
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  static SemaphoreHandle_t s_ov_mtx = nullptr;
  #define OV_LOCK()   do { if (s_ov_mtx) xSemaphoreTake(s_ov_mtx, portMAX_DELAY); } while (0)
  #define OV_UNLOCK() do { if (s_ov_mtx) xSemaphoreGive(s_ov_mtx); } while (0)
#else
  #define OV_LOCK()   do { } while (0)
  #define OV_UNLOCK() do { } while (0)
#endif

// ---- Widget (chi task `ui` dung) ----
static lv_obj_t *ov = nullptr, *ov_state = nullptr, *ov_info = nullptr, *ov_bar = nullptr;

// ===========================================================================
//  Nua 1: ghi trang thai (goi duoc tu moi task - KHONG goi LVGL)
// ===========================================================================
void ui_lock(const bool lock) {
  s_lock = lock;
}

bool ov_create_full(void) {
  OV_LOCK();
  s_vis = true;
  s_pct = 0;
  s_ok  = true;
  if (!s_state[0]) snprintf(s_state, sizeof(s_state), "...");
  OV_UNLOCK();
  return true;
}

// 2026-10-17: hien lop phu TIEN DO UPLOAD o 40% de chup anh kiem tra.
void ov_test_show(void) {
  ov_progress("DANG GUI FILE", 40, 100);
}

void ov_destroy(void) {
  OV_LOCK();
  s_vis = false;
  s_state[0] = '\0';
  s_info[0]  = '\0';
  OV_UNLOCK();
}

void ov_set(const char *state, const char *info, const bool ok_col) {
  OV_LOCK();
  if (state) snprintf(s_state, sizeof(s_state), "%s", state);
  if (info)  snprintf(s_info,  sizeof(s_info),  "%s", info);
  s_ok = ok_col;
  OV_UNLOCK();
}

void ov_note(const char *state, const bool ok) {
  OV_LOCK();
  if (state) snprintf(s_state, sizeof(s_state), "%s", state);
  s_info[0] = '\0';
  s_ok = ok;
  OV_UNLOCK();
}

void rate_tick(const uint32_t done) {
  const uint32_t now = millis();
  if (s_rate_ms == 0) { s_rate_ms = now; s_rate_base = done; return; }
  const uint32_t dt = now - s_rate_ms;
  if (dt < 1000u) return;
  s_rate_x10 = (uint32_t)(((uint64_t)(done - s_rate_base) * 10000ull) / ((uint64_t)dt * 1024ull));
  s_rate_ms = now;
  s_rate_base = done;
}

// Cap nhat tien do (toi da 5 lan/s cho mat nguoi, khong nong CPU)
void ov_progress(const char *state, const uint32_t done, const uint32_t total) {
  if (millis() - s_ov_ms < 200u) return;
  s_ov_ms = millis();
  char info[48];
  snprintf(info, sizeof(info), "%u%%   %u/%u KB   %u.%u KB/s",
           (unsigned)(total ? ((uint64_t)done * 100u / total) : 0u),
           (unsigned)(done / 1024u), (unsigned)(total / 1024u),
           (unsigned)(s_rate_x10 / 10u), (unsigned)(s_rate_x10 % 10u));
  OV_LOCK();
  snprintf(s_state, sizeof(s_state), "%s", state ? state : "");
  snprintf(s_info,  sizeof(s_info),  "%s", info);
  s_ok  = true;
  s_pct = (uint8_t)(total ? ((uint64_t)done * 100u / total) : 0u);
  OV_UNLOCK();
}

// ===========================================================================
//  Nua 2: CHI task `ui` goi - tao/cap nhat/huy widget LVGL
// ===========================================================================
static lv_obj_t *ov_label(lv_obj_t *parent, const char *txt, const uint32_t col, const lv_font_t *font) {
  lv_obj_t *l = lv_label_create(parent);
  if (!l) return nullptr;
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  if (font) lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

static void ov_layout_full(void) {
  if (!ov) return;
  const lv_coord_t W = UI_LCD_HOR_RES, H = UI_LCD_VER_RES;
  lv_obj_set_pos(ov, 0, 0);
  lv_obj_set_size(ov, W, H);
  lv_obj_set_style_radius(ov, 0, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_90, 0);
  if (ov_state) { lv_obj_set_width(ov_state, (lv_coord_t)(W - 16)); lv_obj_align(ov_state, LV_ALIGN_TOP_MID, 0, 54); }
  if (ov_info)  { lv_obj_set_width(ov_info,  (lv_coord_t)(W - 16)); lv_obj_align(ov_info,  LV_ALIGN_TOP_MID, 0, 96); }
  if (ov_bar)   { lv_obj_set_size(ov_bar, (lv_coord_t)(W - 32), 16); lv_obj_align(ov_bar,  LV_ALIGN_TOP_MID, 0, 128); }
  lv_obj_invalidate(ov);
}

static bool ov_build(void) {
  lv_obj_t *top = lv_layer_top();
  if (!top) return false;
  if (ov) { ov_layout_full(); return true; }
  ov = lv_obj_create(top);
  if (!ov) { return false; }                     // het LVGL mem -> lan sau thu lai
  lv_obj_set_style_radius(ov, 0, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_set_style_bg_color(ov, lv_color_hex(OV_BG), 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_90, 0);
  lv_obj_set_style_pad_all(ov, 0, 0);
  lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ov, LV_OBJ_FLAG_CLICKABLE);  // input bi chan bang ui_lock(), khong can click
  lv_obj_t *title = ov_label(ov, "TBT-CYDLCD", OV_FILL, nullptr);
  if (title) {
    lv_obj_set_width(title, (lv_coord_t)(UI_LCD_HOR_RES - 16));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);
  }
  // Font cho DONG TRANG THAI cua lop phu: 2026-10-16 (yeu cau nguoi dung "thong nhat chi dung 1
  // loai font hien thi chu") dung CHUNG font chu cua UI thay vi Montserrat 20/16 rieng.
  // UI_MONO_FACE tu lui ve Montserrat 14 khi khong co font GFX (cac board STM32) nen van an toan.
  #define OV_FONT_BIG UI_MONO_FACE
  ov_state = ov_label(ov, "...", OV_TXT, OV_FONT_BIG);
  ov_info  = ov_label(ov, "", OV_TXT, nullptr);
  ov_bar = lv_bar_create(ov);
  if (ov_bar) {
    lv_bar_set_range(ov_bar, 0, 100);
    lv_bar_set_value(ov_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(ov_bar, lv_color_hex(OV_BAR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ov_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ov_bar, lv_color_hex(OV_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ov_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ov_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(ov_bar, 2, LV_PART_INDICATOR);
  }
  ov_layout_full();
  return true;
}

// ---- Trang thai da AP DUNG (task `ui`) ----
static bool s_app_lock = false;
static bool s_app_vis  = false;

void ui_overlay_update(void) {
#if defined(CYDLCD_BOARD_CYD)
  if (!s_ov_mtx) s_ov_mtx = xSemaphoreCreateMutex();
#endif
  // 1) Khoa/mo input (chi lam khi trang thai DOI)
  const bool want_lock = s_lock;
  if (want_lock != s_app_lock) {
    s_app_lock = want_lock;
    lv_indev_t *i = lv_indev_get_next(nullptr);
    while (i) { lv_indev_enable(i, !want_lock); i = lv_indev_get_next(i); }
  }
  // 2) Lop phu
  const bool want_vis = s_vis;
  if (want_vis) {
    if (!ov_build()) return;
    s_app_vis = true;
    // Lay ban sao chuoi (co khoa) roi moi ghi vao nhan. DUNG `set_text()` (no COPY vao LVGL heap VA tu bo qua
    // khi chuoi khong doi) nen KHONG can buffer phia UI - tiet kiem DRAM (dram0 cua ESP32 da kin).
    // Tren STACK cua task `ui` (khong ton DRAM tinh - dram0 cua ESP32 da kin; `set_text()` copy ngay nen
    // khong can chuoi song lau).
    char b_state[32], b_info[32];
    OV_LOCK();
    snprintf(b_state, sizeof(b_state), "%s", s_state);
    snprintf(b_info,  sizeof(b_info),  "%s", s_info);
    const bool    ok  = s_ok;
    const uint8_t pct = s_pct;
    OV_UNLOCK();
    if (ov_state) {
      lv_obj_set_style_text_color(ov_state, lv_color_hex(ok ? OV_OK : OV_ERR), 0);
      set_text(ov_state, b_state);
    }
    if (ov_info) set_text(ov_info, b_info);
    if (ov_bar && lv_bar_get_value(ov_bar) != (int32_t)pct) lv_bar_set_value(ov_bar, (int32_t)pct, LV_ANIM_OFF);
  }
  else if (s_app_vis || ov) {
    s_app_vis = false;
    if (ov) lv_obj_del(ov);
    ov = nullptr; ov_state = ov_info = ov_bar = nullptr;
  }
}

#endif  // CYDLCD_USE_LVGL
