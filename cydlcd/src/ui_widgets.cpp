/**
 * ui_widgets.cpp -- helper tao widget + format so/chuoi cua UI LVGL (man hinh CYD).
 *
 * Tach tu ui_lvgl.cpp (chang 1/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham/bien dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 * Gom 2 nhom:
 *   - Format: fmt_scaled() (so nguyen da nhan scale, khong dung %f), fmt_time(), state_name(),
 *     temp_lookup().
 *   - Widget: mk_label/_w/_right/_mid, mk_flex, mk_spacer, mk_btn/_grow, mk_hit, mk_icon,
 *     icon_color, mk_page_full, mk_page, focus_mark + on_focus_changed (khung focus), set_text,
 *     style_flat, vn_font/_btn (font co dau tieng Viet), focus_first_in.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#if CYDLCD_USE_LVGL

#include <stdio.h>                  // snprintf (fmt_scaled / fmt_time)
#include <string.h>                 // strcmp (set_text)
#include <Arduino.h>                // Serial.printf (WIFILOG trong style_keyboard - chan doan style ban phim)

// ---------------------------------------------------------------------------
//  Bien chi dung trong file nay -> VAN giu `static`
// ---------------------------------------------------------------------------
static lv_obj_t *s_focus_prev;             // doi tuong dang focus (de ve khung)


// ===========================================================================
//  Format so/chuoi (khong dung %f)
// ===========================================================================
// In `raw` (da nhan `scale`) thanh chuoi thap phan `dec` chu so sau dau cham.
//   vi du fmt_scaled(b, n, 21050, 100, 1) -> "210.5"
//           fmt_scaled(b, n, -120,  1000, 2) -> "-0.12"
//           fmt_scaled(b, n, 1284, 10000, 4) -> "0.1284"   (tham so MPC R/A/F/H can 4 so le nhu Marlin 12864)
void fmt_scaled(char *b, size_t n, int32_t raw, uint32_t scale, uint8_t dec) {
  if (!scale) scale = 1;
  if (dec > 4) dec = 4;                       // SUA 2026-10-15: truoc day chan o 3 -> MPC R/A/F/H bi cat
                                              // con 3 so le ("0.128" thay vi "0.1284"), khac Marlin 12864.
  const bool neg = (raw < 0);
  const uint32_t v = neg ? (uint32_t)(-(raw + 1)) + 1U : (uint32_t)raw;   // tranh tran INT32_MIN
  uint32_t ip = v / scale;
  const uint32_t rem = v % scale;

  uint32_t mul = 1;
  for (uint8_t i = 0; i < dec; i++) mul *= 10U;
  uint32_t frac = (rem * mul + scale / 2U) / scale;                       // co lam tron
  if (frac >= mul) { frac -= mul; ip++; }

  switch (dec) {
    case 0:  snprintf(b, n, "%s%lu", neg ? "-" : "", (unsigned long)ip); break;
    case 1:  snprintf(b, n, "%s%lu.%01lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
    case 2:  snprintf(b, n, "%s%lu.%02lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
    case 3:  snprintf(b, n, "%s%lu.%03lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
    default: snprintf(b, n, "%s%lu.%04lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
  }
}

// 12:34 hoac 1:02:03
void fmt_time(char *b, size_t n, uint32_t s) {
  if (s >= 3600U)
    snprintf(b, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600U),
             (unsigned long)((s / 60U) % 60U), (unsigned long)(s % 60U));
  else
    snprintf(b, n, "%02lu:%02lu", (unsigned long)(s / 60U), (unsigned long)(s % 60U));
}

const char *state_name(uint8_t st) {
  switch (st) {
    case CYDLCD_ST_IDLE:     return "IDLE";
    case CYDLCD_ST_PRINTING: return "PRINTING";
    case CYDLCD_ST_PAUSED:   return "PAUSED";
    case CYDLCD_ST_HOMING:   return "HOMING";
    case CYDLCD_ST_LEVELING: return "LEVELING";
    case CYDLCD_ST_RUNOUT:   return "RUNOUT";
    case CYDLCD_ST_KILLED:   return "KILLED";
    case CYDLCD_ST_BUSY:     return "BUSY";
    default:                return "?";
  }
}

void temp_lookup(const uint8_t id, int16_t *cur, int16_t *tgt) {
  *cur = 0; *tgt = 0;
  // UU TIEN frame PUSH (CYDLCD_F_TEMPS) khi `cydlcd.temps[]` CO muc cho id nay: day la du lieu day
  // du nhat, cap nhat lien tuc khi co link PUSH. Chi khi khong co (che do TRAM: khong co frame TEMPS)
  // moi lay tu 4 token nhiet do o cuoi dong "TBT ..." (xem tbt_protocol.h).
  // VI SAO PHAI UU TIEN PUSH (2026-10-15): nguon TRAM chi ~5 Hz va la duong DU PHONG; da gap that:
  // bo tach token cua dong TBT an nham ky tu '\n' o token CUOI -> target BAN IN luon 0, nen man hinh
  // hien "BED <nhiet> / 0 C" du frame PUSH da bao target 60 dung.
  for (uint8_t i = 0; i < cydlcd.n_temps; i++)
    if (cydlcd.temps[i].id == id) { *cur = cydlcd.temps[i].cur; *tgt = cydlcd.temps[i].tgt; return; }
  if (tbt.ever) {
    if (id == CYDLCD_TEMP_E0)  { *cur = tbt.st.t_e0;  *tgt = tbt.st.t_e0_tgt;  return; }
    if (id == CYDLCD_TEMP_BED) { *cur = tbt.st.t_bed; *tgt = tbt.st.t_bed_tgt; return; }
  }
}

// ===========================================================================
//  Helper tao widget
// ===========================================================================
// Chi set text khi khac noi dung cu -> khong nhap nhay, khong ve lai vo ich
void set_text(lv_obj_t *lbl, const char *txt) {
  if (!lbl || !txt) return;
  const char *old = lv_label_get_text(lbl);
  if (old && strcmp(old, txt) == 0) return;
  lv_label_set_text(lbl, txt);
}

// Gan font CO DAU tieng Viet cho DUNG 1 nhan (khong bao gio gan cho container/nut/page:
// font thay the co line_height 17 > Montserrat 16 nen gan tran ra ca cay se lam chu bi cat
// trong cac hang/nut cao co dinh -> nut "khong co text").
// Che do 4: font mono GFX tu sinh CO du dau tieng Viet (U+1EA0..U+1EF9) -> gan cho nhan co dau.
// Che do 0: KHONG con font nao co dau (da xoa het font sinh bang lv_font_conv) -> no-op; hau qua:
//           3 nhan co dau o lop phu SUA se ra O VUONG. Muon co dau thi dung che do 4.
void vn_font(lv_obj_t *label) {
#if UI_MONO_FONT == 4
  if (label) lv_obj_set_style_text_font(label, UI_MONO_FACE, 0);   // mono GFX: co dau tieng Viet
#else
  (void)label;                                    // che do 0: khong co font nao co dau
#endif
}

// Gan font co dau cho nhan NAM TRONG 1 nut (mk_btn tao dung 1 label con)
void vn_font_btn(lv_obj_t *btn) {
  if (btn) vn_font(lv_obj_get_child(btn, 0));
}

void style_flat(lv_obj_t *o, lv_color_t bg) {
  lv_obj_set_style_bg_color(o, bg, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

// Nhan 1 dong: rong theo noi dung (dat trong flex thi layout tu xep cho)
lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, lv_color_t col) {
  lv_obj_t *l = lv_label_create(parent);
  mono_obj(l);                                   // che do 2: mono cho TUNG nhan
  lv_obj_set_style_text_color(l, col, 0);
  lv_label_set_text(l, txt);
  return l;
}

// Nhan 1 dong, be rong CO DINH: chu dai hon thi tu cuon vong (khong bi cat cut)
lv_obj_t *mk_label_w(lv_obj_t *parent, const char *txt, lv_color_t col, lv_coord_t w) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_width(l, w);
  lv_label_set_long_mode(l, LV_LABEL_LONG_SCROLL_CIRCULAR);
  return l;
}

// Container trong suot de xep con bang FLEX.
//   flow = LV_FLEX_FLOW_COLUMN (xep doc) hoac LV_FLEX_FLOW_ROW (xep ngang)
//   pad  = padding trong; gap = khoang cach giua cac con (ca 2 chieu)
// Hang ROW can giua theo chieu doc (nut thap hon hang van nam giua).
lv_obj_t *mk_flex(lv_obj_t *parent, lv_flex_flow_t flow, lv_coord_t w, lv_coord_t h,
                         lv_coord_t pad, lv_coord_t gap) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, pad, 0);
  lv_obj_set_style_pad_row(o, gap, 0);
  lv_obj_set_style_pad_column(o, gap, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  // KHONG cho nhan cham: lv_obj mac dinh CLICKABLE, ma day chi la khung trong suot de xep con ->
  // de no "nuot" cham thi cham vao vung trong cua trang se khong roi xuong duoc TRANG (xem
  // cb_page_bg_click: cham vung trong -> ve man STATUS).
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(o, flow);
  // Hang ROW: can giua theo chieu doc (nut thap hon hang van nam giua hang).
  lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START,
                        (flow == LV_FLEX_FLOW_ROW) ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                        (flow == LV_FLEX_FLOW_ROW) ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START);
  return o;
}

// Vat dem trong suot: grow > 0 thi chiem het cho trong con lai cua hang flex
// (dung de day cac vat khac ve mot phia).
lv_obj_t *mk_spacer(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, uint8_t grow) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);   // vat dem trong suot: cham phai roi xuong trang
  if (grow) lv_obj_set_flex_grow(o, grow);
  return o;
}

void focus_mark(lv_obj_t *o, const bool on) {
  if (!o) return;
  // Vat co LV_OBJ_FLAG_USER_1 = vat TU QUAN LY NEN (xem jg_axis[] o man JOG): nen cua no dang
  // dung de danh dau "DANG CHON" (COL_ACCENT). Focus cua nhung vat nay CHI duoc ve VIEN.
  // LY DO (loi nguoi dung bao): focus va "dang chon" la 2 trang thai KHAC NHAU, KHONG duoc ghi
  // de nhau. Truoc day focus_mark() doi CA NEN (COL_ACCENT <-> COL_BTN); khi focus roi khoi nut
  // truc dang chon (sang nut buoc / nut di chuyen) no tra nen nut truc ve COL_BTN -> mat dau
  // "truc dang chon" du s_axis van dung. Nay: "chon" = NEN, "focus" = VIEN.
  const bool own_bg = lv_obj_has_flag(o, LV_OBJ_FLAG_USER_1);
  lv_obj_set_style_border_width(o, on ? 2 : 0, 0);
  // Nut truc khi focus co the dang to nen COL_ACCENT -> vien COL_ACCENT se chim vao nen,
  // nen dung mau sang (MR_FRAME) de van nhin thay focus.
  lv_obj_set_style_border_color(o, own_bg ? MR_FRAME : COL_ACCENT, 0);
  // Vat co nen dac (nut, dong menu, bang) -> doi sang COLOR_SELECTION_BG nhu dong dang chon cua Marlin
  if (lv_obj_get_style_bg_opa(o, 0) == LV_OPA_COVER && !own_bg)
    lv_obj_set_style_bg_color(o, on ? MR_SELBG : COL_BTN, 0);
}

// Ve khung cho vat dang focus (encoder hoac cham) -> nguoi dung biet dang o dau
void on_focus_changed(lv_group_t *g) {
  lv_obj_t *f = lv_group_get_focused(g);
  if (s_focus_prev && s_focus_prev != f) focus_mark(s_focus_prev, false);
  if (f) focus_mark(f, true);
  s_focus_prev = f;
}

// Nut bam: co dinh kich thuoc, xoa SCROLLABLE (de encoder bam 1 lan la chay),
// tu dong vao group va tu canh chu vao giua.
lv_obj_t *mk_btn(lv_obj_t *parent, const char *txt, lv_coord_t w, lv_coord_t h,
                        lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_set_style_bg_color(b, COL_BTN, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t *l = lv_label_create(b);
  mono_obj(l);                                   // che do 2: mono cho nhan trong nut
  lv_obj_set_style_text_color(l, COL_TXT, 0);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  if (s_group) lv_group_add_obj(s_group, b);
  return b;
}

// Nut chia deu be rong trong 1 hang flex (khong can biet truoc be rong tung nut)
lv_obj_t *mk_btn_grow(lv_obj_t *parent, const char *txt, lv_coord_t h,
                             lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = mk_btn(parent, txt, 0, h, cb, ud);
  lv_obj_set_flex_grow(b, 1);
  return b;
}

// ---------------------------------------------------------------------------
//  NUT TRONG HOP THOAI (thong nhat moi dialog - xem ghi chu o ui_lvgl_int.h)
// ---------------------------------------------------------------------------
// Hang nut: flex ROW can GIUA theo CA 2 CHIEU. Nut be rong co dinh (mk_dlg_btn) nen hang nay KHONG
// dan het be rong hop -> trong hop thua ra 2 ben, dung y "cac nut can vao giua dialog".
lv_obj_t *mk_dlg_row(lv_obj_t *parent, lv_coord_t w, lv_coord_t h) {
  lv_obj_t *r = mk_flex(parent, LV_FLEX_FLOW_ROW, w, h, 0, DLG_BTN_GAP);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(r, DLG_BTN_GAP, 0);
  return r;
}

// 1 nut trong hop thoai: be rong CO DINH + mau nen chi dinh. Vat TU QUAN LY NEN (LV_OBJ_FLAG_USER_1)
// de focus_mark() chi ve VIEN khi focus (khong ghi de mau xanh/do - giong nut buoc cua lop phu SUA).
lv_obj_t *mk_dlg_btn(lv_obj_t *row, const char *txt, lv_coord_t w, lv_coord_t h,
                            lv_color_t bg, lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = mk_btn(row, txt, w, h, cb, ud);
  lv_obj_set_style_bg_color(b, bg, 0);
  lv_obj_set_style_radius(b, 4, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_USER_1);
  // Chu trang tuyet doi tren nen xanh/do (COL_TXT la trang nga -> tuong phan thap hon tren mau dam).
  lv_obj_t *l = lv_obj_get_child(b, 0);
  if (l) lv_obj_set_style_text_color(l, DLG_TXT, 0);
  return b;
}

// Ban phim lv_keyboard: MAC DINH khong co nen (trong suot) nen chu trang tren nen sang cua trang rat
// kho doc (yeu cau nguoi dung 2026-10-15: "ban phim touch chua co mau nen nen khong nhin ro chu").
// Dat: nen dam (COL_BAR) + vien sang (COL_ACCENT) cho CA ban phim; tung PHIM co nen COL_BTN + chu
// trang; phim dang bam (PRESSED) doi sang COL_ACCENT. O nhap (ta) cung dat nen toi + chu trang.
void style_keyboard(lv_obj_t *kb, lv_obj_t *ta) {
  if (kb) {
    lv_obj_set_style_bg_color(kb, COL_BAR, 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(kb, COL_ACCENT, 0);
    lv_obj_set_style_border_width(kb, 1, 0);
    lv_obj_set_style_radius(kb, 4, 0);
    lv_obj_set_style_pad_all(kb, 3, 0);
    // Tung PHIM: nen xam dam + chu trang; giu mau nen nay ca khi focus (khong doi theo theme).
    lv_obj_set_style_bg_color(kb, COL_BTN, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, DLG_TXT, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, COL_DIM, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 3, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, COL_ACCENT, (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_PRESSED));
    lv_obj_set_style_bg_color(kb, COL_ACCENT, (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_FOCUSED));
  }
  if (ta) {
    lv_obj_set_style_bg_color(ta, lv_color_hex(0x0B0B2B), 0);   // toi hon nen trang: chu trang noi ro
    lv_obj_set_style_bg_opa(ta, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(ta, COL_ACCENT, 0);
    lv_obj_set_style_border_width(ta, 1, 0);
    lv_obj_set_style_text_color(ta, DLG_TXT, 0);
    lv_obj_set_style_radius(ta, 3, 0);
    lv_obj_set_style_pad_all(ta, 4, 0);
  }
  // Doc lai style THAT SU dat duoc (chan doan tu COM8) - `lv_color_to32()` doi RGB565 -> RGB888 de doc
  // bang mat thuong: vd "[UI] ban phim: nen=#160C52 opa=255 | phim nen=#2A2080 chu=#FFFFFF".
  if (kb)
    WIFILOG("[UI] ban phim: nen=#%06lX opa=%u | phim nen=#%06lX chu=#%06lX\n",
            (unsigned long)lv_color_to32(lv_obj_get_style_bg_color(kb, 0)),
            (unsigned)lv_obj_get_style_bg_opa(kb, 0),
            (unsigned long)lv_color_to32(lv_obj_get_style_bg_color(kb, LV_PART_ITEMS)),
            (unsigned long)lv_color_to32(lv_obj_get_style_text_color(kb, LV_PART_ITEMS)));
}

// Vung BAM trong suot co toa do tuyet doi (man STATUS/MENU khong dung flex).
// cb == NULL: khong vao group (dung cho "bang icon" nam trong 1 vung bam khac).
lv_obj_t *mk_hit(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                        lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  if (cb) {
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    if (s_group) lv_group_add_obj(s_group, b);
  }
  else lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);     // de click roi xuong vung bam cha
  return b;
}

// Icon cua Marlin (LVGL ALPHA_4BIT): mau ve bang recolor -> doi theo trang thai
lv_obj_t *mk_icon(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const lv_img_dsc_t *src,
                         lv_color_t col) {
  lv_obj_t *im = lv_img_create(parent);
  lv_img_set_src(im, src);
  lv_obj_set_pos(im, x, y);
  lv_obj_clear_flag(im, LV_OBJ_FLAG_CLICKABLE);       // click roi xuong vung bam cha
  lv_obj_clear_flag(im, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_img_recolor(im, col, 0);
  lv_obj_set_style_img_recolor_opa(im, LV_OPA_COVER, 0);
  return im;
}

// Doi mau icon (Marlin doi mau bitmap theo nhiet do / trang thai)
// CHI ghi khi mau KHAC: `lv_obj_set_style_img_recolor()` luon lam vat "ban" (invalidate) du gia tri khong
// doi. Man STATUS cap nhat 150 ms/lan va goi ham nay cho 3 icon -> truoc day cu 150 ms lai ve lai 3 icon
// (do that 2026-10-15: 7 khung ve ~70 ms MOI GIAY chi vi mau khong doi -> man hinh "chay cham").
void icon_color(lv_obj_t *im, const lv_color_t col) {
  if (!im) return;
  if (lv_obj_get_style_img_recolor(im, 0).full == col.full) return;
  lv_obj_set_style_img_recolor(im, col, 0);
  lv_obj_set_style_img_recolor_opa(im, LV_OPA_COVER, 0);
}

// ---- 3 helper "chi ghi khi DOI" (dong bo voi set_text() o tren) ----
// VI SAO CAN: LVGL KHONG tu bo qua khi gia tri style giong gia tri cu - `lv_obj_set_style_*` /
// `lv_img_set_src` deu goi `lv_obj_invalidate()` (xem lv_img.c:74). Man STATUS/ MENU cap nhat 150 ms/lan
// ma goi chung vo dieu kien => moi 150 ms lai ve lai mot vung lon (do: 7 khung ~70 ms moi giay, SPI ~3,3
// man hinh/giay) => ton CPU + SPI vo ich va man hinh cham.
void set_text_color(lv_obj_t *o, const lv_color_t col) {
  if (!o) return;
  if (lv_obj_get_style_text_color(o, 0).full == col.full) return;
  lv_obj_set_style_text_color(o, col, 0);
}

void set_border_color(lv_obj_t *o, const lv_color_t col) {
  if (!o) return;
  if (lv_obj_get_style_border_color(o, 0).full == col.full) return;
  lv_obj_set_style_border_color(o, col, 0);
}

void set_bg_color(lv_obj_t *o, const lv_color_t col) {
  if (!o) return;
  if (lv_obj_get_style_bg_color(o, 0).full == col.full) return;
  lv_obj_set_style_bg_color(o, col, 0);
}

// Doi anh cua icon: `lv_img_set_src()` invalidate VO DIEU KIEN (lv_img.c:74) ke ca khi cung 1 bitmap.
void set_img_src(lv_obj_t *im, const void *src) {
  if (!im || !src) return;
  if (lv_img_get_src(im) == src) return;
  lv_img_set_src(im, src);
}


// Nhan 1 dong, can phai trong be rong w (Marlin can so theo toa do x phai)
lv_obj_t *mk_label_right(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                                const char *txt, lv_color_t col) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
  return l;
}

// Nhan 1 dong can giua trong be rong w
lv_obj_t *mk_label_mid(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                              const char *txt, lv_color_t col) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

// Man kin 320x240, KHONG flex (con dat bang toa do tuyet doi) + khong padding.
// Nen dung COLOR_BACKGROUND cua Marlin (2 man nay theo bo cuc Marlin).
lv_obj_t *mk_page_full(void) {
  lv_obj_t *p = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(p, 0, 0);
  lv_obj_set_size(p, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  lv_obj_set_style_bg_color(p, MR_BG, 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_radius(p, 0, 0);
  lv_obj_set_style_pad_all(p, 0, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  return p;
}

// Focus vao vat bam duoc dau tien trong 1 nhanh (de encoder bat dau o vat dau man moi)
bool focus_first_in(lv_obj_t *o) {
  const uint32_t n = lv_obj_get_child_cnt(o);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_CLICK_FOCUSABLE) && !lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) {
      lv_group_focus_obj(c);
      return true;
    }
    if (focus_first_in(c)) return true;
  }
  return false;
}

// Cham vao VUNG TRONG cua 1 trang CON -> ve thang man STATUS (yeu cau nguoi dung).
// Co che LVGL: vat KHONG CLICKABLE (nhan - lv_label tu clear co; khung trong suot - da clear trong
// mk_flex/mk_spacer; mk_icon/mk_hit(cb=NULL)) bi BO QUA khi tim vat nhan cham -> su kien roi xuong
// vat CHA, tuc la roi vao handler nay. Vat CO CLICKABLE (nut, roller, bang, mk_hit co cb) duoc
// tim thay TRUOC nen van hoat dong binh thuong. Su kien CLICKED KHONG tu bubble len cha
// (lv_event.c: event_is_bubbled chi cho CHILD_CREATED/DELETED hoac khi bat LV_OBJ_FLAG_EVENT_BUBBLE)
// -> bam nut KHONG lam chay handler nay, khong bi "tuot" khoi trang.
// DANH DOI (da bao truoc): cham HUT ra vung trong la THOAT khoi trang dang mo. Muon quay lai nhu cu:
// bo DUY NHAT 1 dong `lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);` trong mk_page() duoi day.
static void cb_page_bg_click(lv_event_t *e) {
  (void)e;
  show_page(PG_STATUS);
}

// Container noi dung cua 1 man: flex COLUMN, padding PAGE_PAD, cach nhau GAP.
// Con nao muon an het cho trong con lai thi them lv_obj_set_flex_grow(con, 1).
// CHI dung cho cac trang CON; man STATUS dung mk_page_full() nen khong dinh "cham vung trong".
lv_obj_t *mk_page(lv_obj_t *parent) {
  lv_obj_t *p = lv_obj_create(parent);
  lv_obj_set_pos(p, 0, CONT_Y);
  lv_obj_set_size(p, UI_LCD_HOR_RES, CONT_H);
  lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_radius(p, 0, 0);
  lv_obj_set_style_pad_all(p, PAGE_PAD, 0);
  lv_obj_set_style_pad_row(p, GAP, 0);
  lv_obj_set_style_pad_column(p, GAP, 0);
  // Man con duoc phep CUON DOC: noi dung dai hon vung nhin thi thanh day (xem bot_sync)
  // hien ra de phan trang bang PREV/NEXT; khong ve thanh cuon cho do roi man hinh.
  lv_obj_add_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(p, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  // CHAM VAO VUNG TRONG -> ve thang man STATUS (xem cb_page_bg_click ngay tren).
  // Muon tat hanh vi nay: bo DUY NHAT dong add_flag duoi day (handler cu the giu lai, vo hai).
  lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(p, cb_page_bg_click, LV_EVENT_CLICKED, NULL);
  return p;
}

#endif  // CYDLCD_USE_LVGL
