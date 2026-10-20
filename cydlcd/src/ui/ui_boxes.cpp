/**
 * ui_boxes.cpp -- hop thoai cua UI LVGL (man hinh CYD): prompt cua Marlin, thong bao,
 * hop XAC NHAN dung chung va hop "tune xong".
 *
 * Tach tu ui_lvgl.cpp (chang 1/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham/bien dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "ui_overlay.h"     // ov_destroy() cho lenh UI MSGHIDE
#include "ui_lang.h"                // T() - da ngon ngu
#include "net_upload_http.h"        // net_notify_set(): thong bao di TRANG /status thay vi hien hop tren LCD

#include <Arduino.h>                // Serial.printf (lenh chan doan `UI TAP K`)

#if CYDLCD_USE_LVGL


// ===========================================================================
//  Prompt cua Marlin + thong bao
// ===========================================================================
static lv_obj_t *s_prompt_box, *s_msg_box;
static lv_obj_t *s_pid_box;                                  // P5: hop "tune xong"
static uint32_t  s_msg_shown_ms = 0, s_msg_seen_ms = 0;
static uint32_t  s_saved_ms = 0;      // 2026-10-17: hen gio hien hop "SAVED" (ui_boxes_saved_soon)
static uint32_t  s_pid_seen = 0;                             // moc pid_done_ms da hien
static lv_obj_t *s_crit_screen = NULL;                       // != NULL khi dang hien MAN HINH LOI NANG

// ===========================================================================
//  MÀN HÌNH LỖI NẶNG (mức FATAL) — NỀN ĐỎ, KHÔNG TẮT ĐƯỢC
//
//  YEU CAU NGUOI DUNG 2026-10-16: "rieng muc FATAL (may da halt, chi tat nguon moi het) thi cho CYD
//  chiem toan man hinh nen do, chu trang: tieu de CRITICAL ERROR + noi dung loi + dong TAT NGUON ROI BAT
//  LAI, khong co nut OK (khong tat duoc - dung nhu 12864). Cac muc INFO/WARN/ERROR van giu nguyen."
//
//  BAN CHAT BEN MARLIN: may that (12864) khi halt thi MarlinUI::draw_kill_screen() ve 3 dong chu
//  "<loi> / HALTED / Please reset" (12864 la man DON SAC nen KHONG the co mau do); cho "do" la
//  PRINTER_EVENT_LEDS -> leds.set_color(RED) + neo.set_background_color(255,0,0,0) tuc DEN NEN sang do.
//  Build nay khong bat 12864 lan PRINTER_EVENT_LEDS/NEOPIXEL_LED nen khong co gi; CYD truoc day cung chi
//  ghi 1 dong o trang /status (CYDLCD_MSG_BOX_ON_LCD = 0). Nay CYD tu lam man hinh do do.
//
//  KHONG CO NUT NAO va KHONG tu dong dong: may da halt thi chi con cach TAT NGUON, de nut OK o day la
//  noi doi voi nguoi dung. Muon het chi co cach tat nguon (hoac reset board CYD).
// ===========================================================================
static void crit_screen_make(const char *msg) {
  if (!s_crit_screen) {
    lv_obj_t *o = lv_obj_create(lv_layer_top());      // lop tren cung -> de len MOI trang/hop khac
    lv_obj_set_size(o, UI_LCD_HOR_RES, UI_LCD_VER_RES);
    lv_obj_set_pos(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(o, CRIT_BG, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_pad_all(o, 8, 0);
    lv_obj_set_flex_flow(o, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(o, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *l;
    l = lv_label_create(o);                            // 1) tieu de
    lv_label_set_text(l, T(UI_T_BOX_SEV_FATAL));
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_bottom(l, 6, 0);

    l = lv_label_create(o);                            // 2) noi dung loi (chuoi Marlin gui sang)
    // (khong dat ten: LVGL 8 khong co lv_obj_set_name -> dua vao THU TU TAO, con so 1)
    lv_obj_set_width(l, UI_LCD_HOR_RES - 16);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);

    l = lv_label_create(o);                            // 3) dong chi cach duy nhat thoat
    lv_label_set_text(l, T(UI_T_CRIT_POWER));
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_top(l, 8, 0);

    s_crit_screen = o;
    Serial.println("[UI] !!! MAN HINH LOI NANG (den do) - may da halt, phai TAT NGUON !!!");
  }
  // Cap nhat noi dung loi (Marlin co the bao lai nhieu lan)
  lv_obj_t *m = lv_obj_get_child(s_crit_screen, 1);
  if (m && msg && msg[0] && strcmp(lv_label_get_text(m), msg) != 0) lv_label_set_text(m, msg);
}

// Hien man hinh loi nang (dung cho lenh chan doan `UI CRIT <chu>`).
// VI SAO PHAI CO HAM RIENG: lenh chan doan khong the chi ghi `cydlcd.message_sev = FATAL` roi mong man
// hinh hien ra - `cydlcd_link.cpp` dinh ky `memcpy(&cydlcd, &s_src, ...)` nen gia tri do bi ghi de ngay
// (duong THAT thi khac: Marlin gui khung MESSAGE voi sev=FATAL -> s_src.message_sev = FATAL -> publish
// -> hook trong boxes_update() thay va goi crit_screen_make()).
void crit_screen_show(const char *msg) { crit_screen_make(msg); }

// Go man hinh loi nang ra. CHI dung cho lenh chan doan `UI CRIT OFF` - may that bi halt thi KHONG co
// duong nao go (phai tat nguon), do la chu y cua thiet ke.
void crit_screen_clear(void) {
  if (!s_crit_screen) return;
  lv_obj_del(s_crit_screen);
  s_crit_screen = NULL;
}

static void box_close(lv_obj_t **box) {
  if (!*box) return;
  // Bo btnmatrix khoi group TRUOC khi xoa (khong de group giu con tro da chet)
  lv_obj_t *bm = lv_msgbox_get_btns(*box);
  if (bm && s_group) lv_group_remove_obj(bm);
  lv_msgbox_close_async(*box);
  *box = NULL;
}

static const char *sev_name(uint8_t sev) {
  switch (sev) {
    case CYDLCD_SEV_INFO:  return "INFO";
    case CYDLCD_SEV_WARN:  return T(UI_T_BOX_SEV_WARN);
    case CYDLCD_SEV_ERROR: return T(UI_T_BOX_SEV_ERR);
    case CYDLCD_SEV_FATAL: return T(UI_T_BOX_SEV_FATAL);
    default:              return T(UI_T_BOX_SEV_NOTICE);
  }
}

static lv_color_t sev_color(uint8_t sev) {
  switch (sev) {
    case CYDLCD_SEV_INFO:  return COL_ACCENT;
    case CYDLCD_SEV_WARN:  return COL_WARN;
    default:              return COL_ERR;
  }
}

// To MAU tung nut cua hop thoai (yeu cau nguoi dung 2026-10-15: "nut OK luon mau xanh va huy hoac xoa
// luon mau do"): nut cua msgbox nam trong 1 `lv_btnmatrix`, ma ban LVGL nay KHONG co
// LV_BTNMATRIX_CTRL_CUSTOM_1/2 de style theo tung nut -> dung su kien DRAW_PART_BEGIN (LVGL cho phep
// doi `rect_dsc`/`label_dsc` cua TUNG nut qua `dsc->id` truoc khi ve). Xac dinh nut theo CHU cua no:
//   OK / YES / DONG Y  -> xanh (DLG_OK_BG)
//   CANCEL / NO / HUY / XOA -> do (DLG_CANCEL_BG)
// Cac nut khac giu nguyen mau nen COL_BTN.
static void cb_box_btn_draw(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!dsc || dsc->part != LV_PART_ITEMS) return;
  lv_obj_t *bm = lv_event_get_target(e);
  const char *t = lv_btnmatrix_get_btn_text(bm, dsc->id);
  if (!t || !t[0]) return;
  char c0 = t[0];
  if (c0 >= 'a' && c0 <= 'z') c0 = (char)(c0 - 'a' + 'A');
  lv_color_t bg;
  if      (c0 == 'O' || c0 == 'Y' || c0 == 'D') bg = DLG_OK_BG;        // OK / YES / Dong y
  else if (c0 == 'C' || c0 == 'N' || c0 == 'H' || c0 == 'X') bg = DLG_CANCEL_BG;  // Cancel/No/Huy/Xoa
  else return;
  if (dsc->rect_dsc) { dsc->rect_dsc->bg_color = bg; dsc->rect_dsc->bg_opa = LV_OPA_COVER; }
  if (dsc->label_dsc) dsc->label_dsc->color = DLG_TXT;
}

static lv_obj_t *box_make(const char *title, const char *txt, lv_color_t title_col,
                          const char *const *btns) {
  lv_obj_t *box = lv_msgbox_create(NULL, title, txt, (const char **)btns, false);

  // CANH GIUA MAN HINH (nguoi dung bao 2026-10-15: "cac dialog thong bao dang o goc trai tren").
  // VI SAO: lv_msgbox_create(NULL,...) tao hop trong 1 "backdrop" phu kin lv_layer_top(), NHUNG
  // backdrop do KHONG co layout canh giua (lv_msgbox.c: lv_msgbox_backdrop_class chi dat
  // width/height_def = 100%) -> mac dinh hop nam o (0,0) = goc TREN-TRAI. Phai tu canh giua.
  // max_width: thong bao dai (vd loi cua Marlin) khong tran ra ngoai man hinh.
  // 2026-10-17: nen mo cua msgbox (LVGL tao 1 backdrop la CHA cua hop) -> thong nhat den 50% +
  // phu kin VUNG NOI DUNG (giong me_mask cua hop sua o mesh).
  {
    lv_obj_t *bd = lv_obj_get_parent(box);
    if (bd && bd != lv_scr_act() && bd != lv_layer_top()) {
      dlg_backdrop(bd);
      lv_obj_set_pos(bd, 0, CONT_Y);
      lv_obj_set_size(bd, UI_LCD_HOR_RES, CONT_H);
    }
  }
  lv_obj_set_style_max_width(box, UI_LCD_HOR_RES - 12, 0);
  lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);

  lv_obj_set_style_bg_color(box, COL_BAR, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  // 2026-10-17 (yeu cau nguoi dung): "padding de cach phan noi dung, va duong bao cua dialog".
  // - duong bao hop: 2px mau tieu de + bo goc 6 (ro rang hon 1px truoc day)
  // - khoang cach giua title / noi dung / hang nut: pad_row cua HOP (flex column), KHONG dung
  //   padding cua btnmatrix (se lam mat mau nut).
  lv_obj_set_style_border_width(box, 2, 0);
  lv_obj_set_style_border_color(box, title_col, 0);
  lv_obj_set_style_radius(box, 6, 0);
  lv_obj_set_style_pad_row(box, 6, 0);
  // 2026-10-17:
  // - "lai bi mat padding ... ngay phia duoi group nut" -> chua 8px o DAY hop.
  // - "ban cach 4 phia the la background title cung bi tut vao roi" -> KHONG dung pad_all nua:
  //   pad_all lam TIEU DE (con flex dau tien) cung bi thut vao trong. Nay:
  //     pad_all = 0  -> tieu de tran het be ngang, sat vien tren (giong thanh header)
  //     pad_bottom = 8 -> chi chua khoang duoi hang nut
  //     phan NOI DUNG tu chua le trai/phai bang padding cua chinh label (xem duoi)
  lv_obj_set_style_pad_all(box, 0, 0);
  lv_obj_set_style_pad_bottom(box, 8, 0);
  lv_obj_set_style_text_color(box, COL_TXT, 0);

  lv_obj_t *t = lv_msgbox_get_title(box);
  if (t) {
    lv_obj_set_style_text_color(t, title_col, 0);
    // 2026-10-17 (yeu cau nguoi dung): "them padding cho title, doi mau nen title".
    // Tieu de thanh 1 dai nen dam (COL_BTN) chu mau ngu nghia (canh bao do/cam...) -> tuong phan tot.
    lv_obj_set_style_bg_color(t, COL_BTN, 0);
    lv_obj_set_style_bg_opa(t, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(t, 4, 0);
    lv_obj_set_style_pad_all(t, 7, 0);
    lv_obj_set_style_pad_left(t, 10, 0);
    lv_obj_set_style_pad_right(t, 10, 0);
  }
  lv_obj_t *x = lv_msgbox_get_text(box);
  if (x) {
    lv_obj_set_style_text_color(x, COL_TXT, 0);
    // Noi dung khong duoc dinh sat vien: tu chua le 8px (va 4px tren / 3px duoi).
    lv_obj_set_style_pad_left(x, 8, 0);
    lv_obj_set_style_pad_right(x, 8, 0);
    lv_obj_set_style_pad_top(x, 4, 0);
    lv_obj_set_style_pad_bottom(x, 3, 0);
  }

  lv_obj_t *bm = lv_msgbox_get_btns(box);
  if (bm) {
    lv_obj_set_style_bg_color(bm, COL_BTN, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(bm, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, DLG_TXT, LV_PART_ITEMS);
    // 2026-10-17 (yeu cau nguoi dung): "nut ok, cancel o phan xac nhan dang bi co mot duong bao
    // xung quanh" -> BO vien 1px mau COL_ACCENT quanh tung nut.
    lv_obj_set_style_border_width(bm, 0, LV_PART_ITEMS);
    lv_obj_set_style_outline_width(bm, 0, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(bm, 2, LV_PART_ITEMS);
    lv_obj_set_style_radius(bm, 4, LV_PART_ITEMS);
    // 2026-10-17 (nguoi dung: "ca group nut ok, cancel dang bi bao boi 1 box mau cyan, bo duong vien do"):
    // Do la vien FOCUS cua THEME LVGL ve quanh CHINH btnmatrix (LV_PART_MAIN, mau #00E7FF), khong
    // phai vien tung nut. Phai tat o PART_MAIN ca trang thai thuong lan FOCUSED.
    lv_obj_set_style_bg_opa(bm, LV_OPA_TRANSP, 0);          // nen nhom trong suot
    lv_obj_set_style_border_width(bm, 0, 0);
    lv_obj_set_style_outline_width(bm, 0, 0);
    lv_obj_set_style_shadow_width(bm, 0, 0);
    lv_obj_set_style_outline_width(bm, 0, LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(bm, 0, LV_STATE_FOCUSED);
    // Khi nut dang duoc chon (encoder), LVGL mac dinh ve vien focus -> bo luon cho sach.
    lv_obj_set_style_outline_width(bm, 0, LV_PART_ITEMS | LV_STATE_FOCUSED);
    lv_obj_set_style_border_width(bm, 0, LV_PART_ITEMS | LV_STATE_FOCUSED);
    // KHOANG TRANG giua cac nut (truoc day 2 nut dinh lien nhau) + hang nut can GIUA hop.
    lv_obj_set_style_pad_column(bm, DLG_BTN_GAP, 0);
    // 2026-10-17: KHONG dat pad_top/pad_bottom tren btnmatrix nua - no lam btnmatrix doi kich thuoc
    // va mau xanh/do do cb_box_btn_draw() to bi mat (do duoc: xanh OK 1046 -> 0 px, do CANCEL
    // 2045 -> 1176 px). Khoang cach voi phan noi dung lam bang `pad_row` cua HOP (xem duoi).
    lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    // Mau xanh/do cho tung nut (xem cb_box_btn_draw).
    lv_obj_add_event_cb(bm, cb_box_btn_draw, LV_EVENT_DRAW_PART_BEGIN, NULL);
    // Chan doan (COM8): in tung nut + mau se to - de kiem tra tu PC ma khong can nhin man hinh.
    {
      char line[128];
      uint8_t used = 0;
      used += (uint8_t)snprintf(line + used, sizeof(line) - used, "[UI] hop '%s':", title ? title : "");
      // (khong co lv_btnmatrix_get_btn_cnt trong ban LVGL nay -> duyet toi da 4 nut roi dung)
      for (uint16_t i = 0; i < 4 && used < sizeof(line) - 16; i++) {
        const char *t = lv_btnmatrix_get_btn_text(bm, i);
        if (!t || !t[0]) continue;
        char c0 = t[0];
        if (c0 >= 'a' && c0 <= 'z') c0 = (char)(c0 - 'a' + 'A');
        const char *col = (c0 == 'O' || c0 == 'Y' || c0 == 'D') ? "XANH"
                        : (c0 == 'C' || c0 == 'N' || c0 == 'H' || c0 == 'X') ? "DO" : "xam";
        used += (uint8_t)snprintf(line + used, sizeof(line) - used, " [%s=%s]", t, col);
      }
      WIFILOG("%s | be rong btnmatrix=%d px (2 nut: 2*W + %d px khe)\n", line, (int)lv_obj_get_width(bm), DLG_BTN_GAP);
    }
    if (s_group) { lv_group_add_obj(s_group, bm); lv_group_focus_obj(bm); }
  }
  return box;
}

void refocus_page(void) {
  if (!s_group) return;
  if (s_page[s_page_act]) focus_first_in(s_page[s_page_act]);
  else if (s_bar) lv_group_focus_obj(s_bar);
}

// Lay HOP THOAI tu su kien cua nut ben trong no.
//
// !!! PHAI DUNG lv_event_get_current_target(), KHONG duoc dung lv_event_get_target() !!!
// Nut cua msgbox la 1 `lv_btnmatrix` va no duoc gan LV_OBJ_FLAG_EVENT_BUBBLE (lv_msgbox.c:135 -
// "To see the event directly on the message box") de su kien VALUE_CHANGED noi len hop. Khi mot su
// kien BUBBLE thi:
//     lv_event_get_target()         = vat GOC phat su kien = BTNMATRIX  (khong phai hop!)
//     lv_event_get_current_target() = vat DANG nhan su kien  = HOP THOAI (vat ta gan callback)
// (lv_event.c:105-113). Ban cu dung lv_event_get_target() nen `box` la btnmatrix; cac ham
// lv_msgbox_get_*() se doc con tro `((lv_msgbox_t*)btnmatrix)->btns` -> rac -> LoadProhibited ->
// PANEL TU REBOOT moi lan bam OK/CANCEL (gap that tren may that 2026-10-15: `UI TAP K` lam CYD
// panic "Guru Meditation ... LoadProhibited"). Xem cydlcd/README.md muc hop thoai.
static lv_obj_t *box_from_event(lv_event_t *e) { return lv_event_get_current_target(e); }

static void cb_prompt_value(lv_event_t *e) {
  lv_obj_t *box = box_from_event(e);
  const char *t = lv_msgbox_get_active_btn_text(box);
  const uint8_t ok = (t && (t[0] == 'O' || t[0] == 'o')) ? 1 : 0;    // OK / CANCEL
  WIFILOG("[UI] tra loi PROMPT cua Marlin: %s (id=%u)\n", ok ? "OK" : "CANCEL", (unsigned)cydlcd.prompt_id);
  cydlcd_link_answer(ok);          // cydlcd_link_answer() tu dat prompt_active = false
  box_close(&s_prompt_box);
  refocus_page();
}

static void cb_msg_value(lv_event_t *e) {
  (void)e;
  box_close(&s_msg_box);
  refocus_page();
}

// P5: dong hop "tune xong"
static void cb_pid_value(lv_event_t *e) {
  (void)e;
  box_close(&s_pid_box);
  refocus_page();
}

// ---------------------------------------------------------------------------
//  Hop XAC NHAN dung chung (P3/P8/P9): OK moi chay lenh, CANCEL thi thoi.
//  steps = 2 -> phai bam OK 2 lan (dung cho Reboot DFU).
// ---------------------------------------------------------------------------
static lv_obj_t *s_cf_box;
static uint8_t   s_cf_action = 0;
static uint8_t   s_cf_steps  = 1;
static uint32_t  s_cf_autoclose_ms = 0;      // >0: tu dong dong hop XAC NHAN sau moc nay

static void cb_confirm_value(lv_event_t *e) {
  lv_obj_t *box = box_from_event(e);             // HOP THOAI (khong phai btnmatrix - xem box_from_event)
  const char *t = lv_msgbox_get_active_btn_text(box);
  const bool ok = (t && (t[0] == 'O' || t[0] == 'o'));
  if (ok && s_cf_steps > 1) {                    // con 1 buoc xac nhan nua (Reboot DFU)
    s_cf_steps--;
    lv_obj_t *x = lv_msgbox_get_text(s_cf_box);
    if (x) set_text(x, T(UI_T_BOX_AGAIN));
    return;
  }
  const uint8_t act = s_cf_action;
  box_close(&s_cf_box);
  refocus_page();
  if (ok) do_confirmed(act);                     // chi chay lenh khi bam OK
}

void ui_confirm(const char *title, const char *txt, uint8_t action, uint8_t steps) {
  static const char *const btns[] = {"OK", "CANCEL", ""};
  box_close(&s_cf_box);
  s_cf_action = action;
  s_cf_steps  = steps ? steps : 1;
  s_cf_autoclose_ms = 0;                       // hop XAC NHAN binh thuong: cho nguoi dung bam
  s_cf_box = box_make(title, txt, COL_WARN, btns);
  lv_obj_add_event_cb(s_cf_box, cb_confirm_value, LV_EVENT_VALUE_CHANGED, NULL);
}

// Tu dong dong hop XAC NHAN sau `ms` mili giay (0 = tat). Dung cho hop TEST mo tu console
// (`UI TESTBOX`) - de khong de 1 hop modal nam tren man hinh khi khong ai bam.
void ui_confirm_autoclose(const uint32_t ms) {
  s_cf_autoclose_ms = ms ? lv_tick_get() + ms : 0;
}

// CHAN DOAN (lenh `UI TAP K` tren COM7): bam nut OK cua HOP THOAI dang mo (prompt cua Marlin hoac hop
// XAC NHAN). Di DUNG duong su kien that: nut bam trong msgbox la 1 btnmatrix co LV_OBJ_FLAG_EVENT_BUBBLE
// (lv_msgbox.c:135), va cb_prompt_value()/cb_confirm_value() doc nut DANG CHON cua btnmatrix
// (lv_msgbox_get_active_btn_text()) -> chi can chon nut 0 (OK) roi ban VALUE_CHANGED vao chinh
// btnmatrix, y nhu nguoi dung cham vao nut do.
// Dung de kiem tra tu PC: duong TRA LOI PROMPT cua Marlin (vd `M0` -> cho nguoi dung) va duong XOA
// cua man FILE (hop 2 buoc: phai bam OK 2 lan moi chay lenh that).
void boxes_test_ok(void) {
  lv_obj_t *box = s_prompt_box ? s_prompt_box : s_cf_box;   // uu tien prompt cua Marlin (dang cho tra loi)
  if (!box) { Serial.println("[UI] khong co hop PROMPT / XAC NHAN nao dang mo"); return; }
  lv_obj_t *bm = lv_msgbox_get_btns(box);
  if (!bm) return;
  lv_btnmatrix_set_selected_btn(bm, 0);              // 0 = "OK" (mang nut {"OK", "CANCEL", ""})
  lv_event_send(bm, LV_EVENT_VALUE_CHANGED, NULL);
  // Hop XAC NHAN 2 buoc con dang mo (moi qua buoc 1) -> hen lai gio tu dong dong de lan `UI TAP K`
  // ke tiep van kip bam (moi lenh console cach nhau vai giay).
  if (s_cf_box && box == s_cf_box) s_cf_autoclose_ms = lv_tick_get() + 8000U;
}

// In ra HOP THOAI nao dang mo (ten + noi dung) - cho chan doan `UI BOX` tren COM7.
// VI SAO CAN: tren lop tren co 4 hop khac nhau (prompt cua Marlin / XAC NHAN / THONG BAO / PID) va
// chung deu la msgbox giong het nhau khi nhin theo cay vat; doc chu moi biet dang la hop nao (vd
// `M0` -> hop PROMPT cua Marlin dang cho nguoi dung tra loi).
void boxes_dump(void) {
  lv_obj_t *const boxes[4] = { s_prompt_box, s_cf_box, s_msg_box, s_pid_box };
  const char *const names[4] = { "PROMPT cua Marlin", "XAC NHAN", "THONG BAO", "PID" };   // i18n-ok (khong phai chu hien thi)
  for (uint8_t i = 0; i < 4; i++) {
    if (!boxes[i]) continue;
    lv_obj_t *t = lv_msgbox_get_title(boxes[i]);
    lv_obj_t *x = lv_msgbox_get_text(boxes[i]);
    Serial.printf("[UI] hop %s: \"%s\" | \"%s\"\n", names[i],
                  t ? lv_label_get_text(t) : "", x ? lv_label_get_text(x) : "");
  }
}

void boxes_update(void) {
  // ---- prompt cua Marlin: uu tien cao nhat ----
  if (cydlcd.prompt_active) {
    if (!s_prompt_box) {
      box_close(&s_msg_box);                    // thong bao nhuong cho prompt
      static const char *const btns[] = {"OK", "CANCEL", ""};
      s_prompt_box = box_make("MARLIN", cydlcd.prompt_text[0] ? cydlcd.prompt_text : "(prompt)",
                              COL_WARN, btns);
      lv_obj_add_event_cb(s_prompt_box, cb_prompt_value, LV_EVENT_VALUE_CHANGED, NULL);
    }
    else {
      lv_obj_t *x = lv_msgbox_get_text(s_prompt_box);
      if (x) set_text(x, cydlcd.prompt_text[0] ? cydlcd.prompt_text : "(prompt)");
    }
  }
  else box_close(&s_prompt_box);

  // Hop XAC NHAN co hen gio tu dong dong (chi hop TEST tu console dat hen gio - xem ui_confirm_autoclose)
  if (s_cf_box && s_cf_autoclose_ms && lv_tick_elaps(s_cf_autoclose_ms) < 0x80000000UL
      && (int32_t)(lv_tick_get() - s_cf_autoclose_ms) >= 0) {
    s_cf_autoclose_ms = 0;
    box_close(&s_cf_box);
    refocus_page();
  }

  // ---- hop "SAVING..." -> "SAVED" sau khi bam SAVE o man BED MESH ----
  if (s_saved_ms && (uint32_t)(lv_tick_get() - s_saved_ms) >= 1200u) {
    s_saved_ms = 0;
    if (s_msg_box) {
      lv_obj_t *x = lv_msgbox_get_text(s_msg_box);
      if (x) set_text(x, T(UI_T_CAL_SAVED));
      lv_obj_t *bm = lv_msgbox_get_btns(s_msg_box);
      if (bm) lv_obj_clear_flag(bm, LV_OBJ_FLAG_HIDDEN);
      s_msg_shown_ms = lv_tick_get();       // bat dau dem 2,5 s tu dong dong
    }
  }

  // ---- thong bao moi (cydlcd.message_ms doi) ----
  // !!! KHONG HIEN HOP THONG BAO TREN LCD NUA !!! (yeu cau nguoi dung 2026-10-15: "cac hop thoai thong bao
  // status ma co nut ok va tu an khong hien thi tren man hinh, chi hien thi o trang status; o trang status
  // luon luu lai message lan cuoi, sau thoi gian timeout thi tu bien mat neu co time, khong co time thi cu
  // de lai"). Hop nay dung la loai "co nut OK + tu an sau 6 s" -> nay chi luu vao trang /status
  // (`net_notify_set`, TTL = CYDLCD_MSG_TTL_MS) - man hinh khong bi che nua.
  // Muon quay lai hanh vi cu (hien hop tren LCD): dat CYDLCD_MSG_BOX_ON_LCD = 1 trong cydlcd_config.h.
  // ---- MAN HINH LOI NANG (FATAL): chiem toan man hinh, KHONG tat duoc (xem crit_screen_make) ----
  // Bat theo CA `state == KILLED` chu khong chi theo message: Marlin goi onPrinterKilled() ->
  // push_message(SEV_FATAL) + push_status(), nhung neu message den muon/khong kem thi trang thai KILLED
  // van phai lam man hinh do hien ra.
  if (cydlcd.message_sev == CYDLCD_SEV_FATAL || cydlcd.state == CYDLCD_ST_KILLED)
    crit_screen_make(cydlcd.message);

  if (!s_prompt_box && cydlcd.message[0] && cydlcd.message_ms != s_msg_seen_ms) {
    s_msg_seen_ms = cydlcd.message_ms;
    if (CYDLCD_MSG_BOX_ON_LCD) {
      box_close(&s_msg_box);
      static const char *const btns[] = {"OK", ""};
      s_msg_box = box_make(sev_name(cydlcd.message_sev), cydlcd.message,
                           sev_color(cydlcd.message_sev), btns);
      lv_obj_add_event_cb(s_msg_box, cb_msg_value, LV_EVENT_VALUE_CHANGED, NULL);
      s_msg_shown_ms = lv_tick_get();
    } else {
      net_notify_set(cydlcd.message, cydlcd.message_sev, CYDLCD_MSG_TTL_MS);
      Serial.printf("[UI] thong bao [%s] -> TRANG /status (khong hien hop tren LCD): \"%s\"\n",
                    sev_name(cydlcd.message_sev), cydlcd.message);
    }
  }
  // tu dong dong thong bao sau 6s (prompt thi de nguoi dung tra loi) - chi con tac dung khi
  // CYDLCD_MSG_BOX_ON_LCD = 1 (mac dinh 0 nen `s_msg_box` luon NULL).
  if (s_msg_box && lv_tick_elaps(s_msg_shown_ms) > 6000U) box_close(&s_msg_box);

  // ---- P5: Marlin bao tune xong (CYDLCD_EV_PID_DONE) -> hop "da xong" 1 lan ----
  if (cydlcd.pid_done_ms && cydlcd.pid_done_ms != s_pid_seen) {
    s_pid_seen = cydlcd.pid_done_ms;
    if (!s_prompt_box && !s_msg_box && !s_cf_box && !s_pid_box) {
      static const char *const btns[] = {"OK", ""};
      s_pid_box = box_make(T(UI_T_BOX_TUNE_TITLE), T(UI_T_BOX_TUNE_TXT),
                           COL_OK, btns);
      lv_obj_add_event_cb(s_pid_box, cb_pid_value, LV_EVENT_VALUE_CHANGED, NULL);
    }
  }
}

// 2026-10-17 (yeu cau nguoi dung: "chua thay co dialog loi, tao anh de toi nhin"):
// Hop thong bao tren LCD chi hien khi CYDLCD_MSG_BOX_ON_LCD = 1 (mac dinh 0 -> thong bao di trang
// /status). Ham nay mo hop thong bao THAT (cung duong box_make) de chup anh / kiem tra mau theo muc do.
// 2026-10-17: hop PROMPT cua Marlin (tieu de "MARLIN", 2 nut OK/CANCEL) - de chup anh kiem tra.
// 2026-10-17: dong MOI hop thoai + tat lop phu upload (don dep giua cac canh chup anh).
void ui_boxes_test_hide(void) {
  box_close(&s_msg_box);
  box_close(&s_pid_box);
  box_close(&s_prompt_box);
  box_close(&s_cf_box);
  cydlcd.prompt_active = false;
  ov_destroy();                 // lop phu upload: pha huy hoan toan
  refocus_page();
}

void ui_boxes_test_prompt(void) {
  cydlcd.prompt_active = true;
  snprintf(cydlcd.prompt_text, sizeof(cydlcd.prompt_text), "Marlin is waiting for an answer: continue printing?");
  boxes_update();
}

// 2026-10-17: hop "tune xong" (CYDLCD_EV_PID_DONE) - de chup anh kiem tra.
void ui_boxes_test_pid(void) {
  cydlcd.pid_done_ms = lv_tick_get() + 1;      // doi moc -> boxes_update() tao hop
  if (!cydlcd.pid_done_ms) cydlcd.pid_done_ms = 1;
  boxes_update();
}

// 2026-10-17: hop thong bao INFO voi chu tuy y (1 nut OK, tu dong dong sau 2,5 s).
void ui_boxes_info(const char *title, const char *txt) {
  box_close(&s_msg_box);
  static const char *const btns[] = {"OK", ""};
  s_msg_box = box_make(title, txt, COL_ACCENT, btns);
  lv_obj_add_event_cb(s_msg_box, cb_msg_value, LV_EVENT_VALUE_CHANGED, NULL);
  s_msg_shown_ms = lv_tick_get();
}

// 2026-10-17 (yeu cau nguoi dung: "kiem tra ca nut save, neu save ok thi popup dialog saved"):
// Hen hien hop "SAVED" sau 1,2 s. VI SAO CO DO TRE: CYD chi GUI lenh chu khong doc duoc tra loi
// `ok` cua Marlin (khong co kenh ACK nguoc), nen day la bao "da gui lenh luu" sau khi cho Marlin
// kip thuc hien G29 P3 + G29 S0. KHONG phai xac nhan doc ghi EEPROM thanh cong.
void ui_boxes_saving_show(void) {
  // 2026-10-17 (yeu cau nguoi dung: "hop thoai save can hien ngay luc user bam nut save de biet no dang
  // save 'saving', sau khi save xong thi doi text thanh 'saved'"):
  //   - HIEN NGAY hop voi chu "SAVING...", AN nut OK (chua cho bam)
  //   - sau 1,2 s: doi chu thanh "SAVED", HIEN nut OK, bat dau dem 2,5 s tu dong dong
  box_close(&s_msg_box);
  static const char *const btns[] = {"OK", ""};
  s_msg_box = box_make(T(UI_T_PT_MESH), T(UI_T_CAL_SAVING), COL_ACCENT, btns);
  lv_obj_add_event_cb(s_msg_box, cb_msg_value, LV_EVENT_VALUE_CHANGED, NULL);
  lv_obj_t *bm = lv_msgbox_get_btns(s_msg_box);
  if (bm) lv_obj_add_flag(bm, LV_OBJ_FLAG_HIDDEN);     // chua cho bam OK khi dang luu
  s_msg_shown_ms = 0;                                  // chua dem tu dong dong
  s_saved_ms = lv_tick_get() | 1u;
}

void ui_boxes_test_msg(uint8_t sev) {
  // 2026-10-17 (yeu cau nguoi dung: "hop error, warning, marlin, info dang dung tieng viet, hay dung
  // tieng anh"): day la chuoi CHAN DOAN (chi mo bang lenh `UI MSGBOX`) -> viet tieng Anh cho dong bo
  // voi che do EN mac dinh. i18n-ok (khong phai chuoi hien thi thong thuong cua san pham).
  static const char *const txts[4] = {
    "INFO: this is an INFO message box",
    "WARNING: this is a WARNING message box",
    "ERROR: this is an ERROR message box",
    "CRITICAL: this is a CRITICAL message box"
  };
  if (sev > 3) sev = 2;
  Serial.printf("[UI] MSGBOX sev=%u\n", (unsigned)sev);   // chan doan
  box_close(&s_msg_box);
  static const char *const btns[] = {"OK", ""};
  s_msg_box = box_make(sev_name(sev), txts[sev], sev_color(sev), btns);
  lv_obj_add_event_cb(s_msg_box, cb_msg_value, LV_EVENT_VALUE_CHANGED, NULL);
  s_msg_shown_ms = lv_tick_get();
}


#endif  // CYDLCD_USE_LVGL
