/**
 * ui_page_temp.cpp -- man TEMP (NHIET DO) cua UI LVGL (man hinh CYD).
 *
 * Tach tu ui_lvgl.cpp (chang 2/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN (chi khac cho doc
 * kho preset preheat qua API cong khai - xem duoi), khong doi logic/ten/chuoi.
 *
 *   - cb_temp_adj(): +/- 1/10 do C -> M104 S... / M140 S..., hang FAN -> M106 S0..255.
 *   - temp_row()/fan_row(): 1 hang cho 1 nhom (ten | -10 | -1 | gia tri | +1 | +10).
 *   - cb_temp_preheat()/tp_pre_sync(): 3 nut preset PLA/PETG/ABS. Gia tri lay tu KHO PRESET
 *     cua MAN HINH CYD (ui_preheat.cpp) qua API CONG KHAI ui_preheat_get()/ui_preheat_label()
 *     (khai bao trong ui_lvgl.h) - KHONG file nao khac duoc doc thang mang s_pre_*.
 *   - page_temp_create()/page_temp_update(): dung man + cap nhat nhiet/hang FAN.
 *
 * tp_pre_sync() KHONG `static`: ui_preheat_set() goi no de ve lai nhan nut sau khi sua preset
 * o trang SETTINGS (khai bao trong ui_lvgl_int.h).
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf
#include <string.h>                 // strcmp (tp_pre_sync)

#if CYDLCD_USE_LVGL

// user_data cua nut: (heater << 8) | (int8)delta
#define TEMP_ADJ(h, d) ((void *)(intptr_t)(((unsigned)(h) << 8) | (unsigned)(uint8_t)(int8_t)(d)))
// ===========================================================================
//  TEMP: +/- roi gui M104/M140 (gia tri nen lay tu target Marlin dang bao)
//  Moi nhom 1 hang: [ten] [-] [-] [gia tri] [+] [+]  -> tong 134 <= 142
// ===========================================================================
static void cb_temp_adj(lv_event_t *e) {
  const intptr_t v = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  heater = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const int8_t   delta  = (int8_t)(uint8_t)((unsigned)v & 0xFFU);
  char cmd[16];

  // heater 2 = QUAT: gui M106 S0..255 (nhu o FAN tren man STATUS cua Marlin)
  if (heater == 2) {
    int32_t pct = 0;
    if (cydlcd.n_fans) pct = (int32_t)(cydlcd.fans[0].pct / 100);
    pct += delta;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    snprintf(cmd, sizeof(cmd), "M106 S%ld", (long)((pct * 255 + 50) / 100));
    cydlcd_link_send_gcode(cmd);
    return;
  }

  int16_t cur = 0, tgt = 0;
  temp_lookup(heater ? CYDLCD_TEMP_BED : CYDLCD_TEMP_E0, &cur, &tgt);
  int32_t next = (int32_t)(tgt / 100) + (int32_t)delta;      // do C nguyen
  const int32_t hi = heater ? 120 : 300;
  if (next < 0) next = 0;
  if (next > hi) next = hi;

  snprintf(cmd, sizeof(cmd), heater ? "M140 S%ld" : "M104 S%ld", (long)next);
  cydlcd_link_send_gcode(cmd);
}

#define TP_ROW_H   30                                  // 1 hang nhom nhiet do
#define TP_LBL_W   60                                  // "NOZZLE" / "BED"
#define TP_BTN_BIG 32                                  // nut -10 / +10
#define TP_BTN_SML 28                                  // nut -1 / +1

// Nhan "cur / set" cua man TEMP (tao trong page_temp_create)
static lv_obj_t *tp_noz, *tp_bed, *tp_fan;

// 1 hang cho 1 heater: ten | -10 | -1 | gia tri (grow) | +1 | +10
static void temp_row(lv_obj_t *p, const char *name, uint8_t heater, lv_obj_t **val) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, TP_ROW_H, 0, GAP);
  mk_label_w(r, name, COL_TXT, TP_LBL_W);
  mk_btn(r, "-10", TP_BTN_BIG, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(heater, -10));
  mk_btn(r, "-1",  TP_BTN_SML, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(heater, -1));
  *val = mk_label_w(r, "--.- / -- C", COL_ACCENT, 100);
  lv_obj_set_flex_grow(*val, 1);                   // gia tri an het cho con lai (~118 px)
  mk_btn(r, "+1",  TP_BTN_SML, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(heater, +1));
  mk_btn(r, "+10", TP_BTN_BIG, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(heater, +10));
}

// Hang FAN: OFF | -10 | gia tri (grow) | +10 | MAX  -> M106 S0..255
static void fan_row(lv_obj_t *p) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, TP_ROW_H, 0, GAP);
  mk_label_w(r, "FAN", COL_TXT, TP_LBL_W);
  mk_btn(r, "OFF", TP_BTN_BIG, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(2, -100));
  mk_btn(r, "-10", TP_BTN_SML, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(2, -10));
  tp_fan = mk_label_w(r, "-%", MR_COLD, 100);
  lv_obj_set_flex_grow(tp_fan, 1);
  mk_btn(r, "+10", TP_BTN_SML, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(2, +10));
  mk_btn(r, "MAX", TP_BTN_BIG, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(2, +100));
}

// Preset preheat: gia tri lay tu KHO CUA MAN HINH CYD (NVS, sua duoc o trang SETTINGS - xem khoi "PREHEAT
// presets" o phan SETTINGS). Mac dinh ban dau = Configuration.h (PLA 200/60 - PETG 230/60 - ABS 240/60).
// KHONG gui M106 khi FAN = 0: Marlin khong dung FAN_SPEED cua preheat, gui M106 S0 se TAT quat ngoai y.
#define TP_PRE_H    28                                 // nut preset cao 28 >= 26 (du de cham)
#define TP_TUNE_H   28                                 // hang tuning (M306/M303/SAVE)
#define TP_PRE_PLA  0
#define TP_PRE_PETG 1
#define TP_PRE_ABS  2

static char      s_profile[8] = "";                    // profile dang chon ("" = chua chon -> ---)
static lv_obj_t *tp_prof;                              // nhan DONG DAU: "Profile: <ten>"
static lv_obj_t *tp_pre_btn[CYDLCD_PRE_COUNT];          // 3 nut PLA/PETG/ABS (nhan doi theo preset)
static char      s_pre_txt[CYDLCD_PRE_COUNT][16];       // nhan dang hien (de chi ve khi doi)

static void cb_temp_preheat(lv_event_t *e) {
  const uint8_t k = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (k >= CYDLCD_PRE_COUNT) return;

  // Gia tri lay tu KHO PRESET cua MAN HINH CYD qua API cong khai (dinh nghia o ui_preheat.cpp):
  // KHONG doc thang mang s_pre_* - chi ui_preheat.cpp duoc giu kho do.
  uint16_t noz = 0, bed = 0;
  uint8_t  fan = 0;
  if (!ui_preheat_get(k, &noz, &bed, &fan)) return;

  char cmd[20];
  snprintf(cmd, sizeof(cmd), "M104 S%u", (unsigned)noz);
  cydlcd_link_send_gcode(cmd);
  snprintf(cmd, sizeof(cmd), "M140 S%u", (unsigned)bed);
  cydlcd_link_send_gcode(cmd);
  if (fan > 0) {                                       // 0 = khong dong toi quat (xem ghi chu tren)
    snprintf(cmd, sizeof(cmd), "M106 S%u", (unsigned)fan);
    cydlcd_link_send_gcode(cmd);
  }
  // Nho profile dang chon -> hien o DONG DAU (khong luu NVS). KHONG hien hop/dialog nao.
  snprintf(s_profile, sizeof(s_profile), "%s", ui_preheat_label(k));
  // Target nhiet lay tu frame TEMPS/SETTING -> xin gui lai cho o nhiet khop ngay
  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);
}

// Doi nhan 3 nut theo preset hien co (chi ghi khi chuoi doi -> khong day SPI vo ich).
void tp_pre_sync(void) {
  for (uint8_t k = 0; k < CYDLCD_PRE_COUNT; k++) {
    uint16_t noz = 0, bed = 0;
    ui_preheat_get(k, &noz, &bed, NULL);          // doc kho preheat qua API cong khai
    char t[16];
    snprintf(t, sizeof(t), "%s %u/%u", ui_preheat_label(k), (unsigned)noz, (unsigned)bed);
    if (strcmp(t, s_pre_txt[k]) == 0) continue;
    snprintf(s_pre_txt[k], sizeof(s_pre_txt[k]), "%s", t);
    if (tp_pre_btn[k]) set_text(lv_obj_get_child(tp_pre_btn[k], 0), t);
  }
}

void page_temp_create(lv_obj_t *p) {
  // DONG DAU: profile dang chon (bam PLA/PETG/ABS o duoi se doi dong nay)
  tp_prof = mk_label_w(p, "Profile: ---", MR_MENU, PAGE_INN_W);

  temp_row(p, "NOZZLE", 0, &tp_noz);
  temp_row(p, "BED",    1, &tp_bed);
  fan_row(p);

  // 1 hang preset: PLA | PETG | ABS (moi nut gui M104 + M140 theo preset da luu, nhu menu preheat
  // cua Marlin). Nhan nut duoc ve lai khi doi gia tri trong SETTINGS -> xem tp_pre_sync().
  lv_obj_t *rp = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, TP_PRE_H, 0, GAP);
  tp_pre_btn[0] = mk_btn_grow(rp, "PLA",  TP_PRE_H, cb_temp_preheat, (void *)(intptr_t)TP_PRE_PLA);
  tp_pre_btn[1] = mk_btn_grow(rp, "PETG", TP_PRE_H, cb_temp_preheat, (void *)(intptr_t)TP_PRE_PETG);
  tp_pre_btn[2] = mk_btn_grow(rp, "ABS",  TP_PRE_H, cb_temp_preheat, (void *)(intptr_t)TP_PRE_ABS);
  for (uint8_t k = 0; k < CYDLCD_PRE_COUNT; k++) s_pre_txt[k][0] = '\0';   // buoc tp_pre_sync ve lai
  tp_pre_sync();

  mk_btn(p, "COOLDOWN", PAGE_INN_W, 26, cb_action, (void *)(intptr_t)A_COOLDOWN);

  // P5 - tuning: MPC autotune dau in | PID ban in (target 60) | luu EEPROM
  lv_obj_t *rt = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, TP_TUNE_H, 0, GAP);
  mk_btn_grow(rt, "M306 T",      TP_TUNE_H, cb_action, (void *)(intptr_t)A_MPC_TUNE);
  mk_btn_grow(rt, "M303 E-1 60", TP_TUNE_H, cb_action, (void *)(intptr_t)A_PID_BED);
  mk_btn_grow(rt, "SAVE EEPROM", TP_TUNE_H, cb_action, (void *)(intptr_t)A_SAVE);

  // Tong chieu cao: 16 + 30*3 + 28 + 26 + 28 + 6*GAP = 200 <= PAGE_INN_H (206)
}

void page_temp_update(void) {
  if (!tp_prof) return;                  // trang da bi thu hoi (an toan: xem page_temp_destroy)
  char b[40], a1[12], a2[12];
  int16_t cur = 0, tgt = 0;

  tp_pre_sync();                       // nhan PLA/PETG/ABS doi khi preset duoc sua o SETTINGS

  snprintf(b, sizeof(b), "Profile: %s", s_profile[0] ? s_profile : "---");
  set_text(tp_prof, b);

  temp_lookup(CYDLCD_TEMP_E0, &cur, &tgt);
  fmt_scaled(a1, sizeof(a1), cur, 100, 1);
  fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
  snprintf(b, sizeof(b), "%s / %s C", a1, a2);
  set_text(tp_noz, b);

  temp_lookup(CYDLCD_TEMP_BED, &cur, &tgt);
  fmt_scaled(a1, sizeof(a1), cur, 100, 1);
  fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
  snprintf(b, sizeof(b), "%s / %s C", a1, a2);
  set_text(tp_bed, b);

  const uint8_t fp = cydlcd.n_fans ? (uint8_t)(cydlcd.fans[0].pct / 100) : 0;
  snprintf(b, sizeof(b), "%u%%", (unsigned)fp);
  set_text(tp_fan, b);
}

// Huy man TEMP (xem quy uoc o ui_lvgl_int.h): dat NULL cac nhan + 3 nut preset, va xoa ban sao
// nhan dang hien (s_pre_txt) de tp_pre_sync() ve lai nhan cho 3 nut MOI. Container trang do
// show_page() xoa bang lv_obj_del() nen khong xoa widget o day.
//
// s_profile: GIU NGUYEN (ten preset nguoi dung da bam o lan truoc) - khong phai trang thai widget
// va khong can cho viec tao lai: page_temp_update() hien lai ngay sau khi trang duoc tao.
void page_temp_destroy(void) {
  tp_prof = NULL;
  tp_noz  = NULL;
  tp_bed  = NULL;
  tp_fan  = NULL;
  for (uint8_t k = 0; k < CYDLCD_PRE_COUNT; k++) {
    tp_pre_btn[k] = NULL;
    s_pre_txt[k][0] = '\0';
  }
}

#endif  // CYDLCD_USE_LVGL
