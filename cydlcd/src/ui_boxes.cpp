/**
 * ui_boxes.cpp -- hop thoai cua UI LVGL (man hinh CYD): prompt cua Marlin, thong bao,
 * hop XAC NHAN dung chung va hop "tune xong".
 *
 * Tach tu ui_lvgl.cpp (chang 1/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham/bien dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#if CYDLCD_USE_LVGL


// ===========================================================================
//  Prompt cua Marlin + thong bao
// ===========================================================================
static lv_obj_t *s_prompt_box, *s_msg_box;
static lv_obj_t *s_pid_box;                                  // P5: hop "tune xong"
static uint32_t  s_msg_shown_ms = 0, s_msg_seen_ms = 0;
static uint32_t  s_pid_seen = 0;                             // moc pid_done_ms da hien

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
    case CYDLCD_SEV_WARN:  return "CANH BAO";
    case CYDLCD_SEV_ERROR: return "LOI";
    case CYDLCD_SEV_FATAL: return "LOI NANG";
    default:              return "THONG BAO";
  }
}

static lv_color_t sev_color(uint8_t sev) {
  switch (sev) {
    case CYDLCD_SEV_INFO:  return COL_ACCENT;
    case CYDLCD_SEV_WARN:  return COL_WARN;
    default:              return COL_ERR;
  }
}

static lv_obj_t *box_make(const char *title, const char *txt, lv_color_t title_col,
                          const char *const *btns) {
  lv_obj_t *box = lv_msgbox_create(NULL, title, txt, (const char **)btns, false);

  // CANH GIUA MAN HINH (nguoi dung bao 2026-10-15: "cac dialog thong bao dang o goc trai tren").
  // VI SAO: lv_msgbox_create(NULL,...) tao hop trong 1 "backdrop" phu kin lv_layer_top(), NHUNG
  // backdrop do KHONG co layout canh giua (lv_msgbox.c: lv_msgbox_backdrop_class chi dat
  // width/height_def = 100%) -> mac dinh hop nam o (0,0) = goc TREN-TRAI. Phai tu canh giua.
  // max_width: thong bao dai (vd loi cua Marlin) khong tran ra ngoai man hinh.
  lv_obj_set_style_max_width(box, UI_LCD_HOR_RES - 12, 0);
  lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);

  lv_obj_set_style_bg_color(box, COL_BAR, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, title_col, 0);
  lv_obj_set_style_text_color(box, COL_TXT, 0);

  lv_obj_t *t = lv_msgbox_get_title(box);
  if (t) lv_obj_set_style_text_color(t, title_col, 0);
  lv_obj_t *x = lv_msgbox_get_text(box);
  if (x) lv_obj_set_style_text_color(x, COL_TXT, 0);

  lv_obj_t *bm = lv_msgbox_get_btns(box);
  if (bm) {
    lv_obj_set_style_bg_color(bm, COL_BTN, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(bm, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, COL_TXT, LV_PART_ITEMS);
    lv_obj_set_style_border_color(bm, COL_ACCENT, LV_PART_ITEMS);
    lv_obj_set_style_border_width(bm, 1, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(bm, 2, LV_PART_ITEMS);
    if (s_group) { lv_group_add_obj(s_group, bm); lv_group_focus_obj(bm); }
  }
  return box;
}

void refocus_page(void) {
  if (!s_group) return;
  if (s_page[s_page_act]) focus_first_in(s_page[s_page_act]);
  else if (s_bar) lv_group_focus_obj(s_bar);
}

static void cb_prompt_value(lv_event_t *e) {
  lv_obj_t *box = lv_event_get_target(e);
  const char *t = lv_msgbox_get_active_btn_text(box);
  const uint8_t ok = (t && (t[0] == 'O' || t[0] == 'o')) ? 1 : 0;    // OK / CANCEL
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
  lv_obj_t *box = lv_event_get_target(e);
  const char *t = lv_msgbox_get_active_btn_text(box);
  const bool ok = (t && (t[0] == 'O' || t[0] == 'o'));
  if (ok && s_cf_steps > 1) {                    // con 1 buoc xac nhan nua (Reboot DFU)
    s_cf_steps--;
    lv_obj_t *x = lv_msgbox_get_text(s_cf_box);
    if (x) set_text(x, "Chac chan? Bam OK lan nua");
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

  // ---- thong bao moi (cydlcd.message_ms doi) ----
  if (!s_prompt_box && cydlcd.message[0] && cydlcd.message_ms != s_msg_seen_ms) {
    s_msg_seen_ms = cydlcd.message_ms;
    box_close(&s_msg_box);
    static const char *const btns[] = {"OK", ""};
    s_msg_box = box_make(sev_name(cydlcd.message_sev), cydlcd.message,
                         sev_color(cydlcd.message_sev), btns);
    lv_obj_add_event_cb(s_msg_box, cb_msg_value, LV_EVENT_VALUE_CHANGED, NULL);
    s_msg_shown_ms = lv_tick_get();
  }
  // tu dong dong thong bao sau 6s (prompt thi de nguoi dung tra loi)
  if (s_msg_box && lv_tick_elaps(s_msg_shown_ms) > 6000U) box_close(&s_msg_box);

  // ---- P5: Marlin bao tune xong (CYDLCD_EV_PID_DONE) -> hop "da xong" 1 lan ----
  if (cydlcd.pid_done_ms && cydlcd.pid_done_ms != s_pid_seen) {
    s_pid_seen = cydlcd.pid_done_ms;
    if (!s_prompt_box && !s_msg_box && !s_cf_box && !s_pid_box) {
      static const char *const btns[] = {"OK", ""};
      s_pid_box = box_make("TUNE XONG", "Marlin da chay xong M303/M306\nNho SAVE EEPROM neu can",
                           COL_OK, btns);
      lv_obj_add_event_cb(s_pid_box, cb_pid_value, LV_EVENT_VALUE_CHANGED, NULL);
    }
  }
}

#endif  // CYDLCD_USE_LVGL
