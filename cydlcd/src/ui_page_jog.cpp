/**
 * ui_page_jog.cpp -- man JOG cua UI LVGL (man hinh CYD): chon truc + cac buoc nhich + HOME/M84
 * + G28 tung truc + lop phu sua M851 Z.
 *
 * Tach tu ui_lvgl.cpp (chang 2/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 *   - jog_axis_mark()/cb_jog_axis(): danh dau TRUC DANG CHON bang NEN COL_ACCENT (doc lap voi
 *     focus cua encoder - xem focus_mark + LV_OBJ_FLAG_USER_1 trong ui_widgets.cpp).
 *   - cb_jog_move(): G91 + G1 <truc><d> F3000 (X/Y) / F600 (Z) + G90.
 *   - page_jog_create()/page_jog_update().
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "ui_lang.h"                // T() - da ngon ngu

#include <stdio.h>                  // snprintf

#if CYDLCD_USE_LVGL

// ===========================================================================
//  JOG  (30 + 16 + 32 + 28 + 34 + 28 + 16 + 6*GAP = 196 <= 206)
//  P2: them buoc nho 0.01 / 0.05 mm; P7: M84 + G28 tung truc + M851 Z (lop phu sua)
// ===========================================================================
static uint8_t  s_axis = 0;                      // 0=X 1=Y 2=Z
static lv_obj_t *jg_axis[3];
// 0..4 tuong ung JOG_STEP[0..4]; JOG_STEP[step] la chuoi di kem trong lenh G1
static const char *const JOG_STEP[5] = {"0.01", "0.05", "0.1", "1", "10"};   // mm

#define JG_ROW_H   30
#define JG_STEP_H  32
#define JG_FINE_H  28
#define JG_BTN_H   28

// Danh dau TRUC DANG CHON bang NEN COL_ACCENT - DOC LAP HOAN TOAN voi focus.
//  - "dang chon" (s_axis)  = NEN nut: COL_ACCENT
//  - "focus"    (encoder/cham) = VIEN 2 px (xem focus_mark + LV_OBJ_FLAG_USER_1)
// Nho tach 2 kenh nay nen focus chuyen sang nut buoc / nut di chuyen KHONG con ghi de nen
// cua nut truc -> dau "truc dang chon" khong bao gio mat (loi nguoi dung bao truoc day).
static void jog_axis_mark(void) {
  for (uint8_t i = 0; i < 3; i++) {
    if (!jg_axis[i]) continue;
    const lv_color_t want = (i == s_axis) ? COL_ACCENT : COL_BTN;
    // Chi set khi mau THAT SU doi: lv_obj_set_style_bg_color() luon invalidate vat, goi vo
    // dieu kien moi 150 ms se lam man JOG nhap nhay va ton bang thong SPI.
    if (lv_obj_get_style_bg_color(jg_axis[i], 0).full != want.full)
      lv_obj_set_style_bg_color(jg_axis[i], want, 0);
  }
}

static void cb_jog_axis(lv_event_t *e) {
  const uint8_t a = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (a > 2) return;
  s_axis = a;
  jog_axis_mark();
}

// user_data: (chi so buoc << 8) | (1 = am)
static void cb_jog_move(lv_event_t *e) {
  const intptr_t v = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  step = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const bool     neg  = (((unsigned)v & 0xFFU) != 0);
  if (step > 4) return;

  char cmd[24];
  // Toc do nhich: X/Y = 3000 mm/phut = 50 mm/s (bang HOMING_FEEDRATE X/Y cho de so sanh),
  // Z giu 600 mm/phut = 10 mm/s (= tran M203 Z10, vit me nen phai cham).
  // Truoc day dung F600 cho ca 3 truc -> nhich X/Y cham hon home 5 lan (2026-10-13).
  const unsigned f = (s_axis == 2) ? 600U : 3000U;
  snprintf(cmd, sizeof(cmd), "G1 %c%s%s F%u", "XYZ"[s_axis], neg ? "-" : "", JOG_STEP[step], f);
  // G91 ... G90: luon dong lai bang G90 de mot dong bi mat khong de may o che do tuong doi
  cydlcd_link_send_gcode("G91");
  cydlcd_link_send_gcode(cmd);
  cydlcd_link_send_gcode("G90");
}

#define JOG_ARG(step, neg) ((void *)(intptr_t)(((unsigned)(step) << 8) | (unsigned)((neg) ? 1U : 0U)))

void page_jog_create(lv_obj_t *p) {
  lv_obj_t *ra = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_ROW_H, 0, GAP);
  mk_label_w(ra, T(UI_T_JOG_AXIS), COL_DIM, 44);
  jg_axis[0] = mk_btn_grow(ra, "X", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)0);
  jg_axis[1] = mk_btn_grow(ra, "Y", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)1);
  jg_axis[2] = mk_btn_grow(ra, "Z", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)2);
  // 3 nut truc TU QUAN LY NEN (LV_OBJ_FLAG_USER_1): focus_mark() chi ve VIEN cho chung, KHONG doi
  // nen -> nen COL_ACCENT cua truc dang chon khong bi ghi de khi focus sang nut buoc/di chuyen.
  for (uint8_t i = 0; i < 3; i++) lv_obj_add_flag(jg_axis[i], LV_OBJ_FLAG_USER_1);
  jog_axis_mark();                                // to nhat truc dang chon (s_axis = 0 luc tao man)

  mk_label_w(p, T(UI_T_JOG_MOVE), COL_DIM, PAGE_INN_W);

  lv_obj_t *rs = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_STEP_H, 0, GAP);
  mk_btn_grow(rs, "-0.1", JG_STEP_H - 2, cb_jog_move, JOG_ARG(2, true));
  mk_btn_grow(rs, "+0.1", JG_STEP_H - 2, cb_jog_move, JOG_ARG(2, false));
  mk_btn_grow(rs, "-1",   JG_STEP_H - 2, cb_jog_move, JOG_ARG(3, true));
  mk_btn_grow(rs, "+1",   JG_STEP_H - 2, cb_jog_move, JOG_ARG(3, false));
  mk_btn_grow(rs, "-10",  JG_STEP_H - 2, cb_jog_move, JOG_ARG(4, true));
  mk_btn_grow(rs, "+10",  JG_STEP_H - 2, cb_jog_move, JOG_ARG(4, false));

  // P2: buoc nho (0.01 / 0.05 mm) - dung cho can chinh tinh
  lv_obj_t *rf = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_FINE_H, 0, GAP);
  mk_btn_grow(rf, "-0.01", JG_FINE_H - 2, cb_jog_move, JOG_ARG(0, true));
  mk_btn_grow(rf, "+0.01", JG_FINE_H - 2, cb_jog_move, JOG_ARG(0, false));
  mk_btn_grow(rf, "-0.05", JG_FINE_H - 2, cb_jog_move, JOG_ARG(1, true));
  mk_btn_grow(rf, "+0.05", JG_FINE_H - 2, cb_jog_move, JOG_ARG(1, false));

  mk_btn(p, "HOME ALL (G28)", PAGE_INN_W, 34, cb_action, (void *)(intptr_t)A_HOME);

  // P7 + P2: tat dong co, home tung truc, sua probe Z (mo lop phu M851 Z)
  lv_obj_t *rb = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_BTN_H, 0, GAP);
  mk_btn_grow(rb, "M84",    JG_BTN_H, cb_action, (void *)(intptr_t)A_STEPPERS_OFF);
  mk_btn_grow(rb, "G28 X",  JG_BTN_H, cb_action, (void *)(intptr_t)A_HOME_X);
  mk_btn_grow(rb, "G28 Y",  JG_BTN_H, cb_action, (void *)(intptr_t)A_HOME_Y);
  mk_btn_grow(rb, "G28 Z",  JG_BTN_H, cb_action, (void *)(intptr_t)A_HOME_Z);
  mk_btn_grow(rb, "M851 Z", JG_BTN_H, cb_action, (void *)(intptr_t)A_EDIT_PROBE_Z);

  mk_label_w(p, T(UI_T_JOG_HINT), COL_DIM, PAGE_INN_W);
}

void page_jog_update(void) {
  if (!jg_axis[0]) return;               // trang da bi thu hoi (an toan: xem page_jog_destroy)
  // Ve LAI dau truc dang chon o MOI chu ky cap nhat (khong chi luc bam): neu co ai do doi style
  // (hoac doi man roi quay lai) thi nen COL_ACCENT van duoc khoi phuc ngay trong ~150 ms.
  jog_axis_mark();
}

// Huy man JOG (xem quy uoc o ui_lvgl_int.h): chi dat NULL 3 nut truc. Container trang do
// show_page() xoa bang lv_obj_del() nen khong xoa widget o day.
//
// s_axis: GIU NGUYEN (truc nguoi dung dang chon) - page_jog_create() goi jog_axis_mark() nen nut
// MOI duoc to lai dung theo s_axis; tra ve 0 se la DOI HANH VI (mo lai man lai nhay ve truc X).
void page_jog_destroy(void) {
  for (uint8_t i = 0; i < 3; i++) jg_axis[i] = NULL;
}

#endif  // CYDLCD_USE_LVGL
