/**
 * ui_page_custom.cpp -- man CUSTOM cua UI LVGL (man hinh CYD): G34 / Init SD /
 * M119 endstop / M412 filament / reboot DFU - cac muc khong con cho o MENU.
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
//  CUSTOM -- G34/Init SD (nhu menu cu) + P4 endstop M119 + P6 filament + P8 DFU
//  (MENU da het cho: 10 dong x 22 = 220 > 206 nen cac muc moi nam o day)
// ===========================================================================
static lv_obj_t *cu_runout;                     // nut toggle M412 (P6)

// P6: bat/tat cam bien het filament. Trang thai dau lay tu SETTING RUNOUT_EN, sau do
// dao moi lan bam (Marlin khong day lai ngay trang thai nay trong frame STATUS).
static bool s_runout_on = true;

// To lai nut theo s_runout_on. Chi ghi mau khi THAT SU doi (giong jog_axis_mark/ov_marks): ham nay
// duoc goi moi chu ky cap nhat cua man CUSTOM (400 ms) va lv_obj_set_style_bg_color() luon
// invalidate vat -> goi vo dieu kien se ton bang thong SPI.
static void runout_mark(void) {
  if (!cu_runout) return;
  set_text(lv_obj_get_child(cu_runout, 0), s_runout_on ? "M412: BAT (S1)" : "M412: TAT (S0)");
  const lv_color_t want = s_runout_on ? COL_OK : COL_BTN;
  if (lv_obj_get_style_bg_color(cu_runout, 0).full != want.full)
    lv_obj_set_style_bg_color(cu_runout, want, 0);
}

static void cb_runout_toggle(lv_event_t *e) {
  (void)e;
  s_runout_on = !s_runout_on;
  cydlcd_link_send_gcode(s_runout_on ? "M412 S1" : "M412 S0");
  runout_mark();
}

void page_custom_create(lv_obj_t *p) {
  // 2026-10-15 (yeu cau nguoi dung: "tam an va cam chuc nang G34 do thiet ke truc Z cung, ke ca GCODE
  // cung tat, g34 khong lam gi ca"): nut "G34 Bed" DA BO khoi trang nay. Phia Marlin cung da tat
  // (`CYDLCD_NO_G34` trong Configuration.h -> GcodeSuite::G34() thoat ngay, xem Marlin G34.cpp).
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 40, 0, GAP);
  mk_btn_grow(r, "Init SD", 40, cb_action, (void *)(intptr_t)A_INITSD);
  mk_btn_grow(r, "ENDSTOP", 40, cb_action, (void *)(intptr_t)A_GOTO_ENDSTOP);   // P4 -> man M119

  // P6: doi filament (M600) + cong tac cam bien het filament (M412)
  lv_obj_t *rf = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 30, 0, GAP);
  mk_btn_grow(rf, "M600 B0", 30, cb_action, (void *)(intptr_t)A_FILAMENT_CHANGE);
  cu_runout = mk_btn_grow(rf, "M412: BAT (S1)", 30, cb_runout_toggle, NULL);

  // P8: nhay vao ROM DFU bootloader (2 buoc xac nhan)
  lv_obj_t *rd = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 30, 0, GAP);
  mk_btn_grow(rd, "REBOOT DFU", 30, cb_action, (void *)(intptr_t)A_REBOOT_DFU);

  mk_label_w(p, "M600 = doi filament (khong lui dai)", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "M412 = cam bien het filament ON/OFF", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "REBOOT DFU = nap firmware qua USB", COL_DIM, PAGE_INN_W);
  // Tong: 40 + 30 + 30 + 4*16 + 6*GAP = 176 <= 206
}

void page_custom_update(void) {
  if (!cu_runout) return;                // trang da bi thu hoi (an toan: xem page_custom_destroy)
  // Trang thai M412: dong bo lan dau tu SETTING (neu Marlin co gui CYDLCD_SET_RUNOUT_EN)
  static bool got = false;
  if (!got) {
    const int32_t v = cydlcd_setting(CYDLCD_SET_RUNOUT_EN, -1);
    if (v >= 0) { got = true; s_runout_on = (v != 0); }
  }
  // Ve lai nhan/nem theo trang thai dang giu: BAT BUOC sau khi trang duoc TAO LAI (bi thu hoi roi mo
  // lai) vi nut moi luon bat dau bang nhan "M412: BAT (S1)" + nen COL_BTN, con s_runout_on thi giu
  // lua chon cu cua nguoi dung. runout_mark() chi ghi style khi mau THAT SU doi -> re (khong
  // invalidate moi 400 ms).
  runout_mark();
}

// Huy man CUSTOM (xem quy uoc o ui_lvgl_int.h): dat NULL nut M412. Container trang do show_page()
// xoa bang lv_obj_del() nen khong xoa widget o day. s_runout_on: GIU NGUYEN (lua chon cua nguoi
// dung) - page_custom_update() to lai nut MOI theo dung trang thai do (xem runout_mark).
void page_custom_destroy(void) {
  cu_runout = NULL;
}

#endif  // CYDLCD_USE_LVGL

