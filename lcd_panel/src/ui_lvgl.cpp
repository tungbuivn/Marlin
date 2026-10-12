/**
 * ui_lvgl.cpp -- UI LVGL 8.3 cho panel (du lieu lay tu frame PUSH cua Marlin, xem panel_link.h).
 *
 * Kien truc:
 *   - 1 screen duy nhat. Thanh TREN: ten man + trang thai lien ket + nut BACK.
 *     Thanh DUOI: 8 nut chuyen man (ST/TP/JG/SD/CF/MH/SV/IN) -- day cung la cach
 *     "BACK" hoat dong voi cam ung.
 *   - Moi man la 1 container (lv_obj) TAO 1 LAN khi man duoc mo lan dau, sau do chi
 *     an/hien (LV_OBJ_FLAG_HIDDEN) va cap nhat text. Khong tao lai widget moi vong lap.
 *   - 1 lv_group chung cho encoder: LVGL 8.3 tu bo qua vat bi HIDDEN khi di chuyen focus
 *     (lv_group.c: "Hidden objects don't receive focus"), nen khong can group rieng cho
 *     tung man.
 *   - Nut bam: xoa co LV_OBJ_FLAG_SCROLLABLE de encoder BAM 1 LAN la an ngay
 *     (lv_indev.c: vat "editable or scrollable" thi lan bam dau chi vao che do edit).
 *
 * Cac man:
 *   STATUS  : nhiet do nozzle/bed (cur/target), vi tri XYZ, % + thanh tien do + ten file,
 *             trang thai may, co bao nhieu co (HOMED/SD/MESH/RUNOUT/MOTOR), FAN, LINK.
 *   TEMP    : +/- 1/10 do C cho nozzle & bed -> M104 S... / M140 S..., nut COOLDOWN.
 *   JOG     : chon truc X/Y/Z, nut -0.1/+0.1/-1/+1/-10/+10 mm -> G91 + G1 <truc><d> F600 + G90,
 *             nut HOME ALL -> G28.
 *   FILES   : lv_roller danh sach panel.files[], PRINT (M23 + M24), DIR, UP, REFRESH,
 *             PAUSE (M25), RESUME (M24), STOP (M524).
 *   SETTINGS: bang thong so panel_setting(PANEL_SET_*) + SAVE EEPROM (PANEL_REQ_SAVE_SETTINGS).
 *   MESH    : bang luoi bed mesh (panel.mesh[col][row]), o cao = xanh, o thap = do,
 *             REFRESH -> PANEL_REQ_MESH; khong co PANEL_HAS_MESH thi bao "khong ho tro".
 *   CUSTOM  : 2 nut nhu menu cu cua Marlin: G34 Bed (G34 Q99) + Init SD (M22 roi M21).
 *   INFO    : ten firmware, proto, marlin_fw, so frame/loi, phien ban UI.
 *
 * Prompt cua Marlin (panel.prompt_active) -> lv_msgbox OK/CANCEL -> panel_link_answer().
 * Thong bao (panel.message) -> lv_msgbox theo panel.message_sev, tu dong dong sau 6s.
 *
 * LUU Y: chu tren man hinh phai la ASCII (font montserrat_14 khong co dau tieng Viet).
 * Khong dung %f khi format so: fmt_scaled() in so nguyen da nhan 10/100/1000.
 */
#include "ui_lvgl.h"

#if PANEL_USE_LVGL

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "panel_link.h"

// Port day pixel ra LCD lam viec truc tiep tren buffer 16-bit (xem ui_lvgl_port.cpp)
static_assert(LV_COLOR_DEPTH == 16, "UI nay chi ho tro LV_COLOR_DEPTH 16");
static_assert(sizeof(lv_color_t) == 2, "lv_color_t phai la 2 byte");

// ===========================================================================
//  Mau
// ===========================================================================
#define COL_BG     lv_color_hex(0x0D1117)
#define COL_BAR    lv_color_hex(0x1B2430)
#define COL_TXT    lv_color_hex(0xE8EEF5)
#define COL_DIM    lv_color_hex(0x8C9AA8)
#define COL_BTN    lv_color_hex(0x2A3644)
#define COL_ACCENT lv_color_hex(0x25C2A0)
#define COL_WARN   lv_color_hex(0xE0A030)
#define COL_ERR    lv_color_hex(0xE05B5B)
#define COL_OK     lv_color_hex(0x46C46E)

// ===========================================================================
//  Man hinh
// ===========================================================================
enum {
  PG_STATUS = 0, PG_TEMP, PG_JOG, PG_FILES, PG_SETTINGS, PG_MESH, PG_CUSTOM, PG_INFO, PG_COUNT
};

static const char *const PG_NAV[PG_COUNT]   = {"ST", "TP", "JG", "SD", "CF", "MH", "SV", "IN"};
static const char *const PG_TITLE[PG_COUNT] = {"STATUS", "NHIET DO", "JOG", "FILE SD",
                                               "THONG SO", "BED MESH", "CUSTOM", "INFO"};

// Hinh hoc man hinh 240x320
#define BAR_H   26
#define NAV_H   28
#define NAV_Y   (UI_LCD_VER_RES - NAV_H)
#define CONT_Y  (BAR_H + 4)
#define CONT_H  (NAV_Y - CONT_Y)
#define PAD_X   6

static lv_obj_t  *s_page[PG_COUNT];        // container noi dung tung man (NULL = chua tao)
static lv_obj_t  *s_nav[PG_COUNT];         // nut chuyen man (thanh duoi)
static lv_obj_t  *s_title, *s_link;
static lv_group_t *s_group;
static lv_obj_t  *s_focus_prev;            // doi tuong dang focus (de ve khung)
static uint8_t    s_page_act = PG_STATUS;
static bool       s_link_state = false;
static uint32_t   s_tick_ms, s_upd_ms, s_slow_ms;

// ===========================================================================
//  Format so/chuoi (khong dung %f)
// ===========================================================================
// In `raw` (da nhan `scale`) thanh chuoi thap phan `dec` chu so sau dau cham.
//   vi du fmt_scaled(b, n, 21050, 100, 1) -> "210.5"
//           fmt_scaled(b, n, -120,  1000, 2) -> "-0.12"
static void fmt_scaled(char *b, size_t n, int32_t raw, uint32_t scale, uint8_t dec) {
  if (!scale) scale = 1;
  if (dec > 3) dec = 3;
  const bool neg = (raw < 0);
  const uint32_t v = neg ? (uint32_t)(-(raw + 1)) + 1U : (uint32_t)raw;   // tranh tran INT32_MIN
  uint32_t ip = v / scale;
  const uint32_t rem = v % scale;

  uint32_t mul = 1;
  for (uint8_t i = 0; i < dec; i++) mul *= 10U;
  uint32_t frac = (rem * mul + scale / 2U) / scale;                       // co lam tron
  if (frac >= mul) { frac -= mul; ip++; }

  switch (dec) {
    case 0:  snprintf(b, n, "%s%lu", neg ? "-" : "", (unsigned long)ip); break;
    case 1:  snprintf(b, n, "%s%lu.%01lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
    case 2:  snprintf(b, n, "%s%lu.%02lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
    default: snprintf(b, n, "%s%lu.%03lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
  }
}

// 12:34 hoac 1:02:03
static void fmt_time(char *b, size_t n, uint32_t s) {
  if (s >= 3600U)
    snprintf(b, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600U),
             (unsigned long)((s / 60U) % 60U), (unsigned long)(s % 60U));
  else
    snprintf(b, n, "%02lu:%02lu", (unsigned long)(s / 60U), (unsigned long)(s % 60U));
}

static void append_str(char *dst, size_t n, const char *s) {
  const size_t l = strlen(dst);
  if (l + 1U < n) snprintf(dst + l, n - l, "%s", s);
}

static const char *state_name(uint8_t st) {
  switch (st) {
    case PANEL_ST_IDLE:     return "IDLE";
    case PANEL_ST_PRINTING: return "PRINTING";
    case PANEL_ST_PAUSED:   return "PAUSED";
    case PANEL_ST_HOMING:   return "HOMING";
    case PANEL_ST_LEVELING: return "LEVELING";
    case PANEL_ST_RUNOUT:   return "RUNOUT";
    case PANEL_ST_KILLED:   return "KILLED";
    case PANEL_ST_BUSY:     return "BUSY";
    default:                return "?";
  }
}

static void temp_lookup(const uint8_t id, int16_t *cur, int16_t *tgt) {
  *cur = 0; *tgt = 0;
  for (uint8_t i = 0; i < panel.n_temps; i++)
    if (panel.temps[i].id == id) { *cur = panel.temps[i].cur; *tgt = panel.temps[i].tgt; return; }
}

// ===========================================================================
//  Helper tao widget
// ===========================================================================
// Chi set text khi khac noi dung cu -> khong nhap nhay, khong ve lai vo ich
static void set_text(lv_obj_t *lbl, const char *txt) {
  if (!lbl || !txt) return;
  const char *old = lv_label_get_text(lbl);
  if (old && strcmp(old, txt) == 0) return;
  lv_label_set_text(lbl, txt);
}

static void style_flat(lv_obj_t *o, lv_color_t bg) {
  lv_obj_set_style_bg_color(o, bg, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, lv_color_t col,
                          lv_coord_t x, lv_coord_t y) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_color(l, col, 0);
  lv_label_set_text(l, txt);
  lv_obj_set_pos(l, x, y);
  return l;
}

static void focus_mark(lv_obj_t *o, const bool on) {
  if (!o) return;
  lv_obj_set_style_border_width(o, on ? 2 : 0, 0);
  lv_obj_set_style_border_color(o, COL_ACCENT, 0);
}

// Ve khung cho vat dang focus (encoder hoac cham) -> nguoi dung biet dang o dau
static void on_focus_changed(lv_group_t *g) {
  lv_obj_t *f = lv_group_get_focused(g);
  if (s_focus_prev && s_focus_prev != f) focus_mark(s_focus_prev, false);
  if (f) focus_mark(f, true);
  s_focus_prev = f;
}

// Nut bam: co dinh kich thuoc, xoa SCROLLABLE (de encoder bam 1 lan la chay),
// tu dong vao group va tu canh chu vao giua.
static lv_obj_t *mk_btn(lv_obj_t *parent, const char *txt, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_set_style_bg_color(b, COL_BTN, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t *l = lv_label_create(b);
  lv_obj_set_style_text_color(l, COL_TXT, 0);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  if (s_group) lv_group_add_obj(s_group, b);
  return b;
}

// Container noi dung cua 1 man: nam duoi thanh tren, khong cuon
static lv_obj_t *mk_page(lv_obj_t *parent) {
  lv_obj_t *p = lv_obj_create(parent);
  lv_obj_set_pos(p, 0, CONT_Y);
  lv_obj_set_size(p, UI_LCD_HOR_RES, CONT_H);
  lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_radius(p, 0, 0);
  lv_obj_set_style_pad_all(p, 0, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  return p;
}

// ===========================================================================
//  Hanh dong (G-code / request)
// ===========================================================================
enum {
  A_NONE = 0,
  A_BACK, A_PAUSE, A_RESUME, A_STOP, A_SAVE, A_INITSD, A_G34, A_HOME,
  A_REQ_FILES, A_REQ_UP, A_REQ_MESH, A_REQ_SETTINGS, A_REQ_FULL, A_COOLDOWN
};

// user_data cua nut: (heater << 8) | (int8)delta
#define TEMP_ADJ(h, d) ((void *)(intptr_t)(((unsigned)(h) << 8) | (unsigned)(uint8_t)(int8_t)(d)))

static void show_page(uint8_t p);            // forward
static void create_page(uint8_t p);          // forward
static void files_rebuild(void);             // forward

static void cb_action(lv_event_t *e) {
  const int a = (int)(intptr_t)lv_event_get_user_data(e);
  switch (a) {
    case A_BACK:          show_page(PG_STATUS); break;
    case A_HOME:          panel_link_send_gcode("G28"); break;
    case A_PAUSE:         panel_link_send_gcode("M25"); break;
    case A_RESUME:        panel_link_send_gcode("M24"); break;
    case A_STOP:          panel_link_send_gcode("M524"); break;
    case A_COOLDOWN:      panel_link_send_gcode("M104 S0"); panel_link_send_gcode("M140 S0"); break;
    case A_SAVE:          panel_link_request((uint8_t)PANEL_REQ_SAVE_SETTINGS); break;
    case A_INITSD:        panel_link_send_gcode2("M22", "M21"); break;   // nha the SD roi init lai
    case A_G34:           panel_link_send_gcode("G34 Q99"); break;
    case A_REQ_FILES:     panel_link_request((uint8_t)PANEL_REQ_FILES); break;
    case A_REQ_UP:        panel_link_request((uint8_t)PANEL_REQ_FILES_UP); break;
    case A_REQ_MESH:      panel_link_request((uint8_t)PANEL_REQ_MESH); break;
    case A_REQ_SETTINGS:  panel_link_request((uint8_t)PANEL_REQ_SETTINGS); break;
    case A_REQ_FULL:      panel_link_request((uint8_t)PANEL_REQ_FULL); break;
    default: break;
  }
}

// ===========================================================================
//  STATUS
// ===========================================================================
static lv_obj_t *st_noz, *st_bed, *st_xy, *st_ze, *st_file, *st_pct;
static lv_obj_t *st_bar, *st_state, *st_flags, *st_fan;

static void page_status_create(lv_obj_t *p) {
  st_noz   = mk_label(p, "NOZZLE  --.- / --.- C", COL_TXT, PAD_X, 4);
  st_bed   = mk_label(p, "BED     --.- / --.- C", COL_TXT, PAD_X, 24);
  st_xy    = mk_label(p, "X -----  Y -----", COL_DIM, PAD_X, 50);
  st_ze    = mk_label(p, "Z -----  E -----", COL_DIM, PAD_X, 68);
  st_state = mk_label(p, "STATE: -", COL_ACCENT, PAD_X, 92);
  st_flags = mk_label(p, "-", COL_DIM, PAD_X, 110);
  lv_obj_set_width(st_flags, UI_LCD_HOR_RES - 2 * PAD_X);
  lv_label_set_long_mode(st_flags, LV_LABEL_LONG_SCROLL_CIRCULAR);

  st_file = mk_label(p, "FILE: -", COL_TXT, PAD_X, 136);
  lv_obj_set_width(st_file, UI_LCD_HOR_RES - 2 * PAD_X);
  lv_label_set_long_mode(st_file, LV_LABEL_LONG_SCROLL_CIRCULAR);

  st_bar = lv_bar_create(p);
  lv_obj_set_pos(st_bar, PAD_X, 160);
  lv_obj_set_size(st_bar, UI_LCD_HOR_RES - 2 * PAD_X, 16);
  lv_bar_set_range(st_bar, 0, 100);
  lv_bar_set_value(st_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(st_bar, COL_BTN, 0);
  lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(st_bar, 3, 0);
  lv_obj_set_style_bg_color(st_bar, COL_ACCENT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(st_bar, 3, LV_PART_INDICATOR);

  st_pct = mk_label(p, "0%  --:-- / --:--", COL_TXT, PAD_X, 180);
  st_fan = mk_label(p, "FAN -", COL_DIM, PAD_X, 200);
  mk_label(p, "xoay = chon   bam = OK", COL_DIM, PAD_X, 228);
}

static void page_status_update(void) {
  char b[48], a1[12], a2[12], t1[10], t2[10];
  int16_t cur = 0, tgt = 0;

  temp_lookup(PANEL_TEMP_E0, &cur, &tgt);
  fmt_scaled(a1, sizeof(a1), cur, 100, 1);
  fmt_scaled(a2, sizeof(a2), tgt, 100, 1);
  snprintf(b, sizeof(b), "NOZZLE  %s / %s C", a1, a2);
  set_text(st_noz, b);

  temp_lookup(PANEL_TEMP_BED, &cur, &tgt);
  fmt_scaled(a1, sizeof(a1), cur, 100, 1);
  fmt_scaled(a2, sizeof(a2), tgt, 100, 1);
  snprintf(b, sizeof(b), "BED     %s / %s C", a1, a2);
  set_text(st_bed, b);

  fmt_scaled(a1, sizeof(a1), panel.pos.x, 1000, 2);
  fmt_scaled(a2, sizeof(a2), panel.pos.y, 1000, 2);
  snprintf(b, sizeof(b), "X %s  Y %s", a1, a2);
  set_text(st_xy, b);

  fmt_scaled(a1, sizeof(a1), panel.pos.z, 1000, 2);
  fmt_scaled(a2, sizeof(a2), panel.pos.e, 1000, 2);
  snprintf(b, sizeof(b), "Z %s  E %s", a1, a2);
  set_text(st_ze, b);

  snprintf(b, sizeof(b), "STATE: %s", state_name(panel.state));
  set_text(st_state, b);

  // co trang thai (viet tat cho vua 240px)
  b[0] = '\0';
  if (panel.flags & PANEL_SF_HOMED)       append_str(b, sizeof(b), "H ");
  if (panel.flags & PANEL_SF_MEDIA)       append_str(b, sizeof(b), "SD ");
  if (panel.flags & PANEL_SF_SD_PRINTING) append_str(b, sizeof(b), "PRN ");
  if (panel.flags & PANEL_SF_LEVELING)    append_str(b, sizeof(b), "LVL ");
  if (panel.flags & PANEL_SF_MESH_VALID)  append_str(b, sizeof(b), "MSH ");
  if (panel.flags & PANEL_SF_RUNOUT)      append_str(b, sizeof(b), "RUNOUT ");
  if (panel.flags & PANEL_SF_STEPPERS)    append_str(b, sizeof(b), "MTR");
  if (!b[0]) append_str(b, sizeof(b), "(khong co co nao)");
  set_text(st_flags, b);

  snprintf(b, sizeof(b), "FILE: %s", panel.prog.file[0] ? panel.prog.file : "-");
  set_text(st_file, b);

  const uint8_t pct = (panel.progress_pct > 100) ? 100 : panel.progress_pct;
  if (lv_bar_get_value(st_bar) != (int32_t)pct) lv_bar_set_value(st_bar, (int32_t)pct, LV_ANIM_OFF);

  fmt_time(t1, sizeof(t1), panel.prog.elapsed_s);
  fmt_time(t2, sizeof(t2), panel.prog.remaining_s);
  snprintf(b, sizeof(b), "%u%%  %s / %s", (unsigned)pct, t1, t2);
  set_text(st_pct, b);

  const unsigned long fp = (unsigned long)(uint32_t)(panel.pos.feed_pct / 100);
  const unsigned long fl = (unsigned long)(uint32_t)(panel.pos.flow_pct / 100);
  if (panel.n_fans) {
    fmt_scaled(a1, sizeof(a1), panel.fans[0].pct, 100, 0);
    snprintf(b, sizeof(b), "FAN %s%%  F/F %lu/%lu%%", a1, fp, fl);
  }
  else snprintf(b, sizeof(b), "feed/flow %lu/%lu%%", fp, fl);
  set_text(st_fan, b);
}

// ===========================================================================
//  TEMP: +/- roi gui M104/M140 (gia tri nen lay tu target Marlin dang bao)
// ===========================================================================
static void cb_temp_adj(lv_event_t *e) {
  const intptr_t v = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  heater = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const int8_t   delta  = (int8_t)(uint8_t)((unsigned)v & 0xFFU);

  int16_t cur = 0, tgt = 0;
  temp_lookup(heater ? PANEL_TEMP_BED : PANEL_TEMP_E0, &cur, &tgt);
  int32_t next = (int32_t)(tgt / 100) + (int32_t)delta;      // do C nguyen
  const int32_t hi = heater ? 120 : 300;
  if (next < 0) next = 0;
  if (next > hi) next = hi;

  char cmd[16];
  snprintf(cmd, sizeof(cmd), heater ? "M140 S%ld" : "M104 S%ld", (long)next);
  panel_link_send_gcode(cmd);
}

// Nhan "cur / set" cua man TEMP (tao trong page_temp_create)
static lv_obj_t *tp_noz, *tp_bed;

static void page_temp_create(lv_obj_t *p) {
  mk_label(p, "NOZZLE", COL_TXT, PAD_X, 6);
  tp_noz = mk_label(p, "--.- / -- C", COL_ACCENT, 76, 6);
  mk_btn(p, "-10", 6, 24, 56, 30, cb_temp_adj, TEMP_ADJ(0, -10));
  mk_btn(p, "-1",  64, 24, 56, 30, cb_temp_adj, TEMP_ADJ(0, -1));
  mk_btn(p, "+1", 122, 24, 56, 30, cb_temp_adj, TEMP_ADJ(0, +1));
  mk_btn(p, "+10", 180, 24, 56, 30, cb_temp_adj, TEMP_ADJ(0, +10));

  mk_label(p, "BED", COL_TXT, PAD_X, 68);
  tp_bed = mk_label(p, "--.- / -- C", COL_ACCENT, 76, 68);
  mk_btn(p, "-10", 6, 86, 56, 30, cb_temp_adj, TEMP_ADJ(1, -10));
  mk_btn(p, "-1",  64, 86, 56, 30, cb_temp_adj, TEMP_ADJ(1, -1));
  mk_btn(p, "+1", 122, 86, 56, 30, cb_temp_adj, TEMP_ADJ(1, +1));
  mk_btn(p, "+10", 180, 86, 56, 30, cb_temp_adj, TEMP_ADJ(1, +10));

  mk_btn(p, "COOLDOWN", PAD_X, 140, UI_LCD_HOR_RES - 2 * PAD_X, 36, cb_action, (void *)(intptr_t)A_COOLDOWN);
  mk_label(p, "M104 S<nhiet> / M140 S<nhiet>", COL_DIM, PAD_X, 190);
  mk_label(p, "  (gui ngay khi bam +/-)", COL_DIM, PAD_X, 208);
  mk_label(p, "STATUS: xem nhiet do thuc te", COL_DIM, PAD_X, 234);
}

static void page_temp_update(void) {
  char b[40], a1[12], a2[12];
  int16_t cur = 0, tgt = 0;

  temp_lookup(PANEL_TEMP_E0, &cur, &tgt);
  fmt_scaled(a1, sizeof(a1), cur, 100, 1);
  fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
  snprintf(b, sizeof(b), "%s / %s C", a1, a2);
  set_text(tp_noz, b);

  temp_lookup(PANEL_TEMP_BED, &cur, &tgt);
  fmt_scaled(a1, sizeof(a1), cur, 100, 1);
  fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
  snprintf(b, sizeof(b), "%s / %s C", a1, a2);
  set_text(tp_bed, b);
}

// ===========================================================================
//  JOG
// ===========================================================================
static uint8_t  s_axis = 0;                      // 0=X 1=Y 2=Z
static lv_obj_t *jg_axis[3];
static const char *const JOG_STEP[3] = {"0.1", "1", "10"};   // mm

static void cb_jog_axis(lv_event_t *e) {
  const uint8_t a = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (a > 2) return;
  s_axis = a;
  for (uint8_t i = 0; i < 3; i++)
    if (jg_axis[i]) lv_obj_set_style_bg_color(jg_axis[i], (i == a) ? COL_ACCENT : COL_BTN, 0);
}

// user_data: (chi so buoc << 8) | (1 = am)
static void cb_jog_move(lv_event_t *e) {
  const intptr_t v = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  step = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const bool     neg  = (((unsigned)v & 0xFFU) != 0);
  if (step > 2) return;

  char cmd[24];
  snprintf(cmd, sizeof(cmd), "G1 %c%s%s F600", "XYZ"[s_axis], neg ? "-" : "", JOG_STEP[step]);
  // G91 ... G90: luon dong lai bang G90 de mot dong bi mat khong de may o che do tuong doi
  panel_link_send_gcode("G91");
  panel_link_send_gcode(cmd);
  panel_link_send_gcode("G90");
}

#define JOG_ARG(step, neg) ((void *)(intptr_t)(((unsigned)(step) << 8) | (unsigned)((neg) ? 1U : 0U)))

static void page_jog_create(lv_obj_t *p) {
  mk_label(p, "TRUC", COL_DIM, PAD_X, 4);
  jg_axis[0] = mk_btn(p, "X", 6, 22, 74, 30, cb_jog_axis, (void *)(intptr_t)0);
  jg_axis[1] = mk_btn(p, "Y", 83, 22, 74, 30, cb_jog_axis, (void *)(intptr_t)1);
  jg_axis[2] = mk_btn(p, "Z", 160, 22, 74, 30, cb_jog_axis, (void *)(intptr_t)2);
  lv_obj_set_style_bg_color(jg_axis[0], COL_ACCENT, 0);

  mk_label(p, "DI CHUYEN (mm)", COL_DIM, PAD_X, 60);
  mk_btn(p, "-0.1", 6, 78, 112, 32, cb_jog_move, JOG_ARG(0, true));
  mk_btn(p, "+0.1", 122, 78, 112, 32, cb_jog_move, JOG_ARG(0, false));
  mk_btn(p, "-1", 6, 116, 112, 32, cb_jog_move, JOG_ARG(1, true));
  mk_btn(p, "+1", 122, 116, 112, 32, cb_jog_move, JOG_ARG(1, false));
  mk_btn(p, "-10", 6, 154, 112, 32, cb_jog_move, JOG_ARG(2, true));
  mk_btn(p, "+10", 122, 154, 112, 32, cb_jog_move, JOG_ARG(2, false));

  mk_btn(p, "HOME ALL (G28)", PAD_X, 198, UI_LCD_HOR_RES - 2 * PAD_X, 36, cb_action, (void *)(intptr_t)A_HOME);
  mk_label(p, "moi lan bam: G91 + G1 <truc><d> F600 + G90", COL_DIM, PAD_X, 244);
}

static void page_jog_update(void) {
  // khong co gi doi (chi hien thi truc dang chon qua mau nut)
}

// ===========================================================================
//  FILES
// ===========================================================================
static lv_obj_t *fl_roller, *fl_info;
static char      s_roller_buf[PANEL_MAX_FILES * PANEL_FILE_NAME_LEN + 1];
static uint32_t  s_files_sig = 0;
static uint8_t   s_files_n   = 0;

// FNV-1a don gian tren ten+co cua danh sach file -> biet khi nao phai tao lai roller
static uint32_t files_signature(void) {
  uint32_t h = 2166136261UL;
  for (uint8_t i = 0; i < panel.n_files; i++) {
    for (const char *s = panel.files[i].name; *s; s++) h = (h ^ (uint8_t)*s) * 16777619UL;
    h = (h ^ panel.files[i].flags) * 16777619UL;
  }
  return h ^ ((uint32_t)panel.n_files << 24);
}

static void files_rebuild(void) {
  uint8_t n = panel.n_files;
  if (n > PANEL_MAX_FILES) n = PANEL_MAX_FILES;

  char *p = s_roller_buf;
  size_t left = sizeof(s_roller_buf);
  s_roller_buf[0] = '\0';

  if (!n) {
    snprintf(p, left, "(khong co file)");
  }
  else {
    for (uint8_t i = 0; i < n; i++) {
      const bool isdir = ((panel.files[i].flags & 0x01U) != 0);
      const int w = snprintf(p, left, "%s%s%s", (i ? "\n" : ""), isdir ? "[D] " : "",
                             panel.files[i].name);
      if (w < 0 || (size_t)w >= left) break;
      p += w;
      left -= (size_t)w;
    }
  }

  uint16_t sel = lv_roller_get_selected(fl_roller);
  if (sel >= n) sel = n ? (uint16_t)(n - 1) : 0;
  lv_roller_set_options(fl_roller, s_roller_buf, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(fl_roller, sel, LV_ANIM_OFF);
}

// Lay ten file dang chon trong roller (tra ve NULL neu khong hop le)
static const panel_file_t *files_selected(void) {
  const uint16_t sel = lv_roller_get_selected(fl_roller);
  if (sel >= panel.n_files || sel >= PANEL_MAX_FILES) return NULL;
  return &panel.files[sel];
}

static void files_do(const bool force_dir) {
  const panel_file_t *f = files_selected();
  if (!f || !f->name[0]) return;
  if (force_dir || (f->flags & 0x01U)) {
    panel_link_request_dir(f->name);            // vao thu muc
  }
  else {
    char cmd[PANEL_FILE_NAME_LEN + 6];
    snprintf(cmd, sizeof(cmd), "M23 %s", f->name);
    panel_link_send_gcode(cmd);
    panel_link_send_gcode("M24");               // chon file roi in luon
  }
}

static void cb_files_print(lv_event_t *e) { (void)e; files_do(false); }
static void cb_files_dir(lv_event_t *e)   { (void)e; files_do(true); }

static void page_files_create(lv_obj_t *p) {
  fl_info = mk_label(p, "SD: -", COL_TXT, PAD_X, 4);

  fl_roller = lv_roller_create(p);
  lv_obj_set_pos(fl_roller, PAD_X, 24);
  lv_obj_set_size(fl_roller, UI_LCD_HOR_RES - 2 * PAD_X, 92);
  lv_roller_set_visible_row_count(fl_roller, 4);
  lv_roller_set_options(fl_roller, "(chua co du lieu)", LV_ROLLER_MODE_NORMAL);
  lv_obj_set_style_bg_color(fl_roller, COL_BTN, 0);
  lv_obj_set_style_bg_opa(fl_roller, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(fl_roller, COL_TXT, 0);
  lv_obj_set_style_border_width(fl_roller, 0, 0);
  lv_obj_set_style_text_color(fl_roller, lv_color_hex(0x102030), LV_PART_SELECTED);
  lv_obj_set_style_bg_color(fl_roller, COL_ACCENT, LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(fl_roller, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_add_flag(fl_roller, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  if (s_group) lv_group_add_obj(s_group, fl_roller);
  // Voi encoder: bam lan 1 = vao che do edit (xoay chon file), bam lan 2 = in file dang chon
  lv_obj_add_event_cb(fl_roller, cb_files_print, LV_EVENT_CLICKED, NULL);

  mk_btn(p, "PRINT", 6, 124, 74, 32, cb_files_print, NULL);
  mk_btn(p, "DIR", 83, 124, 74, 32, cb_files_dir, NULL);
  mk_btn(p, "UP", 160, 124, 74, 32, cb_action, (void *)(intptr_t)A_REQ_UP);
  mk_btn(p, "REFRESH", 6, 160, 74, 32, cb_action, (void *)(intptr_t)A_REQ_FILES);
  mk_btn(p, "PAUSE", 83, 160, 74, 32, cb_action, (void *)(intptr_t)A_PAUSE);
  mk_btn(p, "RESUME", 160, 160, 74, 32, cb_action, (void *)(intptr_t)A_RESUME);
  mk_btn(p, "STOP (M524)", PAD_X, 196, UI_LCD_HOR_RES - 2 * PAD_X, 34, cb_action, (void *)(intptr_t)A_STOP);
  mk_label(p, "PRINT = M23 + M24 khi dang cho file", COL_DIM, PAD_X, 238);
}

static void page_files_update(void) {
  char b[40];
  snprintf(b, sizeof(b), "SD: %u file%s", (unsigned)panel.n_files, panel.files_done ? "" : " (dang nhan)");
  set_text(fl_info, b);

  const uint32_t sig = files_signature();
  if (sig != s_files_sig || panel.n_files != s_files_n) {
    s_files_sig = sig;
    s_files_n   = panel.n_files;
    files_rebuild();
  }
}

// ===========================================================================
//  SETTINGS (chi doc gia tri Marlin gui; muon sua thi gui M-code)
// ===========================================================================
typedef struct {
  uint16_t    id;
  const char *name;
  uint16_t    scale;     // PANEL_VAL_SCALE / PANEL_TEMP_SCALE / PANEL_PCT_SCALE
  uint8_t     dec;
} set_row_t;

static const set_row_t SET_ROWS[] = {
  { PANEL_SET_Z_OFFSET,      "Z offset mm",   1000, 3 },
  { PANEL_SET_PROBE_OFFSET_Z,"Probe Z mm",    1000, 3 },
  { PANEL_SET_PROBE_OFFSET_X,"Probe X mm",    1000, 2 },
  { PANEL_SET_PROBE_OFFSET_Y,"Probe Y mm",    1000, 2 },
  { PANEL_SET_FEEDRATE_PCT,  "Feed %",         100, 0 },
  { PANEL_SET_FLOW_PCT,      "Flow %",         100, 0 },
  { PANEL_SET_FAN_PCT_E0,    "Fan E0 %",       100, 0 },
  { PANEL_SET_TARGET_TEMP_E0,"Target E0 C",    100, 0 },
  { PANEL_SET_TARGET_TEMP_BED,"Target bed C",  100, 0 },
  { PANEL_SET_STEPS_MM_X,    "Steps X",       1000, 2 },
  { PANEL_SET_STEPS_MM_Y,    "Steps Y",       1000, 2 },
  { PANEL_SET_STEPS_MM_Z,    "Steps Z",       1000, 2 },
  { PANEL_SET_STEPS_MM_E,    "Steps E",       1000, 2 },
  { PANEL_SET_MAXFEED_X,     "Max feed X",    1000, 1 },
  { PANEL_SET_MAXFEED_Y,     "Max feed Y",    1000, 1 },
  { PANEL_SET_MAXFEED_Z,     "Max feed Z",    1000, 1 },
  { PANEL_SET_MAXFEED_E,     "Max feed E",    1000, 1 },
  { PANEL_SET_MAXACCEL_X,    "Max accel X",   1000, 0 },
  { PANEL_SET_MAXACCEL_Y,    "Max accel Y",   1000, 0 },
  { PANEL_SET_MAXACCEL_Z,    "Max accel Z",   1000, 0 },
  { PANEL_SET_MAXACCEL_E,    "Max accel E",   1000, 0 },
  { PANEL_SET_JERK_X,        "Jerk X",        1000, 1 },
  { PANEL_SET_JERK_Y,        "Jerk Y",        1000, 1 },
  { PANEL_SET_JERK_Z,        "Jerk Z",        1000, 1 },
  { PANEL_SET_JERK_E,        "Jerk E",        1000, 1 },
  { PANEL_SET_JUNCTION_DEV,  "Junction dev",  1000, 3 },
  { PANEL_SET_LINEAR_ADV_E,  "Linear adv E",  1000, 3 },
  { PANEL_SET_PID_E_KP,      "PID E Kp",      1000, 2 },
  { PANEL_SET_PID_E_KI,      "PID E Ki",      1000, 3 },
  { PANEL_SET_PID_E_KD,      "PID E Kd",      1000, 2 },
  { PANEL_SET_PID_BED_KP,    "PID bed Kp",    1000, 2 },
  { PANEL_SET_PID_BED_KI,    "PID bed Ki",    1000, 3 },
  { PANEL_SET_PID_BED_KD,    "PID bed Kd",    1000, 2 }
};
#define SET_NROWS ((uint8_t)(sizeof(SET_ROWS) / sizeof(SET_ROWS[0])))

static lv_obj_t *se_table, *se_info;

static void cb_scroll_table(lv_event_t *e) {
  const lv_coord_t d = (lv_coord_t)(intptr_t)lv_event_get_user_data(e);
  if (se_table) lv_obj_scroll_by(se_table, 0, d, LV_ANIM_OFF);
}

static void page_settings_create(lv_obj_t *p) {
  se_info = mk_label(p, "Marlin gui: 0 thong so", COL_DIM, PAD_X, 4);

  se_table = lv_table_create(p);
  lv_obj_set_pos(se_table, PAD_X, 24);
  lv_obj_set_size(se_table, UI_LCD_HOR_RES - 2 * PAD_X, 154);
  lv_table_set_col_cnt(se_table, 2);
  lv_table_set_row_cnt(se_table, (uint16_t)(SET_NROWS + 1));
  lv_table_set_col_width(se_table, 0, 148);
  lv_table_set_col_width(se_table, 1, 82);
  lv_table_set_cell_value(se_table, 0, 0, "THONG SO");
  lv_table_set_cell_value(se_table, 0, 1, "GIA TRI");
  for (uint8_t i = 0; i < SET_NROWS; i++)
    lv_table_set_cell_value(se_table, (uint16_t)(i + 1), 0, SET_ROWS[i].name);

  lv_obj_set_style_bg_color(se_table, COL_BG, 0);
  lv_obj_set_style_bg_opa(se_table, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(se_table, COL_DIM, 0);
  lv_obj_set_style_border_width(se_table, 1, 0);
  lv_obj_set_style_pad_all(se_table, 0, 0);
  lv_obj_set_style_text_color(se_table, COL_TXT, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(se_table, COL_BTN, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(se_table, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_border_color(se_table, COL_DIM, LV_PART_ITEMS);
  lv_obj_set_style_border_width(se_table, 1, LV_PART_ITEMS);
  lv_obj_set_style_pad_all(se_table, 2, LV_PART_ITEMS);
  lv_obj_set_style_text_color(se_table, COL_ACCENT, LV_PART_ITEMS | LV_STATE_FOCUSED);

  mk_btn(p, "SAVE EEPROM", PAD_X, 186, 136, 32, cb_action, (void *)(intptr_t)A_SAVE);
  mk_btn(p, "UP", 148, 186, 42, 32, cb_scroll_table, (void *)(intptr_t)(-48));
  mk_btn(p, "DN", 194, 186, 42, 32, cb_scroll_table, (void *)(intptr_t)(48));
  mk_label(p, "SAVE = M500 (Marlin luu vao EEPROM)", COL_DIM, PAD_X, 226);
  mk_label(p, "UP/DN = cuon bang", COL_DIM, PAD_X, 244);
}

static void page_settings_update(void) {
  char b[40], a[16];
  uint8_t n = 0;
  for (uint8_t i = 0; i < SET_NROWS; i++) {
    const int32_t v = panel_setting(SET_ROWS[i].id, INT32_MIN);
    if (v != INT32_MIN) {
      fmt_scaled(a, sizeof(a), v, SET_ROWS[i].scale, SET_ROWS[i].dec);
      n++;
    }
    else snprintf(a, sizeof(a), "-");
    // chi set khi khac -> khong nhay + khong day SPI vo ich
    const char *old = lv_table_get_cell_value(se_table, (uint16_t)(i + 1), 1);
    if (!old || strcmp(old, a) != 0)
      lv_table_set_cell_value(se_table, (uint16_t)(i + 1), 1, a);
  }
  snprintf(b, sizeof(b), "%u/%u thong so da nhan", (unsigned)n, (unsigned)SET_NROWS);
  set_text(se_info, b);
}

// ===========================================================================
//  MESH
// ===========================================================================
static lv_obj_t *me_table, *me_info, *me_note;
static uint8_t   s_mesh_nx = 0, s_mesh_ny = 0;

// Nguong to mau: |z| > 0.05mm thi to xanh (cao) / do (thap)
#define MESH_HI_UM  50
#define MESH_LO_UM (-50)

// LVGL 8.3: LV_EVENT_DRAW_PART_BEGIN cho o cua bang -> dsc->id = chi so o
// (id = row * so_cot + col). Vat lieu to mau rieng tung o.
static void mesh_draw_cb(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!dsc || dsc->part != LV_PART_ITEMS || !dsc->rect_dsc) return;

  lv_obj_t *tbl = lv_event_get_target(e);
  const uint16_t cols = lv_table_get_col_cnt(tbl);
  if (!cols) return;

  const uint32_t row = dsc->id / cols;
  const uint32_t col = dsc->id % cols;
  if (col >= panel.mesh_nx || row >= panel.mesh_ny) return;

  const int32_t z = panel.mesh[col][row];          // panel_link.cpp: mesh[cot][hang]
  lv_color_t bg = COL_BTN;
  if (z > MESH_HI_UM)      bg = lv_color_hex(0x1E6B3A);   // xanh: diem cao
  else if (z < MESH_LO_UM) bg = lv_color_hex(0x7A2626);   // do  : diem thap
  dsc->rect_dsc->bg_color = bg;
  dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
}

static void page_mesh_create(lv_obj_t *p) {
  if (!(panel.hello_flags & PANEL_HAS_MESH)) {
    me_note = mk_label(p, "khong ho tro (PANEL_HAS_MESH=0)", COL_WARN, PAD_X, 60);
    (void)me_note;
    return;
  }

  me_info = mk_label(p, "mesh -", COL_TXT, PAD_X, 4);
  me_table = lv_table_create(p);
  lv_obj_set_pos(me_table, PAD_X, 24);
  lv_obj_set_size(me_table, UI_LCD_HOR_RES - 2 * PAD_X, 160);
  lv_table_set_col_cnt(me_table, 1);
  lv_table_set_row_cnt(me_table, 1);
  lv_obj_set_style_bg_color(me_table, COL_BG, 0);
  lv_obj_set_style_bg_opa(me_table, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(me_table, COL_DIM, 0);
  lv_obj_set_style_border_width(me_table, 1, 0);
  lv_obj_set_style_pad_all(me_table, 0, 0);
  lv_obj_set_style_text_color(me_table, COL_TXT, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(me_table, COL_BTN, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(me_table, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_border_color(me_table, COL_DIM, LV_PART_ITEMS);
  lv_obj_set_style_border_width(me_table, 1, LV_PART_ITEMS);
  lv_obj_set_style_pad_all(me_table, 2, LV_PART_ITEMS);
  lv_obj_add_event_cb(me_table, mesh_draw_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
  if (s_group) lv_group_add_obj(s_group, me_table);       // de encoder co the focus/cuon

  mk_btn(p, "REFRESH (PANEL_REQ_MESH)", PAD_X, 190, UI_LCD_HOR_RES - 2 * PAD_X, 32,
         cb_action, (void *)(intptr_t)A_REQ_MESH);
  mk_label(p, "xanh = diem cao, do = diem thap (+-0.05mm)", COL_DIM, PAD_X, 228);
  mk_label(p, "-  o ghi mm  -", COL_DIM, PAD_X, 246);
}

static void page_mesh_update(void) {
  if (!me_table) return;

  // So cot/hang doi -> tao lai khung bang (chi khi kich thuoc luoi doi)
  if (panel.mesh_nx && panel.mesh_ny && (panel.mesh_nx != s_mesh_nx || panel.mesh_ny != s_mesh_ny)) {
    s_mesh_nx = panel.mesh_nx;
    s_mesh_ny = panel.mesh_ny;
    lv_table_set_col_cnt(me_table, s_mesh_nx);
    lv_table_set_row_cnt(me_table, s_mesh_ny);
    const lv_coord_t cw = (lv_coord_t)(226 / (s_mesh_nx ? s_mesh_nx : 1));
    lv_table_set_col_width(me_table, 0, cw);
    for (uint16_t c = 1; c < s_mesh_nx; c++) lv_table_set_col_width(me_table, c, cw);
  }

  char b[40], a[12];
  int32_t vmin = 0, vmax = 0;
  bool first = true;
  for (uint8_t r = 0; r < panel.mesh_ny; r++) {
    for (uint8_t c = 0; c < panel.mesh_nx; c++) {
      const int32_t z = panel.mesh[c][r];
      if (first) { vmin = vmax = z; first = false; }
      if (z < vmin) vmin = z;
      if (z > vmax) vmax = z;
      fmt_scaled(a, sizeof(a), z, 1000, 2);
      const char *old = lv_table_get_cell_value(me_table, r, c);
      if (!old || strcmp(old, a) != 0) lv_table_set_cell_value(me_table, r, c, a);
    }
  }

  char b1[12], b2[12];
  fmt_scaled(b1, sizeof(b1), vmin, 1000, 2);
  fmt_scaled(b2, sizeof(b2), vmax, 1000, 2);
  snprintf(b, sizeof(b), "mesh %ux%u  rows %u%s", (unsigned)panel.mesh_nx, (unsigned)panel.mesh_ny,
           (unsigned)panel.mesh_rows, panel.mesh_valid ? "  valid" : "  -");
  set_text(me_info, b);
}

// ===========================================================================
//  CUSTOM (dung 2 nut nhu menu cu cua Marlin)
// ===========================================================================
static void page_custom_create(lv_obj_t *p) {
  mk_btn(p, "G34 Bed", PAD_X, 40, UI_LCD_HOR_RES - 2 * PAD_X, 48, cb_action, (void *)(intptr_t)A_G34);
  mk_label(p, "G34 Q99 - can bang 2 truc Z", COL_DIM, PAD_X, 94);
  mk_btn(p, "Init SD", PAD_X, 130, UI_LCD_HOR_RES - 2 * PAD_X, 48, cb_action, (void *)(intptr_t)A_INITSD);
  mk_label(p, "M22 (nha the) roi M21 (init lai)", COL_DIM, PAD_X, 184);
  mk_label(p, "2 nut giong menu cu cua Marlin", COL_DIM, PAD_X, 226);
}

static void page_custom_update(void) { }

// ===========================================================================
//  INFO
// ===========================================================================
static lv_obj_t *in_fw, *in_proto, *in_marlin, *in_rx, *in_stat, *in_board, *in_ui;

static void page_info_create(lv_obj_t *p) {
  in_fw     = mk_label(p, "FW: -", COL_TXT, PAD_X, 4);
  in_board  = mk_label(p, "BOARD: -", COL_TXT, PAD_X, 24);
  in_proto  = mk_label(p, "PROTO: -", COL_DIM, PAD_X, 44);
  in_marlin = mk_label(p, "MARLIN: -", COL_DIM, PAD_X, 64);
  in_stat   = mk_label(p, "LINK: -", COL_DIM, PAD_X, 84);
  in_rx     = mk_label(p, "RX: -", COL_DIM, PAD_X, 104);
  in_ui     = mk_label(p, "UI: LVGL 8.3 / ui_lvgl 1.0", COL_DIM, PAD_X, 124);
  mk_btn(p, "REFRESH FULL", PAD_X, 160, UI_LCD_HOR_RES - 2 * PAD_X, 36,
         cb_action, (void *)(intptr_t)A_REQ_FULL);
  mk_label(p, "PANEL_REQ_FULL: xin lai HELLO/STATUS/", COL_DIM, PAD_X, 208);
  mk_label(p, "TEMPS/POSITION/FANS/SETTINGS", COL_DIM, PAD_X, 226);
}

static void page_info_update(void) {
  char b[48];
  snprintf(b, sizeof(b), "FW: %s", panel.name[0] ? panel.name : "(chua co HELLO)");
  set_text(in_fw, b);

  snprintf(b, sizeof(b), "BOARD: %s  %dx%d", PANEL_NAME, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  set_text(in_board, b);

  snprintf(b, sizeof(b), "PROTO: %u (panel %u)", (unsigned)panel.proto, (unsigned)PANEL_PROTO_VERSION);
  set_text(in_proto, b);

  snprintf(b, sizeof(b), "MARLIN fw: 0x%04X", (unsigned)panel.marlin_fw);
  set_text(in_marlin, b);

  snprintf(b, sizeof(b), "LINK: %s  state %s", panel.linked ? "OK" : "NO", state_name(panel.state));
  set_text(in_stat, b);

  snprintf(b, sizeof(b), "RX: %u frame / %u loi  flags 0x%02X",
           (unsigned)panel.rx_frames, (unsigned)panel.rx_errors, (unsigned)panel.hello_flags);
  set_text(in_rx, b);

  snprintf(b, sizeof(b), "UI: LVGL 8.3 + %s", ui_port_input_is_encoder() ? "encoder" : "touch");
  set_text(in_ui, b);
}

// ===========================================================================
//  Tao man / chuyen man
// ===========================================================================
static void create_page(uint8_t p) {
  s_page[p] = mk_page(lv_scr_act());
  switch (p) {
    case PG_STATUS:   page_status_create(s_page[p]);   break;
    case PG_TEMP:     page_temp_create(s_page[p]);     break;
    case PG_JOG:      page_jog_create(s_page[p]);      break;
    case PG_FILES:    page_files_create(s_page[p]);    break;
    case PG_SETTINGS: page_settings_create(s_page[p]); break;
    case PG_MESH:     page_mesh_create(s_page[p]);     break;
    case PG_CUSTOM:   page_custom_create(s_page[p]);   break;
    case PG_INFO:     page_info_create(s_page[p]);     break;
    default: break;
  }
  lv_obj_add_flag(s_page[p], LV_OBJ_FLAG_HIDDEN);      // se duoc hien boi show_page()
}

static void show_page(uint8_t p) {
  if (p >= PG_COUNT) p = PG_STATUS;
  if (!s_page[p]) create_page(p);

  for (uint8_t i = 0; i < PG_COUNT; i++) {
    if (!s_page[i]) continue;
    if (i == p) lv_obj_clear_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_add_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
  }
  for (uint8_t i = 0; i < PG_COUNT; i++)
    if (s_nav[i]) lv_obj_set_style_bg_color(s_nav[i], (i == p) ? COL_ACCENT : COL_BTN, 0);

  set_text(s_title, PG_TITLE[p]);
  s_page_act = p;

  if (s_group && s_nav[p]) lv_group_focus_obj(s_nav[p]);
}

// ===========================================================================
//  Thanh tren + thanh duoi
// ===========================================================================
static void cb_nav(lv_event_t *e) {
  const uint8_t p = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  show_page(p);
}

static void build_chrome(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // ---- thanh tren ----
  lv_obj_t *bar = lv_obj_create(scr);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_size(bar, UI_LCD_HOR_RES, BAR_H);
  style_flat(bar, COL_BAR);

  s_title = mk_label(bar, "STATUS", COL_TXT, 4, 4);
  s_link  = mk_label(bar, "NO LINK", COL_ERR, 0, 5);
  lv_obj_align(s_link, LV_ALIGN_TOP_MID, 0, 5);
  mk_btn(bar, "BACK", 194, 2, 42, 22, cb_action, (void *)(intptr_t)A_BACK);

  // ---- thanh duoi: 8 nut chuyen man ----
  lv_obj_t *nav = lv_obj_create(scr);
  lv_obj_set_pos(nav, 0, NAV_Y);
  lv_obj_set_size(nav, UI_LCD_HOR_RES, NAV_H);
  style_flat(nav, COL_BAR);

  for (uint8_t i = 0; i < PG_COUNT; i++)
    s_nav[i] = mk_btn(nav, PG_NAV[i], (lv_coord_t)(4 + i * 29), 2, 28, 24,
                      cb_nav, (void *)(intptr_t)i);
}

// ===========================================================================
//  Prompt cua Marlin + thong bao
// ===========================================================================
static lv_obj_t *s_prompt_box, *s_msg_box;
static uint32_t  s_msg_shown_ms = 0, s_msg_seen_ms = 0;

static void box_close(lv_obj_t **box) {
  if (!*box) return;
  // Bo btnmatrix khoi group TRUOC khi xoa (khong de group giu con tro da chet)
  lv_obj_t *bm = lv_msgbox_get_btns(*box);
  if (bm && s_group) lv_group_remove_obj(bm);
  lv_msgbox_close_async(*box);
  *box = NULL;
}

static const char *sev_name(uint8_t sev) {
  switch (sev) {
    case PANEL_SEV_INFO:  return "INFO";
    case PANEL_SEV_WARN:  return "CANH BAO";
    case PANEL_SEV_ERROR: return "LOI";
    case PANEL_SEV_FATAL: return "LOI NANG";
    default:              return "THONG BAO";
  }
}

static lv_color_t sev_color(uint8_t sev) {
  switch (sev) {
    case PANEL_SEV_INFO:  return COL_ACCENT;
    case PANEL_SEV_WARN:  return COL_WARN;
    default:              return COL_ERR;
  }
}

static lv_obj_t *box_make(const char *title, const char *txt, lv_color_t title_col,
                          const char *const *btns) {
  lv_obj_t *box = lv_msgbox_create(NULL, title, txt, (const char **)btns, false);
  lv_obj_set_style_bg_color(box, COL_BAR, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, title_col, 0);
  lv_obj_set_style_text_color(box, COL_TXT, 0);

  lv_obj_t *t = lv_msgbox_get_title(box);
  if (t) lv_obj_set_style_text_color(t, title_col, 0);
  lv_obj_t *x = lv_msgbox_get_text(box);
  if (x) lv_obj_set_style_text_color(x, COL_TXT, 0);

  lv_obj_t *bm = lv_msgbox_get_btns(box);
  if (bm) {
    lv_obj_set_style_bg_color(bm, COL_BTN, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(bm, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, COL_TXT, LV_PART_ITEMS);
    lv_obj_set_style_border_color(bm, COL_ACCENT, LV_PART_ITEMS);
    lv_obj_set_style_border_width(bm, 1, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(bm, 2, LV_PART_ITEMS);
    if (s_group) { lv_group_add_obj(s_group, bm); lv_group_focus_obj(bm); }
  }
  return box;
}

static void cb_prompt_value(lv_event_t *e) {
  lv_obj_t *box = lv_event_get_target(e);
  const char *t = lv_msgbox_get_active_btn_text(box);
  const uint8_t ok = (t && (t[0] == 'O' || t[0] == 'o')) ? 1 : 0;    // OK / CANCEL
  panel_link_answer(ok);          // panel_link_answer() tu dat prompt_active = false
  box_close(&s_prompt_box);
  if (s_group && s_nav[s_page_act]) lv_group_focus_obj(s_nav[s_page_act]);
}

static void cb_msg_value(lv_event_t *e) {
  (void)e;
  box_close(&s_msg_box);
  if (s_group && s_nav[s_page_act]) lv_group_focus_obj(s_nav[s_page_act]);
}

static void boxes_update(void) {
  // ---- prompt cua Marlin: uu tien cao nhat ----
  if (panel.prompt_active) {
    if (!s_prompt_box) {
      box_close(&s_msg_box);                    // thong bao nhuong cho prompt
      static const char *const btns[] = {"OK", "CANCEL", ""};
      s_prompt_box = box_make("MARLIN", panel.prompt_text[0] ? panel.prompt_text : "(prompt)",
                              COL_WARN, btns);
      lv_obj_add_event_cb(s_prompt_box, cb_prompt_value, LV_EVENT_VALUE_CHANGED, NULL);
    }
    else {
      lv_obj_t *x = lv_msgbox_get_text(s_prompt_box);
      if (x) set_text(x, panel.prompt_text[0] ? panel.prompt_text : "(prompt)");
    }
  }
  else box_close(&s_prompt_box);

  // ---- thong bao moi (panel.message_ms doi) ----
  if (!s_prompt_box && panel.message[0] && panel.message_ms != s_msg_seen_ms) {
    s_msg_seen_ms = panel.message_ms;
    box_close(&s_msg_box);
    static const char *const btns[] = {"OK", ""};
    s_msg_box = box_make(sev_name(panel.message_sev), panel.message,
                         sev_color(panel.message_sev), btns);
    lv_obj_add_event_cb(s_msg_box, cb_msg_value, LV_EVENT_VALUE_CHANGED, NULL);
    s_msg_shown_ms = lv_tick_get();
  }
  // tu dong dong thong bao sau 6s (prompt thi de nguoi dung tra loi)
  if (s_msg_box && lv_tick_elaps(s_msg_shown_ms) > 6000U) box_close(&s_msg_box);
}

// ===========================================================================
//  API
// ===========================================================================
void ui_lvgl_begin(void) {
  lv_init();
  ui_port_display_init();

  // 1 vung dem 240x40 pixel (1/8 man hinh): du nhanh ma khong ton RAM
  static lv_color_t s_buf[UI_LCD_HOR_RES * 40];
  static lv_disp_draw_buf_t s_draw_buf;
  lv_disp_draw_buf_init(&s_draw_buf, s_buf, NULL, UI_LCD_HOR_RES * 40);

  static lv_disp_drv_t s_disp_drv;
  lv_disp_drv_init(&s_disp_drv);
  s_disp_drv.hor_res  = UI_LCD_HOR_RES;
  s_disp_drv.ver_res  = UI_LCD_VER_RES;
  s_disp_drv.flush_cb = ui_port_flush_cb;
  s_disp_drv.draw_buf = &s_draw_buf;
  lv_disp_drv_register(&s_disp_drv);

  // group chung cho encoder (an/hien man -> LVGL tu bo qua vat bi HIDDEN)
  s_group = lv_group_create();
  lv_group_set_focus_cb(s_group, on_focus_changed);
  lv_group_set_wrap(s_group, true);

  ui_port_input_init();

  static lv_indev_drv_t s_indev_drv;
  lv_indev_drv_init(&s_indev_drv);
  s_indev_drv.type    = ui_port_input_is_encoder() ? LV_INDEV_TYPE_ENCODER : LV_INDEV_TYPE_POINTER;
  s_indev_drv.read_cb = ui_port_read_cb;
  lv_indev_t *indev = lv_indev_drv_register(&s_indev_drv);
  if (indev && ui_port_input_is_encoder()) lv_indev_set_group(indev, s_group);

  build_chrome();
  show_page(PG_STATUS);

  s_tick_ms = s_upd_ms = s_slow_ms = ui_port_millis();
  lv_timer_handler();                     // ve man hinh dau tien
}

void ui_lvgl_task(void) {
  // Tick cho LVGL: LVGL 8.3 chi chay lv_timer khi tick da tang, nen KHONG the dung
  // mot lv_timer goi lv_tick_inc() (timer do se khong bao gio chay -> treo).
  // Lay truc tiep tu millis() la cach dung.
  const uint32_t now = ui_port_millis();
  if (now != s_tick_ms) {
    lv_tick_inc(now - s_tick_ms);
    s_tick_ms = now;
  }

  lv_timer_handler();

  if (now - s_upd_ms < UI_REFRESH_MS) return;      // UI_REFRESH_MS trong panel_config.h
  s_upd_ms = now;

  // thanh tren: trang thai lien ket (chi set khi doi de khong set style mai)
  if (panel.linked != s_link_state) {
    s_link_state = panel.linked;
    set_text(s_link, s_link_state ? "LINK" : "NO LINK");
    lv_obj_set_style_text_color(s_link, s_link_state ? COL_OK : COL_ERR, 0);
  }

  switch (s_page_act) {
    case PG_STATUS: page_status_update(); break;
    case PG_TEMP:   page_temp_update();   break;
    case PG_JOG:    page_jog_update();    break;
    case PG_INFO:   page_info_update();   break;
    default: break;
  }

  // man nang (bang/danh sach): cap nhat cham hon cho do ton SPI
  if (now - s_slow_ms >= 400U) {
    s_slow_ms = now;
    switch (s_page_act) {
      case PG_FILES:    page_files_update();    break;
      case PG_SETTINGS: page_settings_update(); break;
      case PG_MESH:     page_mesh_update();     break;
      case PG_CUSTOM:   page_custom_update();   break;
      default: break;
    }
  }

  boxes_update();
}

#endif  // PANEL_USE_LVGL
