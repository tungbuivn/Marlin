/**
 * ui_page_endstop.cpp -- man ENDSTOP cua UI LVGL (man hinh CYD): 6 o trang thai
 * kieu M119 tu frame CYDLCD_F_ENDSTOPS.
 *
 * Tach tu ui_lvgl.cpp (chang 4/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf
#include <string.h>                 // strcmp

#if CYDLCD_USE_LVGL

// ===========================================================================
//  ENDSTOP (P4) -- trang thai kieu M119 tu frame CYDLCD_F_ENDSTOPS
//  6 o: PROBE / X min / Y min / Z min / X max / Y max (id theo cydlcd_endstop_id_t)
//  May dung Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN thi chi bao PROBE (id 0), Z min luon "mo".
//  Chieu cao: 16 + 6*23 + 30 + 7*GAP = 198 <= 206 (font mono size 15: line_height 20 -> dong phai
//  cao >= 20; truoc day 20 px voi line_height 17)
// ===========================================================================
#define ES_ROW_H 23
#define ES_NAME_W 120

static const uint8_t ES_ID[CYDLCD_ENDSTOP_MAX] = {
  CYDLCD_ES_PROBE, CYDLCD_ES_X_MIN, CYDLCD_ES_Y_MIN, CYDLCD_ES_Z_MIN, CYDLCD_ES_X_MAX, CYDLCD_ES_Y_MAX
};
static const char *const ES_NAME[CYDLCD_ENDSTOP_MAX] = {
  "PROBE", "X min", "Y min", "Z min", "X max", "Y max"
};
static lv_obj_t *es_val[CYDLCD_ENDSTOP_MAX], *es_info;

void page_endstop_create(lv_obj_t *p) {
  es_info = mk_label_w(p, "M119: -", COL_DIM, PAGE_INN_W);
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) {
    lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, ES_ROW_H, 0, GAP);
    mk_label_w(r, ES_NAME[i], COL_TXT, ES_NAME_W);
    es_val[i] = mk_label_w(r, "-", COL_DIM, PAGE_INN_W - ES_NAME_W - GAP);
  }
  mk_btn(p, "REFRESH (M119)", PAGE_INN_W, 30, cb_action, (void *)(intptr_t)A_REQ_ENDSTOPS);
  cydlcd_link_request((uint8_t)CYDLCD_REQ_ENDSTOPS);       // doc ngay khi mo man
}

void page_endstop_update(void) {
  if (!es_info) return;                  // trang da bi thu hoi (an toan: xem page_endstop_destroy)
  char b[40];
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) {
    const bool hit = (cydlcd.es_hit[ES_ID[i]] != 0);
    set_text(es_val[i], hit ? "CHAM (triggered)" : "mo (open)");
    set_text_color(es_val[i], hit ? COL_ERR : COL_OK);
  }
  snprintf(b, sizeof(b), "M119: %u o - bam REFRESH de doc lai", (unsigned)cydlcd.n_endstops);
  set_text(es_info, b);
}

// Huy man ENDSTOP (xem quy uoc o ui_lvgl_int.h): dat NULL 6 nhan gia tri + dong info. Container
// trang do show_page() xoa bang lv_obj_del() nen khong xoa widget o day.
void page_endstop_destroy(void) {
  es_info = NULL;
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) es_val[i] = NULL;
}

#endif  // CYDLCD_USE_LVGL

