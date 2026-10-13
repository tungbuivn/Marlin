/**
 * ui_lvgl.cpp -- UI LVGL 8.3/8.4 cho panel (du lieu lay tu frame PUSH cua Marlin, xem panel_link.h).
 *
 * BO CUC MAN STATUS: lay Y NGUYEN toa do tu UI TFT ILI9341 cua Marlin
 *   Marlin/src/lcd/tft/ui_320x240.cpp  (MarlinUI::draw_status_screen(), cung 320x240):
 *     - 3 o 64x100 tai x = 21 / 127 / 234   (x = (320/3 - 64)/2 + 320*i/3): dau in, ban in, quat
 *         target o y=8, "icon" o y=18 (Marlin dung bitmap 64x64), nhiet do hien tai o y=72
 *     - hang XYZ: khung (4, 103, 312, 24), nhan X/Y/Z trong khung (x = 10 / 127 / 219,
 *       gia tri can phai ket thuc o x = 68 / 185 / 301)
 *     - FR (70, 136, 80, 32) va FL (170, 136, 80, 32)
 *     - nut SD 64x64 o (0, 130)  va  nut SETTINGS 64x64 o (256, 130)  <- "nut setting ben phai"
 *     - thoi gian in (96, 176, 128, 20), thanh tien do (4, 198, 312, 9),
 *       dong thong bao/trang thai (0, 216, 320, 20)
 *     Mau theo Marlin/src/lcd/tft/tft_color.h: nen #1E156E, COLD/FAN aqua, HOTEND scarlet,
 *     HEATED_BED dark-orange, AXIS_HOMED trang, AXIS_NOT_HOMED vang, RATE_100 xanh la,
 *     RATE_ALTERED vang, PRINT_TIME aqua, PROGRESS_BAR xanh duong.
 *   Icon 64x64: DA PORT THAT tu bitmap GREYSCALE4 cua Marlin -> sinh ra ui_lvgl_icons.h bang
 *   tools/make_lvgl_icons.py (xem muc 5.2 trong lcd_panel/README.md), ve bang LVGL (LV_USE_IMG).
 *   (Ban cu chi ve "bang" khung + nhan NOZ/BED/FAN/SD/SET - nay KHONG dung nua.)
 *
 * Cac man khac (TEMP/JOG/FILES/SETTINGS/MESH/CUSTOM/INFO) nam duoi 1 MAN MENU kieu Marlin:
 *   STATUS --(nut SETTINGS)--> MENU --(chon dong)--> man con --(BACK)--> MENU --(BACK)--> STATUS.
 *   Cac o tren STATUS cung la nut: dau in/ban in/quat -> TEMP (co hang FAN), hang XYZ -> JOG,
 *   SD -> FILES, FR/FL -> SETTINGS, SETTINGS -> MENU.
 *   (Bo thanh nav 8 nut o duoi: no chiem 64 px va lam STATUS khac han man TFT cua Marlin.)
 *
 * Kien truc:
 *   - 1 screen duy nhat. STATUS dung kin 320x240 theo bo cuc Marlin (khong co dong tieu de).
 *     MOI MAN KHAC (ke ca MENU) deu nam duoi MOT dong tieu de chung o tren:
 *         [ ten man ] ............ [ LINK / NO LINK ]  [ BACK ]
 *     => nut BACK LUON o goc PHAI dong tieu de, khong ve BACK rieng tung man (do ton dien tich).
 *     BACK = di len 1 cap: man con -> MENU -> STATUS (man goc khong co BACK).
 *   - Moi man la 1 container (lv_obj) TAO 1 LAN khi man duoc mo lan dau, sau do chi
 *     an/hien (LV_OBJ_FLAG_HIDDEN) va cap nhat text. Khong tao lai widget moi vong lap.
 *   - Bo cuc bang FLEX (LV_USE_FLEX), khong dung toa do y cung: container = flex COLUMN,
 *     moi hang = flex ROW, nut can chia deu thi lv_obj_set_flex_grow(). Moi kich thuoc suy ra
 *     tu UI_LCD_HOR_RES/VER_RES + ROW_H (1 dong chu montserrat_14 = 16 px) nen doi man hinh
 *     chi phai xem lai cac hang so o duoi.
 *   - 1 lv_group chung cho encoder: LVGL 8.3 tu bo qua vat bi HIDDEN khi di chuyen focus
 *     (lv_group.c: "Hidden objects don't receive focus"), nen khong can group rieng cho
 *     tung man.
 *   - Nut bam: xoa co LV_OBJ_FLAG_SCROLLABLE de encoder BAM 1 LAN la an ngay
 *     (lv_indev.c: vat "editable or scrollable" thi lan bam dau chi vao che do edit).
 *
 * Cac man:
 *   STATUS  : THEO MARLIN ui_320x240 (nhiet dau in / ban in / quat + XYZ + FR/FL + SD + SETTINGS
 *             + thoi gian + thanh tien do + dong trang thai).
 *   MENU    : menu doc kieu Marlin, mo bang nut SETTINGS tren STATUS.
 *   TEMP    : +/- 1/10 do C cho nozzle & bed -> M104 S... / M140 S..., hang FAN -> M106 S..., COOLDOWN.
 *   JOG     : chon truc X/Y/Z, nut -0.1/+0.1/-1/+1/-10/+10 mm -> G91 + G1 <truc><d> F600 + G90,
 *             nut HOME ALL -> G28.
 *   FILES   : lv_roller danh sach panel.files[], PRINT (M23 + M24), DIR, UP, REFRESH,
 *             PAUSE (M25), RESUME (M24), STOP (M524).
 *   SETTINGS: bang thong so panel_setting(PANEL_SET_*) + SAVE EEPROM (PANEL_REQ_SAVE_SETTINGS).
 *   MESH    : bang luoi bed mesh (panel.mesh[col][row]), o cao = xanh, o thap = do,
 *             REFRESH -> PANEL_REQ_MESH; khong co PANEL_HAS_MESH thi bao "khong ho tro".
 *   CUSTOM  : 2 nut nhu menu cu cua Marlin: G34 Bed (G34 Q99) + Init SD (M22 roi M21).
 *   INFO    : ten firmware, proto, marlin_fw, so frame/loi, phien ban UI.
 *   TRAM    : Bed Tramming Wizard cua Marlin (4 so do lech + huong dan van vit + 4 nut
 *             PROBE / MAX<->MIN / BACK / DONE). KHONG dung frame PUSH: Marlin chi can bat
 *             #define PANEL_TRAM_LINK la du (link ASCII tren USART1 - xem tram_link.h), nho vay
 *             man 12864 + menu MarlinUI (Tramming Wizard, Z ALIGN MANUAL...) VAN CON NGUYEN.
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
#include "tram_link.h"              // Bed Tramming Wizard cua Marlin (link ASCII, xem tram_link.h)
#include "ui_lvgl_icons.h"          // icon 64x64/32x32 chuyen tu bitmap cua Marlin

// Port day pixel ra LCD lam viec truc tiep tren buffer 16-bit (xem ui_lvgl_port.cpp)
static_assert(LV_COLOR_DEPTH == 16, "UI nay chi ho tro LV_COLOR_DEPTH 16");
static_assert(sizeof(lv_color_t) == 2, "lv_color_t phai la 2 byte");

// ===========================================================================
//  Mau -- DONG NHAT theo Marlin/src/lcd/tft/tft_color.h (moi man dung cung bang mau)
// ===========================================================================
#define MR_BG      lv_color_hex(0x1E156E)   // COLOR_BACKGROUND
#define MR_COLD    lv_color_hex(0x00FFFF)   // COLOR_COLD / COLOR_FAN (aqua)
#define MR_HOTEND  lv_color_hex(0xFF2020)   // COLOR_HOTEND (scarlet)
#define MR_BEDHOT  lv_color_hex(0xFF8C00)   // COLOR_HEATED_BED (dark orange)
#define MR_AXIS    lv_color_hex(0xFFFFFF)   // COLOR_AXIS_HOMED / COLOR_CONTROL_ENABLED
#define MR_NOAXIS  lv_color_hex(0xFFFF00)   // COLOR_AXIS_NOT_HOMED / COLOR_MENU_TEXT
#define MR_RATE100 lv_color_hex(0x7FFF00)   // COLOR_RATE_100 (vivid green)
#define MR_RATEALT lv_color_hex(0xFFFF00)   // COLOR_RATE_ALTERED
#define MR_TIME    lv_color_hex(0x00FFFF)   // COLOR_PRINT_TIME
#define MR_MSG     lv_color_hex(0xFFFF00)   // COLOR_STATUS_MESSAGE
#define MR_PROGBAR lv_color_hex(0x0000FF)   // COLOR_PROGRESS_BAR
#define MR_PROGBG  lv_color_hex(0x000000)   // COLOR_PROGRESS_BG
#define MR_FRAME   lv_color_hex(0xFFFFFF)   // COLOR_PROGRESS_FRAME
#define MR_MENU    lv_color_hex(0xFFFF00)   // COLOR_MENU_TEXT
#define MR_MENUVAL lv_color_hex(0xFFFFFF)   // COLOR_MENU_VALUE
#define MR_SELBG   lv_color_hex(0x992380)   // COLOR_SELECTION_BG
#define MR_DISABLED lv_color_hex(0x808080)  // COLOR_CONTROL_DISABLED

// Cac man con dung cung bang mau nay (khong con theme rieng)
#define COL_BG     MR_BG
#define COL_BAR    lv_color_hex(0x160C52)   // dong tieu de: dam hon nen
#define COL_TXT    MR_MENUVAL
#define COL_DIM    lv_color_hex(0x9AA7E8)   // chu phu (xanh nhat)
#define COL_BTN    lv_color_hex(0x2A2080)   // nen nut
#define COL_ACCENT MR_COLD                   // focus / diem nhan
#define COL_WARN   MR_RATEALT
#define COL_ERR    MR_HOTEND
#define COL_OK     MR_RATE100

// ===========================================================================
//  Man hinh
// ===========================================================================
enum {
  PG_STATUS = 0, PG_MENU, PG_RATE, PG_TEMP, PG_JOG, PG_FILES, PG_SETTINGS, PG_MESH, PG_CUSTOM,
  PG_TRAM, PG_INFO,
  PG_COUNT
};

static const char *const PG_TITLE[PG_COUNT] = {"STATUS", "MENU", "FEED/FLOW", "NHIET DO", "JOG",
                                               "FILE SD", "THONG SO", "BED MESH", "CUSTOM",
                                               "TRAMMING", "INFO"};

// ===========================================================================
//  Hinh hoc 320x240
//  A) Man STATUS: toa do y nguyen tu Marlin/src/lcd/tft/ui_320x240.cpp
//  B) Cac man con: 1 dong tieu de (BAR_H) + noi dung flex (PAGE_INN_*)
// ===========================================================================
// --- A) STATUS (Marlin) ---
#define MR_W         UI_LCD_HOR_RES                       // 320
#define MR_H         UI_LCD_VER_RES                       // 240
#define MR_CELL_W    64                                   // o nhiet/quat: 64x100
#define MR_CELL_H    100
#define MR_CELL_X(i) ((lv_coord_t)(21 + 106 * (int)(i)))  // 3 o: 21 / 127 / 234
#define MR_TGT_Y     8                                    // target temp (Marlin: y=8)
#define MR_ICON_Y    18                                   // icon 64x64 (Marlin: add_image(0,18,..))
#define MR_ICON_SZ   64
#define MR_FAN_ICON_Y 10                                  // icon quat (Marlin: add_image(0,10,..))
#define MR_CUR_Y     72                                   // nhiet do hien tai (Marlin: y=72)
#define MR_RATE_ICON_Y 0                                  // icon FR/FL 32x32 (Marlin: add_image(0,0,..))
#define MR_XYZ_X     4                                    // canvas(4, 103, 312, 24)
#define MR_XYZ_Y     103
#define MR_XYZ_W     312
#define MR_XYZ_H     24
#define MR_XYZ_TY    3
#define MR_XYZ_LX(i) ((lv_coord_t)((i) == 0 ? 10 : ((i) == 1 ? 127 : 219)))
#define MR_XYZ_VW(i) ((lv_coord_t)((i) == 0 ? 58 : ((i) == 1 ? 58 : 82)))   // can phai -> 68 / 185 / 301
#define MR_RATE_Y    136                                  // canvas(70/170, 136, 80, 32)
#define MR_RATE_W    80
#define MR_RATE_H    32
#define MR_FR_X      70
#define MR_FL_X      170
#define MR_TILE_Y    130                                  // nut SD (0,130) va SETTINGS (256,130), 64x64
#define MR_TILE_SZ   64
#define MR_SD_X      0
#define MR_SET_X     256
#define MR_TIME_X    96                                   // canvas(96, 176, 128, 20)
#define MR_TIME_Y    176
#define MR_TIME_W    128
#define MR_PCT_X     226                                  // % tien do (cho trong 224..256)
#define MR_PCT_W     30
#define MR_BAR_X     4                                    // canvas(4, 198, 312, 9)
#define MR_BAR_Y     198
#define MR_BAR_W     312
#define MR_BAR_H     9
#define MR_MSG_X     0                                    // canvas(0, 216, 320, 20)
#define MR_MSG_Y     216
#define MR_MSG_W     320

// --- B) cac man con ---
#define BAR_H      26                                  // dong tieu de: [ten man] ... [LINK][BACK]
#define CONT_Y     BAR_H
#define CONT_H     (UI_LCD_VER_RES - BAR_H)             // 214
#define PAGE_PAD   4                                    // padding trong vung noi dung
#define PAGE_INN_W (UI_LCD_HOR_RES - 2 * PAGE_PAD)      // 312: be rong dung duoc
#define PAGE_INN_H (CONT_H - 2 * PAGE_PAD)              // 206: TONG chieu cao moi man phai <= so nay
#define ROW_H      16                                   // 1 dong chu montserrat_14
#define GAP        2                                    // khoang cach giua cac hang / doi tuong

// --- C) man MENU (menu doc kieu Marlin; cac dong do FLEX cua mk_page tu xep doc) ---
#define MENU_ROW_H  22                                  // 20 px mo dong + 2 px khoang

static lv_obj_t  *s_page[PG_COUNT];        // container noi dung tung man (NULL = chua tao)
static lv_obj_t  *s_bar;                   // dong tieu de chung: [ten man] ... [LINK][BACK] (an o STATUS)
static lv_obj_t  *s_title, *s_link;
static lv_group_t *s_group;
static lv_obj_t  *s_focus_prev;            // doi tuong dang focus (de ve khung)
static uint8_t    s_page_act = PG_STATUS;
static uint8_t    s_link_state = 255;      // 255 = chua set lan nao (`link_state_now()` tra 0/1/2)
static uint32_t   s_tick_ms, s_upd_ms, s_slow_ms;

// "Co link voi Marlin" - CO HAI giao thuc LOAI TRU nhau (PUSH: panel.linked / TRAM: tram.linked)
// nen phai xet CA HAI. Dung ham nay o MOI cho hien thi link (thanh tren + trang STATUS).
// Chinh sach hien thi (2026-10-13, theo yeu cau): chi bao link chi nam o
//   1) THANH TREN (moi man), va 2) dong trang thai + khung XYZ cua trang STATUS (home).
// Cac man khac KHONG hien lai nua (truoc day man TRAMMING va MENU cung hien -> nhieu cho mau thuan).
static bool link_ok(void) { return panel.linked || tram.linked; }

// Trang thai link 3 muc - "co byte toi" KHONG du de coi la link tot (co the la byte rac do sai baud/
// nhieu/day loi), nhung no phan biet duoc 2 kieu hong khac nhau:
//   0 = NO LINK  : khong co byte nao  -> dut day / sai cong / board tat
//   1 = LINK OK  : doc duoc dong "TRAM ..." hop le
//   2 = LINK ERR : CO byte toi nhung khong thanh dong hop le -> sai baud / sai khung / nhieu
static uint8_t link_state_now(void) {
  if (link_ok()) return 1;
  return tram.rx_active ? 2 : 0;
}

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

// Nhan 1 dong: rong theo noi dung (dat trong flex thi layout tu xep cho)
static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, lv_color_t col) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_color(l, col, 0);
  lv_label_set_text(l, txt);
  return l;
}

// Nhan 1 dong, be rong CO DINH: chu dai hon thi tu cuon vong (khong bi cat cut)
static lv_obj_t *mk_label_w(lv_obj_t *parent, const char *txt, lv_color_t col, lv_coord_t w) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_width(l, w);
  lv_label_set_long_mode(l, LV_LABEL_LONG_SCROLL_CIRCULAR);
  return l;
}

// Container trong suot de xep con bang FLEX.
//   flow = LV_FLEX_FLOW_COLUMN (xep doc) hoac LV_FLEX_FLOW_ROW (xep ngang)
//   pad  = padding trong; gap = khoang cach giua cac con (ca 2 chieu)
// Hang ROW can giua theo chieu doc (nut thap hon hang van nam giua).
static lv_obj_t *mk_flex(lv_obj_t *parent, lv_flex_flow_t flow, lv_coord_t w, lv_coord_t h,
                         lv_coord_t pad, lv_coord_t gap) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, pad, 0);
  lv_obj_set_style_pad_row(o, gap, 0);
  lv_obj_set_style_pad_column(o, gap, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(o, flow);
  // Hang ROW: can giua theo chieu doc (nut thap hon hang van nam giua hang).
  lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START,
                        (flow == LV_FLEX_FLOW_ROW) ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                        (flow == LV_FLEX_FLOW_ROW) ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START);
  return o;
}

// Vat dem trong suot: grow > 0 thi chiem het cho trong con lai cua hang flex
// (dung de day cac vat khac ve mot phia).
static lv_obj_t *mk_spacer(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, uint8_t grow) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  if (grow) lv_obj_set_flex_grow(o, grow);
  return o;
}

static void focus_mark(lv_obj_t *o, const bool on) {
  if (!o) return;
  lv_obj_set_style_border_width(o, on ? 2 : 0, 0);
  lv_obj_set_style_border_color(o, COL_ACCENT, 0);
  // Vat co nen dac (nut, dong menu, bang) -> doi sang COLOR_SELECTION_BG nhu dong dang chon cua Marlin
  if (lv_obj_get_style_bg_opa(o, 0) == LV_OPA_COVER)
    lv_obj_set_style_bg_color(o, on ? MR_SELBG : COL_BTN, 0);
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
static lv_obj_t *mk_btn(lv_obj_t *parent, const char *txt, lv_coord_t w, lv_coord_t h,
                        lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_btn_create(parent);
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

// Nut chia deu be rong trong 1 hang flex (khong can biet truoc be rong tung nut)
static lv_obj_t *mk_btn_grow(lv_obj_t *parent, const char *txt, lv_coord_t h,
                             lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = mk_btn(parent, txt, 0, h, cb, ud);
  lv_obj_set_flex_grow(b, 1);
  return b;
}

// Vung BAM trong suot co toa do tuyet doi (man STATUS/MENU khong dung flex).
// cb == NULL: khong vao group (dung cho "bang icon" nam trong 1 vung bam khac).
static lv_obj_t *mk_hit(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                        lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  if (cb) {
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    if (s_group) lv_group_add_obj(s_group, b);
  }
  else lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);     // de click roi xuong vung bam cha
  return b;
}

// Icon cua Marlin (LVGL ALPHA_4BIT): mau ve bang recolor -> doi theo trang thai
static lv_obj_t *mk_icon(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const lv_img_dsc_t *src,
                         lv_color_t col) {
  lv_obj_t *im = lv_img_create(parent);
  lv_img_set_src(im, src);
  lv_obj_set_pos(im, x, y);
  lv_obj_clear_flag(im, LV_OBJ_FLAG_CLICKABLE);       // click roi xuong vung bam cha
  lv_obj_clear_flag(im, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_img_recolor(im, col, 0);
  lv_obj_set_style_img_recolor_opa(im, LV_OPA_COVER, 0);
  return im;
}

// Doi mau icon (Marlin doi mau bitmap theo nhiet do / trang thai)
static void icon_color(lv_obj_t *im, const lv_color_t col) {
  if (!im) return;
  lv_obj_set_style_img_recolor(im, col, 0);
  lv_obj_set_style_img_recolor_opa(im, LV_OPA_COVER, 0);
}

// Nhan 1 dong, can phai trong be rong w (Marlin can so theo toa do x phai)
static lv_obj_t *mk_label_right(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                                const char *txt, lv_color_t col) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
  return l;
}

// Nhan 1 dong can giua trong be rong w
static lv_obj_t *mk_label_mid(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                              const char *txt, lv_color_t col) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

// Man kin 320x240, KHONG flex (con dat bang toa do tuyet doi) + khong padding.
// Nen dung COLOR_BACKGROUND cua Marlin (2 man nay theo bo cuc Marlin).
static lv_obj_t *mk_page_full(void) {
  lv_obj_t *p = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(p, 0, 0);
  lv_obj_set_size(p, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  lv_obj_set_style_bg_color(p, MR_BG, 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_radius(p, 0, 0);
  lv_obj_set_style_pad_all(p, 0, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  return p;
}

// Focus vao vat bam duoc dau tien trong 1 nhanh (de encoder bat dau o vat dau man moi)
static bool focus_first_in(lv_obj_t *o) {
  const uint32_t n = lv_obj_get_child_cnt(o);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_CLICK_FOCUSABLE) && !lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) {
      lv_group_focus_obj(c);
      return true;
    }
    if (focus_first_in(c)) return true;
  }
  return false;
}

// Container noi dung cua 1 man: flex COLUMN, padding PAGE_PAD, cach nhau GAP.
// Con nao muon an het cho trong con lai thi them lv_obj_set_flex_grow(con, 1).
static lv_obj_t *mk_page(lv_obj_t *parent) {
  lv_obj_t *p = lv_obj_create(parent);
  lv_obj_set_pos(p, 0, CONT_Y);
  lv_obj_set_size(p, UI_LCD_HOR_RES, CONT_H);
  lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_radius(p, 0, 0);
  lv_obj_set_style_pad_all(p, PAGE_PAD, 0);
  lv_obj_set_style_pad_row(p, GAP, 0);
  lv_obj_set_style_pad_column(p, GAP, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  return p;
}

// ===========================================================================
//  Hanh dong (G-code / request)
// ===========================================================================
enum {
  A_NONE = 0,
  A_BACK, A_PAUSE, A_RESUME, A_STOP, A_SAVE, A_INITSD, A_G34, A_HOME,
  A_REQ_FILES, A_REQ_UP, A_REQ_MESH, A_REQ_SETTINGS, A_REQ_FULL, A_COOLDOWN,
  A_GOTO_MENU, A_GOTO_TEMP, A_GOTO_JOG, A_GOTO_FILES, A_GOTO_SETTINGS, A_GOTO_RATE
};

// user_data cua nut: (heater << 8) | (int8)delta
#define TEMP_ADJ(h, d) ((void *)(intptr_t)(((unsigned)(h) << 8) | (unsigned)(uint8_t)(int8_t)(d)))

static void show_page(uint8_t p);            // forward
static void create_page(uint8_t p);          // forward
static void files_rebuild(void);             // forward

static void cb_action(lv_event_t *e) {
  const int a = (int)(intptr_t)lv_event_get_user_data(e);
  switch (a) {
    // BACK: tu man con -> MENU, tu MENU -> STATUS (giong "up" trong menu Marlin)
    case A_BACK:          show_page(s_page_act == PG_MENU ? PG_STATUS : PG_MENU); break;
    case A_GOTO_MENU:     show_page(PG_MENU); break;
    case A_GOTO_TEMP:     show_page(PG_TEMP); break;
    case A_GOTO_JOG:      show_page(PG_JOG); break;
    case A_GOTO_FILES:    show_page(PG_FILES); break;
    case A_GOTO_SETTINGS: show_page(PG_SETTINGS); break;
    case A_GOTO_RATE:     show_page(PG_RATE); break;
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
//  STATUS -- TOA DO Y NGUYEN tu Marlin/src/lcd/tft/ui_320x240.cpp
//   3 o nhiet/quat 64x100 tai x = 21/127/234 + hang XYZ + FR/FL + SD + SETTINGS
//   + thoi gian (96,176) + thanh tien do (4,198) + dong trang thai (0,216)
//   Icon 64x64 cua Marlin (bitmap GREYSCALE4) da port that vao ui_lvgl_icons.h.
// ===========================================================================
#define ST_CELLS 3                     // 0 = dau in (E0), 1 = ban in (BED), 2 = quat (FAN)

static lv_obj_t *st_cell[ST_CELLS], *st_icon[ST_CELLS];
static lv_obj_t *st_tgt[ST_CELLS], *st_cur[ST_CELLS];
static lv_obj_t *st_axis_val[3], *st_xyz_frame;
static lv_obj_t *st_rate_val[2], *st_rate_icon[2];
static lv_obj_t *st_time, *st_pct, *st_bar, *st_msg;

// 1 o 64x100: [target] / [icon 64x64] / [gia tri]; ca o la 1 nut (bam -> man tuong ung)
// Thu tu tao: nut -> ICON -> 2 nhan, de nhan ve DE LEN icon (giong Marlin: text ve sau image).
static void status_cell(lv_obj_t *p, const uint8_t i, const lv_img_dsc_t *icon, const uint8_t act) {
  lv_obj_t *c = mk_hit(p, MR_CELL_X(i), 0, MR_CELL_W, MR_CELL_H,
                       cb_action, (void *)(intptr_t)act);
  st_cell[i] = c;
  st_icon[i] = mk_icon(c, 0, MR_ICON_Y, icon, MR_COLD);
  st_tgt[i]  = mk_label_mid(c, 0, MR_TGT_Y, MR_CELL_W, "-", MR_COLD);
  st_cur[i]  = mk_label_mid(c, 0, MR_CUR_Y, MR_CELL_W, "-", MR_COLD);
}

static void page_status_create(lv_obj_t *p) {
  // ---- 3 o: dau in (E0) / ban in (BED) / quat (FAN) ----
  status_cell(p, 0, &ui_icon_hotend, A_GOTO_TEMP);
  status_cell(p, 1, &ui_icon_bed,    A_GOTO_TEMP);
  status_cell(p, 2, &ui_icon_fan,    A_GOTO_TEMP);
  lv_obj_set_pos(st_icon[2], 0, MR_FAN_ICON_Y);        // Marlin ve icon quat o y=10

  // ---- hang XYZ: khung (4, 103, 312, 24) ----
  // Khung ve rieng (de doi mau theo LINK) + vung bam trong suot dat de len tren
  st_xyz_frame = lv_obj_create(p);
  lv_obj_set_pos(st_xyz_frame, MR_XYZ_X, MR_XYZ_Y);
  lv_obj_set_size(st_xyz_frame, MR_XYZ_W, MR_XYZ_H);
  lv_obj_clear_flag(st_xyz_frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(st_xyz_frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(st_xyz_frame, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(st_xyz_frame, 1, 0);
  lv_obj_set_style_border_color(st_xyz_frame, MR_AXIS, 0);
  lv_obj_set_style_radius(st_xyz_frame, 0, 0);
  lv_obj_set_style_pad_all(st_xyz_frame, 0, 0);

  lv_obj_t *xyz_hit = mk_hit(p, MR_XYZ_X, MR_XYZ_Y, MR_XYZ_W, MR_XYZ_H,
                             cb_action, (void *)(intptr_t)A_GOTO_JOG);
  static const char *const AX_L[3] = {"X", "Y", "Z"};
  for (uint8_t i = 0; i < 3; i++) {
    lv_obj_t *n = mk_label(xyz_hit, AX_L[i], MR_AXIS);
    lv_obj_set_pos(n, MR_XYZ_LX(i), MR_XYZ_TY);
    st_axis_val[i] = mk_label_right(xyz_hit, MR_XYZ_LX(i), MR_XYZ_TY, MR_XYZ_VW(i),
                                    "-", MR_AXIS);
  }

  // ---- FR (70,136,80,32) va FL (170,136,80,32): icon 32x32 + gia tri o x=32,y=6 ----
  static const lv_img_dsc_t *const RATE_ICON[2] = { &ui_icon_feedrate, &ui_icon_flowrate };
  const lv_coord_t rate_x[2] = {MR_FR_X, MR_FL_X};
  for (uint8_t i = 0; i < 2; i++) {
    lv_obj_t *b = mk_hit(p, rate_x[i], MR_RATE_Y, MR_RATE_W, MR_RATE_H,
                         cb_action, (void *)(intptr_t)A_GOTO_RATE);
    st_rate_icon[i] = mk_icon(b, 0, MR_RATE_ICON_Y, RATE_ICON[i], MR_RATE100);
    st_rate_val[i]  = mk_label(b, "100%", MR_RATE100);
    lv_obj_set_pos(st_rate_val[i], 32, 6);
  }

  // ---- nut SD (0,130) va nut SETTINGS (256,130): icon 64x64 nhu Marlin ----
  lv_obj_t *sd = mk_hit(p, MR_SD_X, MR_TILE_Y, MR_TILE_SZ, MR_TILE_SZ,
                        cb_action, (void *)(intptr_t)A_GOTO_FILES);
  mk_icon(sd, 0, 0, &ui_icon_sd, MR_TIME);

  lv_obj_t *set = mk_hit(p, MR_SET_X, MR_TILE_Y, MR_TILE_SZ, MR_TILE_SZ,
                         cb_action, (void *)(intptr_t)A_GOTO_MENU);
  mk_icon(set, 0, 0, &ui_icon_settings, MR_FRAME);

  // ---- thoi gian in (96,176,128,20) + % (cho trong 224..256) ----
  st_time = mk_label_mid(p, MR_TIME_X, MR_TIME_Y, MR_TIME_W, "-", MR_TIME);
  st_pct  = mk_label(p, "0%", MR_TIME);
  lv_obj_set_pos(st_pct, MR_PCT_X, MR_TIME_Y);

  // ---- thanh tien do (4, 198, 312, 9): khung trang + ruot xanh (nhu Marlin) ----
  st_bar = lv_bar_create(p);
  lv_obj_set_pos(st_bar, MR_BAR_X, MR_BAR_Y);
  lv_obj_set_size(st_bar, MR_BAR_W, MR_BAR_H);
  lv_bar_set_range(st_bar, 0, 100);
  lv_bar_set_value(st_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(st_bar, MR_PROGBG, 0);
  lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(st_bar, 1, 0);
  lv_obj_set_style_border_color(st_bar, MR_FRAME, 0);
  lv_obj_set_style_radius(st_bar, 0, 0);
  lv_obj_set_style_pad_all(st_bar, 1, 0);
  lv_obj_set_style_bg_color(st_bar, MR_PROGBAR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(st_bar, 0, LV_PART_INDICATOR);

  // ---- dong trang thai / ten file (0, 216, 320, 20) ----
  st_msg = mk_label_w(p, "-", MR_MSG, MR_MSG_W);
  lv_obj_set_pos(st_msg, MR_MSG_X, MR_MSG_Y);
  lv_obj_set_style_text_align(st_msg, LV_TEXT_ALIGN_CENTER, 0);
}

static void page_status_update(void) {
  char b[48], a1[12], a2[12], t1[10], t2[10];
  int16_t cur = 0, tgt = 0;
  const bool homed = (panel.flags & PANEL_SF_HOMED) != 0;

  // ---- 3 o: mau + icon theo nhiet do (Marlin: >= 50 do C moi doi mau/icon) ----
  for (uint8_t i = 0; i < ST_CELLS; i++) {
    lv_color_t col = MR_COLD;
    if (i == 2) {                                     // quat: icon theo toc do, chi 1 dong "%"
      uint8_t fp = 0;
      if (panel.n_fans) fp = (uint8_t)(panel.fans[0].pct / 100);
      snprintf(b, sizeof(b), "%u%%", (unsigned)fp);
      set_text(st_cur[i], b);
      set_text(st_tgt[i], "");
      lv_img_set_src(st_icon[i], (fp >= 50) ? &ui_icon_fan_fast : (fp ? &ui_icon_fan_slow : &ui_icon_fan));
    }
    else {                                            // dau in / ban in: so nguyen do C
      temp_lookup(i == 0 ? PANEL_TEMP_E0 : PANEL_TEMP_BED, &cur, &tgt);
      if (cur >= 50) col = (i == 0) ? MR_HOTEND : MR_BEDHOT;
      if (i == 1) lv_img_set_src(st_icon[i], (tgt > 0) ? &ui_icon_bed_heated : &ui_icon_bed);
      fmt_scaled(a1, sizeof(a1), cur, 100, 0);
      fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
      set_text(st_cur[i], a1);
      set_text(st_tgt[i], a2);
    }
    icon_color(st_icon[i], col);
    lv_obj_set_style_text_color(st_cur[i], col, 0);
    lv_obj_set_style_text_color(st_tgt[i], col, 0);
  }

  // ---- XYZ: chua home -> "?" (Marlin nhay "?" mau AXIS_NOT_HOMED) ----
  const int32_t posv[3] = { panel.pos.x, panel.pos.y, panel.pos.z };
  for (uint8_t i = 0; i < 3; i++) {
    if (!homed) {
      set_text(st_axis_val[i], "?");
      lv_obj_set_style_text_color(st_axis_val[i], MR_NOAXIS, 0);
    }
    else {
      fmt_scaled(a1, sizeof(a1), posv[i], 1000, 2);
      set_text(st_axis_val[i], a1);
      lv_obj_set_style_text_color(st_axis_val[i], MR_AXIS, 0);
    }
  }
  lv_obj_set_style_border_color(st_xyz_frame, link_ok() ? MR_AXIS : COL_ERR, 0);

  // ---- FR / FL: xanh la khi 100%, vang khi khac (Marlin) ----
  const uint8_t rate[2] = { (uint8_t)(panel.pos.feed_pct / 100), (uint8_t)(panel.pos.flow_pct / 100) };
  for (uint8_t i = 0; i < 2; i++) {
    const lv_color_t col = (rate[i] == 100) ? MR_RATE100 : MR_RATEALT;
    snprintf(b, sizeof(b), "%u%%", (unsigned)rate[i]);
    set_text(st_rate_val[i], b);
    lv_obj_set_style_text_color(st_rate_val[i], col, 0);
    icon_color(st_rate_icon[i], col);
  }

  // ---- thoi gian: da in / con lai ----
  fmt_time(t1, sizeof(t1), panel.prog.elapsed_s);
  fmt_time(t2, sizeof(t2), panel.prog.remaining_s);
  if (panel.prog.remaining_s) snprintf(b, sizeof(b), "%s/%s", t1, t2);
  else snprintf(b, sizeof(b), "%s", t1);
  set_text(st_time, b);

  // ---- % + thanh tien do ----
  const uint8_t pct = (panel.progress_pct > 100) ? 100 : panel.progress_pct;
  snprintf(b, sizeof(b), "%u%%", (unsigned)pct);
  set_text(st_pct, b);
  if (lv_bar_get_value(st_bar) != (int32_t)pct) lv_bar_set_value(st_bar, (int32_t)pct, LV_ANIM_OFF);

  // ---- dong trang thai: mat lien ket > "STATE  ten file" ----
  if (!link_ok()) {
    set_text(st_msg, tram.rx_active ? "LINK ERR (byte khong hop le)" : "NO LINK");
    lv_obj_set_style_text_color(st_msg, COL_ERR, 0);
  }
  else {
    if (panel.prog.file[0]) snprintf(b, sizeof(b), "%s  %s", state_name(panel.state), panel.prog.file);
    else snprintf(b, sizeof(b), "%s", state_name(panel.state));
    set_text(st_msg, b);
    lv_obj_set_style_text_color(st_msg, MR_MSG, 0);
  }
}

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
    if (panel.n_fans) pct = (int32_t)(panel.fans[0].pct / 100);
    pct += delta;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    snprintf(cmd, sizeof(cmd), "M106 S%ld", (long)((pct * 255 + 50) / 100));
    panel_link_send_gcode(cmd);
    return;
  }

  int16_t cur = 0, tgt = 0;
  temp_lookup(heater ? PANEL_TEMP_BED : PANEL_TEMP_E0, &cur, &tgt);
  int32_t next = (int32_t)(tgt / 100) + (int32_t)delta;      // do C nguyen
  const int32_t hi = heater ? 120 : 300;
  if (next < 0) next = 0;
  if (next > hi) next = hi;

  snprintf(cmd, sizeof(cmd), heater ? "M140 S%ld" : "M104 S%ld", (long)next);
  panel_link_send_gcode(cmd);
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

static void page_temp_create(lv_obj_t *p) {
  temp_row(p, "NOZZLE", 0, &tp_noz);
  temp_row(p, "BED",    1, &tp_bed);
  fan_row(p);

  mk_btn(p, "COOLDOWN", PAGE_INN_W, 34, cb_action, (void *)(intptr_t)A_COOLDOWN);

  mk_label_w(p, "M104/M140 = nhiet, M106 = quat (gui ngay khi bam +/-)", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "COOLDOWN = M104 S0 + M140 S0", COL_DIM, PAGE_INN_W);
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

  const uint8_t fp = panel.n_fans ? (uint8_t)(panel.fans[0].pct / 100) : 0;
  snprintf(b, sizeof(b), "%u%%", (unsigned)fp);
  set_text(tp_fan, b);
}

// ===========================================================================
//  FEED/FLOW (M220/M221) -- o FR / FL tren man STATUS mo vao day, dung nhu Marlin:
//  2 o do chinh la 2 trinh chinh feedrate / flowrate
// ===========================================================================
#define RT_ROW_H   34
#define RT_LBL_W   52
#define RT_BTN_W   40

static lv_obj_t *rt_fr, *rt_fl;

// user_data: (which << 8) | delta   (which 0 = feed -> M220, 1 = flow -> M221)
#define RATE_ADJ(w, d) ((void *)(intptr_t)(((unsigned)(w) << 8) | (unsigned)(uint8_t)(int8_t)(d)))

static void cb_rate_adj(lv_event_t *e) {
  const intptr_t v     = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  which = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const int8_t   delta = (int8_t)(uint8_t)((unsigned)v & 0xFFU);

  const int32_t cur = panel_setting(which ? PANEL_SET_FLOW_PCT : PANEL_SET_FEEDRATE_PCT, 10000) / 100;
  int32_t next = cur + delta;
  if (next < 10) next = 10;                 // duoi 10% thi vo nghia
  if (next > 999) next = 999;

  char cmd[16];
  snprintf(cmd, sizeof(cmd), which ? "M221 S%ld" : "M220 S%ld", (long)next);
  panel_link_send_gcode(cmd);
}

static void cb_rate_reset(lv_event_t *e) {
  (void)e;
  panel_link_send_gcode2("M220 S100", "M221 S100");
}

static void rate_row(lv_obj_t *p, const char *name, uint8_t which, const lv_img_dsc_t *icon,
                     lv_obj_t **val) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, RT_ROW_H, 0, GAP);
  mk_icon(r, 0, 0, icon, MR_RATE100);
  mk_label_w(r, name, COL_TXT, RT_LBL_W);
  mk_btn(r, "-10", RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, -10));
  mk_btn(r, "-1",  RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, -1));
  *val = mk_label_w(r, "100%", MR_RATE100, 72);
  lv_obj_set_flex_grow(*val, 1);
  mk_btn(r, "+1",  RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, +1));
  mk_btn(r, "+10", RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, +10));
}

static void page_rate_create(lv_obj_t *p) {
  rate_row(p, "FEED", 0, &ui_icon_feedrate, &rt_fr);
  rate_row(p, "FLOW", 1, &ui_icon_flowrate, &rt_fl);
  mk_btn(p, "RESET 100% (M220/M221 S100)", PAGE_INN_W, 34, cb_rate_reset, NULL);
  mk_label_w(p, "M220 S<n> = feed rate, M221 S<n> = flow rate", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "gui ngay khi bam +/-  (xem o FR/FL tren STATUS)", COL_DIM, PAGE_INN_W);
}

static void page_rate_update(void) {
  char b[24];
  const int32_t fr = panel_setting(PANEL_SET_FEEDRATE_PCT, 10000) / 100;
  const int32_t fl = panel_setting(PANEL_SET_FLOW_PCT, 10000) / 100;
  snprintf(b, sizeof(b), "%ld%%", (long)fr);
  set_text(rt_fr, b);
  lv_obj_set_style_text_color(rt_fr, (fr == 100) ? MR_RATE100 : MR_RATEALT, 0);
  snprintf(b, sizeof(b), "%ld%%", (long)fl);
  set_text(rt_fl, b);
  lv_obj_set_style_text_color(rt_fl, (fl == 100) ? MR_RATE100 : MR_RATEALT, 0);
}

// ===========================================================================
//  JOG  (30 + 16 + 32 + 34 + 16 + 4*GAP = 136 <= 142)
// ===========================================================================
static uint8_t  s_axis = 0;                      // 0=X 1=Y 2=Z
static lv_obj_t *jg_axis[3];
static const char *const JOG_STEP[3] = {"0.1", "1", "10"};   // mm

#define JG_ROW_H   30
#define JG_STEP_H  32

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
  lv_obj_t *ra = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_ROW_H, 0, GAP);
  mk_label_w(ra, "TRUC", COL_DIM, 44);
  jg_axis[0] = mk_btn_grow(ra, "X", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)0);
  jg_axis[1] = mk_btn_grow(ra, "Y", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)1);
  jg_axis[2] = mk_btn_grow(ra, "Z", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)2);
  lv_obj_set_style_bg_color(jg_axis[0], COL_ACCENT, 0);

  mk_label_w(p, "DI CHUYEN (mm)", COL_DIM, PAGE_INN_W);

  lv_obj_t *rs = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_STEP_H, 0, GAP);
  mk_btn_grow(rs, "-0.1", JG_STEP_H - 2, cb_jog_move, JOG_ARG(0, true));
  mk_btn_grow(rs, "+0.1", JG_STEP_H - 2, cb_jog_move, JOG_ARG(0, false));
  mk_btn_grow(rs, "-1",   JG_STEP_H - 2, cb_jog_move, JOG_ARG(1, true));
  mk_btn_grow(rs, "+1",   JG_STEP_H - 2, cb_jog_move, JOG_ARG(1, false));
  mk_btn_grow(rs, "-10",  JG_STEP_H - 2, cb_jog_move, JOG_ARG(2, true));
  mk_btn_grow(rs, "+10",  JG_STEP_H - 2, cb_jog_move, JOG_ARG(2, false));

  mk_btn(p, "HOME ALL (G28)", PAGE_INN_W, 34, cb_action, (void *)(intptr_t)A_HOME);
  mk_label_w(p, "bam = G91 + G1 <truc><d> F600 + G90", COL_DIM, PAGE_INN_W);
}

static void page_jog_update(void) {
  // khong co gi doi (chi hien thi truc dang chon qua mau nut)
}

// ===========================================================================
//  FILES  (16 + 64 + 26 + 26 + 3*GAP = 138 <= 142)
// ===========================================================================
#define FILES_BTN_H   26
#define FILES_ROLLER_H (4 * ROW_H)      // 64: 4 dong nhin thay (< 104)

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
  fl_info = mk_label_w(p, "SD: -", COL_TXT, PAGE_INN_W);

  fl_roller = lv_roller_create(p);
  lv_obj_set_width(fl_roller, PAGE_INN_W);
  lv_roller_set_visible_row_count(fl_roller, 4);
  lv_obj_set_height(fl_roller, FILES_ROLLER_H);
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

  // 2 hang nut: 4 nut tren (PRINT/DIR/UP/REFRESH) + 3 nut duoi (PAUSE/RESUME/STOP)
  lv_obj_t *r1 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, FILES_BTN_H, 0, GAP);
  mk_btn_grow(r1, "PRINT",   FILES_BTN_H, cb_files_print, NULL);
  mk_btn_grow(r1, "DIR",     FILES_BTN_H, cb_files_dir, NULL);
  mk_btn_grow(r1, "UP",      FILES_BTN_H, cb_action, (void *)(intptr_t)A_REQ_UP);
  mk_btn_grow(r1, "REFRESH", FILES_BTN_H, cb_action, (void *)(intptr_t)A_REQ_FILES);

  lv_obj_t *r2 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, FILES_BTN_H, 0, GAP);
  mk_btn_grow(r2, "PAUSE",  FILES_BTN_H, cb_action, (void *)(intptr_t)A_PAUSE);
  mk_btn_grow(r2, "RESUME", FILES_BTN_H, cb_action, (void *)(intptr_t)A_RESUME);
  mk_btn_grow(r2, "STOP (M524)", FILES_BTN_H, cb_action, (void *)(intptr_t)A_STOP);
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
//  Bang ben trai (cao 104 <= 124) + 3 nut ben phai -> 16 + 104 + 16 + 2*GAP = 140
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

#define SET_TABLE_H 104                              // <= 124, du 6 dong + cuon
#define SET_BTN_W   108                              // du cho chu "SAVE EEPROM" (105 px)
#define SET_TABLE_W (PAGE_INN_W - SET_BTN_W - GAP)   // 202
#define SET_COL0_W  124
#define SET_COL1_W  (SET_TABLE_W - SET_COL0_W)       // 78

static lv_obj_t *se_table, *se_info;

static void cb_scroll_table(lv_event_t *e) {
  const lv_coord_t d = (lv_coord_t)(intptr_t)lv_event_get_user_data(e);
  if (se_table) lv_obj_scroll_by(se_table, 0, d, LV_ANIM_OFF);
}

static void page_settings_create(lv_obj_t *p) {
  se_info = mk_label_w(p, "Marlin gui: 0 thong so", COL_DIM, PAGE_INN_W);

  lv_obj_t *row = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, SET_TABLE_H, 0, GAP);

  se_table = lv_table_create(row);
  lv_obj_set_size(se_table, SET_TABLE_W, SET_TABLE_H);
  lv_table_set_col_cnt(se_table, 2);
  lv_table_set_row_cnt(se_table, (uint16_t)(SET_NROWS + 1));
  lv_table_set_col_width(se_table, 0, SET_COL0_W);
  lv_table_set_col_width(se_table, 1, SET_COL1_W);
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

  // cot nut ben phai bang: SAVE EEPROM / UP / DN
  lv_obj_t *cb = mk_flex(row, LV_FLEX_FLOW_COLUMN, SET_BTN_W, SET_TABLE_H, 0, GAP);
  mk_btn(cb, "SAVE EEPROM", SET_BTN_W, 30, cb_action, (void *)(intptr_t)A_SAVE);
  mk_btn(cb, "UP", SET_BTN_W, 28, cb_scroll_table, (void *)(intptr_t)(-48));
  mk_btn(cb, "DN", SET_BTN_W, 28, cb_scroll_table, (void *)(intptr_t)(48));

  mk_label_w(p, "SAVE = M500   UP/DN = cuon bang", COL_DIM, PAGE_INN_W);
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
//  MESH  (16 + 104 + 16 + 2*GAP = 140 <= 142)
//  Bang luoi ben trai (cao 104 <= 124, cuon duoc) + REFRESH + chu thich ben phai
// ===========================================================================
#define MESH_TABLE_H 104
#define MESH_BTN_W   96
#define MESH_TABLE_W (PAGE_INN_W - MESH_BTN_W - GAP)   // 214
#define MESH_CELL_W  38                                // be rong toi thieu 1 o: du "-0.20" (33 px) + 2*2 pad

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
    mk_spacer(p, 1, 30, 0);                      // day chu thich xuong giua vung noi dung
    me_note = mk_label_w(p, "khong ho tro (PANEL_HAS_MESH=0)", COL_WARN, PAGE_INN_W);
    (void)me_note;
    return;
  }

  me_info = mk_label_w(p, "mesh -", COL_TXT, PAGE_INN_W);

  lv_obj_t *row = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, MESH_TABLE_H, 0, GAP);

  me_table = lv_table_create(row);
  lv_obj_set_size(me_table, MESH_TABLE_W, MESH_TABLE_H);
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

  lv_obj_t *cb = mk_flex(row, LV_FLEX_FLOW_COLUMN, MESH_BTN_W, MESH_TABLE_H, 0, GAP);
  mk_btn(cb, "REFRESH", MESH_BTN_W, 30, cb_action, (void *)(intptr_t)A_REQ_MESH);
  mk_label_w(cb, "xanh = cao", COL_DIM, MESH_BTN_W);
  mk_label_w(cb, "do = thap", COL_DIM, MESH_BTN_W);
  mk_label_w(cb, "+-0.05mm", COL_DIM, MESH_BTN_W);

  mk_label_w(p, "- o ghi mm -  REFRESH = xin lai luoi", COL_DIM, PAGE_INN_W);
}

static void page_mesh_update(void) {
  if (!me_table) return;

  // So cot/hang doi -> tao lai khung bang (chi khi kich thuoc luoi doi)
  if (panel.mesh_nx && panel.mesh_ny && (panel.mesh_nx != s_mesh_nx || panel.mesh_ny != s_mesh_ny)) {
    s_mesh_nx = panel.mesh_nx;
    s_mesh_ny = panel.mesh_ny;
    lv_table_set_col_cnt(me_table, s_mesh_nx);
    lv_table_set_row_cnt(me_table, s_mesh_ny);
    // O rong = chia deu be rong bang; luoi nhieu cot thi giu toi thieu MESH_CELL_W
    // (bang tu cuon ngang: lv_table bao kich thuoc noi dung = tong be rong cac cot)
    const lv_coord_t tw  = lv_obj_get_width(me_table);
    const lv_coord_t fit = (lv_coord_t)((tw > 0 ? tw : MESH_TABLE_W) / (s_mesh_nx ? s_mesh_nx : 1));
    const lv_coord_t cw  = (fit > MESH_CELL_W) ? fit : MESH_CELL_W;
    lv_table_set_col_width(me_table, 0, cw);
    for (uint16_t c = 1; c < s_mesh_nx; c++) lv_table_set_col_width(me_table, c, cw);
  }

  char b[64], a[12];
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
  snprintf(b, sizeof(b), "mesh %ux%u  rows %u  %s..%s%s", (unsigned)panel.mesh_nx, (unsigned)panel.mesh_ny,
           (unsigned)panel.mesh_rows, b1, b2, panel.mesh_valid ? "  valid" : "  -");
  set_text(me_info, b);
}

// ===========================================================================
//  CUSTOM (dung 2 nut nhu menu cu cua Marlin) -- 44 + 3*ROW_H + 3*GAP = 98
// ===========================================================================
static void page_custom_create(lv_obj_t *p) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 44, 0, GAP);
  mk_btn_grow(r, "G34 Bed", 44, cb_action, (void *)(intptr_t)A_G34);
  mk_btn_grow(r, "Init SD", 44, cb_action, (void *)(intptr_t)A_INITSD);

  mk_label_w(p, "G34 Q99 - can bang 2 truc Z", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "M22 (nha the) roi M21 (init lai)", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "2 nut giong menu cu cua Marlin", COL_DIM, PAGE_INN_W);
}

static void page_custom_update(void) { }

// ===========================================================================
//  INFO  (cot thong tin 198 px + cot nut 112 px "REFRESH FULL"; 7 dong = 124,
//  roi 1 dong chu thich -> 124 + GAP + ROW_H = 142 <= PAGE_INN_H)
// ===========================================================================
#define INFO_BTN_W  112
#define INFO_W      (PAGE_INN_W - INFO_BTN_W - GAP)       // 198
#define INFO_COL_H  (7 * ROW_H + 6 * GAP)                 // 124

static lv_obj_t *in_fw, *in_proto, *in_marlin, *in_rx, *in_stat, *in_board, *in_ui;

static void page_info_create(lv_obj_t *p) {
  lv_obj_t *row = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, INFO_COL_H, 0, GAP);
  lv_obj_t *cl  = mk_flex(row, LV_FLEX_FLOW_COLUMN, INFO_W, INFO_COL_H, 0, GAP);
  lv_obj_t *cr  = mk_flex(row, LV_FLEX_FLOW_COLUMN, INFO_BTN_W, INFO_COL_H, 0, GAP);

  in_fw     = mk_label_w(cl, "FW: -", COL_TXT, INFO_W);
  in_board  = mk_label_w(cl, "BOARD: -", COL_TXT, INFO_W);
  in_proto  = mk_label_w(cl, "PROTO: -", COL_DIM, INFO_W);
  in_marlin = mk_label_w(cl, "MARLIN: -", COL_DIM, INFO_W);
  in_stat   = mk_label_w(cl, "LINK: -", COL_DIM, INFO_W);
  in_rx     = mk_label_w(cl, "RX: -", COL_DIM, INFO_W);
  in_ui     = mk_label_w(cl, "UI: LVGL 8.3 / ui_lvgl 1.0", COL_DIM, INFO_W);

  mk_btn(cr, "REFRESH FULL", INFO_BTN_W, 32, cb_action, (void *)(intptr_t)A_REQ_FULL);

  mk_label_w(p, "REFRESH FULL = PANEL_REQ_FULL", COL_DIM, PAGE_INN_W);
}

static void page_info_update(void) {
  char b[48];
  // Ten may: o che do PUSH lay tu goi HELLO cua Marlin. O che do TRAM khong co HELLO
  // (link chi gui dong "TRAM ...") nen dung ten khai trong panel_config.h.
  snprintf(b, sizeof(b), "FW: %s", panel.name[0] ? panel.name
           : (tram.ever ? PANEL_MACHINE_NAME " (TRAM link)" : "(chua co HELLO)"));
  set_text(in_fw, b);

  snprintf(b, sizeof(b), "BOARD: %s %dx%d", PANEL_NAME, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  set_text(in_board, b);

  snprintf(b, sizeof(b), "PROTO: %u (panel %u)", (unsigned)panel.proto, (unsigned)PANEL_PROTO_VERSION);
  set_text(in_proto, b);

  snprintf(b, sizeof(b), "MARLIN fw: 0x%04X", (unsigned)panel.marlin_fw);
  set_text(in_marlin, b);

  snprintf(b, sizeof(b), "LINK: %s%s  state %s", link_ok() ? "OK" : "NO",
           tram.ever ? " (TRAM)" : (panel.linked ? " (PUSH)" : ""), state_name(panel.state));
  set_text(in_stat, b);

  snprintf(b, sizeof(b), "RX: %u frame / %u loi  0x%02X",
           (unsigned)panel.rx_frames, (unsigned)panel.rx_errors, (unsigned)panel.hello_flags);
  set_text(in_rx, b);

  snprintf(b, sizeof(b), "UI: LVGL 8.3 + %s", ui_port_input_is_encoder() ? "encoder" : "touch");
  set_text(in_ui, b);
}

// ===========================================================================
//  TRAMMING -- dieu khien Bed Tramming Wizard cua Marlin qua link ASCII (tram_link.h)
//
//  KHONG dung ExtUI/PANEL_SERIAL_UI: phia Marlin chi can 1 dong #define PANEL_TRAM_LINK
//  (xem Marlin/src/feature/panel_tram_link.h). Nho vay menu 12864 + MarlinUI van con nguyen.
//
//  Bo cuc (tong 168 <= PAGE_INN_H 206):
//    dong 1 : MODE + phase + goc dang dung .......... LINK / NO LINK
//    2 dong : 2 goc mot dong, mm so voi moc, dau '*' = goc nozzle dang dung
//    dong 4 : huong dan van vit (Marlin tinh san: "CW 0.150" / "CCW nhe" / "DAT")
//    dong 5 : 4 nut PROBE | MAX/MIN | BACK | DONE
//    2 dong : chu thich
// ===========================================================================
static lv_obj_t *tr_head, *tr_corner[TRAM_PROTO_POINTS], *tr_hint;

// Ten 4 goc - PHAI theo dung thu tu TRAMMING_POINT_XY trong Marlin/Configuration_adv.h:
//   0 = Back-Right (280,285)  1 = Back-Left (25,285)  2 = Front-Left (25,25)  3 = Front-Right (280,25)
static const char *const TRAM_NAME[TRAM_PROTO_POINTS] = { "BR", "BL", "FL", "FR" };

// Bam 1 trong 4 nut -> gui dung 1 ky tu lenh xuong Marlin (xem tram_protocol.h)
static void cb_tram_cmd(lv_event_t *e) {
  const char c = (char)(intptr_t)lv_event_get_user_data(e);
  tram_link_send(c);
  // BACK = thoat man nay (giong nut BACK cua wizard tren man 12864): bao Marlin thoat wizard
  // (stow probe + danh dau Z chua home) roi quay ve MENU.
  if (c == TRAM_CMD_BACK) show_page(PG_MENU);
}

// " BR  -0.012" / "*FL    ---" : 1 goc, mm so voi moc (Marlin gui san gia tri da tinh)
static void tram_fmt_corner(char *b, size_t n, const uint8_t i) {
  char num[12];
  if (tram.st.ok[i]) {
    const float d = tram.st.d[i];
    fmt_scaled(num, sizeof(num), (int32_t)(d * 1000.0f + (d < 0.0f ? -0.5f : 0.5f)), 1000, 3);
  }
  else snprintf(num, sizeof(num), "  ---");
  snprintf(b, n, "%c%s %s", (tram.st.active && tram.st.idx == i) ? '*' : ' ', TRAM_NAME[i], num);
}

static void page_tram_create(lv_obj_t *p) {
  // KHONG con nhan "LINK OK / NO LINK" o day nua: chi bao link nam o THANH TREN (xem link_ok()).
  lv_obj_t *rh = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 20, 0, GAP);
  tr_head = mk_label_w(rh, "-", COL_TXT, PAGE_INN_W);
  lv_obj_set_flex_grow(tr_head, 1);

  for (uint8_t r = 0; r < 2; r++) {
    lv_obj_t *row = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 24, 0, GAP);
    for (uint8_t k = 0; k < 2; k++) {
      const uint8_t i = (uint8_t)(r * 2 + k);
      tr_corner[i] = mk_label_w(row, "-", COL_TXT, 100);
      lv_obj_set_flex_grow(tr_corner[i], 1);
    }
  }

  tr_hint = mk_label_w(p, "NO LINK", COL_ACCENT, PAGE_INN_W);

  // R = home + probe lai ca 4 goc. Marlin tu bat dau wizard neu chua chay (tramming_run::begin)
  // nen nut nay la diem KHOI DONG cua man Tramming tren panel.
  lv_obj_t *r0 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 38, 0, GAP);
  mk_btn_grow(r0, "HOME + PROBE 4 GOC", 38, cb_tram_cmd, (void *)(intptr_t)TRAM_CMD_RESTART);

  lv_obj_t *rb = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 38, 0, GAP);
  mk_btn_grow(rb, "PROBE",   38, cb_tram_cmd, (void *)(intptr_t)TRAM_CMD_PROBE);
  mk_btn_grow(rb, "MAX/MIN", 38, cb_tram_cmd, (void *)(intptr_t)TRAM_CMD_MINMAX);
  mk_btn_grow(rb, "BACK",    38, cb_tram_cmd, (void *)(intptr_t)TRAM_CMD_BACK);
  mk_btn_grow(rb, "DONE",    38, cb_tram_cmd, (void *)(intptr_t)TRAM_CMD_DONE);

  mk_label_w(p, "DONE = goc xong | MAX/MIN = doi moc", COL_DIM, PAGE_INN_W);
}

static void page_tram_update(void) {
  char b[44];

  // ---- dong 1: MODE + phase + goc dang dung | LINK ----
  if (!tram.st.active)
    snprintf(b, sizeof(b), "%s  READY (bam HOME + PROBE)", tram.st.mode_max ? "MAX" : "MIN");
  else
    snprintf(b, sizeof(b), "%s  %s  goc %u", tram.st.mode_max ? "MAX" : "MIN",
             tram_phase_name(tram.st.phase), (unsigned)(tram.st.idx + 1));
  set_text(tr_head, b);

  // 4 goc: gia tri mm so voi moc (Marlin da tinh)
  for (uint8_t i = 0; i < TRAM_PROTO_POINTS; i++) {
    tram_fmt_corner(b, sizeof(b), i);
    set_text(tr_corner[i], b);
    lv_color_t col = COL_TXT;
    if (tram.st.active && tram.st.idx == i) col = COL_ACCENT;      // goc nozzle dang dung
    else if (tram.st.accepted[i])           col = COL_DIM;         // da bam DONE
    else if (tram.st.ok[i])                 col = COL_OK;          // da do duoc
    lv_obj_set_style_text_color(tr_corner[i], col, 0);
  }

  // ---- dong huong dan: Marlin gui token "CW:0.150" / "CCW:nhe" / "DAT" / "PROBE" / "ERR" / "-"
  lv_color_t hcol = COL_ACCENT;
  if (!tram.linked) {
    snprintf(b, sizeof(b), "chua co du lieu (PANEL_TRAM_LINK?)");
    hcol = COL_ERR;
  }
  else if (tram.st.phase != TRAM_PH_IDLE) {
    snprintf(b, sizeof(b), "%s...", tram_phase_name(tram.st.phase));
  }
  else if (tram.st.all_dat) {
    snprintf(b, sizeof(b), "DAT - lech so voi tam (chi con BACK)");
    hcol = COL_OK;
  }
  else {
    // "CW:0.150" -> "CW 0.150"; "CCW:nhe" -> "CCW nhe"
    const char *h = tram.st.hint[0] ? tram.st.hint : "-";
    uint8_t k = 0;
    for (; h[k] && k < sizeof(b) - 1; k++) b[k] = (h[k] == ':') ? ' ' : h[k];
    b[k] = '\0';
    if (h[0] == 'D') hcol = COL_OK;                                // DAT
    else if (h[0] == 'E') hcol = COL_ERR;                          // ERR
  }
  set_text(tr_hint, b);
  lv_obj_set_style_text_color(tr_hint, hcol, 0);
}

// ===========================================================================
//  MENU -- menu doc kieu Marlin (nut SETTINGS tren man STATUS mo vao day)
//  Moi dong: [ten vang] .......... [gia tri trang]; dong la 1 nut (encoder bam = mo)
// ===========================================================================
#define MENU_ROWS 9
typedef struct { const char *name; uint8_t page; } menu_item_t;
static const menu_item_t MENU_ITEMS[MENU_ROWS] = {
  { "Temperature",     PG_TEMP },
  { "Motion (JOG)",    PG_JOG },
  { "Feed/Flow rate",  PG_RATE },
  { "SD card",         PG_FILES },
  { "Bed mesh",        PG_MESH },
  { "Custom G34/SD",   PG_CUSTOM },
  { "Settings",        PG_SETTINGS },
  { "Tramming wizard", PG_TRAM },
  { "Info",            PG_INFO }
};
static lv_obj_t *mn_val[MENU_ROWS];
static lv_obj_t *mn_val_cal;                 // dong "Calibrate touch" (chi co tren board co touch)

static void cb_menu_row(lv_event_t *e) {
  show_page((uint8_t)(intptr_t)lv_event_get_user_data(e));
}

// "Calibrate touch": chay trinh hieu chuan 4 goc cua port (blocking, tu ve bang TFT_eSPI),
// sau do xoa toan bo man hinh de LVGL ve lai tu dau.
//
// CHONG BAM NHAM: phai GIU TAY >= CAL_TOUCH_HOLD_MS roi NHAC moi vao; 1 lan cham ngan
// (lo bam trong MENU) khong lam gi. Ly do phai chan: trinh hieu chuan la blocking, moi goc
// CHI CAN 1 lan cham la duoc nhan, va no GHI NGAY vao NVS -> bam nham co the lam hong cam ung
// (khi do chi con duong giu tay luc cap dien de vao lai).
#define CAL_TOUCH_HOLD_MS 1500
static uint32_t s_cal_hold_ms = 0;      // >0 = dang giu tay tren dong nay
static bool     s_cal_req     = false;  // da giu du -> ui_lvgl_task() se chay hieu chuan

static void cb_cal_touch(lv_event_t *e) {
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      s_cal_hold_ms = ui_port_millis();
      if (!s_cal_hold_ms) s_cal_hold_ms = 1;               // 0 = "khong giu"
      break;

    case LV_EVENT_RELEASED: {
      const uint32_t held = s_cal_hold_ms ? (ui_port_millis() - s_cal_hold_ms) : 0;
      s_cal_hold_ms = 0;
      if (held >= CAL_TOUCH_HOLD_MS) s_cal_req = true;     // du lau -> cho chay o ui_lvgl_task()
      break;
    }

    // Keo/scroll lam mat diem bam -> coi nhu huy giu
    case LV_EVENT_PRESS_LOST:  s_cal_hold_ms = 0; break;

    default: break;                                        // LV_EVENT_CLICKED (cham ngan) -> bo qua
  }
}

//  MENU la 1 man BINH THUONG: co dong tieu de chung o tren (ten man + LINK + BACK ben phai),
//  cac dong menu do FLEX cua mk_page() tu xep doc (khong can toa do y).
static void page_menu_create(lv_obj_t *p) {
  // 9 dong menu + (tren board co touch) 1 dong "Calibrate touch" = 10 dong x 20 px = 200 <= 206.
  // mk_page() dat pad_row = GAP cho moi man, o day phai de 0 thi moi du (cac dong la o dac lien
  // nhau - giong danh sach menu cua Marlin; dong dang chon duoc to sang + vien xanh).
  lv_obj_set_style_pad_row(p, 0, 0);

  for (uint8_t i = 0; i < MENU_ROWS; i++) {
    lv_obj_t *r = mk_hit(p, 0, 0, PAGE_INN_W, MENU_ROW_H - 2,
                         cb_menu_row, (void *)(intptr_t)MENU_ITEMS[i].page);
    lv_obj_set_style_bg_color(r, COL_BTN, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_t *n = mk_label(r, MENU_ITEMS[i].name, MR_MENU);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, 0);
    mn_val[i] = mk_label(r, "", MR_MENUVAL);
    lv_obj_align(mn_val[i], LV_ALIGN_RIGHT_MID, -4, 0);
  }

  // Dong hieu chuan cam ung: chi hien tren board co touch (CYD).
  // PHAI GIU 1.5s (xem cb_cal_touch) -> 3 event rieng, khong dung CLICKED cua mk_hit.
  if (ui_port_touch_cal_available()) {
    lv_obj_t *r = mk_hit(p, 0, 0, PAGE_INN_W, MENU_ROW_H - 2, cb_cal_touch, NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_RELEASED,   NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_set_style_bg_color(r, COL_BTN, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_t *n = mk_label(r, "Calibrate touch (giu 1.5s)", MR_MENU);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, 0);
    mn_val_cal = mk_label(r, "4 goc", MR_MENUVAL);
    lv_obj_align(mn_val_cal, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  // KHONG ve nut BACK o day nua: da co nut BACK o goc phai dong tieu de (build_chrome)
}

static void page_menu_update(void) {
  char b[24];
  // gia tri ben phai tung dong (giong cot "value" trong menu Marlin)
  int16_t cur = 0, tgt = 0;
  temp_lookup(PANEL_TEMP_E0, &cur, &tgt);
  char a1[12];
  fmt_scaled(a1, sizeof(a1), cur, 100, 0);
  snprintf(b, sizeof(b), "%s C", a1);
  set_text(mn_val[0], b);

  snprintf(b, sizeof(b), "X%u Y%u Z%u",
           (unsigned)((panel.flags & PANEL_SF_HOMED) ? 1 : 0),
           (unsigned)((panel.flags & PANEL_SF_HOMED) ? 1 : 0),
           (unsigned)((panel.flags & PANEL_SF_HOMED) ? 1 : 0));
  set_text(mn_val[1], b);

  snprintf(b, sizeof(b), "%ld/%ld%%",
           (long)(panel_setting(PANEL_SET_FEEDRATE_PCT, 10000) / 100),
           (long)(panel_setting(PANEL_SET_FLOW_PCT, 10000) / 100));
  set_text(mn_val[2], b);

  snprintf(b, sizeof(b), "%u file%s", (unsigned)panel.n_files, panel.files_done ? "" : "...");
  set_text(mn_val[3], b);

  if (panel.hello_flags & PANEL_HAS_MESH)
    snprintf(b, sizeof(b), "%ux%u", (unsigned)panel.mesh_nx, (unsigned)panel.mesh_ny);
  else snprintf(b, sizeof(b), "n/a");
  set_text(mn_val[4], b);

  set_text(mn_val[5], "G34/M22");
  snprintf(b, sizeof(b), "%u/33", (unsigned)panel.n_settings);
  set_text(mn_val[6], b);

  // Tramming wizard: hien trang thai WIZARD (khong phai chi bao link - chi bao link o THANH TREN).
  // Chua co dong TRAM nao / mat link thi ghi ro de biet duong nao chua thong.
  if (!link_ok()) snprintf(b, sizeof(b), "%s", tram.ever ? "mat link" : "chua bat");
  else if (!tram.st.active) snprintf(b, sizeof(b), "san sang");
  else snprintf(b, sizeof(b), "%s %s", tram.st.mode_max ? "MAX" : "MIN", tram_phase_name(tram.st.phase));
  set_text(mn_val[7], b);

  // Duong dang dung (PUSH / TRAM / chua co) - KHONG hien lai trang thai link o day,
  // vi chi bao link chi nam o THANH TREN (xem link_ok()).
  snprintf(b, sizeof(b), "%s", tram.ever ? "TRAM" : (panel.linked ? "PUSH" : "-"));
  set_text(mn_val[8], b);
  // Dong "Calibrate touch": thuong hien nguon gia tri (NVS / mac dinh); khi dang GIU tay thi
  // hien tien do de nguoi dung biet da du 1.5s chua.
  if (s_cal_hold_ms)
    set_text(mn_val_cal, (ui_port_millis() - s_cal_hold_ms) >= CAL_TOUCH_HOLD_MS ? "NHAC TAY" : "dang giu");
  else
    set_text(mn_val_cal, ui_port_touch_cal_is_saved() ? "NVS" : "mac dinh");
}

// ===========================================================================
//  Tao man / chuyen man
// ===========================================================================
static void create_page(uint8_t p) {
  // STATUS dung kin 320x240 theo bo cuc Marlin (khong co dong tieu de);
  // cac man khac (ke ca MENU) deu nam duoi dong tieu de chung -> nut BACK luon o goc phai.
  s_page[p] = (p == PG_STATUS) ? mk_page_full() : mk_page(lv_scr_act());
  switch (p) {
    case PG_STATUS:   page_status_create(s_page[p]);   break;
    case PG_MENU:     page_menu_create(s_page[p]);     break;
    case PG_RATE:     page_rate_create(s_page[p]);     break;
    case PG_TEMP:     page_temp_create(s_page[p]);     break;
    case PG_JOG:      page_jog_create(s_page[p]);      break;
    case PG_FILES:    page_files_create(s_page[p]);    break;
    case PG_SETTINGS: page_settings_create(s_page[p]); break;
    case PG_MESH:     page_mesh_create(s_page[p]);     break;
    case PG_CUSTOM:   page_custom_create(s_page[p]);   break;
    case PG_TRAM:     page_tram_create(s_page[p]);     break;
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

  // Dong tieu de (co nut BACK o goc phai) hien o MOI man tru STATUS (man goc, khong co BACK)
  const bool chrome = (p != PG_STATUS);
  if (s_bar) {
    if (chrome) lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
  }

  set_text(s_title, PG_TITLE[p]);
  s_page_act = p;

  // encoder: focus vao vat dau tien cua man vua mo
  if (s_group && !focus_first_in(s_page[p]) && s_bar) lv_group_focus_obj(s_bar);
}

// ===========================================================================
//  Dong tieu de cho cac man con: [ten man] ...... [LINK] [BACK]
//  (STATUS/MENU khong dung: chung co bo cuc rieng theo Marlin)
// ===========================================================================
static void build_chrome(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  s_bar = lv_obj_create(scr);
  lv_obj_set_pos(s_bar, 0, 0);
  lv_obj_set_size(s_bar, UI_LCD_HOR_RES, BAR_H);
  style_flat(s_bar, COL_BAR);
  lv_obj_set_style_pad_left(s_bar, 4, 0);
  lv_obj_set_style_pad_right(s_bar, 4, 0);
  lv_obj_set_style_pad_column(s_bar, 6, 0);
  lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  s_title = mk_label(s_bar, "STATUS", COL_TXT);
  mk_spacer(s_bar, 0, 1, 1);                     // an het cho trong -> day 2 vat sau ve phai
  s_link  = mk_label(s_bar, "NO LINK", COL_ERR);
  mk_btn(s_bar, "BACK", 48, 20, cb_action, (void *)(intptr_t)A_BACK);
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

static void refocus_page(void) {
  if (!s_group) return;
  if (s_page[s_page_act]) focus_first_in(s_page[s_page_act]);
  else if (s_bar) lv_group_focus_obj(s_bar);
}

static void cb_prompt_value(lv_event_t *e) {
  lv_obj_t *box = lv_event_get_target(e);
  const char *t = lv_msgbox_get_active_btn_text(box);
  const uint8_t ok = (t && (t[0] == 'O' || t[0] == 'o')) ? 1 : 0;    // OK / CANCEL
  panel_link_answer(ok);          // panel_link_answer() tu dat prompt_active = false
  box_close(&s_prompt_box);
  refocus_page();
}

static void cb_msg_value(lv_event_t *e) {
  (void)e;
  box_close(&s_msg_box);
  refocus_page();
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

  // 1 vung dem UI_LCD_HOR_RES x 40 pixel: du nhanh ma khong ton RAM
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

  // Hieu chuan cam ung: chay NGOAI event callback cua LVGL (blocking, tu ve bang TFT_eSPI),
  // xong xoa lai toan man hinh de LVGL ve lai tu dau.
  if (s_cal_req) {
    s_cal_req = false;
    ui_port_touch_calibrate();
    lv_obj_invalidate(lv_scr_act());
    s_tick_ms = s_upd_ms = s_slow_ms = ui_port_millis();
    return;
  }

  if (now - s_upd_ms < UI_REFRESH_MS) return;      // UI_REFRESH_MS trong panel_config.h
  s_upd_ms = now;

  // Thanh tren = NOI DUY NHAT hien chi bao link cho moi man (tru trang STATUS co them dong
  // trang thai + khung XYZ cung bao mat link). 3 muc: LINK OK / LINK ERR / NO LINK
  // (xem link_state_now() - "co byte toi" khac voi "doc duoc dong TRAM").
  const uint8_t lst = link_state_now();
  if (lst != s_link_state) {
    s_link_state = lst;
    set_text(s_link, lst == 1 ? "LINK OK" : (lst == 2 ? "LINK ERR" : "NO LINK"));
    lv_obj_set_style_text_color(s_link, lst == 1 ? COL_OK : (lst == 2 ? COL_ACCENT : COL_ERR), 0);
  }

  switch (s_page_act) {
    case PG_STATUS: page_status_update(); break;
    case PG_MENU:   page_menu_update();   break;
    case PG_RATE:   page_rate_update();   break;
    case PG_TEMP:   page_temp_update();   break;
    case PG_JOG:    page_jog_update();    break;
    case PG_TRAM:   page_tram_update();   break;
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
