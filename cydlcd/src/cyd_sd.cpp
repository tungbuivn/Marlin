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
uint32_t s_cleanup_ms = 0;             // moc lan don file tam dinh ky (CYDLCD_SD_CLEANUP_MS)
File     s_file;
char     s_tmp[72] = "";               // file DANG mo (1 PART, hoac file .tmp luc merge/doi ten)
uint32_t s_got = 0;                    // TONG byte da ghi vao the cua CA PHIEN (cong don qua cac part)
// ---- Kien truc PART (theo de xuat nguoi dung 2026-10-15) ----
// Moi phan file (UPLOAD_FILE_START..UPLOAD_FILE_END) duoc ghi vao MOT FILE RIENG roi DONG NGAY:
//   /cydlcd_tmp/<ten>_001.tmp, _002.tmp, ...
// Xong het request thi: 1 part -> doi ten thang thanh /<ten> (KHONG copy); >=2 part -> MERGE lai roi moi
// doi ten. VI SAO: du lieu cua part da xong nam CHAC tren the (da close) nen part sau loi/ngat khong lam
// mat part truoc; va khong con chuyen "mo lai file tam lam xoa du lieu da nhan".
uint32_t s_cur_bytes = 0;              // byte da ghi vao DUNG file dang mo (s_tmp)
uint8_t  s_part = 0;                   // so thu tu PART dang mo (1..)
uint8_t  s_parts_done = 0;             // so PART da DONG (da nam tren the)
// Dem ghi (write-back) cho upload: gom du CYDLCD_SD_WBUF byte roi moi ghi xuong the (xem sd_wbuf_flush).
// CAP PHAT DONG (khong phai mang static): RAM tinh (dram0) cua ESP32 da kin - mang 4 KB static lam
// linker bao "region dram0_0_seg overflowed by 304 bytes". Heap luc upload con ~130 KB nen cap phat
// khi mo file part va tra lai khi dong/bo phien la hoan toan on.
// 16 KB (32 sector/lan) thay vi 4 KB: moi phep ghi la 1 lan "co hoi" loi the/SPI -> giam so phep ghi 4 lan
// (4,5 MB: 276 lan thay vi 1104 lan) ma van can sector.
#define CYDLCD_SD_WBUF 16384
static uint8_t *s_wbuf = nullptr;
static uint16_t s_wbuf_n = 0;
// CHAN DOAN phien upload (2026-10-15) - de tra loi "co phai do multiple part dan den open/close file
// khac khong?": dem so lan MO file tam + moc thoi gian de bao tien do (KB/s) va heap.
static uint16_t s_opens = 0;          // so lan mo file tam tu luc khoi dong
static uint8_t  s_sess_opens = 0;     // so lan mo trong PHIEN dang chay (>=2 = bi mo lai giua phien!)
static uint32_t s_t0_ms = 0;          // moc bat dau phien (tinh KB/s)
static uint32_t s_log_mb = 0;         // da bao tien do den moc MB nao
static uint8_t  s_recover_ok = 0, s_recover_fail = 0;  // so lan PHUC HOI the thanh cong / that bai
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

// Marlin dang IN TU THE SD? (khong cho nhan file khi may dang in - 2 nguoi ghi cung 1 FAT la khong an toan)
//
// !!! CHI xet CO IN TU THE SD, KHONG xet trang thai may noi chung !!!
// VI SAO (loi nguoi dung bao 2026-10-15: "upload cung bi block voi ly do may in dang in, toi bam G35
// thoi chu co phai in dau?"): truoc day ham nay con chan khi `cydlcd.state` la PRINTING/HOMING/
// LEVELING/RUNOUT/KILLED. Nhung:
//   - G35 (Bed Tramming Wizard) chi HOME + PROBE -> Marlin bao HOMING/LEVELING (hoac PRINTING qua
//     `isPrinting()` vi print_job_timer dang chay) trong khi KHONG he doc the SD;
//   - cac trang thai do KHONG dung the SD -> ghi file upload tu man hinh la AN TOAN.
// Chi khi Marlin dang IN TU THE SD (`CYDLCD_SF_SD_PRINTING`: Marlin doc the lien tuc) hoac dang TAM
// DUNG ban in tu the (`CYDLCD_SF_PAUSED`) thi moi that su co 2 ben cung dung 1 FAT -> phai chan.
// (Ngoai ra `cyd_sd_temp_open()` con gui M22 cho Marlin nha the trong suot phien upload, va
//  `cyd_sd_raw_read()` tu choi sector khi dang ghi -> lop chan an toan thu 2.)
bool printer_busy(void) {
  return (cydlcd.flags & (CYDLCD_SF_SD_PRINTING | CYDLCD_SF_PAUSED)) != 0;
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
uint32_t s_raw_wr_refused = 0;          // so lan TU CHOI ghi tho vi dang nhan/ghi upload (xem cyd_sd_raw_write)

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

// Xoa cac file *.tmp trong /cydlcd_tmp (don file mo coi sau reset / mat dien / loi).
// `min_age_s` > 0: CHI xoa file "gia" hon nguon do (dung cho phien don dinh ky 30 phut - theo yeu cau
// nguoi dung). Tuoi lay tu `File::getLastWrite()` (FAT). Dong ho cua ESP32 chua duoc dat (boot = 1970)
// nhung FAT cung duoc ghi bang chinh dong ho do -> hieu so van dung trong cung lan cap dien; sau khi
// reboot thi `now < mtime` -> coi nhu file CU (rac cua lan chay truoc) va xoa.
uint16_t clean_dir(const char *why, const bool verbose, const uint32_t min_age_s = 0) {
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
  uint16_t n = 0, kept = 0;
  uint64_t freed = 0;
  const time_t nowt = time(nullptr);
  for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
    const bool   isdir = f.isDirectory();
    const size_t sz    = f.size();
    const time_t mt    = f.getLastWrite();   // phai doc TRUOC khi close
    const String nm    = f.name();          // ESP32 core: ten DAY DU dang "/cydlcd_tmp/x.tmp"
    char path[64];
    if (nm.length() && nm[0] == '/') snprintf(path, sizeof(path), "%s", nm.c_str());
    else                             snprintf(path, sizeof(path), "%s/%s", CYDLCD_UPLOAD_TMP_DIR, nm.c_str());
    f.close();                              // dong TRUOC khi xoa (an toan tren FATFS)
    const size_t ln = strlen(path);
    if (isdir || ln <= 4u || strcmp(path + ln - 4u, ".tmp") != 0) continue;
    if (min_age_s) {
      const bool old = (mt <= 0) || (nowt < mt) || ((uint32_t)(nowt - mt) >= min_age_s);
      if (!old) { kept++; continue; }        // con moi -> de lai (co the dang la phien dang chay)
    }
    if (SD.remove(path)) { n++; freed += sz; }
    else ELOG("[UP] CANH BAO: khong xoa duoc file tam %s\n", path);
  }
  dir.close();
  if (n || kept || verbose)
    ELOG("[UP] don %s (%s): xoa %u file .tmp (%lu KB), giu lai %u file con moi\n",
         CYDLCD_UPLOAD_TMP_DIR, why, (unsigned)n, (unsigned long)(freed / 1024ull), (unsigned)kept);
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
  // DANG CO PHIEN GHI THE CUA MAN HINH (nhan upload / dang chot phien) -> TU CHOI ghi tho.
  // VI SAO: FATFS cua man hinh dang giu cache rieng (FAT/dir/FSInfo); Marlin ghi THANG sector se lam
  // cache do LECH va co the de len chinh file vua nhan hoac file dang doi ten.
  // Truong hop that: `cyd_sd_finish_upload()` CHAY O VONG LAP CHINH (co nuoi UART) nen trong luc DOI TEN
  // file tam -> /<ten> van co the nhan duoc frame ghi sector cua Marlin; truoc day nhanh do goi
  // `fatfs_sync()` = `SD.end()` + mount lai NGAY GIUA luc doi ten.
  // Marlin coi la loi ghi the (nhu loi doc da bi tu choi tu truoc) va se doc lai sau khi phien xong
  // (cuoi phien man hinh gui M22+M21 -> Marlin mount lai). Ghi tho tu Marlin chi xay ra voi M28/M29/M30
  // va POWER_LOSS_RECOVERY - deu khong cap thiet trong luc nhan file tu WiFi.
  if (s_file || s_busy) {
    s_raw_wr_refused++;
    if (s_raw_wr_refused == 1 || (s_raw_wr_refused % 64u) == 0)
      ELOG("[UP] TU CHOI ghi RAW sector %lu: dang co phien ghi the cua man hinh (lan %lu)\n",
           (unsigned long)sector, (unsigned long)s_raw_wr_refused);
    return false;
  }
  // DONG BO TRUOC KHI GHI THO: Marlin ghi thang len the (FAT/dir), con FATFS cua man hinh CYD co cache
  // rieng -> phai day het du lieu dang cho xuong the truoc, neu khong cache cu se de len.
  if (s_fatfs_written) { fatfs_sync(); if (!s_sd_ready) return false; }
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
  // KHONG mount lai the o day (theo yeu cau nguoi dung 2026-10-15: "khong mount lai sdcard, vi sdcard
  // luon dinh vao lcd roi"): the KHONG BAO GIO doi chu nen chi mount lai khi THAT SU can - tuc la khi
  // Marlin vua ghi tho (co `s_fatfs_stale` do cyd_sd_raw_write() dat). Mount lai vo dieu kien o moi lan
  // xoa / doc trang thai thu muc la thua va lam gian doan the khong can thiet.
  if (s_fatfs_stale) fatfs_sync();
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
  // Chi mount lai khi Marlin vua ghi tho (xem giai thich o cyd_sd_delete()).
  if (s_fatfs_stale) fatfs_sync();
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
uint8_t     cyd_sd_temp_parts_done(void) { return s_parts_done; }
const char *cyd_sd_temp_err(void) { return s_err; }

// CHAN DOAN: liet ke GOC the bang FATFS cua man hinh (`UI TAP L` tren COM7).
// VI SAO CAN: frame CYDLCD_F_FILES mang ten trong 1 truong CO DINH 32 byte (CYDLCD_FILE_NAME_LEN) nen
// ten dai hon se bi CAT; Marlin cung chi giu LFN trong `LONG_FILENAME_LENGTH` byte. Neu ten hien tren
// man hinh khac ten THAT tren the thi `M23 <ten>` se khong mo duoc file (bam PRINT khong in duoc) -
// ham nay in ten DAY DU doc thang tu FATFS de doi chieu.
void cyd_sd_log_dir(const char *path) {
#if CYDLCD_SD_UART
  if (!s_sd_ready) { ELOG("[UP] goc the: chua co the\n"); return; }
  // Chi mount lai khi Marlin vua ghi tho (xem giai thich o cyd_sd_delete()).
  if (s_fatfs_stale) fatfs_sync();
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
// API: ghi file tam (KIEN TRUC PART: moi part 1 file rieng, dong ngay khi het part)
// ===========================================================================
// Duong dan file cua PART thu `k` (1..255): /cydlcd_tmp/<ten>_001.tmp ...
static void part_path(char *dst, const size_t n, const uint8_t k) {
  snprintf(dst, n, "%s/%s_%03u.tmp", CYDLCD_UPLOAD_TMP_DIR, s_name, (unsigned)k);
}

// Duong dan file tam GOC (dich cua buoc MERGE / nguon cua buoc doi ten).
static void merged_path(char *dst, const size_t n) {
  snprintf(dst, n, "%s/%s.tmp", CYDLCD_UPLOAD_TMP_DIR, s_name);
}

// Xoa file part (1..maxk) + file .tmp cua phien hien tai (dung khi loi/bi bo phien / don phien moi).
static uint16_t parts_remove_all(const uint8_t maxk) {
  uint16_t n = 0;
  char p[72];
  for (uint8_t k = 1; k <= maxk; k++) {
    part_path(p, sizeof(p), k);
    if (SD.exists(p) && SD.remove(p)) n++;
  }
  merged_path(p, sizeof(p));
  if (SD.exists(p) && SD.remove(p)) n++;
  return n;
}

// So part toi da can quet de don (part da dong + part dang mo + 1 du phong). Tran 255.
static uint8_t parts_scan_max(void) {
  const uint8_t last = s_parts_done > s_part ? s_parts_done : s_part;
  return (uint8_t)(last < 1 ? 1 : (last + 1u > 250u ? 255u : last + 1u));
}

bool cyd_sd_temp_open(const char *name, uint32_t size_hint, const uint8_t part) {
  s_err[0] = '\0';
  s_result[0] = '\0';
  const bool first = (part <= 1u);
  if (first) {
    // PART #1 = PHIEN MOI. Doi ten dich + don sach part/.tmp cu cua lan gui truoc (neu co).
    sanitize_name(name, s_name, sizeof(s_name));
    s_result[0] = '\0';
    s_written = 0;
    s_got = 0;
    s_parts_done = 0;
  } else if (s_file) {
    // An toan: part truoc phai duoc DONG o UPLOAD_FILE_END; neu khong (client gui chong) thi dong ngay.
    ELOG("[UP] CANH BAO: part truoc CHUA dong - dong ngay truoc khi mo part #%u\n", (unsigned)part);
    cyd_sd_temp_close();
  }
  // Chua in TU THE SD moi duoc ghi the (2 ben cung dung 1 FAT) - lop chan thu 2 sau net_upload_http.
  // LUU Y: `printer_busy()` chi xet co IN TU THE SD, khong xet HOMING/LEVELING/G35 (khong dung the).
  if (printer_busy()) {
    snprintf(s_err, sizeof(s_err), "Marlin dang IN TU THE SD - khong dung the");
    ELOG("[UP] TU CHOI ghi the: dang in tu the SD (st=%u fl=0x%02X)\n",
         (unsigned)cydlcd.state, (unsigned)cydlcd.flags);
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
  // Don file mo coi ngay khi VUA gianh duoc bus (MUX) / ngay trước PHIEN MOI (UART).
  if (first && !s_file) clean_dir("truoc phien moi", true);
#endif
  // Don file mo coi TRUOC khi mo file moi (chi 1 lan/lan mo - re)
  if (s_file) s_file.close();
  const uint64_t total = SD.totalBytes(), used = SD.usedBytes();
  const uint64_t freeb = (total > used) ? (total - used) : 0;
  if (size_hint) {
    ELOG("[UP] the TF: con %lu KB / tong %lu KB (khach khai bao file moi: %lu B)\n",
         (unsigned long)(freeb / 1024ull), (unsigned long)(total / 1024ull),
         (unsigned long)size_hint);
    if ((uint64_t)size_hint + (256ull * 1024ull) > freeb) {
      snprintf(s_err, sizeof(s_err), "the TF cydlcd het cho (%lu KB trong)",
               (unsigned long)(freeb / 1024ull));
      return false;
    }
  }
  if (!SD.exists(CYDLCD_UPLOAD_TMP_DIR)) SD.mkdir(CYDLCD_UPLOAD_TMP_DIR);
  if (first) parts_remove_all(9);         // bo rac cua lan gui truoc (quet 9 part dau + file .tmp)
  s_part = part;
  part_path(s_tmp, sizeof(s_tmp), part);
  if (SD.exists(s_tmp)) SD.remove(s_tmp);
  s_file = SD.open(s_tmp, FILE_WRITE);
  if (!s_file) {
    snprintf(s_err, sizeof(s_err), "khong tao duoc file part %u", (unsigned)part);
    s_tmp[0] = '\0';
    return false;
  }
  s_cur_bytes = 0;                        // dem lai tu dau cho FILE PART nay
  s_wbuf_n = 0;                           // dem ghi: bat dau part moi voi dem RONG
  s_opens++;
  s_sess_opens++;
  s_t0_ms = millis();
  s_log_mb = 0;
  s_recover_ok = 0;
  s_recover_fail = 0;
  ELOG("[UP] MO PART #%u -> %s (phien mo lan %u, da dong %u part / %lu B, heap=%lu B)\n",
       (unsigned)part, s_tmp, (unsigned)s_opens, (unsigned)s_parts_done,
       (unsigned long)s_got, (unsigned long)ESP.getFreeHeap());
  ELOG("[UP] ghi vao %s (ten dich: %s)\n", s_tmp, s_name);
  // KHONG goi M22/M21 o day (theo yeu cau nguoi dung 2026-10-15: "khong mount lai sdcard, vi sdcard
  // luon dinh vao lcd roi"): the KHONG BAO GIO doi chu -> khong can nha/mount lai the cua Marlin de
  // bat dau ghi. Trong luc nhan file, lop chan `cyd_sd_raw_read()` TU CHOI doc sector (tra loi loi cho
  // Marlin, phuc hoi sau khi phien xong) chu KHONG dung cach nha the nua. Cuoi phien,
  // cyd_sd_finish_upload() -> notify_media_changed() van bao Marlin mount lai de thay FILE MOI.
  return true;
}

// ---------------------------------------------------------------------------
//  DEM GHI (write-back buffer) cho file upload.
//  VI SAO (do tren may that 2026-10-15): WebServer goi `cyd_sd_temp_write()` voi chunk 1436 B -> moi
//  lan la 1 phep ghi NHO, KHONG can sector, va the phai tu lo (moi lan do mat 50-170 ms = ~10-16 KB/s,
//  sau ~1-2 MB thi tra ve thieu). The SD ghi MULTI-BLOCK (nhieu sector lien tiep) nhanh va on dinh hon
//  nhieu so voi nhieu phep ghi nho roi rac. Nay gom du 4096 B (8 sector, can theo sector) roi moi ghi.
//  (Bien `s_wbuf`/`s_wbuf_n` khai bao o dau file - xem `s_got`.)
// ---------------------------------------------------------------------------
// Bao tien do moi khi vuot 1 moc MB (kem KB/s + heap) - de doc log biet phien cham/dut o dau.
static void up_progress_log(void) {
  const uint32_t mb = s_got >> 20;
  if (mb == s_log_mb) return;
  s_log_mb = mb;
  const uint32_t dt = millis() - s_t0_ms;
  const uint32_t kbps = dt ? (uint32_t)(((uint64_t)s_got * 1000ull) / ((uint64_t)dt * 1024ull)) : 0;
  ELOG("[UP] da ghi %lu KB / %lu ms (%lu KB/s, heap=%lu B heapmin=%lu B)\n",
       (unsigned long)(s_got / 1024u), (unsigned long)dt, (unsigned long)kbps,
       (unsigned long)ESP.getFreeHeap(), (unsigned long)s_heap_min);
}

// ---------------------------------------------------------------------------
//  PHUC HOI KHI THE GHI LOI GIUA PHIEN (ghi duoc 0 byte).
//  VI SAO (loi nguoi dung bao 2026-10-15, upload bang TRINH DUYET qua trang web cua man hinh):
//    "cydlcd: error | nhan 1744896/4563245 B | ket qua: the ghi thieu (0/4096 B)"
//  Do lai tren may that: ghi duoc 930 lan 4096 B (3 809 280 B) roi `s_file.write()` tra ve 0 mai. Log 1 Hz
//  cua panel sau do cho thay: `free=0KB` (f_getfree LOI), `sd=0` (the MAT KET NOI), roi `sd=1 free=15 GB`
//  tro lai sau vai giay => THE/CARD bi "dut" tam thoi roi TU HOI, khong phai do ai unmount.
//  Trong luc nhan upload, vong lap chinh bi `server.handleClient()` chan HOAN TOAN nen khong ai khac cham
//  duoc the -> loi nam o TANG DRIVER the (SPI/card).
//
//  !!! KHONG MOUNT LAI THE !!! (yeu cau nguoi dung 2026-10-15: "khong bao gio mount lai sdcard")
//  Chi DONG file dang mo roi MO LAI o che do GHI TIEP (FILE_APPEND): `f_open` doc lai dir entry/FAT cua
//  chinh file do -> go duoc file handle bi FATFS vo hieu hoa, KHONG dung den volume (khong SD.end/SD.begin).
//
//  AN TOAN: CHI tiep tuc khi kich thuoc file tren the SAU khi mo lai == dung so byte da ghi nhan
//  (`expect`). Neu thieu byte (du lieu da mat) thi file se bi LECH giua duong -> PHAI bo phien: KHONG bao
//  gio de mot file .gcode cut/lech nam trong goc the (in ra la hong ban in).
// ---------------------------------------------------------------------------
static bool sd_recover_part(const uint32_t expect) {
  if (s_tmp[0] == '\0' || !s_sd_ready) return false;
  ELOG("[UP] PHUC HOI (KHONG mount lai the): dong + mo lai file part o che do GHI TIEP (can %lu B)\n",
       (unsigned long)expect);
  if (s_file) s_file.close();
  s_file = SD.open(s_tmp, FILE_APPEND);
  if (!s_file) {
    ELOG("[UP] PHUC HOI THAT BAI: khong mo lai duoc %s (the dang dut?)\n", s_tmp);
    s_recover_fail++;
    return false;
  }
  const uint32_t oncard = (uint32_t)s_file.size();
  if (oncard != expect) {
    ELOG("[UP] PHUC HOI KHONG AN TOAN: file tren the %lu B KHAC %lu B da ghi -> BO PHIEN "
         "(khong de file cut trong goc the)\n",
         (unsigned long)oncard, (unsigned long)expect);
    s_recover_fail++;
    return false;
  }
  ELOG("[UP] PHUC HOI OK: mo lai duoc %s (%lu B), ghi tiep\n", s_tmp, (unsigned long)oncard);
  s_recover_ok++;
  return true;
}

// Ghi het buffer xuong the. Tra ve so byte da ghi THANH CONG.
// KIEN NHAN KHI THE "DUT": bang cho tang dan (tong ~7,4 s) thay vi bo phien ngay - do that cho thay the
// mat ket noi vai giay roi TU HOI (sd=1 lai), nen cho doi re hon nhieu so voi bat nguoi dung gui lai 4,5 MB.
static size_t sd_wbuf_flush(void) {
  if (!s_wbuf_n || !s_wbuf) { s_wbuf_n = 0; return 0; }
  if (!s_file) { s_wbuf_n = 0; return 0; }
  const uint16_t want = s_wbuf_n;
  size_t done = 0;
  uint8_t try_i = 0, rec = 0;
  static const uint16_t backoff[] = { 20, 50, 100, 200, 500, 1000, 1500, 2000, 2000 };
  const uint8_t n_back = (uint8_t)(sizeof(backoff) / sizeof(backoff[0]));
  while (done < want) {
    const size_t w = s_file.write(s_wbuf + done, want - done);
    done += w;
    if (done >= want) break;
    if (try_i >= n_back) break;
    const uint16_t ms = backoff[try_i++];
    // Lan thu 2 va 5: DONG + MO LAI file part (KHONG mount lai the) de go file handle bi FATFS vo hieu.
    if ((try_i == 2u || try_i == 5u) && rec < 2u) {
      if (sd_recover_part(s_cur_bytes + (uint32_t)done)) rec++;
    }
    ELOG("[UP] the ghi thieu (%u/%u B, da ghi %lu B) - cho %u ms roi THU LAI lan %u/%u\n",
         (unsigned)done, (unsigned)want, (unsigned long)(s_got + done), (unsigned)ms,
         (unsigned)try_i, (unsigned)n_back);
    delay(ms);
    yield();
  }
  s_wbuf_n = 0;
  return done;
}

size_t cyd_sd_temp_write(const uint8_t *d, const size_t n) {
  // PHAN BIET RO 2 LOAI "ghi loi" (truoc day gop chung thanh "ghi the TF loi" nen khong biet loi that):
  //  - file tam KHONG MO (phien bi ngat / bi chia phan lai) -> day moi la loi logic, khong phai loi the;
  //  - the ghi THIEU byte -> loi the/FATFS that.
  if (!s_file) {
    if (d && n) {
      snprintf(s_err, sizeof(s_err), "file tam khong mo (phien bi ngat?)");
      ELOG("[UP] GHI khi FILE TAM CHUA MO (chunk %u B, da ghi %lu B) - phien upload bi ngat giua duong\n",
           (unsigned)n, (unsigned long)s_got);
    }
    return 0;
  }
  if (!d || n == 0) return 0;
  if (!s_wbuf) {                                 // cap phat dem ghi (xem chu thich o khai bao)
    s_wbuf = (uint8_t *)malloc(CYDLCD_SD_WBUF);
    if (!s_wbuf) {                               // khong du heap -> ghi thang (nhu ban cu)
      ELOG("[UP] CANH BAO: khong cap duoc dem ghi %u B - ghi truc tiep (heap=%lu B)\n",
           (unsigned)CYDLCD_SD_WBUF, (unsigned long)ESP.getFreeHeap());
      const size_t w0 = s_file.write(d, n);
      if (w0 != n) {
        snprintf(s_err, sizeof(s_err), "the ghi thieu (ghi truc tiep)");
        snprintf(s_result, sizeof(s_result), "ERR %s", s_err);
        s_written = s_got;
        if (s_file) s_file.close();
        return w0;
      }
      s_got += (uint32_t)w0;
      s_cur_bytes += (uint32_t)w0;
      up_progress_log();
      return w0;
    }
  }
  size_t copied = 0;
  while (copied < n) {
    size_t space = (size_t)(CYDLCD_SD_WBUF - s_wbuf_n);
    size_t k = n - copied;
    if (k > space) k = space;
    memcpy(s_wbuf + s_wbuf_n, d + copied, k);
    s_wbuf_n = (uint16_t)(s_wbuf_n + k);
    copied += k;
    if (s_wbuf_n == CYDLCD_SD_WBUF) {
      const size_t w = sd_wbuf_flush();
      if (w != (size_t)CYDLCD_SD_WBUF) {
        ELOG("[UP] GHI LOI: buffer %u B chi ghi duoc %u B (da ghi %lu B, heap=%lu, heapmin=%lu, "
             "phuc hoi ok/loi=%u/%u, raw rd=%lu wr=%lu refused=%lu/%lu)\n",
             (unsigned)CYDLCD_SD_WBUF, (unsigned)w, (unsigned long)s_got,
             (unsigned long)ESP.getFreeHeap(), (unsigned long)s_heap_min,
             (unsigned)s_recover_ok, (unsigned)s_recover_fail,
             (unsigned long)s_raw_rd, (unsigned long)s_raw_wr,
             (unsigned long)s_raw_rd_refused, (unsigned long)s_raw_wr_refused);
        snprintf(s_err, sizeof(s_err), "the ghi thieu (%u/%u B)",
                 (unsigned)w, (unsigned)CYDLCD_SD_WBUF);
        snprintf(s_result, sizeof(s_result), "ERR %s", s_err);   // UI/`/status` bao dung phien dang loi
        s_written = s_got;                    // KHONG de UI hien so lieu CU (phien truoc da thanh cong)
        if (s_file) s_file.close();
        s_got += (uint32_t)w;                   // van tinh phan da ghi duoc
        s_cur_bytes += (uint32_t)w;
        return (copied - (size_t)CYDLCD_SD_WBUF + w);
      }
      s_got += (uint32_t)w;
      s_cur_bytes += (uint32_t)w;
      heap_min_track();
      up_progress_log();
    }
  }
  return copied;
}

// DONG PART dang mo: day het dem ghi xuong the + flush + close NGAY (theo yeu cau nguoi dung: "moi lan
// nhan duoc part thi open va close part do luon"). Idempotent: goi luc khong mo file thi khong lam gi.
// (ESP32 Arduino tat exception (-fno-exceptions) nen "try/catch" o day = MOI duong thoat deu goi ham nay:
//  het part, het request, loi ghi, bi ngat, va ca khi mo part moi ma part cu chua dong.)
uint32_t cyd_sd_temp_close(void) {
  if (!s_file && !s_wbuf) return s_got;             // da dong roi -> khong lam gi
  const size_t flushed = sd_wbuf_flush();
  s_got += (uint32_t)flushed;
  s_cur_bytes += (uint32_t)flushed;
  if (s_file) {
    s_file.flush();
    s_file.close();
  }
  if (s_wbuf) { free(s_wbuf); s_wbuf = nullptr; }   // tra dem ghi 4 KB lai cho heap
  const uint32_t partsz = s_cur_bytes;
  if (partsz || s_part) s_parts_done = (uint8_t)(s_parts_done < s_part ? s_part : s_parts_done);
  ELOG("[UP] DONG PART #%u %s: %lu B (tong ca phien %lu B, da dong %u part)\n",
       (unsigned)s_part, s_tmp, (unsigned long)partsz, (unsigned long)s_got, (unsigned)s_parts_done);
  s_cur_bytes = 0;
  s_tmp[0] = '\0';
#if CYDLCD_SD_UART
  s_fatfs_written = true;    // P3: cydlcd vua ghi qua FATFS -> truoc khi cho Marlin ghi tho phai flush
#endif
  return s_got;
}

// BO PHIEN: dong file dang mo (neu con) + XOA MOI file part/.tmp cua phien.
void cyd_sd_temp_abort(void) {
  const uint16_t left = s_wbuf_n;         // byte con trong dem (chua xuong the) - de bao trong log
  s_wbuf_n = 0;                           // bo dem ghi (khong day phan con lai xuong the)
  if (s_wbuf) { free(s_wbuf); s_wbuf = nullptr; }
  if (s_file) s_file.close();
  uint16_t removed = 0;
  if (s_sd_ready) removed = parts_remove_all(parts_scan_max());
  ELOG("[UP] BO PHIEN: da xoa %u file part/.tmp (da ghi %lu B, %u part da dong, %u B con trong dem)\n",
       (unsigned)removed, (unsigned long)s_got, (unsigned)s_parts_done, (unsigned)left);
  s_tmp[0] = '\0';
  s_got = 0;
  s_cur_bytes = 0;
  s_parts_done = 0;
  s_part = 0;
#if CYDLCD_SD_SHARED
  // Phien bi bo -> PHAI tra bus SPI the cho Marlin (khong thi Marlin mat the vinh vien).
  sd_release_to_marlin(false);
#endif
}

// Kich thuoc THAT cua 1 file tren the: MO LAI bang FILE_READ roi doc `size()` (khong tin bo dem RAM).
// Tra ve:
//   >= 1 : mo duoc, day la size that
//   0    : KHONG MO DUOC (the loi/dang dut) HOAC file 0 byte -> `*opened` phan biet hai truong hop nay
// VI SAO phai phan biet: khi the "dut" vai giay thi `SD.open()` that bai -> neu coi la "size = 0" thi se ket
// luan SAI la file thieu byte roi XOA mot file vua ghi dung (da tung xay ra kieu loi nay o buoc doi ten).
// Co THU LAI vai lan (backoff) de cho the hoi.
static uint32_t file_size_on_card(const char *path, bool *opened = nullptr) {
  if (opened) *opened = false;
  if (!s_sd_ready || !path || !path[0]) return 0;
  static const uint16_t rback[] = { 100, 200, 400, 800, 1500 };   // ~3 s
  for (uint8_t i = 0; i <= (uint8_t)(sizeof(rback) / sizeof(rback[0])); i++) {
    File f = SD.open(path, FILE_READ);
    if (f) {
      const uint32_t n = (uint32_t)f.size();
      f.close();
      if (opened) *opened = true;
      return n;
    }
    if (i >= (uint8_t)(sizeof(rback) / sizeof(rback[0]))) break;
    ELOG("[UP] mo lai %s de kiem tra size THAT BAI - cho %u ms roi thu lai\n", path, (unsigned)rback[i]);
    delay(rback[i]);
    yield();
  }
  return 0;
}

// Doi ten file tren the, CO THU LAI KIEN NHAN (the co the "dut" vai giay roi tu hoi - do that 2026-10-15:
// 1/3 lan upload 4,5 MB bi loi ngay o buoc doi ten, ngay sau do log 1 Hz bao free=0KB, sd=0).
static bool rename_with_retry(const char *src, const char *dst) {
  static const uint16_t rback[] = { 100, 200, 300, 500, 800, 1000, 1500, 2000, 2000 };  // ~8,4 s
  const uint8_t n_back = (uint8_t)(sizeof(rback) / sizeof(rback[0]));
  for (uint8_t i = 0;; i++) {
    if (SD.rename(src, dst)) return true;
    if (i >= n_back) return false;
    ELOG("[UP] doi ten %s -> %s THAT BAI - cho %u ms roi THU LAI lan %u/%u\n",
         src, dst, (unsigned)rback[i], (unsigned)(i + 1), (unsigned)n_back);
    delay(rback[i]);
    yield();
  }
}

// ===========================================================================
// MERGE cac PART thanh 1 file: /cydlcd_tmp/<ten>_001.tmp + _002.tmp ... -> /cydlcd_tmp/<ten>.tmp
//   - CHI can khi phien co >=2 part (client gui file thanh nhieu phan). 1 part thi KHONG copy: doi ten
//     thang file part -> /<ten> (khong ton them ~30 s doc+ghi lai ca file).
//   - Doc tung part theo thu tu, ghi qua CHINH duong ghi cua upload (cyd_sd_temp_write -> dem 4 KB + co
//     phuc hoi khi the loi) nen toc do/tinh on dinh giong luc nhan file.
//   - Xoa tung part NGAY sau khi copy xong (thieu dung luong; part sau van con nguyen neu loi giua duong).
//   - Kiem tra tong byte == so byte da nhan; lech => tra false (KHONG doi ten vao goc the).
// ===========================================================================
static bool merge_parts(const uint32_t expect, char *err, const size_t errn) {
  if (err && errn) err[0] = '\0';
  if (s_parts_done < 2u) return false;
  if (!s_sd_ready) { if (err && errn) snprintf(err, errn, "mat the truoc khi ghep part"); return false; }
  uint8_t *rb = (uint8_t *)malloc(CYDLCD_SD_WBUF);
  if (!rb) { if (err && errn) snprintf(err, errn, "khong du RAM de ghep part"); return false; }
  // Mo file dich (ten .tmp - sau do finish_upload doi ten vao goc the)
  char dst[72];
  merged_path(dst, sizeof(dst));
  if (SD.exists(dst)) SD.remove(dst);
  s_file = SD.open(dst, FILE_WRITE);
  if (!s_file) {
    free(rb);
    if (err && errn) snprintf(err, errn, "khong tao duoc file ghep");
    return false;
  }
  snprintf(s_tmp, sizeof(s_tmp), "%s", dst);
  s_got = 0;
  s_cur_bytes = 0;
  s_wbuf_n = 0;
  s_t0_ms = millis();
  s_log_mb = 0;
  bool ok = true;
  ELOG("[UP] GHEP %u part -> %s\n", (unsigned)s_parts_done, dst);
  for (uint8_t k = 1; k <= s_parts_done && ok; k++) {
    char src[72];
    part_path(src, sizeof(src), k);
    File in = SD.open(src, FILE_READ);
    if (!in) {
      ok = false;
      snprintf(s_err, sizeof(s_err), "khong mo duoc part #%u", (unsigned)k);
      ELOG("[UP] GHEP LOI: khong mo duoc %s\n", src);
      break;
    }
    const uint32_t psz_card = (uint32_t)in.size();   // size ma FAT bao cho part nay
    uint32_t psz = 0;
    while (true) {
      const int r = in.read(rb, CYDLCD_SD_WBUF);
      if (r <= 0) break;
      if (cyd_sd_temp_write(rb, (size_t)r) != (size_t)r) {
        ok = false;
        snprintf(s_err, sizeof(s_err), "ghep part #%u loi khi ghi the", (unsigned)k);
        break;
      }
      psz += (uint32_t)r;
    }
    in.close();
    ELOG("[UP] GHEP part #%u: doc duoc %lu B / size tren the %lu B%s\n",
         (unsigned)k, (unsigned long)psz, (unsigned long)psz_card, ok ? "" : " (LOI)");
    // KIEM TRA SIZE TUNG PART: doc duoc phai dung bang size ma FAT bao (lech = part bi cut/loi).
    if (ok && psz != psz_card) {
      ok = false;
      snprintf(s_err, sizeof(s_err), "part #%u tren the %lu B nhung doc duoc %lu B",
               (unsigned)k, (unsigned long)psz_card, (unsigned long)psz);
    }
    if (ok && SD.remove(src)) ELOG("[UP] da xoa part %s sau khi ghep\n", src);
  }
  // DONG file ghep: KHONG dung `cyd_sd_temp_close()` (ham do danh cho PART: no xoa `s_tmp` va cap nhat
  // `s_parts_done` -> sau khi ghep xong thi buoc DOI TEN ngay sau day mat duong dan, do that 2026-10-15:
  // "ERR khong co file tam tren the" du file ghep da ghi xong).
  const size_t flushed = sd_wbuf_flush();
  s_got += (uint32_t)flushed;
  if (s_file) { s_file.flush(); s_file.close(); }
  if (s_wbuf) { free(s_wbuf); s_wbuf = nullptr; }
  const uint32_t got = s_got;
  snprintf(s_tmp, sizeof(s_tmp), "%s", dst);      // giu duong dan file ghep cho buoc doi ten
  free(rb);
  if (!ok) { if (err && errn) snprintf(err, errn, "%s", s_err); return false; }
  // ---- KIEM TRA SIZE SAU KHI MERGE (yeu cau nguoi dung 2026-10-15: "them doan kiem tra file size sau khi
  // merge, neu khong dung thi loi"): mo lai file ghep va doc size THAT tren the, khong tin bo dem RAM.
  bool opened = false;
  const uint32_t oncard = file_size_on_card(dst, &opened);
  ELOG("[UP] GHEP: kiem tra size file ghep tren the = %lu B (bo dem %lu B / can %lu B, mo lai duoc=%d)\n",
       (unsigned long)oncard, (unsigned long)got, (unsigned long)expect, (int)opened);
  if (!opened || oncard != expect) {
    snprintf(s_err, sizeof(s_err), "%s (%lu B / can %lu B)",
             opened ? "file ghep tren the sai size" : "khong mo lai duoc file ghep de kiem tra size",
             (unsigned long)oncard, (unsigned long)expect);
    ELOG("[UP] GHEP LOI: %s\n", s_err);
    if (err && errn) snprintf(err, errn, "%s", s_err);
    return false;
  }
  if (got != expect) {
    snprintf(s_err, sizeof(s_err), "ghep part ra %lu B, can %lu B", (unsigned long)got, (unsigned long)expect);
    ELOG("[UP] GHEP LOI: %s\n", s_err);
    if (err && errn) snprintf(err, errn, "%s", s_err);
    return false;
  }
  ELOG("[UP] GHEP XONG: %lu B -> %s\n", (unsigned long)got, s_tmp);
  return true;
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
  // Duong dan nguon de doi ten: 1 part -> chinh file part; >=2 part -> file .tmp sau khi GHEP.
  if (s_parts_done <= 1u && s_tmp[0] == '\0') part_path(s_tmp, sizeof(s_tmp), 1);
  else if (s_parts_done > 1u) merged_path(s_tmp, sizeof(s_tmp));

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
  const uint32_t expect = size ? size : s_got;     // so byte PHAI co (khach da gui / da nhan duoc)
  if (s_parts_done > 1u) {
    // Nhieu part (client gui file thanh nhieu phan): GHEP lai thanh 1 file truoc khi doi ten.
    ov_set("DANG GHEP CAC PART...", s_name, true);
    if (!merge_parts(expect, s_err, sizeof(s_err))) {
      ELOG("[UP] GHEP THAT BAI: %s -> xoa het part, KHONG doi ten\n", s_err[0] ? s_err : "?");
      parts_remove_all(parts_scan_max());
      s_tmp[0] = '\0';
    }
  }
  if (s_tmp[0] == '\0' || !s_sd_ready) {
    if (!s_err[0]) snprintf(s_err, sizeof(s_err), "khong co file tam tren the");
    parts_remove_all(parts_scan_max());     // don sach part/.tmp (khong de rac cho lan sau)
  } else if (s_got != expect) {
    // CHOT AN TOAN (cuoi cung): tong byte tren the PHAI bang so byte da nhan. Lech = file cut/lech ->
    // XOA, KHONG doi ten (mot file .gcode cut ma lot vao goc the thi co the duoc chon in -> hong ban in).
    snprintf(s_err, sizeof(s_err), "size khong khop (the co %lu B / can %lu B)",
             (unsigned long)s_got, (unsigned long)expect);
    ELOG("[UP] %s -> xoa file tam, KHONG doi ten\n", s_err);
    parts_remove_all(parts_scan_max());
    s_tmp[0] = '\0';
  } else {
    char dst[72];
    snprintf(dst, sizeof(dst), "/%s", s_name);
    if (SD.exists(dst)) SD.remove(dst);   // FATFS rename KHONG ghi de -> xoa ban cu truoc
    ok = rename_with_retry(s_tmp, dst);   // co thu lai kien nhan (the co the "dut" vai giay)
    if (ok) {
      // ---- KIEM TRA SIZE THAT SAU KHI DOI TEN (file trong GOC THE): sai size => XOA, bao loi. ----
      // Khong bao gio de mot file .gcode sai/cut nam trong goc the (nguoi dung co the chon in -> hong in).
      // Phan biet RO "khong mo lai duoc" (the dang loi) voi "sai size" de khong xoa nham file dung.
      bool opened = false;
      const uint32_t oncard = file_size_on_card(dst, &opened);
      if (!opened || oncard != expect) {
        ok = false;
        snprintf(s_err, sizeof(s_err), "%s (%lu B / can %lu B) - da xoa, gui lai",
                 opened ? "file tren the sai size" : "khong mo lai duoc file tren the de kiem tra size",
                 (unsigned long)oncard, (unsigned long)expect);
        ELOG("[UP] %s -> XOA %s\n", s_err, dst);
        SD.remove(dst);
        s_tmp[0] = '\0';
      } else {
        ELOG("[UP] da doi ten %s -> %s (%lu B, kiem tra lai tren the = %lu B)\n",
             s_tmp, dst, (unsigned long)s_got, (unsigned long)oncard);
        s_tmp[0] = '\0';
        s_written = s_got;
        snprintf(s_result, sizeof(s_result), "DONE %lu", (unsigned long)s_got);
        ov_set("DA GHI VAO THE SD", s_name, true);
      }
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
//  CHAN DOAN: ghi thu N KB vao the bang CHINH duong cua upload (cyd_sd_temp_write)
//  Lenh `UI WTEST <KB>` tren COM7. VI SAO: loi "upload file lon bi ghi the TF loi" co the do (a) the/
//  FATFS, hoac (b) xen ke voi WiFi. Ham nay ghi KHONG qua WiFi -> neu cung chet o ~2 MB thi loi nam o
//  the/FATFS; neu ghi tron thi loi nam o duong upload.
// ===========================================================================
bool cyd_sd_write_test(const uint32_t kb) {
  const uint32_t total = kb ? (kb * 1024u) : (4u * 1024u * 1024u);
  static uint8_t buf[1436];                       // dung bang kich thuoc chunk cua WebServer
  for (size_t i = 0; i < sizeof(buf); i++) buf[i] = (uint8_t)(i * 7u + 3u);
  ELOG("[UP] WTEST: ghi thu %lu KB bang duong upload (khong qua WiFi)\n", (unsigned long)(total / 1024u));
  if (!cyd_sd_temp_open("WTEST.BIN", total, 1)) {
    ELOG("[UP] WTEST: khong mo duoc file tam: %s\n", s_err);
    return false;
  }
  uint32_t done = 0;
  bool ok = true;
  while (done < total) {
    uint32_t n = (total - done) > sizeof(buf) ? (uint32_t)sizeof(buf) : (total - done);
    const size_t w = cyd_sd_temp_write(buf, n);
    if (w != n) {
      ELOG("[UP] WTEST: GHI LOI sau %lu B (lan nay xin %lu, duoc %u): %s\n",
           (unsigned long)done, (unsigned long)n, (unsigned)w, s_err);
      ok = false;
      break;
    }
    done += (uint32_t)w;
  }
  if (ok) {
    const uint32_t w2 = cyd_sd_temp_close();
    ELOG("[UP] WTEST: XONG - da ghi %lu B vao file tam\n", (unsigned long)w2);
    ok = (w2 == total);
  }
  cyd_sd_temp_abort();                            // xoa file tam (khong de lai rac tren the)
  ELOG("[UP] WTEST: %s\n", ok ? "THANH CONG (the ghi duoc)" : "THAT BAI (the/FATFS co van de)");
  return ok;
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
  // ---------------------------------------------------------------------------
  // !!! KHONG KIEM TRA THE / MOUNT LAI KHI DANG NHAN-GHI UPLOAD (`s_file` mo / `s_busy`) !!!
  // LOI THAT 2026-10-15: "gui file 4,3 MB qua WiFi -> ghi the TF loi". Do duoc: ghi duoc ~1,7-2 MB roi
  // `SD.write()` tra ve thieu, va log co dong "da mount LAI the TF ... don /cydlcd_tmp (mount lai): xoa
  // 1 file .tmp" NGAY GIUA luc dang ghi. Nguyen nhan: phep kiem tra dinh ky 1 Hz (`sd_alive()`) that
  // bai TAM THOI khi the dang ban ghi (SPI/thead ban) -> `sd_lost()` nha the -> file tam DANG MO bi vo
  // hieu -> cac lan ghi sau tra ve thieu; lan mount lai ke tiep con XOA luon file tam.
  // => Trong luc ghi thi bo qua hoan toan: khong sd_alive(), khong sd_lost(), khong mount lai.
  // ---------------------------------------------------------------------------
  if (s_file || s_busy) return;
  const uint32_t now = millis();
  // ---------------------------------------------------------------------------
  // DON FILE TAM DINH KY (30 phut/lan) - theo yeu cau nguoi dung 2026-10-15:
  // "doi voi truong hop file rac do bi abort, loi thi cho chay dinh ky 1 lenh don dep moi 30 phut 1 lan,
  //  thoi gian tao file tam qua 30 phut thi xoa".
  // Chi chay khi KHONG co phien upload nao (`s_file`/`s_busy`, da `return` o tren) nen moi file .tmp con
  // lai trong /cydlcd_tmp deu la rac cua lan gui bi loi/ngat/reboot truoc do; van kiem tra tuoi > 30 phut
  // (clean_dir(..., 1800)) cho dung y nguoi dung.
  // ---------------------------------------------------------------------------
  if (s_sd_ready && (uint32_t)(now - s_cleanup_ms) >= CYDLCD_SD_CLEANUP_MS) {
    s_cleanup_ms = now;
    char why[40];
    snprintf(why, sizeof(why), "dinh ky moi %lu phut", (unsigned long)(CYDLCD_SD_CLEANUP_MS / 60000u));
    clean_dir(why, true, CYDLCD_SD_CLEANUP_MS / 1000u);
  }
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
  Serial.printf("[MRL] st=%u fl=0x%02X prog=%u%% file=\"%s\" | SD r=%lu/%lu w=%lu/%lu refused rd/wr=%lu/%lu\n",
                (unsigned)cydlcd.state, (unsigned)cydlcd.flags, (unsigned)cydlcd.progress_pct,
                cydlcd.prog.file,
                (unsigned long)s_raw_rd, (unsigned long)s_raw_rd_err,
                (unsigned long)s_raw_wr, (unsigned long)s_raw_wr_err,
                (unsigned long)s_raw_rd_refused, (unsigned long)s_raw_wr_refused);
}

#else   // ---- CYDLCD_WIFI_UPLOAD = 0: stub rong (moi env van link duoc) ----

void cyd_sd_begin(void) {}
void cyd_sd_task(void) {}
void cyd_sd_log(void) {}
bool cyd_sd_ready(void) { return false; }
bool cyd_sd_mount_now(void) { return false; }
void cyd_sd_retry(void) {}
bool cyd_sd_temp_open(const char *name, uint32_t size_hint, uint8_t part) {
  (void)name; (void)size_hint; (void)part; return false;
}
size_t cyd_sd_temp_write(const uint8_t *d, const size_t n) { (void)d; (void)n; return 0; }
uint32_t cyd_sd_temp_close(void) { return 0; }
void cyd_sd_temp_abort(void) {}
const char *cyd_sd_temp_path(void) { return ""; }
uint32_t cyd_sd_temp_bytes(void) { return 0; }
uint8_t cyd_sd_temp_parts_done(void) { return 0; }
const char *cyd_sd_temp_err(void) { return ""; }
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
