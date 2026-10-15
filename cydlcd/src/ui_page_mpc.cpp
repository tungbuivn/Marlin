/**
 * ui_page_mpc.cpp -- man TUNING (MPC/PID) cua UI LVGL (man hinh CYD).
 *
 * YEU CAU NGUOI DUNG 2026-10-15: "chua co MPC value trong setting 'Nhiet do' giong marlin 12864, hay them
 * nut tunning vao, o do se hien thi giong marlin 12864, co may nut turning trong do, co the move sang neu can".
 *
 * Man nay lam 2 viec:
 *   1) HIEN THI 6 tham so MPC (M306: P/C/R/A/F/H) - giong menu "MPC" cua Marlin 12864. Gia tri lay tu frame
 *      SETTING ma Marlin day xuong (cydlcd_serial.cpp: push_settings_body) nen luon la so THAT trong Marlin.
 *   2) Cac NUT TUNING (truoc day nam o man NHIET DO, nay MOVE sang day theo yeu cau):
 *        M306 T  = autotune MPC (nozzle phai NGUOI - Marlin do nhiet do moi truong truoc)
 *        M303 E-1 60 = autotune PID ban in (target 60 do C)
 *        SAVE EEPROM (M500) = luu ket qua vua tune
 *      Sua TAY tung tham so MPC lam o man THONG SO (bang SETTINGS co 6 dong "MPC ...").
 *
 * Chieu cao: tieu de 20 + 6 dong * (16 + GAP 2) = 108 + note 20 + 2 hang nut 26 + 5*GAP(10) = 210
 * 2026-10-15: nut 28 -> 26 de tong <= PAGE_INN_H (206). Font mono size 15 (line_height 20) lam moi
 * dong chu cao 20 px; 6 dong tham so VAN de khung 16 px (nhan duoc can giua -> net chu ~11 px van nam
 * gon, chi sat hon) vi nang 6 dong do len 20 se lam CA TRANG tran han (>240).
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf

#if CYDLCD_USE_LVGL

// (id trong frame SETTING, nhan ngan, scale, so chu so thap phan) - KHOP voi Marlin:
//   P/C gui scale 1000, R/A/F/H gui scale 10000 (xem cydlcd_serial.cpp).
typedef struct { uint16_t id; const char *name; uint16_t scale; uint8_t dec; } mpc_row_t;

static const mpc_row_t MPC_ROWS[6] = {
  { CYDLCD_SET_MPC_P, "P  heater W",    1000, 2 },
  { CYDLCD_SET_MPC_C, "C  block J/K",   1000, 2 },
  { CYDLCD_SET_MPC_R, "R  sensor K/sK", 10000, 4 },
  { CYDLCD_SET_MPC_A, "A  amb fan0 W/K",10000, 4 },
  { CYDLCD_SET_MPC_F, "F  amb fanN W/K",10000, 4 },
  { CYDLCD_SET_MPC_H, "H  filament",    10000, 4 }
};

static lv_obj_t *mp_val[6];
static lv_obj_t *mp_note;

void page_mpc_create(lv_obj_t *p) {
  mk_label_w(p, "MPC (M306) - nhu Marlin 12864", MR_MENU, PAGE_INN_W);

  for (uint8_t i = 0; i < 6; i++) {
    lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 16, 0, GAP);
    mk_label_w(r, MPC_ROWS[i].name, COL_TXT, 130);
    mp_val[i] = mk_label_w(r, "-", COL_ACCENT, 100);
    lv_obj_set_flex_grow(mp_val[i], 1);         // gia tri an het phan con lai
  }

  mp_note = mk_label_w(p, "sua tay: 6 dong MPC o man THONG SO", COL_DIM, PAGE_INN_W);

  lv_obj_t *r1 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 26, 0, GAP);
  mk_btn_grow(r1, "M306 T (tune)", 26, cb_action, (void *)(intptr_t)A_MPC_TUNE);
  mk_btn_grow(r1, "M303 E-1 60",   26, cb_action, (void *)(intptr_t)A_PID_BED);

  lv_obj_t *r2 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 26, 0, GAP);
  mk_btn_grow(r2, "SAVE EEPROM", 26, cb_action, (void *)(intptr_t)A_SAVE);
  mk_btn_grow(r2, "THONG SO",    26, cb_action, (void *)(intptr_t)A_GOTO_SETTINGS);
}

void page_mpc_update(void) {
  if (!mp_val[0]) return;                        // trang da bi thu hoi

  char b[24], a[16];
  for (uint8_t i = 0; i < 6; i++) {
    const int32_t raw = cydlcd_setting(MPC_ROWS[i].id, INT32_MIN);
    if (raw == INT32_MIN) snprintf(b, sizeof(b), "-");        // ban build khong co MPC
    else {
      fmt_scaled(a, sizeof(a), raw, MPC_ROWS[i].scale, MPC_ROWS[i].dec);
      snprintf(b, sizeof(b), "%s", a);
    }
    set_text(mp_val[i], b);
  }

  // Dong nhac: chi ro trang thai nozzle de biet co chay M306 T duoc khong (autotune can nozzle NGUOI).
  int16_t cur = 0, tgt = 0;
  temp_lookup(CYDLCD_TEMP_E0, &cur, &tgt);
  fmt_scaled(a, sizeof(a), cur, 100, 0);
  snprintf(b, sizeof(b), "nozzle %s C%s", a, (cur > 4000) ? " - CHO NGUOI truoc khi tune!" : "");
  set_text(mp_note, b);
  lv_obj_set_style_text_color(mp_note, (cur > 4000) ? COL_WARN : COL_DIM, 0);
}

// Huy man TUNING (xem quy uoc o ui_lvgl_int.h): dat NULL cac nhan; container trang do show_page() xoa.
void page_mpc_destroy(void) {
  for (uint8_t i = 0; i < 6; i++) mp_val[i] = NULL;
  mp_note = NULL;
}

#endif  // CYDLCD_USE_LVGL
