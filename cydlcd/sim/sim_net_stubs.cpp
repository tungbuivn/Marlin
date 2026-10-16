/**
 * sim_net_stubs.cpp -- WiFi / upload gia cho ban GIA LAP tren PC.
 *
 * VI SAO TACH RIENG FILE (bai hoc that 2026-10-16):
 *   `ui_lvgl_int.h` khai bao 8 ham `net_wifi_*` la `__attribute__((weak))` (de nhanh firmware chua
 *   merge van link duoc). Neu file dinh nghia stub CUNG include `ui_lvgl.h`/`ui_lvgl_int.h` thi GCC
 *   gan luon thuoc tinh weak cho DINH NGHIA, va MinGW (PE/COFF) KHONG co "weak undefined = 0" nhu ELF
 *   => linker bao `undefined reference to 'net_wifi_scan'` du stub da duoc viet.
 *   Vi vay: file nay CHI include header goc (net_wifi.h / net_upload_http.h) - khong keo UI vao - nen
 *   cac dinh nghia la STRONG, dung nhu `net_wifi.cpp` / `net_upload_http.cpp` that.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "net_wifi.h"          // 8 ham + scan (extern "C")
#include "net_upload_http.h"   // net_upload_* + net_notify_*

extern "C" {

// ---- WiFi: "khong co module" (giong nhanh STUB cua net_wifi.cpp khi CYDLCD_WIFI_MGMT = 0) ----
uint8_t     net_wifi_state(void)          { return 0; }         // 0 = WiFi tat / khong cau hinh
const char *net_wifi_ssid(void)           { return ""; }
const char *net_wifi_ip(void)             { return ""; }
int8_t      net_wifi_rssi(void)           { return 0; }
bool        net_wifi_configured(void)     { return false; }
const char *net_wifi_saved_ssid(void)     { return ""; }
int         net_wifi_scan(void)           { return 0; }
int         net_wifi_scan_begin(void)     { return 0; }
int         net_wifi_scan_poll(void)      { return 0; }
void        net_wifi_scan_cancel(void)    { }
bool        net_wifi_scanning(void)       { return false; }
int         net_wifi_scan_max(void)       { return 0; }
const char *net_wifi_scan_ssid(int)       { return ""; }
int8_t      net_wifi_scan_rssi(int)       { return 0; }
uint32_t    net_wifi_scan_ms(void)        { return 0; }
bool        net_wifi_connect(const char *, const char *, bool) { return false; }
void        net_wifi_disconnect(bool)     { }
void        net_wifi_begin(void)          { }
void        net_wifi_task(void)           { }
bool        net_wifi_up(void)             { return false; }
void        net_wifi_down(void)           { }
void        net_wifi_poll(void)           { }
bool        net_wifi_connected(void)      { return false; }
bool        net_wifi_ram_ok(void)         { return true; }
void        net_wifi_ps(bool)             { }

// ---- Upload qua web server: khong co (giong nhanh stub cua net_upload_http.cpp) ----
void     net_upload_http_begin(void)       { }
void     net_upload_http_task(void)        { }
void     net_upload_http_log(void)         { }
void     net_upload_http_perf_report(void) { }
bool     net_upload_http_active(void)      { return false; }
bool     net_upload_active(void)           { return false; }
uint32_t net_upload_last_activity_ms(void) { return 0; }
bool     net_sd_delete_file(const char *)  { return false; }
int      net_sd_dir_count(const char *)    { return -1; }
bool     net_sd_delete_dir(const char *, bool *not_empty) { if (not_empty) *not_empty = false; return false; }
bool     net_upload_mdns_pause(bool)       { return false; }

// Thong bao di TRANG /status (khong hien hop tren LCD - xem CYDLCD_MSG_BOX_ON_LCD): ban gia lap chi in ra.
void net_notify_set(const char *msg, uint8_t sev, uint32_t) {
  printf("[SIM] (/status) thong bao muc %u: %s\n", (unsigned)sev, msg ? msg : "");
}
void net_notify_clear(void) { }
void net_notify_dump(void)  { }

}  // extern "C"
