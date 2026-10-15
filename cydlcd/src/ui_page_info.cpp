/**
 * ui_page_info.cpp -- man INFO cua UI LVGL (man hinh CYD): 11 dong thong tin THAT
 * (firmware, board, font, WiFi, protocol, link, SD, he thong).
 *
 * Tach tu ui_lvgl.cpp (chang 4/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf
#include <string.h>                 // strcmp
#include <Arduino.h>                // ESP.getFreeHeap() (dong "Heap") + Serial.printf (WIFILOG)

#if CYDLCD_USE_LVGL

// ===========================================================================
//  INFO -- 11 dong thong tin THAT (moi dong 1 label rong het PAGE_INN_W)
//
//  Chieu cao: 11 dong * 20 px (font mono che do 4: line_height 20 tu 2026-10-15) + 10 *
//  INFO_ROW_GAP(1) = 230 > PAGE_INN_H 206 -> man INFO nay TRAN va thanh day PREV/NEXT HIEN (truoc day
//  font line_height 17 nen 197 px, vua khin). Khong sao: 11 dong la chu thong tin, cuon 2 trang van doc
//  het; muon het tran thi phai bo bot dong hoac chia 2 cot (chua lam).
//
//  Truoc day: cot 7 dong cao CO DINH 124 + nut REFRESH FULL + 1 dong chu thich. Da bo nut va dong
//  chu thich (theo yeu cau) va bo luon KHUNG CAO CO DINH: nay cac label la CON TRUC TIEP cua
//  trang, ma mk_page da bat LV_OBJ_FLAG_SCROLLABLE -> noi dung dai hon man thi trang TU CUON duoc
//  va bot_sync() se hien thanh day phan trang. Khong tu chan cuon o dau.
// ===========================================================================
#define INFO_ROW_GAP 1     // GAP chuan cua mk_page la 2: 11 dong se thanh 207 > 206 (cat 1 px)

static lv_obj_t *in_fw, *in_board, *in_ui, *in_font, *in_wifi, *in_proto, *in_marlin, *in_stat,
                *in_rx, *in_sd, *in_sys;

void page_info_create(lv_obj_t *p) {
  // Rieng man nay cach nhau 1 px (11 dong) - xem INFO_ROW_GAP.
  lv_obj_set_style_pad_row(p, INFO_ROW_GAP, 0);

  // Thu tu tao = thu tu hien (mk_page la flex COLUMN).
  in_fw     = mk_label_w(p, "FW: -",     COL_TXT, PAGE_INN_W);
  in_board  = mk_label_w(p, "BOARD: -",  COL_TXT, PAGE_INN_W);
  in_ui     = mk_label_w(p, "UI: -",     COL_DIM, PAGE_INN_W);
  in_font   = mk_label_w(p, "FONT: -",   COL_DIM, PAGE_INN_W);
  in_wifi   = mk_label_w(p, "WIFI: -",   COL_DIM, PAGE_INN_W);
  in_proto  = mk_label_w(p, "PROTO: -",  COL_DIM, PAGE_INN_W);
  in_marlin = mk_label_w(p, "MARLIN: -", COL_DIM, PAGE_INN_W);
  in_stat   = mk_label_w(p, "LINK: -",   COL_DIM, PAGE_INN_W);
  in_rx     = mk_label_w(p, "RX: -",     COL_DIM, PAGE_INN_W);
  in_sd     = mk_label_w(p, "SD: -",     COL_DIM, PAGE_INN_W);
  in_sys    = mk_label_w(p, "SYS: -",    COL_DIM, PAGE_INN_W);
}

void page_info_update(void) {
  if (!in_fw) return;                    // trang da bi thu hoi (an toan: xem page_info_destroy)
  char b[64], t[16];

  // Ten may: o che do PUSH lay tu goi HELLO cua Marlin. O che do TRAM khong co HELLO
  // (link chi gui dong "TBT ...") nen dung ten khai trong cydlcd_config.h.
  snprintf(b, sizeof(b), "FW: %s", cydlcd.name[0] ? cydlcd.name
           : (tbt.ever ? CYDLCD_MACHINE_NAME " (TBT link)" : "(chua co HELLO)"));
  set_text(in_fw, b);

  // Board + do phan giai + so heater/quat MA MARLIN BAO trong HELLO (chua co HELLO thi thoi,
  // khong in so 0 gia).
  int n = snprintf(b, sizeof(b), "BOARD: %s %dx%d", CYDLCD_NAME, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  if (n > 0 && (size_t)n < sizeof(b) && (cydlcd.n_heaters || cydlcd.n_fans_max))
    snprintf(b + n, sizeof(b) - (size_t)n, " h%u f%u",
             (unsigned)cydlcd.n_heaters, (unsigned)cydlcd.n_fans_max);
  set_text(in_board, b);

  // Phien ban LVGL THAT (macro cua lvgl.h - truoc day ghi cung "LVGL 8.3", sai vi dang la 8.4)
  // + kieu dau vao dang dung (encoder/touch) + che do font (4 = mono GFX, 0 = Montserrat).
#if UI_MONO_FONT == 4
  snprintf(b, sizeof(b), "UI: LVGL %d.%d %s mono4", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
           ui_port_input_is_encoder() ? "encoder" : "touch");
#else
  snprintf(b, sizeof(b), "UI: LVGL %d.%d %s montserrat", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
           ui_port_input_is_encoder() ? "encoder" : "touch");
#endif
  set_text(in_ui, b);

  // Che do 4: bpp + so dai glyph + ket qua TU KIEM TRA driver luc khoi dong
  // (s_gfx_mono NULL = driver loi -> UI dang dung lui ve Montserrat 14).
  // Che do 0: chi Montserrat -> noi ro KHONG co dau tieng Viet.
#if UI_MONO_FONT == 4
  snprintf(b, sizeof(b), "FONT: mono4 %dbpp %d dai %s", UI_MONO_GFX_BPP, (int)UI_GFX_RANGE_COUNT,
           s_gfx_mono ? "OK" : "LOI->MS14");
#else
  snprintf(b, sizeof(b), "FONT: Montserrat (khong dau)");
#endif
  set_text(in_font, b);

  // Trang thai WiFi - CUNG NGUON voi icon o header (wifi_state_now(), xem dau file).
  static const char *const WIFI_TXT[3] = { "OFF", "DANG KET NOI", "DA KET NOI" };
  const uint8_t ws = wifi_state_now();
  snprintf(b, sizeof(b), "WIFI: %s", WIFI_TXT[ws <= 2 ? ws : 0]);
  set_text(in_wifi, b);

  // Phien ban protocol PUSH: cua Marlin (gui trong HELLO) / cua MAN HINH CYD + co nang luc (hello_flags).
  snprintf(b, sizeof(b), "PROTO: %u/%u cap 0x%02X", (unsigned)cydlcd.proto,
           (unsigned)CYDLCD_PROTO_VERSION, (unsigned)cydlcd.hello_flags);
  set_text(in_proto, b);

  // marlin_fw (u16 trong HELLO) + SHORT_BUILD_VERSION neu Marlin gui (proto >= 2).
  if (cydlcd.marlin_ver[0])
    snprintf(b, sizeof(b), "MARLIN: 0x%04X %s", (unsigned)cydlcd.marlin_fw, cydlcd.marlin_ver);
  else
    snprintf(b, sizeof(b), "MARLIN: 0x%04X", (unsigned)cydlcd.marlin_fw);
  set_text(in_marlin, b);

  // Link (PUSH/TRAM) + trang thai may.
  snprintf(b, sizeof(b), "LINK: %s %s %s", link_ok() ? "OK" : "NO",
           tbt.ever ? "(TBT)" : (cydlcd.linked ? "(PUSH)" : ""), state_name(cydlcd.state));
  set_text(in_stat, b);

  // RX/ERR: 2 CO MOT CHIEU (cydlcd_link.h) - khong con bo dem nen KHONG BAO GIO tran:
  //   rx_frames = da nhan duoc it nhat 1 frame hop le; rx_errors = da tung gap frame loi.
  snprintf(b, sizeof(b), "RX: %s  ERR: %s", cydlcd.rx_frames ? "OK" : "--",
           cydlcd.rx_errors ? "co" : "khong");
  set_text(in_rx, b);

  // Danh sach file SD + luoi bed mesh (ca 2 deu lay tu du lieu Marlin da gui).
  if (cydlcd.mesh_valid)
    snprintf(b, sizeof(b), "SD: %u file  mesh %ux%u", (unsigned)cydlcd.n_files,
             (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny);
  else
    snprintf(b, sizeof(b), "SD: %u file  mesh -", (unsigned)cydlcd.n_files);
  set_text(in_sd, b);

  // Thoi gian chay cua MAN HINH CYD + heap tu do (chi ESP32 moi co ESP.getFreeHeap()).
  fmt_time(t, sizeof(t), ui_port_millis() / 1000U);
#if defined(ARDUINO_ARCH_ESP32)
  snprintf(b, sizeof(b), "SYS: up %s heap %lu", t, (unsigned long)ESP.getFreeHeap());
#else
  snprintf(b, sizeof(b), "SYS: up %s", t);
#endif
  set_text(in_sys, b);
}

// Huy man INFO (xem quy uoc o ui_lvgl_int.h): dat NULL 11 nhan. Container trang do show_page()
// xoa bang lv_obj_del() nen khong xoa widget o day. Khong co trang thai rieng phai tra ve ban dau:
// moi dong deu duoc page_info_update() dung lai tu du lieu cua Marlin / cua man hinh.
void page_info_destroy(void) {
  in_fw = NULL; in_board = NULL; in_ui = NULL; in_font = NULL; in_wifi = NULL;
  in_proto = NULL; in_marlin = NULL; in_stat = NULL; in_rx = NULL; in_sd = NULL; in_sys = NULL;
}

#endif  // CYDLCD_USE_LVGL

