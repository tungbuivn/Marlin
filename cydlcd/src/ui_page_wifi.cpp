/**
 * ui_page_wifi.cpp -- man WIFI cua UI LVGL (man hinh CYD): xem trang thai, quet mang,
 * nhap mat khau, ket noi/ngat/quen cau hinh (NVS).
 * API WiFi deu `weak`: thieu module -> trang VAN MO DUOC va bao "khong ho tro".
 *
 * Tach tu ui_lvgl.cpp (chang 4/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf
#include <string.h>                 // strcmp
#include <Arduino.h>                // millis() (hen gioi quet) + Serial.printf (WIFILOG)

#if CYDLCD_USE_LVGL

// ===========================================================================
//  WIFI -- trang quan ly WiFi (mo tu: icon WiFi o header / man STATUS, hoac muc "WiFi" trong MENU)
//
//  BO CUC (PAGE_INN_H = 206; font mono line_height 20 tu 2026-10-15; gap GAP = 2):
//    WiFi: <trang thai> <RSSI>    20
//    SSID: <ssid>                 20
//    IP: <ip>                     20
//    [ MAT KHAU: ... ]            26
//    [ roller "-58dBm <ssid>" ]   68   (4 dong cua font MAC DINH (montserrat_14, line_height 16) chu
//                                       KHONG phai font mono -> khong doi khi font mono to len)
//    [QUET][KET NOI][NGAT][XOA]   26
//    => 20*3 + 26 + 68 + 26 + 5*GAP(10) = 202 <= 206 -> KHONG tran (thanh day khong hien), sat 4 px.
//
//  API WiFi deu `weak` (xem khoi khai bao o dau file): thieu module -> trang VAN MO DUOC, hien
//  "KHONG HO TRO", moi nut khong lam gi (khong crash, khong loi link).
//  Nut QUET goi net_wifi_scan() BLOCKING ~2-4 s -> hien lop phu "DANG QUET MANG..." + KHOA input
//  (lv_indev_enable) trong luc quet (giong cach nhanh firmware lam o pha init), xong moi mo lai.
// ===========================================================================
#define WIFI_ROLLER_H  (4 * 17)        // 68 = 4 dong x line_height 17 (mono mode 4)
#define WIFI_SCAN_SHOW 8               // toi da 8 mang vao roller (o 68 px van cuon duoc)
#define WIFI_PASS_MAX  64

static lv_obj_t *wf_state, *wf_ssid, *wf_ip, *wf_pass, *wf_roller;
static char       s_wifi_pass[WIFI_PASS_MAX] = "";  // mat khau da nhap (KHONG hien ra man hinh)
static bool       s_wifi_busy = false;              // dang quet -> khoa moi callback cua trang
// `s_indev` (input device de KHOA/MO khi quet) KHONG phai cua trang: no thuoc UI chung, dinh nghia
// o ui_lvgl.cpp (khoi "bien toan cuc dung chung") va gan trong ui_lvgl_begin() - xem ui_lvgl_int.h.

// ---- lop phu "DANG QUET..." (tao 1 lan roi an/hien) ----
static lv_obj_t *s_wf_ov, *s_wf_ov_lbl;

static void wifi_ov_show(const bool on, const char *txt) {
  if (!s_wf_ov) {
    s_wf_ov = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(s_wf_ov, 0, 0);
    lv_obj_set_size(s_wf_ov, UI_LCD_HOR_RES, UI_LCD_VER_RES);
    lv_obj_set_style_bg_color(s_wf_ov, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_wf_ov, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_wf_ov, 0, 0);
    lv_obj_set_style_radius(s_wf_ov, 0, 0);
    lv_obj_set_style_pad_all(s_wf_ov, 0, 0);
    lv_obj_clear_flag(s_wf_ov, LV_OBJ_FLAG_SCROLLABLE);
    s_wf_ov_lbl = mk_label_mid(s_wf_ov, 0, 104, UI_LCD_HOR_RES, "", COL_TXT);
    lv_obj_add_flag(s_wf_ov, LV_OBJ_FLAG_HIDDEN);
  }
  if (txt) set_text(s_wf_ov_lbl, txt);
  if (on) lv_obj_clear_flag(s_wf_ov, LV_OBJ_FLAG_HIDDEN);
  else    lv_obj_add_flag(s_wf_ov, LV_OBJ_FLAG_HIDDEN);
}

// ---- lop phu NHAP MAT KHAU: TAO khi bam nut "MAT KHAU...", XOA ngay khi dong (tra pool LVGL) ----
// CHI co khi env bat ban phim: `src/lv_conf_cydlcd.h` (env CYD) dat LV_USE_KEYBOARD/TEXTAREA = 1.
// Env STM32 LVGL dung `lv_conf.h` (de 0) -> khong co ban phim: nut MAT KHAU chi bao "nhap qua COM7"
// (xem nhanh #else) chu KHONG lam hong build.
#if LV_USE_KEYBOARD && LV_USE_TEXTAREA
static lv_obj_t *s_kb_bd, *s_kb_ta, *s_kb_kb;

static void wifi_kb_close(void) {
  if (s_kb_bd) {
    lv_obj_del_async(s_kb_bd);      // xoa trong event cua chinh no -> phai dung _async
    s_kb_bd = NULL; s_kb_ta = NULL; s_kb_kb = NULL;
  }
  refocus_page();                   // tra focus ve man WIFI
}

static void cb_wifi_kb(lv_event_t *e) {
  switch (lv_event_get_code(e)) {
    case LV_EVENT_READY:            // phim "v" (dong y) tren ban phim
      if (s_kb_ta) {
        const char *t = lv_textarea_get_text(s_kb_ta);
        snprintf(s_wifi_pass, sizeof(s_wifi_pass), "%s", t ? t : "");
      }
      wifi_kb_close();
      break;
    case LV_EVENT_CANCEL:           // phim "x" (huy)
      wifi_kb_close();
      break;
    default: break;
  }
}

static void cb_wifi_pass(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || s_kb_bd) return;
  s_kb_bd = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(s_kb_bd, 0, 0);
  lv_obj_set_size(s_kb_bd, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  lv_obj_set_style_bg_color(s_kb_bd, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_kb_bd, LV_OPA_70, 0);
  lv_obj_set_style_border_width(s_kb_bd, 0, 0);
  lv_obj_set_style_radius(s_kb_bd, 0, 0);
  lv_obj_set_style_pad_all(s_kb_bd, 0, 0);
  lv_obj_clear_flag(s_kb_bd, LV_OBJ_FLAG_SCROLLABLE);
  // DAY lop phu len TRUOC (trang WIFI co the bi dung lai sau khi ban phim mo -> trang tao sau se ve de
  // len ban phim). Xem ghi chu giong o ui_page_preheat.cpp.
  lv_obj_move_foreground(s_kb_bd);

  s_kb_ta = lv_textarea_create(s_kb_bd);
  lv_obj_set_pos(s_kb_ta, 2, 4);
  lv_obj_set_size(s_kb_ta, UI_LCD_HOR_RES - 4, KB_TA_H);
  lv_textarea_set_one_line(s_kb_ta, true);
  lv_textarea_set_password_mode(s_kb_ta, true);       // hien ****
  lv_textarea_set_max_length(s_kb_ta, WIFI_PASS_MAX - 1);
  if (s_wifi_pass[0]) lv_textarea_set_text(s_kb_ta, s_wifi_pass);
  lv_obj_add_event_cb(s_kb_ta, cb_wifi_kb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_kb_ta, cb_wifi_kb, LV_EVENT_CANCEL, NULL);

  s_kb_kb = lv_keyboard_create(s_kb_bd);
  // RONG HET man + CAN DAY (phim fill het le trai/phai, hang cuoi khong bi cat) - xem KB_H.
  lv_obj_set_size(s_kb_kb, UI_LCD_HOR_RES, KB_H);
  lv_obj_align(s_kb_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(s_kb_kb, s_kb_ta);
  style_keyboard(s_kb_kb, s_kb_ta);        // nen + vien + mau phim (mac dinh ban phim trong suot)
  lv_obj_add_event_cb(s_kb_kb, cb_wifi_kb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_kb_kb, cb_wifi_kb, LV_EVENT_CANCEL, NULL);
  if (s_group) { lv_group_add_obj(s_group, s_kb_kb); lv_group_focus_obj(s_kb_kb); }
}
#else
// Khong co ban phim trong cau hinh LVGL cua env nay -> nut MAT KHAU khong lam gi (van bam duoc,
// khong crash); mat khau nhap qua console COM7 / dung mang da luu trong NVS.
static void cb_wifi_pass(lv_event_t *e) { (void)e; }
#endif  // LV_USE_KEYBOARD && LV_USE_TEXTAREA

// Do danh sach mang vao roller. Dinh dang 1 dong: "-58dBm <ssid>" (RSSI TRUOC de lay SSID = phan
// sau dau cach DAU TIEN - SSID co the chua dau cach nen khong the cat tu cuoi).
static void wifi_fill_roller(const int n) {
  if (!wf_roller) return;             // trang WIFI da bi thu hoi (xem page_wifi_destroy)
  char buf[WIFI_SCAN_SHOW * 44];      // tren STACK (~352 B) - chi dung 1 lan ngay sau khi quet
  char *p = buf;
  size_t left = sizeof(buf);
  buf[0] = '\0';
  int cnt = (n > WIFI_SCAN_SHOW) ? WIFI_SCAN_SHOW : n;
  if (cnt < 0) cnt = 0;
  for (int i = 0; i < cnt; i++) {
    const char *ssid = net_wifi_scan_ssid ? net_wifi_scan_ssid(i) : NULL;
    if (!ssid) ssid = "?";
    const int rssi = net_wifi_scan_rssi ? (int)net_wifi_scan_rssi(i) : 0;
    const int w = snprintf(p, left, "%s%ddBm %s", (i ? "\n" : ""), rssi, ssid);
    if (w < 0 || (size_t)w >= left) break;
    p += w; left -= (size_t)w;
  }
  if (!cnt) snprintf(buf, sizeof(buf), "(khong tim thay mang)");
  lv_roller_set_options(wf_roller, buf, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(wf_roller, 0, LV_ANIM_OFF);
}

// QUET MANG: uu tien duong KHONG CHAN (net_wifi_scan_begin/poll). Duong cu (dong bo) chi con la
// duong lui khi firmware khong co API async.
//
// !! CANH BAO (2026-10-14): quet dong bo CHAN vong lap 2-8 s (DO THAT: 8,2 s voi 30 mang) ->
// trong luc do MAN HINH CYD KHONG tra loi sector the SD cho Marlin (Marlin bao "SD read error" va DUNG BAN
// IN) va khong phuc vu web server. Vi vay: (1) TU CHOI quet khi Marlin dang in; (2) UI dung duong
// ASYNC de vong lap van chay.
// Ngoai ra: roi trang WIFI KHONG dung toi WiFi (xem wifi_page_leave) - chi QUET/NGAT/XOA moi doi WiFi.
extern "C" bool cyd_sd_printer_busy(void) __attribute__((weak));
extern "C" int  net_wifi_scan_begin(void) __attribute__((weak));
extern "C" int  net_wifi_scan_poll(void) __attribute__((weak));
extern "C" void net_wifi_scan_cancel(void) __attribute__((weak));
extern "C" bool net_wifi_scanning(void) __attribute__((weak));
static bool net_wifi_scanning_weak(void) { return net_wifi_scanning && net_wifi_scanning(); }
// Tam dung mDNS trong luc quet (xem net_upload_http.h) - giam nguy co panic khi interface "down"
extern "C" bool net_upload_mdns_pause(bool pause) __attribute__((weak));


static void cb_wifi_scan(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || !wifi_api_ok() || !wf_roller) return;   // !wf_roller: trang da bi thu hoi
  if (cyd_sd_printer_busy && cyd_sd_printer_busy()) {
    // Quet lam WiFi roi kenh (va truoc day con CHAN vong lap) -> Marlin mat the giua ban in.
    lv_roller_set_options(wf_roller, "(Marlin dang in - khong quet mang)", LV_ROLLER_MODE_NORMAL);
    WIFILOG("[WIFI] TU CHOI quet: Marlin dang in\n");
    return;
  }
  if (net_wifi_scan_begin && net_wifi_scan_poll) {
    // DUONG MOI: quet KHONG CHAN - vong lap van chay (tra loi Marlin + web server + ve man hinh).
    // Tat mDNS trong luc quet (interface se "down" khi radio roi kenh -> mDNS de panic).
    if (net_upload_mdns_pause) net_upload_mdns_pause(true);
    if (net_wifi_scan_begin() != 0) {
      if (net_upload_mdns_pause) net_upload_mdns_pause(false);
      return;                                        // dang quet roi -> bo qua
    }
    s_wifi_busy = true;
    wifi_ov_show(true, "DANG QUET MANG...");
    return;                                          // ket qua lay o page_wifi_update()
  }
  // Duong lui (firmware khong co API async): quet dong bo 2-8 s + khoa input
  s_wifi_busy = true;
  wifi_ov_show(true, "DANG QUET MANG...");
  if (s_indev) lv_indev_enable(s_indev, false);        // KHOA cam ung + encoder
  for (uint8_t k = 0; k < 3; k++) lv_timer_handler();  // ve lop phu TRUOC khi goi ham blocking
  const uint32_t t0 = millis();
  const int n = net_wifi_scan();                       // BLOCKING 2-8 s (duong lui)
  WIFILOG("[WIFI] QUET dong bo xong sau %lu ms\n", (unsigned long)(millis() - t0));
  wifi_fill_roller(n);
  wifi_ov_show(false, NULL);
  if (s_indev) lv_indev_enable(s_indev, true);         // MO KHOA
  s_wifi_busy = false;
  refocus_page();
}

// KET NOI: lay SSID dang chon trong roller + mat khau da nhap; luu NVS (save = true).
// KHONG cho o day: net_wifi_connect() tra ve ngay, trang thai se tu doi qua page_wifi_update().
static void cb_wifi_connect(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || !net_wifi_connect || !wf_roller) return;
  char line[48] = "";
  lv_roller_get_selected_str(wf_roller, line, sizeof(line));
  const char *sp = strchr(line, ' ');
  const char *ssid = (sp && sp[1]) ? (sp + 1) : "";
  if (!ssid[0] || line[0] == '(') return;              // chua chon mang nao
  net_wifi_connect(ssid, s_wifi_pass, true);
}

static void cb_wifi_disconnect(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || !net_wifi_disconnect) return;
  net_wifi_disconnect(false);                          // ngat nhung GIU cau hinh
}

// XOA CAU HINH: khong hoan tac duoc -> xac nhan 2 buoc (giong M502/REBOOT DFU/XOA FILE)
static void cb_wifi_forget(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || !net_wifi_disconnect) return;
  ui_confirm("XOA WIFI", "Xoa cau hinh WiFi da luu (NVS)?\nLan sau phai nhap lai mat khau!",
             (uint8_t)A_WIFI_FORGET, 2);
}

// Dong moi thu CUA TRANG WIFI khi roi trang: ban phim nhap mat khau + lop phu "DANG QUET...".
// (Ca 2 vat deu la con cua lv_scr_act() chu KHONG phai con cua trang, nen neu khong don thi chung
//  van nam tren man hinh o trang khac.) KHONG dung toi WiFi: roi trang khong ngat/ket noi lai gi ca.
void wifi_page_leave(void) {
#if LV_USE_KEYBOARD && LV_USE_TEXTAREA
  if (s_kb_bd) {
    // Ngoai event cua chinh no -> dung lv_obj_del (khong can _async). LVGL tu go vat khoi group.
    lv_obj_del(s_kb_bd);
    s_kb_bd = NULL; s_kb_ta = NULL; s_kb_kb = NULL;
    WIFILOG("[WIFI] roi trang: da dong ban phim mat khau\n");
  }
#endif
  if (s_wf_ov) lv_obj_add_flag(s_wf_ov, LV_OBJ_FLAG_HIDDEN);
  // Dang quet KHONG CHAN ma roi trang -> huy phien quet (khong de no chay ngam)
  if (s_wifi_busy && net_wifi_scan_cancel && net_wifi_scanning_weak()) {
    net_wifi_scan_cancel();
    WIFILOG("[WIFI] roi trang: da huy phien quet\n");
  }
  if (s_wifi_busy && net_upload_mdns_pause) net_upload_mdns_pause(false);   // bat lai mDNS
  s_wifi_busy = false;
  if (s_indev) lv_indev_enable(s_indev, true);   // mo khoa cho chac (truong hop duong lui)
}

void page_wifi_create(lv_obj_t *p) {
  wf_state = mk_label_w(p, "WiFi: -", COL_TXT, PAGE_INN_W);
  wf_ssid  = mk_label_w(p, "SSID: -", COL_DIM, PAGE_INN_W);
  wf_ip    = mk_label_w(p, "IP: -",   COL_DIM, PAGE_INN_W);

  wf_pass = mk_btn(p, "MAT KHAU...", PAGE_INN_W, 26, cb_wifi_pass, NULL);

  wf_roller = lv_roller_create(p);
  lv_obj_set_width(wf_roller, PAGE_INN_W);
  lv_roller_set_visible_row_count(wf_roller, 4);
  lv_obj_set_height(wf_roller, WIFI_ROLLER_H);
  lv_roller_set_options(wf_roller, "(bam QUET de tim mang)", LV_ROLLER_MODE_NORMAL);
  lv_obj_set_style_bg_color(wf_roller, COL_BTN, 0);
  lv_obj_set_style_bg_opa(wf_roller, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(wf_roller, COL_TXT, 0);
  lv_obj_set_style_border_width(wf_roller, 0, 0);
  lv_obj_set_style_text_color(wf_roller, lv_color_hex(0x102030), LV_PART_SELECTED);
  lv_obj_set_style_bg_color(wf_roller, COL_ACCENT, LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(wf_roller, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_add_flag(wf_roller, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  if (s_group) lv_group_add_obj(s_group, wf_roller);

  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 26, 0, GAP);
  mk_btn_grow(r, "QUET",    26, cb_wifi_scan, NULL);
  mk_btn_grow(r, "KET NOI", 26, cb_wifi_connect, NULL);
  mk_btn_grow(r, "NGAT",    26, cb_wifi_disconnect, NULL);
  mk_btn_grow(r, "XOA",     26, cb_wifi_forget, NULL);
}

void page_wifi_update(void) {
  if (!wf_state || !wf_roller || !wf_pass) return;   // trang da bi thu hoi (xem page_wifi_destroy)

  // Dang quet mang (duong KHONG CHAN): hoi ket qua moi nhip 400 ms. Xong -> do vao roller + mo khoa.
  if (s_wifi_busy && net_wifi_scan_poll && net_wifi_scan_begin) {
    const int r = net_wifi_scan_poll();
    if (r == -1) return;                       // con dang quet -> giu nguyen lop phu
    if (r >= 0) wifi_fill_roller(r);
    else        lv_roller_set_options(wf_roller, "(quet that bai)", LV_ROLLER_MODE_NORMAL);
    if (net_upload_mdns_pause) net_upload_mdns_pause(false);   // bat lai mDNS sau khi quet
    wifi_ov_show(false, NULL);
    s_wifi_busy = false;
    refocus_page();
  }

  char b[64];
  if (!wifi_api_ok()) {                                // nhanh firmware chua merge API WiFi
    set_text(wf_state, "WiFi: KHONG HO TRO (chua co module)");
    set_text(wf_ssid, "");
    set_text(wf_ip, "");
    set_text(lv_obj_get_child(wf_pass, 0), "MAT KHAU: -");
    return;
  }
  static const char *const WST[3] = { "TAT", "DANG KET NOI", "DA KET NOI" };
  const uint8_t st = wifi_state_now();
  const int8_t  rs = wifi_rssi_now();
  if (st == 2 && rs) snprintf(b, sizeof(b), "WiFi: %s  %d dBm", WST[2], (int)rs);
  else               snprintf(b, sizeof(b), "WiFi: %s", WST[st <= 2 ? st : 0]);
  set_text(wf_state, b);

  const char *ssid = wifi_ssid_now();
  const char *ip   = wifi_ip_now();
  snprintf(b, sizeof(b), "SSID: %s", ssid[0] ? ssid : "-");
  set_text(wf_ssid, b);
  snprintf(b, sizeof(b), "IP: %s", ip[0] ? ip : "-");
  set_text(wf_ip, b);

  // Nut mat khau: CHI hien do dai (khong lo mat khau ra man hinh). set_text tu so sanh -> cache.
  lv_obj_t *pl = lv_obj_get_child(wf_pass, 0);
  if (pl) {
#if LV_USE_KEYBOARD && LV_USE_TEXTAREA
    if (s_wifi_pass[0]) snprintf(b, sizeof(b), "MAT KHAU: **** (%u)", (unsigned)strlen(s_wifi_pass));
    else                snprintf(b, sizeof(b), "MAT KHAU: (bam de nhap)");
#else
    // Env khong bat ban phim (xem #if o cb_wifi_pass): noi ro duong nhap mat khau khac.
    snprintf(b, sizeof(b), "MAT KHAU: nhap qua COM7 (khong co ban phim)");
#endif
    set_text(pl, b);
  }
}

// Huy man WIFI (xem quy uoc o ui_lvgl_int.h): dat NULL 3 nhan + nut mat khau + roller, va DON ban
// phim nhap mat khau + lop phu "DANG QUET..." (ca 2 la con cua lv_scr_act, KHONG phai con cua trang)
// + huy phien quet khong chan dang chay. show_page() da goi wifi_page_leave() truoc khi thu hoi
// trang; goi lai o day la vo hai (idempotent) va giu cho ham huy tu no an toan.
// s_wifi_pass: GIU NGUYEN (mat khau nguoi dung da nhap - khong phai trang thai widget).
void page_wifi_destroy(void) {
  wifi_page_leave();
  wf_state  = NULL;
  wf_ssid   = NULL;
  wf_ip     = NULL;
  wf_pass   = NULL;
  wf_roller = NULL;
}

#endif  // CYDLCD_USE_LVGL

