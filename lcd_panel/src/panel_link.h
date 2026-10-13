/**
 * panel_link.h -- Phia PANEL: nhan frame PUSH tu Marlin + gui lenh G-code len.
 *
 * Day la ban thay the cho marlin_link.cpp (kieu cu: panel tu hoi M105/M114/M27). Kieu moi:
 *   - Marlin day STATUS / TEMPS / POSITION / PROGRESS / FANS / SETTINGS / MESH / FILES / EVENT /
 *     PROMPT theo frame nhi phan (panel_protocol.h) => panel co DU du lieu de ve moi khoi.
 *   - Panel chi gui lenh G-code cho HANH DONG (G28, M104, M140, M24, M524...) va vai REQUEST
 *     (xin lai danh sach file / luoi mesh / thong so).
 *
 * Khong phu thuoc LVGL: dung duoc cho ca UI LVGL lan UI tu ve. Khong cap phat dong.
 */
#pragma once

#include <stdint.h>
#include "panel_protocol.h"

#define PANEL_MAX_FILES      32      // so file nho toi da trong 1 lan liet ke
#define PANEL_FILE_NAME_LEN  32
#define PANEL_SETTINGS_MAX   48
#define PANEL_MESH_MAX       16      // UBL 7x7 (hoac 10x10) -> 16 la du
#define PANEL_TEXT_LEN       48

typedef struct { uint8_t id; int16_t cur, tgt; } panel_temp_t;
typedef struct { uint8_t idx; int16_t pct; }     panel_fan_t;
typedef struct { uint16_t id; int32_t value; }   panel_setting_val_t;
typedef struct { char name[PANEL_FILE_NAME_LEN]; uint8_t flags; } panel_file_t;

typedef struct {
  // ---- lien ket ----
  bool     linked;                 // con nhan duoc frame trong PANEL_LINK_TIMEOUT_MS
  uint32_t last_rx_ms;
  uint8_t  proto;
  uint16_t marlin_fw;
  uint8_t  hello_flags, n_heaters, n_fans_max;
  char     name[24];

  // ---- trang thai may ----
  uint8_t  state;                  // panel_state_t
  uint8_t  flags;                  // panel_status_flags_t
  uint8_t  progress_pct;

  // ---- du lieu ----
  panel_temp_t temps[8];  uint8_t n_temps;
  struct { int32_t x, y, z, e; int16_t feed_pct, flow_pct; } pos;
  struct { uint8_t pct; uint32_t elapsed_s, remaining_s; char file[PANEL_FILE_NAME_LEN]; } prog;
  panel_fan_t fans[4];    uint8_t n_fans;

  // ---- thong bao / su kien moi nhat ----
  char     message[PANEL_TEXT_LEN]; uint8_t message_sev; uint32_t message_ms;
  uint8_t  event; uint16_t event_v1, event_v2; uint32_t event_ms;

  // ---- prompt dang cho tra loi ----
  bool     prompt_active; uint8_t prompt_id, prompt_type; char prompt_text[PANEL_TEXT_LEN];

  // ---- cau hinh (de hien thi) ----
  panel_setting_val_t settings[PANEL_SETTINGS_MAX]; uint8_t n_settings;

  // ---- bed mesh ----
  int16_t  mesh[PANEL_MESH_MAX][PANEL_MESH_MAX]; uint8_t mesh_nx, mesh_ny, mesh_rows; bool mesh_valid;

  // ---- danh sach file SD ----
  panel_file_t files[PANEL_MAX_FILES]; uint8_t n_files; bool files_done, files_pending;

  // ---- thong ke loi ----
  uint16_t rx_frames, rx_errors;
} panel_data_t;

extern panel_data_t panel;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void panel_link_begin(const uint32_t baud);
void panel_link_task();                                  // goi lien tuc trong loop()
bool panel_link_connected();

void panel_link_ping();
void panel_link_send_gcode(const char *line);            // 1 dong, khong can '\n'
void panel_link_send_gcode2(const char *a, const char *b); // 2 dong lien tiep
void panel_link_request(const uint8_t what);             // panel_request_t
void panel_link_request_dir(const char *dirname);        // vao thu muc roi liet ke
void panel_link_answer(const uint8_t answer);            // tra loi prompt dang cho (1 = OK)
void panel_link_send_tram_cmd(char cmd);                 // lenh Tramming Wizard P/D/M/R/B/Q (frame 0x44)

// Doi so nguyen trong frame thanh don vi that
static inline float panel_mm(const int32_t v)   { return (float)v / PANEL_POS_SCALE; }
static inline float panel_celsius(const int16_t v) { return (float)v / PANEL_TEMP_SCALE; }
static inline float panel_pct(const int16_t v)  { return (float)v / PANEL_PCT_SCALE; }
static inline float panel_val(const int32_t v)  { return (float)v / PANEL_VAL_SCALE; }

// Lay 1 thong so (PANEL_SET_*); tra ve 'def' neu Marlin chua gui
int32_t panel_setting(const uint16_t id, const int32_t def);
