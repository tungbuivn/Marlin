/**
 * ui_page_mesh.cpp -- man BED MESH cua UI LVGL (man hinh CYD): luoi bed mesh
 * dung het be rong + 2 hang nut UBL (G29). Lop phu/xac nhan o ui_boxes.cpp.
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
  if (col >= cydlcd.mesh_nx || row >= cydlcd.mesh_ny) return;

  const int32_t z = cydlcd.mesh[col][row];          // cydlcd_link.cpp: mesh[cot][hang]
  lv_color_t bg = COL_BTN;
  if (z > MESH_HI_UM)      bg = lv_color_hex(0x1E6B3A);   // xanh: diem cao
  else if (z < MESH_LO_UM) bg = lv_color_hex(0x7A2626);   // do  : diem thap
  dsc->rect_dsc->bg_color = bg;
  dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
}

void page_mesh_create(lv_obj_t *p) {
  if (!(cydlcd.hello_flags & CYDLCD_HAS_MESH)) {
    mk_spacer(p, 1, 30, 0);                      // day chu thich xuong giua vung noi dung
    me_note = mk_label_w(p, "khong ho tro (CYDLCD_HAS_MESH=0)", COL_WARN, PAGE_INN_W);
    (void)me_note;
    return;
  }

  me_info = mk_label_w(p, "mesh -", COL_TXT, PAGE_INN_W);

  // Luoi la CON TRUC TIEP cua trang (da bo cot nut ben phai) -> rong het 312 px.
  me_table = lv_table_create(p);
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
  lv_obj_t *ru1 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 28, 0, GAP);
  mk_btn_grow(ru1, "G29 P1",   28, cb_action, (void *)(intptr_t)A_G29_P1);
  mk_btn_grow(ru1, "G29 P3T0", 28, cb_action, (void *)(intptr_t)A_G29_FILL);
  mk_btn_grow(ru1, "G29 P4RT", 28, cb_action, (void *)(intptr_t)A_G29_FT);

  lv_obj_t *ru2 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 28, 0, GAP);
  mk_btn_grow(ru2, "UBL ON",   28, cb_action, (void *)(intptr_t)A_G29_ON);
  mk_btn_grow(ru2, "UBL OFF",  28, cb_action, (void *)(intptr_t)A_G29_OFF);
  mk_btn_grow(ru2, "SAVE S0",  28, cb_action, (void *)(intptr_t)A_G29_SAVE);
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
  snprintf(b, sizeof(b), "mesh %ux%u  rows %u  %s..%s%s", (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny,
           (unsigned)cydlcd.mesh_rows, b1, b2, cydlcd.mesh_valid ? "  valid" : "  -");
  set_text(me_info, b);
}

// Huy man BED MESH (xem quy uoc o ui_lvgl_int.h): dat NULL bang/nhan va QUEN kich thuoc luoi da ve.
// VI SAO phai quen s_mesh_nx/s_mesh_ny: page_mesh_update() chi dat lai so cot/hang khi kich thuoc luoi
// DOI so voi gia tri da nho - ma bang moi luon duoc tao 1 cot x 1 hang, nen neu giu cache cu thi
// luoi se KHONG duoc dung lai (bang 1x1 = khong hien gi).
void page_mesh_destroy(void) {
  me_table = NULL;
  me_info  = NULL;
  me_note  = NULL;
  s_mesh_nx = 0;
  s_mesh_ny = 0;
}

#endif  // CYDLCD_USE_LVGL

