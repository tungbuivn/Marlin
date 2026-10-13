/**
 * panel_bt_upload.cpp -- BT SPP -> the TF cua panel -> POST len ESP3D -> M22/M21.
 *
 * Xem panel_bt_upload.h de biet luong tong the. File nay chi duoc bien dich khi
 * PANEL_BTUP_ENABLED = 1 (board CYD + LVGL + PANEL_BT_UPLOAD); cac env khac chi thay stub rong.
 *
 * VI SAO PHAI CO TASK RIENG CHO BT (khong doc BT trong loop()):
 *   BluetoothSerial cua ESP32 (core 2.0.17) day byte vao mot queue chi 512 byte
 *   (libraries/BluetoothSerial/src/BluetoothSerial.cpp:43 "RX_QUEUE_SIZE 512", :341 xQueueSend voi
 *   timeout 0 -> byte nao khong kip lay se bi MAT IM LANG). Ghi the (SD.write 4 KB) co the block
 *   vai chuc ms, va o 60 KB/s thi 512 byte chi ~8.5 ms -> neu doc BT trong loop() thi chac chan
 *   mat byte. Vi vay: 1 task rieng (uu tien 5) chi lam moi viec "hut" queue 512 byte vao vong dem
 *   8 KB (cap phat tu heap trong panel_bt_upload_begin); loop() lay tu vong dem ra ghi the.
 */
#include "panel_config.h"

// Can cho CA nhanh stub (khi PANEL_BT_UPLOAD = 0: khong include Arduino.h/WiFi.h gi ca).
#include <stdint.h>
#include <stdbool.h>

#if defined(PANEL_BT_UPLOAD) && PANEL_BT_UPLOAD && PANEL_USE_LVGL && defined(PANEL_BOARD_CYD)

#include <Arduino.h>
#include <WiFi.h>
#include <BluetoothSerial.h>
#include <SD.h>
#include <SPI.h>
#include <esp_system.h>          // esp_random()
#include <esp_heap_caps.h>       // heap_caps_get_largest_free_block() - cua an toan truoc khi bat BT
#include <esp_bt.h>              // esp_bt_controller_get_status() - try-and-check sau SerialBT.begin()
#include <esp_bt_main.h>         // esp_bluedroid_get_status()
#include <esp_gap_bt_api.h>      // esp_bt_gap_set_security_param() - SSP "Just Works"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lvgl.h>

#if PANEL_ESP3D_USE_MDNS
  #include <ESPmDNS.h>            // chi keo vao khi bat -DPANEL_ESP3D_USE_MDNS=1 (xem panel_config.h)
#endif

#include "panel_bt_upload.h"
#include "panel_link.h"
#include "ui_lvgl.h"             // UI_LCD_HOR_RES / UI_LCD_VER_RES

#if defined(PANEL_CYD_DEBUG)
  #define BTLOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define BTLOG(...)   do { } while (0)
#endif

namespace {

// ---------------------------------------------------------------------------
// Hang so
// ---------------------------------------------------------------------------
enum : uint8_t { ST_IDLE = 0, ST_SNIFF, ST_RECV, ST_POST, ST_SDWAIT, ST_NOTE, ST_DRAIN };

constexpr uint32_t RING_BYTES  = PANEL_BT_RING_BYTES;      // luy thua cua 2
constexpr uint32_t RING_MASK   = RING_BYTES - 1u;
constexpr uint32_t RING_KEEP   = RING_MASK - 1u;           // chua 1 o de phan biet day/rong
constexpr size_t   SD_CHUNK    = PANEL_BT_SD_CHUNK;        // khoi ghi xuong the (4 KB)
constexpr size_t   SNIFF_MAX   = 96;                       // du cho "PUT <ten 31> <10 so>\r\n"
// Stack cua task hut BT: task chi goi available()/read() (xQueueReceive) + vTaskDelay, khong
// printf/khong goi sau -> 2 KB la du (vua du de tiet kiem heap; tang len 3072 neu gap panic
// "Stack canary watchpoint triggered (btup_rx)").
constexpr uint32_t TASK_STACK  = 2048;

// Mau: lay DUNG gia tri cua ui_lvgl.cpp (MR_BG / MR_PROGBG / MR_PROGBAR / MR_FRAME /
// MR_RATE100 / MR_HOTEND) de lop phu dong bo, nhung khong include/sua file do.
constexpr uint32_t OV_BG = 0x1E156E, OV_BAR = 0x2A2080, OV_FILL = 0x00E5FF,
                   OV_TXT = 0xE6E1E5, OV_OK = 0x81C784, OV_ERR = 0xFF8A80;

// ---------------------------------------------------------------------------
// Trang thai
// ---------------------------------------------------------------------------
BluetoothSerial s_bt;
bool            s_bt_ready = false;
bool            s_sd_ready = false;

// WiFi: BAT SAN tu luc khoi dong (icon trang thai o header can trang thai THAT, xem net_upload.h).
// s_wifi_state la bien DA CACHE de net_wifi_state() tra ve ngay, khong goi WiFi.* trong vong lap UI.
bool              s_wifi_started = false;
volatile uint8_t  s_wifi_state = 0;        // 0 tat/chua cau hinh, 1 dang ket noi, 2 da ket noi
uint32_t          s_wifi_retry_ms = 0;
bool              s_post_retryable = false; // loi ket noi -> cho phep POST lai
bool              s_both_ready_logged = false;
uint32_t          s_heap_min = 0xFFFFFFFFu; // heap thap nhat trong phien (in ra o log 1 Hz)

// IP module ESP3D: do 1 lan luc bat dau pha 2 roi CACHE (khong quet moi vong lap).
IPAddress         s_esp3d_ip;               // 0.0.0.0 = chua biet
bool              s_esp3d_found = false;
#if PANEL_ESP3D_USE_MDNS
bool              s_mdns_started = false;
#endif

// vong dem don-san-xuat / mot-nguoi-tieu-thu (task BT ghi, loop doc).
// CAP PHAT TU HEAP (khong phai mang tinh): vung DRAM cho bien tinh cua ESP32 chi 124.580 byte
// (tools/sdk/esp32/ld/memory.ld:66) va da bi LVGL + buffer ve an gan het; 12 KB nay lam linker
// bao "dram0_0_seg overflowed". Cap phat 1 lan trong panel_bt_upload_begin() (task BT can vong
// dem NGAY ca khi chua co phien -> khong the free khi ranh).
uint8_t          *s_ring = nullptr;
volatile uint32_t s_head = 0, s_tail = 0;
volatile uint32_t s_drop = 0;              // so byte bi mat (vong dem day)
volatile uint32_t s_bt_rx_ms = 0;          // millis() cua byte BT cuoi cung

uint8_t  s_state = ST_IDLE;
File     s_file;
uint8_t  *s_stage = nullptr;               // khoi dang gom de ghi xuong the (cung tu heap)
size_t   s_stage_len = 0;
char     s_name[PANEL_BT_NAME_MAX + 1];    // ten file se nam tren the SD cua Marlin
char     s_tmp[48];                        // duong dan file tam tren the TF cua panel
bool     s_have_size = false;              // true: header "PUT <ten> <so_byte>"
uint32_t s_declared = 0;
uint32_t s_rx = 0;                         // so byte da nhan duoc (pha 1)
uint32_t s_next_prog = 0;                  // moc gui "PROG" qua BT ke tiep
uint32_t s_bytes_posted = 0;               // so byte da POST (pha 2)
uint32_t s_state_ms = 0;
uint32_t s_note_ms = 0;
uint32_t s_sdinit_ms = 0;
uint32_t s_discard_until = 0;              // bo qua byte vun cua phien truoc
char     s_err[48];

// do toc do (KB/s x10) + nhip cap nhat lop phu
uint32_t s_rate_ms = 0, s_rate_base = 0, s_rate_x10 = 0;
uint32_t s_ov_ms = 0;

// sniff dong header
uint8_t  s_sniff[SNIFF_MAX];
size_t   s_sniff_len = 0;

// lop phu LVGL (lv_layer_top)
lv_obj_t *s_ov = nullptr;
lv_obj_t *s_ov_name = nullptr, *s_ov_pct = nullptr, *s_ov_rate = nullptr,
         *s_ov_state = nullptr, *s_ov_bar = nullptr;
char      s_txt_name[24], s_txt_pct[32], s_txt_rate[24], s_txt_state[48];

// ---- Theo doi THE TF nong (rut ra / cam vao) - xem README 9.12 ----
uint32_t s_sd_check_ms = 0;                 // lan kiem tra "the con do" gan nhat
uint32_t s_sd_retry_ms = 0;                 // lan thu mount lai gan nhat
uint8_t  s_space_tick = 0;                  // dem khoi de kiem dung luong the dinh ky
bool     s_sd_retry_req = false;            // UI yeu cau thu mount ngay (panel_bt_upload_sd_retry)
bool     s_sd_was_ok = false;               // da tung mount duoc (de log "da mount LAI")

// ---- Tam dung task hut BT khi goi SerialBT.end() ----
// _stop_bt() XOA _spp_rx_queue (BluetoothSerial.cpp:729-733); neu task hut dang nam trong
// xQueueReceive tren queue do thi se dung handle da bi xoa -> phai cho task "dung bai" truoc.
volatile bool s_bt_pump_pause  = false;     // true = task KHONG duoc cham vao s_bt
volatile bool s_bt_pump_parked = false;     // task bao "da ra khoi vung doc BT"

// ---- Ket qua phien upload, de GUI LAI cho app khi BT bat lai / app noi lai ----
// (luc POST thi BT da tat nen app khong nhan duoc DONE/ERR ngay - xem README 9.11)
char     s_last_msg[80];
bool     s_last_msg_pending = false;
uint16_t s_bt_cycles = 0;                   // so lan tat/bat BT (theo doi ro ri bo nho)

// ---- Che do nhan file (mac dinh TAT - xem PANEL_BT_UPLOAD_AUTOSTART) + bao mat ----
bool     s_enabled = false;                 // co dang CHO PHEP nhan file (bat bang API/UI)
uint32_t s_enable_ms = 0;                   // luc bat (de tu tat sau PANEL_BT_ENABLE_MIN phut)
uint32_t s_last_act_ms = 0;                 // hoat dong cuoi cua phien (de tu tat)
bool     s_scan_hidden = false;             // da chuyen NON-DISCOVERABLE sau lan client dau tien
bool     s_client_ever = false;             // da tung co client ket noi (dat trong callback SPP)
bool     s_task_created = false;            // task hut BT da tao (chi tao 1 lan)
char     s_peer_mac[20] = "?";              // MAC client (log ra COM7 + callback SPP)

// ===========================================================================
// Vong dem BT (SPSC)
// ===========================================================================
inline uint32_t ring_used(void) {
  if (s_ring == nullptr) return 0;
  return (s_head - s_tail) & RING_MASK;
}

// Cap phat 2 buffer lam viec (heap). Tra ve false neu khong du RAM -> tat phan nhan file.
bool buf_alloc(void) {
  if (s_ring == nullptr) s_ring = (uint8_t *)malloc(RING_BYTES);
  if (s_stage == nullptr) s_stage = (uint8_t *)malloc(SD_CHUNK);
  if (s_ring == nullptr || s_stage == nullptr) {
    free(s_ring);
    free(s_stage);
    s_ring = nullptr;
    s_stage = nullptr;
    return false;
  }
  return true;
}

// Lay toi da max_len byte ra khoi vong dem. Tra ve so byte lay duoc.
size_t ring_take(uint8_t *dst, const size_t max_len) {
  __sync_synchronize();
  const uint32_t t = s_tail;
  uint32_t n = (s_head - t) & RING_MASK;
  if (n > max_len) n = (uint32_t)max_len;
  for (uint32_t i = 0; i < n; i++) dst[i] = s_ring[(t + i) & RING_MASK];
  s_tail = (t + n) & RING_MASK;
  return (size_t)n;
}

// Bo het byte dang co trong vong dem (dung khi ket thuc/loi phien).
void ring_discard(void) {
  __sync_synchronize();
  s_tail = s_head;
}

// ---------------------------------------------------------------------------
// Task hut queue BT -> vong dem. KHONG cham the SD/LVGL (chi doc BT).
// ---------------------------------------------------------------------------
void bt_pump_task(void *) {
  for (;;) {
    if (!s_bt_pump_pause && s_ring != nullptr) {
      int guard = 0;
      while (s_bt.available() > 0) {
        const int c = s_bt.read();
        if (c < 0) break;
        const uint32_t h = s_head;
        if (((h - s_tail) & RING_MASK) >= RING_KEEP) {
          s_drop++;                                   // het cho -> mat byte nay
        } else {
          s_ring[h] = (uint8_t)c;
          __sync_synchronize();
          s_head = (h + 1u) & RING_MASK;
        }
        s_bt_rx_ms = millis();
        if (++guard >= 1024) break;                    // nhuong CPU cho task khac
      }
    }
    // Bao cho main biet da ROI khoi vung doc BT (de no goi SerialBT.end() an toan).
    s_bt_pump_parked = s_bt_pump_pause;
    vTaskDelay(1);
  }
}

// ---------------------------------------------------------------------------
// Tra loi text qua BT (app dien thoai doc duoc)
// ---------------------------------------------------------------------------
void bt_send(const char *fmt, ...) {
  if (!s_bt_ready) return;
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  const size_t len = ((size_t)n < sizeof(buf)) ? (size_t)n : (sizeof(buf) - 1u);
  s_bt.write(reinterpret_cast<const uint8_t *>(buf), len);
}

void err_set(const char *txt) {
  snprintf(s_err, sizeof(s_err), "%s", txt);
}

void err_setf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(s_err, sizeof(s_err), fmt, ap);
  va_end(ap);
}

// SPP con ket noi khong? BluetoothSerial::hasClient() = `_spp_client > 0`, va khi app ngat
// (ESP_SPP_CLOSE_EVT) thu vien dat `_spp_client = 0` (BluetoothSerial.cpp:300, :823) -> dung de
// phat hien "app ngat giua pha 1" (khong the biet con bao nhieu byte nua => phai coi la loi).
bool bt_alive(void) {
  return s_bt_ready && s_bt.hasClient();
}

// ===========================================================================
// Lop phu LVGL tren lv_layer_top (KHONG dung ui_lvgl.cpp)
// ===========================================================================
lv_obj_t *ov_label(const char *txt, const uint32_t col) {
  lv_obj_t *l = lv_label_create(s_ov);
  if (!l) return nullptr;
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_color(l, lv_color_hex(col), 0);
  return l;
}

// Dat lai vi tri/kich thuoc cho 2 che do: nho (pha 1) va toan man hinh (pha 2).
void ov_layout(const bool full) {
  if (!s_ov) return;
  const lv_coord_t x  = full ? 8 : 6;
  const lv_coord_t w  = UI_LCD_HOR_RES - (full ? 16 : 12);
  const lv_coord_t y0 = full ? 56 : 4;
  if (full) lv_obj_set_pos(s_ov, 0, 0), lv_obj_set_size(s_ov, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  else      lv_obj_set_pos(s_ov, 4, 28), lv_obj_set_size(s_ov, UI_LCD_HOR_RES - 8, 92);
  if (s_ov_name)  lv_obj_set_pos(s_ov_name, x, y0);
  if (s_ov_state) lv_obj_set_pos(s_ov_state, x, y0 + 20);
  if (s_ov_pct)   lv_obj_set_pos(s_ov_pct, x, y0 + 40);
  if (s_ov_rate)  lv_obj_set_pos(s_ov_rate, x + 170, y0 + 40);
  if (s_ov_bar)   { lv_obj_set_pos(s_ov_bar, x, y0 + 62); lv_obj_set_size(s_ov_bar, w, 10); }
}

bool ov_create(const bool full) {
  if (s_ov) { ov_layout(full); return true; }
  lv_obj_t *top = lv_layer_top();
  if (!top) return false;
  s_ov = lv_obj_create(top);
  if (!s_ov) { BTLOG("[BTUP] khong tao duoc lop phu (het LVGL mem)\n"); return false; }
  lv_obj_set_style_radius(s_ov, 4, 0);
  lv_obj_set_style_border_width(s_ov, 1, 0);
  lv_obj_set_style_border_color(s_ov, lv_color_hex(OV_TXT), 0);
  lv_obj_set_style_bg_color(s_ov, lv_color_hex(OV_BG), 0);
  lv_obj_set_style_bg_opa(s_ov, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(s_ov, 0, 0);
  lv_obj_clear_flag(s_ov, LV_OBJ_FLAG_SCROLLABLE);
  // Pha 1: lop phu nho -> KHONG an cam ung (nguoi dung van dung duoc UI cho khac).
  lv_obj_clear_flag(s_ov, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *title = ov_label("BT UPLOAD", OV_FILL);
  if (title) lv_obj_set_pos(title, 6, full ? 30 : 4);

  s_ov_name  = ov_label("?", OV_TXT);
  s_ov_state = ov_label("...", OV_TXT);
  s_ov_pct   = ov_label("", OV_TXT);
  s_ov_rate  = ov_label("", OV_TXT);

  s_ov_bar = lv_bar_create(s_ov);
  if (s_ov_bar) {
    lv_bar_set_range(s_ov_bar, 0, 100);
    lv_obj_set_style_bg_color(s_ov_bar, lv_color_hex(OV_BAR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_ov_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_ov_bar, lv_color_hex(OV_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_ov_bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_ov_bar, 2, LV_PART_MAIN);
    lv_obj_set_style_radius(s_ov_bar, 2, LV_PART_INDICATOR);
  }
  ov_layout(full);
  return true;
}

void ov_destroy(void) {
  if (s_ov) lv_obj_del(s_ov);
  s_ov = nullptr;
  s_ov_name = s_ov_pct = s_ov_rate = s_ov_state = s_ov_bar = nullptr;
  s_txt_name[0] = s_txt_pct[0] = s_txt_rate[0] = s_txt_state[0] = '\0';
}

// Pha 2: lop phu TOAN MAN HINH (nen mo phu ca UI) + khoa input.
void ov_set_full(const bool full) {
  if (!s_ov) return;
  ov_layout(full);
  lv_obj_set_style_bg_opa(s_ov, full ? LV_OPA_90 : LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_ov, full ? 0 : 1, 0);
  lv_obj_invalidate(s_ov);
}

// Cap nhat text. Dung buffer tinh + lv_label_set_text_static de KHONG ton them LVGL heap
// (pool LVGL da bi ha xuong 30 KB cho du DRAM - xem src/lv_conf_panel.h); phai tu goi
// lv_obj_invalidate vi static khong tu invalidate.
void ov_set(const char *state, const char *name, const char *pct, const char *rate,
            const bool ok_col) {
  if (!s_ov) return;
  if (name && s_ov_name) {
    snprintf(s_txt_name, sizeof(s_txt_name), "%s", name);
    lv_label_set_text_static(s_ov_name, s_txt_name);
    lv_obj_invalidate(s_ov_name);
  }
  if (state && s_ov_state) {
    snprintf(s_txt_state, sizeof(s_txt_state), "%s", state);
    lv_obj_set_style_text_color(s_ov_state, lv_color_hex(ok_col ? OV_OK : OV_ERR), 0);
    lv_label_set_text_static(s_ov_state, s_txt_state);
    lv_obj_invalidate(s_ov_state);
  }
  if (pct && s_ov_pct) {
    snprintf(s_txt_pct, sizeof(s_txt_pct), "%s", pct);
    lv_label_set_text_static(s_ov_pct, s_txt_pct);
    lv_obj_invalidate(s_ov_pct);
  }
  if (rate && s_ov_rate) {
    snprintf(s_txt_rate, sizeof(s_txt_rate), "%s", rate);
    lv_label_set_text_static(s_ov_rate, s_txt_rate);
    lv_obj_invalidate(s_ov_rate);
  }
  if (s_ov_bar) lv_obj_invalidate(s_ov_bar);
}

void ov_rate_str(char *dst, const size_t n) {
  snprintf(dst, n, "%u.%u KB/s", (unsigned)(s_rate_x10 / 10u), (unsigned)(s_rate_x10 % 10u));
}

void ov_bar_pct(const uint32_t done, const uint32_t total) {
  if (!s_ov_bar) return;
  lv_bar_set_value(s_ov_bar, total ? (int32_t)((uint64_t)done * 100u / total) : 0, LV_ANIM_OFF);
}

void ov_phase1(const char *state) {
  if (millis() - s_ov_ms < 200) return;
  s_ov_ms = millis();
  char pct[32], rate[24];
  if (s_have_size && s_declared)
    snprintf(pct, sizeof(pct), "%u%%  %u/%u KB",
             (unsigned)((uint64_t)s_rx * 100u / s_declared),
             (unsigned)(s_rx / 1024u), (unsigned)(s_declared / 1024u));
  else
    snprintf(pct, sizeof(pct), "%u KB", (unsigned)(s_rx / 1024u));
  ov_rate_str(rate, sizeof(rate));
  ov_set(state, s_name, pct, rate, true);
  ov_bar_pct(s_rx, s_have_size ? s_declared : 0);
}

void ov_phase2(const uint32_t done, const uint32_t total) {
  if (millis() - s_ov_ms < 200) return;
  s_ov_ms = millis();
  char pct[32], rate[24];
  snprintf(pct, sizeof(pct), "%u%%  %u/%u KB",
           (unsigned)((uint64_t)done * 100u / (total ? total : 1u)),
           (unsigned)(done / 1024u), (unsigned)(total / 1024u));
  ov_rate_str(rate, sizeof(rate));
  ov_set("DANG DAY LEN ESP3D", s_name, pct, rate, true);
  ov_bar_pct(done, total);
}

void ov_note(const char *state, const bool ok_col) {
  if (!s_ov) return;
  ov_set_full(false);
  ov_set(state, s_name[0] ? s_name : "?", "", "", ok_col);
}

// Tat/mo TAT CA input device (cam ung + encoder). API LVGL 8.4: lv_indev_get_next nam trong
// src/hal/lv_hal_indev.h (lvgl.h:33 -> src/hal/lv_hal.h) -> dung duoc ma khong sua ui_lvgl.cpp.
void ui_lock(const bool lock) {
  lv_indev_t *i = lv_indev_get_next(nullptr);
  while (i) {
    lv_indev_enable(i, !lock);
    i = lv_indev_get_next(i);
  }
  BTLOG("[BTUP] UI %s\n", lock ? "KHOA" : "MO KHOA");
}

// ===========================================================================
// Tien ich
// ===========================================================================
void rate_tick(const uint32_t done) {
  const uint32_t now = millis();
  if (s_rate_ms == 0) { s_rate_ms = now; s_rate_base = done; return; }
  const uint32_t dt = now - s_rate_ms;
  if (dt < 1000u) return;
  s_rate_x10 = (uint32_t)(((uint64_t)(done - s_rate_base) * 10000ull) / ((uint64_t)dt * 1024ull));
  s_rate_ms = now;
  s_rate_base = done;
}

void rate_reset(void) {
  s_rate_ms = 0; s_rate_base = 0; s_rate_x10 = 0;
}

// Marlin dang in? (khong cho nhan file khi may dang in: ESP3D se gianh bus SPI cua the SD)
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

// Ten file: bo moi ky tu la, khong cho duong dan, them ".gco" neu thieu phan mo rong.
void sanitize_name(const char *in) {
  char out[PANEL_BT_NAME_MAX + 1];
  size_t o = 0;
  for (const char *p = in; *p && o < PANEL_BT_NAME_MAX; p++) {
    const char c = *p;
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
        c == '_' || c == '-' || c == '.')
      out[o++] = c;
  }
  out[o] = '\0';
  while (out[0] == '.') memmove(out, out + 1, strlen(out));       // bo '.' dau (file an)
  if (out[0] == '\0') snprintf(out, sizeof(out), "%s", PANEL_BT_DEF_NAME);
  bool dot = false;
  for (const char *p = out; *p; p++) if (*p == '.') dot = true;
  if (!dot) {
    if (strlen(out) > (PANEL_BT_NAME_MAX - 4u)) out[PANEL_BT_NAME_MAX - 4u] = '\0';
    strncat(out, ".gco", sizeof(out) - strlen(out) - 1u);
  }
  snprintf(s_name, sizeof(s_name), "%s", out);
}

// Khai bao truoc (dinh nghia o duoi trong file) - tranh phai sap xep lai cac khoi lon.
void     heap_track(void);
uint16_t clean_dir(const char *why, const bool verbose);

bool sd_mount(const bool verbose) {
  if (s_sd_ready) return true;
  // The TF cua CYD nam tren VSPI (bus RIENG voi LCD): User_Setup.h:56-57 ghi ro
  // "LCD o HSPI (VSPI de danh cho khe the SD: CS 5 / MOSI 23 / MISO 19 / SCLK 18)".
  // Kiem tra lai (2026-10-14): TFT_eSPI chon HSPI nho `#define USE_HSPI_PORT` (User_Setup.h:57)
  // nen KHONG dung chung bus voi the; trong ui_lvgl_port.cpp nhanh CYD chi dung `s_lcd` (TFT_eSPI)
  // + cam ung bit-bang 25/32/39/33/36, con `SPI.beginTransaction` o dong 117 nam trong nhanh
  // STM32 (`#endif // !PANEL_BOARD_CYD` dong 121) -> khong co xung dot VSPI.
  // SPIClass::begin() khong lam gi neu bus da init (SPI.cpp:73) nen goi lai vo hai.
  SPI.begin(PANEL_SD_SCK, PANEL_SD_MISO, PANEL_SD_MOSI, PANEL_SD_CS);

  // Thu lan luot 20 -> 10 -> 4 MHz. `verbose=false` (thu lai dinh ky khi dang mat the) thi KHONG
  // log tung buoc de khong spam moi 2,5 s - chi log khi THANH CONG (o duoi).
  const uint32_t hz_try[3] = { PANEL_SD_HZ, PANEL_SD_HZ / 2u, 4000000u };
  for (uint8_t i = 0; i < 3u; i++) {
    if (verbose) BTLOG("[BTUP] thu mount the TF @ %u Hz ...\n", (unsigned)hz_try[i]);
    if (SD.begin(PANEL_SD_CS, SPI, hz_try[i], "/sd", 4)) {
      s_sd_ready = true;
      BTLOG("[BTUP] %s the TF @ %u Hz: cardType=%d, tong %lu KB, da dung %lu KB\n",
            s_sd_was_ok ? "da mount LAI" : "the TF OK", (unsigned)hz_try[i], (int)SD.cardType(),
            (unsigned long)(SD.totalBytes() / 1024ull), (unsigned long)(SD.usedBytes() / 1024ull));
      s_sd_was_ok = true;
      return true;
    }
    delay(30);                                  // SD.begin da tu don trang thai khi that bai
  }
  if (verbose)
    BTLOG("[BTUP] SD.begin(CS=%d) THAT BAI o ca 3 muc toc do (cardType=%d). Kiem tra: "
          "(1) da cam the TF chua, (2) the phai FAT32 (KHONG exFAT), (3) khe TF/bus VSPI 18/19/23/5.\n",
          (int)PANEL_SD_CS, (int)SD.cardType());
  return false;
}

// Kiem tra RE xem the con do khong: 1 lan f_stat tren thu muc goc (~0,3-1 ms SPI).
// KHONG dung SD.cardType() vi no tra gia tri CACHE (khong noi len the con cam hay khong).
bool sd_alive(void) {
  if (!s_sd_ready) return false;
  return SD.exists("/");
}

// Danh dau MAT THE: dong file dang mo, ep unmount (SD.end()) de lan sau SD.begin() mount THAT SU
// (neu khong, SDFS::begin() thay _pdrv != 0xFF se tra true ngay ma khong mount lai).
void sd_lost(const char *why) {
  if (!s_sd_ready) return;
  if (s_file) s_file.close();
  s_sd_ready = false;
  SD.end();
  BTLOG("[BTUP] MAT THE TF (rut ra?): %s\n", why);
  s_sd_retry_ms = millis();                     // bat dau dem nhip thu mount lai
}

// ---- Theo doi nong the TF: goi moi vong lap tu panel_bt_upload_task() ----
void sd_poll(void) {
  // CHI kiem/thu mount khi RANH (khong o giua phien nhan, khong luc dang POST/SDWAIT) -> khong bao
  // gio tranh SPI voi duong ghi file/duong doc file cua pha 2.
  const bool idle_ok = (s_state == ST_IDLE || s_state == ST_NOTE);
  if (!idle_ok) return;
  const uint32_t now = millis();

  if (s_sd_ready) {
    if (now - s_sd_check_ms < PANEL_SD_CHECK_MS) return;
    s_sd_check_ms = now;
    if (!sd_alive()) sd_lost("kiem tra dinh ky 1 Hz");
    return;
  }
  // Dang mat the: thu mount lai co gioi han nhip (hoac ngay khi UI yeu cau)
  if (s_sd_retry_req) s_sd_retry_req = false;
  else if (now - s_sd_retry_ms < PANEL_SD_RETRY_MS) return;
  s_sd_retry_ms = now;
  if (sd_mount(false)) {
    clean_dir("mount lai", false);
    heap_track();
  }
}

void del_temp(const char *why) {
  if (s_file) s_file.close();
  if (s_tmp[0] == '\0') return;
  if (!s_sd_ready) {
    // The dang mat -> KHONG xoa duoc; file se bi don o lan mount lai / lan khoi dong sau.
    BTLOG("[BTUP] khong xoa duoc file tam %s (%s): the TF dang mat\n", s_tmp, why);
    s_tmp[0] = '\0';
    return;
  }
  if (SD.exists(s_tmp) && !SD.remove(s_tmp))
    BTLOG("[BTUP] CANH BAO: khong xoa duoc file tam %s (%s)\n", s_tmp, why);
  else
    BTLOG("[BTUP] da xoa file tam %s (%s)\n", s_tmp, why);
  s_tmp[0] = '\0';
}

// ---------------------------------------------------------------------------
// Don thu muc file tam. VI SAO CAN: neu panel bi RESET / mat dien / watchdog giua luc dang nhan
// hoac dang POST thi /btup/<ten>.tmp nam lai VINH VIEN -> day the dan ma khong ai biet. Vi vay
// quet lai luc KHOI DONG (va ca dau moi phien cho chac).
// ---------------------------------------------------------------------------
bool is_tmp_file(const char *path) {
  const size_t n = strlen(path);
  return (n > 4u) && (strcmp(path + n - 4u, ".tmp") == 0);
}

// Noi chuoi co gioi han (khong dung snprintf voi nguon khong biet do dai -> tranh warning
// -Wformat-truncation khi build -O2 -Wall).
void append_cap(char *dst, const size_t cap, size_t &o, const char *src) {
  for (size_t i = 0; src[i] != '\0' && o + 1u < cap; i++) dst[o++] = src[i];
  dst[o] = '\0';
}

// Xoa MOI file *.tmp trong PANEL_BTUP_DIR. Tra ve so file da xoa; moi truong hop loi deu log ro.
uint16_t clean_dir(const char *why, const bool verbose) {
  if (!s_sd_ready) return 0;
  if (!SD.exists(PANEL_BTUP_DIR)) {
    if (SD.mkdir(PANEL_BTUP_DIR)) {
      if (verbose) BTLOG("[BTUP] da tao thu muc %s\n", PANEL_BTUP_DIR);
    } else {
      BTLOG("[BTUP] CANH BAO: khong tao duoc thu muc %s (%s)\n", PANEL_BTUP_DIR, why);
    }
    return 0;
  }
  File dir = SD.open(PANEL_BTUP_DIR);
  if (!dir || !dir.isDirectory()) {
    BTLOG("[BTUP] CANH BAO: khong mo duoc thu muc %s (%s) -> khong don duoc file tam\n",
          PANEL_BTUP_DIR, why);
    if (dir) dir.close();
    return 0;
  }

  uint16_t n = 0, failed = 0;
  uint64_t freed = 0;
  File f = dir.openNextFile();
  while (f) {
    const bool isdir = f.isDirectory();
    const size_t sz = f.size();
    const char *nm = f.name();               // ESP32: ten DAY DU dang "/btup/x.tmp" (vfs_api.cpp:467)
    char path[64];
    size_t o = 0;
    path[0] = '\0';
    if (nm && nm[0] == '/') {
      append_cap(path, sizeof(path), o, nm);
    } else {                                  // phong khi chi tra ve ten file
      append_cap(path, sizeof(path), o, PANEL_BTUP_DIR);
      append_cap(path, sizeof(path), o, "/");
      append_cap(path, sizeof(path), o, nm ? nm : "");
    }
    f.close();                               // dong TRUOC khi xoa (an toan hon tren FATFS)
    if (!isdir && is_tmp_file(path)) {
      if (SD.remove(path)) { n++; freed += sz; }
      else { failed++; BTLOG("[BTUP] CANH BAO: khong xoa duoc file tam %s\n", path); }
    }
    f = dir.openNextFile();
  }
  dir.close();

  const uint64_t total = SD.totalBytes(), used = SD.usedBytes();
  const uint64_t freeb = (total > used) ? (total - used) : 0;
  if (n || failed || verbose)
    BTLOG("[BTUP] don %s (%s): xoa %u file .tmp (%lu KB)%s; the TF con %lu/%lu KB\n",
          PANEL_BTUP_DIR, why, (unsigned)n, (unsigned long)(freed / 1024ull),
          failed ? " [CO FILE XOA LOI]" : "",
          (unsigned long)(freeb / 1024ull), (unsigned long)(total / 1024ull));
  return n;
}

// Tao file tam /btup/<ten>.tmp; don file mo coi cua phien truoc (neu co) va kiem tra cho trong.
bool open_temp(void) {
  if (!sd_mount(true)) { err_set("the TF panel loi"); return false; }
  clean_dir("dau phien", false);            // phong khi con file mo coi (reset giua 2 phien...)
  const uint64_t total = SD.totalBytes(), used = SD.usedBytes();
  const uint64_t freeb = (total > used) ? (total - used) : 0;
  // Log 1 dong moi phien (yeu cau: khong de the day am tham)
  BTLOG("[BTUP] the TF: con %lu KB / tong %lu KB (%s)\n", (unsigned long)(freeb / 1024ull),
        (unsigned long)(total / 1024ull), s_have_size ? "kieu header" : "kieu RAW");
  if (s_have_size && ((uint64_t)s_declared + (256ull * 1024ull) > freeb)) {
    err_setf("the TF panel het cho (%lu KB trong)", (unsigned long)(freeb / 1024ull));
    return false;
  }
  if (!SD.exists(PANEL_BTUP_DIR)) SD.mkdir(PANEL_BTUP_DIR);
  snprintf(s_tmp, sizeof(s_tmp), "%s/%s.tmp", PANEL_BTUP_DIR, s_name);
  if (SD.exists(s_tmp)) SD.remove(s_tmp);
  s_file = SD.open(s_tmp, FILE_WRITE);
  if (!s_file) { err_set("khong tao duoc file tam"); s_tmp[0] = '\0'; return false; }
  BTLOG("[BTUP] ghi vao %s\n", s_tmp);
  return true;
}

bool stage_flush(void) {
  if (s_stage_len == 0) return true;
  const size_t w = s_file.write(s_stage, s_stage_len);
  if (w != s_stage_len) {
    err_set("ghi the TF loi");
    sd_lost("ghi file tam that bai (the rut ra / loi the?)");
    return false;
  }
  s_stage_len = 0;
  return true;
}

bool stage_push(const uint8_t *p, size_t n) {
  while (n) {
    const size_t room = SD_CHUNK - s_stage_len;
    const size_t c = (n < room) ? n : room;
    memcpy(s_stage + s_stage_len, p, c);
    s_stage_len += c;
    p += c;
    n -= c;
    if (s_stage_len == SD_CHUNK && !stage_flush()) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// WiFi: bat san tu boot, tu ket noi lai khi mat, va cap nhat bien trang thai da cache.
// KHONG block (chi goi tu panel_bt_upload_task() / vong cho cua pha 2).
// ---------------------------------------------------------------------------
void wifi_start(void) {
  if (PANEL_ESP3D_SSID[0] == '\0') {
    BTLOG("[BTUP] PANEL_ESP3D_SSID rong -> khong bat WiFi (net_wifi_state = 0)\n");
    return;
  }
  WiFi.persistent(false);          // KHONG ghi credential xuong NVS moi lan ket noi (do flash)
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);     // driver tu ket noi lai khi mat
  WiFi.setSleep(true);             // pha 2 khi day file se tat lai; pha 1 WiFi TAT han
  WiFi.begin(PANEL_ESP3D_SSID, PANEL_ESP3D_PASS);
  s_wifi_started = true;
  s_wifi_state = 1;
  s_wifi_retry_ms = millis();
  BTLOG("[BTUP] WiFi bat dau ket noi \"%s\" ... (heap truoc khi bat: %u B)\n",
        PANEL_ESP3D_SSID, (unsigned)ESP.getFreeHeap());
}

// Tat WiFi NGAY (khong cho) - dung sau khi POST xong: tra ~40-50 KB heap + nhuong radio cho BT.
void wifi_off_now(const char *why) {
  if (!s_wifi_started) return;
  s_wifi_started = false;          // dat truoc de wifi_poll() khong bat lai
  const uint32_t before = ESP.getFreeHeap();
  WiFi.disconnect(true);           // ngat ket noi + tat STA
  WiFi.mode(WIFI_OFF);
  s_wifi_state = 0;                // net_wifi_state() = 0 -> icon WiFi xam
  s_both_ready_logged = false;
  heap_track();
  BTLOG("[BTUP] %s -> TAT WiFi: heap %u -> %u B (khoi lien tuc=%u B)\n", why,
        (unsigned)before, (unsigned)ESP.getFreeHeap(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
}

// Cua an toan RAM TRUOC khi bat WiFi (sau khi da tat BT): WiFi STA (driver + lwip + supplicant)
// can ~40-50 KB, va lwip/TCP can vai khoi lien tuc.
bool wifi_ram_ok(void) {
  const uint32_t freeb = ESP.getFreeHeap();
  const size_t   big   = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  BTLOG("[BTUP] cua an toan WiFi: heap=%u B, khoi lien tuc=%u B (can >= %u / %u)\n",
        (unsigned)freeb, (unsigned)big, (unsigned)PANEL_WIFI_MIN_HEAP,
        (unsigned)PANEL_WIFI_MIN_BLOCK);
  return (freeb >= PANEL_WIFI_MIN_HEAP) && (big >= PANEL_WIFI_MIN_BLOCK);
}

// Tat WiFi khi da ranh (chi la luoi an toan - duong chinh da goi wifi_off_now() sau moi POST).
// Goi tu st_idle()/st_note(); moc "ranh" = s_note_ms (luc phien truoc ket thuc).
void wifi_stop_idle(void) {
  if (!s_wifi_started) return;
  if (PANEL_ESP3D_KEEP_MS && (millis() - s_note_ms) < PANEL_ESP3D_KEEP_MS) return;
  wifi_off_now("ranh");
}

// Theo doi heap thap nhat (goi ca trong vong lap blocking cua pha 2 - luc do log 1 Hz KHONG chay).
void heap_track(void) {
  const uint32_t h = ESP.getFreeHeap();
  if (h < s_heap_min) s_heap_min = h;
}

// ===========================================================================
// BAT / TAT BT Classic (chay TUAN TU voi WiFi - xem README 9.11) + scan mode (README 9.13)
// ===========================================================================
// Callback SPP rieng: CHI de lay MAC cua client (core goi custom callback cho MOI su kien SPP -
// BluetoothSerial.cpp:414). Chay trong context task BTC -> chi ghi chuoi/co, KHONG goi LVGL/SD.
void spp_evt_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT && param) {
    snprintf(s_peer_mac, sizeof(s_peer_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             param->srv_open.rem_bda[0], param->srv_open.rem_bda[1], param->srv_open.rem_bda[2],
             param->srv_open.rem_bda[3], param->srv_open.rem_bda[4], param->srv_open.rem_bda[5]);
    s_client_ever = true;
  }
}

// Doi phan DISCOVERABLE cua scan mode: CONNECTABLE luon bat => may DA PAIR van ket noi duoc.
//   DISCOVERABLE     = hien trong danh sach quet (de pair may moi / pair lai sau khi "quen" thiet bi)
//   NON-DISCOVERABLE = may la khong thay
// Log MOI lan doi kem ly do (yeu cau cua nguoi dung).
void bt_scan_mode(const bool discoverable, const char *why) {
  if (!s_bt_ready) return;
  const esp_err_t e = esp_bt_gap_set_scan_mode(
      ESP_BT_CONNECTABLE, discoverable ? ESP_BT_GENERAL_DISCOVERABLE : ESP_BT_NON_DISCOVERABLE);
  s_scan_hidden = !discoverable;
  BTLOG("[BTUP] scan mode -> %s (%s) [esp_err=%d]\n",
        discoverable ? "DISCOVERABLE" : "NON-DISCOVERABLE", why, (int)e);
}

// (Khong con "ly do tat" rieng: che do nhan file mac dinh luon BAT (AUTOSTART=1); API
// panel_bt_upload_set_enabled(false) chi dung de khoa tam khi can.)

// Cho task hut BT "dung bai" truoc khi goi SerialBT.end() (end() xoa _spp_rx_queue; neu task dang
// nam trong xQueueReceive tren queue do thi dung handle da bi xoa -> hong).
void bt_pump_pause_wait(void) {
  s_bt_pump_pause = true;
  const uint32_t t0 = millis();
  while (!s_bt_pump_parked && (millis() - t0) < 200u) {
    lv_timer_handler();
    panel_link_task();
    delay(1);
  }
}

// Bat BT Classic. Task hut BT CHI tao 1 lan duy nhat trong ca firmware (s_task_created).
// Co CUA AN TOAN RAM: Bluedroid cap phat dong ma khong kiem tra NULL -> thieu RAM la PANIC.
bool bt_start(void) {
  const uint32_t freeb = ESP.getFreeHeap();
  const size_t   big   = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  BTLOG("[BTUP] bat BT Classic%s: heap=%u B, khoi lien tuc=%u B (can >= %u / %u)\n",
        s_task_created ? " lai" : " (lan dau)", (unsigned)freeb, (unsigned)big,
        (unsigned)PANEL_BT_MIN_HEAP, (unsigned)PANEL_BT_MIN_BLOCK);
  if (freeb < PANEL_BT_MIN_HEAP || big < PANEL_BT_MIN_BLOCK) {
    BTLOG("[BTUP] KHONG bat BT: khong du RAM (panel van chay binh thuong)\n");
    return false;
  }
  // PIN phai dat TRUOC begin() (core doc _isPinSet/_pin_len/_pin_code trong begin():
  // BluetoothSerial.cpp:686-688 -> btSetPin()). _pin_* la bien STATIC nen gia tri con sau end(),
  // nhung cu dat lai moi lan cho chac chan (ca lan begin lai sau khi end - xem README 9.11).
  s_bt.setPin(PANEL_BT_PIN);
  s_bt.register_callback(spp_evt_cb);    // de biet MAC client (log ai da gui)
  if (!s_bt.begin(PANEL_BT_NAME)) {
    BTLOG("[BTUP] BluetoothSerial.begin(\"%s\") THAT BAI\n", PANEL_BT_NAME);
    return false;
  }
#if PANEL_BT_JUST_WORKS
  // Ghi de IO capability ve NONE de pair "Just Works" (khong hoi ma). Phai goi SAU begin() vi
  // begin() moi dat ESP_BT_IO_CAP_IO (BluetoothSerial.cpp:694) - goi sau thi gia tri nay thang.
  {
    esp_bt_sp_param_t sp  = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t   cap = ESP_BT_IO_CAP_NONE;
    const esp_err_t   e   = esp_bt_gap_set_security_param(sp, &cap, sizeof(cap));
    BTLOG("[BTUP] SSP Just Works (IO cap NONE) -> esp_bt_gap_set_security_param = %d\n", (int)e);
  }
#endif
  // Try-and-check: doc LAI trang thai that cua 2 tang stack (Bluedroid co the panic trong task BTU -
  // khong bat duoc bang try/catch - nhung neu no tra ve ma stack khong ENABLED thi tat tinh nang).
  const int ctrl_st = (int)esp_bt_controller_get_status();
  const int bd_st   = (int)esp_bluedroid_get_status();
  if (esp_bt_controller_get_status() != ESP_BT_CONTROLLER_STATUS_ENABLED ||
      esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
    BTLOG("[BTUP] CANH BAO: stack BT khong ENABLED (controller=%d, bluedroid=%d) -> khong dung BT\n",
          ctrl_st, bd_st);
    return false;
  }
  s_bt.setTimeout(1);                    // read() chi block toi da 1 tick
  s_bt_pump_parked = false;
  s_bt_pump_pause  = false;
  if (!s_task_created) {
    const BaseType_t r = xTaskCreatePinnedToCore(bt_pump_task, "btup_rx", TASK_STACK, nullptr, 5,
                                                 nullptr, 1);
    if (r != pdPASS) {
      BTLOG("[BTUP] KHONG tao duoc task hut BT -> tat nhan file BT\n");
      return false;
    }
    s_task_created = true;
  }
  s_bt_ready = true;
  heap_track();
  // SCAN MODE: LUON DISCOVERABLE (yeu cau nguoi dung) => bat ky dien thoai nao cung thay TBT-PANEL
  // va pair duoc bat cu luc nao. Bao mat nam o TOKEN (dong dau phien) chu khong o scan mode.
  bt_scan_mode(true, s_enabled ? "san sang nhan file" : "BT bat nhung dang khoa (set_enabled(false))");
  BTLOG("[BTUP] BT SPP \"%s\" (PIN: %s%s) san sang (controller=%d, bluedroid=%d): heap=%u B, "
        "khoi lien tuc=%u B\n", PANEL_BT_NAME, PANEL_BT_PIN,
        PANEL_BT_JUST_WORKS ? ", SSP Just Works" : "",
        ctrl_st, bd_st, (unsigned)ESP.getFreeHeap(),
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  return true;
}

// Tat BT Classic de TRA BO NHO cho WiFi. Tra ve so byte thu hoi duoc.
// SerialBT.end() -> _stop_bt(): esp_spp_deinit + esp_bluedroid_disable/deinit + btStop()
// (BluetoothSerial.cpp:710-751; btStop -> esp_bt_controller_disable/deinit o esp32-hal-bt.c:62-88).
uint32_t bt_stop(const char *why) {
  if (!s_bt_ready) return 0;
  s_bt_ready = false;                    // chan moi bt_send/bt_alive tu day
  bt_pump_pause_wait();
  const uint32_t before = ESP.getFreeHeap();
  s_bt.end();
  const uint32_t after = ESP.getFreeHeap();
  const uint32_t gain = (after > before) ? (after - before) : 0;
  s_bt_cycles++;
  heap_track();
  BTLOG("[BTUP] %s -> TAT BT (lan %u): heap %u -> %u B (+%u B), khoi lien tuc=%u B\n", why,
        (unsigned)s_bt_cycles, (unsigned)before, (unsigned)after, (unsigned)gain,
        (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  if (gain < PANEL_BT_END_MIN_GAIN)
    BTLOG("[BTUP] !! CANH BAO: SerialBT.end() chi tra %u B (< %u B). Neu lap lai nhieu lan ma "
          "khong tra du thi phai cat DRAM tinh (xem README 9.11)\n",
          (unsigned)gain, (unsigned)PANEL_BT_END_MIN_GAIN);
  return gain;
}

// ---- Ket qua phien: luu lai de GUI LAI cho app (luc POST thi BT da tat) ----
void result_set(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(s_last_msg, sizeof(s_last_msg), fmt, ap);
  va_end(ap);
  s_last_msg_pending = true;
}

// Gui ket qua dang cho ngay khi BT san sang VA co client (goi moi vong lap - re).
void result_try_send(void) {
  if (!s_last_msg_pending || !s_bt_ready || !bt_alive()) return;
  bt_send("%s\n", s_last_msg);
  BTLOG("[BTUP] da gui ket qua phien truoc cho app qua BT: %s\n", s_last_msg);
  s_last_msg_pending = false;
}

void wifi_poll(void) {
  if (!s_wifi_started) { s_wifi_state = 0; return; }   // WiFi dang TAT (lazy) -> icon xam
  if (WiFi.status() == WL_CONNECTED) {
    if (s_wifi_state != 2)
      BTLOG("[BTUP] WiFi DA ket noi, IP %s, heap=%u B\n",
            WiFi.localIP().toString().c_str(), (unsigned)ESP.getFreeHeap());
    s_wifi_state = 2;
    if (s_bt_ready && !s_both_ready_logged) {
      // Con so quan trong nhat de quyet dinh cat buffer: heap NGAY SAU khi ca WiFi lan BT da len.
      s_both_ready_logged = true;
      heap_track();
      BTLOG("[BTUP] WiFi + BT Classic DA SAN SANG: heap tu do = %u B "
            "(buffer cua module da lay %u B)\n",
            (unsigned)ESP.getFreeHeap(), (unsigned)(RING_BYTES + SD_CHUNK + TASK_STACK));
    }
    return;
  }
  s_wifi_state = 1;                                     // dang thu / dang thu lai -> icon DO
  const uint32_t now = millis();
  if (now - s_wifi_retry_ms >= PANEL_BT_WIFI_RETRY_MS) {
    s_wifi_retry_ms = now;
    if (WiFi.getMode() != WIFI_STA) WiFi.mode(WIFI_STA);
    WiFi.begin(PANEL_ESP3D_SSID, PANEL_ESP3D_PASS);     // autoReconnect lo phan lon; day la chot ha
    BTLOG("[BTUP] WiFi mat ket noi -> thu lai\n");
  }
}

void wifi_ps(const bool power_save) {
  if (!s_wifi_started) return;
  WiFi.setSleep(power_save);      // pha 2: tat tiet kiem de day nhanh; pha 1: WiFi da TAT han
}

// Cho WiFi ket noi (co ve lop phu + nuoi UART; khong chan cung vi van goi lv_timer_handler).
// WIFI DUOC BAT O DAY (lazy) - khong bat luc boot, de SerialBT.begin() chay mot minh.
bool wifi_up(void) {
  if (PANEL_ESP3D_SSID[0] == '\0') { err_set("chua cau hinh WiFi ESP3D"); return false; }
  if (WiFi.status() == WL_CONNECTED) { s_wifi_state = 2; return true; }
  if (!s_wifi_started) {
    // Bat WiFi lan dau (chi tu pha 2, sau khi da tat BT) - co cua an toan RAM.
    if (!wifi_ram_ok()) { err_set("thieu RAM cho WiFi"); return false; }
    wifi_start();
  }
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - t0 > PANEL_BT_WIFI_MS) { err_set("WiFi khong ket noi"); return false; }
    wifi_poll();
    lv_timer_handler();
    panel_link_task();
    delay(10);
  }
  s_wifi_state = 2;
  return true;
}

// ===========================================================================
// TU DO IP MODULE ESP3D (chi khi bat dau PHA 2; ket qua duoc cache)
// ===========================================================================
void ip_str(char *dst, const size_t n, const IPAddress &ip) {
  snprintf(dst, n, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

void tried_add(char *buf, const size_t cap, const IPAddress &ip) {
  size_t l = strlen(buf);
  if (l + 18u >= cap) return;
  if (l) buf[l++] = ',';                  // dau phay ngan cach; l = vi tri ghi tiep theo
  char t[16];
  ip_str(t, sizeof(t), ip);
  for (size_t i = 0; t[i] != '\0' && l + 1u < cap; i++) buf[l++] = t[i];
  buf[l] = '\0';
}

// <ip> co phai web server ESP3D khong? Gui lenh [ESP800] (lenh duy nhat khong can dang nhap) va
// tim chuoi "FW version:" trong phan hoi (esp3d/command.cpp:1916+). KHONG dung tay vao the SD.
bool probe_esp3d(const IPAddress &ip, const uint16_t port) {
  WiFiClient c;
  c.setTimeout(PANEL_BT_PROBE_MS);
  if (!c.connect(ip, port, PANEL_BT_PROBE_MS)) return false;

  char req[112];
  const int rn = snprintf(req, sizeof(req),
      "GET /command?plain=%%5BESP800%%5D HTTP/1.0\r\nHost: %u.%u.%u.%u\r\n"
      "Connection: close\r\n\r\n", ip[0], ip[1], ip[2], ip[3]);
  if (rn <= 0 || (size_t)rn >= sizeof(req)) { c.stop(); return false; }
  c.write(reinterpret_cast<const uint8_t *>(req), (size_t)rn);

  static const char MARK[] = "FW version:";      // nhan dang ESP3D
  const size_t mark_len = sizeof(MARK) - 1u;
  char   win[24];
  size_t wl = 0;
  bool   found = false;
  const uint32_t t0 = millis();
  while (millis() - t0 < PANEL_BT_PROBE_MS) {
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
  return found;
}

void esp3d_set(const IPAddress &ip, const char *src) {
  s_esp3d_ip = ip;
  s_esp3d_found = true;
  char t[16];
  ip_str(t, sizeof(t), ip);
  BTLOG("[BTUP] ESP3D thay o %s (nguon: %s)\n", t, src);
}

// Do IP ESP3D: [PANEL_ESP3D_HOST neu dat] -> [mDNS neu bat] -> [gateway] -> [danh sach IP].
bool esp3d_ensure(void) {
  if (s_esp3d_found) return true;
  const uint32_t t0 = millis();
  char tried[112];
  tried[0] = '\0';
  BTLOG("[BTUP] dang do IP ESP3D...\n");

  // 1) IP nguoi dung ep: dung luon neu tra loi (khong do tiep)
  if (PANEL_ESP3D_HOST[0] != '\0') {
    IPAddress ip;
    if (ip.fromString(PANEL_ESP3D_HOST)) {
      if (probe_esp3d(ip, PANEL_ESP3D_PORT)) { esp3d_set(ip, "cau hinh"); return true; }
      BTLOG("[BTUP] PANEL_ESP3D_HOST (%s) khong tra loi -> do tiep\n", PANEL_ESP3D_HOST);
      tried_add(tried, sizeof(tried), ip);
    } else {
      BTLOG("[BTUP] CANH BAO: PANEL_ESP3D_HOST \"%s\" khong phai IP hop le\n", PANEL_ESP3D_HOST);
    }
  }

#if PANEL_ESP3D_USE_MDNS
  // 2) mDNS (mac dinh TAT - xem panel_config.h; ten mac dinh cua ESP3D la "MYESP" chu khong phai "esp3d")
  if (!s_mdns_started) {
    s_mdns_started = MDNS.begin("tbt-panel");
    if (!s_mdns_started) BTLOG("[BTUP] CANH BAO: khong khoi dong duoc mDNS\n");
  }
  if (s_mdns_started) {
    const char *p = PANEL_ESP3D_MDNS_NAMES;
    while (*p && (millis() - t0) < PANEL_BT_PROBE_TOTAL_MS) {
      const char *e = strchr(p, ',');
      const size_t n = e ? (size_t)(e - p) : strlen(p);
      if (n && n < 32u) {
        char nm[32];
        memcpy(nm, p, n);
        nm[n] = '\0';
        IPAddress ip = MDNS.queryHost(nm, PANEL_BT_PROBE_MS);
        if (ip != IPAddress(0, 0, 0, 0)) {
          if (probe_esp3d(ip, PANEL_ESP3D_PORT)) { esp3d_set(ip, "mdns"); return true; }
          tried_add(tried, sizeof(tried), ip);
        }
      }
      if (!e) break;
      p = e + 1;
    }
  }
#endif

  // 3) gateway cua mang dang noi: che do AP thi CHINH LA module ESP3D
  const IPAddress gw = WiFi.gatewayIP();
  if (gw != IPAddress(0, 0, 0, 0) && (millis() - t0) < PANEL_BT_PROBE_TOTAL_MS) {
    if (probe_esp3d(gw, PANEL_ESP3D_PORT)) { esp3d_set(gw, "gateway"); return true; }
    tried_add(tried, sizeof(tried), gw);
  }

  // 4) danh sach IP ung vien
  const char *p = PANEL_ESP3D_IP_LIST;
  while (*p && (millis() - t0) < PANEL_BT_PROBE_TOTAL_MS) {
    const char *e = strchr(p, ',');
    const size_t n = e ? (size_t)(e - p) : strlen(p);
    if (n && n < 20u) {
      char buf[20];
      memcpy(buf, p, n);
      buf[n] = '\0';
      IPAddress ip;
      if (ip.fromString(buf)) {
        if (ip != gw) {                       // gateway da thu o tren
          if (probe_esp3d(ip, PANEL_ESP3D_PORT)) { esp3d_set(ip, "danh sach"); return true; }
          tried_add(tried, sizeof(tried), ip);
        }
      }
    }
    if (!e) break;
    p = e + 1;
  }

  BTLOG("[BTUP] KHONG tim thay ESP3D (da thu: %s)\n", tried[0] ? tried : "(khong co ung vien)");
  return false;
}

// ===========================================================================
// PHA 2: POST file tam len ESP3D (blocking, da khoa UI)
// ===========================================================================
bool write_all(WiFiClient &sock, const char *src, const size_t len) {
  size_t off = 0;
  const uint32_t t0 = millis();
  while (off < len) {
    const size_t w = sock.write(reinterpret_cast<const uint8_t *>(src) + off, len - off);
    if (w == 0) {
      if (!sock.connected() || (millis() - t0) > PANEL_BT_HTTP_TIMEOUT_MS) return false;
      lv_timer_handler();
      panel_link_task();
      delay(1);
      continue;
    }
    off += w;
  }
  return true;
}

uint16_t parse_code(const char *ln) {
  const char *sp = strchr(ln, ' ');
  if (!sp) return 0;
  return (uint16_t)atoi(sp + 1);
}

// Doc phan hoi HTTP cua ESP3D (handleSDFileList tra JSON co "status":"Ok"/"Upload failed").
bool http_read_reply(WiFiClient &sock, uint16_t *code, char *status, const size_t status_sz,
                     bool *saw_status) {
  char    ln[96];
  size_t  ln_len = 0;
  uint8_t phase = 0;                 // 0 = dong trang thai, 1 = header, 2 = than
  char    win[10];
  size_t  win_len = 0;
  bool    in_st = false;
  size_t  st_len = 0;
  uint32_t bytes = 0;
  const uint32_t t0 = millis();

  for (;;) {
    while (sock.available() > 0) {
      const int ci = sock.read();
      if (ci < 0) break;
      const char c = (char)ci;
      bytes++;
      if (!*saw_status) {
        if (phase == 0) {
          if (c == '\n') { ln[ln_len] = '\0'; *code = parse_code(ln); phase = 1; ln_len = 0; }
          else if (c != '\r' && ln_len + 1u < sizeof(ln)) ln[ln_len++] = c;
        } else if (phase == 1) {
          if (c == '\n') { if (ln_len == 0) phase = 2; ln_len = 0; }
          else if (c != '\r' && ln_len + 1u < sizeof(ln)) ln[ln_len++] = c;
        } else if (in_st) {
          if (c == '"') { status[st_len] = '\0'; in_st = false; *saw_status = true; }
          else if (st_len + 1u < status_sz) status[st_len++] = c;
        } else {
          if (win_len < sizeof(win)) win[win_len++] = c;
          else { memmove(win, win + 1, sizeof(win) - 1u); win[sizeof(win) - 1u] = c; }
          if (win_len >= sizeof(win) && memcmp(win, "\"status\":\"", sizeof(win)) == 0) {
            in_st = true;
            st_len = 0;
          }
        }
      }
    }
    if (*saw_status) return true;
    if (bytes > PANEL_BT_MAX_REPLY_BYTES) return (*code != 0);
    if (!sock.connected() && sock.available() == 0) return (*code != 0);
    if (millis() - t0 > PANEL_BT_HTTP_TIMEOUT_MS) return (*code != 0);
    delay(1);
  }
}

bool http_post(const uint32_t size) {
  ov_set("KET NOI WIFI...", s_name, "", "", true);
  if (!wifi_up()) { s_post_retryable = true; return false; }

  // Tim IP module ESP3D (cache lai; chi do khi chua biet hoac IP cu da fail).
  // Luu y: 192.168.0.0/24 trung dai IP mang nha nguoi dung -> BAT BUOC phai xac nhan day dung la
  // ESP3D (probe_esp3d gui [ESP800] va doi "FW version:"), khong chi kiem "port 80 mo".
  ov_set("TIM ESP3D...", s_name, "", "", true);
  if (!esp3d_ensure()) {
    err_set("khong tim thay ESP3D");
    s_post_retryable = true;
    return false;
  }
  char ipbuf[16];
  ip_str(ipbuf, sizeof(ipbuf), s_esp3d_ip);

  ov_set("KET NOI ESP3D...", s_name, "", "", true);
  WiFiClient sock;
  sock.setTimeout(PANEL_BT_HTTP_TIMEOUT_MS);
  if (!sock.connect(s_esp3d_ip, PANEL_ESP3D_PORT, PANEL_BT_HTTP_TIMEOUT_MS)) {
    s_esp3d_found = false;                 // IP sai / module doi IP -> lan sau do lai
    BTLOG("[BTUP] khong ket noi duoc ESP3D %s -> se do lai o phien sau\n", ipbuf);
    err_set("khong ket noi duoc ESP3D");
    s_post_retryable = true;
    return false;
  }
  wifi_ps(false);                 // dang day file: uu tien WiFi (tat modem sleep)

  // multipart/form-data: ESP3D-direct-sd chi nhan file qua web_server.upload() khi
  // Content-Type la "multipart/*" (xem bao cao: webinterface.cpp:386 + Parsing-impl.h _parseForm).
  // Ten field ("file") khong quan trong - ESP8266WebServer nhan dien phan file qua "filename="
  // trong Content-Disposition, va doi "name=" dung TRUOC "filename=".
  char boundary[40];
  snprintf(boundary, sizeof(boundary), "TBTpanel%08x", (unsigned)esp_random());

  char pre[256];
  const int pre_n = snprintf(pre, sizeof(pre),
      "--%s\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
      "Content-Type: application/octet-stream\r\n\r\n", boundary, s_name);
  char epi[64];
  const int epi_n = snprintf(epi, sizeof(epi), "\r\n--%s--\r\n", boundary);
  if (pre_n <= 0 || epi_n <= 0 || (size_t)pre_n >= sizeof(pre) || (size_t)epi_n >= sizeof(epi)) {
    sock.stop();
    wifi_ps(true);
    err_set("loi dung request");
    return false;
  }

  char hdr[320];
  const int hdr_n = snprintf(hdr, sizeof(hdr),
      "POST %s HTTP/1.1\r\n"
      "Host: %s:%u\r\n"
      "Content-Type: multipart/form-data; boundary=%s\r\n"
      "Content-Length: %lu\r\n"
      "Connection: close\r\n\r\n",
      PANEL_ESP3D_PATH, ipbuf, (unsigned)PANEL_ESP3D_PORT, boundary,
      (unsigned long)((uint64_t)pre_n + (uint64_t)size + (uint64_t)epi_n));
  if (hdr_n <= 0 || (size_t)hdr_n >= sizeof(hdr)) {
    sock.stop();
    wifi_ps(true);
    err_set("loi dung header");
    return false;
  }

  BTLOG("[BTUP] POST %s%s (%lu B + %d B multipart)\n", ipbuf, PANEL_ESP3D_PATH,
        (unsigned long)size, pre_n + epi_n);

  bool ok = write_all(sock, hdr, (size_t)hdr_n) && write_all(sock, pre, (size_t)pre_n);
  if (!ok) s_post_retryable = true;

  File f;
  uint32_t sent = 0;
  if (ok) {
    f = SD.open(s_tmp, FILE_READ);
    if (!f) { err_set("khong mo lai duoc file tam"); ok = false; }
  }
  rate_reset();
  s_ov_ms = 0;
  const uint32_t t0 = millis();
  while (ok && sent < size) {
    // WiFi rá»›t giá»¯a lÃºc Ä‘ang Ä‘áº©y -> há»§y sáº¡ch (Ä‘Ã³ng socket á»Ÿ dÆ°á»›i, má»Ÿ khÃ³a UI á»Ÿ finish_recv).
    if (WiFi.status() != WL_CONNECTED) {
      err_set("WiFi mat ket noi");
      s_post_retryable = true;
      ok = false;
      break;
    }
    const size_t n = f.read(s_stage, SD_CHUNK);
    if (n == 0) { err_set("doc file tam loi"); ok = false; break; }
    if (!write_all(sock, reinterpret_cast<const char *>(s_stage), n)) {
      err_set("ESP3D ngat ket noi");
      s_post_retryable = true;
      ok = false;
      break;
    }
    sent += (uint32_t)n;
    rate_tick(sent);
    ov_phase2(sent, size);
    heap_track();
    lv_timer_handler();
    panel_link_task();
    if (millis() - t0 > PANEL_BT_POST_TIMEOUT_MS) {
      err_set("qua thoi gian POST");
      s_post_retryable = true;
      ok = false;
      break;
    }
  }
  if (f) f.close();
  if (ok && !write_all(sock, epi, (size_t)epi_n)) { ok = false; s_post_retryable = true; }

  if (ok) {
    uint16_t code = 0;
    char status[24] = {0};
    bool saw = false;
    if (!http_read_reply(sock, &code, status, sizeof(status), &saw)) {
      err_set("ESP3D khong tra loi");
      s_post_retryable = true;
      ok = false;
    } else if (code != 200) {
      err_setf("ESP3D tra HTTP %u", (unsigned)code);
      ok = false;
    } else if (saw && strcmp(status, "Ok") != 0) {
      err_setf("ESP3D: %s", status);
      ok = false;
    } else if (!saw) {
      BTLOG("[BTUP] CANH BAO: khong thay \"status\" trong phan hoi (>%u B) - coi nhu OK\n",
            (unsigned)PANEL_BT_MAX_REPLY_BYTES);
    }
  }
  sock.stop();
  wifi_ps(true);                  // tra radio ve che do tiet kiem (cho pha 1 / BT)
  s_bytes_posted = sent;
  return ok;
}

// ===========================================================================
// May trang thai phien
// ===========================================================================
void fail_session(void) {
  if (s_file) s_file.close();
  s_stage_len = 0;
  if (s_tmp[0]) del_temp("phien loi");
  const char *why = s_err[0] ? s_err : "loi khong ro";
  BTLOG("[BTUP] ERR: %s\n", why);
  bt_send("ERR %s\n", why);
  if (s_ov) ov_note(why, false);
  s_state = ST_DRAIN;
  s_state_ms = millis();
}

// PHA 1 xong -> PHA 2 -> M22/M21 -> SDWAIT
void finish_recv(void) {
  if (!stage_flush()) { fail_session(); return; }
  const uint32_t got = s_rx;
  s_file.flush();
  s_file.close();

  if (s_have_size && got != s_declared) {
    err_setf("chi nhan %lu/%lu B", (unsigned long)got, (unsigned long)s_declared);
    fail_session();
    return;
  }
  if (got == 0) { err_set("file rong"); fail_session(); return; }
  if (s_drop) { err_set("mat du lieu BT (vong dem day)"); fail_session(); return; }

  // 1) Bao app: da nhan xong file (truoc khi tat BT - sau khi tat thi khong gui duoc nua)
  bt_send("OK %s %lu\n", s_name, (unsigned long)got);
  s_bt.flush();
  delay(80);                     // cho RFCOMM kip day het du lieu cuoi ra khong day

  // 2) TAT BT de TRA ~100 KB heap cho WiFi (BT va WiFi KHONG BAO GIO bat dong thoi - README 9.11)
  s_state = ST_POST;
  bt_stop("ket thuc pha 1");

  // 3) Cua an toan RAM cho WiFi: khong du thi KHONG co bat WiFi (tranh panic), bao ERR + bat lai BT
  if (!wifi_ram_ok()) {
    err_set("thieu RAM cho WiFi");
    result_set("ERR %s", s_err);
    if (s_tmp[0]) del_temp("thieu RAM cho WiFi");
    bt_start();
    BTLOG("[BTUP] het pha 1 nhung khong du RAM cho WiFi -> da bat lai BT, bao ERR cho app\n");
    if (s_ov) ov_note(s_err, false);
    s_note_ms = millis();
    s_state = ST_NOTE;
    if (s_have_size) s_discard_until = millis() + PANEL_BT_DISCARD_MS;
    return;
  }

  // ---- PHA 2: khoa UI roi day len ESP3D (BT dang TAT, chi WiFi chay) ----
  s_ov_ms = 0;
  ov_set_full(true);
  ui_lock(true);
  bool ok = false;
  for (uint8_t attempt = 0;; attempt++) {
    s_post_retryable = false;
    ok = http_post(got);
    if (ok || !s_post_retryable || attempt >= PANEL_BT_POST_RETRY) break;
    // Loi ket noi (WiFi/ESP3D): thu lai sau it giay. File tam van con tren the -> khong phai gui lai qua BT.
    BTLOG("[BTUP] POST loi (%s) -> thu lai lan %u\n", s_err, (unsigned)(attempt + 1));
    ov_set("THU LAI KET NOI...", s_name, "", "", true);
    const uint32_t tr = millis();
    while (millis() - tr < PANEL_BT_POST_RETRY_MS) {
      wifi_poll();
      lv_timer_handler();
      panel_link_task();
      delay(5);
    }
  }
  ui_lock(false);
  ov_set_full(false);

  // 4) Tat WiFi (tra ~40-50 KB) roi BAT LAI BT (an lai ~100 KB) de san sang phien sau
  wifi_off_now("POST xong");
  bt_start();

  if (!ok) {
    if (s_tmp[0]) del_temp("post loi");     // khong giu file tam: lan sau gui lai file khac
    result_set("ERR %s", s_err[0] ? s_err : "post loi");
    result_try_send();                      // gui ngay neu app con ket noi; khong thi se gui khi noi lai
    if (s_ov) ov_note(s_err[0] ? s_err : "post loi", false);
    s_note_ms = millis();
    s_state = ST_NOTE;
    if (s_have_size) s_discard_until = millis() + PANEL_BT_DISCARD_MS;
    return;
  }

  // ---- POST xong: init lai the SD cho Marlin (M22 = nha media, M21 = init media) ----
  BTLOG("[BTUP] POST OK %lu B -> M22/M21\n", (unsigned long)got);
  result_set("DONE %lu", (unsigned long)s_bytes_posted);
  panel_link_send_gcode2("M22", "M21");
  s_sdinit_ms = millis();
  s_state = ST_SDWAIT;
  if (s_have_size) s_discard_until = millis() + PANEL_BT_DISCARD_MS;
}

void start_raw(void) {
  sanitize_name(PANEL_BT_DEF_NAME);
  s_have_size = false;
  s_declared = 0;
  bt_send("OK %s RAW\n", s_name);
  ov_set("DANG NHAN (RAW)", s_name, "", "", true);
  if (!open_temp()) { fail_session(); return; }
  if (s_sniff_len) {
    s_rx += (uint32_t)s_sniff_len;
    if (!stage_push(s_sniff, s_sniff_len)) { fail_session(); return; }
  }
  s_state_ms = millis();
  s_next_prog = PANEL_BT_PROG_STEP;
  s_state = ST_RECV;
}

// TU CHOI phien do sai/thieu TOKEN: KHONG nhan byte nao, khong tao file tam, dong phien.
void reject_token(void) {
  BTLOG("[BTUP] TU CHOI phien: sai token (tu %s)\n", s_peer_mac);
  err_set("sai ma token");
  bt_send("ERR sai ma token\n");
  s_tmp[0] = '\0';                         // chac chan khong co file tam nao
  s_stage_len = 0;
  s_rx = 0;
  if (s_ov) ov_note("sai ma token", false);
  s_note_ms = millis();
  s_discard_until = millis() + PANEL_BT_IDLE_MS;
  s_state = ST_DRAIN;                      // bo het du lieu con lai cua ke gui sai
  s_state_ms = millis();
}

void start_header(const size_t nl) {
  char ln[SNIFF_MAX + 1];
  size_t l = nl;
  if (l && s_sniff[l - 1] == '\r') l--;
  if (l > SNIFF_MAX) l = SNIFF_MAX;
  memcpy(ln, s_sniff, l);
  ln[l] = '\0';

  const char *p = ln + 4;                                   // bo "PUT "
  while (*p == ' ' || *p == '\t') p++;

  // ---- TOKEN (lop bao mat chinh): "PUT <token> <ten_file> <so_byte>" ----
  if (PANEL_BT_TOKEN[0] != '\0') {
    const char *e0 = p;
    while (*e0 && *e0 != ' ' && *e0 != '\t') e0++;
    const size_t tl = (size_t)(e0 - p);
    const size_t want = strlen(PANEL_BT_TOKEN);
    if (tl != want || memcmp(p, PANEL_BT_TOKEN, want) != 0) { reject_token(); return; }
    p = e0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '\0') { reject_token(); return; }             // chi co token, thieu ten file
  }

  const char *e = p;
  while (*e && *e != ' ' && *e != '\t') e++;
  const size_t nm_len = (size_t)(e - p);
  if (nm_len == 0 || nm_len > PANEL_BT_NAME_MAX) { err_set("header: ten file sai"); fail_session(); return; }
  char nm[PANEL_BT_NAME_MAX + 1];
  memcpy(nm, p, nm_len);
  nm[nm_len] = '\0';

  while (*e == ' ' || *e == '\t') e++;

  // SO BYTE: co the la so, hoac '*' / KHONG CO => KHONG BIET TRUOC (ket thuc khi im 3 s, giong RAW
  // nhung DA co token nen van an toan). Nho vay nguoi dung khong phai tu tra dung so byte.
  uint64_t sz = 0;
  bool have_sz = false;
  if (*e >= '0' && *e <= '9') {
    have_sz = true;
    for (const char *q = e; *q >= '0' && *q <= '9'; q++) {
      sz = sz * 10ull + (uint64_t)(*q - '0');
      if (sz > 0xFFFFFFFFull) { sz = 0xFFFFFFFFull; break; }
    }
    if (sz == 0) { err_set("header: so byte = 0"); fail_session(); return; }
  } else if (*e == '*' || *e == '\0') {
    have_sz = false;
  } else {
    err_set("header: so byte khong hop le"); fail_session(); return;
  }

  sanitize_name(nm);
  s_have_size = have_sz;
  s_declared  = have_sz ? (uint32_t)sz : 0;
  s_last_act_ms = millis();
  if (have_sz) {
    BTLOG("[BTUP] phien moi tu %s: %s (%lu B)\n", s_peer_mac, s_name, (unsigned long)s_declared);
    bt_send("OK %s %lu\n", s_name, (unsigned long)s_declared);
  } else {
    BTLOG("[BTUP] phien moi tu %s: %s (nhan kieu KHONG BIET TRUOC - ket thuc khi im 3 s)\n",
          s_peer_mac, s_name);
    bt_send("OK %s *\n", s_name);
  }
  ov_set("DANG NHAN QUA BT", s_name, "", "", true);
  if (!open_temp()) { fail_session(); return; }

  // Byte di kem SAU dong header trong buffer sniff (neu co) -> dua vao file luon.
  const size_t rest = (s_sniff_len > nl + 1u) ? (s_sniff_len - (nl + 1u)) : 0;
  if (rest) {
    const size_t allow = (!have_sz || rest <= s_declared) ? rest : s_declared;
    s_rx += (uint32_t)allow;
    if (allow && !stage_push(s_sniff + nl + 1u, allow)) { fail_session(); return; }
  }
  s_next_prog = PANEL_BT_PROG_STEP;
  s_state = ST_RECV;
  s_state_ms = millis();
}

// Kieu gui THO (khong header) KHONG the mang token => mac dinh KHONG cho (PANEL_BT_ALLOW_RAW=0).
// Bat RAW = bo lop token, chi nen bat khi that can (co canh bao luc khoi dong).
void sniff_raw_or_reject(void) {
  if (PANEL_BT_ALLOW_RAW) { start_raw(); return; }
  BTLOG("[BTUP] TU CHOI phien: kieu gui THO khong duoc phep (thieu token) (tu %s)\n", s_peer_mac);
  reject_token();
}

void st_idle(void) {
  wifi_stop_idle();                       // ranh => tat WiFi, tra heap + nhuong radio cho BT
  if (ring_used() == 0) return;
  if (millis() < s_discard_until) { ring_discard(); return; }

  if (printer_busy()) { bt_send("ERR dang in\n"); s_state = ST_DRAIN; s_state_ms = millis(); return; }
  // Che do nhan file co the bi khoa bang panel_bt_upload_set_enabled(false) (mac dinh la BAT)
  if (!s_enabled) {
    BTLOG("[BTUP] TU CHOI phien: che do nhan file dang KHOA (tu %s)\n", s_peer_mac);
    bt_send("ERR chua bat che do nhan file\n");
    s_state = ST_DRAIN;
    s_state_ms = millis();
    return;
  }
  // The TF phai san sang truoc khi nhan: thu mount NGAY (nguoi dung co the vua cam the vao)
  if (!s_sd_ready) {
    sd_mount(true);
    if (!s_sd_ready) {
      err_set("the TF chua san sang");
      BTLOG("[BTUP] phien moi bi tu choi: the TF chua san sang (khong mount duoc)\n");
      bt_send("ERR %s\n", s_err);
      s_state = ST_DRAIN;
      s_state_ms = millis();
      return;
    }
  }
  if (ESP.getFreeHeap() < PANEL_BT_HEAP_MIN) {
    err_setf("heap thap (%u B)", (unsigned)ESP.getFreeHeap());
    BTLOG("[BTUP] %s -> tu choi phien moi\n", s_err);
    bt_send("ERR %s\n", s_err);
    s_state = ST_DRAIN;
    s_state_ms = millis();
    return;
  }

  s_sniff_len = 0;
  s_rx = 0;
  s_stage_len = 0;
  s_have_size = false;
  s_declared = 0;
  s_err[0] = '\0';
  s_ov_ms = 0;
  rate_reset();
  s_drop = 0;
  s_heap_min = ESP.getFreeHeap();        // moc heap luc bat dau phien (log in heapmin=)
  s_state_ms = millis();
  ov_create(false);
  ov_set("NHAN QUA BT...", "?", "", "", true);
  s_state = ST_SNIFF;
}

void st_sniff(void) {
  s_sniff_len += ring_take(s_sniff + s_sniff_len, SNIFF_MAX - s_sniff_len);

  // 1) 4 byte dau co dung "PUT " khong? (khong phan biet hoa/thuong) -> neu khong: du lieu tho
  const size_t chk = (s_sniff_len < 4u) ? s_sniff_len : 4u;
  for (size_t i = 0; i < chk; i++) {
    char c = (char)s_sniff[i];
    if (c >= 'a' && c <= 'z') c = (char)(c - 32);
    if (c != "PUT "[i]) { sniff_raw_or_reject(); return; }
  }
  // 2) du "PUT " -> cho het dong
  if (s_sniff_len >= 4u) {
    for (size_t i = 0; i < s_sniff_len; i++) {
      if (s_sniff[i] == '\n') { start_header(i); return; }
    }
  }
  if (s_sniff_len >= SNIFF_MAX) { sniff_raw_or_reject(); return; }   // dong qua dai -> coi la du lieu
  if (millis() - s_state_ms > PANEL_BT_SNIFF_MS) {
    if (s_sniff_len == 0) { s_state = ST_IDLE; return; }
    err_set("header qua thoi gian");
    fail_session();
  }
}

void st_recv(void) {
  if (s_drop) { err_set("mat du lieu BT (vong dem day)"); fail_session(); return; }

  // Lay du 1 khoi 4 KB (hoac het phan con lai theo header) roi ghi xuong the.
  for (;;) {
    size_t want = SD_CHUNK - s_stage_len;
    if (s_have_size) {
      const uint32_t left = s_declared - s_rx;
      if (want > left) want = left;
    }
    if (want == 0) break;
    const size_t n = ring_take(s_stage + s_stage_len, want);
    if (n == 0) break;
    s_stage_len += n;
    s_rx += (uint32_t)n;
    if (s_stage_len == SD_CHUNK && !stage_flush()) { fail_session(); return; }
    if (s_have_size && s_rx >= s_declared) break;
  }

  if (s_rx >= s_next_prog) {
    bt_send("PROG %lu\n", (unsigned long)s_rx);
    s_next_prog += PANEL_BT_PROG_STEP;
  }
  rate_tick(s_rx);
  ov_phase1("DANG NHAN QUA BT");

  if (s_have_size) {
    if (s_rx >= s_declared) { finish_recv(); return; }
    // App ngat BT (hoac mat song) giua pha 1: khong the biet con thieu bao nhieu byte -> LOI,
    // xoa file tam (khong de file thieu lot len the SD cua Marlin).
    if (!bt_alive()) { err_set("BT mat ket noi giua pha 1"); fail_session(); return; }
    if (millis() - s_bt_rx_ms > PANEL_BT_STALL_MS) {
      err_setf("thieu du lieu (%lu/%lu B)", (unsigned long)s_rx, (unsigned long)s_declared);
      fail_session();
    }
    return;
  }
  // Kieu RAW / header khong so byte: cung coi ngat BT giua chung la LOI (an toan hon la chap
  // nhan mot file cut lam file in; doi lai app phai giu ket noi toi khi xong - xem README 9.3).
  if (!bt_alive()) { err_set("BT mat ket noi giua pha 1"); fail_session(); return; }
  // Kiem dung luong the dinh ky khi KHONG biet truoc kich thuoc (chua 512 KB) - tranh day the
  // am tham: cu moi 64 khoi (~128 KB) hoi FATFS mot lan.
  if ((++s_space_tick & 0x3Fu) == 0u) {
    const uint64_t total = SD.totalBytes(), used = SD.usedBytes();
    const uint64_t freeb = (total > used) ? (total - used) : 0;
    if (freeb < (512ull * 1024ull)) {
      err_set("the TF panel sap het cho");
      fail_session();
      return;
    }
  }
  // Ket thuc khi khong con byte nao trong 3 giay (PANEL_BT_IDLE_MS)
  if (ring_used() == 0 && (millis() - s_bt_rx_ms) >= PANEL_BT_IDLE_MS) {
    if (!stage_flush()) { fail_session(); return; }
    finish_recv();
  }
}

void st_sdwait(void) {
  if (millis() - s_sdinit_ms < PANEL_BT_SDWAIT_MS) return;
  // Marlin xu ly M22/M21 tuan tu trong hang doi lenh -> sau ~800 ms moi xin lai danh sach.
  // Xoa dem truoc khi xin: thu muc chi con file cua phien truoc thi Marlin chi gui frame ket thuc.
  panel.files_done = false;
  panel.n_files = 0;
  panel_link_request((uint8_t)PANEL_REQ_FILES);
  del_temp("xong");
  if (s_ov) ov_note("DONE", true);
  result_try_send();                       // DONE ... (gui ngay neu app con ket noi; khong thi luu lai)
  s_state_ms = millis();
  s_note_ms = millis();
  s_state = ST_NOTE;
}

void st_note(void) {
  if (ring_used() > 0) { ov_destroy(); s_state = ST_IDLE; return; }   // phien moi bat dau
  if (millis() - s_note_ms >= PANEL_BT_NOTE_MS) { ov_destroy(); s_state = ST_IDLE; }
}

void st_drain(void) {
  ring_discard();
  if (millis() - s_state_ms >= PANEL_BT_IDLE_MS && millis() - s_bt_rx_ms >= PANEL_BT_IDLE_MS) {
    s_note_ms = millis();
    s_state = ST_NOTE;                       // giu thong bao loi tren man them PANEL_BT_NOTE_MS
  }
}

const char *state_name(const uint8_t s) {
  switch (s) {
    case ST_IDLE:   return "idle";
    case ST_SNIFF:  return "sniff";
    case ST_RECV:   return "recv";
    case ST_POST:   return "post";
    case ST_SDWAIT: return "sdinit";
    case ST_NOTE:   return "note";
    case ST_DRAIN:  return "drain";
    default:        return "?";
  }
}

// 1 dong log 1 Hz (main.cpp goi qua panel_bt_upload_log()). heapmin = heap thap nhat KE TU luc bat
// dau phien gan nhat -> sau khi thu 1 file, doc dong nay la biet muc heap te nhat (ke ca trong
// pha 2 blocking, vi heap_track() duoc goi trong vong lap day file).
// Guard CA ham (khong chi guard than ham): neu chi guard than thi khi tat PANEL_CYD_DEBUG ham se
// "defined but not used" -> warning.
#if defined(PANEL_CYD_DEBUG)
void heartbeat_log(void) {
  heap_track();
  const uint32_t heap = ESP.getFreeHeap();
  lv_mem_monitor_t m;
  lv_mem_monitor(&m);
  Serial.printf("[BTUP] bt=%d wifi=%u(st=%d) sd=%d st=%s rx=%lu B posted=%lu B %u.%u KB/s "
                "ring=%lu/%lu drop=%lu heap=%lu heapmin=%lu btcyc=%u lvmem=%u/%u\n",
                (int)s_bt_ready, (unsigned)net_wifi_state(), (int)WiFi.status(),
                (int)s_sd_ready, state_name(s_state),
                (unsigned long)s_rx, (unsigned long)s_bytes_posted,
                (unsigned)(s_rate_x10 / 10u), (unsigned)(s_rate_x10 % 10u),
                (unsigned long)ring_used(), (unsigned long)RING_KEEP, (unsigned long)s_drop,
                (unsigned long)heap, (unsigned long)s_heap_min, (unsigned)s_bt_cycles,
                (unsigned)m.free_size, (unsigned)m.total_size);
  if (heap < PANEL_BT_HEAP_WARN)
    Serial.printf("[BTUP] !! CANH BAO HEAP THAP: %lu B < %lu B (xem README muc BT upload)\n",
                  (unsigned long)heap, (unsigned long)PANEL_BT_HEAP_WARN);
  if (s_drop)
    Serial.printf("[BTUP] !! MAT %lu byte tren vong dem BT (tang PANEL_BT_RING_BYTES)\n",
                  (unsigned long)s_drop);
}
#endif

}  // namespace

// ===========================================================================
// API cong khai (main.cpp + nhanh UI)
// ===========================================================================
void panel_bt_upload_begin(void) {
  // =========================================================================
  // THU TU KHOI DONG (xem README 9.10-9.13):
  //   1) Buffer lam viec (heap) truoc -> biet chac con bao nhieu RAM.
  //   2) The TF: mount + don file tam mo coi. Khong mount duoc thi VAN tiep tuc (the cam sau duoc -
  //      sd_poll() thu lai moi ~2,5 s, va phien se bi tu choi khi the chua san sang).
  //   3) Che do nhan file: mac dinh TAT (PANEL_BT_UPLOAD_AUTOSTART=0) => KHONG bat BT (tiet kiem
  //      ~100 KB heap). Bat bang panel_bt_upload_set_enabled(true) (nut UI / lenh console BTUP ON).
  //   4) KHONG bat WiFi o day (WiFi chi bat khi vao pha 2, sau khi da tat BT - README 9.11).
  // =========================================================================

  // 1) Buffer lam viec (heap)
  if (!buf_alloc()) {
    BTLOG("[BTUP] TAT tinh nang nhan file: khong du heap cho %u B vong dem + %u B khoi ghi "
          "(heap=%u B)\n", (unsigned)RING_BYTES, (unsigned)SD_CHUNK, (unsigned)ESP.getFreeHeap());
    return;
  }

  // 2) The TF + don rac
  if (sd_mount(true)) {
    clean_dir("luc khoi dong", true);
  } else {
    BTLOG("[BTUP] CANH BAO: chua mount duoc the TF luc khoi dong (cam the FAT32 roi doi ~2,5 s)\n");
    s_sd_retry_ms = millis();
  }

  // Thong tin bao mat (de doi chieu tren COM7)
  BTLOG("[BTUP] bao mat: token=%s, RAW=%s, PIN=%s%s, tu dong nhan file=%s\n",
        (PANEL_BT_TOKEN[0] != '\0') ? "BAT (PUT <token> <ten> <so_byte>)" : "TAT (khong khuyen nghi)",
        PANEL_BT_ALLOW_RAW ? "CHO PHEP (khong can token!)" : "khong cho",
        PANEL_BT_PIN, PANEL_BT_JUST_WORKS ? ", SSP Just Works" : "",
        PANEL_BT_UPLOAD_AUTOSTART ? "CO" : "KHONG (phai bat bang API/UI)");
  if (PANEL_BT_ALLOW_RAW && PANEL_BT_TOKEN[0] != '\0')
    BTLOG("[BTUP] !! CANH BAO: PANEL_BT_ALLOW_RAW=1 => kieu gui THO bo qua lop TOKEN\n");

  // 3) Che do nhan file
  if (PANEL_BT_UPLOAD_AUTOSTART) {
    panel_bt_upload_set_enabled(true);
  } else {
    s_enabled = false;
    BTLOG("[BTUP] che do nhan file DANG TAT -> khong bat BT (tiet kiem ~100 KB heap). "
          "Bat bang nut tren UI hoac lenh console 'BTUP ON'. The TF: %s\n",
          s_sd_ready ? "co" : "chua co");
  }
  s_state_ms = millis();
}

void panel_bt_upload_task(void) {
  wifi_poll();                     // cap nhat net_wifi_state() + thu ket noi lai (khong chan)
  sd_poll();                       // theo doi the TF nong (chi khi ranh) + tu mount lai
  result_try_send();               // gui lai DONE/ERR cua phien truoc khi app da noi lai
  if (!s_bt_ready) return;
  switch (s_state) {
    case ST_IDLE:   st_idle();   break;
    case ST_SNIFF:  st_sniff();  break;
    case ST_RECV:   st_recv();   break;
    case ST_SDWAIT: st_sdwait(); break;
    case ST_NOTE:   st_note();   break;
    case ST_DRAIN:  st_drain();  break;
    default:        s_state = ST_IDLE; break;      // ST_POST chi la moc trong luc chay blocking
  }
}

// Khoa/mo che do nhan file (khong co nut UI - de danh cho sau nay; mac dinh AUTOSTART=1 nen luon BAT).
// Tat => tat BT (tra ~100 KB) + NON-DISCOVERABLE cho toi khi bat lai.
void panel_bt_upload_set_enabled(bool on) {
  if (on) {
    if (s_enabled && s_bt_ready) return;
    s_enabled  = true;
    s_enable_ms = millis();
    s_last_act_ms = millis();
    if (!s_bt_ready && !bt_start()) {
      s_enabled = false;
      BTLOG("[BTUP] KHONG bat duoc che do nhan file (BT khong len duoc - xem log tren)\n");
      return;
    }
    if (s_bt_ready) bt_scan_mode(true, "bat che do nhan file");
    BTLOG("[BTUP] CHE DO NHAN FILE: BAT (the TF: %s)\n", s_sd_ready ? "co" : "CHUA CO");
    return;
  }
  const bool was = s_enabled;
  s_enabled = false;
  if (s_bt_ready) {
    bt_scan_mode(false, "khoa che do nhan file");
    bt_stop("khoa che do nhan file");
  }
  if (was) BTLOG("[BTUP] CHE DO NHAN FILE: TAM KHOA (BT da tat)\n");
}

// Hop dong voi nhanh UI: chi doc co da cache, khong chan, khong cap phat.
bool panel_bt_upload_enabled(void) {
  return s_enabled;
}

// Hop dong voi nhanh UI: chi doc bien da cache, khong goi WiFi.*, khong chan, khong cap phat.
uint8_t net_wifi_state(void) {
  return s_wifi_state;
}

// Hop dong voi nhanh UI: the TF con dung duoc khong (cache; khong SPI, khong chan).
bool panel_bt_upload_sd_ready(void) {
  return s_sd_ready;
}

// UI yeu cau thu mount lai the NGAY: chi bat co, viec that su chay trong panel_bt_upload_task().
void panel_bt_upload_sd_retry(void) {
  s_sd_retry_req = true;
  s_sd_retry_ms  = millis() - PANEL_SD_RETRY_MS;   // cho phep chay ngay o vong lap ke tiep
}

void panel_bt_upload_log(void) {
#if defined(PANEL_CYD_DEBUG)
  heartbeat_log();
#endif
}

bool panel_bt_upload_busy(void) {
  return s_state != ST_IDLE;
}

#else   // ---------- khong bat PANEL_BT_UPLOAD (hoac khong phai CYD): stub rong ----------

void panel_bt_upload_begin(void) {}
void panel_bt_upload_task(void) {}
void panel_bt_upload_log(void) {}
bool panel_bt_upload_busy(void) { return false; }

// 0 = khong co module / WiFi tat: nhanh UI ve icon trang thai "tat".
uint8_t net_wifi_state(void) { return 0; }

// Khong co module nhan file => luon bao "chua co the TF dung duoc".
bool panel_bt_upload_sd_ready(void) { return false; }
void panel_bt_upload_sd_retry(void) {}

// Khong co module: coi nhu KHONG bat che do nhan file (UI se ve nut o trang thai tat).
void panel_bt_upload_set_enabled(bool on) { (void)on; }
bool panel_bt_upload_enabled(void) { return false; }

#endif
