/**
 * ui_page_preheat.cpp -- HOP SUA PREHEAT 3-TRONG-1 + BAN PHIM dat ten vat lieu.
 *
 * YEU CAU NGUOI DUNG 2026-10-15:
 *   - "gom [preheat] vao 1 menu la preheat, trong preheat cho phep user them bot cac loai vat lieu,
 *      max la 6";
 *   - "neu co the gop 3 thuoc tinh cua preheat tren 1 dong de dieu chinh thi khong can vao menu con
 *      de sua nua" -> HOP SUA nay: 3 o NOZ/BED/FAN nam CUNG 1 HANG (cham o nao thi -/+ chinh o do),
 *      KHONG phai di qua menu con.
 *
 * KHO du lieu nam o ui_preheat.cpp (NVS "ph2"); file nay CHI ve giao dien + goi API cong khai
 * (ui_preheat_count/name/get/set/add/remove/rename/rev trong ui_lvgl.h).
 *
 * HAI lop phu, deu la con cua `lv_scr_act()` (KHONG phai con cua trang) nen `show_page()` phai goi
 * preheat_edit_hide()/preheat_kb_hide() khi doi trang - giong cach man WIFI don ban phim mat khau:
 *   1) HOP SUA 3-trong-1 (preheat_edit_open)  : noi/bed/fan + 3 muc buoc + [-][+][OK][Huy][Xoa];
 *   2) BAN PHIM dat ten (preheat_add_open)   : textarea + lv_keyboard (chi co khi env bat ban phim -
 *      `lv_conf_cydlcd.h` dat LV_USE_KEYBOARD/TEXTAREA = 1; env khac: nut Them chi bao qua console).
 *
 * Xoa vat lieu: nut "Xoa" trong hop sua -> hop XAC NHAN 2 BUOC cua ui_boxes.cpp (A_PRE_DELETE) vi
 * xoa xong la mat luon gia tri da dat. KHONG cho xoa vat lieu CUOI CUNG (trang NHIET DO phai con 1 nut).
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "ui_lang.h"                // T() - da ngon ngu

#include <stdio.h>                  // snprintf / sscanf
#include <string.h>                 // strcmp
#include <Arduino.h>                // Serial.printf (§ chan doan `UI PRE`)

#if CYDLCD_USE_LVGL

// ===========================================================================
//  HOP SUA 3-TRONG-1
// ===========================================================================
// 2026-10-15: 268 -> 300 (rong hon de hang nut 3-trong-1 + hang 5 nut co the CAN GIUA voi khoang
// trang DLG_BTN_GAP; man 320 px nen hop 300 van thua 2 ben 10 px).
#define PO_W      300
#define PO_H      122
#define PO_FIELD_H 26                // hang 3 o NOZ/BED/FAN
#define PO_STEP_H  24                // hang 3 muc buoc
#define PO_ACT_H   26                // hang nut - + OK Huy Xoa

// 3 thuoc tinh: 0 = nozzle (do C), 1 = bed (do C), 2 = fan (0..255 - gia tri M106 S...)
#define PO_FIELDS 3
static const char *const PO_FNAME[PO_FIELDS] = { "NOZ", "BED", "FAN" };
// Muc buoc theo tung loai: nhiet do 1/5/10 do C; quat 1/5/25 (0..255).
static const uint8_t PO_STEP_TEMP[3] = { 1, 5, 10 };
static const uint8_t PO_STEP_FAN[3]  = { 1, 5, 25 };
static const char *const PO_STEP_TXT[2][3] = { { "1", "5", "10" }, { "1", "5", "25" } };

static lv_obj_t *s_po_bd, *s_po_box, *s_po_title;
static lv_obj_t *s_po_fld[PO_FIELDS], *s_po_stp[3], *s_po_minus, *s_po_plus;
static bool     s_po_open = false;
static uint8_t  s_po_slot = 0;       // vat lieu dang sua (chi so trong kho preheat)
static uint8_t  s_po_field = 0;      // o dang chon (0..2)
static uint8_t  s_po_step = 1;       // muc buoc dang chon (0..2)
static uint16_t s_po_h = 0, s_po_b = 0;
static uint8_t  s_po_f = 0;
static bool     s_po_rep = false;    // da nhay do GIU nut -> bo qua CLICKED luc nhac tay
static uint8_t  s_po_del_slot = 0;   // vat lieu dang cho xac nhan xoa (A_PRE_DELETE)

bool preheat_edit_active(void) { return s_po_open; }

// Gia tri cua o dang chon (int32 de dung chung cho 3 loai).
static int32_t po_val(const uint8_t f) {
  return (f == 0) ? (int32_t)s_po_h : ((f == 1) ? (int32_t)s_po_b : (int32_t)s_po_f);
}
static void po_set_val(const uint8_t f, int32_t v) {
  if (f == 0) {
    if (v < 0) v = 0; else if (v > PRE_H_MAX) v = PRE_H_MAX;
    s_po_h = (uint16_t)v;
  } else if (f == 1) {
    if (v < 0) v = 0; else if (v > PRE_B_MAX) v = PRE_B_MAX;
    s_po_b = (uint16_t)v;
  } else {
    if (v < 0) v = 0; else if (v > PRE_F_MAX) v = PRE_F_MAX;
    s_po_f = (uint8_t)v;
  }
}
// Buoc dang ap dung cho O dang chon (nut buoc doi nghia theo o: nhiet do 1/5/10 - quat 1/5/25).
static uint8_t po_step_now(void) {
  const uint8_t s = (s_po_step > 2) ? 2 : s_po_step;
  return (s_po_field == 2) ? PO_STEP_FAN[s] : PO_STEP_TEMP[s];
}

// Ve lai toan bo chu cua hop (ten vat lieu, 3 o gia tri, 3 nhan buoc) + to nut dang chon.
// CHI ghi khi chu/mau DOI (set_text/set_bg_color deu invalidate vat -> tranh day SPI vo ich).
static void po_refresh(void) {
  char b[24];
  if (s_po_title) {
    snprintf(b, sizeof(b), "PREHEAT: %s", ui_preheat_name(s_po_slot));
    set_text(s_po_title, b);
  }
  const int32_t v[PO_FIELDS] = { (int32_t)s_po_h, (int32_t)s_po_b, (int32_t)s_po_f };
  for (uint8_t f = 0; f < PO_FIELDS; f++) {
    if (!s_po_fld[f]) continue;
    snprintf(b, sizeof(b), "%s %ld", PO_FNAME[f], (long)v[f]);
    set_text(lv_obj_get_child(s_po_fld[f], 0), b);
    // O dang chon: nen COL_ACCENT (giong nut buoc / nut truc man JOG). Vat TU QUAN LY NEN -> USER_1
    // de focus_mark() chi ve vien, khong ghi de nen. mk_btn_set_sel() doi CA NEN va MAU CHU.
    mk_btn_set_sel(s_po_fld[f], f == s_po_field);
  }
  // 3 nut buoc: chu doi theo o dang chon (1/5/10 hoac 1/5/25) va to nut dang chon.
  const uint8_t kind = (s_po_field == 2) ? 1 : 0;
  for (uint8_t k = 0; k < 3; k++) {
    if (!s_po_stp[k]) continue;
    set_text(lv_obj_get_child(s_po_stp[k], 0), PO_STEP_TXT[kind][k]);
    mk_btn_set_sel(s_po_stp[k], k == s_po_step);
  }
}

static void cb_po_field(lv_event_t *e) {
  const uint8_t f = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (f >= PO_FIELDS) return;
  s_po_field = f;
  s_po_step  = 1;                    // ve muc buoc mac dinh (1) cho o vua chon - tranh lech nghia buoc
  po_refresh();
}

static void cb_po_step(lv_event_t *e) {
  const uint8_t k = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (k > 2) return;
  s_po_step = k;
  po_refresh();
}

// -/+ : ap buoc dang chon vao O dang chon. GIU nut -> LVGL ban LONG_PRESSED_REPEAT lien tuc.
static void cb_po_adj(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  const int8_t d = (int8_t)(intptr_t)lv_event_get_user_data(e);
  if (code == LV_EVENT_PRESSED) { s_po_rep = false; return; }
  if (code == LV_EVENT_LONG_PRESSED_REPEAT) s_po_rep = true;
  else if (s_po_rep) { s_po_rep = false; return; }        // bo qua CLICKED luc nhac tay
  if (!s_po_open) return;
  po_set_val(s_po_field, po_val(s_po_field) + (int32_t)d * (int32_t)po_step_now());
  po_refresh();
}

static void preheat_edit_close(void) {
  s_po_open = false;
  if (s_po_bd)  lv_obj_add_flag(s_po_bd, LV_OBJ_FLAG_HIDDEN);
  if (s_po_box) lv_obj_add_flag(s_po_box, LV_OBJ_FLAG_HIDDEN);
}

void preheat_edit_hide(void) {
  if (!s_po_open) return;
  preheat_edit_close();
  refocus_page();
}

static void cb_po_ok(lv_event_t *e) {
  (void)e;
  if (!s_po_open) return;
  const uint8_t i = s_po_slot;
  preheat_edit_close();
  ui_preheat_set(i, s_po_h, s_po_b, s_po_f);   // ghi NVS + tang rev -> cac trang tu ve lai
  refocus_page();
}

static void cb_po_cancel(lv_event_t *e) { (void)e; preheat_edit_hide(); }

// Xoa vat lieu: dong hop truoc (de hop XAC NHAN nam tren), roi hoi 2 buoc. Lenh that chay o
// do_confirmed(A_PRE_DELETE) -> preheat_delete_pending() (xem ui_lvgl.cpp).
static void cb_po_del(lv_event_t *e) {
  (void)e;
  if (!s_po_open) return;
  char t[40];
  snprintf(t, sizeof(t), T(UI_T_PRE_DEL_Q), ui_preheat_name(s_po_slot));
  s_po_del_slot = s_po_slot;
  preheat_edit_close();
  ui_confirm(T(UI_T_PRE_DEL_TITLE), t, (uint8_t)A_PRE_DELETE, 2);
}

void preheat_delete_pending(void) {
  ui_preheat_remove(s_po_del_slot);            // rev++ -> MENU CONFIGURATION + trang NHIET DO ve lai
}

// Tao 1 lan (giu nguyen giua cac lan mo - giong lop phu SUA cua trang THONG SO).
static void po_create(void) {
  // !!! `lv_layer_top()`: hop phai luon nam TREN trang (trang duoc TAO LAI moi lan mo -> neu de tren
  // lv_scr_act thi tu lan mo thu 2 tro di no bi trang tao sau che mat - loi that 2026-10-15).
  lv_obj_t *scr = lv_layer_top();

  s_po_bd = lv_obj_create(scr);
  lv_obj_set_pos(s_po_bd, 0, CONT_Y);
  lv_obj_set_size(s_po_bd, UI_LCD_HOR_RES, CONT_H);
  lv_obj_set_style_bg_color(s_po_bd, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_po_bd, LV_OPA_70, 0);
  lv_obj_set_style_border_width(s_po_bd, 0, 0);
  lv_obj_set_style_radius(s_po_bd, 0, 0);
  lv_obj_set_style_pad_all(s_po_bd, 0, 0);
  lv_obj_clear_flag(s_po_bd, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_po_bd, LV_OBJ_FLAG_HIDDEN);

  s_po_box = lv_obj_create(scr);
  lv_obj_set_pos(s_po_box, (UI_LCD_HOR_RES - PO_W) / 2, (lv_coord_t)(CONT_Y + (CONT_H - PO_H) / 2));
  lv_obj_set_size(s_po_box, PO_W, PO_H);
  lv_obj_set_style_bg_color(s_po_box, COL_BAR, 0);
  lv_obj_set_style_bg_opa(s_po_box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(s_po_box, COL_ACCENT, 0);
  lv_obj_set_style_border_width(s_po_box, 1, 0);
  lv_obj_set_style_radius(s_po_box, 3, 0);
  lv_obj_set_style_pad_all(s_po_box, 4, 0);
  lv_obj_set_style_pad_row(s_po_box, GAP, 0);
  lv_obj_clear_flag(s_po_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(s_po_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(s_po_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  // KHONG dat HIDDEN o day: con tao trong luc cha dang an thi LVGL khong tinh layout/flex.

  const lv_coord_t iw = PO_W - 2 * 4;                  // be rong trong hop

  // Dong 1: ten vat lieu
  s_po_title = mk_label_w(s_po_box, "-", MR_MENU, iw);

  // Dong 2: 3 O (nozzle | bed | fan) - CA 3 TRONG 1 HANG, cham o nao thi -/+ chinh o do. Nut be rong
  // CO DINH + can GIUA + cach nhau DLG_BTN_GAP (yeu cau nguoi dung 2026-10-15).
  lv_obj_t *rf = mk_dlg_row(s_po_box, iw, PO_FIELD_H);
  for (uint8_t f = 0; f < PO_FIELDS; f++) {
    s_po_fld[f] = mk_dlg_btn(rf, PO_FNAME[f], 84, PO_FIELD_H, DLG_NEUTRAL_BG,
                             cb_po_field, (void *)(intptr_t)f);
    lv_obj_add_flag(s_po_fld[f], LV_OBJ_FLAG_USER_1);  // tu quan ly NEN (xem po_refresh)
  }

  // Dong 3: 3 muc buoc (nghia doi theo o dang chon)
  lv_obj_t *rs = mk_dlg_row(s_po_box, iw, PO_STEP_H);
  for (uint8_t k = 0; k < 3; k++) {
    s_po_stp[k] = mk_dlg_btn(rs, PO_STEP_TXT[0][k], 60, PO_STEP_H, DLG_NEUTRAL_BG,
                             cb_po_step, (void *)(intptr_t)k);
    lv_obj_add_flag(s_po_stp[k], LV_OBJ_FLAG_USER_1);
  }

  // Dong 4: - | + | OK (XANH) | Huy (DO) | Xoa (DO) - can giua, co khoang trang
  lv_obj_t *ra = mk_dlg_row(s_po_box, iw, PO_ACT_H);
  s_po_minus = mk_dlg_btn(ra, "-", 42, PO_ACT_H, DLG_NEUTRAL_BG, cb_po_adj, (void *)(intptr_t)(int8_t)-1);
  s_po_plus  = mk_dlg_btn(ra, "+", 42, PO_ACT_H, DLG_NEUTRAL_BG, cb_po_adj, (void *)(intptr_t)(int8_t)+1);
  mk_dlg_btn(ra, "OK", 56, PO_ACT_H, DLG_OK_BG, cb_po_ok, NULL);
  lv_obj_t *huy = mk_dlg_btn(ra, "H\xE1\xBB\xA7y", 56, PO_ACT_H, DLG_CANCEL_BG, cb_po_cancel, NULL);  // "Huy"
  lv_obj_t *xoa = mk_dlg_btn(ra, "X\xC3\xB3a", 56, PO_ACT_H, DLG_CANCEL_BG, cb_po_del, NULL);          // "Xoa"
  vn_font_btn(huy);
  vn_font_btn(xoa);
  // Giu nut -/+ de nhay nhanh (nhu lop phu SUA cua trang THONG SO).
  lv_obj_t *adj[2] = { s_po_minus, s_po_plus };
  const int8_t dir[2] = { -1, +1 };
  for (uint8_t k = 0; k < 2; k++) {
    lv_obj_add_event_cb(adj[k], cb_po_adj, LV_EVENT_PRESSED, (void *)(intptr_t)dir[k]);
    lv_obj_add_event_cb(adj[k], cb_po_adj, LV_EVENT_LONG_PRESSED_REPEAT, (void *)(intptr_t)dir[k]);
  }
}

void preheat_edit_open(uint8_t slot) {
  if (slot >= ui_preheat_count()) return;
  uint16_t h = 0, b = 0;
  uint8_t  f = 0;
  if (!ui_preheat_get(slot, &h, &b, &f)) return;
  if (!s_po_box) po_create();
  if (!s_po_box) return;                      // het bo nho LVGL: khong mo (khong crash)

  s_po_slot  = slot;
  s_po_h = h; s_po_b = b; s_po_f = f;
  s_po_field = 0;
  s_po_step  = 1;
  s_po_rep   = false;
  s_po_open  = true;

  po_refresh();
  if (s_po_bd)  lv_obj_move_foreground(s_po_bd);
  if (s_po_box) lv_obj_move_foreground(s_po_box);
  lv_obj_clear_flag(s_po_bd, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(s_po_box, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(s_po_box);             // ep tinh layout ngay (khong doi vong refresh)
  if (s_group && s_po_minus) lv_group_focus_obj(s_po_minus);
}

// ===========================================================================
//  BAN PHIM dat ten vat lieu moi (chi co khi env bat LV_USE_KEYBOARD/TEXTAREA)
// ===========================================================================
#if LV_USE_KEYBOARD && LV_USE_TEXTAREA
static lv_obj_t *s_kb_bd, *s_kb_ta, *s_kb_kb;

void preheat_kb_hide(void) {
  if (!s_kb_bd) return;
  lv_obj_del_async(s_kb_bd);                  // xoa trong event cua chinh no -> phai dung _async
  s_kb_bd = NULL; s_kb_ta = NULL; s_kb_kb = NULL;
  refocus_page();
}

bool preheat_kb_active(void) { return s_kb_bd != NULL; }

static void cb_pre_kb(lv_event_t *e) {
  switch (lv_event_get_code(e)) {
    case LV_EVENT_READY:                      // phim "v" (dong y) tren ban phim
      if (s_kb_ta) {
        const char *t = lv_textarea_get_text(s_kb_ta);
        const bool ok = ui_preheat_add(t ? t : "");   // rev++ -> cac trang tu ve lai
        WIFILOG("[PRE] ban phim dat ten: '%s' -> %s\n", t ? t : "",
                ok ? "da them" : T(UI_T_PRE_ADD_FULL));
      }
      preheat_kb_hide();
      break;
    case LV_EVENT_CANCEL:                     // phim "x" (huy)
      preheat_kb_hide();
      break;
    default: break;
  }
}

void preheat_add_open(void) {
  if (s_kb_bd) return;
  if (ui_preheat_count() >= CYDLCD_PRE_MAX) {
    WIFILOG("[PRE] da du %u vat lieu - xoa bot truoc khi them\n", (unsigned)CYDLCD_PRE_MAX);
    return;
  }
  s_kb_bd = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(s_kb_bd, 0, 0);
  lv_obj_set_size(s_kb_bd, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  lv_obj_set_style_bg_color(s_kb_bd, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_kb_bd, LV_OPA_70, 0);
  lv_obj_set_style_border_width(s_kb_bd, 0, 0);
  lv_obj_set_style_radius(s_kb_bd, 0, 0);
  lv_obj_set_style_pad_all(s_kb_bd, 0, 0);
  lv_obj_clear_flag(s_kb_bd, LV_OBJ_FLAG_SCROLLABLE);
  // DAY lop phu len TRUOC: neu trang dang mo bi DUNG LAI (page_rebuild) sau khi ban phim mo thi trang
  // do (con tao sau) se duoc ve DE LEN ban phim (LVGL ve con theo thu tu tao) -> ban phim bi che.
  lv_obj_move_foreground(s_kb_bd);

  s_kb_ta = lv_textarea_create(s_kb_bd);
  lv_obj_set_pos(s_kb_ta, 2, 4);
  lv_obj_set_size(s_kb_ta, UI_LCD_HOR_RES - 4, KB_TA_H);
  lv_textarea_set_one_line(s_kb_ta, true);
  lv_textarea_set_max_length(s_kb_ta, PRE_NAME_LEN - 1);   // 8 ky tu (ten vat lieu)
  lv_textarea_set_text(s_kb_ta, "");                       // rong -> ui_preheat_add() tu dat "MAT n"
  lv_obj_add_event_cb(s_kb_ta, cb_pre_kb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_kb_ta, cb_pre_kb, LV_EVENT_CANCEL, NULL);

  s_kb_kb = lv_keyboard_create(s_kb_bd);
  // RONG HET man + CAN DAY: phim fill het le trai/phai, hang cuoi khong bao gio bi cat (xem KB_H).
  lv_obj_set_size(s_kb_kb, UI_LCD_HOR_RES, KB_H);
  lv_obj_align(s_kb_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(s_kb_kb, s_kb_ta);
  style_keyboard(s_kb_kb, s_kb_ta);        // nen + vien + mau phim (mac dinh ban phim trong suot)
  lv_obj_add_event_cb(s_kb_kb, cb_pre_kb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_kb_kb, cb_pre_kb, LV_EVENT_CANCEL, NULL);
  if (s_group) { lv_group_add_obj(s_group, s_kb_kb); lv_group_focus_obj(s_kb_kb); }
}
#else
void preheat_kb_hide(void) {}
bool preheat_kb_active(void) { return false; }
void preheat_add_open(void) {
  WIFILOG("[PRE] env nay khong co ban phim (LV_USE_KEYBOARD=0) - them vat lieu bang lenh "
          "`UI PRE ADD <ten>` tren COM8\n");
}
#endif  // LV_USE_KEYBOARD && LV_USE_TEXTAREA

// ===========================================================================
//  CHAN DOAN: `UI PRE` (in danh sach) / `UI PRE <k>` (bam nut preset k o man NHIET DO - nhu cu) /
//  `UI PRE ADD <ten>` / `UI PRE DEL <i>` / `UI PRE SET <i> <h> <b> <f>` / `UI PRE NAME <i> <ten>` /
//  `UI PRE OPEN <i>` (mo hop sua 3-trong-1) / `UI PRE KB` (mo ban phim dat ten).
//  VI SAO CAN: khong cham duoc vao LCD tu PC -> day la cach DUY NHAT them/bot/sua vat lieu tu xa.
// ===========================================================================
void preheat_test_cmd(const char *arg) {
  while (arg && *arg == ' ') arg++;
  if (!arg || !*arg) {
    Serial.printf("[UI] PRE: %u/%u vat lieu (NVS \"ph2\")\n", (unsigned)ui_preheat_count(),
                  (unsigned)CYDLCD_PRE_MAX);
    for (uint8_t i = 0; i < ui_preheat_count(); i++) {
      uint16_t h = 0, b = 0;
      uint8_t  f = 0;
      ui_preheat_get(i, &h, &b, &f);
      Serial.printf("[UI] PRE   %u '%s' nozzle=%u bed=%u fan=%u\n",
                    (unsigned)i, ui_preheat_name(i), (unsigned)h, (unsigned)b, (unsigned)f);
    }
    Serial.printf("[UI] PRE hop sua 3-trong-1: %s | ban phim: %s\n",
                  preheat_edit_active() ? "DANG MO" : "dong", preheat_kb_active() ? "DANG MO" : "dong");
    return;
  }
  // "<so>" = bam nut preset thu k o man NHIET DO (duong su kien that - xem temp_test_preheat)
  if (arg[0] >= '0' && arg[0] <= '9') {
    int k = -1;
    sscanf(arg, "%d", &k);
    if (k >= 0) temp_test_preheat((uint8_t)k);
    return;
  }
  if (strncasecmp(arg, "ADD", 3) == 0) {
    const char *nm = arg + 3;
    while (*nm == ' ') nm++;
    const bool ok = ui_preheat_add(nm);
    Serial.printf("[UI] PRE ADD '%s' -> %s (%u vat lieu)\n", nm, ok ? "OK" : "DAY (da du 6)",
                  (unsigned)ui_preheat_count());
    return;
  }
  if (strncasecmp(arg, "DEL", 3) == 0) {
    int i = -1;
    sscanf(arg + 3, "%d", &i);
    const bool ok = (i >= 0) && ui_preheat_remove((uint8_t)i);
    Serial.printf("[UI] PRE DEL %d -> %s (%u vat lieu)\n", i, ok ? "OK" : "KHONG (i sai / con 1)",
                  (unsigned)ui_preheat_count());
    return;
  }
  if (strncasecmp(arg, "SET", 3) == 0) {
    int i = -1, h = 0, b = 0, f = 0;
    if (sscanf(arg + 3, "%d %d %d %d", &i, &h, &b, &f) != 4 || i < 0) {
      Serial.println("[UI] dung: UI PRE SET <i> <nozzle> <bed> <fan>");
      return;
    }
    const bool ok = ui_preheat_set((uint8_t)i, (uint16_t)h, (uint16_t)b, (uint8_t)f);
    Serial.printf("[UI] PRE SET %d %d/%d/%d -> %s\n", i, h, b, f, ok ? "OK" : "i sai");
    return;
  }
  if (strncasecmp(arg, "NAME", 4) == 0) {
    int i = -1;
    char nm[PRE_NAME_LEN + 8] = "";
    if (sscanf(arg + 4, "%d %8s", &i, nm) != 2 || i < 0) {
      Serial.println("[UI] dung: UI PRE NAME <i> <ten>");
      return;
    }
    const bool ok = ui_preheat_rename((uint8_t)i, nm);
    Serial.printf("[UI] PRE NAME %d '%s' -> %s\n", i, nm, ok ? "OK" : "i sai");
    return;
  }
  if (strncasecmp(arg, "OPEN", 4) == 0) {
    int i = -1;
    sscanf(arg + 4, "%d", &i);
    if (i < 0) { Serial.println("[UI] dung: UI PRE OPEN <i>"); return; }
    preheat_edit_open((uint8_t)i);
    Serial.printf("[UI] PRE OPEN %d -> %s '%s' %u/%u/%u | o chon=%u buoc=%u\n", i,
                  preheat_edit_active() ? "MO" : "khong mo duoc", ui_preheat_name((uint8_t)i),
                  (unsigned)s_po_h, (unsigned)s_po_b, (unsigned)s_po_f,
                  (unsigned)s_po_field, (unsigned)po_step_now());
    return;
  }
  if (strncasecmp(arg, "KB", 2) == 0) {
    preheat_add_open();
    Serial.printf("[UI] PRE KB -> ban phim: %s\n", preheat_kb_active() ? "DANG MO" : "khong mo");
    return;
  }
  Serial.println("[UI] dung: UI PRE [<k>|ADD <ten>|DEL <i>|SET <i> h b f|NAME <i> <ten>|OPEN <i>|KB]");
}

#endif  // CYDLCD_USE_LVGL
