/**
 * ui_page_settings.cpp -- man SETTINGS cua UI LVGL (man hinh CYD): bang thong so Marlin gui
 * xuong (steps/mm, feed/flow, PID, probe offset, 9 dong preset preheat...) + SUA duoc tung dong.
 *
 * Tach tu ui_lvgl.cpp (chang 3/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 * Gom:
 *   - bang SET_ROWS (id -> ten/scale/so chu so) + settings_build_list()/settings_fill_table();
 *   - lop phu SUA so (ov_*): hop -/+/OK dung CHUNG, mo tu 1 dong bang hoac tu nut JOG (M851 Z);
 *   - se_send_mcode(): doi 1 dong thanh dung M-code cua Marlin (M92/M203/M204/M205/M301/M304/
 *     M851/M220/M221/M106/M104/M140/M145...).
 *
 * LUU Y: lop phu ov_* la con cua lv_scr_act (KHONG phai con cua trang) nen show_page() phai goi
 * ov_hide() TRUOC khi thu hoi trang - xem show_page() trong ui_lvgl.cpp.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf
#include <string.h>                 // strcmp
#include <Arduino.h>                // Serial.printf (WIFILOG - xem ui_lvgl_int.h)

#if CYDLCD_USE_LVGL

// ===========================================================================
//  SETTINGS (bang thong so Marlin gui + SUA duoc: cham 1 dong -> lop phu -/+/OK)
//  Bang (cao SET_TABLE_H) + 1 hang nut UP | DOWN | SAVE EEPROM + 1 dong info <= 206
// ===========================================================================
typedef struct {
  uint16_t    id;
  const char *name;
  uint16_t    scale;     // CYDLCD_VAL_SCALE / CYDLCD_TEMP_SCALE / CYDLCD_PCT_SCALE
  uint8_t     dec;
} set_row_t;

static const set_row_t SET_ROWS[] = {
  // KHONG co dong "Z offset": CYDLCD_SET_Z_OFFSET va CYDLCD_SET_PROBE_OFFSET_Z cung la
  // getZOffset_mm() (probe.offset.z) -> se trung 2 dong, va sua bang M206 Z se ghi vao
  // HOME OFFSET (sai cho Voron Tap). Chi giu "Probe Z mm" -> M851 Z.
  { CYDLCD_SET_PROBE_OFFSET_Z,"Probe Z mm",    1000, 3 },
  { CYDLCD_SET_PROBE_OFFSET_X,"Probe X mm",    1000, 2 },
  { CYDLCD_SET_PROBE_OFFSET_Y,"Probe Y mm",    1000, 2 },
  // FADE HEIGHT (M420 Z, mm): chieu cao ma ban in chuyen dan tu "bu mesh" sang "khong bu mesh".
  // YEU CAU 2026-10-15: "trong setting chua co fade height". Chi hien khi ban build Marlin co
  // ENABLE_LEVELING_FADE_HEIGHT (Marlin day gia tri xuong; khong co thi dong nay tu bi loc khoi bang).
  // VI TRI (sua 2026-10-15: nguoi dung bao "mat setting fade height roi"): bang SETTINGS gio co 56 dong ma
  // khung chi hien ~7 dong -> dong nao nam duoi mep thi phai CUON moi thay. Fade height truoc o dong 9 (ngay
  // duoi mep) nen bi tuong la "mat"; nay dua len NGAY SAU 3 dong probe (dong 4) de luon thay khong can cuon.
  { CYDLCD_SET_FADE_HEIGHT,   "Fade height mm", 1000, 1 },
  { CYDLCD_SET_FEEDRATE_PCT,  "Feed %",         100, 0 },
  { CYDLCD_SET_FLOW_PCT,      "Flow %",         100, 0 },
  { CYDLCD_SET_FAN_PCT_E0,    "Fan E0 %",       100, 0 },
  { CYDLCD_SET_TARGET_TEMP_E0,"Target E0 C",    100, 0 },
  { CYDLCD_SET_TARGET_TEMP_BED,"Target bed C",  100, 0 },
  // 6 tham so MPC (M306) - SUA duoc o day, va man TUNING chi hien thi (giong menu MPC cua Marlin 12864).
  // Chi hien khi Marlin co MPCTEMP (khong co thi Marlin khong day -> dong tu bi loc).
  { CYDLCD_SET_MPC_P,         "MPC P heater W",   1000, 2 },
  { CYDLCD_SET_MPC_C,         "MPC C block J/K",  1000, 2 },
  { CYDLCD_SET_MPC_R,         "MPC R sensor",    10000, 4 },
  { CYDLCD_SET_MPC_A,         "MPC A fan0 W/K",  10000, 4 },
  { CYDLCD_SET_MPC_F,         "MPC F fanM W/K",  10000, 4 },
  { CYDLCD_SET_MPC_H,         "MPC H fil J/Kmm", 10000, 4 },
  // 2026-10-15: 3 nhom nguoi dung chon them "cho day du nhu M503" (xem cydlcd_serial.cpp: push_settings_body)
  { CYDLCD_SET_HOME_OFFSET_X, "Home off X mm",   1000, 2 },   // M206 X (KHONG co Z - xem chu thich tren)
  { CYDLCD_SET_HOME_OFFSET_Y, "Home off Y mm",   1000, 2 },   // M206 Y
  // M200 D: gui kem S<trang thai hien tai> vi `M200 D<v>` mot minh se TU BAT bu bang the tich (Marlin doc:
  // "Dn = enable for compatibility") -> luong ep doi => hong ban in. Xem se_send_mcode().
  { CYDLCD_SET_FILAMENT_DIA,  "Filament D mm",   1000, 2 },
  { CYDLCD_SET_FILAMENT_VOL,  "Filament vol",       1, 0 },  // M200 S0/S1 (0 = tat bu bang the tich)
  { CYDLCD_SET_FC_LOAD,       "FC load mm",      1000, 1 },  // M603 L
  { CYDLCD_SET_FC_UNLOAD,     "FC unload mm",    1000, 1 },  // M603 U
  // PREHEAT presets (MAN HINH CYD tu luu trong NVS - xem khoi o tren): do C / do C / 0..255
  { CYDLCD_PRE_ID_BASE + 1, "PLA nozzle C",     100, 0 },
  { CYDLCD_PRE_ID_BASE + 2, "PLA bed C",        100, 0 },
  { CYDLCD_PRE_ID_BASE + 3, "PLA fan",            1, 0 },
  { CYDLCD_PRE_ID_BASE + 4, "PETG nozzle C",    100, 0 },
  { CYDLCD_PRE_ID_BASE + 5, "PETG bed C",       100, 0 },
  { CYDLCD_PRE_ID_BASE + 6, "PETG fan",           1, 0 },
  { CYDLCD_PRE_ID_BASE + 7, "ABS nozzle C",     100, 0 },
  { CYDLCD_PRE_ID_BASE + 8, "ABS bed C",        100, 0 },
  { CYDLCD_PRE_ID_BASE + 9, "ABS fan",            1, 0 },
  { CYDLCD_SET_STEPS_MM_X,    "Steps X",       1000, 2 },
  { CYDLCD_SET_STEPS_MM_Y,    "Steps Y",       1000, 2 },
  { CYDLCD_SET_STEPS_MM_Z,    "Steps Z",       1000, 2 },
  { CYDLCD_SET_STEPS_MM_E,    "Steps E",       1000, 2 },
  { CYDLCD_SET_MAXFEED_X,     "Max feed X",    1000, 1 },
  { CYDLCD_SET_MAXFEED_Y,     "Max feed Y",    1000, 1 },
  { CYDLCD_SET_MAXFEED_Z,     "Max feed Z",    1000, 1 },
  { CYDLCD_SET_MAXFEED_E,     "Max feed E",    1000, 1 },
  { CYDLCD_SET_MAXACCEL_X,    "Max accel X",   1000, 0 },
  { CYDLCD_SET_MAXACCEL_Y,    "Max accel Y",   1000, 0 },
  { CYDLCD_SET_MAXACCEL_Z,    "Max accel Z",   1000, 0 },
  { CYDLCD_SET_MAXACCEL_E,    "Max accel E",   1000, 0 },
  { CYDLCD_SET_JERK_X,        "Jerk X",        1000, 1 },
  { CYDLCD_SET_JERK_Y,        "Jerk Y",        1000, 1 },
  { CYDLCD_SET_JERK_Z,        "Jerk Z",        1000, 1 },
  { CYDLCD_SET_JERK_E,        "Jerk E",        1000, 1 },
  { CYDLCD_SET_JUNCTION_DEV,  "Junction dev",  1000, 3 },
  { CYDLCD_SET_LINEAR_ADV_E,  "Linear adv E",  1000, 3 },
  { CYDLCD_SET_PID_E_KP,      "PID E Kp",      1000, 2 },
  { CYDLCD_SET_PID_E_KI,      "PID E Ki",      1000, 3 },
  { CYDLCD_SET_PID_E_KD,      "PID E Kd",      1000, 2 },
  { CYDLCD_SET_PID_BED_KP,    "PID bed Kp",    1000, 2 },
  { CYDLCD_SET_PID_BED_KI,    "PID bed Ki",    1000, 3 },
  { CYDLCD_SET_PID_BED_KD,    "PID bed Kd",    1000, 2 },
  // P8: gia toc M204 + dong TMC M906
  { CYDLCD_SET_MAXACCEL_PRINT,   "Accel print",   1000, 0 },
  { CYDLCD_SET_MAXACCEL_RETRACT, "Accel retract", 1000, 0 },
  { CYDLCD_SET_MAXACCEL_TRAVEL,  "Accel travel",  1000, 0 },
  // LUU Y (sua 2026-10-15): dong dien TMC Marlin gui RAW bang mA (cydlcd_serial.cpp:
  // `push_setting(CYDLCD_SET_TMC_CURRENT_X, (int32_t)stepperX.val_mA)`), KHONG nhan he so -> scale phai la 1.
  // Truoc day de 1000 nen 600 mA hien thanh "1" (600/1000 = 0.6 -> lam tron 1) - nguoi dung nhin nham.
  { CYDLCD_SET_TMC_CURRENT_X,    "TMC I X mA",       1, 0 },
  { CYDLCD_SET_TMC_CURRENT_Y,    "TMC I Y mA",       1, 0 },
  { CYDLCD_SET_TMC_CURRENT_Z,    "TMC I Z mA",       1, 0 },
  { CYDLCD_SET_TMC_CURRENT_E,    "TMC I E mA",       1, 0 }
};
#define SET_NROWS ((uint8_t)(sizeof(SET_ROWS) / sizeof(SET_ROWS[0])))

// Bang 124 + info 16 + 2 hang nut (30 + 28) + 3*GAP = 206 <= PAGE_INN_H
#define SET_TABLE_H 124                              // bang (cuon duoc) - xem tong chieu cao tren
#define SET_COL0_W  190
#define SET_COL1_W  (PAGE_INN_W - SET_COL0_W - 4)

static lv_obj_t *se_table, *se_info;
static uint8_t   se_idx[SET_NROWS];   // CHI so cac dong CO gia tri: loc bo cac dong ma ban build
static uint8_t   se_n = 0;            // nay khong gui (Junction dev / Linear adv / PID... -> trong)

static void cb_scroll_table(lv_event_t *e) {
  const lv_coord_t d = (lv_coord_t)(intptr_t)lv_event_get_user_data(e);
  if (se_table) lv_obj_scroll_by(se_table, 0, d, LV_ANIM_OFF);
}

// Loc: chi giu nhung dong ma MAN HINH CYD DA NHAN duoc gia tri tu Marlin (2026-10-13, yeu cau nguoi dung)
static void settings_build_list(void) {
  se_n = 0;
  for (uint8_t i = 0; i < SET_NROWS; i++)
    if (cydlcd_setting(SET_ROWS[i].id, INT32_MIN) != INT32_MIN) se_idx[se_n++] = i;
}

static void settings_fill_table(void) {
  lv_table_set_row_cnt(se_table, (uint16_t)(se_n + 1));
  for (uint8_t k = 0; k < se_n; k++)
    lv_table_set_cell_value(se_table, (uint16_t)(k + 1), 0, SET_ROWS[se_idx[k]].name);
}

// ---------------------------------------------------------------------------
//  Lop phu SUA 1 dong: cham vao 1 dong cua bang -> hien khung nho ngay TREN bang
//  gom ten thong so + gia tri dang sua + 4 NUT BUOC 0.01 | 0.1 | 1 | 10
//  + 2 nut - | + de ap buoc do vao gia tri + OK | Huy.
//  Buoc nguoi dung chon la don vi HIEN THI -> quy ve raw = buoc * scale, va KHONG nho hon
//  1 don vi hien thi cuoi cua dong (scale / 10^dec); nho vay gia tri luon nam dung so le
//  hien thi (khong bao gio co nhieu chu so thap phan hon dong do). Moi tinh toan la so nguyen.
//  Bam OK: gui M-code dung voi .id (xem se_send_mcode) roi xin Marlin gui lai thong so.
//  Chu trong lop phu dung font mono GFX co dau tieng Viet (che do 4 - xem vn_font()); o che do 0
//  thi cac nhan co dau nay se ra O VUONG (Montserrat khong co U+1EA0..U+1EF9).
// ---------------------------------------------------------------------------
#define OV_W       260                              // khung noi dung (giua man 320)
#define OV_H       112
#define OV_BTN_H   28                               // nut >= 26 px cho de cham
#define OV_LINE_H  20                               // 1 dong chu cua font Segoe (line_height 18)
#define OV_PAD     4
#define OV_VCAP    150                              // be rong o gia tri
#define OV_RAW_LIM 100000000L                       // tran raw (du moi thong so o day)
#define OV_ADJ(d)  ((void *)(intptr_t)(int8_t)(d))
#define OV_STEP(d) ((void *)(intptr_t)(d))

static const uint32_t OV_TEN_POW[4] = {1, 10, 100, 1000};

// 4 muc buoc, don vi HIEN THI nhan 100 (giu so nguyen: 0.01 -> 1)
static const int32_t OV_STEP100[4] = { 1, 10, 100, 1000 };
static const char *const OV_STEP_TXT[4] = { "0.01", "0.1", "1", "10" };
// So CHU SO THAP PHAN tuong ung tung muc buoc (0.01 -> 2, 0.1 -> 1, 1 va 10 -> 0).
// VI SAO CAN: neu dong chi hien 1 chu so thap phan (vd "Fade height 10.0") ma nguoi dung bam buoc 0.01 thi
// gia tri CO doi (10.00 -> 10.01) nhung man hinh van hien "10.0" => tuong nhu nut khong co tac dung
// (phan hoi nguoi dung 2026-10-15: "nut chon 0.01 trong setting chua co hieu luc"). Nay khi dang sua dong
// nao thi dong do hien DU so thap phan theo buoc dang chon (xem se_fmt_id()).
static const uint8_t OV_STEP_DEC[4] = { 2, 1, 0, 0 };

static lv_obj_t *ov_bd, *ov_box, *ov_name, *ov_val, *ov_hint, *ov_minus;
static lv_obj_t *ov_step_btn[4];
static uint8_t  ov_step_k = 0;   // muc buoc dang chon (0..3)
static uint8_t  ov_i = 0;        // chi so THAT trong SET_ROWS[] (khong phai chi so dong dang hien)
static int32_t  ov_raw = 0;      // gia tri dang sua (raw = gia tri hien thi * scale)
static bool     ov_open = false;
static bool     ov_rep = false;  // da nhay do GIU nut -> bo qua lan CLICKED luc nhac tay

// 1 don vi HIEN THI cuoi cung cua dong (raw) = scale / 10^dec
static int32_t ov_quantum(const uint8_t i) {
  const uint8_t  dec = (SET_ROWS[i].dec > 3) ? 3 : SET_ROWS[i].dec;
  const uint32_t sc  = SET_ROWS[i].scale ? SET_ROWS[i].scale : 1;
  const uint32_t q   = sc / OV_TEN_POW[dec];
  return (int32_t)(q ? q : 1);
}

// Buoc ap dung (raw) = buoc dang chon * scale / 100, toi thieu 1 raw.
// SUA 2026-10-15 (loi nguoi dung bao: "nut chon 0.01 trong setting chua co hieu luc, khi tang giam van la
// 0.1"): truoc day ham nay con chan `(st > q) ? st : q` voi q = 1 don vi hien thi cuoi cua dong -> dong
// nao hien 1 chu so thap phan (dec=1) thi buoc 0.01 (raw = scale/100 = 10) bi NANG len q = 100 = 0.1 =>
// bam 0.01 nhung van nhay 0.1. Nay tra ve DUNG buoc da chon (loai bo chan nay); phan "nhin thay doi" duoc
// xu ly bang cach tang so chu so thap phan khi hien/sua (OV_STEP_DEC + se_fmt_id()).
static int32_t ov_step(const uint8_t i) {
  const uint32_t sc = SET_ROWS[i].scale ? SET_ROWS[i].scale : 1;
  const int32_t  st = (int32_t)(((int64_t)OV_STEP100[ov_step_k] * (int64_t)sc) / 100);
  return st > 0 ? st : 1;
}

// Muc buoc gan nhat voi 10^(-dec) cua dong (dec 2/3 -> 0.01 - dec 1 -> 0.1 - dec 0 -> 1)
static uint8_t ov_step_default(const uint8_t i) {
  const uint8_t dec = (SET_ROWS[i].dec > 3) ? 3 : SET_ROWS[i].dec;
  return (dec >= 2) ? 0 : (uint8_t)(2 - dec);
}

// To nut buoc dang chon (giong cach jg_axis[] to COL_ACCENT).
// Cung quy uoc voi jg_axis[] man JOG: "dang chon" = NEN COL_ACCENT, "focus" = VIEN (focus_mark
// bo qua doi nen voi vat co LV_OBJ_FLAG_USER_1 -> focus roi di KHONG tra nen nut ve COL_BTN).
static void ov_marks(void) {
  for (uint8_t k = 0; k < 4; k++) {
    if (!ov_step_btn[k]) continue;
    const lv_color_t want = (k == ov_step_k) ? COL_ACCENT : COL_BTN;
    // Chi set khi mau THAT SU doi: ham nay duoc goi moi chu ky cap nhat cua man SETTINGS
    // (400 ms) va lv_obj_set_style_bg_color() luon invalidate vat -> goi vo dieu kien se
    // lam lop phu nhap nhay va ton bang thong SPI.
    if (lv_obj_get_style_bg_color(ov_step_btn[k], 0).full != want.full)
      lv_obj_set_style_bg_color(ov_step_btn[k], want, 0);
  }
}

// Tim dong khai bao cua 1 id (de in so dung so le nhu bang dang hien)
static const set_row_t *se_find(const uint16_t id) {
  for (uint8_t i = 0; i < SET_NROWS; i++)
    if (SET_ROWS[i].id == id) return &SET_ROWS[i];
  return NULL;
}

static void se_fmt_id(char *b, size_t n, const uint16_t id, const int32_t raw) {
  const set_row_t *r = se_find(id);
  uint8_t dec = r ? r->dec : 2;
  // Dang SUA dong nay va buoc dang chon MIN HON so thap phan cua dong -> hien du so thap phan theo buoc,
  // neu khong nguoi dung bam 0.01 ma man hinh van lam tron (xem OV_STEP_DEC).
  if (r && ov_open && id == SET_ROWS[ov_i].id && OV_STEP_DEC[ov_step_k] > dec)
    dec = OV_STEP_DEC[ov_step_k];
  fmt_scaled(b, n, raw, r ? r->scale : 1000, dec);
}

// Ky tu truc cho nhom id LIEN TIEP X Y Z E (tra 0 neu id khong thuoc nhom).
// n = so phan tu cua nhom (4 cho X/Y/Z/E, 3 cho probe X/Y/Z) - phai kiem tra dung so
// luong vi cac nhom nay nam lien nhau trong enum cydlcd_setting_t.
static char se_axis_char(const uint16_t id, const uint16_t first, const uint8_t n) {
  const uint16_t d = (uint16_t)(id - first);
  return (d < n) ? "XYZE"[d] : (char)0;
}

// Gui M-code ung voi 1 dong (dung gia tri 'raw' dang sua). false = id khong ho tro sua.
static bool se_send_mcode(const uint8_t i, const int32_t raw) {
  const uint16_t id = SET_ROWS[i].id;
  char v[16], a[16], b[16], c[16], cmd[64];
  char ax;

  se_fmt_id(v, sizeof(v), id, raw);

  if ((ax = se_axis_char(id, CYDLCD_SET_STEPS_MM_X, 4)) != 0)
    snprintf(cmd, sizeof(cmd), "M92 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_MAXFEED_X, 4)) != 0)
    snprintf(cmd, sizeof(cmd), "M203 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_MAXACCEL_X, 4)) != 0)
    snprintf(cmd, sizeof(cmd), "M201 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_JERK_X, 4)) != 0)
    snprintf(cmd, sizeof(cmd), "M205 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_PROBE_OFFSET_X, 3)) != 0)
    snprintf(cmd, sizeof(cmd), "M851 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_TMC_CURRENT_X, 4)) != 0)   // P8: M906 X/Y/Z/E <mA>
    snprintf(cmd, sizeof(cmd), "M906 %c%s", ax, v);
  else if (id == CYDLCD_SET_MAXACCEL_PRINT)      // P8: M204 P (gia toc in)
    snprintf(cmd, sizeof(cmd), "M204 P%s", v);
  else if (id == CYDLCD_SET_MAXACCEL_RETRACT)    //     M204 R (gia toc lui dao)
    snprintf(cmd, sizeof(cmd), "M204 R%s", v);
  else if (id == CYDLCD_SET_MAXACCEL_TRAVEL)     //     M204 T (gia toc di chuyen)
    snprintf(cmd, sizeof(cmd), "M204 T%s", v);
  else if (id == CYDLCD_SET_FEEDRATE_PCT)
    snprintf(cmd, sizeof(cmd), "M220 S%s", v);
  else if (id == CYDLCD_SET_FLOW_PCT)
    snprintf(cmd, sizeof(cmd), "M221 S%s", v);
  else if (id == CYDLCD_SET_TARGET_TEMP_E0)
    snprintf(cmd, sizeof(cmd), "M104 S%s", v);
  else if (id == CYDLCD_SET_TARGET_TEMP_BED)
    snprintf(cmd, sizeof(cmd), "M140 S%s", v);
  else if (id == CYDLCD_SET_FADE_HEIGHT)
    // M420 Z<mm> = chieu cao fade cua bed leveling (planner.z_fade_height). 0 = tat fade.
    // Luu y: lenh nay nam TRUOC cac nhanh se_axis_char() o tren trong chuoi if/else -> khong dung nhau
    // (CYDLCD_SET_FADE_HEIGHT = 42, ngoai moi khoang [first, first+n) cua cac nhom truc).
    snprintf(cmd, sizeof(cmd), "M420 Z%s", v);
  else if (id >= CYDLCD_SET_MPC_P && id <= CYDLCD_SET_MPC_H) {
    // 6 tham so MPC: gui RIENG tham so dang sua (`M306 P<v>` ... `M306 H<v>`) - Marlin cho phep gui thieu
    // (M306.cpp: parser.seenval tung chu cai). Thu tu chu cai P C R A F H khop enum CYDLCD_SET_MPC_*.
    static const char MPC_LET[6] = { 'P', 'C', 'R', 'A', 'F', 'H' };
    snprintf(cmd, sizeof(cmd), "M306 %c%s", MPC_LET[id - CYDLCD_SET_MPC_P], v);
  }
  // ---- 2026-10-15: 3 nhom them cho day du nhu M503 ----
  else if (id == CYDLCD_SET_HOME_OFFSET_X) snprintf(cmd, sizeof(cmd), "M206 X%s", v);
  else if (id == CYDLCD_SET_HOME_OFFSET_Y) snprintf(cmd, sizeof(cmd), "M206 Y%s", v);
  else if (id == CYDLCD_SET_FILAMENT_DIA) {
    // !!! PHAI gui kem `S<trang thai hien tai>`: `M200 D<v>` mot minh se TU BAT bu bang the tich
    // (M200-M205.cpp: "Dn = enable for compatibility") -> luong ep doi => hong ban in.
    const int32_t vol = cydlcd_setting(CYDLCD_SET_FILAMENT_VOL, 0);
    snprintf(cmd, sizeof(cmd), "M200 D%s S%ld", v, (long)(vol ? 1 : 0));
  }
  else if (id == CYDLCD_SET_FILAMENT_VOL)
    // 0/1 (M200 S). Gia tri raw cua dong nay da la 0/1 (scale 1) -> gui thang, chan ve 0/1.
    snprintf(cmd, sizeof(cmd), "M200 S%ld", (long)(raw ? 1 : 0));
  else if (id == CYDLCD_SET_FC_LOAD)   snprintf(cmd, sizeof(cmd), "M603 L%s", v);
  else if (id == CYDLCD_SET_FC_UNLOAD) snprintf(cmd, sizeof(cmd), "M603 U%s", v);
  else if (id == CYDLCD_SET_FAN_PCT_E0) {
    // % -> 0..255 (lam tron). raw o day da nhan 100 (CYDLCD_PCT_SCALE)
    int32_t s = (raw * 255 + 5000) / 10000;
    if (s < 0) s = 0; else if (s > 255) s = 255;
    snprintf(cmd, sizeof(cmd), "M106 S%ld", (long)s);
  }
  else if (id >= CYDLCD_SET_PID_E_KP && id <= CYDLCD_SET_PID_E_KD) {
    // M301 P/I/D: gui CA 3 gia tri, cho nao dang sua thi lay gia tri moi
    int32_t kp = cydlcd_setting(CYDLCD_SET_PID_E_KP, 0);
    int32_t ki = cydlcd_setting(CYDLCD_SET_PID_E_KI, 0);
    int32_t kd = cydlcd_setting(CYDLCD_SET_PID_E_KD, 0);
    if (id == CYDLCD_SET_PID_E_KP) kp = raw;
    else if (id == CYDLCD_SET_PID_E_KI) ki = raw;
    else kd = raw;
    se_fmt_id(a, sizeof(a), CYDLCD_SET_PID_E_KP, kp);
    se_fmt_id(b, sizeof(b), CYDLCD_SET_PID_E_KI, ki);
    se_fmt_id(c, sizeof(c), CYDLCD_SET_PID_E_KD, kd);
    snprintf(cmd, sizeof(cmd), "M301 P%s I%s D%s", a, b, c);
  }
  else if (id >= CYDLCD_SET_PID_BED_KP && id <= CYDLCD_SET_PID_BED_KD) {
    int32_t kp = cydlcd_setting(CYDLCD_SET_PID_BED_KP, 0);
    int32_t ki = cydlcd_setting(CYDLCD_SET_PID_BED_KI, 0);
    int32_t kd = cydlcd_setting(CYDLCD_SET_PID_BED_KD, 0);
    if (id == CYDLCD_SET_PID_BED_KP) kp = raw;
    else if (id == CYDLCD_SET_PID_BED_KI) ki = raw;
    else kd = raw;
    se_fmt_id(a, sizeof(a), CYDLCD_SET_PID_BED_KP, kp);
    se_fmt_id(b, sizeof(b), CYDLCD_SET_PID_BED_KI, ki);
    se_fmt_id(c, sizeof(c), CYDLCD_SET_PID_BED_KD, kd);
    snprintf(cmd, sizeof(cmd), "M304 P%s I%s D%s", a, b, c);
  }
  else if (id > CYDLCD_PRE_ID_BASE && id <= (uint16_t)(CYDLCD_PRE_ID_BASE + CYDLCD_PRE_COUNT * 3)) {
    // PREHEAT preset: KHONG gui M-code nao (Marlin khong co M145 trong build nay) - ghi vao NVS
    // CUA MAN HINH CYD roi bom lai vao bang de bang + nut o man TEMP thay gia tri moi ngay.
    // Gia tri doc/ghi qua API CONG KHAI (ui_preheat_get/ui_preheat_set): chi ui_preheat.cpp giu mang s_pre_*.
    const uint8_t k   = (uint8_t)(id - CYDLCD_PRE_ID_BASE - 1);   // 0..8
    const uint8_t pre = (uint8_t)(k / 3);                        // 0..2 (PLA/PETG/ABS)
    const uint8_t fld = (uint8_t)(k % 3);                        // 0 hotend, 1 bed, 2 fan
    uint16_t h = 0, b = 0;                                       // 2 truong kia giu nguyen gia tri cu
    uint8_t  fan = 0;
    ui_preheat_get(pre, &h, &b, &fan);
    int32_t v = raw;
    if (fld == 2) {
      if (v < 0) v = 0; else if (v > PRE_F_MAX) v = PRE_F_MAX;
      fan = (uint8_t)v;
    } else {
      const int32_t mx = (fld == 0) ? PRE_H_MAX : PRE_B_MAX;
      v = v / 100;                                     // raw = do C * 100 (scale 100) -> do C
      if (v < 0) v = 0; else if (v > mx) v = mx;
      if (fld == 0) h = (uint16_t)v; else b = (uint16_t)v;
    }
    ui_preheat_set(pre, h, b, fan);                    // chan gia tri + luu NVS + bom lai bang + nhan nut TEMP
    WIFILOG("[PRE] %s: nozzle=%u bed=%u fan=%u (da luu NVS)\n", ui_preheat_label(pre),
            (unsigned)h, (unsigned)b, (unsigned)fan);
    return true;
  }
  else return false;      // id khac (Junction dev / Linear adv...) -> khong gui gi

  cydlcd_link_send_gcode(cmd);
  return true;
}

// Hien lai gia tri dang sua + buoc dang ap dung.
// Chu co dau duoc viet bang MA UTF-8 (\x..) de file nguon van la ASCII thuan - xem ghi chu
// "font tieng Viet" o dau file. Hai chuoi o day la "Gia tri: <v>" va "Buoc <buoc>".
static void ov_refresh(void) {
  char a[16], b[40];
  fmt_scaled(a, sizeof(a), ov_raw, SET_ROWS[ov_i].scale, SET_ROWS[ov_i].dec);
  snprintf(b, sizeof(b), "Gi\xC3\xA1 tr\xE1\xBB\x8B: %s", a);     // "Gia tri: <v>" (co dau)
  set_text(ov_val, b);
  fmt_scaled(a, sizeof(a), ov_step(ov_i), SET_ROWS[ov_i].scale, SET_ROWS[ov_i].dec);
  snprintf(b, sizeof(b), "B\xC6\xB0\xE1\xBB\x9B\x63 %s", a);      // "Buoc <buoc>" (co dau)
  set_text(ov_hint, b);
}

void ov_hide(void) {
  ov_open = false;
  ov_rep  = false;
  if (ov_bd)  lv_obj_add_flag(ov_bd, LV_OBJ_FLAG_HIDDEN);
  if (ov_box) lv_obj_add_flag(ov_box, LV_OBJ_FLAG_HIDDEN);
}

static void ov_close(void) {
  if (!ov_open) return;
  ov_hide();
  refocus_page();                     // tra focus ve nut dau tien cua man SETTINGS
}

static void cb_se_ov_adj(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  const int8_t d = (int8_t)(intptr_t)lv_event_get_user_data(e);
  if (code == LV_EVENT_PRESSED) { ov_rep = false; return; }
  if (code == LV_EVENT_LONG_PRESSED_REPEAT) { ov_rep = true; }   // GIU nut -> nhay lien tuc
  else if (ov_rep) { ov_rep = false; return; }                   // bo qua CLICKED luc nhac tay
  if (!ov_open) return;

  const int32_t st = ov_step(ov_i);
  int64_t nv = (int64_t)ov_raw + (int64_t)d * (int64_t)st;
  if (nv >  (int64_t)OV_RAW_LIM) nv =  (int64_t)OV_RAW_LIM;
  if (nv < -(int64_t)OV_RAW_LIM) nv = -(int64_t)OV_RAW_LIM;
  ov_raw = (int32_t)nv;
  ov_refresh();
}

static void cb_se_ov_ok(lv_event_t *e) {
  (void)e;
  if (!ov_open) return;
  const uint8_t i = ov_i;
  const int32_t v = ov_raw;
  ov_hide();
  se_send_mcode(i, v);                                  // id khong ho tro -> khong gui gi
  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);      // xin lai de bang tu khop gia tri
  refocus_page();
}

static void cb_se_ov_cancel(lv_event_t *e) { (void)e; ov_close(); }

// Chon buoc: to lai nut dang chon + cap nhat dong "Buoc ..." (khong doi gia tri dang sua)
static void cb_se_ov_step(lv_event_t *e) {
  const uint8_t k = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (k > 3) return;
  ov_step_k = k;
  ov_marks();
  if (ov_open) ov_refresh();
}

static void ov_create(void) {
  lv_obj_t *scr = lv_scr_act();

  // Nen mo: nuot moi lan cham ra NGOAI khung (khong cho roi xuong bang ben duoi)
  ov_bd = lv_obj_create(scr);
  lv_obj_set_pos(ov_bd, 0, CONT_Y);
  lv_obj_set_size(ov_bd, UI_LCD_HOR_RES, CONT_H);
  lv_obj_set_style_bg_color(ov_bd, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(ov_bd, LV_OPA_70, 0);
  lv_obj_set_style_border_width(ov_bd, 0, 0);
  lv_obj_set_style_radius(ov_bd, 0, 0);
  lv_obj_set_style_pad_all(ov_bd, 0, 0);
  lv_obj_clear_flag(ov_bd, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ov_bd, LV_OBJ_FLAG_HIDDEN);

  ov_box = lv_obj_create(scr);
  lv_obj_set_pos(ov_box, (UI_LCD_HOR_RES - OV_W) / 2, (lv_coord_t)(CONT_Y + (CONT_H - OV_H) / 2));
  lv_obj_set_size(ov_box, OV_W, OV_H);
  lv_obj_set_style_bg_color(ov_box, COL_BAR, 0);
  lv_obj_set_style_bg_opa(ov_box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(ov_box, COL_ACCENT, 0);
  lv_obj_set_style_border_width(ov_box, 1, 0);
  lv_obj_set_style_radius(ov_box, 3, 0);
  lv_obj_set_style_pad_all(ov_box, OV_PAD, 0);
  lv_obj_set_style_pad_row(ov_box, GAP, 0);
  lv_obj_clear_flag(ov_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(ov_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ov_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  // KHONG dat HIDDEN o day: neu tao con trong luc cha dang an thi LVGL khong tinh layout
  // (align/flex) cho nhanh do -> nut rong 0 px = "khong co text". ov_open_row() se hien sau.

  // Dong 1: ten thong so (ten tieng Anh lay tu SET_ROWS, khong co dau)
  ov_name = mk_label_w(ov_box, "-", MR_MENU, OV_W - 2 * OV_PAD);

  // Dong 2: "Gia tri: <so>" (xanh la) + "Buoc <buoc>" (xam) - xem ov_refresh()
  lv_obj_t *rv = mk_flex(ov_box, LV_FLEX_FLOW_ROW, OV_W - 2 * OV_PAD, OV_LINE_H, 0, GAP);
  ov_val  = mk_label_w(rv, "-", COL_OK, OV_VCAP);
  ov_hint = mk_label_w(rv, "", COL_DIM, OV_W - 2 * OV_PAD - OV_VCAP - GAP);
  vn_font(ov_val);              // 2 nhan nay co dau tieng Viet -> dung font Segoe
  vn_font(ov_hint);

  // Dong 3: 4 NUT BUOC co dinh - nut dang chon duoc to COL_ACCENT (nhu jg_axis[])
  lv_obj_t *rs = mk_flex(ov_box, LV_FLEX_FLOW_ROW, OV_W - 2 * OV_PAD, OV_BTN_H, 0, GAP);
  for (uint8_t k = 0; k < 4; k++)
    ov_step_btn[k] = mk_btn_grow(rs, OV_STEP_TXT[k], OV_BTN_H, cb_se_ov_step, OV_STEP(k));
  // 4 nut buoc TU QUAN LY NEN (LV_OBJ_FLAG_USER_1): giong 3 nut truc man JOG - focus_mark()
  // chi ve VIEN (mau sang MR_FRAME) cho chung, KHONG doi nen -> nen COL_ACCENT cua buoc dang
  // chon khong bi ghi de khi focus chuyen sang nut khac trong lop phu.
  for (uint8_t k = 0; k < 4; k++) lv_obj_add_flag(ov_step_btn[k], LV_OBJ_FLAG_USER_1);

  // Dong 4: - | + | OK | Huy
  lv_obj_t *rb = mk_flex(ov_box, LV_FLEX_FLOW_ROW, OV_W - 2 * OV_PAD, OV_BTN_H, 0, GAP);
  ov_minus = mk_btn_grow(rb, "-",   OV_BTN_H, cb_se_ov_adj, OV_ADJ(-1));
  lv_obj_t *plus = mk_btn_grow(rb, "+", OV_BTN_H, cb_se_ov_adj, OV_ADJ(+1));
  mk_btn_grow(rb, "OK",  OV_BTN_H, cb_se_ov_ok, NULL);
  lv_obj_t *huy = mk_btn_grow(rb, "H\xE1\xBB\xA7y", OV_BTN_H, cb_se_ov_cancel, NULL);  // "Huy"
  vn_font_btn(huy);             // chu co dau: chi doi font cho nhan BEN TRONG nut

  // Giu nut de tang nhanh: nhan them PRESSED + LONG_PRESSED_REPEAT cho 2 nut -/+
  lv_obj_t *adj[2] = { ov_minus, plus };
  const int8_t dir[2] = { -1, +1 };
  for (uint8_t k = 0; k < 2; k++) {
    lv_obj_add_event_cb(adj[k], cb_se_ov_adj, LV_EVENT_PRESSED, OV_ADJ(dir[k]));
    lv_obj_add_event_cb(adj[k], cb_se_ov_adj, LV_EVENT_LONG_PRESSED_REPEAT, OV_ADJ(dir[k]));
  }
}

// Mo lop phu cho 1 dong THAT trong SET_ROWS (i = chi so trong SET_ROWS[])
static void ov_open_edit(const uint8_t i) {
  if (i >= SET_NROWS) return;
  ov_i   = i;
  ov_raw = cydlcd_setting(SET_ROWS[ov_i].id, 0);
  if (!ov_box) ov_create();
  ov_step_k = ov_step_default(ov_i);      // buoc gan nhat voi 10^(-dec) cua dong
  set_text(ov_name, SET_ROWS[ov_i].name);
  ov_marks();
  ov_refresh();
  ov_open = true;
  ov_rep  = false;
  lv_obj_clear_flag(ov_bd, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ov_box, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(ov_box);           // ep tinh layout/flex ngay (khong doi vong refresh)
  if (s_group && ov_minus) lv_group_focus_obj(ov_minus);
}

// Mo lop phu cho dong dang HIEN thu k (0..se_n-1); chi so THAT lay qua se_idx[]
static void ov_open_row(const uint8_t k) {
  if (k >= se_n) return;
  ov_open_edit(se_idx[k]);
}

// P2: mo lop phu theo ID thong so (vi du nut "M851 Z" o man JOG) - khong can qua bang
void ov_open_id(const uint16_t id) {
  const set_row_t *r = se_find(id);
  if (r) ov_open_edit((uint8_t)(r - SET_ROWS));
}

// Cham 1 o cua bang -> mo lop phu (hang 0 la dong tieu de nen bo qua)
static void cb_se_cell(lv_event_t *e) {
  (void)e;
  uint16_t row = 0, col = 0;
  if (!se_table) return;
  lv_table_get_selected_cell(se_table, &row, &col);
  if (row < 1 || row > (uint16_t)se_n) return;
  ov_open_row((uint8_t)(row - 1));
}

void page_settings_create(lv_obj_t *p) {
  settings_build_list();
  se_info = mk_label_w(p, "Marlin gui: 0 thong so", COL_DIM, PAGE_INN_W);

  se_table = lv_table_create(p);
  lv_obj_set_size(se_table, PAGE_INN_W, SET_TABLE_H);
  lv_table_set_col_cnt(se_table, 2);
  lv_table_set_col_width(se_table, 0, SET_COL0_W);
  lv_table_set_col_width(se_table, 1, SET_COL1_W);
  lv_table_set_cell_value(se_table, 0, 0, "THONG SO");
  lv_table_set_cell_value(se_table, 0, 1, "GIA TRI");
  settings_fill_table();

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
  // Cham 1 o -> mo lop phu sua (-/+/OK). LVGL 8.3 gui VALUE_CHANGED khi NHAC tay (khong keo)
  lv_obj_add_event_cb(se_table, cb_se_cell, LV_EVENT_VALUE_CHANGED, NULL);

  // 1 HANG NUT o DAY bang, trai -> phai: UP | DOWN | SAVE EEPROM (2026-10-13)
  lv_obj_t *rb = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 30, 0, GAP);
  mk_btn_grow(rb, "UP",          28, cb_scroll_table, (void *)(intptr_t)(-48));
  mk_btn_grow(rb, "DOWN",        28, cb_scroll_table, (void *)(intptr_t)(48));
  mk_btn_grow(rb, "SAVE EEPROM", 28, cb_action, (void *)(intptr_t)A_SAVE);

  // M502 RESET: tra thong so trong RAM ve mac dinh cua firmware. XAC NHAN 2 BUOC (xem do_action).
  // Da BO nut "M501 LOAD" (2026-10-13, theo yeu cau): M501 chi doc lai thong so tu EEPROM vao RAM
  // (hoan tac thay doi chua luu) chu khong "load" tu file nao -> nhan "LOAD" gay hieu nham.
  lv_obj_t *r2 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 28, 0, GAP);
  mk_btn_grow(r2, "M502 RESET (ve mac dinh)", 28, cb_action, (void *)(intptr_t)A_EEPROM_RESET);
}

void page_settings_update(void) {
  char b[40], a[16];
  // 9 dong preheat do MAN HINH CYD tu quan ly (khong phai Marlin) -> bom lai truoc khi dung danh sach,
  // de sau M501/M502 (Marlin xoa bang) hay luc moi ket noi chung van con.
  preheat_inject();
  if (!se_table || !se_info) return;      // trang da bi thu hoi (an toan: xem page_settings_destroy)
  // Neu so thong so nhan duoc doi (HELLO moi / bam lai) -> dung lai danh sach dong co gia tri
  static uint8_t last_n = 255;
  if (last_n != cydlcd.n_settings) {
    last_n = cydlcd.n_settings;
    settings_build_list();
    settings_fill_table();
  }

  for (uint8_t k = 0; k < se_n; k++) {
    const uint8_t i = se_idx[k];
    const int32_t v = cydlcd_setting(SET_ROWS[i].id, INT32_MIN);
    fmt_scaled(a, sizeof(a), v, SET_ROWS[i].scale, SET_ROWS[i].dec);
    // chi set khi khac -> khong nhay + khong day SPI vo ich
    const char *old = lv_table_get_cell_value(se_table, (uint16_t)(k + 1), 1);
    if (!old || strcmp(old, a) != 0)
      lv_table_set_cell_value(se_table, (uint16_t)(k + 1), 1, a);
  }
  // Ghi ro cach dung: bang co the NHIEU dong hon khung nhin (~7 dong) -> phai VUOT de cuon; dong nao cung
  // cham duoc de sua. (Them 2026-10-15 sau khi nguoi dung tuong "mat setting fade height" - thuc ra dong do
  // nam duoi mep bang, phai cuon moi thay.)
  snprintf(b, sizeof(b), "vuot=cuon | cham=SUA | %u/%u thong so", (unsigned)se_n, (unsigned)SET_NROWS);
  set_text(se_info, b);

  // Lop phu SUA co the dang mo -> ve lai dau "buoc dang chon" moi chu ky (giong jog_axis_mark()
  // o man JOG): focus va "dang chon" la 2 trang thai doc lap, ov_marks() chi ghi khi mau doi.
  // Chay ca khi lop phu dang AN: ov_step_btn[] khong doi, khong ton gi them.
  ov_marks();
}

// CHAN DOAN (lenh `UI SET` tren COM8): in cac dong cua bang SETTINGS kem gia tri RAW + gia tri da format.
// VI SAO CAN: bang nam tren LCD, khong doc nguoc duoc tu PC - day la cach DUY NHAT kiem tra "Marlin co day
// thong so nay khong / man hinh dang hien gi" (vd khi moi them dong "Fade height mm").
void page_settings_dump(void) {
  preheat_inject();                       // 9 dong preheat do man hinh CYD tu quan ly
  // PHAI dung lai danh sach truoc khi in: `se_idx/se_n` chi duoc dien khi mo trang SETTINGS hoac khi so
  // thong so doi -> neu nguoi dung chua mo trang thi mang con rong (lan dau chay da in ra "0/49 dong").
  settings_build_list();
  Serial.printf("[UI] SETTINGS: %u/%u dong CO gia tri (tong bang %u, Marlin da gui %u thong so)\n",
                (unsigned)se_n, (unsigned)SET_NROWS, (unsigned)SET_NROWS, (unsigned)cydlcd.n_settings);
  char a[20];
  for (uint8_t k = 0; k < se_n; k++) {
    const uint8_t i = se_idx[k];
    const int32_t raw = cydlcd_setting(SET_ROWS[i].id, INT32_MIN);
    if (raw == INT32_MIN) snprintf(a, sizeof(a), "(khong co)");
    else                  fmt_scaled(a, sizeof(a), raw, SET_ROWS[i].scale, SET_ROWS[i].dec);
    Serial.printf("[UI]   id %2u  %-18s = %-12s (raw %ld)\n", (unsigned)SET_ROWS[i].id, SET_ROWS[i].name,
                  a, (long)raw);
  }
}

// Huy man SETTINGS (xem quy uoc o ui_lvgl_int.h): dat NULL bang + dong info. Container trang do
// show_page() xoa bang lv_obj_del() nen khong xoa widget o day.
//
// LOP PHU SUA (ov_*: ov_bd/ov_box/ov_name/ov_val/ov_hint/ov_minus/ov_step_btn[]) KHONG xoa o day:
// chung la con cua lv_scr_act() (khong phai con cua trang) nen khong bi xoa theo trang, va duoc tai
// dung cho lan mo SETTINGS sau. show_page() da goi ov_hide() TRUOC khi thu hoi trang (xem show_page)
// -> lop phu khong con hien tren trang khac (dung nhu hanh vi cu khi doi man).
// `se_n` / `se_idx[]`: GIU NGUYEN - page_settings_create() goi settings_build_list() lai tu dau.
void page_settings_destroy(void) {
  se_table = NULL;
  se_info  = NULL;
}

#endif  // CYDLCD_USE_LVGL

