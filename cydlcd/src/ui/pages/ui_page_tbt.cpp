/**
 * ui_page_tbt.cpp -- man TRAMMING cua UI LVGL (man hinh CYD): dieu khien Bed Tramming
 * Wizard cua Marlin qua link ASCII (tbt_protocol.h), khong dung ExtUI:
 * 4 so cua 4 goc + dau `*` goc dang dung + huong dan van vit + 4 nut.
 *
 * Tach tu ui_lvgl.cpp (chang 4/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "ui_lang.h"                // T() - da ngon ngu

#include <stdio.h>                  // snprintf
#include <string.h>                 // strcmp

#if CYDLCD_USE_LVGL

// ===========================================================================
//  TRAMMING -- dieu khien Bed Tramming Wizard cua Marlin qua link ASCII (tbt_link.h)
//
//  KHONG dung ExtUI/CYDLCD_SERIAL_UI: phia Marlin chi can 1 dong #define CYDLCD_TBT_LINK
//  (xem Marlin/src/feature/cydlcd_tbt_link.h). Nho vay menu 12864 + MarlinUI van con nguyen.
//
//  Bo cuc (tong 168 <= PAGE_INN_H 206):
//    dong 1 : MODE + phase + goc dang dung .......... LINK / NO LINK
//    2 dong : 2 goc mot dong, mm so voi moc, dau '*' = goc nozzle dang dung
//    dong 4 : huong dan van vit (Marlin tinh san: "CW 0.150" / "CCW nhe" / "DAT")
//    dong 5 : 4 nut PROBE | MAX/MIN | BACK | DONE
//    2 dong : chu thich
// ===========================================================================
static lv_obj_t *tr_head, *tr_corner[TBT_PROTO_POINTS], *tr_hint;

// Ten 4 goc - PHAI theo dung thu tu TRAMMING_POINT_XY trong Marlin/Configuration_adv.h:
//   0 = Back-Right (280,285)  1 = Back-Left (25,285)  2 = Front-Left (25,25)  3 = Front-Right (280,25)
static const char *const TBT_NAME[TBT_PROTO_POINTS] = { "BR", "BL", "FL", "FR" };

// Bam 1 trong 4 nut -> gui dung 1 ky tu lenh xuong Marlin (xem tbt_protocol.h)
static void cb_tbt_cmd(lv_event_t *e) {
  const char c = (char)(intptr_t)lv_event_get_user_data(e);
  tbt_link_send(c);
  // (2026-10-17) Truoc day con nhanh `if (c == TBT_CMD_BACK) show_page(PG_MENU);` cho nut BACK o hang
  // 4 nut. Nut do da bo theo yeu cau nguoi dung.
}

// " BR  -0.012" / "*FL    ---" : 1 goc, mm so voi moc (Marlin gui san gia tri da tinh)
static void tbt_fmt_corner(char *b, size_t n, const uint8_t i) {
  char num[12];
  if (tbt.st.ok[i]) {
    const float d = tbt.st.d[i];
    fmt_scaled(num, sizeof(num), (int32_t)(d * 1000.0f + (d < 0.0f ? -0.5f : 0.5f)), 1000, 3);
  }
  else snprintf(num, sizeof(num), "  ---");
  snprintf(b, n, "%c%s %s", (tbt.st.active && tbt.st.idx == i) ? '*' : ' ', TBT_NAME[i], num);
}

void page_tbt_create(lv_obj_t *p) {
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

  // R = home + probe lai ca 4 goc. Marlin tu bat dau wizard neu chua chay (tbt_run::begin)
  // nen nut nay la diem KHOI DONG cua man Tramming tren MAN HINH CYD.
  lv_obj_t *r0 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 38, 0, GAP);
  mk_btn_grow(r0, T(UI_T_TBT_RESTART), 38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_RESTART);

  lv_obj_t *rb = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 38, 0, GAP);
  mk_btn_grow(rb, "PROBE",   38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_PROBE);
  mk_btn_grow(rb, "MAX/MIN", 38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_MINMAX);
  // 2026-10-17: BO nut "BACK" o day (yeu cau nguoi dung - tren header da co nut BACK).
  // KHONG thay bang hook "roi trang": probe da duoc deploy/thu trong TUNG lan probe, va moi lenh in
  // deu co G28 -> khong can ket thuc wizard khi roi trang (y kien nguoi dung 2026-10-17).
  mk_btn_grow(rb, "DONE",    38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_DONE);

  mk_label_w(p, T(UI_T_TBT_HINT), COL_DIM, PAGE_INN_W);
}

void page_tbt_update(void) {
  if (!tr_head) return;                  // trang da bi thu hoi (an toan: xem page_tbt_destroy)
  char b[44];

  // ---- dong 1: MODE + phase + goc dang dung | LINK ----
  if (!tbt.st.active)
    snprintf(b, sizeof(b), "%s  READY (bam HOME + PROBE)", tbt.st.mode_max ? "MAX" : "MIN");
  else
    snprintf(b, sizeof(b), "%s  %s  goc %u", tbt.st.mode_max ? "MAX" : "MIN",
             tbt_phase_name(tbt.st.phase), (unsigned)(tbt.st.idx + 1));
  set_text(tr_head, b);

  // 4 goc: gia tri mm so voi moc (Marlin da tinh)
  for (uint8_t i = 0; i < TBT_PROTO_POINTS; i++) {
    tbt_fmt_corner(b, sizeof(b), i);
    set_text(tr_corner[i], b);
    lv_color_t col = COL_TXT;
    if (tbt.st.active && tbt.st.idx == i) col = COL_ACCENT;      // goc nozzle dang dung
    else if (tbt.st.accepted[i])           col = COL_DIM;         // da bam DONE
    else if (tbt.st.ok[i])                 col = COL_OK;          // da do duoc
    lv_obj_set_style_text_color(tr_corner[i], col, 0);
  }

  // ---- dong huong dan: Marlin gui token "CW:0.150" / "CCW:nhe" / "DAT" / "PROBE" / "ERR" / "-"
  lv_color_t hcol = COL_ACCENT;
  if (!tbt.linked) {
    snprintf(b, sizeof(b), "chua co du lieu (CYDLCD_TBT_LINK?)");
    hcol = COL_ERR;
  }
  else if (tbt.st.phase != TBT_PH_IDLE) {
    snprintf(b, sizeof(b), "%s...", tbt_phase_name(tbt.st.phase));
  }
  else if (tbt.st.all_dat) {
    snprintf(b, sizeof(b), T(UI_T_TBT_RESULT));
    hcol = COL_OK;
  }
  else {
    // "CW:0.150" -> "CW 0.150"; "CCW:nhe" -> "CCW nhe"
    const char *h = tbt.st.hint[0] ? tbt.st.hint : "-";
    uint8_t k = 0;
    for (; h[k] && k < sizeof(b) - 1; k++) b[k] = (h[k] == ':') ? ' ' : h[k];
    b[k] = '\0';
    if (h[0] == 'D') hcol = COL_OK;                                // DAT
    else if (h[0] == 'E') hcol = COL_ERR;                          // ERR
  }
  set_text(tr_hint, b);
  lv_obj_set_style_text_color(tr_hint, hcol, 0);
}

// Huy man TRAMMING (xem quy uoc o ui_lvgl_int.h): dat NULL nhan dau + 4 nhan goc + dong huong dan.
// Container trang do show_page() xoa bang lv_obj_del() nen khong xoa widget o day. Du lieu cua
// wizard (tbt.st/tbt.linked...) KHONG phai cua trang: giu nguyen, Marlin van gui tiep dong "TBT ...".
void page_tbt_destroy(void) {
  tr_head = NULL;
  tr_hint = NULL;
  for (uint8_t i = 0; i < TBT_PROTO_POINTS; i++) tr_corner[i] = NULL;
}

#endif  // CYDLCD_USE_LVGL

