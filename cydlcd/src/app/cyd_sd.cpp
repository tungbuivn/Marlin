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
#include <stdio.h>
#include <string.h>
#include "cyd_sd.h"

// ===========================================================================
// BAO MAT TEN FILE (whitelist) - dung chung cho CA 2 nhanh build (co/khong WiFi upload)
// Xem cyd_sd.h + CYDLCD_NAME_ALLOW_EXTRA (cydlcd_config.h). VI SAO: ten file den tu client (field `name`
// cua multipart / query `name` cua /delete) la DU LIEU NGUOI NGOAI -> khong duoc tin. Chi cho phep dung
// tap ky tu an toan, bo het duong dan, chan '.', '..' va ten qua dai => khong the thoat ra ngoai goc the,
// khong the tao ten la de gay roi cho FATFS/log/JSON.
// ===========================================================================
static bool name_char_ok(const char c) {
  if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;
  if (c == '.' || c == '_') return true;
  for (const char *p = CYDLCD_NAME_ALLOW_EXTRA; p && *p; p++)
    if (*p == c) return true;
  return false;
}

const char *cyd_sd_name_check(const char *name) {
  if (!name || !name[0]) return "ten rong";
  const size_t n = strlen(name);
  if (n > 64u) return "ten qua dai (toi da 64 ky tu)";
  if (name[0] == '.') return "ten khong duoc bat dau bang '.'";
  if (strstr(name, "..")) return "ten khong duoc chua '..'";
  for (size_t i = 0; i < n; i++)
    if (!name_char_ok(name[i]))
      return "ten co ky tu khong hop le (chi cho phep a-z A-Z 0-9 . _ va cac ky tu trong "
             "CYDLCD_NAME_ALLOW_EXTRA)";
  return nullptr;
}

void cyd_sd_name_clean(const char *in, char *out, const size_t cap) {
  if (!out || cap == 0) return;
  size_t o = 0;
  const char *slash = in ? strrchr(in, '/') : nullptr;   // bo phan duong dan (chong "../")
  const char *src = slash ? slash + 1 : (in ? in : "");
  for (size_t i = 0; src[i] != '\0' && o + 1u < cap; i++) {
    const char c = src[i];
    out[o++] = name_char_ok(c) ? c : '_';
  }
  out[o] = '\0';
  if (o == 0) snprintf(out, cap, "upload.gco");
  if (strchr(out, '.') == nullptr) {                     // chua co duoi nao -> them ".gco"
    const size_t l = strlen(out);
    if (l + 4u < cap) strcat(out, ".gco");
  }
}

// Log chan doan cua phan the SD. PHAI dinh nghia TRUOC `#if CYDLCD_WIFI_UPLOAD` (sua 2026-10-16):
// nhanh STUB khi CYDLCD_WIFI_UPLOAD = 0 (board STM32) cung goi ELOG (vd cyd_sd_log_dir o cuoi file)
// => de trong nhanh WiFi thi board STM32 LVGL BAO LOI BUILD: "'ELOG' was not declared in this scope".
#if defined(CYDLCD_CYD_DEBUG)
  #define ELOG(...)   do { logq_post(__VA_ARGS__); } while (0)
#else
  #define ELOG(...)   do { } while (0)
#endif

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
#include "logq.h"             // log KHONG CHAN (xem logq.h): ELOG co the chay trong task `link`
                              // (doc/ghi sector tho cho Marlin) nen KHONG duoc in truc tiep ra UART0
#include "ui_overlay.h"       // lop phu tien do upload (chi ghi trang thai; task `ui` ve)

namespace {

// Mau lop phu: lay DUNG gia tri cua ui_lvgl_int.h (MR_BG/MR_PROGBG/MR_PROGBAR/MR_FRAME/MR_RATE100)

// ---- the TF ----
bool     s_sd_ready = false;
bool     s_sd_was   = false;
uint32_t s_sd_check_ms = 0, s_sd_retry_ms = 0;
bool     s_sd_retry_req = false;
// Nhip thu mount lai HIEN TAI khi dang mat the (gian dan 2.5 -> 30 s, xem cyd_sd_task()).
uint32_t s_sd_retry_gap_ms = CYDLCD_SD_RETRY_MS;
uint32_t s_cleanup_ms = 0;             // moc lan don file tam dinh ky (CYDLCD_SD_CLEANUP_MS)
// ---- DO HIEU NANG (`UI PERF`) cua chang cyd_sd_task(): xem cyd_sd_perf_report() ----
uint32_t s_pf_check_n = 0, s_pf_check_us = 0, s_pf_check_max_us = 0, s_pf_clean_us = 0;
uint32_t s_pf_log_n = 0, s_pf_log_us = 0;       // dong log 1 Hz (co goi SD.totalBytes/usedBytes)

// ---- KHOA THE SD (xem khoi chu thich o tren) ----
#if defined(CYDLCD_BOARD_CYD)
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
  static SemaphoreHandle_t s_card_mtx = nullptr;
#endif
uint32_t s_pf_card_wait_max_us = 0;    // cho lau nhat (chan doan - `UI PERF`)
uint32_t s_pf_card_wait_to = 0;        // so lan het tran cho khoa
#define CARD_WAIT_LINK_MS  300u        // tran cho khoa cua duong `link` (Marlin cho 1000 ms/sector)

static void card_mtx_init(void) {
#if defined(CYDLCD_BOARD_CYD)
  if (!s_card_mtx) s_card_mtx = xSemaphoreCreateRecursiveMutex();
#endif
}

// Lay khoa the. `ms` = tran cho: CARD_WAIT_LINK_MS cho duong `link`, **0 = cho den khi co** (core 0).
bool cyd_sd_lock(const uint32_t ms) {
#if defined(CYDLCD_BOARD_CYD)
  card_mtx_init();
  if (!s_card_mtx) return true;                       // khong tao duoc khoa -> chay nhu cu (khong chan)
  const uint32_t t0 = micros();
  const TickType_t ticks = ms ? pdMS_TO_TICKS(ms) : portMAX_DELAY;
  const bool ok = (xSemaphoreTakeRecursive(s_card_mtx, ticks) == pdTRUE);
  const uint32_t dt = (uint32_t)(micros() - t0);
  if (dt > s_pf_card_wait_max_us) s_pf_card_wait_max_us = dt;
  if (!ok) { s_pf_card_wait_to++; return false; }
  return true;
#else
  (void)ms; return true;
#endif
}

void cyd_sd_unlock(void) {
#if defined(CYDLCD_BOARD_CYD)
  if (s_card_mtx) xSemaphoreGiveRecursive(s_card_mtx);
#endif
}

// RAII: tu dong nha khoa khi ra khoi pham vi (moi duong `return` deu dung, khong phai sua tung return).
// Dung: `CardLock lk(0); if (!lk.ok) return false;`   (0 = cho den khi co khoa; duong `link` dung ms=300)
struct CardLock {
  bool ok;
  explicit CardLock(const uint32_t ms) : ok(cyd_sd_lock(ms)) {}
  ~CardLock() { if (ok) cyd_sd_unlock(); }
  // Nha khoa SOM (khi con phan viec KHONG cham the o cuoi ham, vd cho Marlin mount lai doc sector).
  void release() { if (ok) { cyd_sd_unlock(); ok = false; } }
};

// ===========================================================================
//  UU TIEN MARLIN KHI DUNG THE (yeu cau nguoi dung 2026-10-15)
// ===========================================================================
// YEU CAU: *"neu luc in ma user mo trang list file thi se thanh loi (do list nhieu file nen timeout chang han),
// cho nen neu Marlin request thi huy access ben phia LCD, tru truong hop dang upload"*.
// VI SAO: Marlin doc the qua UART TUNG SECTOR voi tran 1000 ms/sector (CYDLCD_SD_TIMEOUT_MS). Neu man hinh CYD
// dang tu quet the (liet ke file cho trang web `/files`, dem muc con, tim thu muc, don rac) va giu khoa the lau
// -> Marlin cho qua tran -> bao loi doc the GIUA BAN IN (ban in dung).
// CACH LAM: moi lan Marlin hoi 1 sector (`cyd_sd_raw_read/write`), ghi moc `s_marlin_sd_ms`. Cac thao tac the
// CUA MAN HINH (khong phai upload) se TU HUY / BO QUA khi Marlin dang dung the (trong 400 ms vua qua) va thu lai
// sau; rieng vong quet thu muc thi THOAT GIUA DUONG (tra ve phan da lam) de khong giu khoa lau.
// NGOAI LE (dung y nguoi dung): dang co PHIEN UPLOAD (`s_file`/`s_busy`) thi KHONG huy - upload phai chay xong;
// va trong luc upload thi chieu nguoc lai da bi TU CHOI tu truoc (xem cyd_sd_raw_read).
volatile uint32_t s_marlin_sd_ms = 0;
// Trang thai thao tac the CUOI CUNG cua MAN HINH (xem cyd_sd.h): de phan biet "BAN vi Marlin dang doc/ghi the"
// (=> thu lai sau) voi "loi that" (y kien nguoi dung 2026-10-15).
volatile uint8_t s_sd_last_op = 0;
#define MARLIN_SD_ACTIVE_MS 400u        // "Marlin dang dung the" = co request trong 400 ms vua qua
static inline bool marlin_sd_active(void) {
  return s_marlin_sd_ms && (uint32_t)(millis() - s_marlin_sd_ms) < MARLIN_SD_ACTIVE_MS;
}
// `upload_session()` / `cyd_sd_yield_to_marlin()` dinh nghia SAU phan khai bao `s_file`/`s_busy` (o duoi),
// vi chung can 2 bien do.

// ===========================================================================
//  KHOA THE SD - dung chung cho CA 2 CORE (xem cydlcd/docs/ke-hoach-da-luong.md, B2a)
// ===========================================================================
// VI SAO PHAI CO: tu B1 (tach task `link`), the SD bi 2 CORE cham vao:
//   - core 1 (task `link`): doc/ghi SECTOR THO cho Marlin (`cyd_sd_raw_read/write`) - phai tra loi trong
//     1000 ms/sector (CYDLCD_SD_TIMEOUT_MS ben Marlin),
//   - core 0 (task loop / sau nay la `net`): FATFS (mount, `sd_alive()` 1 Hz, `clean_dir()` 30 phut, xoa file,
//     liet ke thu muc) va phien upload (SD.write tung khoi 16 KB).
// Driver SD cua ESP32 (`SPIClass` + `SD`/FATFS) KHONG re-entrant => 2 core cung vao la hong du lieu/treo.
//
// LUAT (de KHONG pha muc tieu "serial khong bi gian doan"):
//   1. `link` lay khoa voi TRAN 300 ms (`CARD_WAIT_LINK_MS`); het tran -> tra false (Marlin nhan ERR) chu
//      KHONG cho vo han. Marlin cho 1000 ms/sector nen van con du.
//   2. Core 0 giu khoa quanh TUNG thao tac (mot lan `SD.write` ~10-30 ms), khong giu suot ca phien upload.
//   3. KHONG mount lai / don rac khi dang in (xem cyd_sd_task) - `sd_mount()` khi the loi mat ~2 s > 300 ms.
//   4. Chi 1 khoa, dung RECURSIVE (cac ham nhu cyd_sd_finish_upload() goi long nhau).
//   5. Do duoc: `s_pf_card_wait_max_us` (cho lau nhat) + `s_pf_card_wait_to` (so lan het tran) - in o `UI PERF`.
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
bool      s_busy = false;              // dang ghi vao the (de /status bao "writing")

// ---- UU TIEN MARLIN (xem khoi chu thich o dau file) ----
// Dat SAU `s_file`/`s_busy` vi can 2 bien do. (Ham CONG KHAI `cyd_sd_yield_to_marlin()` dinh nghia o CUOI
// file, NGOAI anonymous namespace - vi cyd_sd.h khai bao no trong `extern "C"`.)
static inline bool upload_session(void) { return s_file || s_busy; }

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

// Ten file an toan (bo duong dan, ky tu la, gioi han do dai + duoi .gco).
// VI SAO goi `cyd_sd_name_clean()`: de TAP KY TU CHO PHEP chi co MOT dinh nghia duy nhat
// (`name_char_ok()` + CYDLCD_NAME_ALLOW_EXTRA) - khong the co chuyen lop bao mat web va lop lam sach ten
// hieu khac nhau (truoc day noi nay liet ke tay 'a-z A-Z 0-9 . _ -').
void sanitize_name(const char *in, char *out, const size_t cap) {
  cyd_sd_name_clean(in, out, cap);
}

// ===========================================================================
// LOP PHU TIEN DO UPLOAD: da CHUYEN sang ui_overlay.cpp (2026-10-15, B3 - da luong).
// VI SAO: lop phu nay duoc bat/tat tu duong upload (task `net`) nhung LVGL chi duoc goi tu task `ui`
// => cyd_sd.cpp chi con GOI cac ham `ov_*` / `ui_lock()` (chi ghi trang thai), con viec ve do
// ui_overlay_update() lam trong nhip cua task `ui`. Xem ui_overlay.h.
// ===========================================================================

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
      ELOG("[UP] %s @ %u Hz: cardType=%d\n",
           s_sd_was ? "da mount LAI the TF" : "the TF OK", (unsigned)hz_try[i], (int)SD.cardType());
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
// DA LUONG (2026-10-15): duong `link` (core 1) KHONG duoc tu dong bo FATFS (`SD.end()` + mount lai ~2 s lam
// gian doan serial) -> chi BAT CO nay; core 0 lam o `cyd_sd_task()` (ham do da giu khoa the).
volatile bool s_fatfs_sync_req = false;

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
  // DA LUONG (sua 2026-10-15): KHONG con goi `cydclcd_link_task()` / `lv_timer_handler()` o day nua:
  //   - `cydclcd_link_task()` chay trong TASK `link` (core 1) => goi tu core 0 = HAI CORE CUNG CHAY MOT HAM
  //     khong re-entrant (bo giai frame, hang doi TX, trang thai) => hong du lieu/treo. Truoc B1 chi co 1
  //     nguoi goi nen khong lo; tu B1 phai bo.
  //   - `lv_timer_handler()` thuoc task UI (core 0); o day chi can CHO Marlin mount lai (delay nhuong CPU).
  delay(CYDLCD_HTTP_SDWAIT_MS);
  // DA LUONG: KHONG ghi thang vao `cydlcd` (do la ban cong khai, task `link` la chu so huu) - chi YEU CAU.
  cydlcd_link_reset_files();
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
    // UU TIEN MARLIN (yeu cau nguoi dung 2026-10-15): dang in/doc the -> THOAT GIUA DUONG (tra ve so da don)
    // de khong giu khoa the lau lam Marlin qua tran 1000 ms/sector. Lan don sau (30 phut) lam tiep.
    if (marlin_sd_active() && !upload_session()) {
      ELOG("[UP] don %s: TAM DUNG giua duong vi Marlin dang doc the (da xoa %u file)\n", why, (unsigned)n);
      break;
    }
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

bool cyd_sd_mount_now(void) {
  CardLock lk(0); if (!lk.ok) return false;
  return sd_mount(true);
}

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
  s_marlin_sd_ms = millis();          // UU TIEN MARLIN: bao cho cac thao tac the CUA MAN HINH biet de NHUONG
  // KHOA THE (B2a): core 0 co the dang FATFS (sd_alive/clean_dir/xoa file/liet ke) -> cho toi da 300 ms
  // (Marlin cho 1000 ms/sector). Het tran -> tra false (Marlin coi la loi doc, thu lai sau) - KHONG cho vo han.
  if (!cyd_sd_lock(CARD_WAIT_LINK_MS)) return false;
  // SD.readRAW(): doc THANG sector vat ly, khong qua FATFS (core ESP32 SD.cpp:124 -> sd_read_raw).
  // Doc lap voi FATFS dang mount: chi doc, khong dung cache cua FATFS.
  const bool ok = SD.readRAW(dst, sector);
  cyd_sd_unlock();
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
  s_marlin_sd_ms = millis();          // UU TIEN MARLIN: xem chu thich o cyd_sd_raw_read
  // DONG BO TRUOC KHI GHI THO: Marlin ghi thang len the (FAT/dir), con FATFS cua man hinh CYD co cache
  // rieng -> phai day het du lieu dang cho xuong the truoc, neu khong cache cu se de len.
  // DA LUONG (sua 2026-10-15): KHONG goi `fatfs_sync()` NGAY O DAY nua. `fatfs_sync()` = `SD.end()` +
  // mount lai => co the ton ~2 s, ma day la duong `link` (core 1) -> se lam GIAN DOAN duong serial (vuot
  // xa tran 300 ms). Nay chi YEU CAU core 0 dong bo (o `cyd_sd_task()`, ngoai luc nay) va TU CHOI lan ghi
  // nay: Marlin bao loi ghi the va thu lai sau - ghi tho tu Marlin chi la M28/M29/PLR, khong cap thiet
  // khi dang in (xem README muc 12).
  if (s_fatfs_written) {
    s_fatfs_sync_req = true;
    s_raw_wr_refused++;
    if (s_raw_wr_refused == 1 || (s_raw_wr_refused % 64u) == 0)
      ELOG("[UP] TU CHOI ghi RAW sector %lu: chua dong bo FATFS (da xin core 0 dong bo, lan %lu)\n",
           (unsigned long)sector, (unsigned long)s_raw_wr_refused);
    return false;
  }
  if (!cyd_sd_lock(CARD_WAIT_LINK_MS)) return false;
  const bool ok = SD.writeRAW((uint8_t *)src, sector);   // core ESP32: sd_write_raw() (SD.h/SD.cpp)
  cyd_sd_unlock();
  if (ok) { s_raw_wr++; s_fatfs_stale = true; }   // FATFS cua cydlcd gio co the LECH -> mount lai truoc
  else    { s_raw_wr_err++; ELOG("[UP] ghi RAW sector %lu THAT BAI\n", (unsigned long)sector); }
  return ok;
#else
  (void)sector; (void)src;
  return false;
#endif
}

bool cyd_sd_delete(const char *name) {
  CardLock lk(0); if (!lk.ok) return false;   // KHOA THE (B2a)
  s_sd_last_op = CYDLCD_SDOP_OK;                    // mac dinh: khong phai 'ban vi Marlin'
  // UU TIEN MARLIN: dang in/doc the thi KHONG xoa (xem khoi chu thich o dau file).
  if (cyd_sd_yield_to_marlin()) { s_sd_last_op = CYDLCD_SDOP_BUSY; return false; }
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
// o tren - vong lap do khong phai tu loc gi) nen chi can dem.
//   >= 0 = so muc con,  -1 = khong phai thu muc / khong mo duoc,  **-2 = BAN** (Marlin dang doc/ghi the SD
//   -> nguoi goi phai hieu la "thu lai sau", KHONG phai loi that).
int cyd_sd_dir_count(const char *name) {
  CardLock lk(0); if (!lk.ok) return -1;
#if CYDLCD_SD_UART
  if (!s_sd_ready || !name || !name[0]) return -1;
  // UU TIEN MARLIN (yeu cau nguoi dung 2026-10-15): dang in/doc the thi KHONG quet thu muc -> tra -2 (BAN).
  // Vong quet nhieu file co the lam Marlin qua tran 1000 ms/sector.
  if (cyd_sd_yield_to_marlin()) { s_sd_last_op = CYDLCD_SDOP_BUSY; return -2; }
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
  CardLock lk(0); if (!lk.ok) { if (not_empty) *not_empty = false; return false; }
  s_sd_last_op = CYDLCD_SDOP_OK;
  // UU TIEN MARLIN: dang in/doc the thi KHONG xoa.
  if (cyd_sd_yield_to_marlin()) { s_sd_last_op = CYDLCD_SDOP_BUSY; if (not_empty) *not_empty = false; return false; }
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

// ===========================================================================
// DUYET GOC THE + BAO MARLIN MOUNT LAI (cho TRANG WEB cua man hinh)
// ===========================================================================
// BO DUYET MOT LUOT (2026-10-16). Xem ghi chu day du o cyd_sd.h.
//
// TRUOC DAY: `cyd_sd_root_entry(idx, ...)` mo lai thu muc roi dem toi `idx` cho TUNG muc -> O(N^2).
// Do tren board that (the co N file nho):
//     7 file -> 0,18 s | 12 -> 0,51 s | 17 -> 1,06 s | 22 -> 1,89 s | 27 -> 3,11 s
// Nay duyet MOT lan -> O(N). Va dung `getNextFileName(&isDir)` (1 readdir, KHONG mo file, KHONG stat)
// thay cho `openNextFile()` (tao doi tuong File + stat tung muc) -> khong con doc size file nua.
static File s_list_dir;              // thu muc goc dang duyet (chi 1 phien duyet tai mot thoi diem)
static bool s_list_open = false;

bool cyd_sd_list_begin(void) {
#if CYDLCD_SD_UART
  if (s_list_open) cyd_sd_list_end();          // phien duyet truoc bi bo quen -> dong lai
  if (!s_sd_ready) return false;
  // UU TIEN MARLIN (yeu cau nguoi dung 2026-10-15): dang in/doc the -> KHONG liet ke (tra false = het
  // danh sach lan nay). Trang web `/files` se bao "ban, thu lai sau" (xem net_upload_http.cpp).
  if (cyd_sd_yield_to_marlin()) return false;
  if (s_fatfs_stale) fatfs_sync();             // chi khi Marlin vua ghi tho (xem cyd_sd_delete())
  if (!s_sd_ready) return false;
  s_list_dir = SD.open("/");
  if (!s_list_dir || !s_list_dir.isDirectory()) {
    if (s_list_dir) s_list_dir.close();
    return false;
  }
  s_list_open = true;
  return true;
#else
  return false;
#endif
}

bool cyd_sd_list_next(char *name, size_t name_n, bool *isdir) {
#if CYDLCD_SD_UART
  if (!s_list_open || !name || name_n == 0) return false;
  for (;;) {
    bool d = false;
    // getNextFileName: MOT readdir, khong mo file/khong stat (khac openNextFile). Khong lay duoc size
    // nhung trang web khong can (xem cyd_sd.h).
    const String nm = s_list_dir.getNextFileName(&d);
    if (nm.length() == 0) return false;        // het muc
    const char *p = nm.c_str();
    if (*p == '/') p++;
    if (cydlcd_hidden_dir(p, d ? 1u : 0u)) continue;   // muc he thong -> an
    snprintf(name, name_n, "%s", p);
    if (isdir) *isdir = d;
    return true;
  }
#else
  (void)name; (void)name_n; (void)isdir;
  return false;
#endif
}

void cyd_sd_list_end(void) {
#if CYDLCD_SD_UART
  if (s_list_open) { s_list_dir.close(); s_list_open = false; }
#endif
}

// Bao Marlin mount lai the + xin lai danh sach file. Dung sau khi XOA file qua trang web: FAT cua Marlin
// dang cache trang thai CU (file vua xoa) nen phai mount lai, neu khong Marlin co the ghi de cache cu len
// the (lam file da xoa "song lai") hoac hien file da xoa trong danh sach.
void cyd_sd_notify_marlin_remount(void) {
#if CYDLCD_SD_UART
  if (!s_sd_ready) return;
  notify_media_changed();
#endif
}
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
  // UU TIEN MARLIN (yeu cau nguoi dung 2026-10-15): dang in/doc the -> KHONG liet ke (tra loi ngay, khong
  // giu khoa the lau). Thu lai sau khi Marlin doc xong.
  if (cyd_sd_yield_to_marlin()) { ELOG("[UP] goc the: NHUONG cho Marlin (dang doc the) - thu lai sau\n"); return; }
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
  CardLock lk(0); if (!lk.ok) return false;   // KHOA THE (B2a)
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
    // DA LUONG: KHONG goi lv_timer_handler()/cydclcd_link_task() o day (task `ui`/`link` tu chay).
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
  CardLock lk(0); if (!lk.ok) return 0;      // KHOA THE: nha ngay sau moi khoi ghi (xem ke hoach)
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
  CardLock lk(0); if (!lk.ok) return 0;
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
  CardLock lk(0);
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
  CardLock lk(0); if (!lk.ok) { if (err && errn) snprintf(err, errn, "dang ban the SD"); return false; }   // KHOA THE (recursive)
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
  // ---------------------------------------------------------------------------
  // !!! KET THUC PHIEN GHI **TRUOC** KHI BAO MARLIN MOUNT LAI !!!
  // LOI THAT (tu cau hoi nguoi dung 2026-10-15: "neu trong luc upload ma Marlin gui M21/M22 thi co bi block
  // khong?"): `notify_media_changed()` gui M22+M21 roi NUOI UART ~800 ms de Marlin mount lai + xin lai danh
  // sach file. Trong khoang do Marlin DOC SECTOR (mount = doc boot sector/FAT), nhung lop chan chong ghi de
  // (`cyd_sd_raw_read/write` tra false khi `s_file || s_busy`) van dang bat vi `s_busy` chi duoc xoa o CUOI
  // ham => man hinh VUA BAO "mount lai di" VUA TU CHOI moi sector => Marlin mount loi / khong lay duoc danh
  // sach file (log cu co dong `refused rd/wr=1/0` ngay sau DONE). Nay xoa `s_busy` TRUOC khi bao.
  // An toan: luc nay file da dong + da doi ten xong, khong con gi phai bao ve.
  // ---------------------------------------------------------------------------
  s_busy = false;
  s_part = 0;
  // !!! NHA KHOA THE **TRUOC** KHI BAO MARLIN MOUNT LAI !!!
  // LOI THAT (2026-10-15, do duoc sau khi tach task): `notify_media_changed()` gui M22/M21 roi CHO Marlin
  // mount lai; trong luc do Marlin DOC SECTOR (boot sector/FAT) qua UART -> task `link` phai lay KHOA THE,
  // nhung khoa nay dang bi CHINH HAM NAY giu (CardLock o dau ham) => qua tran 300 ms => Marlin bao
  // "khong doc duoc sector 0 qua UART" / "No SD card" (da gap trong log COM4).
  // Doan con lai cua ham chi cham chuoi/overlay, khong dung the -> nha khoa som la dung.
  lk.release();
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
    // DA LUONG: khong goi LVGL / link task tu day nua.
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
  CardLock lk(0); if (!lk.ok) return false;
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
  card_mtx_init();                    // KHOA THE (B2a): tao TRUOC khi cac task bat dau chay
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
  CardLock lk(0);   // KHOA THE (B2a): FATFS tren core 0 vs sector tho tren core 1
  // YEU CAU DONG BO FATFS tu duong `link` (core 1 khong duoc tu lam - xem cyd_sd_raw_write):
  // thao tac nay can mount lai (~2 s khi the loi) nen PHAI chay o core 0, va dang giu khoa the.
  if (s_fatfs_sync_req) { s_fatfs_sync_req = false; fatfs_sync(); }
  // UU TIEN MARLIN (yeu cau nguoi dung 2026-10-15): Marlin dang doc the (dang in / mo danh sach file) thi
  // MAN HINH KHONG cham vao the nua - bo qua kiem the/don rac/mount lai o vong nay (tru khi dang upload).
  if (cyd_sd_yield_to_marlin()) return;
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
    const uint32_t t_cl = micros();
    clean_dir(why, true, CYDLCD_SD_CLEANUP_MS / 1000u);
    s_pf_clean_us += micros() - t_cl;
  }
  if (s_sd_ready) {
    if (now - s_sd_check_ms < CYDLCD_SD_CHECK_MS) return;
    s_sd_check_ms = now;
    // Do hieu nang (`UI PERF`): do duoc `cyd_sd_task()` an ~2 s MOI vong lap => vong lap 0.5 Hz => man
    // hinh cham. Tach tiep xem phan nao: `sd_alive()` (SD.exists("/") = cham vao the) hay `clean_dir()`.
    const uint32_t t_chk = micros();
    const bool alive = sd_alive();
    const uint32_t d_chk = micros() - t_chk;
    s_pf_check_us += d_chk; s_pf_check_n++;
    if (d_chk > s_pf_check_max_us) s_pf_check_max_us = d_chk;
    if (!alive) sd_lost("kiem tra dinh ky 1 Hz");
    return;
  }
  // ---------------------------------------------------------------------------
  // THE KHONG MOUNT DUOC -> THU LAI THUA DAN (backoff). LOI THAT 2026-10-15 (nguoi dung: "LCD chay cuc
  // cham"): do bang `UI PERF` -> `sd_task: sd_alive 0 lan, clean_dir 0 us` nhung `http: sd_the 10 425 880 us`
  // trong ~12 s, va log day dong `sdcard_mount(): f_mount failed: (3)`.
  // NGUYEN NHAN: the TF khong mount duoc, ma vong lap cu 2.5 s lai goi `sd_mount()`; moi lan mount thu 3 muc
  // toc do (20/10/4 MHz) -> moi muc ton ~0.7-1 s (SD.begin that bai + delay 30 ms + log cua core) => ~2 s.
  // => VONG LAP BI CHAN 2 s MOI VONG => man hinh cap nhat 0.5 Hz => "chay cuc cham" (phan ve chi 3-5 ms,
  // SPI 2 ms/lan nen KHONG phai loi ve). Nay nhip thu gian dan 2.5 -> 5 -> 10 -> 20 -> 30 s (toi da 30 s):
  // the mat han thi chi ton ~2 s moi 30 s (6%) thay vi 80% thoi gian. Bam REFRESH / cam the -> thu NGAY
  // (cyd_sd_retry() dat `s_sd_retry_req`).
  // ---------------------------------------------------------------------------
  if (s_sd_retry_req) { s_sd_retry_req = false; s_sd_retry_gap_ms = CYDLCD_SD_RETRY_MS; }
  else if (now - s_sd_retry_ms < s_sd_retry_gap_ms) return;
  s_sd_retry_ms = now;
  if (sd_mount(false)) { s_sd_retry_gap_ms = CYDLCD_SD_RETRY_MS; clean_dir("mount lai", false); }
  else if (s_sd_retry_gap_ms < 30000u) {
    s_sd_retry_gap_ms *= 2u;
    if (s_sd_retry_gap_ms > 30000u) s_sd_retry_gap_ms = 30000u;
  }
#endif
}

// In + XOA bo dem hieu nang cua chang `cyd_sd_task()` (lenh `UI PERF`).
// VI SAO CAN: do duoc chang nay an ~2 s MOI vong lap (=> vong lap 0.5 Hz => "man hinh chay cuc cham" du
// phan ve chi 3-5 ms). Hai nghi pham: `sd_alive()` (= `SD.exists("/")`, cham vao the moi lan) va `clean_dir()`
// (quet thu muc /cydlcd_tmp). Hai con so duoi day noi ro cai nao.
// UU TIEN MARLIN (yeu cau nguoi dung 2026-10-15: "neu Marlin request thi huy access ben phia LCD, tru truong
// hop dang upload"): dinh nghia O DAY (ngoai anonymous namespace) vi cyd_sd.h khai bao trong `extern "C"`.
bool cyd_sd_yield_to_marlin(void) { return marlin_sd_active() && !upload_session(); }

// Trang thai thao tac the cuoi cung cua MAN HINH (xem cyd_sd.h). Dinh nghia o day (ngoai anon namespace).
uint8_t cyd_sd_last_op(void) { return s_sd_last_op; }

void cyd_sd_perf_report(void) {
  Serial.printf("[PERF] sd_task: sd_alive %lu lan, %lu us tong, tb %lu us, max %lu us | clean_dir %lu us\n",
                (unsigned long)s_pf_check_n, (unsigned long)s_pf_check_us,
                (unsigned long)(s_pf_check_n ? s_pf_check_us / s_pf_check_n : 0),
                (unsigned long)s_pf_check_max_us, (unsigned long)s_pf_clean_us);
  Serial.printf("[PERF] log 1 Hz: %lu lan, %lu us tong, tb %lu us (log trang thai, KHONG con f_getfree)\n",
                (unsigned long)s_pf_log_n, (unsigned long)s_pf_log_us,
                (unsigned long)(s_pf_log_n ? s_pf_log_us / s_pf_log_n : 0));
  // KHOA THE (B2a): cho lau nhat + so lan het tran. YEU CAU: cho cua duong `link` phai << 1000 ms (Marlin).
  Serial.printf("[PERF] khoa the SD: cho lau nhat %lu us | so lan het tran (300 ms): %lu\n",
                (unsigned long)s_pf_card_wait_max_us, (unsigned long)s_pf_card_wait_to);
  s_pf_check_n = s_pf_check_us = s_pf_check_max_us = s_pf_clean_us = 0;
  s_pf_log_n = s_pf_log_us = 0;
  s_pf_card_wait_max_us = 0; s_pf_card_wait_to = 0;
}

void cyd_sd_log(void) {
  const uint32_t t_log = micros();
  const uint32_t heap = ESP.getFreeHeap();
  if (heap < s_heap_min) s_heap_min = heap;
  // Ten file tam dang co (chi ten, khong ca duong dan - dong log gon)
  const char *t = s_tmp[0] ? strrchr(s_tmp, '/') : nullptr;
#if CYDLCD_SD_SHARED
  // The dung chung: trang thai the that su nam o co CYDLCD_SF_MEDIA cua Marlin.
  const int sd_state = (int)cyd_sd_ready();
#else
  const int sd_state = (int)s_sd_ready;
#endif
  // 2026-10-16 (YEU CAU NGUOI DUNG "cho nao lay size file hay size the sd thi bo"): KHONG con doc
  // dung luong con trong cua the o day. `SD.totalBytes()/usedBytes()` = FATFS `f_getfree` QUET BANG FAT
  // (~20 ms/lan tren the 16 GB, co the lau hon nhieu khi the day) ma chi de IN LOG -> vo ich.
  // Cho DUY NHAT con doc dung luong the la trong duong UPLOAD (cyd_sd_temp_open): o do no la kiem tra
  // "con du cho khong" TRUOC khi nhan file - dung nhu ngoai le nguoi dung cho phep.
  Serial.printf("[UP] wifi=%u ssid=\"%s\" ip=%s rssi=%d sd=%d busy=%d rx=%lu written=%lu "
                "heap=%lu heapmin=%lu | file=%s | %s\n",
                (unsigned)net_wifi_state(), net_wifi_ssid(), net_wifi_ip(), (int)net_wifi_rssi(),
                sd_state, (int)s_busy, (unsigned long)s_got, (unsigned long)s_written,
                (unsigned long)heap, (unsigned long)s_heap_min,
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
  // Do hieu nang (`UI PERF`) cho ca dong log 1 Hz nay. TRUOC 2026-10-16 trong do co
  // `SD.totalBytes()/usedBytes()` (FATFS `f_getfree`, QUET BANG FAT) nen phai do; nay da bo phan do.
  s_pf_log_us += micros() - t_log; s_pf_log_n++;
}

#else   // ---- CYDLCD_WIFI_UPLOAD = 0: stub rong (moi env van link duoc) ----

void cyd_sd_begin(void) {}
void cyd_sd_task(void) {}
void cyd_sd_log(void) {}
void cyd_sd_perf_report(void) {}
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
bool cyd_sd_list_begin(void) { return false; }
bool cyd_sd_list_next(char *name, size_t name_n, bool *isdir) {
  (void)name; (void)name_n; (void)isdir; return false;
}
void cyd_sd_list_end(void) {}
void cyd_sd_notify_marlin_remount(void) {}
void cyd_sd_log_dir(const char *path) { (void)path; ELOG("[UP] goc the: che do the khong phai CYDLCD_SD_UART\n"); }
bool cyd_sd_finish_upload(const char *name, uint32_t size, uint32_t *written, char *err, size_t errn) {
  (void)name; (void)size; (void)written; (void)err; (void)errn;
  return false;
}
const char *cyd_sd_last_result(void) { return ""; }
uint32_t cyd_sd_last_written(void) { return 0; }
// 2026-10-16: thieu symbol nay lam env STM32F407 LVGL loi LIEN KET (`undefined reference to
// cyd_sd_last_op`) — ban day du o nhanh tren co no (dong ~1558) nhung nhanh stub thi quen.
// Tra 0 = "khong ro thao tac cuoi" (man STATUS/chan doan chi doc de hien thi).
uint8_t cyd_sd_last_op(void) { return 0; }

#endif  // CYDLCD_WIFI_UPLOAD
