/**
 * ui_page_mesh.cpp -- man BED MESH cua UI LVGL (man hinh CYD): luoi bed mesh
 * dung het be rong + 2 hang nut UBL (G29). Lop phu/xac nhan o ui_boxes.cpp.
 *
 * Tach tu ui_lvgl.cpp (chang 4/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "ui_lang.h"                // T() - da ngon ngu

#include <stdio.h>                  // snprintf
#include <string.h>                 // strcmp
#include <Arduino.h>                // Serial.printf (log chan doan WIFILOG ben duoi)

#if CYDLCD_USE_LVGL

// ===========================================================================
//  MESH -- luoi bed mesh dung HET be rong + 2 hang nut UBL (G29)
//    info 17 + luoi 124 + hang nut 28 + hang nut 28 + 3*GAP 6 = 203 <= PAGE_INN_H 206
//  Da BO theo yeu cau:
//    - nut REFRESH (Marlin TU day lai luoi khi mesh doi / khi link hoi phuc; enum A_REQ_MESH
//      van giu lai trong do_action() cho gon, hien khong con nut nao dung),
//    - 3 dong chu thich mau o cot phai ("xanh = cao" / "do = thap" / "+-0.05mm"),
//    - dong chu thich duoi luoi ("- o ghi mm - REFRESH = xin lai luoi").
//  Nho bo cot phai, luoi rong tu 214 -> 312 px (o rong hon ~45%) va cao 104 -> 124 px (o cao
//  hon ~19%: voi luoi 7x7 la 30x15 -> 44x18 px). Tong 203 <= 206 nen KHONG tran -> thanh day
//  PREV/NEXT khong hien (bot_sync van chay dung nhu moi man khac).
// ===========================================================================
#define MESH_TABLE_H 124
#define MESH_TABLE_W PAGE_INN_W                         // 312: luoi dung het be rong
#define MESH_CELL_W  38                                 // be rong toi thieu 1 o: du "-0.20" (33 px) + 2*2 pad

static lv_obj_t *me_table, *me_info, *me_note, *me_btn_on, *me_btn_off;
// 2026-10-17: rect cua tung o (lay trong mesh_draw_cb) de cuon o vua cham ve TAM bang.
static lv_area_t s_cell_area[CYDLCD_MESH_MAX][CYDLCD_MESH_MAX];
static uint8_t   s_mesh_nx = 0, s_mesh_ny = 0;
static bool      s_mesh_on_prev = false, s_mesh_on_valid = false;
static bool      s_mesh_valid_prev = false;

// ===========================================================================
//  CHE DO SUA (EDIT) - yeu cau nguoi dung 2026-10-15:
//    "them nut edit vao bed mesh day la nut toogle, khi click thi fill mau, click lan nua ve ban dau, o che
//     do edit nguoi dung co the [cham] 1 o de thay doi gia tri giong trang setting, page navigation nam o
//     4 huong trai, tren, phai, duoi de dich chuyen vung view"
//  - Nut EDIT: TOGGLE - bat thi to NEN mau COL_ACCENT (fill), tat thi ve mau nut thuong.
//  - Bat EDIT: cham vao 1 o -> mo hop sua (giong lop phu SUA cua trang THONG SO: chon buoc 0.01/0.05/0.1 mm,
//    nut -/+ doi gia tri, OK gui `M421 I<cot> J<hang> Z<mm>` cho Marlin (UBL: M421.cpp - co ping ExtUI nen
//    Marlin TU day lai luoi), HUY thi thoi).
//  - 4 nut dieu huong < ^ v > (chi hien khi bat EDIT) de DICH CHUYEN vung nhin cua bang (cuon bang).
// ===========================================================================
#define ME_NAV_W   30                       // be rong/cao nut dieu huong
#define ME_NAV_H   24
#define ME_EDIT_W  56                       // nut EDIT (goc tren phai cua bang)
#define ME_EDIT_H  20
#define ME_STEP_N  3
static const int32_t ME_STEP_UM[ME_STEP_N] = { 10, 50, 100 };            // 0.01 / 0.05 / 0.1 mm (um)
static const char *const ME_STEP_TXT[ME_STEP_N] = { "0.01", "0.05", "0.1" };

static lv_obj_t *me_layer;                  // lop phu dinh vi tuyet doi tren bang (nut EDIT + dieu huong)
static lv_obj_t *me_edit_btn;
static lv_obj_t *me_nav[4];                 // 0 trai, 1 tren, 2 phai, 3 duoi
static bool      me_edit = false;
static lv_obj_t *me_mask, *me_box;          // lop phu + hop sua 1 o
static lv_obj_t *me_box_title, *me_box_val;
static lv_obj_t *me_step_btn[ME_STEP_N];
static uint8_t   me_c = 0, me_r = 0;        // o dang sua
static int32_t   me_raw = 0;                // gia tri dang sua (um, cung don vi cydlcd.mesh[][])
static uint8_t   me_step_k = 1;             // buoc dang chon (mac dinh 0.05 mm)

// To nut buoc dang chon (giong trang THONG SO: nen COL_ACCENT cho muc dang chon).
// mk_btn_set_sel() doi CA NEN va MAU CHU (nen accent la mau sang -> chu phai toi).
static void me_step_marks(void) {
  for (uint8_t k = 0; k < ME_STEP_N; k++)
    mk_btn_set_sel(me_step_btn[k], k == me_step_k);
}

// Hien lai gia tri dang sua trong hop.
static void me_box_refresh(void) {
  char a[16], b[32];
  fmt_scaled(a, sizeof(a), me_raw, 1000, 3);           // um -> mm, 3 so le (du chinh xac tung um)
  snprintf(b, sizeof(b), "Z = %s mm", a);
  set_text(me_box_val, b);
  snprintf(b, sizeof(b), "MESH o %u,%u", (unsigned)me_c, (unsigned)me_r);
  set_text(me_box_title, b);
  me_step_marks();
}

static void me_box_hide(void) {
  if (me_mask) lv_obj_add_flag(me_mask, LV_OBJ_FLAG_HIDDEN);
  if (me_box)  lv_obj_add_flag(me_box, LV_OBJ_FLAG_HIDDEN);
}

// Hien hop sua cho o vua cham (me_c/me_r/me_raw da dat truoc).
static void me_box_show(void) {
  if (!me_box) return;
  me_box_refresh();
  // Day len truoc cho chac (loi "dialog bi tut ra phia sau" o lop phu SUA cua SETTINGS - xem ui_page_settings.cpp:
  // ov_create). Hop nay la con cua TRANG va duoc tao SAU bang nen binh thuong da o tren; van goi de an toan.
  if (me_mask) { lv_obj_move_foreground(me_mask); lv_obj_clear_flag(me_mask, LV_OBJ_FLAG_HIDDEN); }
  lv_obj_move_foreground(me_box);
  lv_obj_clear_flag(me_box, LV_OBJ_FLAG_HIDDEN);
  if (s_group && me_box) lv_group_focus_obj(me_step_btn[me_step_k]);
}

// Cham vao 1 o cua bang -> (chi khi bat EDIT) mo hop sua o do.
static void cb_mesh_cell(lv_event_t *e) {
  (void)e;
  if (!me_edit || !me_table) return;
  uint16_t r = 0, c = 0;
  lv_table_get_selected_cell(me_table, &r, &c);
  // LOG MOI lan cham 1 o (ke ca khi EDIT dang TAT) - de chan doan "cham o khong thay gi" tu COM8:
  // neu KHONG thay dong nay thi cu cham KHONG toi duoc bang (bi vat khac an), con co dong nay ma hop khong
  // mo thi loi o phia hop/lop phu.
  WIFILOG("[UI] MESH: cham o %u,%u (EDIT=%d)\n", (unsigned)c, (unsigned)r, (int)me_edit);
  if (c >= cydlcd.mesh_nx || r >= cydlcd.mesh_ny) return;
  me_c = (uint8_t)c;
  me_r = (uint8_t)r;
  me_raw = cydlcd.mesh[c][r];
  // 2026-10-17 (yeu cau nguoi dung): BO hop sua o. Cham vao o nao thi CUON O DO VE TAM bang
  // (lo ra cac o xung quanh). LVGL tu KEP mep: o o canh van bam canh, khong bi keo vao tam.
  {
    const lv_area_t &a = s_cell_area[c][r];
    const lv_coord_t cw = a.x2 - a.x1 + 1, ch = a.y2 - a.y1 + 1;
    if (cw > 0 && ch > 0) {
      const lv_coord_t tw = lv_obj_get_width(me_table), th = lv_obj_get_height(me_table);
      lv_obj_scroll_to(me_table, a.x1 + cw / 2 - tw / 2, a.y1 + ch / 2 - th / 2, LV_ANIM_ON);
    }
  }
  WIFILOG("[UI] MESH: cham o %u,%u -> cuon ve tam\n", (unsigned)c, (unsigned)r);
}

// Ap trang thai `me_edit` len giao dien: nut EDIT (fill mau), 4 nut dieu huong (hien/an), cuon cua bang.
// Tach rieng de CA nut bam tren man hinh LAN lenh chan doan `UI MCELL` deu dung chung 1 duong.
static void mesh_edit_apply(void) {
  if (me_edit_btn) mk_btn_set_sel(me_edit_btn, me_edit);   // doi CA NEN va CHU
  for (uint8_t k = 0; k < 4; k++)
    if (me_nav[k]) {
      if (me_edit) lv_obj_clear_flag(me_nav[k], LV_OBJ_FLAG_HIDDEN);
      else         lv_obj_add_flag(me_nav[k], LV_OBJ_FLAG_HIDDEN);
    }
  // !!! TAT CUON CUA BANG KHI EDIT !!!
  // VI SAO: lv_table chi gui LV_EVENT_VALUE_CHANGED o LV_EVENT_RELEASED khi `lv_indev_get_scroll_obj()==NULL`
  // (lv_table.c:587-593). Khi bang CON cuon duoc (flag SCROLLABLE) thi mot cu cham hoi xe dich cung bi coi
  // la KEO -> khong co su kien -> "cham 1 o khong thay gi" (loi nguoi dung bao 2026-10-15). O che do EDIT,
  // viec dich vung nhin da co 4 nut < ^ v >, nen tat cuon de moi cu cham la 1 lan CHON O.
  if (me_table) {
    if (me_edit) lv_obj_clear_flag(me_table, LV_OBJ_FLAG_SCROLLABLE);
    else         lv_obj_add_flag(me_table, LV_OBJ_FLAG_SCROLLABLE);
  }
  if (!me_edit) me_box_hide();
}

// Toggle EDIT: bat -> to nen (fill) + hien 4 nut dieu huong; tat -> ve nhu cu.
static void cb_mesh_edit_toggle(lv_event_t *e) {
  (void)e;
  me_edit = !me_edit;
  mesh_edit_apply();
  WIFILOG("[UI] MESH EDIT: %s\n", me_edit ? "BAT (cham o de sua, 4 nut dieu huong hien ra)"
                                          : T(UI_T_MESH_OFF));
}

// 4 nut dieu huong: dich vung nhin cua bang (cuon) theo trai/tren/phai/duoi.
static void cb_mesh_nav(lv_event_t *e) {
  if (!me_table) return;
  const intptr_t d = (intptr_t)lv_event_get_user_data(e);
  lv_coord_t dx = 0, dy = 0;
  switch (d) {
    case 0: dx = -40; break;                 // trai
    case 1: dy = -20; break;                 // tren
    case 2: dx = +40; break;                 // phai
    default: dy = +20; break;                // duoi
  }
  lv_obj_scroll_by(me_table, dx, dy, LV_ANIM_OFF);
  WIFILOG("[UI] MESH VIEW: dich %s (bang dang o x=%d y=%d)\n",
          d == 0 ? "trai" : (d == 1 ? "tren" : (d == 2 ? "phai" : "duoi")),
          (int)lv_obj_get_scroll_x(me_table), (int)lv_obj_get_scroll_y(me_table));
}

// Chon buoc sua.
static void cb_mesh_step(lv_event_t *e) {
  me_step_k = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (me_step_k >= ME_STEP_N) me_step_k = 0;
  me_step_marks();
}

// -/+ : doi gia tri theo buoc dang chon (gioi han +-2 mm).
static void cb_mesh_adj(lv_event_t *e) {
  const int8_t s = (int8_t)(intptr_t)lv_event_get_user_data(e);
  int32_t v = me_raw + (int32_t)s * ME_STEP_UM[me_step_k];
  if (v >  2000) v =  2000;
  if (v < -2000) v = -2000;
  me_raw = v;
  me_box_refresh();
}

// OK: gui `M421 I<cot> J<hang> Z<mm>` (UBL). Marlin ping ExtUI onMeshUpdate -> TU day lai luoi.
static void cb_mesh_ok(lv_event_t *e) {
  (void)e;
  char a[16], cmd[40];
  fmt_scaled(a, sizeof(a), me_raw, 1000, 3);
  snprintf(cmd, sizeof(cmd), "M421 I%u J%u Z%s", (unsigned)me_c, (unsigned)me_r, a);
  cydlcd_link_send_gcode(cmd);
  cydlcd_link_request((uint8_t)CYDLCD_REQ_MESH);      // xin lai cho chac (Marlin cung tu day khi mesh doi)
  WIFILOG("[UI] MESH EDIT: gui \"%s\"\n", cmd);
  me_box_hide();
}

static void cb_mesh_cancel(lv_event_t *e) { (void)e; me_box_hide(); }

// Nguong to mau: |z| > 0.05mm thi to xanh (cao) / do (thap)
#define MESH_HI_UM  50
#define MESH_LO_UM (-50)
// 2026-10-17 (yeu cau nguoi dung: "to mau cam cac diem nghi ngo"):
// Mat ban that Lien tuc, khong co "gai". Mot diem lech qua xa so voi trung binh cac diem ke
// gan nhu chac chan la do sai (probe nhieu / bui ban / diem mesh cu).
#define MESH_SUSPECT_UM 100      // 0.10 mm

// LVGL 8.3: LV_EVENT_DRAW_PART_BEGIN cho o cua bang -> dsc->id = chi so o
// (id = row * so_cot + col). Vat lieu to mau rieng tung o.
// 2026-10-17: diem nay co "nghi ngo" khong? So voi TRUNG BINH 8 diem ke (bo qua diem ngoai luoi).
// Bien chi co it diem ke -> can >= 3 diem moi ket luan (tranh bao nham o goc luoi).
static bool mesh_cell_suspect(const uint8_t c, const uint8_t r) {
  if (c >= cydlcd.mesh_nx || r >= cydlcd.mesh_ny) return false;
  int32_t sum = 0;
  uint8_t n = 0;
  for (int8_t dc = -1; dc <= 1; dc++)
    for (int8_t dr = -1; dr <= 1; dr++) {
      if (!dc && !dr) continue;
      const int16_t cc = (int16_t)c + dc, rr = (int16_t)r + dr;
      if (cc < 0 || rr < 0 || cc >= (int16_t)cydlcd.mesh_nx || rr >= (int16_t)cydlcd.mesh_ny) continue;
      sum += cydlcd.mesh[cc][rr];
      n++;
    }
  if (n < 3) return false;
  const int32_t avg = sum / (int32_t)n;
  int32_t d = (int32_t)cydlcd.mesh[c][r] - avg;
  if (d < 0) d = -d;
  return d > MESH_SUSPECT_UM;
}

static void mesh_draw_cb(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!dsc || dsc->part != LV_PART_ITEMS || !dsc->rect_dsc) return;

  lv_obj_t *tbl = lv_event_get_target(e);
  const uint16_t cols = lv_table_get_col_cnt(tbl);
  if (!cols) return;

  const uint32_t row = dsc->id / cols;
  const uint32_t col = dsc->id % cols;
  if (col >= cydlcd.mesh_nx || row >= cydlcd.mesh_ny) return;

  const int32_t z = cydlcd.mesh[col][row];          // cydlcd_link.cpp: mesh[cot][hang]
  lv_color_t bg = COL_BTN;
  // 2026-10-17: DIEM NGHI NGO -> to CAM, uu tien hon mau cao/thap (canh bao quan trong hon).
  if (mesh_cell_suspect((uint8_t)col, (uint8_t)row)) bg = lv_color_hex(0xF9A825);
  else if (z > MESH_HI_UM)      bg = lv_color_hex(0x1E6B3A);   // xanh: diem cao
  else if (z < MESH_LO_UM) bg = lv_color_hex(0x7A2626);   // do  : diem thap
  dsc->rect_dsc->bg_color = bg;
  dsc->rect_dsc->bg_opa   = LV_OPA_COVER;

  // 2026-10-17 (yeu cau nguoi dung): luu RECT cua tung o de khi cham vao o nao thi CUON o do ve TAM.
  if (dsc->draw_area) s_cell_area[col][row] = *dsc->draw_area;
}

void page_mesh_create(lv_obj_t *p) {
  // XIN LAI LUOI moi khi mo trang neu chua co du lieu (yeu cau tu loi 2026-10-15: nguoi dung vao trang
  // BED MESH thay TRANG). Luoi chi duoc Marlin day khi co HELLO / khi do mesh; neu panel lo mat HELLO thi
  // `mesh_nx = 0` -> truoc day trang chi bao "khong ho tro" va KHONG BAO GIO xin lai. Nay xin ngay.
  if (!cydlcd.mesh_nx) {
    cydlcd_link_request((uint8_t)CYDLCD_REQ_MESH);
    WIFILOG("[UI] MESH: chua co luoi -> xin lai tu Marlin (CYDLCD_REQ_MESH)\n");
  }
  if (!(cydlcd.hello_flags & CYDLCD_HAS_MESH)) {
    mk_spacer(p, 1, 30, 0);                      // day chu thich xuong giua vung noi dung
    me_note = mk_label_w(p, T(UI_T_MESH_UNSUPPORTED), COL_WARN, PAGE_INN_W);
    (void)me_note;
    return;
  }

  me_info = mk_label_w(p, "mesh -", COL_TXT, PAGE_INN_W);

  // Luoi la CON TRUC TIEP cua trang (da bo cot nut ben phai) -> rong het 312 px.
  me_table = lv_table_create(p);
  mono_obj(me_table);                        // font CHUNG cua UI (yeu cau thong nhat 1 ho font)
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

  // Da bo: nut REFRESH + 3 dong chu thich mau (cot phai) va dong chu thich duoi luoi.

  // P1 - UBL: 2 hang x 3 nut (G29 P1 / P3T0 / P4RT roi bat-tat UBL + luu slot 0)
  // 2026-10-17 (yeu cau nguoi dung): BO nut "G29 P3T0" - viec dien cac diem trong/bien gio do nut
  // SAVE tu lam (SAVE -> G29 P3 roi G29 S0). Hang 1 con: G29 P1 | EDIT | PROBE.
  lv_obj_t *ru1 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 28, 0, GAP);
  mk_btn_grow(ru1, "G29 P1",   28, cb_action, (void *)(intptr_t)A_G29_P1);
  mk_btn_grow(ru1, "EDIT",     28, cb_action, (void *)(intptr_t)A_G29_FT);
  mk_btn_grow(ru1, "PROBE",    28, cb_action, (void *)(intptr_t)A_MESH_PROBE);

  lv_obj_t *ru2 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 28, 0, GAP);
  // 2026-10-17 (yeu cau nguoi dung): UBL ON/OFF GOP THANH MOT NUT, giong nut AUTO cua man TRAMMING:
  //   2 nut CHONG NHAU + an/hien: "UBL ON" nen XANH, "UBL OFF" nen DO. Text ON/OFF nam o CUOI.
  // Dat trong khay trong suot de 2 nut cung mot o tren hang flex.
  {
    lv_obj_t *kheap = lv_obj_create(ru2);
    lv_obj_set_size(kheap, 96, 28);
    lv_obj_clear_flag(kheap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(kheap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(kheap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(kheap, 0, 0);
    lv_obj_set_style_pad_all(kheap, 0, 0);
    me_btn_on  = mk_btn(kheap, "UBL ON",  96, 28, cb_action, (void *)(intptr_t)A_G29_ON);
    me_btn_off = mk_btn(kheap, "UBL OFF", 96, 28, cb_action, (void *)(intptr_t)A_G29_OFF);
    lv_obj_set_pos(me_btn_on,  0, 0);
    lv_obj_set_pos(me_btn_off, 0, 0);
    mk_btn_set_bg(me_btn_on,  COL_OK);      // xanh = ON
    mk_btn_set_bg(me_btn_off, COL_ERR);     // do   = OFF
    lv_obj_add_flag(me_btn_on,  LV_OBJ_FLAG_USER_1);
    lv_obj_add_flag(me_btn_off, LV_OBJ_FLAG_USER_1);
  }
  // 2026-10-17: SAVE gio TU GOI G29 P3 (dien cac diem trong/bien) roi G29 S0 (luu slot 0) - xem do_action.
  mk_btn_grow(ru2, "SAVE",     28, cb_action, (void *)(intptr_t)A_G29_SAVE);

  // ---------------------------------------------------------------------------
  // LOP PHU DINH VI TUYET DOI TREN BANG: nut EDIT (toggle) + 4 nut dieu huong + hop sua 1 o.
  // PHai dat SAU khi tao bang/ cac hang nut de tinh duoc toa do that cua bang, va phai co
  // LV_OBJ_FLAG_IGNORE_LAYOUT de flex COLUMN cua trang BO QUA lop nay (khong day cac vat khac le).
  // ---------------------------------------------------------------------------
  // 2026-10-17 (yeu cau nguoi dung): BO lop phu EDIT toggle + 4 mui ten dieu huong.
  // Di chuyen tren luoi = cham vao o (xem cb_mesh_cell). Nut EDIT o hang tren la UBL manual edit.

}

// CHAN DOAN (lenh `UI MCELL <cot> <hang>` tren COM8): mo hop sua cho 1 o Y NHU VUA CHAM VAO O DO.
// VI SAO CAN: khong the "cham" bang tay tu PC; lenh nay di dung duong mo hop (khong tu gui M421) nen kiem tra
// duoc hop sua + nut OK/HUY tu xa. Vi du: `UI MCELL 3 3` -> hop sua o cot 3 hang 3.
// 2026-10-17 (yeu cau nguoi dung: "nut probe da di chuyen dau probe den diem duoc chon tren grid chua?"):
// TRUOC DAY nut PROBE chi gui G30 -> probe tai cho nozzle dang dung, KHONG di chuyen.
// Nay:
//   1. M420 S0                  tat bu mesh (giong G35)
//   2. G42 I<cot> J<hang> P1    di chuyen de DAU DO nam tren diem mesh dang chon (P1 = tinh offset probe)
//   3. G30 x2                   probe lai 2 lan
//   4. M420 S1                  bat lai bu mesh
void page_mesh_probe_selected(void) {
  if (!cydlcd.mesh_nx || !cydlcd.mesh_ny) return;
  if (me_c >= cydlcd.mesh_nx || me_r >= cydlcd.mesh_ny) return;
  char g[40];
  cydlcd_link_send_gcode("M420 S0");
  snprintf(g, sizeof(g), "G42 I%u J%u P1", (unsigned)me_c, (unsigned)me_r);
  cydlcd_link_send_gcode(g);
  cydlcd_link_send_gcode("G30");
  cydlcd_link_send_gcode("G30");
  cydlcd_link_send_gcode("M420 S1");
  WIFILOG("[UI] MESH PROBE: o %u,%u -> %s\n", (unsigned)me_c, (unsigned)me_r, g);
}

void page_mesh_update(void) {
  if (!me_table) return;

  // So cot/hang doi -> tao lai khung bang (chi khi kich thuoc luoi doi)
  if (cydlcd.mesh_nx && cydlcd.mesh_ny && (cydlcd.mesh_nx != s_mesh_nx || cydlcd.mesh_ny != s_mesh_ny)) {
    s_mesh_nx = cydlcd.mesh_nx;
    s_mesh_ny = cydlcd.mesh_ny;
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
  for (uint8_t r = 0; r < cydlcd.mesh_ny; r++) {
    for (uint8_t c = 0; c < cydlcd.mesh_nx; c++) {
      const int32_t z = cydlcd.mesh[c][r];
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
  // Dong thong tin co ca TRANG THAI UBL ON/OFF (phan hoi nguoi dung 2026-10-15: "phan setting bed mesh chua
  // hien thi duoc tinh trang mesh on hay off, bam xong ma khong biet da duoc hay chua"). `cydlcd.mesh_on` =
  // planner.leveling_active cua Marlin (byte thu 4 cua CYDLCD_F_STATUS, day ~1 Hz) nen sau khi bam UBL ON/OFF
  // khoang 1 s dong nay tu doi; dong thoi TO MAU dong nay + 2 nut theo trang thai (xem duoi).
  snprintf(b, sizeof(b), "%ux%u rows %u %s..%s%s  UBL:%s", (unsigned)cydlcd.mesh_nx,
           (unsigned)cydlcd.mesh_ny, (unsigned)cydlcd.mesh_rows, b1, b2,
           cydlcd.mesh_valid ? " valid" : " -", cydlcd.mesh_on ? "ON" : "OFF");
  set_text(me_info, b);

  // ---- TRANG THAI UBL/BED LEVELING: chi cap nhat khi gia tri DOI (khong set style moi vong) ----
  if (!s_mesh_on_valid || s_mesh_on_prev != cydlcd.mesh_on || s_mesh_valid_prev != cydlcd.mesh_valid) {
    s_mesh_on_valid   = true;
    s_mesh_on_prev    = cydlcd.mesh_on;
    s_mesh_valid_prev = cydlcd.mesh_valid;
    // Xanh = dang bat (co bu mesh), Vang = dang tat. Neu BAT ma chua co mesh nao duoc luu thi canh bao do.
    lv_color_t c = cydlcd.mesh_on ? (cydlcd.mesh_valid ? COL_OK : COL_ERR) : COL_WARN;
    lv_obj_set_style_text_color(me_info, c, 0);
    // 2026-10-17: UBL ON/OFF gop 1 nut (2 nut chong nhau, da to mau co dinh) -> chi AN/HIEN.
    if (me_btn_on && me_btn_off) {
      if (cydlcd.mesh_on) {
        lv_obj_clear_flag(me_btn_on,  LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(me_btn_off, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_add_flag(me_btn_on, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(me_btn_off, LV_OBJ_FLAG_HIDDEN);
      }
    }
  }
}

void mesh_test_cell(const char *arg) {
  int c = -1, r = -1;
  if (arg && *arg) sscanf(arg, "%d %d", &c, &r);
  if (c < 0 || r < 0 || c >= (int)cydlcd.mesh_nx || r >= (int)cydlcd.mesh_ny) {
    Serial.printf("[UI] dung: UI MCELL <cot> <hang>  (luoi hien tai %ux%u)\n",
                  (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny);
    return;
  }
  if (!me_edit) {
    // Tu BAT EDIT de kiem tra duoc hop sua tu PC (nut EDIT tren man hinh lam y het).
    me_edit = true;
    mesh_edit_apply();
    Serial.println("[UI] MCELL: EDIT dang TAT -> da tu BAT de test hop sua");
  }
  me_c = (uint8_t)c;
  me_r = (uint8_t)r;
  me_raw = cydlcd.mesh[c][r];
  Serial.printf("[UI] MCELL: mo hop sua o %d,%d (dang %ld um)\n", c, r, (long)me_raw);
  me_box_show();
}

// Huy man BED MESH (xem quy uoc o ui_lvgl_int.h): dat NULL bang/nhan va QUEN kich thuoc luoi da ve.
// VI SAO phai quen s_mesh_nx/s_mesh_ny: page_mesh_update() chi dat lai so cot/hang khi kich thuoc luoi
// DOI so voi gia tri da nho - ma bang moi luon duoc tao 1 cot x 1 hang, nen neu giu cache cu thi
// luoi se KHONG duoc dung lai (bang 1x1 = khong hien gi).
void page_mesh_destroy(void) {
  if (me_table) {
    me_table = NULL;
  }
  me_info  = NULL;
  me_note  = NULL;
  me_btn_on = NULL;
  me_btn_off = NULL;
  // Lop phu EDIT/dieu huong + hop sua 1 o la CON cua trang -> bi xoa theo trang, chi can quen con tro va
  // tra trang thai ve ban dau (lan mo sau phai la "khong sua", nut EDIT mau thuong).
  me_layer = NULL; me_edit_btn = NULL;
  for (uint8_t k = 0; k < 4; k++) me_nav[k] = NULL;
  for (uint8_t k = 0; k < ME_STEP_N; k++) me_step_btn[k] = NULL;
  me_mask = NULL; me_box = NULL; me_box_title = NULL; me_box_val = NULL;
  me_edit = false;
  s_mesh_nx = 0;
  s_mesh_ny = 0;
  s_mesh_on_valid = false;      // de lan mo sau cap nhat lai dong UBL ON/OFF
  s_mesh_valid_prev = false;
}

#endif  // CYDLCD_USE_LVGL

