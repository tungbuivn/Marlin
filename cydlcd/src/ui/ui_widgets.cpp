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

// 2026-10-17 (yeu cau nguoi dung: "dialog ... mesh height edit van chua co mau nen title"):
// Cac hop thoai TU DUNG (mesh / preheat / settings) dung `mk_label_w` lam tieu de -> chi co chu,
// khong co nen. Ham nay bien label do thanh 1 THANH TIEU DE: nen COL_BTN dac + bo goc + padding
// (cung kieu voi tieu de cua lv_msgbox trong box_make()).
// 2026-10-17 (yeu cau nguoi dung: "title khong padding the la content lai dinh vao duong bien roi,
// them container padding 3 phia di"): tach THAN hop thoai ra 1 container rieng, chua padding
// trai/phai/duoi; thanh tieu de van la con truc tiep cua hop nen TRAN het be ngang, cham vien.
lv_obj_t *dlg_body(lv_obj_t *box, lv_coord_t pad, lv_coord_t gap) {
  if (!box) return NULL;
  lv_obj_t *b = lv_obj_create(box);
  lv_obj_set_width(b, LV_PCT(100));
  lv_obj_set_height(b, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_pad_left(b, pad, 0);
  lv_obj_set_style_pad_right(b, pad, 0);
  lv_obj_set_style_pad_bottom(b, pad, 0);
  lv_obj_set_style_pad_top(b, 4, 0);
  lv_obj_set_style_pad_row(b, gap, 0);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(b, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  return b;
}

void dlg_title_bar(lv_obj_t *label) {
  if (!label) return;
  // 2026-10-17 (nguoi dung: "dialog edit mesh node dang dung mau chu title khac voi cac dialog khac
  // (mau vang)"): truoc day moi hop truyen mau rieng (mesh/settings = COL_ACCENT cyan, preheat =
  // MR_MENU vang, msgbox = title_col). Nay THONG NHAT: chu tieu de luon MAU VANG MR_MENU.
  lv_obj_set_style_text_color(label, MR_MENU, 0);
  lv_obj_set_style_bg_color(label, COL_BTN, 0);
  lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(label, 4, 0);
  lv_obj_set_style_pad_all(label, 6, 0);
  lv_obj_set_style_pad_left(label, 10, 0);
  lv_obj_set_style_pad_right(label, 10, 0);
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

// Do sang cam nhan (xap xi do sang tuong doi cua WCAG, KHONG gamma — du chinh xac de PHAN LOAI).
// ⚠️ `bg.ch.red/green/blue` la gia tri RGB565 (5/6/5 BIT), KHONG phai 8 bit: voi LV_COLOR_DEPTH 16
// thi `lv_color_hex()` nhet THANG RGB565 vao `r.full` (xem lv_color_hex trong lvgl/src/misc/lv_color.h).
// Phai mo rong ve 8 bit TRUOC khi nhan trong so, neu khong MOI mau deu ra "toi" (da mac loi nay that).
// Cung KHONG dung lv_color_brightness() cua LVGL: cong thuc (3R+G+B)>>3 cho cyan #00E5FF chi 60/255
// vi R = 0 -> xep cyan vao nhom "nen toi".
// Nguong 40% tach dung theo bang mau cua UI:
//     nen TOI : COL_BTN 16% · MR_BG 11% · COL_BAR 7%
//     nen SANG: COL_ERR 64% · COL_OK 71% · COL_ACCENT 72% · COL_WARN 83%
static bool bg_is_light(const lv_color_t bg) {
  const uint32_t r = (uint32_t)((bg.ch.red   << 3) | (bg.ch.red   >> 2));
  const uint32_t g = (uint32_t)((bg.ch.green << 2) | (bg.ch.green >> 4));
  const uint32_t b = (uint32_t)((bg.ch.blue  << 3) | (bg.ch.blue  >> 2));
  return (r * 2126u + g * 7152u + b * 722u) > 1020000u;   // 0.40 * 255 * 10000
}

// Ve/bo vien "outlined" cho NUT (vat co LV_OBJ_FLAG_USER_2) theo mau NEN HIEN TAI cua no.
//   nen TOI  -> CAN vien: khong thi nut lan vao nen man hinh (xem ghi chu COL_BTN_OUTLINE).
//   nen SANG -> BO vien: nut da noi san (>= 7:1); vien sang tren nen sang chi lam roi mat.
//   PHANG (USER_3) hoac nen TRONG SUOT -> BO vien: icon/ten nam truc tiep tren thanh tieu de.
// PHAI goi lai moi khi NEN doi (mk_btn_set_bg, focus_mark) vi vien phu thuoc mau nen.
static void outline_apply(lv_obj_t *o) {
  if (!o || !lv_obj_has_flag(o, LV_OBJ_FLAG_USER_2)) return;
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_USER_3) ||
      lv_obj_get_style_bg_opa(o, 0) == LV_OPA_TRANSP) {
    lv_obj_set_style_border_width(o, 0, 0);
    return;
  }
  const bool dark = !bg_is_light(lv_obj_get_style_bg_color(o, 0));
  lv_obj_set_style_border_width(o, dark ? 1 : 0, 0);
  if (dark) lv_obj_set_style_border_color(o, COL_BTN_OUTLINE, 0);
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
  // Vat PHANG (LV_OBJ_FLAG_USER_3: TEN MAN va ICON trai cung cua thanh tieu de): khong bao gio ve
  // vien, khong doi nen (yeu cau nguoi dung 2026-10-16). Bao focus bang MAU CHU — khong dung border
  // hay mau nen, nhung van biet dang o dau khi dung encoder.
  const bool flat = lv_obj_has_flag(o, LV_OBJ_FLAG_USER_3);
  // Vat co nen dac (nut, dong menu, bang) -> doi sang COLOR_SELECTION_BG nhu dong dang chon cua Marlin.
  // PHAI lam TRUOC khi ve vien: vien "outlined" duoi day phu thuoc mau nen (xem outline_apply).
  if (lv_obj_get_style_bg_opa(o, 0) == LV_OPA_COVER && !own_bg && !flat)
    lv_obj_set_style_bg_color(o, on ? MR_SELBG : COL_BTN, 0);
  // Vat co LV_OBJ_FLAG_USER_4: KHONG BAO GIO ve vien focus va KHONG doi mau chu.
  // YEU CAU NGUOI DUNG 2026-10-16: "bo duong vien cua 3 nut hotend, bed va fan khi click o trang STATUS".
  // VI SAO KHONG DUNG USER_3 (vat phang): USER_3 con doi MAU CHU de bao focus, ma icon cua 3 o nay la
  // GLYPH FONT (khong phai anh) nen doi mau chu = doi mau icon -> se de mat mau NHIET DO (mau icon o day
  // do temp_ramp() quyet dinh). Nen phai co mot co RIENG: chi bo vien, khong dung gi khac.
  // (Nen khi NHAN khong lam o day: do la style cho LV_STATE_PRESSED dat ngay tai status_cell().)
  if (lv_obj_has_flag(o, LV_OBJ_FLAG_USER_4)) {
    lv_obj_set_style_border_width(o, 0, 0);
    return;
  }
  if (flat) {
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_t *l = lv_obj_get_child(o, 0);      // nut -> doi mau CHU CON; nhan -> doi mau chinh no
    lv_obj_set_style_text_color(l ? l : o, on ? COL_ACCENT : COL_TXT, 0);
  }
  else if (on) {
    lv_obj_set_style_border_width(o, 2, 0);
    // Nut truc khi focus co the dang to nen COL_ACCENT -> vien COL_ACCENT se chim vao nen,
    // nen dung mau sang (MR_FRAME) de van nhin thay focus.
    lv_obj_set_style_border_color(o, own_bg ? MR_FRAME : COL_ACCENT, 0);
  }
  else if (lv_obj_has_flag(o, LV_OBJ_FLAG_USER_2)) {
    // NUT co vien "outlined": tra ve vien do. KHONG duoc dat width 0 — lam vay se xoa duong bao
    // va nut lai lan vao nen man hinh (dung cai loi vua sua).
    outline_apply(o);
  }
  else {
    lv_obj_set_style_border_width(o, 0, 0);     // vat khac: bo vien focus
  }
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
  // Danh dau "nut co vien outlined" de focus_mark() biet duong TRA LAI vien khi focus roi di
  // (khong duoc dat border_width = 0, lam vay se xoa duong bao — xem outline_apply).
  lv_obj_add_flag(b, LV_OBJ_FLAG_USER_2);
  lv_obj_set_style_bg_color(b, COL_BTN, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 0, 0);        // outline_apply() dat lai theo mau nen ben duoi
  // Bo goc 5 px (truoc la 3, roi 8): 8 px bi nguoi dung thay la qua tron (2026-10-16), 3 px thi
  // qua vuong. 5 px con ro la nut Material 3 ma khong an vao vung chu o cac o thap (20 px).
  // `mk_hit()` giu 3 vi la vung bam TRONG SUOT (khong thay).
  lv_obj_set_style_radius(b, 5, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t *l = lv_label_create(b);
  mono_obj(l);                                   // che do 2: mono cho nhan trong nut
  lv_obj_set_style_text_color(l, COL_TXT, 0);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  if (s_group) lv_group_add_obj(s_group, b);
  outline_apply(b);                              // nen COL_BTN (toi) -> ve vien outlined
  return b;
}

// Nut chia deu be rong trong 1 hang flex (khong can biet truoc be rong tung nut)
lv_obj_t *mk_btn_grow(lv_obj_t *parent, const char *txt, lv_coord_t h,
                             lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = mk_btn(parent, txt, 0, h, cb, ud);
  lv_obj_set_flex_grow(b, 1);
  return b;
}

// Mau CHU tuong phan voi mau NEN (xem ghi chu o ui_lvgl_int.h).
// Do sang cam nhan xap xi do sang tuong doi cua WCAG (KHONG gamma — du chinh xac de PHAN LOAI):
//     y = 0.2126*R + 0.7152*G + 0.0722*B      (nhan 10000 de dung so nguyen)
// KHONG dung lv_color_brightness(): cong thuc cua LVGL la (3R+G+B)>>3, cho cyan #00E5FF chi 60/255
// (vi R = 0) -> se xep cyan vao nhom "nen toi" va chon chu SANG, dung lai loi cu.
// Nguong 40% tach dung theo bang mau hien co cua UI:
//     nen TOI : COL_BTN 16% · MR_BG 11% · COL_BAR 7%
//     nen SANG: COL_ERR 64% · COL_OK 71% · COL_ACCENT 72% · COL_WARN 83%
lv_color_t mk_color_on(const lv_color_t bg) {
  return bg_is_light(bg) ? COL_ON_ACCENT : COL_TXT;
}

// Doi NEN cua nut VA chon mau CHU tuong phan theo chinh mau nen do. Xem ghi chu o ui_lvgl_int.h.
void mk_btn_set_bg(lv_obj_t *btn, const lv_color_t bg) {
  if (!btn) return;
  const lv_color_t fg = mk_color_on(bg);
  // Chi set khi THAT SU doi: lv_obj_set_style_* luon invalidate vat -> goi vo dieu kien moi chu ky
  // cap nhat se lam man nhap nhay va ton bang thong SPI (xem jog_axis_mark / ov_marks).
  if (lv_obj_get_style_bg_color(btn, 0).full != bg.full)
    lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_t *l = lv_obj_get_child(btn, 0);
  if (l && lv_obj_get_style_text_color(l, 0).full != fg.full)
    lv_obj_set_style_text_color(l, fg, 0);
  outline_apply(btn);       // vien phu thuoc mau nen -> phai cap nhat lai (xem outline_apply)
}

void mk_btn_set_sel(lv_obj_t *btn, const bool sel) {
  mk_btn_set_bg(btn, sel ? COL_ACCENT : COL_BTN);
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
  // mk_btn() da ve vien theo nen COL_BTN; nay nen doi sang xanh/do dam nen phai ve lai vien cho khop.
  outline_apply(b);
  return b;
}

// Ban phim lv_keyboard: MAC DINH khong co nen (trong suot) nen chu trang tren nen sang cua trang rat
// kho doc (yeu cau nguoi dung 2026-10-15: "ban phim touch chua co mau nen nen khong nhin ro chu").
// Dat: nen dam (COL_BAR) + vien sang (COL_ACCENT) cho CA ban phim; tung PHIM co nen COL_BTN + chu
// trang; phim dang bam (PRESSED) doi sang COL_ACCENT. O nhap (ta) cung dat nen toi + chu trang.
// ===========================================================================
//  BANG PHIM THAY THE: doi 6 nhan `LV_SYMBOL_*` thanh CHU ASCII
//
//  YEU CAU NGUOI DUNG 2026-10-16: "khi hien thi ban phim co mot so text khong hien thi".
//  NGUYEN NHAN: ban phim cua LVGL dan nhan cac phim dac biet bang `LV_SYMBOL_*` = U+F00C (OK),
//  U+F00D (CLOSE), U+F053 (LEFT), U+F054 (RIGHT), U+F11C (KEYBOARD), U+F55A (BACKSPACE),
//  U+F8A2 (NEW_LINE). Font cua project CHI co 5 dai (ascii U+0020..U+00FF + vn1..vn4) nen 6 ma do
//  KHONG co glyph -> LVGL ve ra O TRONG (dung 6 phim do bi "mat chu", cac phim chu/so van binh thuong).
//
//  CACH SUA: KHONG the sinh lai font (may build khong co file Segoe UI .ttf), va ban LVGL nay khong co
//  `lv_btnmatrix_set_btn_text()` de doi nhan tung phim. Cach dung duoc: `lv_keyboard_set_map()` voi bang
//  cua project - chep Y NGUYEN bang mac dinh cua LVGL (lv_keyboard.c) va chi thay 6 cho:
//      LV_SYMBOL_BACKSPACE -> "DEL"    LV_SYMBOL_NEW_LINE -> "ENT"
//      LV_SYMBOL_KEYBOARD  -> "123" (o bang chu) / "abc" (o bang so)
//      LV_SYMBOL_LEFT      -> "<"      LV_SYMBOL_RIGHT    -> ">"
//      LV_SYMBOL_OK        -> "OK"
//  Dat cho CA 4 CHE DO (thuong/thuong-hoa/dac biet/so) nen doi che do khong lam mat nhan da sua.
//  Bang ctrl chep nguyen (no danh theo VI TRI o, khong lien quan chu tren phim).
// ===========================================================================
// ⚠️ Phai boc `#if LV_USE_KEYBOARD`: env STM32 (`cydlcd_f407vgz_lvgl`) KHONG bat ban phim (xem
// src/gfx/lv_conf.h) nen `LV_KEYBOARD_CTRL_BTN_FLAGS` / `lv_keyboard_mode_t` khong ton tai -> build loi.
#if LV_USE_KEYBOARD
#define KB_BTN(w)  (LV_BTNMATRIX_CTRL_POPOVER | (w))          // = LV_KB_BTN() (macro noi bo cua LVGL)

// 2026-10-17 (nguoi dung: "truoc day ban phim co nut an ban phim, bo roi a?"):
// LVGL danh dau phim AN ban phim bang `LV_SYMBOL_KEYBOARD` (U+F11C) - xem lv_keyboard.c:303.
// Nhung font cua project KHONG co glyph U+F11C (chi ASCII + vn1..vn4), nen khi thay 6 nhan
// LV_SYMBOL_* bang chu ASCII (2026-10-16) phim nay bi thay bang "123" -> mot phim KHONG LAM GI
// (vi "123" khong phai chuoi chuyen che do ma LVGL hieu: chi "abc"/"ABC"/"1#").
// Nay dung nhan "ESC" va tu xu ly: LVGL se chen "ESC" vao o nhap truoc, ta xoa dung 3 ky tu do
// roi gui LV_EVENT_CANCEL (chinh la viec phim goc lam).
#define KB_HIDE_TXT "ESC"

static const char *const kb_map_lc[] = {
  "1#", "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "DEL", "\n",
  "ABC", "a", "s", "d", "f", "g", "h", "j", "k", "l", "ENT", "\n",
  "_", "-", "z", "x", "c", "v", "b", "n", "m", ".", ",", ":", "\n",
  KB_HIDE_TXT, "<", " ", ">", "OK", ""
};
static const char *const kb_map_uc[] = {
  "1#", "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "DEL", "\n",
  "abc", "A", "S", "D", "F", "G", "H", "J", "K", "L", "ENT", "\n",
  "_", "-", "Z", "X", "C", "V", "B", "N", "M", ".", ",", ":", "\n",
  KB_HIDE_TXT, "<", " ", ">", "OK", ""
};
static const char *const kb_map_spec[] = {
  "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "DEL", "\n",
  "abc", "+", "&", "/", "*", "=", "%", "!", "?", "#", "<", ">", "\n",
  "\\", "@", "$", "(", ")", "{", "}", "[", "]", ";", "\"", "'", "\n",
  KB_HIDE_TXT, "<", " ", ">", "OK", ""
};
static const char *const kb_map_num[] = {
  "1", "2", "3", "abc", "\n",
  "4", "5", "6", "OK", "\n",
  "7", "8", "9", "DEL", "\n",
  KB_HIDE_TXT, "0", ".", "<", ">", ""
};
static const lv_btnmatrix_ctrl_t kb_ctrl_lc[] = {
  LV_KEYBOARD_CTRL_BTN_FLAGS | 5, KB_BTN(4), KB_BTN(4), KB_BTN(4), KB_BTN(4), KB_BTN(4), KB_BTN(4),
  KB_BTN(4), KB_BTN(4), KB_BTN(4), KB_BTN(4), LV_BTNMATRIX_CTRL_CHECKED | 7,
  LV_KEYBOARD_CTRL_BTN_FLAGS | 6, KB_BTN(3), KB_BTN(3), KB_BTN(3), KB_BTN(3), KB_BTN(3), KB_BTN(3),
  KB_BTN(3), KB_BTN(3), KB_BTN(3), LV_BTNMATRIX_CTRL_CHECKED | 7,
  LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1), LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1), KB_BTN(1), KB_BTN(1),
  KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1),
  LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1), LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1),
  LV_KEYBOARD_CTRL_BTN_FLAGS | 2, LV_BTNMATRIX_CTRL_CHECKED | 2, 6,
  LV_BTNMATRIX_CTRL_CHECKED | 2, LV_KEYBOARD_CTRL_BTN_FLAGS | 2
};
static const lv_btnmatrix_ctrl_t kb_ctrl_uc[] = {
  LV_KEYBOARD_CTRL_BTN_FLAGS | 5, KB_BTN(4), KB_BTN(4), KB_BTN(4), KB_BTN(4), KB_BTN(4), KB_BTN(4),
  KB_BTN(4), KB_BTN(4), KB_BTN(4), KB_BTN(4), LV_BTNMATRIX_CTRL_CHECKED | 7,
  LV_KEYBOARD_CTRL_BTN_FLAGS | 6, KB_BTN(3), KB_BTN(3), KB_BTN(3), KB_BTN(3), KB_BTN(3), KB_BTN(3),
  KB_BTN(3), KB_BTN(3), KB_BTN(3), LV_BTNMATRIX_CTRL_CHECKED | 7,
  LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1), LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1), KB_BTN(1), KB_BTN(1),
  KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1),
  LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1), LV_BTNMATRIX_CTRL_CHECKED | KB_BTN(1),
  LV_KEYBOARD_CTRL_BTN_FLAGS | 2, LV_BTNMATRIX_CTRL_CHECKED | 2, 6,
  LV_BTNMATRIX_CTRL_CHECKED | 2, LV_KEYBOARD_CTRL_BTN_FLAGS | 2
};
static const lv_btnmatrix_ctrl_t kb_ctrl_spec[] = {
  KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1),
  KB_BTN(1), LV_BTNMATRIX_CTRL_CHECKED | 2,
  LV_KEYBOARD_CTRL_BTN_FLAGS | 2, KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1),
  KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1),
  KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1), KB_BTN(1),
  KB_BTN(1), KB_BTN(1), KB_BTN(1),
  LV_KEYBOARD_CTRL_BTN_FLAGS | 2, LV_BTNMATRIX_CTRL_CHECKED | 2, 6,
  LV_BTNMATRIX_CTRL_CHECKED | 2, LV_KEYBOARD_CTRL_BTN_FLAGS | 2
};
static const lv_btnmatrix_ctrl_t kb_ctrl_num[] = {
  1, 1, 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 2,
  1, 1, 1, LV_KEYBOARD_CTRL_BTN_FLAGS | 2,
  1, 1, 1, 2,
  1, 1, 1, 1, 1
};

/// Dat bang phim cua project (nhan ASCII) cho CA 4 che do. Goi trong style_keyboard().
static void kb_apply_maps(lv_obj_t *kb) {
  if (!kb) return;
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_LOWER,   (const char **)kb_map_lc,   kb_ctrl_lc);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_TEXT_UPPER,   (const char **)kb_map_uc,   kb_ctrl_uc);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_SPECIAL, (const char **)kb_map_spec, kb_ctrl_spec);
  lv_keyboard_set_map(kb, LV_KEYBOARD_MODE_NUMBER,       (const char **)kb_map_num,  kb_ctrl_num);
}
#endif  // LV_USE_KEYBOARD

#if LV_USE_KEYBOARD
// Phim AN BAN PHIM (nhan "ESC"): LVGL da chen "ESC" vao o nhap -> xoa di roi gui CANCEL.
static void kb_hide_key_cb(lv_event_t *e) {
  lv_obj_t *kb = lv_event_get_target(e);
  if (!kb) return;
  const char *txt = lv_btnmatrix_get_btn_text(kb, lv_btnmatrix_get_selected_btn(kb));
  if (!txt || strcmp(txt, KB_HIDE_TXT) != 0) return;
  lv_obj_t *ta = lv_keyboard_get_textarea(kb);
  if (ta) {
    // LVGL da chen "ESC" ngay TRUOC con tro. Chi xoa khi 3 ky tu ngay truoc con tro DUNG LA "ESC"
    // -> khong bao gio xoa nham chu that (truong hop o nhap da day, LVGL khong chen duoc gi).
    const size_t n = strlen(KB_HIDE_TXT);
    const uint32_t pos = lv_textarea_get_cursor_pos(ta);
    const char *cur = lv_textarea_get_text(ta);
    if (cur && pos >= n && strncmp(cur + pos - n, KB_HIDE_TXT, n) == 0)
      for (size_t i = 0; i < n; i++) lv_textarea_del_char(ta);
  }
  lv_event_send(kb, LV_EVENT_CANCEL, NULL);      // -> cb_pre_kb / cb_wifi_kb -> ham hide
}
#endif

void style_keyboard(lv_obj_t *kb, lv_obj_t *ta) {
  if (kb) {
#if LV_USE_KEYBOARD
    kb_apply_maps(kb);        // nhan ASCII cho 6 phim dac biet (xem khoi bang phim o tren)
    lv_obj_add_event_cb(kb, kb_hide_key_cb, LV_EVENT_VALUE_CHANGED, NULL);   // phim ESC
#endif
    // 2026-10-16 (yeu cau nguoi dung "thong nhat chi dung 1 loai font hien thi chu"): ban phim va o
    // nhap la widget LVGL tao truc tiep (khong qua mk_label/mk_btn) nen truoc day dung font MAC DINH
    // cua LVGL (Montserrat 14) -> man WIFI / NHIET DO bi lech ho font. Gan font chung o day.
    // Ban phim TU chia hang theo chieu cao vat (KB_H) nen doi font khong lam tran.
    mono_obj(kb);
    lv_obj_set_style_bg_color(kb, COL_BAR, 0);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(kb, COL_ACCENT, 0);
    lv_obj_set_style_border_width(kb, 1, 0);
    lv_obj_set_style_radius(kb, 4, 0);
    // Padding NHO (2 px) de cac phim FILL HET be ngang man hinh (yeu cau nguoi dung 2026-10-15:
    // "ban phim chua fill het margin trai phai"); pad_row/pad_column = khe giua cac phim.
    lv_obj_set_style_pad_all(kb, 2, 0);
    lv_obj_set_style_pad_row(kb, 2, 0);
    lv_obj_set_style_pad_column(kb, 2, 0);
    // Tung PHIM: nen xam dam + chu trang; giu mau nen nay ca khi focus (khong doi theo theme).
    lv_obj_set_style_bg_color(kb, COL_BTN, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(kb, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(kb, DLG_TXT, LV_PART_ITEMS);
    lv_obj_set_style_border_color(kb, COL_DIM, LV_PART_ITEMS);
    lv_obj_set_style_border_width(kb, 1, LV_PART_ITEMS);
    lv_obj_set_style_radius(kb, 3, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(kb, COL_ACCENT, (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_PRESSED));
    lv_obj_set_style_bg_color(kb, COL_ACCENT, (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_FOCUSED));
    // 2026-10-17 (yeu cau nguoi dung: "ban phim bo duong vien focus"): theme LVGL ve vien focus quanh
    // phim dang chon -> tat o CA part chinh lan tung phim, ca trang thai thuong lan FOCUSED.
    lv_obj_set_style_outline_width(kb, 0, 0);
    lv_obj_set_style_outline_width(kb, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_outline_width(kb, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(kb, 0, (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_FOCUSED));
    lv_obj_set_style_border_width(kb, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(kb, 0, (lv_style_selector_t)(LV_PART_ITEMS | LV_STATE_FOCUSED));
  }
  if (ta) {
    mono_obj(ta);                                               // font chung (xem ghi chu o tren)
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
