/**
 * sim_stubs.cpp -- cac module PHAN CUNG cua man hinh CYD duoc thay bang ban gia tren PC.
 *
 * Trong ban that, cac viec nay do:
 *   cyd_sd.cpp / net_wifi.cpp / net_upload_http.cpp / logq.cpp / main.cpp (UART Marlin, heartbeat)
 * Ban gia lap khong keo cac file do vao (chung can Arduino/ESP32/SD/WiFi that) ma cung cap:
 *   - CydlcdSerialStub + cydlcd_millis(): UART gia cho `cydlcd_port.h` (che do CYDLCD_HOST_TEST) de
 *     chinh `src/cydlcd_link.cpp` + `src/tbt_link.cpp` (code THAT) chay duoc tren PC;
 *   - cyd_sd_*: the SD "khong co the" (man FILE/WIFI van ve duoc, chi bao chua co the);
 *   - logq_*: log khong chan -> in thang ra stdout;
 *   - g_hb_*: heartbeat (main.cpp that dinh nghia; UI chi doc).
 *
 * KHONG dinh nghia net_wifi_* / net_upload_*: `src/ui_lvgl.cpp` da khai bao chung la `weak` va co
 * accessor an toan (tra "khong ho tro") khi thieu dinh nghia - xem ui_lvgl_int.h khoi "WiFi (weak)".
 */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#include "cydlcd_config.h"
#include "cydlcd_port.h"        // che do CYDLCD_HOST_TEST: khai bao CydlcdSerialStub + CYDLCD_LINK_BEGIN

// ===========================================================================
//  UART Marlin GIA (hop dong cua cydlcd_port.h khi CYDLCD_HOST_TEST)
// ===========================================================================
// Giong `tools/test_cydlcd_link.cpp`: bo tao/giai frame that cua cydlcd_link.cpp ghi vao `txlog`,
// con `rxq` la duong "Marlin -> man hinh" (kich ban co the bom frame vao de xem UI cap nhat).
#define SIM_RXQ 4096
#define SIM_TXQ 4096
static uint8_t  s_rxq[SIM_RXQ];
static int      s_rx_head = 0, s_rx_tail = 0;
static uint8_t  s_txlog[SIM_TXQ];
static int      s_tx_len = 0;

int    CydlcdSerialStub::available() { return s_rx_head - s_rx_tail; }
int    CydlcdSerialStub::read() { return (s_rx_head > s_rx_tail) ? s_rxq[s_rx_tail++] : -1; }
size_t CydlcdSerialStub::write(const uint8_t *b, size_t l) {
  if (s_tx_len + (int)l > SIM_TXQ) l = (size_t)(SIM_TXQ - s_tx_len);
  memcpy(s_txlog + s_tx_len, b, l);
  s_tx_len += (int)l;
  return l;
}
size_t CydlcdSerialStub::write(uint8_t c) { if (s_tx_len < SIM_TXQ) s_txlog[s_tx_len++] = c; return 1; }
int    CydlcdSerialStub::availableForWrite() { return SIM_TXQ - s_tx_len; }

CydlcdSerialStub CYDLCD_MARLIN_SERIAL;

// Dong ho: ban that la `millis()` cua Arduino; o day lay dong ho AO cua sim (`cydlcd_millis()` duoc
// `cydlcd_port.h` yeu cau khi CYDLCD_HOST_TEST).
#include "sim.h"
uint32_t cydlcd_millis(void) { return sim_millis(); }

// ===========================================================================
//  THE SD gia: "chua co the" (cyd_sd.h khai bao trong khoi extern "C")
// ===========================================================================
extern "C" {
bool    cyd_sd_ready(void)         { return false; }
bool    cyd_sd_printer_busy(void)  { return false; }
uint8_t cyd_sd_last_op(void)       { return 0; }
void    cyd_sd_perf_report(void)   { }
void    cyd_sd_log_dir(const char *) { }
void    cyd_sd_log_root(void)      { }
}  // extern "C"

// ===========================================================================
//  WiFi / upload gia: xem sim_net_stubs.cpp (PHAI la file rieng - tranh thuoc tinh `weak`)
// ===========================================================================


// ===========================================================================
//  LOG KHONG CHAN gia: in thang ra stdout
// ===========================================================================
void    logq_begin(void) { }
bool    logq_post(const char *fmt, ...) {
  va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
  return true;
}
void     logq_task(uint8_t) { }
uint32_t logq_dropped(void) { return 0; }
uint8_t  logq_pending(void) { return 0; }

// ===========================================================================
//  Heartbeat tung task (ban that o main.cpp): UI chi doc de hien thi/chan doan
// ===========================================================================
volatile uint32_t g_hb_link = 0, g_hb_ui = 0, g_hb_net = 0, g_hb_log = 0;

// ===========================================================================
//  `ui_console_dispatch` (ban that o main.cpp - o day chi can du cho kich ban)
// ===========================================================================
#include <stdlib.h>              // atoi() cho `UI MSGBOX <n>`
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

void ui_console_dispatch(const char *a3) {
  if (!a3) return;
  while (*a3 == ' ') a3++;
  if (!*a3) { ui_lvgl_show_page_name("?"); return; }

  if (!strncasecmp(a3, "HIT", 3))        ui_lvgl_test_hit(a3 + 3);
  else if (!strcasecmp(a3, "BOX"))       ui_lvgl_dump_layout();
  else if (!strcasecmp(a3, "TEMPS"))     ui_lvgl_dump_temps();
  else if (!strcasecmp(a3, "GRAD"))      ui_lvgl_test_grad();
  else if (!strcasecmp(a3, "HELLO"))     ui_lvgl_test_hello();
  else if (!strcasecmp(a3, "RX"))        ui_lvgl_test_rx();
  else if (!strcasecmp(a3, "PERF"))      ui_lvgl_test_perf();
  else if (!strcasecmp(a3, "COLORTEST")) ui_port_colortest_raw();
  else if (!strcasecmp(a3, "BUSY"))      ui_lvgl_dump_busy();
  else if (!strncasecmp(a3, "LANG", 4))  ui_lvgl_test_lang(a3 + 4);
  else if (!strncasecmp(a3, "PRE", 3))   preheat_test_cmd(a3 + 3);
  else if (!strncasecmp(a3, "TAP", 3))   ui_lvgl_test_tap(a3 + 3);
  else if (!strncasecmp(a3, "EDIT", 4))  settings_test_edit(a3 + 4);
  else if (!strncasecmp(a3, "MCELL", 5)) mesh_test_cell(a3 + 5);
  else if (!strncasecmp(a3, "MSEL", 4))  mesh_test_select(a3 + 4);
  // 2026-10-17: BO SUNG cac lenh TRUOC DAY BI THIEU -> chung roi vao nhanh cuoi ("ten trang") nen
  // KHONG LAM GI, khien thua vien anh (tools/make_gallery.py) co 3 canh bi TRUNG: menu-tree/confirm
  // va fatal-red deu la anh cua trang dang mo. Thieu: TESTBOX, CRIT, CFG, NOTIFY, SET, BACK/NEXT/PREV.
  else if (!strcasecmp(a3, "TESTBOX"))     ui_lvgl_test_dialog();
  // 2026-10-17 (yeu cau nguoi dung: "chua thay co dialog loi, tao anh de toi nhin"):
  // `UI MSGBOX <0..3>` = mo hop THONG BAO that theo muc do (0 INFO / 1 WARN / 2 ERROR / 3 FATAL).
  else if (!strncasecmp(a3, "MSGBOX", 6))  ui_boxes_test_msg((uint8_t)atoi(a3 + 6));
  else if (!strncasecmp(a3, "CRIT", 4))    ui_lvgl_test_crit(a3 + 4);
  else if (!strncasecmp(a3, "CFG", 3))     submenu_dump(a3 + 3);
  else if (!strncasecmp(a3, "NOTIFY", 6))  ui_lvgl_test_notify(a3 + 6);
  else if (!strcasecmp(a3, "SET"))         page_settings_dump();
  else if (!strcasecmp(a3, "BACK"))        ui_lvgl_test_back();
  else if (!strcasecmp(a3, "NEXT"))        ui_lvgl_test_scroll(true);
  else if (!strcasecmp(a3, "PREV"))        ui_lvgl_test_scroll(false);
  else                                   ui_lvgl_show_page_name(a3);   // ten trang (STATUS/MENU/...)
}
