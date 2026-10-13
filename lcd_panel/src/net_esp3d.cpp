/**
 * net_esp3d.cpp -- the TF + POST len ESP3D + M22/M21 cho duong upload qua WiFi (README 9.21).
 *
 * Dung chung net_wifi.cpp de quan ly WiFi (khong tu goi WiFi.begin nua). Hai che do mang:
 *   PANEL_ESP3D_VIA_AP = 0 (mac dinh): panel o mang nha (STA) va POST toi IP LAN cua ESP3D
 *       (ESP3D phai chay AP+STA). IP lay tu PANEL_ESP3D_LAN_HOST, neu rong thi tu do.
 *   PANEL_ESP3D_VIA_AP = 1: panel TAM doi sang AP cua ESP3D (Voron2308) de POST roi quay ve.
 */
#include "panel_config.h"

// Can cho CA nhanh stub (khi PANEL_WIFI_UPLOAD = 0: khong include Arduino.h/WiFi.h gi ca).
// Include header truoc `#if` de stub co DUNG linkage C nhu khai bao (neu khong, ten bi mang hoa
// -> main.cpp/ net_upload_http.cpp goi khong thay).
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "net_esp3d.h"

#if PANEL_WIFI_UPLOAD

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#include "net_esp3d.h"
#include "net_wifi.h"
#include "panel_link.h"
#include "ui_lvgl.h"          // UI_LCD_HOR_RES / UI_LCD_VER_RES

#if defined(PANEL_CYD_DEBUG)
  #define ELOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define ELOG(...)   do { } while (0)
#endif

namespace {

// Mau lop phu: lay DUNG gia tri cua ui_lvgl.cpp (MR_BG/MR_PROGBG/MR_PROGBAR/MR_FRAME/MR_RATE100)
constexpr uint32_t OV_BG = 0x1E156E, OV_BAR = 0x2A2080, OV_FILL = 0x00E5FF,
                   OV_TXT = 0xE6E1E5, OV_OK = 0x81C784, OV_ERR = 0xFF8A80;

// ---- the TF ----
bool     s_sd_ready = false;
bool     s_sd_was   = false;
uint32_t s_sd_check_ms = 0, s_sd_retry_ms = 0;
bool     s_sd_retry_req = false;
File     s_file;
char     s_tmp[64] = "";
uint32_t s_got = 0;                    // byte da ghi vao file tam
char     s_err[64] = "";               // ly do loi cua phien dang chay
uint32_t s_heap_min = 0xFFFFFFFFu;
uint32_t s_posted = 0;                 // byte da day len ESP3D (phien gan nhat)
char     s_result[48] = "";            // "DONE 1234" / "ERR ..."

// ---- lop phu (pha 2: toan man hinh + khoa UI) ----
lv_obj_t *ov = nullptr, *ov_state = nullptr, *ov_info = nullptr, *ov_bar = nullptr;
char      txt_state[48], txt_info[48];
uint32_t  ov_ms = 0, rate_ms = 0, rate_base = 0, rate_x10 = 0;
bool      s_busy = false;              // dang day len ESP3D (de /status bao "posting")

// ===========================================================================
// Tien ich nho
// ===========================================================================
void heap_min_track(void) {
  const uint32_t h = ESP.getFreeHeap();
  if (h < s_heap_min) s_heap_min = h;
}

void ip_str(char *dst, const size_t n, const IPAddress &ip) {
  snprintf(dst, n, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

// Marlin dang in? (khong cho nhan file khi may dang in - ESP3D se gianh bus SPI cua the SD)
bool printer_busy(void) {
  if (panel.flags & PANEL_SF_SD_PRINTING) return true;
  switch (panel.state) {
    case PANEL_ST_PRINTING:
    case PANEL_ST_PAUSED:
    case PANEL_ST_HOMING:
    case PANEL_ST_LEVELING:
    case PANEL_ST_RUNOUT:
    case PANEL_ST_KILLED:
      return true;
    default:
      return false;
  }
}

// Ten file an toan (bo duong dan, ky tu la, gioi han do dai + duoi .gco)
void sanitize_name(const char *in, char *out, const size_t cap) {
  size_t o = 0;
  const char *slash = strrchr(in ? in : "", '/');
  const char *src = slash ? slash + 1 : (in ? in : "");
  for (size_t i = 0; src[i] != '\0' && o + 1u < cap; i++) {
    const char c = src[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
        c == '.' || c == '_' || c == '-')
      out[o++] = c;
    else
      out[o++] = '_';
  }
  out[o] = '\0';
  if (o == 0) snprintf(out, cap, "upload.gco");
  // Chi THEM ".gco" khi ten CHUA co duoi nao. Truoc day ham chi so sanh 4 ky tu cuoi voi ".gco"
  // nen "V300_Part3_proj.gcode" (duoi .gcode hop le, Marlin in duoc) bi thanh
  // "...gcode.gco" (do that 2026-10-14: file tren the SD bi sai ten).
  if (strrchr(out, '.') == nullptr) strncat(out, ".gco", cap - strlen(out) - 1u);
}

// ===========================================================================
// Lop phu LVGL (lv_layer_top) - toan man hinh + khoa moi input device
// ===========================================================================
void ui_lock(const bool lock) {
  lv_indev_t *i = lv_indev_get_next(nullptr);
  while (i) {
    lv_indev_enable(i, !lock);
    i = lv_indev_get_next(i);
  }
  ELOG("[UP] UI %s\n", lock ? "KHOA" : "MO KHOA");
}

lv_obj_t *ov_label(lv_obj_t *parent, const char *txt, const uint32_t col, const lv_font_t *font) {
  lv_obj_t *l = lv_label_create(parent);
  if (!l) return nullptr;
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  if (font) lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

void ov_layout_full(void) {
  if (!ov) return;
  const lv_coord_t W = UI_LCD_HOR_RES, H = UI_LCD_VER_RES;
  lv_obj_set_pos(ov, 0, 0);
  lv_obj_set_size(ov, W, H);
  lv_obj_set_style_radius(ov, 0, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_90, 0);
  if (ov_state) {
    lv_obj_set_width(ov_state, (lv_coord_t)(W - 16));
    lv_obj_align(ov_state, LV_ALIGN_TOP_MID, 0, 54);      // chu TO (montserrat 20)
  }
  if (ov_info) {
    lv_obj_set_width(ov_info, (lv_coord_t)(W - 16));
    lv_obj_align(ov_info, LV_ALIGN_TOP_MID, 0, 96);       // "% KB KB/s"
  }
  if (ov_bar) {
    lv_obj_set_size(ov_bar, (lv_coord_t)(W - 32), 16);
    lv_obj_align(ov_bar, LV_ALIGN_TOP_MID, 0, 128);
  }
  lv_obj_invalidate(ov);
}

bool ov_create_full(void) {
  lv_obj_t *top = lv_layer_top();
  if (!top) return false;
  if (ov) { ov_layout_full(); return true; }
  ov = lv_obj_create(top);
  if (!ov) { ELOG("[UP] khong tao duoc lop phu (het LVGL mem)\n"); return false; }
  lv_obj_set_style_radius(ov, 0, 0);
  lv_obj_set_style_border_width(ov, 0, 0);
  lv_obj_set_style_bg_color(ov, lv_color_hex(OV_BG), 0);
  lv_obj_set_style_bg_opa(ov, LV_OPA_90, 0);
  lv_obj_set_style_pad_all(ov, 0, 0);
  lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(ov, LV_OBJ_FLAG_CLICKABLE);        // input bi chan bang ui_lock(), khong can click
  lv_obj_t *title = ov_label(ov, "TBT-PANEL", OV_FILL, nullptr);
  if (title) {
    lv_obj_set_width(title, (lv_coord_t)(UI_LCD_HOR_RES - 16));
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 18);
  }
  ov_state = ov_label(ov, "...", OV_TXT, &lv_font_montserrat_20);
  ov_info  = ov_label(ov, "", OV_TXT, nullptr);
  ov_bar = lv_bar_create(ov);
  if (ov_bar) {
    lv_bar_set_range(ov_bar, 0, 100);
    lv_obj_set_style_bg_color(ov_bar, lv_color_hex(OV_BAR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(ov_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(ov_bar, lv_color_hex(OV_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(ov_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(ov_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(ov_bar, 2, LV_PART_INDICATOR);
  }
  ov_layout_full();
  return true;
}

void ov_destroy(void) {
  if (ov) lv_obj_del(ov);
  ov = nullptr;
  ov_state = ov_info = ov_bar = nullptr;
  txt_state[0] = txt_info[0] = '\0';
}

// Dung buffer tinh + text_static -> khong ton them LVGL heap; phai tu invalidate.
void ov_set(const char *state, const char *info, const bool ok_col) {
  if (!ov) return;
  if (state && ov_state) {
    snprintf(txt_state, sizeof(txt_state), "%s", state);
    lv_obj_set_style_text_color(ov_state, lv_color_hex(ok_col ? OV_OK : OV_ERR), 0);
    lv_label_set_text_static(ov_state, txt_state);
    lv_obj_invalidate(ov_state);
  }
  if (info && ov_info) {
    snprintf(txt_info, sizeof(txt_info), "%s", info);
    lv_label_set_text_static(ov_info, txt_info);
    lv_obj_invalidate(ov_info);
  }
}

void rate_tick(const uint32_t done) {
  const uint32_t now = millis();
  if (rate_ms == 0) { rate_ms = now; rate_base = done; return; }
  const uint32_t dt = now - rate_ms;
  if (dt < 1000u) return;
  rate_x10 = (uint32_t)(((uint64_t)(done - rate_base) * 10000ull) / ((uint64_t)dt * 1024ull));
  rate_ms = now;
  rate_base = done;
}

// Cap nhat tien do (toi da 5 lan/s cho mat nguoi, khong nong CPU)
void ov_progress(const char *state, const uint32_t done, const uint32_t total) {
  if (millis() - ov_ms < 200u) return;
  ov_ms = millis();
  char info[48];
  snprintf(info, sizeof(info), "%u%%   %u/%u KB   %u.%u KB/s",
           (unsigned)(total ? ((uint64_t)done * 100u / total) : 0u),
           (unsigned)(done / 1024u), (unsigned)(total / 1024u),
           (unsigned)(rate_x10 / 10u), (unsigned)(rate_x10 % 10u));
  ov_set(state, info, true);
  if (ov_bar) lv_bar_set_value(ov_bar, total ? (int32_t)((uint64_t)done * 100u / total) : 0, LV_ANIM_OFF);
}

// Thong bao ket qua trong cung lop phu (giu toan man hinh cho don gian - chi doi chu)
void ov_note(const char *state, const bool ok) {
  if (!ov) return;
  ov_set(state, "", ok);
}

// ===========================================================================
// The TF (VSPI rieng: CS 5 / MOSI 23 / MISO 19 / SCLK 18)
// ===========================================================================
#if PANEL_SD_SHARED
// Khai bao truoc (dinh nghia day du o duoi, sau sd_lost) - sd_mount() can sd_mux_panel().
void sd_mux_panel(void);
void sd_mux_marlin(void);
void sd_release_to_marlin(bool reinit_marlin);
#endif
bool sd_mount(const bool verbose) {
  if (s_sd_ready) return true;
#if PANEL_SD_SHARED
  sd_mux_panel();                       // gianh bus SPI the ve phia ESP32 TRUOC khi mount
#endif
  SPI.begin(PANEL_SD_SCK, PANEL_SD_MISO, PANEL_SD_MOSI, PANEL_SD_CS);
  const uint32_t hz_try[3] = { PANEL_SD_HZ, PANEL_SD_HZ / 2u, 4000000u };
  for (uint8_t i = 0; i < 3u; i++) {
    if (verbose) ELOG("[UP] thu mount the TF @ %u Hz ...\n", (unsigned)hz_try[i]);
    if (SD.begin(PANEL_SD_CS, SPI, hz_try[i], "/sd", 4)) {
      s_sd_ready = true;
      ELOG("[UP] %s @ %u Hz: cardType=%d, tong %lu KB, da dung %lu KB\n",
           s_sd_was ? "da mount LAI the TF" : "the TF OK", (unsigned)hz_try[i], (int)SD.cardType(),
           (unsigned long)(SD.totalBytes() / 1024ull), (unsigned long)(SD.usedBytes() / 1024ull));
      s_sd_was = true;
      return true;
    }
    delay(30);
  }
  if (verbose)
    ELOG("[UP] SD.begin(CS=%d) THAT BAI o ca 3 muc toc do. Kiem tra: da cam the TF chua, "
         "the phai FAT32, khe TF/bus VSPI 18/19/23/5.\n", (int)PANEL_SD_CS);
  return false;
}

bool sd_alive(void) {
  if (!s_sd_ready) return false;
  return SD.exists("/");
}

void sd_lost(const char *why) {
  if (!s_sd_ready) return;
  if (s_file) s_file.close();
  s_sd_ready = false;
  SD.end();
  ELOG("[UP] MAT THE TF: %s\n", why);
  s_sd_retry_ms = millis();
}

// ===========================================================================
// Bao Marlin "the SD vua thay doi" (panel vua ghi/xoa file) -> M22 (nha the) + M21 (mount lai) +
// xin lai danh sach file. Dung cho CA HAI che do:
//   - MUX (PANEL_SD_SHARED): sau khi tra bus SPI ve Marlin;
//   - UART (PANEL_SD_UART): sau khi panel ghi xong file (Marlin doc sector qua UART, cache FAT
//     cua Marlin phai duoc lam moi).
// ===========================================================================
void notify_media_changed(void) {
  ELOG("[UP] bao Marlin mount lai the (M22/M21) + xin lai danh sach file\n");
  panel_link_send_gcode2("M22", "M21");
  const uint32_t t0 = millis();
  while (millis() - t0 < PANEL_HTTP_SDWAIT_MS) {
    lv_timer_handler();
    panel_link_task();
    delay(10);
  }
  panel.files_done = false;
  panel.n_files = 0;
  panel_link_request((uint8_t)PANEL_REQ_FILES);
}

#if PANEL_SD_SHARED
// ===========================================================================
// MUX SPI (74HC4053): the SD nam o khe CYD, HAI master thay nhau (panel <-> Marlin)
//   Xem panel_config.h (khoi PANEL_SD_SHARED) de biet so do chan. Nguyen tac BAT BUOC:
//   - Muc MAC DINH khi panel khong dung = MARLIN (co dien tro 10k keo chan chon xuong GND).
//   - Panel chi doi sang minh NGAY TRUOC khi mount va tra lai NGAY sau khi ghi xong.
//   - KHONG BAO GIO gianh bus khi Marlin dang in / dang tam dung (xem printer_busy()).
// ===========================================================================
void sd_mux_panel(void) {
  digitalWrite(PANEL_SD_MUX_PIN, PANEL_SD_MUX_PANEL_LEVEL);
  delay(PANEL_SD_MUX_SETTLE_MS);          // cho 4053 + the on dinh truoc khi clock
}

void sd_mux_marlin(void) {
  digitalWrite(PANEL_SD_MUX_PIN, PANEL_SD_MUX_MARLIN_LEVEL);
}

// Tra bus SPI ve Marlin. `reinit_marlin` = gui M22+M21 de Marlin mount lai the (bat buoc sau khi
// panel VUA GHI vao the: FAT cua Marlin dang cache trang thai cu) roi xin lai danh sach file.
void sd_release_to_marlin(const bool reinit_marlin) {
  if (s_file) s_file.close();
  if (s_sd_ready) {
    SD.end();                             // flush FATFS + tra chan SPI ve trang thai input
    s_sd_ready = false;
    s_sd_was = false;
  }
  sd_mux_marlin();
  if (!reinit_marlin) {
    ELOG("[UP] tra bus SPI the cho MARLIN (khong doi FAT)\n");
    return;
  }
  notify_media_changed();
}
#endif  // PANEL_SD_SHARED

// Xoa moi file *.tmp trong /btup (don file mo coi sau reset / mat dien / loi)
uint16_t clean_dir(const char *why, const bool verbose) {
  if (!s_sd_ready) return 0;
  if (!SD.exists(PANEL_BTUP_DIR)) {
    SD.mkdir(PANEL_BTUP_DIR);
    return 0;
  }
  File dir = SD.open(PANEL_BTUP_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return 0;
  }
  uint16_t n = 0;
  uint64_t freed = 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const bool   isdir = f.isDirectory();
    const size_t sz    = f.size();
    const String nm    = f.name();          // ESP32 core: ten DAY DU dang "/btup/x.tmp"
    char path[64];
    if (nm.length() && nm[0] == '/') snprintf(path, sizeof(path), "%s", nm.c_str());
    else                             snprintf(path, sizeof(path), "%s/%s", PANEL_BTUP_DIR, nm.c_str());
    f.close();                              // dong TRUOC khi xoa (an toan tren FATFS)
    const size_t ln = strlen(path);
    if (!isdir && ln > 4u && strcmp(path + ln - 4u, ".tmp") == 0) {
      if (SD.remove(path)) { n++; freed += sz; }
      else ELOG("[UP] CANH BAO: khong xoa duoc file tam %s\n", path);
    }
  }
  dir.close();
  if (n || verbose)
    ELOG("[UP] don %s (%s): xoa %u file .tmp (%lu KB)\n", PANEL_BTUP_DIR, why, (unsigned)n,
         (unsigned long)(freed / 1024ull));
  return n;
}

// ---- Ten file dich (da lam sach) cua phien dang chay ----
char s_name[40] = "";

// ===========================================================================
// WiFi: bat + cho ket noi (co nuoi UI/UART, khong chan cung)
// ===========================================================================
bool wifi_wait_connected(const uint32_t timeout_ms) {
  const uint32_t t0 = millis();
  while (!net_wifi_connected()) {
    if (millis() - t0 > timeout_ms) return false;
    net_wifi_poll();
    lv_timer_handler();
    panel_link_task();
    delay(10);
  }
  return true;
}

#if PANEL_ESP3D_VIA_AP
// Che do doi mang: TAM sang AP cua ESP3D de POST (ESP3D chi phat AP, khong o mang nha)
bool wifi_enter_esp3d(void) {
  ELOG("[UP] che do PANEL_ESP3D_VIA_AP=1 -> tam doi sang AP \"%s\"\n", PANEL_ESP3D_AP_SSID);
  if (!net_wifi_join_temp(PANEL_ESP3D_AP_SSID, PANEL_ESP3D_AP_PASS)) return false;
  return wifi_wait_connected(PANEL_HTTP_AP_WAIT_MS);
}
void wifi_leave_esp3d(void) { net_wifi_leave_temp(); }
#else
bool wifi_enter_esp3d(void) {
  if (!net_wifi_up()) return false;                       // thieu RAM / chua cau hinh
  return wifi_wait_connected(PANEL_HTTP_WIFI_WAIT_MS);    // cho ket noi mang nha (STA)
}
void wifi_leave_esp3d(void) {}                            // giu nguyen WiFi (panel la web server)
#endif

// ===========================================================================
// Do IP module ESP3D + POST multipart (giong hop dong cua duong BT)
// ===========================================================================
bool probe_esp3d(const IPAddress &ip) {
  WiFiClient c;
  c.setTimeout(PANEL_HTTP_PROBE_MS);
  if (!c.connect(ip, PANEL_ESP3D_PORT, PANEL_HTTP_PROBE_MS)) return false;
  char req[112];
  const int rn = snprintf(req, sizeof(req),
      "GET /command?plain=%%5BESP800%%5D HTTP/1.0\r\nHost: %u.%u.%u.%u\r\nConnection: close\r\n\r\n",
      ip[0], ip[1], ip[2], ip[3]);
  if (rn <= 0 || (size_t)rn >= sizeof(req)) { c.stop(); return false; }
  c.write(reinterpret_cast<const uint8_t *>(req), (size_t)rn);
  static const char MARK[] = "FW version:";
  const size_t mark_len = sizeof(MARK) - 1u;
  char win[24];
  size_t wl = 0;
  bool found = false;
  const uint32_t t0 = millis();
  while (millis() - t0 < PANEL_HTTP_PROBE_MS) {
    while (c.available() > 0) {
      const int ch = c.read();
      if (ch < 0) break;
      if (wl < sizeof(win)) win[wl++] = (char)ch;
      else { memmove(win, win + 1, sizeof(win) - 1u); win[sizeof(win) - 1u] = (char)ch; }
      if (wl >= mark_len && memcmp(win + (wl - mark_len), MARK, mark_len) == 0) { found = true; break; }
    }
    if (found) break;
    if (!c.connected() && c.available() == 0) break;
    delay(5);
  }
  c.stop();
  char t[16];
  ip_str(t, sizeof(t), ip);
  ELOG("[UP] thu ESP3D %s: %s\n", t, found ? "DUNG (co FW version:)" : "khong phai");
  return found;
}

// IP ESP3D: [PANEL_ESP3D_LAN_HOST] -> [gateway] -> [PANEL_ESP3D_IP_LIST]
bool esp3d_find(IPAddress *out) {
#if PANEL_ESP3D_VIA_AP
  const char *host = PANEL_ESP3D_HOST;          // o che do AP: IP tinh cua module
#else
  const char *host = (PANEL_ESP3D_LAN_HOST[0] != '\0') ? PANEL_ESP3D_LAN_HOST : PANEL_ESP3D_HOST;
#endif
  if (host[0]) {
    IPAddress ip;
    if (ip.fromString(host) && probe_esp3d(ip)) { *out = ip; return true; }
  }
  const IPAddress gw = WiFi.gatewayIP();
  if ((uint32_t)gw != 0u && probe_esp3d(gw)) { *out = gw; return true; }
  char list[112];
  snprintf(list, sizeof(list), "%s", PANEL_ESP3D_IP_LIST);
  char *save = nullptr;
  for (char *t = strtok_r(list, ",", &save); t; t = strtok_r(nullptr, ",", &save)) {
    while (*t == ' ') t++;
    IPAddress ip;
    if (ip.fromString(t) && probe_esp3d(ip)) { *out = ip; return true; }
  }
  return false;
}

bool write_all(WiFiClient &sock, const char *src, const size_t len) {
  size_t sent = 0;
  const uint32_t t0 = millis();
  while (sent < len) {
    const size_t w = sock.write(reinterpret_cast<const uint8_t *>(src) + sent, len - sent);
    if (w == 0) {
      if (millis() - t0 > PANEL_HTTP_HTTP_TIMEOUT_MS) return false;
      delay(2);
      continue;
    }
    sent += w;
  }
  return true;
}

// POST multipart/form-data voi Content-Length (file da nam tren the TF -> biet truoc do dai).
// LUU Y (da kiem lai theo canh bao cua nhanh script): gia tri trong Content-Disposition BAT BUOC
// co dau " bao quanh (`name="file"` / `filename="<ten>"`), neu thieu thi ESP3D/WebServer cat chuoi
// kieu substring(idx+2, len-1) se LAM MAT ky tu dau/cuoi cua ten file.
bool http_post_file(const IPAddress &ip, const uint32_t size) {
  File f = SD.open(s_tmp, FILE_READ);
  if (!f) { snprintf(s_err, sizeof(s_err), "khong mo duoc file tam"); return false; }
  char boundary[40];
  snprintf(boundary, sizeof(boundary), "TBTpanel%08x", (unsigned)esp_random());
  char pro[256], epi[64];
  const int pro_n = snprintf(pro, sizeof(pro),
      "--%s\r\nContent-Disposition: form-data; name=\"name\"\r\n\r\n%s\r\n"
      "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
      "Content-Type: application/octet-stream\r\n\r\n",
      boundary, s_name, boundary, s_name);
  const int epi_n = snprintf(epi, sizeof(epi), "\r\n--%s--\r\n", boundary);
  if (pro_n <= 0 || (size_t)pro_n >= sizeof(pro) || epi_n <= 0 || (size_t)epi_n >= sizeof(epi)) {
    f.close();
    snprintf(s_err, sizeof(s_err), "ten file qua dai");
    return false;
  }
  const uint32_t total = (uint32_t)pro_n + size + (uint32_t)epi_n;

  ov_progress("KET NOI ESP3D...", 0, size);
  WiFiClient sock;
  sock.setTimeout(PANEL_HTTP_HTTP_TIMEOUT_MS);
  if (!sock.connect(ip, PANEL_ESP3D_PORT, PANEL_HTTP_HTTP_TIMEOUT_MS)) {
    f.close();
    snprintf(s_err, sizeof(s_err), "khong ket noi duoc ESP3D");
    return false;
  }
  char head[256], ipbuf[16];
  ip_str(ipbuf, sizeof(ipbuf), ip);
  const int hn = snprintf(head, sizeof(head),
      "POST %s HTTP/1.1\r\nHost: %s\r\nContent-Type: multipart/form-data; boundary=%s\r\n"
      "Content-Length: %lu\r\nConnection: close\r\n\r\n",
      PANEL_ESP3D_PATH, ipbuf, boundary, (unsigned long)total);
  if (hn <= 0 || (size_t)hn >= sizeof(head) || !write_all(sock, head, (size_t)hn) ||
      !write_all(sock, pro, (size_t)pro_n)) {
    sock.stop(); f.close();
    snprintf(s_err, sizeof(s_err), "gui header that bai");
    return false;
  }
  ov_progress("DANG DAY LEN ESP3D...", 0, size);

  // Than file: doc tung khoi 4 KB tu the TF roi day len (vua day vua cap nhat tien do)
  static uint8_t buf[4096];
  uint32_t sent = 0;
  const uint32_t t0 = millis();
  bool ok = true;
  while (sent < size) {
    const size_t want = (size - sent > sizeof(buf)) ? sizeof(buf) : (size - sent);
    const size_t rd = f.read(buf, want);
    if (rd == 0) { snprintf(s_err, sizeof(s_err), "doc the TF loi"); ok = false; break; }
    if (!write_all(sock, reinterpret_cast<const char *>(buf), rd)) {
      snprintf(s_err, sizeof(s_err), "gui du lieu that bai");
      ok = false;
      break;
    }
    sent += (uint32_t)rd;
    s_posted = sent;
    rate_tick(sent);
    ov_progress("DANG DAY LEN ESP3D...", sent, size);
    heap_min_track();
    if (millis() - t0 > PANEL_HTTP_POST_TIMEOUT_MS) {
      snprintf(s_err, sizeof(s_err), "qua thoi gian POST");
      ok = false;
      break;
    }
  }
  if (ok && !write_all(sock, epi, (size_t)epi_n)) {
    snprintf(s_err, sizeof(s_err), "gui ket thuc that bai");
    ok = false;
  }
  f.close();

  // Doc phan hoi: "HTTP/1.1 200 OK" + "{\"status\":\"Ok\"...}"
  uint16_t code = 0;
  char status[24] = "";
  char body[160] = "";
  if (ok) {
    const uint32_t tr = millis();
    size_t bl = 0;
    bool head_done = false;
    while (millis() - tr < PANEL_HTTP_HTTP_TIMEOUT_MS) {
      const int avail = sock.available();
      if (avail > 0) {
        const int ch = sock.read();
        if (ch < 0) continue;
        if (!head_done) {
          static char line[128];
          static size_t ll = 0;
          if (ch == '\n') {
            line[ll] = '\0';
            if (ll && line[ll - 1] == '\r') line[ll - 1] = '\0';
            if (code == 0 && strncmp(line, "HTTP/", 5) == 0) {
              const char *sp = strchr(line, ' ');
              if (sp) code = (uint16_t)atoi(sp + 1);
              snprintf(status, sizeof(status), "%s", sp ? sp + 1 : "");
            }
            if (ll == 0) head_done = true;         // dong trong -> het header
            ll = 0;
          } else if (ll + 1u < sizeof(line)) {
            line[ll++] = (char)ch;
          }
        } else if (bl + 1u < sizeof(body)) {
          body[bl++] = (char)ch;
          body[bl] = '\0';
        }
      } else if (!sock.connected()) {
        break;
      } else {
        delay(5);
      }
    }
  }
  sock.stop();
  if (ok) {
    // ESP3D-direct-sd tra {"status":"Ok",...} (webinterface.cpp); chap nhan ca khi chi co 200.
    ok = (code == 200) && (strstr(body, "\"Ok\"") != nullptr || strstr(body, "Ok") != nullptr);
    if (!ok) snprintf(s_err, sizeof(s_err), "ESP3D tu choi (%u)", (unsigned)code);
    ELOG("[UP] ESP3D tra %u %s | body: %s\n", (unsigned)code, status, body);
  }
  if (!ok) s_posted = 0;
  return ok;
}

}  // namespace

// ===========================================================================
// API: the TF + ket qua phien
// ===========================================================================
bool net_esp3d_sd_ready(void) {
#if PANEL_SD_SHARED
  // THE DUNG CHUNG: panel khong mount the khi ranh (bus thuoc Marlin) -> "the co san" nghia la
  // MARLIN bao co the (co PANEL_SF_MEDIA trong frame PUSH). Dung cho ca icon STATUS lan /status.
  return (panel.flags & PANEL_SF_MEDIA) != 0;
#else
  return s_sd_ready;
#endif
}

void net_esp3d_sd_retry(void) {
  s_sd_retry_req = true;
  s_sd_retry_ms = millis() - PANEL_SD_RETRY_MS;    // cho phep chay ngay o vong lap ke tiep
}

bool net_esp3d_sd_mount_now(void) { return sd_mount(true); }

bool net_esp3d_printer_busy(void) { return printer_busy(); }

// ===========================================================================
// API: DOC/GHI SECTOR THO + XOA FILE (che do PANEL_SD_UART = "may chu the SD" cho Marlin)
//   Marlin khong tu doc SPI nua: moi lan SdVolume can 1 sector 512 B, panel_link.cpp goi vao day
//   roi tra ket qua qua frame PANEL_F_SDDATA.
// ===========================================================================
bool net_esp3d_raw_read(const uint32_t sector, uint8_t *dst) {
#if PANEL_SD_UART
  if (!s_sd_ready || !dst) return false;
  // SD.readRAW(): doc THANG sector vat ly, khong qua FATFS (core ESP32 SD.cpp:124 -> sd_read_raw).
  // Doc lap voi FATFS dang mount: chi doc, khong dung cache cua FATFS.
  const bool ok = SD.readRAW(dst, sector);
  if (!ok) ELOG("[UP] doc RAW sector %lu THAT BAI (the rut ra?)\n", (unsigned long)sector);
  return ok;
#else
  (void)sector; (void)dst;
  return false;
#endif
}

bool net_esp3d_raw_write(const uint32_t sector, const uint8_t *src) {
  // GIAI DOAN 1: chi doc (Marlin nap voi SDCARD_READONLY). Duong ghi lam o P3.
  (void)sector; (void)src;
  return false;
}

bool net_esp3d_delete(const char *name) {
#if PANEL_SD_UART
  if (!s_sd_ready || !name || !name[0]) return false;
  // Dang in thi KHONG xoa (Marlin dang doc the qua UART; xoa file khac thi duoc nhung de an toan
  // va don gian, tu choi luon).
  if (printer_busy()) { ELOG("[UP] TU CHOI xoa file: Marlin dang in\n"); return false; }
  char path[80];
  snprintf(path, sizeof(path), "/%s", name);
  if (!SD.exists(path)) { ELOG("[UP] khong thay file de xoa: %s\n", path); return false; }
  const bool ok = SD.remove(path);
  ELOG("[UP] %s file %s (panel tu xoa - che do the qua UART)\n",
       ok ? "da xoa" : "KHONG xoa duoc", path);
  // LUU Y: KHONG goi notify_media_changed() o day - ham do cho ~800 ms (M22/M21 + xin danh sach),
  // goi tu callback nut bam se treo LVGL. Nguoi goi (ui_lvgl.cpp) tu lo phan do bang files_init_sd().
  return ok;
#else
  (void)name;
  return false;
#endif
}

const char *net_esp3d_last_result(void) { return s_result; }
uint32_t    net_esp3d_last_posted(void) { return s_posted; }
const char *net_esp3d_temp_path(void) { return s_tmp; }
uint32_t    net_esp3d_temp_bytes(void) { return s_got; }

// ===========================================================================
// API: ghi file tam
// ===========================================================================
bool net_esp3d_temp_open(const char *name, uint32_t size_hint) {
  s_err[0] = '\0';
  s_result[0] = '\0';
  // Chua in moi duoc ghi the (the dung chung voi Marlin) - lop chan thu 2 sau net_upload_http.
  if (printer_busy()) {
    snprintf(s_err, sizeof(s_err), "Marlin dang in - khong dung the");
    return false;
  }
#if PANEL_SD_SHARED
  // CHUA BIET trang thai Marlin (mat lien lac UART) => KHONG duoc gianh the: co the Marlin dang in
  // ma panel khong biet, gianh bus SPI the giua luc in se lam hong ban in.
  if (!panel.linked) {
    snprintf(s_err, sizeof(s_err), "chua lien lac duoc Marlin - khong gianh the");
    ELOG("[UP] TU CHOI: %s\n", s_err);
    return false;
  }
  // Neu Marlin dang liet ke danh sach file (do nguoi dung vua bam REFRESH) thi doi xong da (~<400 ms)
  // roi moi gianh bus, tranh cat ngang luc Marlin dang doc the.
  const uint32_t tw = millis();
  while (!panel.files_done && (millis() - tw) < 400u) {
    lv_timer_handler();
    panel_link_task();
    delay(5);
  }
#endif
#if PANEL_SD_UART
  // Che do the qua UART: panel va Marlin KHONG tranh bus, nhung van can lien lac de con bao
  // M22/M21 sau khi ghi xong -> mat lien lac thi tu choi luon (tranh ghi file ma Marlin khong biet).
  if (!panel.linked) {
    snprintf(s_err, sizeof(s_err), "chua lien lac duoc Marlin");
    ELOG("[UP] TU CHOI: %s\n", s_err);
    return false;
  }
#endif
  if (!sd_mount(true)) { snprintf(s_err, sizeof(s_err), "the TF panel loi"); return false; }
#if PANEL_SD_SHARED || PANEL_SD_UART
  // Don file mo coi ngay khi VUA gianh duoc bus (MUX) / ngay truoc phien moi (UART).
  if (!s_file) clean_dir("truoc phien moi", true);
#endif
  // Don file mo coi TRUOC khi mo file moi (chi 1 lan/lan mo - re)
  if (s_file) s_file.close();
  const uint64_t total = SD.totalBytes(), used = SD.usedBytes();
  const uint64_t freeb = (total > used) ? (total - used) : 0;
  ELOG("[UP] the TF: con %lu KB / tong %lu KB (file moi: %lu B)\n",
       (unsigned long)(freeb / 1024ull), (unsigned long)(total / 1024ull),
       (unsigned long)size_hint);
  if (size_hint && ((uint64_t)size_hint + (256ull * 1024ull) > freeb)) {
    snprintf(s_err, sizeof(s_err), "the TF panel het cho (%lu KB trong)",
             (unsigned long)(freeb / 1024ull));
    return false;
  }
  sanitize_name(name, s_name, sizeof(s_name));
  if (!SD.exists(PANEL_BTUP_DIR)) SD.mkdir(PANEL_BTUP_DIR);
  snprintf(s_tmp, sizeof(s_tmp), "%s/%s.tmp", PANEL_BTUP_DIR, s_name);
  if (SD.exists(s_tmp)) SD.remove(s_tmp);
  s_file = SD.open(s_tmp, FILE_WRITE);
  if (!s_file) {
    snprintf(s_err, sizeof(s_err), "khong tao duoc file tam");
    s_tmp[0] = '\0';
    return false;
  }
  s_got = 0;
  ELOG("[UP] ghi vao %s (ten dich: %s)\n", s_tmp, s_name);
  return true;
}

size_t net_esp3d_temp_write(const uint8_t *d, const size_t n) {
  if (!s_file || !d || n == 0) return 0;
  const size_t w = s_file.write(d, n);
  if (w != n) {
    snprintf(s_err, sizeof(s_err), "ghi the TF loi");
    if (s_file) s_file.close();
    return w;
  }
  s_got += (uint32_t)w;
  heap_min_track();
  return w;
}

uint32_t net_esp3d_temp_close(void) {
  if (s_file) {
    s_file.flush();
    s_file.close();
  }
  return s_got;
}

void net_esp3d_temp_abort(void) {
  if (s_file) s_file.close();
  if (s_tmp[0] && s_sd_ready) {
    if (SD.exists(s_tmp)) SD.remove(s_tmp);
    ELOG("[UP] da xoa file tam %s (bo phien)\n", s_tmp);
  }
  s_tmp[0] = '\0';
  s_got = 0;
#if PANEL_SD_SHARED
  // Phien bi bo -> PHAI tra bus SPI the cho Marlin (khong thi Marlin mat the vinh vien).
  sd_release_to_marlin(false);
#endif
}

// ===========================================================================
// API: day file tam len ESP3D (CHAN - co lop phu + khoa UI)
// ===========================================================================
bool net_esp3d_push(const char *name, uint32_t size, uint32_t *posted, char *err, const size_t errn) {
  if (posted) *posted = 0;
  if (err && errn) err[0] = '\0';
  s_busy = true;
  s_posted = 0;
  s_err[0] = '\0';
  if (name && name[0]) snprintf(s_name, sizeof(s_name), "%s", name);
  if (s_tmp[0] == '\0') snprintf(s_tmp, sizeof(s_tmp), "%s/%s.tmp", PANEL_BTUP_DIR, s_name);

#if PANEL_SD_SHARED || PANEL_SD_UART
  // -------------------------------------------------------------------------
  // CHE DO "THE CUA PANEL" (KHONG mang, KHONG ESP3D): file tam /btup/<ten>.tmp da duoc ghi vao the
  // trong luc nhan HTTP (temp_open/temp_write). O day chi con:
  //   1) DOI TEN file tam -> /<ten> (goc the, cho Marlin nhin thay)
  //   2) Bao Marlin mount lai the (M22/M21) + xin lai danh sach file
  //      - MUX (PANEL_SD_SHARED): tra bus SPI cho Marlin TRUOC khi bao;
  //      - UART (PANEL_SD_UART): khong co bus de tra, chi bao Marlin (no doc sector qua UART).
  // -------------------------------------------------------------------------
  ov_create_full();                       // lop phu + khoa UI trong luc ghi the (ngan nguoi dung
  ui_lock(true);                          //   bam IN / mo FILES trong luc ghi)
  ov_set("DANG GHI VAO THE SD...", s_name, true);
  bool ok = false;
  if (s_tmp[0] == '\0' || !s_sd_ready) {
    snprintf(s_err, sizeof(s_err), "khong co file tam tren the");
  } else {
    char dst[72];
    snprintf(dst, sizeof(dst), "/%s", s_name);
    if (SD.exists(dst)) SD.remove(dst);   // FATFS rename KHONG ghi de -> xoa ban cu truoc
    ok = SD.rename(s_tmp, dst);
    if (ok) {
      ELOG("[UP] da doi ten %s -> %s (%lu B)\n", s_tmp, dst, (unsigned long)s_got);
      s_tmp[0] = '\0';
      s_posted = s_got;
      snprintf(s_result, sizeof(s_result), "DONE %lu", (unsigned long)s_got);
      ov_set("DA GHI VAO THE SD", s_name, true);
    } else {
      snprintf(s_err, sizeof(s_err), "khong doi ten duoc file tren the");
      if (SD.exists(s_tmp)) SD.remove(s_tmp);   // don file tam ngay (con dang giu bus)
      s_tmp[0] = '\0';
    }
  }
  ui_lock(false);
  #if PANEL_SD_SHARED
    sd_release_to_marlin(ok);             // tra bus (+ M22/M21 + xin lai danh sach neu OK)
  #else
    if (ok) notify_media_changed();       // the cua panel: chi can bao Marlin mount lai
  #endif
  if (!ok) {
    snprintf(s_result, sizeof(s_result), "ERR %s", s_err);
    ov_set(s_err[0] ? s_err : "ghi the loi", "", false);
  }
  if (posted) *posted = s_posted;
  if (err && errn) snprintf(err, errn, "%s", s_err[0] ? s_err : "");
#else
  // 1) WiFi: mang nha (STA) hoac tam sang AP cua ESP3D (che do PANEL_ESP3D_VIA_AP)
  if (!wifi_enter_esp3d()) {
    snprintf(s_err, sizeof(s_err), "WiFi khong ket noi");
    ELOG("[UP] %s -> khong day duoc\n", s_err);
    // Che do doi mang (PANEL_ESP3D_VIA_AP=1): neu JOIN THAT BAI thi van phai QUAY VE mang nha,
    // khong thi panel mat WiFi vinh vien (web server khong truy cap duoc cho tới khi reboot).
    wifi_leave_esp3d();
  } else {
    net_wifi_ps(false);                 // dang day: tat modem sleep cho nhanh
    ov_ms = 0; rate_ms = 0; rate_base = 0; rate_x10 = 0;
    ov_create_full();                   // lop phu TOAN MAN HINH + khoa UI (chi trong luc day)
    ui_lock(true);
    // 2) Do IP ESP3D + POST (thu lai khi loi ket noi - file tam van con tren the)
    IPAddress ip;
    bool ok = false;
    if (!esp3d_find(&ip)) {
      snprintf(s_err, sizeof(s_err), "khong tim thay ESP3D");
    } else {
      for (uint8_t attempt = 0;; attempt++) {
        ok = http_post_file(ip, size);
        if (ok || attempt >= PANEL_HTTP_POST_RETRY) break;
        ELOG("[UP] POST loi (%s) -> thu lai lan %u\n", s_err, (unsigned)(attempt + 1));
        ov_set("THU LAI KET NOI...", "", true);
        const uint32_t tr = millis();
        while (millis() - tr < PANEL_HTTP_POST_RETRY_MS) {
          net_wifi_poll();
          lv_timer_handler();
          panel_link_task();
          delay(5);
        }
      }
    }
    ui_lock(false);
    net_wifi_ps(true);
    wifi_leave_esp3d();                 // che do doi mang: quay ve mang nha (web server)

    if (ok) {
      // 3) Init lai the SD cho Marlin (M22 = nha media, M21 = init media) roi xin lai danh sach file
      ELOG("[UP] POST OK %lu B -> M22/M21\n", (unsigned long)s_posted);
      panel_link_send_gcode2("M22", "M21");
      const uint32_t t0 = millis();
      while (millis() - t0 < PANEL_HTTP_SDWAIT_MS) {
        lv_timer_handler();
        panel_link_task();
        delay(10);
      }
      panel.files_done = false;
      panel.n_files = 0;
      panel_link_request((uint8_t)PANEL_REQ_FILES);
      snprintf(s_result, sizeof(s_result), "DONE %lu", (unsigned long)s_posted);
      ov_note("DONE", true);
      net_esp3d_temp_abort();           // xoa file tam (da nam tren the SD cua Marlin)
      if (posted) *posted = s_posted;
    } else {
      net_esp3d_temp_abort();
      snprintf(s_result, sizeof(s_result), "ERR %s", s_err);
      ov_note(s_err[0] ? s_err : "post loi", false);
      if (err && errn) snprintf(err, errn, "%s", s_err[0] ? s_err : "post loi");
    }
  }
#endif  // PANEL_SD_SHARED
  if (!s_result[0]) {
    snprintf(s_result, sizeof(s_result), "ERR %s", s_err[0] ? s_err : "loi khong ro");
    if (err && errn) snprintf(err, errn, "%s", s_err[0] ? s_err : "loi khong ro");
  }
  // Go lop phu sau 3 s (che do the dung chung: chi 1,5 s vi buoc ghi the gan nhu tuc thi) de
  // nguoi dung kip doc ket qua (khong chan vong lap: chi hen gio)
  const uint32_t t_note = millis();
#if PANEL_SD_SHARED
  const uint32_t note_ms = 1500u;
#else
  const uint32_t note_ms = 3000u;
#endif
  while (millis() - t_note < note_ms) {
    lv_timer_handler();
    panel_link_task();
    delay(10);
  }
  ov_destroy();
  s_busy = false;
  return (s_result[0] == 'D' && s_result[1] == 'O');
}

// ===========================================================================
// Vong doi + log
// ===========================================================================
void net_esp3d_begin(void) {
#if PANEL_SD_SHARED
  // THE DUNG CHUNG: luc khoi dong the thuoc MARLIN -> panel KHONG mount, chi dat chan chon MUX ve
  // phia Marlin. Panel chi gianh bus khi co phien upload (xem sd_mount/sd_release_to_marlin).
  pinMode(PANEL_SD_MUX_PIN, OUTPUT);
  digitalWrite(PANEL_SD_MUX_PIN, PANEL_SD_MUX_MARLIN_LEVEL);
  ELOG("[UP] the SD DUNG CHUNG voi Marlin qua MUX (chan chon = GPIO%d, muc mac dinh = MARLIN). "
       "Panel chi gianh bus khi nhan file va khong in.\n", (int)PANEL_SD_MUX_PIN);
#else
  if (sd_mount(true)) clean_dir("luc khoi dong", true);
  else ELOG("[UP] CANH BAO: chua mount duoc the TF (cam the FAT32 - se tu thu lai moi 2,5 s)\n");
#endif
}

void net_esp3d_task(void) {
#if PANEL_SD_SHARED
  // The dung chung: khi panel ranh thi bus thuoc Marlin => TUYET DOI khong cham vao the (khong
  // kiem tra 1 Hz, khong mount lai). Trang thai the lay tu Marlin (co PANEL_SF_MEDIA trong PUSH).
  return;
#else
  const uint32_t now = millis();
  if (s_sd_ready) {
    if (now - s_sd_check_ms < PANEL_SD_CHECK_MS) return;
    s_sd_check_ms = now;
    if (!sd_alive()) sd_lost("kiem tra dinh ky 1 Hz");
    return;
  }
  if (s_sd_retry_req) s_sd_retry_req = false;
  else if (now - s_sd_retry_ms < PANEL_SD_RETRY_MS) return;
  s_sd_retry_ms = now;
  if (sd_mount(false)) clean_dir("mount lai", false);
#endif
}

void net_esp3d_log(void) {
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < s_heap_min) s_heap_min = heap;
  // Ten file tam dang co (chi ten, khong ca duong dan - dong log gon)
  const char *t = s_tmp[0] ? strrchr(s_tmp, '/') : nullptr;
#if PANEL_SD_SHARED
  // The dung chung: KHONG goi SD.totalBytes() khi chua mount (vua ton SPI vua vo nghia vi bus
  // dang thuoc Marlin). Trang thai the that su nam o co PANEL_SF_MEDIA cua Marlin.
  const uint32_t free_kb = 0;
  const int sd_state = (int)net_esp3d_sd_ready();
#else
  const uint32_t free_kb = (SD.totalBytes() > SD.usedBytes())
                               ? (uint32_t)((SD.totalBytes() - SD.usedBytes()) / 1024ull) : 0u;
  const int sd_state = (int)s_sd_ready;
#endif
  Serial.printf("[UP] wifi=%u ssid=\"%s\" ip=%s rssi=%d sd=%d busy=%d rx=%lu posted=%lu "
                "heap=%lu heapmin=%lu free=%luKB | file=%s | %s\n",
                (unsigned)net_wifi_state(), net_wifi_ssid(), net_wifi_ip(), (int)net_wifi_rssi(),
                sd_state, (int)s_busy, (unsigned long)s_got, (unsigned long)s_posted,
                (unsigned long)heap, (unsigned long)s_heap_min, (unsigned long)free_kb,
                t ? t + 1 : "-", s_result[0] ? s_result : "-");
}

#else   // ---- PANEL_WIFI_UPLOAD = 0: stub rong (moi env van link duoc) ----

void net_esp3d_begin(void) {}
void net_esp3d_task(void) {}
void net_esp3d_log(void) {}
bool net_esp3d_sd_ready(void) { return false; }
bool net_esp3d_sd_mount_now(void) { return false; }
void net_esp3d_sd_retry(void) {}
bool net_esp3d_temp_open(const char *name, uint32_t size_hint) {
  (void)name; (void)size_hint; return false;
}
size_t net_esp3d_temp_write(const uint8_t *d, const size_t n) { (void)d; (void)n; return 0; }
uint32_t net_esp3d_temp_close(void) { return 0; }
void net_esp3d_temp_abort(void) {}
const char *net_esp3d_temp_path(void) { return ""; }
uint32_t net_esp3d_temp_bytes(void) { return 0; }
bool net_esp3d_printer_busy(void) { return false; }
bool net_esp3d_raw_read(uint32_t sector, uint8_t *dst) { (void)sector; (void)dst; return false; }
bool net_esp3d_raw_write(uint32_t sector, const uint8_t *src) { (void)sector; (void)src; return false; }
bool net_esp3d_delete(const char *name) { (void)name; return false; }
bool net_esp3d_push(const char *name, uint32_t size, uint32_t *posted, char *err, size_t errn) {
  (void)name; (void)size; (void)posted; (void)err; (void)errn;
  return false;
}
const char *net_esp3d_last_result(void) { return ""; }
uint32_t net_esp3d_last_posted(void) { return 0; }

#endif  // PANEL_WIFI_UPLOAD

