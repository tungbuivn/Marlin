/**
 * ui_page_endstop.cpp -- man ENDSTOP cua UI LVGL (man hinh CYD): cac o trang thai
 * kieu M119 tu frame CYDLCD_F_ENDSTOPS.
 *
 * Mo tu: MENU -> Motion -> "Endstops (M119)" (2026-10-15: truoc day la 1 dong cua MENU chinh; nay nam
 * NGAY SAU "Stow Z-Probe (M402)" trong menu MOTION).
 *
 * 2026-10-15 (YEU CAU NGUOI DUNG: "doc tin hieu M115 30 lan 1 giay ... khong can nut refresh nua" va
 * "khi ra khoi menu thi bo viec monitor tin hieu cua man hinh endstop di"):
 *   - BO nut "REFRESH (M119)".
 *   - Xin lai `CYDLCD_REQ_ENDSTOPS` o nhip **30 Hz** (33 ms) - KHONG the dung nhip chung UI_REFRESH_MS
 *     (150 ms), nen `page_endstop_poll()` duoc goi tu `ui_lvgl_task_body()` (ui task 30 Hz) TRUOC cong
 *     UI_REFRESH_MS, va CHI khi `s_page_act == PG_ENDSTOP`.
 *   - RA KHOI MAN la NGUNG monitor ngay: `page_endstop_poll()` khong con duoc goi (xem ui_lvgl.cpp) va
 *     `page_endstop_destroy()` xoa moc thoi gian -> lan sau vao lai xin frame dau tien ngay lap tuc.
 *   - Chi AN han nhung dong ma ban build KHONG co (bit trong `cydlcd.es_present`): may nay Marlin chi
 *     bao 3 chan (PROBE + X min + Y min) nen truoc day 3 dong "Z min / X max / Y max" hien "mo (open)"
 *     GIA TAO (khong phai trang thai that).
 *
 * LUU Y ve dong PROBE: tren may nay `Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN` + `PROBE_ENABLE_DISABLE`, nen
 * chan Z_MIN chinh la dau probe va probe CHI tra tin hieu khi da duoc bat (M401 / G28 Z -> probe.deploy()
 * -> endstops.enable_z_probe(true)). Chua deploy thi dau doc len muc "TRIGGERED" - Marlin da bo qua chan
 * nay trong `endstops.cpp update()` (chi xu ly khi `z_probe_enabled`), nen Marlin-side `push_endstops()`
 * bao "open" cho dung (yeu cau nguoi dung: "gia tri hien thi cua z-probe dang bi nguoc, boi vi chua goi
 * deploy probe z nen value chua la open").
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf

#if CYDLCD_USE_LVGL

// ===========================================================================
//  ENDSTOP (P4) -- trang thai kieu M119 tu frame CYDLCD_F_ENDSTOPS
//  Toi da 6 o: PROBE / X min / Y min / Z min / X max / Y max (id theo cydlcd_endstop_id_t);
//  o nao ban build khong bao thi bi AN (cydlcd.es_present).
//  Cao nhat (6 dong, font Segoe UI 16 = dong 20 px): 20 + 6*23 + 6*GAP(2) = 170 <= 206 -> KHONG tran.
// ===========================================================================
#define ES_ROW_H 23
#define ES_NAME_W 120
#define ES_REQ_MS 30                   // 30 lan/giay (task `ui` chay nhip 33 ms -> moi nhip xin 1 lan)

static const uint8_t ES_ID[CYDLCD_ENDSTOP_MAX] = {
  CYDLCD_ES_PROBE, CYDLCD_ES_X_MIN, CYDLCD_ES_Y_MIN, CYDLCD_ES_Z_MIN, CYDLCD_ES_X_MAX, CYDLCD_ES_Y_MAX
};
static const char *const ES_NAME[CYDLCD_ENDSTOP_MAX] = {
  "PROBE", "X min", "Y min", "Z min", "X max", "Y max"
};
static lv_obj_t *es_val[CYDLCD_ENDSTOP_MAX], *es_info;
static uint32_t  es_next_ms;           // moc (ms) xin frame M119 ke tiep

// Ve lai cac nhan tu `cydlcd` (goi tu page_endstop_poll o nhip 30 Hz VA tu page_endstop_update o nhip
// chung). `set_text`/`set_text_color` tu so sanh truoc nen chu khong doi thi KHONG ve lai.
static void es_refresh(void) {
  char b[40];
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) {
    // Dong nao ban build khong bao (bit khong bat trong es_present) thi AN han di - truoc day no hien
    // "mo (open)" gia tao. Flex cua LVGL bo qua vat co co HIDDEN nen cac dong con lai tu don len.
    // ⚠️ CHI doi co khi trang thai THAT SU doi: `lv_obj_add_flag/clear_flag` voi HIDDEN luon goi
    // `lv_obj_invalidate()` + danh dau layout lai -> goi vo dieu kien moi luot (30 lan/giay) lam CA TRANG
    // ve lai qua SPI (DO THAT: luot cap nhat tu 0,1 ms nhay len 27 ms => man hinh giat).
    lv_obj_t *row = lv_obj_get_parent(es_val[i]);
    const bool present = ((cydlcd.es_present & (uint8_t)(1u << ES_ID[i])) != 0);
    if (row) {
      const bool hidden = lv_obj_has_flag(row, LV_OBJ_FLAG_HIDDEN);
      if (present && hidden)        lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);
      else if (!present && !hidden) lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    }
    if (!present) continue;
    const bool hit = (cydlcd.es_hit[ES_ID[i]] != 0);
    set_text(es_val[i], hit ? "CHAM (triggered)" : "mo (open)");
    set_text_color(es_val[i], hit ? COL_ERR : COL_OK);
  }
  snprintf(b, sizeof(b), "M119: %u chan - tu dong doc 30 lan/giay", (unsigned)cydlcd.n_endstops);
  set_text(es_info, b);
}

void page_endstop_create(lv_obj_t *p) {
  es_info = mk_label_w(p, "M119: -", COL_DIM, PAGE_INN_W);
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) {
    lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, ES_ROW_H, 0, GAP);
    mk_label_w(r, ES_NAME[i], COL_TXT, ES_NAME_W);
    es_val[i] = mk_label_w(r, "-", COL_DIM, PAGE_INN_W - ES_NAME_W - GAP);
  }
  // KHONG con nut REFRESH: xin ngay khi mo man (moc = 0 -> nhip dau tien trong page_endstop_poll gui lien).
  es_next_ms = 0;
  cydlcd_link_request((uint8_t)CYDLCD_REQ_ENDSTOPS);
}

// 30 lan/giay: CHI duoc goi khi dang o man ENDSTOP (ui_lvgl.cpp, truoc cong UI_REFRESH_MS va TRONG khoa
// trang thai). Ra khoi man thi khong con ai goi -> NGUNG monitor.
void page_endstop_poll(void) {
  if (!es_info) return;                  // trang da bi thu hoi
  const uint32_t now = ui_port_millis();
  if ((int32_t)(now - es_next_ms) < 0) return;
  es_next_ms = now + ES_REQ_MS;
  cydlcd_link_request((uint8_t)CYDLCD_REQ_ENDSTOPS);
  es_refresh();                          // cap nhat ngay, khong doi luot 150 ms
}

// Luot cap nhat chung (UI_REFRESH_MS): chi ve lai (nhan co the doi vi frame M119 vua toi o nhip 30 Hz).
void page_endstop_update(void) {
  if (!es_info) return;                  // trang da bi thu hoi (an toan: xem page_endstop_destroy)
  es_refresh();
}

// Huy man ENDSTOP (xem quy uoc o ui_lvgl_int.h): dat NULL cac nhan gia tri + dong info, va XOA moc thoi
// gian de lan sau vao lai xin frame dau tien ngay. Container trang do show_page() xoa bang lv_obj_del()
// nen khong xoa widget o day.
void page_endstop_destroy(void) {
  es_info = NULL;
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) es_val[i] = NULL;
  es_next_ms = 0;                        // ra khoi man = ket thuc phien monitor (xem ghi chu dau file)
}

#endif  // CYDLCD_USE_LVGL
