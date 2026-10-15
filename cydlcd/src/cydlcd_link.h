/**
 * cydlcd_link.h -- Phia MAN HINH CYD: nhan frame PUSH tu Marlin + gui lenh G-code len.
 *
 * Day la ban thay the cho marlin_link.cpp (kieu cu: MAN HINH CYD tu hoi M105/M114/M27). Kieu moi:
 *   - Marlin day STATUS / TEMPS / POSITION / PROGRESS / FANS / SETTINGS / MESH / FILES / EVENT /
 *     PROMPT theo frame nhi phan (cydlcd_protocol.h) => MAN HINH CYD co DU du lieu de ve moi khoi.
 *   - MAN HINH CYD chi gui lenh G-code cho HANH DONG (G28, M104, M140, M24, M524...) va vai REQUEST
 *     (xin lai danh sach file / luoi mesh / thong so).
 *
 * Khong phu thuoc LVGL: dung duoc cho ca UI LVGL lan UI tu ve. Khong cap phat dong.
 */
#pragma once

#include <stdint.h>
#include "cydlcd_protocol.h"

#define CYDLCD_MAX_FILES      32      // so file nho toi da trong 1 lan liet ke
#define CYDLCD_FILE_NAME_LEN  32
#define CYDLCD_FILE_SFN_LEN   16      // ten 8.3 (vd "V300_C~1.GCO") - proto >= 4, xem cydlcd_file_t
// 2026-10-15: nang 56 -> 72. LOI THAT: Marlin gui 56 thong so = DUNG BANG gioi han cu => 2 thong so gui
// CUOI (M200 D + M200 S) bi BO AM THAM (cydlcd_link.cpp: chi luu khi n_settings < MAX, khong bao loi) =>
// bang THONG SO thieu dong ma khong hieu vi sao. So luong thuc te: ~41 Marlin (them 6 MPC + fade + 2 M206 +
// 2 M200 + 2 M603 = ~52) + 9 preheat + du phong.
// (Moi muc = u16 id + i32 value = 8 byte -> tang 16 muc = 128 byte RAM tinh.)
#define CYDLCD_SETTINGS_MAX   72
#define CYDLCD_MESH_MAX       16      // UBL 7x7 (hoac 10x10) -> 16 la du
#define CYDLCD_TEXT_LEN       48

typedef struct { uint8_t id; int16_t cur, tgt; } cydlcd_temp_t;
typedef struct { uint8_t idx; int16_t pct; }     cydlcd_fan_t;
typedef struct { uint16_t id; int32_t value; }   cydlcd_setting_val_t;
// 1 muc trong danh sach file cua Marlin.
//   name  = ten DAI (LFN) de HIEN thi - co the bi SdFat CAT o 26 ky tu (LONG_FILENAME_LENGTH 27) nen
//           ten dai se mat ca phan mo rong;
//   sfn   = TEN 8.3 Marlin gui kem (proto >= 4) - dung de GUI LENH (`M23 <sfn>`): ten 8.3 luon khop
//           voi ten that tren the, con ten dai da bi cat thi `M23` khong bao gio mo duoc file.
//           Rong khi Marlin la ban cu (proto 3) -> lui ve dung `name` nhu truoc.
typedef struct { char name[CYDLCD_FILE_NAME_LEN]; char sfn[CYDLCD_FILE_SFN_LEN]; uint8_t flags; } cydlcd_file_t;

/**
 * true = `name` (chi khi `flags` bao la THU MUC) la THU MUC HE THONG trong CYDLCD_HIDE_DIRS
 * (`cydlcd_tmp`, `BTUP`). Danh sach Marlin gui xuong VAN giu DU (cydlcd.files[]/n_files); man hinh
 * chi AN cac muc nay khi hien (ui_page_files.cpp: files_view_build()).
 */
bool cydlcd_hidden_dir(const char *name, uint8_t flags);

typedef struct {
  // ---- lien ket ----
  bool     linked;                 // con nhan duoc frame trong CYDLCD_LINK_TIMEOUT_MS
  uint32_t last_rx_ms;
  uint8_t  proto;
  uint16_t marlin_fw;
  uint8_t  hello_flags, n_heaters, n_fans_max;
  char     name[24];
  char     marlin_ver[24];         // proto >= 2: SHORT_BUILD_VERSION Marlin gui trong HELLO
                                   // (MAN HINH CYD ban cu khong gui -> chuoi rong)

  // ---- trang thai may ----
  uint8_t  state;                  // cydlcd_state_t
  uint8_t  flags;                  // cydlcd_status_flags_t
  uint8_t  progress_pct;

  // ---- du lieu ----
  cydlcd_temp_t temps[8];  uint8_t n_temps;
  struct { int32_t x, y, z, e; int16_t feed_pct, flow_pct; } pos;
  struct { uint8_t pct; uint32_t elapsed_s, remaining_s; char file[CYDLCD_FILE_NAME_LEN]; } prog;
  cydlcd_fan_t fans[4];    uint8_t n_fans;

  // ---- endstop/probe (CYDLCD_F_ENDSTOPS, kieu M119) ----
  // es_hit[id] = 1 khi cham; id theo cydlcd_endstop_id_t (0 PROBE, 1 X_MIN, 2 Y_MIN,
  // 3 Z_MIN, 4 X_MAX, 5 Y_MAX). May dung Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN thi
  // Marlin bao chan Z_MIN voi id 0 (PROBE) va khong bao id 3.
  uint8_t  es_hit[CYDLCD_ENDSTOP_MAX]; uint8_t n_endstops;

  // ---- thong bao / su kien moi nhat ----
  char     message[CYDLCD_TEXT_LEN]; uint8_t message_sev; uint32_t message_ms;
  uint8_t  event; uint16_t event_v1, event_v2; uint32_t event_ms;
  uint32_t pid_done_ms;            // moc thoi gian CYDLCD_EV_PID_DONE (0 = chua xong lan nao)

  // ---- prompt dang cho tra loi ----
  bool     prompt_active; uint8_t prompt_id, prompt_type; char prompt_text[CYDLCD_TEXT_LEN];

  // ---- cau hinh (de hien thi) ----
  cydlcd_setting_val_t settings[CYDLCD_SETTINGS_MAX]; uint8_t n_settings;

  // ---- bed mesh ----
  int16_t  mesh[CYDLCD_MESH_MAX][CYDLCD_MESH_MAX]; uint8_t mesh_nx, mesh_ny, mesh_rows; bool mesh_valid;
  // BED LEVELING dang BAT hay TAT (byte thu 4 cua CYDLCD_F_STATUS, tu 2026-10-15):
  // = planner.leveling_active cua Marlin (UBL: G29 A/G29 D, ABL: M420 S1/S0...).
  // Man BED MESH hien "UBL ON/OFF" theo co nay - truoc day bam UBL ON/OFF xong khong biet da an hay chua.
  bool     mesh_on;

  // ---- danh sach file SD ----
  cydlcd_file_t files[CYDLCD_MAX_FILES]; uint8_t n_files; bool files_done, files_pending;

  // ---- thong ke loi: 2 CO MOT CHIEU (chi bat true, KHONG BAO GIO ve false) ----
  // Truoc day la 2 BO DEM uint16_t: nhip that ~4 frame/s -> TRAN sau 65 536 frame (~4,5 gio).
  // Tran ve 0 khong chi lam hien thi sai ma con lam dieu kien TAT SNIFFER TRAM trong
  // cydlcd_link.cpp (`if (cydlcd.rx_frames)`) tro nen SAI -> sniffer bat lai giua chung va co the
  // an byte cua frame chua "TBT" (dung loi da tung gap). Dung CO thi khong con so de tran.
  //
  // VI SAO GIU TEN CU (rx_frames/rx_errors) chu khong doi thanh rx_ok/rx_err: main.cpp - dong log
  // debug "[CYD] ... rx=%d err=%d" - van doc cydlcd.rx_frames / cydlcd.rx_errors (ep (int), xem
  // main.cpp:63-65), ma main.cpp KHONG duoc sua (nhanh upload dang sua song song, sua vao la ghi
  // de code cua ho). Gia tri in ra van dung: rx=1 khi da nhan duoc frame hop le / 0 khi chua;
  // err=1 khi da tung gap frame loi. Muon doi ten cho dep thi phai sua them 2 dong trong main.cpp
  // (cydlcd.rx_ok / cydlcd.rx_err) - luc do moi doi ten o day duoc.
  bool rx_frames;    // CO: da nhan duoc it nhat 1 frame PUSH hop le
  bool rx_errors;    // CO: da tung gap frame loi (CRC sai hoac do dai payload qua lon)
} cydlcd_data_t;

extern cydlcd_data_t cydlcd;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void cydlcd_link_begin(const uint32_t baud);
void cydlcd_link_task();                                  // 1 vong phuc vu serial (goi tu task `link`)

// ---------------------------------------------------------------------------
// DA LUONG (2026-10-15, xem cydlcd/docs/ke-hoach-da-luong.md):
//  - `cydlcd_link_start_task()`: tao task `link` (core 1, uu tien 20) - task DUY NHAT cham vao UART Marlin
//    va phuc vu sector SD cho Marlin. Goi 1 lan trong setup(). Tra false neu khong tao duoc task.
//  - `cydlcd` (struct trang thai) la BAN CONG KHAI cho nguoi doc: task `link` ghi vao ban rieng cua no roi
//    "publish" sang `cydlcd`. `cydlcd_link_state_lock()`/`unlock()` giu khoa trong luc nguoi doc doc nhieu
//    truong lien tiep (vd ca 1 lan cap nhat man hinh) - task `link` KHONG BAO GIO cho khoa nay (timeout 0,
//    ban ghi van nam o ban rieng nen khong mat du lieu, chi cham vai ms).
// ---------------------------------------------------------------------------
#if defined(CYDLCD_BOARD_CYD)
  bool cydlcd_link_start_task(void);
#else
  static inline bool cydlcd_link_start_task(void) { return true; }
#endif
void cydlcd_link_state_lock(void);
void cydlcd_link_state_unlock(void);
// YEU CAU (tu task khac, KHONG BAO GIO chan): xoa danh sach file de xin lai tu dau. Thay cho viec ghi thang
// `cydlcd.files_done = false; cydlcd.n_files = 0;` (task `link` la chu so huu trang thai - xem ke hoach).
void cydlcd_link_reset_files(void);
bool cydlcd_link_connected();

// Hang doi TX (xem cydlcd_link.cpp): frame duoc gui NGAY khi TX ring du cho, con lai xep hang doi
// va trai dan o day. Nho vay KHONG BAO GIO chan vong lap du Marlin tam thoi khong doc UART.
void     cydlcd_link_tx_task();                           // trai hang doi (goi trong cydlcd_link_task)
uint32_t cydlcd_link_tx_dropped();                        // so frame bi bo (hang doi day) - chan doan
uint8_t  cydlcd_link_tx_pending();                        // so frame dang cho trong hang doi

// Chan doan chieu MARLIN -> MAN HINH (lenh `UI RX` tren COM8): so frame da nhan theo LOAI + tuoi PONG.
// VI SAO CAN: hai chieu cua UART hong DOC LAP nhau - Marlin van day STATUS deu (chieu xuong OK) trong khi
// lenh tu man hinh khong toi Marlin (chieu len hong). Nhin "frame nao den" la biet ngay ben nao hong.
uint16_t cydlcd_link_rx_count(uint8_t type);              // so frame loai `type` da nhan
uint32_t cydlcd_link_pong_age_ms();                       // tu lan cuoi nhan PONG (0xFFFFFFFF = chua bao gio)
uint16_t cydlcd_link_pong_lost();                         // so lan phat hien "mat PONG > 5 s"

void cydlcd_link_ping();
void cydlcd_link_send_gcode(const char *line);            // 1 dong, khong can '\n'
void cydlcd_link_send_gcode2(const char *a, const char *b); // 2 dong lien tiep
void cydlcd_link_request(const uint8_t what);             // cydlcd_request_t
void cydlcd_link_request_dir(const char *dirname);        // vao thu muc roi liet ke

// Tra loi Marlin 1 sector 512 B cua the SD (che do CYDLCD_SD_UART - xem README muc 12):
// gui CYDLCD_SD_CHUNKS frame CYDLCD_F_SDDATA. status != OK -> chi gui 1 frame (n = 0).
#if CYDLCD_SD_UART
  void cydlcd_link_send_sd_data(uint8_t status, uint32_t sector, const uint8_t *data);
  void cydlcd_link_send_sd_ack(uint8_t status, uint32_t sector);   // tra loi lenh GHI (P3)
#endif
void cydlcd_link_answer(const uint8_t answer);            // tra loi prompt dang cho (1 = OK)
void cydlcd_link_send_tbt_cmd(char cmd);                 // lenh Tramming Wizard P/D/M/R/B/Q (frame 0x44)

// Doi so nguyen trong frame thanh don vi that
static inline float cydlcd_mm(const int32_t v)   { return (float)v / CYDLCD_POS_SCALE; }
static inline float cydlcd_celsius(const int16_t v) { return (float)v / CYDLCD_TEMP_SCALE; }
static inline float cydlcd_pct(const int16_t v)  { return (float)v / CYDLCD_PCT_SCALE; }
static inline float cydlcd_val(const int32_t v)  { return (float)v / CYDLCD_VAL_SCALE; }

// Lay 1 thong so (CYDLCD_SET_*); tra ve 'def' neu Marlin chua gui
int32_t cydlcd_setting(const uint16_t id, const int32_t def);

// Ghi 1 thong so vao bang `cydlcd.settings[]` (them neu chua co) - dung cho thong so do MAN HINH CYD tu
// quan ly (vd 9 preset preheat id 0x40xx): Marlin khong gui chung nen MAN HINH CYD phai tu bom vao bang
// de trang SETTINGS hien/sua duoc.
void cydlcd_setting_set(uint16_t id, int32_t value);
