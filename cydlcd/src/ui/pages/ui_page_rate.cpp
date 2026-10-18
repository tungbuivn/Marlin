/**
 * ui_page_rate.cpp -- man FEED/FLOW (M220/M221) cua UI LVGL (man hinh CYD): 2 o FR / FL tren
 * man STATUS mo vao day, dung nhu Marlin.
 *
 * Tach tu ui_lvgl.cpp (chang 2/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 *   - rate_row()/cb_rate_adj()/cb_rate_reset(): +/- 10/1 % -> M220 S... / M221 S...; RESET 100%.
 *   - page_rate_create()/page_rate_update().
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "ui_lang.h"                // T() - da ngon ngu

#include "ui_lvgl_icons.h"          // icon 32x32 FR/FL chuyen tu bitmap cua Marlin

#include <stdio.h>                  // snprintf

#if CYDLCD_USE_LVGL

// ===========================================================================
//  FEED/FLOW (M220/M221) -- o FR / FL tren man STATUS mo vao day, dung nhu Marlin:
//  2 o do chinh la 2 trinh chinh feedrate / flowrate
// ===========================================================================
#define RT_ROW_H   34
#define RT_LBL_W   52
#define RT_BTN_W   40

static lv_obj_t *rt_fr, *rt_fl;
static lv_obj_t *rt_icon[2];            // 2026-10-17: icon FEED/FLOW de to mau theo gia tri (giong STATUS)

// ---------------------------------------------------------------------------
//  TU XIN LAI BANG THONG SO (FR/FL lay tu cydlcd_setting -> goi CYDLCD_F_SETTING cua Marlin).
//
//  VI SAO CAN: Marlin CHI gui lai bang thong so khi duoc HOI (`CYDLCD_REQ_SETTINGS`), khong tu day
//  khi gia tri doi. Truoc day trang chi hoi SAU KHI BAM +/- cua chinh no, nen neu FR/FL doi tu cho
//  khac (console COM7, G-code cua ban in, M220/M221 trong start G-code...) thi 2 nhan dung yen o
//  gia tri CU cho toi khi roi trang roi vao lai (gap that khi do `M220 S42` tu console).
//  => Mo trang thi hoi NGAY, va khi trang dang mo thi hoi lai dinh ky RT_REQ_MS.
// ---------------------------------------------------------------------------
static uint32_t s_req_ms = 0;               // moc lan hoi gan nhat
#define RT_REQ_MS 2000u                     // 2 s: du tuoi, khong ton bang thong (1 lenh ngan)

static void rate_request(void) {
  s_req_ms = lv_tick_get();
  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);
}

// user_data: (which << 8) | delta   (which 0 = feed -> M220, 1 = flow -> M221)
#define RATE_ADJ(w, d) ((void *)(intptr_t)(((unsigned)(w) << 8) | (unsigned)(uint8_t)(int8_t)(d)))

static void cb_rate_adj(lv_event_t *e) {
  const intptr_t v     = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  which = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const int8_t   delta = (int8_t)(uint8_t)((unsigned)v & 0xFFU);

  const int32_t cur = cydlcd_setting(which ? CYDLCD_SET_FLOW_PCT : CYDLCD_SET_FEEDRATE_PCT, 10000) / 100;
  int32_t next = cur + delta;
  if (next < 10) next = 10;                 // duoi 10% thi vo nghia
  if (next > 999) next = 999;

  char cmd[16];
  snprintf(cmd, sizeof(cmd), which ? "M221 S%ld" : "M220 S%ld", (long)next);
  cydlcd_link_send_gcode(cmd);
  // Marlin doi that nhung so tren man lay tu goi SETTING -> xin gui lai cho khoi cu
  rate_request();
}

static void cb_rate_reset(lv_event_t *e) {
  (void)e;
  cydlcd_link_send_gcode2("M220 S100", "M221 S100");
  rate_request();
}

static void rate_row(lv_obj_t *p, const char *name, uint8_t which, const lv_img_dsc_t *icon,
                     lv_obj_t **val) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, RT_ROW_H, 0, GAP);
  rt_icon[which] = mk_icon(r, 0, 0, icon, MR_RATE100);
  mk_label_w(r, name, COL_TXT, RT_LBL_W);
  mk_btn(r, "-10", RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, -10));
  mk_btn(r, "-1",  RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, -1));
  *val = mk_label_w(r, "100%", MR_RATE100, 72);
  lv_obj_set_flex_grow(*val, 1);
  mk_btn(r, "+1",  RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, +1));
  mk_btn(r, "+10", RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, +10));
}

void page_rate_create(lv_obj_t *p) {
  rate_row(p, "FEED", 0, &ui_icon_feedrate, &rt_fr);
  rate_row(p, "FLOW", 1, &ui_icon_flowrate, &rt_fl);
  mk_btn(p, "RESET 100% (M220/M221 S100)", PAGE_INN_W, 34, cb_rate_reset, NULL);
  mk_label_w(p, "M220 S<n> = feed rate, M221 S<n> = flow rate", COL_DIM, PAGE_INN_W);
  mk_label_w(p, T(UI_T_RATE_HINT2), COL_DIM, PAGE_INN_W);
  rate_request();                          // hoi NGAY khi mo trang (dung gia tri, khong cho nhip sau)
}

void page_rate_update(void) {
  if (!rt_fr || !rt_fl) return;          // trang da bi thu hoi (an toan: xem page_rate_destroy)
  // Dinh ky hoi lai bang thong so: FR/FL co the doi tu CHO KHAC (console, G-code ban in...).
  if (lv_tick_elaps(s_req_ms) >= RT_REQ_MS) rate_request();
  char b[24];
  const int32_t fr = cydlcd_setting(CYDLCD_SET_FEEDRATE_PCT, 10000) / 100;
  const int32_t fl = cydlcd_setting(CYDLCD_SET_FLOW_PCT, 10000) / 100;
  // 2026-10-17 (yeu cau nguoi dung: "mau icon feed, flow o page 02 chua giong voi page 00"):
  // STATUS to icon theo GIA TRI (100% = xanh MR_RATE100, khac 100% = vang MR_RATEALT);
  // trang nay truoc day chi to MAU CHU, con icon giu mau xanh co dinh -> nay to ca icon.
  const lv_color_t cf = (fr == 100) ? MR_RATE100 : MR_RATEALT;
  const lv_color_t cl = (fl == 100) ? MR_RATE100 : MR_RATEALT;
  snprintf(b, sizeof(b), "%ld%%", (long)fr);
  set_text(rt_fr, b);
  set_text_color(rt_fr, cf);
  if (rt_icon[0]) icon_color(rt_icon[0], cf);
  snprintf(b, sizeof(b), "%ld%%", (long)fl);
  set_text(rt_fl, b);
  set_text_color(rt_fl, cl);
  if (rt_icon[1]) icon_color(rt_icon[1], cl);
}

// Huy man FEED/FLOW (xem quy uoc o ui_lvgl_int.h): chi dat NULL 2 nhan FR/FL. Container trang do
// show_page() xoa bang lv_obj_del() nen khong xoa widget o day. Khong co trang thai rieng phai tra
// ve ban dau: gia tri FR/FL doc truc tiep tu bang thong so cua Marlin (cydlcd_setting) moi chu ky.
void page_rate_destroy(void) {
  rt_fr = NULL;
  rt_fl = NULL;
  rt_icon[0] = NULL; rt_icon[1] = NULL;
}

#endif  // CYDLCD_USE_LVGL
