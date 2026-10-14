/**
 * cyd_sd.cpp -- TANG THE SD CUA MAN HINH CYD (vi sao + hop dong API: xem cyd_sd.h).
 *
 * GOM: mount the (leo thang toc do), doc/ghi sector tho 512 B cho Marlin qua UART, ghi file tam cho
 * web server WiFi, doi ten file tam vao GOC the, xoa file, bo dem sector + log chan doan 1 Hz.
 *
 * Duong POST file len module ESP3D va duong upload qua Bluetooth da bi XOA (2026-10-14) nen file nay
 * KHONG con goi WiFiClient/HTTPClient.
 */
#include "cydlcd_config.h"

// Can cho CA nhanh stub (khi CYDLCD_WIFI_UPLOAD = 0: khong include Arduino.h/WiFi.h gi ca).
// Include header truoc `#if` de stub co DUNG linkage C nhu khai bao (neu khong, ten bi mang hoa
// -> main.cpp/ net_upload_http.cpp goi khong thay).
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "cyd_sd.h"

#if CYDLCD_WIFI_UPLOAD

#include <Arduino.h>
#include <WiFi.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#include "cyd_sd.h"
#include "net_wifi.h"
#include "cydlcd_link.h"
#include "ui_lvgl.h"          // UI_LCD_HOR_RES / UI_LCD_VER_RES

#if defined(CYDLCD_CYD_DEBUG)
  #define ELOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define ELOG(...)   do { } while (0)
#endif

namespace {

// Mau lop phu: lay DUNG gia tri cua ui_lvgl_int.h (MR_BG/MR_PROGBG/MR_PROGBAR/MR_FRAME/MR_RATE100)
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
uint32_t s_written = 0;                 // byte da ghi vao the (phien gan nhat)
char     s_result[48] = "";            // "DONE 1234" / "ERR ..."

// ---- lop phu (pha 2: toan man hinh + khoa UI) ----
lv_obj_t *ov = nullptr, *ov_state = nullptr, *ov_info = nullptr, *ov_bar = nullptr;
char      txt_state[48], txt_info[48];
uint32_t  ov_ms = 0, rate_ms = 0, rate_base = 0, rate_x10 = 0;
bool      s_busy = false;              // dang ghi vao the (de /status bao "writing")

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

// Marlin dang in? (khong cho nhan file khi may dang in - 2 nguoi ghi cung 1 FAT la khong an toan)
bool printer_busy(void) {
  if (cydlcd.flags & CYDLCD_SF_SD_PRINTING) return true;
  switch (cydlcd.state) {
    case CYDLCD_ST_PRINTING:
    case CYDLCD_ST_PAUSED:
    case CYDLCD_ST_HOMING:
    case CYDLCD_ST_LEVELING:
    case CYDLCD_ST_RUNOUT:
    case CYDLCD_ST_KILLED:
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
  lv_obj_t *title = ov_label(ov, "TBT-CYDLCD", OV_FILL, nullptr);
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
#if CYDLCD_SD_SHARED
// Khai bao truoc (dinh nghia day du o duoi, sau sd_lost) - sd_mount() can sd_mux_panel().
void sd_mux_panel(void);
void sd_mux_marlin(void);
void sd_release_to_marlin(bool reinit_marlin);
#endif
bool sd_mount(const bool verbose) {
  if (s_sd_ready) return true;
#if CYDLCD_SD_SHARED
  sd_mux_panel();                       // gianh bus SPI the ve phia ESP32 TRUOC khi mount
#endif
  SPI.begin(CYDLCD_SD_SCK, CYDLCD_SD_MISO, CYDLCD_SD_MOSI, CYDLCD_SD_CS);
  const uint32_t hz_try[3] = { CYDLCD_SD_HZ, CYDLCD_SD_HZ / 2u, 4000000u };
  for (uint8_t i = 0; i < 3u; i++) {
    if (verbose) ELOG("[UP] thu mount the TF @ %u Hz ...\n", (unsigned)hz_try[i]);
    if (SD.begin(CYDLCD_SD_CS, SPI, hz_try[i], "/sd", 4)) {
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
         "the phai FAT32, khe TF/bus VSPI 18/19/23/5.\n", (int)CYDLCD_SD_CS);
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

#if CYDLCD_SD_UART
// ===========================================================================
// DONG BO FATFS <-> GHI THO (P3): Marlin ghi sector truc tiep len the (M28/M29/M30/PLR) nen cache
// cua FATFS phia man hinh CYD co the LECH. Hai co:
//   s_fatfs_stale   = Marlin vua ghi tho -> truoc thao tac FATFS ke tiep phai MOUNT LAI (doc lai FAT)
//   s_fatfs_written = man hinh CYD vua ghi qua FATFS -> truoc khi cho Marlin ghi tho phai day xuong the
// ===========================================================================
bool s_fatfs_stale   = false;
bool s_fatfs_written = false;

// Bo dem phuc vu Marlin (de KIEM CHUNG driver SD qua UART: doc bao nhieu sector, bao nhieu loi)
uint32_t s_raw_rd = 0, s_raw_rd_err = 0, s_raw_wr = 0, s_raw_wr_err = 0;
uint32_t s_raw_rd_refused = 0;          // so lan TU CHOI doc tho vi dang nhan/ghi upload (xem cyd_sd_raw_read)

void fatfs_sync(void) {
  if (!s_sd_ready) return;
  SD.end();                        // unmount: flush het cache + dong volume
  s_sd_ready = false;
  s_sd_was   = false;
  if (sd_mount(false)) {           // mount lai -> doc FAT/dir MOI tu the
    s_fatfs_stale = false;
    s_fatfs_written = false;
    ELOG("[UP] mount lai the de dong bo FATFS (sau khi Marlin ghi tho)\n");
  }
}
#endif  // CYDLCD_SD_UART

// ===========================================================================
// Bao Marlin "the SD vua thay doi" (man hinh CYD vua ghi/xoa file) -> M22 (nha the) + M21 (mount lai) +
// xin lai danh sach file. Dung cho CA HAI che do:
//   - MUX (CYDLCD_SD_SHARED): sau khi tra bus SPI ve Marlin;
//   - UART (CYDLCD_SD_UART): sau khi man hinh CYD ghi xong file (Marlin doc sector qua UART, cache FAT
//     cua Marlin phai duoc lam moi).
// ===========================================================================
void notify_media_changed(void) {
  ELOG("[UP] bao Marlin mount lai the (M22/M21) + xin lai danh sach file\n");
  cydlcd_link_send_gcode2("M22", "M21");
  const uint32_t t0 = millis();
  while (millis() - t0 < CYDLCD_HTTP_SDWAIT_MS) {
    lv_timer_handler();
    cydlcd_link_task();
    delay(10);
  }
  cydlcd.files_done = false;
  cydlcd.n_files = 0;
  cydlcd_link_request((uint8_t)CYDLCD_REQ_FILES);
}

#if CYDLCD_SD_SHARED
// ===========================================================================
// MUX SPI (74HC4053): the SD nam o khe CYD, HAI master thay nhau (man hinh CYD <-> Marlin)
//   Xem cydlcd_config.h (khoi CYDLCD_SD_SHARED) de biet so do chan. Nguyen tac BAT BUOC:
//   - Muc MAC DINH khi man hinh CYD khong dung = MARLIN (co dien tro 10k keo chan chon xuong GND).
//   - Panel chi doi sang minh NGAY TRUOC khi mount va tra lai NGAY sau khi ghi xong.
//   - KHONG BAO GIO gianh bus khi Marlin dang in / dang tam dung (xem printer_busy()).
// ===========================================================================
void sd_mux_panel(void) {
  digitalWrite(CYDLCD_SD_MUX_PIN, CYDLCD_SD_MUX_CYDLCD_LEVEL);
  delay(CYDLCD_SD_MUX_SETTLE_MS);          // cho 4053 + the on dinh truoc khi clock
}

void sd_mux_marlin(void) {
  digitalWrite(CYDLCD_SD_MUX_PIN, CYDLCD_SD_MUX_MARLIN_LEVEL);
}

// Tra bus SPI ve Marlin. `reinit_marlin` = gui M22+M21 de Marlin mount lai the (bat buoc sau khi
// man hinh CYD VUA GHI vao the: FAT cua Marlin dang cache trang thai cu) roi xin lai danh sach file.
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
#endif  // CYDLCD_SD_SHARED

// Xoa moi file *.tmp trong /cydlcd_tmp (don file mo coi sau reset / mat dien / loi)
uint16_t clean_dir(const char *why, const bool verbose) {
  if (!s_sd_ready) return 0;
  if (!SD.exists(CYDLCD_UPLOAD_TMP_DIR)) {
    SD.mkdir(CYDLCD_UPLOAD_TMP_DIR);
    return 0;
  }
  File dir = SD.open(CYDLCD_UPLOAD_TMP_DIR);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return 0;
  }
  uint16_t n = 0;
  uint64_t freed = 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const bool   isdir = f.isDirectory();
    const size_t sz    = f.size();
    const String nm    = f.name();          // ESP32 core: ten DAY DU dang "/cydlcd_tmp/x.tmp"
    char path[64];
    if (nm.length() && nm[0] == '/') snprintf(path, sizeof(path), "%s", nm.c_str());
    else                             snprintf(path, sizeof(path), "%s/%s", CYDLCD_UPLOAD_TMP_DIR, nm.c_str());
    f.close();                              // dong TRUOC khi xoa (an toan tren FATFS)
    const size_t ln = strlen(path);
    if (!isdir && ln > 4u && strcmp(path + ln - 4u, ".tmp") == 0) {
      if (SD.remove(path)) { n++; freed += sz; }
      else ELOG("[UP] CANH BAO: khong xoa duoc file tam %s\n", path);
    }
  }
  dir.close();
  if (n || verbose)
    ELOG("[UP] don %s (%s): xoa %u file .tmp (%lu KB)\n", CYDLCD_UPLOAD_TMP_DIR, why, (unsigned)n,
         (unsigned long)(freed / 1024ull));
  return n;
}

// ---- Ten file dich (da lam sach) cua phien dang chay ----
char s_name[40] = "";

}  // namespace

// ===========================================================================
// API: the TF + ket qua phien
// ===========================================================================
bool cyd_sd_ready(void) {
#if CYDLCD_SD_SHARED
  // THE DUNG CHUNG: man hinh CYD khong mount the khi ranh (bus thuoc Marlin) -> "the co san" nghia la
  // MARLIN bao co the (co CYDLCD_SF_MEDIA trong frame PUSH). Dung cho ca icon STATUS lan /status.
  return (cydlcd.flags & CYDLCD_SF_MEDIA) != 0;
#else
  return s_sd_ready;
#endif
}

void cyd_sd_retry(void) {
  s_sd_retry_req = true;
  s_sd_retry_ms = millis() - CYDLCD_SD_RETRY_MS;    // cho phep chay ngay o vong lap ke tiep
}

bool cyd_sd_mount_now(void) { return sd_mount(true); }

bool cyd_sd_printer_busy(void) { return printer_busy(); }

// ===========================================================================
// API: DOC/GHI SECTOR THO + XOA FILE (che do CYDLCD_SD_UART = "may chu the SD" cho Marlin)
//   Marlin khong tu doc SPI nua: moi lan SdVolume can 1 sector 512 B, cydlcd_link.cpp goi vao day
//   roi tra ket qua qua frame CYDLCD_F_SDDATA.
// ===========================================================================
bool cyd_sd_raw_read(const uint32_t sector, uint8_t *dst) {
#if CYDLCD_SD_UART
  if (!s_sd_ready || !dst) return false;
  // ---------------------------------------------------------------------------
  // TU CHOI DOC THO KHI MAN HINH DANG NHAN/GHI FILE UPLOAD (`s_file` dang mo).
  // VI SAO (loi that 2026-10-15: "gui file 4,3 MB qua WiFi bi dut ket noi giua chung"):
  //   Trong luc nhan upload, man hinh ghi file tam qua FATFS (cyd_sd_temp_write -> SD.write) MA
  //   Marlin VAN doc the qua UART (moi lan liet ke file / kiem tra the -> hang tram sector). Hai
  //   duong cung dung MOT driver SD cua ESP32 -> FATFS dang giu phien ghi thi SD.readRAW() lam hong
  //   phien do: `SD.write()` tra ve thieu byte -> "ghi the TF loi" -> driver SD con bi treo vai giay
  //   (vong lap chinh khong chay 15 s) -> watchdog chan doan cua man hinh TU KHOI DONG LAI -> PC thay
  //   "ket noi bi dut giua chung". Da do duoc: upload 2 MB (trang dang mo KHONG phai FILES) thi qua,
  //   con upload 4,3 MB (trang FILES dang mo -> Marlin liet ke lien tuc) thi chet o ~46%.
  //   Tra false => man hinh tra frame loi cho sector do; Marlin coi la loi doc the (KHONG the dang in
  //   vi phien upload da bi chan khi may ban) va se doc lai sau khi phien xong (cuoi phien man hinh
  //   gui M22+M21 -> Marlin mount lai).
  // ---------------------------------------------------------------------------
  if (s_file || s_busy) { s_raw_rd_refused++; return false; }
  // SD.readRAW(): doc THANG sector vat ly, khong qua FATFS (core ESP32 SD.cpp:124 -> sd_read_raw).
  // Doc lap voi FATFS dang mount: chi doc, khong dung cache cua FATFS.
  const bool ok = SD.readRAW(dst, sector);
  if (ok) s_raw_rd++; else { s_raw_rd_err++; ELOG("[UP] doc RAW sector %lu THAT BAI (the rut ra?)\n", (unsigned long)sector); }
  return ok;
#else
  (void)sector; (void)dst;
  return false;
#endif
}

bool cyd_sd_raw_write(const uint32_t sector, const uint8_t *src) {
#if CYDLCD_SD_UART
  if (!s_sd_ready || !src) return false;
  // DONG BO TRUOC KHI GHI THO: Marlin ghi thang len the (FAT/dir), con FATFS cua man hinh CYD co cache
  // rieng -> phai day het du lieu dang cho xuong the truoc, neu khong cache cu se de len.
  if (s_file) s_file.flush();                    // dang nhan upload: day file + FAT xuong the
  else if (s_fatfs_written) { fatfs_sync(); if (!s_sd_ready) return false; }
  const bool ok = SD.writeRAW((uint8_t *)src, sector);   // core ESP32: sd_write_raw() (SD.h/SD.cpp)
  if (ok) { s_raw_wr++; s_fatfs_stale = true; }   // FATFS cua cydlcd gio co the LECH -> mount lai truoc
  else    { s_raw_wr_err++; ELOG("[UP] ghi RAW sector %lu THAT BAI\n", (unsigned long)sector); }
  return ok;
#else
  (void)sector; (void)src;
  return false;
#endif
}

bool cyd_sd_delete(const char *name) {
#if CYDLCD_SD_UART
  if (!s_sd_ready || !name || !name[0]) return false;
  // Dang in thi KHONG xoa (Marlin dang doc the qua UART; xoa file khac thi duoc nhung de an toan
  // va don gian, tu choi luon).
  if (printer_busy()) { ELOG("[UP] TU CHOI xoa file: Marlin dang in\n"); return false; }
  // LUON mount lai FATFS truoc khi XOA: Marlin ghi the bang SECTOR THO (cyd_sd_raw_write) nen cache
  // FATFS phia man hinh co the LECH; co `s_fatfs_stale` chi duoc dat khi di qua dung cyd_sd_raw_write
  // nen khong bao quat het moi duong Marlin ghi. DA GAP THAT (2026-10-15): `SD.open("/BTUP")` MO DUOC
  // (thu muc CO tren the) nhung `UI TAP L` (readdir goc the) KHONG thay no, va van thay file da xoa.
  fatfs_sync();
  if (!s_sd_ready) return false;
  char path[80];
  snprintf(path, sizeof(path), "/%s", name);
  if (!SD.exists(path)) { ELOG("[UP] khong thay file de xoa: %s\n", path); return false; }
  const bool ok = SD.remove(path);
  if (ok) s_fatfs_written = true;           // P3: vua ghi FATFS -> phai flush truoc khi Marlin ghi tho
  ELOG("[UP] %s file %s (cydlcd tu xoa - che do the qua UART)\n",
       ok ? "da xoa" : "KHONG xoa duoc", path);
  // LUU Y: KHONG goi notify_media_changed() o day - ham do cho ~800 ms (M22/M21 + xin danh sach),
  // goi tu callback nut bam se treo LVGL. Nguoi goi (ui_page_files.cpp) tu lo phan do bang files_init_sd().
  return ok;
#else
  (void)name;
  return false;
#endif
}

// Dem so MUC CON cua 1 thu muc o goc the. ESP32 VFS da bo qua "." va ".." khi liet ke (xem clean_dir()
// o tren - vong lap do khong phai tu loc gi) nen chi can dem. -1 = khong mo duoc thu muc.
int cyd_sd_dir_count(const char *name) {
#if CYDLCD_SD_UART
  if (!s_sd_ready || !name || !name[0]) return -1;
  // LUON mount lai FATFS truoc khi DOC trang thai thu muc (xem giai thich o cyd_sd_delete()).
  fatfs_sync();
  if (!s_sd_ready) return -1;
  char path[80];
  snprintf(path, sizeof(path), "/%s", name);
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return -1;
  }
  int n = 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) n++;
  dir.close();
  return n;
#else
  (void)name;
  return -1;
#endif
}

// Xoa 1 THU MUC RONG o goc the. VI SAO KHONG DUNG M30 CUA MARLIN: `M30` -> CardReader::removeFile()
// -> SdBaseFile::remove() -> truncate(0) - ham nay KHONG xoa duoc thu muc (chinh chu thich trong
// Marlin/src/sd/SdBaseFile.cpp: "Reasons for failure include the file read-only, is a directory").
// The dang o khe MAN HINH CYD (CYDLCD_SD_UART) nen man hinh tu xoa bang FATFS: ESP32 VFS `remove()`
// goi rmdir() khi duong dan la thu muc, va rmdir chi xoa duoc thu muc RONG.
bool cyd_sd_delete_dir(const char *name, bool *not_empty) {
  if (not_empty) *not_empty = false;
#if CYDLCD_SD_UART
  if (!s_sd_ready || !name || !name[0]) return false;
  if (printer_busy()) { ELOG("[UP] TU CHOI xoa thu muc: Marlin dang in\n"); return false; }
  char path[80];
  snprintf(path, sizeof(path), "/%s", name);
  // Dem TRUOC khi xoa: nho vay bao duoc DUNG ly do ("con N muc con") thay vi chi "khong xoa duoc",
  // va KHONG bao gio cham vao thu muc con du lieu.
  const int cnt = cyd_sd_dir_count(name);
  if (cnt < 0) { ELOG("[UP] khong mo duoc thu muc de xoa: %s\n", path); return false; }
  if (cnt > 0) {
    if (not_empty) *not_empty = true;
    ELOG("[UP] KHONG xoa thu muc %s: con %d muc con (chi xoa duoc thu muc RONG)\n", path, cnt);
    return false;
  }
  // PHAI dung SD.rmdir(), KHONG duoc dung SD.remove(): VFSImpl::remove() TU CHOI thu muc
  // ("%s does not exists or is directory" - FS/src/vfs_api.cpp), con VFSImpl::rmdir() moi goi
  // rmdir() cua VFS FATFS -> f_unlink (FatFs: xoa duoc thu muc RONG). Da gap that: SD.remove() tra
  // false lam man hinh bao "Khong xoa duoc thu muc" du thu muc da RONG.
  const bool ok = SD.rmdir(path);
  if (ok) s_fatfs_written = true;           // P3: vua ghi FATFS -> phai flush truoc khi Marlin ghi tho
  ELOG("[UP] %s thu muc RONG %s (cydlcd tu xoa - che do the qua UART)\n",
       ok ? "da xoa" : "KHONG xoa duoc", path);
  return ok;
#else
  (void)name;
  return false;
#endif
}

const char *cyd_sd_last_result(void) { return s_result; }
uint32_t    cyd_sd_last_written(void) { return s_written; }
const char *cyd_sd_temp_path(void) { return s_tmp; }
uint32_t    cyd_sd_temp_bytes(void) { return s_got; }

// CHAN DOAN: liet ke GOC the bang FATFS cua man hinh (`UI TAP L` tren COM7).
// VI SAO CAN: frame CYDLCD_F_FILES mang ten trong 1 truong CO DINH 32 byte (CYDLCD_FILE_NAME_LEN) nen
// ten dai hon se bi CAT; Marlin cung chi giu LFN trong `LONG_FILENAME_LENGTH` byte. Neu ten hien tren
// man hinh khac ten THAT tren the thi `M23 <ten>` se khong mo duoc file (bam PRINT khong in duoc) -
// ham nay in ten DAY DU doc thang tu FATFS de doi chieu.
void cyd_sd_log_dir(const char *path) {
#if CYDLCD_SD_UART
  if (!s_sd_ready) { ELOG("[UP] goc the: chua co the\n"); return; }
  // LUON mount lai FATFS truoc khi LIET KE (xem giai thich o cyd_sd_delete()).
  fatfs_sync();
  if (!s_sd_ready) { ELOG("[UP] goc the: chua co the\n"); return; }
  if (!path || !path[0]) path = "/";
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    ELOG("[UP] goc the: khong mo duoc\n");
    return;
  }
  uint16_t n = 0;
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const String nm = f.name();                       // ESP32 core: ten DAY DU, dang "/ten"
    const char  *s  = nm.c_str();
    ELOG("[UP] goc the %2u: %s %-34s %lu B (%u ky tu)\n", (unsigned)(n + 1),
         f.isDirectory() ? "[D]" : "   ", s, (unsigned long)f.size(),
         (unsigned)strlen(s > s ? s + 1 : s));        // bo dau '/' dau tien khi dem do dai
    n++;
  }
  dir.close();
  ELOG("[UP] goc the: %u muc\n", (unsigned)n);
#else
  ELOG("[UP] goc the: che do the khong phai CYDLCD_SD_UART -> khong liet ke duoc\n");
#endif
}

// ===========================================================================
// API: ghi file tam
// ===========================================================================
bool cyd_sd_temp_open(const char *name, uint32_t size_hint) {
  s_err[0] = '\0';
  s_result[0] = '\0';
  // Chua in moi duoc ghi the (the dung chung voi Marlin) - lop chan thu 2 sau net_upload_http.
  if (printer_busy()) {
    snprintf(s_err, sizeof(s_err), "Marlin dang in - khong dung the");
    return false;
  }
#if CYDLCD_SD_UART
  // P3: Marlin vua ghi tho thi cache FATFS cua man hinh CYD co the lech -> mount lai truoc khi ghi file moi.
  if (s_fatfs_stale) fatfs_sync();
  // Che do the qua UART: man hinh CYD va Marlin KHONG tranh bus, nhung van can lien lac de con bao
  // M22/M21 sau khi ghi xong -> mat lien lac thi tu choi luon (tranh ghi file ma Marlin khong biet).
  if (!cydlcd.linked) {
    snprintf(s_err, sizeof(s_err), "chua lien lac duoc Marlin");
    ELOG("[UP] TU CHOI: %s\n", s_err);
    return false;
  }
#endif
#if CYDLCD_SD_SHARED
  // CHUA BIET trang thai Marlin (mat lien lac UART) => KHONG duoc gianh the: co the Marlin dang in
  // ma man hinh CYD khong biet, gianh bus SPI the giua luc in se lam hong ban in.
  if (!cydlcd.linked) {
    snprintf(s_err, sizeof(s_err), "chua lien lac duoc Marlin - khong gianh the");
    ELOG("[UP] TU CHOI: %s\n", s_err);
    return false;
  }
  // Neu Marlin dang liet ke danh sach file (do nguoi dung vua bam REFRESH) thi doi xong da (~<400 ms)
  // roi moi gianh bus, tranh cat ngang luc Marlin dang doc the.
  const uint32_t tw = millis();
  while (!cydlcd.files_done && (millis() - tw) < 400u) {
    lv_timer_handler();
    cydlcd_link_task();
    delay(5);
  }
#endif
#if CYDLCD_SD_UART
  // Che do the qua UART: man hinh CYD va Marlin KHONG tranh bus, nhung van can lien lac de con bao
  // M22/M21 sau khi ghi xong -> mat lien lac thi tu choi luon (tranh ghi file ma Marlin khong biet).
  if (!cydlcd.linked) {
    snprintf(s_err, sizeof(s_err), "chua lien lac duoc Marlin");
    ELOG("[UP] TU CHOI: %s\n", s_err);
    return false;
  }
#endif
  if (!sd_mount(true)) { snprintf(s_err, sizeof(s_err), "the TF cydlcd loi"); return false; }
#if CYDLCD_SD_SHARED || CYDLCD_SD_UART
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
    snprintf(s_err, sizeof(s_err), "the TF cydlcd het cho (%lu KB trong)",
             (unsigned long)(freeb / 1024ull));
    return false;
  }
  sanitize_name(name, s_name, sizeof(s_name));
  if (!SD.exists(CYDLCD_UPLOAD_TMP_DIR)) SD.mkdir(CYDLCD_UPLOAD_TMP_DIR);
  snprintf(s_tmp, sizeof(s_tmp), "%s/%s.tmp", CYDLCD_UPLOAD_TMP_DIR, s_name);
  if (SD.exists(s_tmp)) SD.remove(s_tmp);
  s_file = SD.open(s_tmp, FILE_WRITE);
  if (!s_file) {
    snprintf(s_err, sizeof(s_err), "khong tao duoc file tam");
    s_tmp[0] = '\0';
    return false;
  }
  s_got = 0;
  ELOG("[UP] ghi vao %s (ten dich: %s)\n", s_tmp, s_name);
  // ---------------------------------------------------------------------------
  // BAO MARLIN NHA THE (M22) TRONG SUOT PHIEN UPLOAD.
  // VI SAO: trong luc nhan file, man hinh ghi the qua FATFS; neu Marlin van doc the qua UART (moi lan
  // liet ke file / kiem tra the = hang tram sector) thi hai duong dung chung 1 driver SD -> phien ghi
  // FATFS bi hong ("ghi the TF loi") -> man hinh treo >15 s -> watchdog TU KHOI DONG LAI -> PC thay
  // "ket noi bi dut giua chung" (loi that 2026-10-15 voi file 4,3 MB, chet o ~46%).
  // M22 = card.release() -> Marlin dong volume, KHONG doc sector nua (board nay bat NO_SD_DETECT nen
  // Marlin khong tu mount lai). Cuoi phien, cyd_sd_finish_upload() -> notify_media_changed() gui
  // M22+M21 -> Marlin mount lai va thay file moi.
  // (Lop chan thu 2: cyd_sd_raw_read() cung TU CHOI sector trong luc s_file/s_busy - phong khi lenh
  //  M22 chua kip duoc Marlin xu ly.)
  // ---------------------------------------------------------------------------
  cydlcd_link_send_gcode("M22");
  ELOG("[UP] da bao Marlin NHA THE (M22) trong luc nhan file\n");
  return true;
}

size_t cyd_sd_temp_write(const uint8_t *d, const size_t n) {
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

uint32_t cyd_sd_temp_close(void) {
  if (s_file) {
    s_file.flush();
    s_file.close();
  }
#if CYDLCD_SD_UART
  s_fatfs_written = true;    // P3: cydlcd vua ghi qua FATFS -> truoc khi cho Marlin ghi tho phai flush
#endif
  return s_got;
}

void cyd_sd_temp_abort(void) {
  if (s_file) s_file.close();
  if (s_tmp[0] && s_sd_ready) {
    if (SD.exists(s_tmp)) SD.remove(s_tmp);
    ELOG("[UP] da xoa file tam %s (bo phien)\n", s_tmp);
  }
  s_tmp[0] = '\0';
  s_got = 0;
#if CYDLCD_SD_SHARED
  // Phien bi bo -> PHAI tra bus SPI the cho Marlin (khong thi Marlin mat the vinh vien).
  sd_release_to_marlin(false);
#endif
}

// ===========================================================================
// API: chot phien upload - doi ten file tam vao goc the (CHAN - co lop phu + khoa UI)
// ===========================================================================
bool cyd_sd_finish_upload(const char *name, uint32_t size, uint32_t *written, char *err, const size_t errn) {
  if (written) *written = 0;
  if (err && errn) err[0] = '\0';
  s_busy = true;
  s_written = 0;
  s_err[0] = '\0';
  if (name && name[0]) snprintf(s_name, sizeof(s_name), "%s", name);
  if (s_tmp[0] == '\0') snprintf(s_tmp, sizeof(s_tmp), "%s/%s.tmp", CYDLCD_UPLOAD_TMP_DIR, s_name);

  // -------------------------------------------------------------------------
  // CHE DO "THE CUA MAN HINH" (khong dung mang): file tam /cydlcd_tmp/<ten>.tmp da duoc ghi vao the
  // trong luc nhan HTTP (temp_open/temp_write). O day chi con:
  //   1) DOI TEN file tam -> /<ten> (goc the, cho Marlin nhin thay)
  //   2) Bao Marlin mount lai the (M22/M21) + xin lai danh sach file
  //      - MUX (CYDLCD_SD_SHARED): tra bus SPI cho Marlin TRUOC khi bao;
  //      - UART (CYDLCD_SD_UART): khong co bus de tra, chi bao Marlin (no doc sector qua UART).
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
      s_written = s_got;
      snprintf(s_result, sizeof(s_result), "DONE %lu", (unsigned long)s_got);
      ov_set("DA GHI VAO THE SD", s_name, true);
    } else {
      snprintf(s_err, sizeof(s_err), "khong doi ten duoc file tren the");
      if (SD.exists(s_tmp)) SD.remove(s_tmp);   // don file tam ngay (con dang giu bus)
      s_tmp[0] = '\0';
    }
  }
  ui_lock(false);
  #if CYDLCD_SD_SHARED
    sd_release_to_marlin(ok);             // tra bus (+ M22/M21 + xin lai danh sach neu OK)
  #else
    if (ok) notify_media_changed();       // the cua cydlcd: chi can bao Marlin mount lai
  #endif
  if (!ok) {
    snprintf(s_result, sizeof(s_result), "ERR %s", s_err);
    ov_set(s_err[0] ? s_err : "ghi the loi", "", false);
  }
  if (written) *written = s_written;
  if (err && errn) snprintf(err, errn, "%s", s_err[0] ? s_err : "");
  if (!s_result[0]) {
    snprintf(s_result, sizeof(s_result), "ERR %s", s_err[0] ? s_err : "loi khong ro");
    if (err && errn) snprintf(err, errn, "%s", s_err[0] ? s_err : "loi khong ro");
  }
  // Go lop phu sau 3 s (che do the dung chung: chi 1,5 s vi buoc ghi the gan nhu tuc thi) de
  // nguoi dung kip doc ket qua (khong chan vong lap: chi hen gio)
  const uint32_t t_note = millis();
#if CYDLCD_SD_SHARED
  const uint32_t note_ms = 1500u;
#else
  const uint32_t note_ms = 3000u;
#endif
  while (millis() - t_note < note_ms) {
    lv_timer_handler();
    cydlcd_link_task();
    delay(10);
  }
  ov_destroy();
  s_busy = false;
  return (s_result[0] == 'D' && s_result[1] == 'O');
}

// ===========================================================================
// Vong doi + log
// ===========================================================================
void cyd_sd_begin(void) {
#if CYDLCD_SD_SHARED
  // THE DUNG CHUNG: luc khoi dong the thuoc MARLIN -> man hinh CYD KHONG mount, chi dat chan chon MUX ve
  // phia Marlin. Panel chi gianh bus khi co phien upload (xem sd_mount/sd_release_to_marlin).
  pinMode(CYDLCD_SD_MUX_PIN, OUTPUT);
  digitalWrite(CYDLCD_SD_MUX_PIN, CYDLCD_SD_MUX_MARLIN_LEVEL);
  ELOG("[UP] the SD DUNG CHUNG voi Marlin qua MUX (chan chon = GPIO%d, muc mac dinh = MARLIN). "
       "Panel chi gianh bus khi nhan file va khong in.\n", (int)CYDLCD_SD_MUX_PIN);
#else
  if (sd_mount(true)) clean_dir("luc khoi dong", true);
  else ELOG("[UP] CANH BAO: chua mount duoc the TF (cam the FAT32 - se tu thu lai moi 2,5 s)\n");
#endif
}

void cyd_sd_task(void) {
#if CYDLCD_SD_SHARED
  // The dung chung: khi man hinh CYD ranh thi bus thuoc Marlin => TUYET DOI khong cham vao the (khong
  // kiem tra 1 Hz, khong mount lai). Trang thai the lay tu Marlin (co CYDLCD_SF_MEDIA trong PUSH).
  return;
#else
  const uint32_t now = millis();
  if (s_sd_ready) {
    if (now - s_sd_check_ms < CYDLCD_SD_CHECK_MS) return;
    s_sd_check_ms = now;
    if (!sd_alive()) sd_lost("kiem tra dinh ky 1 Hz");
    return;
  }
  if (s_sd_retry_req) s_sd_retry_req = false;
  else if (now - s_sd_retry_ms < CYDLCD_SD_RETRY_MS) return;
  s_sd_retry_ms = now;
  if (sd_mount(false)) clean_dir("mount lai", false);
#endif
}

void cyd_sd_log(void) {
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < s_heap_min) s_heap_min = heap;
  // Ten file tam dang co (chi ten, khong ca duong dan - dong log gon)
  const char *t = s_tmp[0] ? strrchr(s_tmp, '/') : nullptr;
#if CYDLCD_SD_SHARED
  // The dung chung: KHONG goi SD.totalBytes() khi chua mount (vua ton SPI vua vo nghia vi bus
  // dang thuoc Marlin). Trang thai the that su nam o co CYDLCD_SF_MEDIA cua Marlin.
  const uint32_t free_kb = 0;
  const int sd_state = (int)cyd_sd_ready();
#else
  const uint32_t free_kb = (SD.totalBytes() > SD.usedBytes())
                               ? (uint32_t)((SD.totalBytes() - SD.usedBytes()) / 1024ull) : 0u;
  const int sd_state = (int)s_sd_ready;
#endif
  Serial.printf("[UP] wifi=%u ssid=\"%s\" ip=%s rssi=%d sd=%d busy=%d rx=%lu written=%lu "
                "heap=%lu heapmin=%lu free=%luKB | file=%s | %s\n",
                (unsigned)net_wifi_state(), net_wifi_ssid(), net_wifi_ip(), (int)net_wifi_rssi(),
                sd_state, (int)s_busy, (unsigned long)s_got, (unsigned long)s_written,
                (unsigned long)heap, (unsigned long)s_heap_min, (unsigned long)free_kb,
                t ? t + 1 : "-", s_result[0] ? s_result : "-");
  // Trang thai MARLIN (tu frame PUSH) + bo dem sector phuc vu Marlin - de chan doan TU XA khi khong
  // nhin duoc man hinh:
  //   st = cydlcd_state_t (0 idle, 1 printing, 2 paused, 3 homing, 4 leveling, 5 killed...),
  //   fl = cydlcd_status_flags_t (bit1 CYDLCD_SF_MEDIA, bit2 CYDLCD_SF_SD_PRINTING, bit3 PAUSED),
  //   r=%lu/%lu = so sector DOC thanh cong / loi, w=%lu/%lu = so sector GHI thanh cong / loi.
  Serial.printf("[MRL] st=%u fl=0x%02X prog=%u%% file=\"%s\" | SD r=%lu/%lu w=%lu/%lu refused=%lu\n",
                (unsigned)cydlcd.state, (unsigned)cydlcd.flags, (unsigned)cydlcd.progress_pct,
                cydlcd.prog.file,
                (unsigned long)s_raw_rd, (unsigned long)s_raw_rd_err,
                (unsigned long)s_raw_wr, (unsigned long)s_raw_wr_err,
                (unsigned long)s_raw_rd_refused);
}

#else   // ---- CYDLCD_WIFI_UPLOAD = 0: stub rong (moi env van link duoc) ----

void cyd_sd_begin(void) {}
void cyd_sd_task(void) {}
void cyd_sd_log(void) {}
bool cyd_sd_ready(void) { return false; }
bool cyd_sd_mount_now(void) { return false; }
void cyd_sd_retry(void) {}
bool cyd_sd_temp_open(const char *name, uint32_t size_hint) {
  (void)name; (void)size_hint; return false;
}
size_t cyd_sd_temp_write(const uint8_t *d, const size_t n) { (void)d; (void)n; return 0; }
uint32_t cyd_sd_temp_close(void) { return 0; }
void cyd_sd_temp_abort(void) {}
const char *cyd_sd_temp_path(void) { return ""; }
uint32_t cyd_sd_temp_bytes(void) { return 0; }
bool cyd_sd_printer_busy(void) { return false; }
bool cyd_sd_raw_read(uint32_t sector, uint8_t *dst) { (void)sector; (void)dst; return false; }
bool cyd_sd_raw_write(uint32_t sector, const uint8_t *src) { (void)sector; (void)src; return false; }
bool cyd_sd_delete(const char *name) { (void)name; return false; }
int  cyd_sd_dir_count(const char *name) { (void)name; return -1; }
bool cyd_sd_delete_dir(const char *name, bool *not_empty) { (void)name; if (not_empty) *not_empty = false; return false; }
void cyd_sd_log_dir(const char *path) { (void)path; ELOG("[UP] goc the: che do the khong phai CYDLCD_SD_UART\n"); }
bool cyd_sd_finish_upload(const char *name, uint32_t size, uint32_t *written, char *err, size_t errn) {
  (void)name; (void)size; (void)written; (void)err; (void)errn;
  return false;
}
const char *cyd_sd_last_result(void) { return ""; }
uint32_t cyd_sd_last_written(void) { return 0; }

#endif  // CYDLCD_WIFI_UPLOAD
