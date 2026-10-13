/**
 * panel_protocol.h -- Giao thuc day du lieu (PUSH) giua Marlin va MCU man hinh.
 *
 *   Marlin (ExtUI "panel_serial", src/lcd/extui/panel_serial)  <--UART-->  lcd_panel
 *
 * VI SAO PUSH, KHONG PHAI G-CODE:
 *   Man hinh co MCU rieng KHONG doc duoc bien noi bo cua Marlin. Neu panel tu hoi bang
 *   G-code (M105/M114/M27) thi chi lay duoc vai con so, con rat nhieu "khoi" khong the
 *   dien: danh sach file SD, luoi bed mesh, thong so PID/steps/mm/jerk, trang thai
 *   endstop/probe, prompt cua nguoi dung, ten file dang in... (MKS TFT lam dung kieu
 *   G-code nay va phai them hang loat M-code rieng — xem makerbase-mks/MKS-TFT*-Firmware.)
 *   => Marlin CHU DONG gui frame nhi phan xuong panel; panel chi gui lenh G-code cho
 *      HANH DONG (G28/M104/M140/M24...) chu khong dung G-code de VE.
 *
 * FILE DUNG CHUNG: ban y nguyen van nam o
 *      Marlin/src/lcd/extui/panel_serial/panel_protocol.h   (Marlin)
 *      lcd_panel/src/panel_protocol.h                        (panel)
 *   Sua file nay thi phai copy y nguyen sang file kia (header chi dung <stdint.h>).
 *
 * KHUNG TRUYEN (little-endian):
 *      [0] 0xA5  SYNC0        (PANEL_SYNC0)
 *      [1] 0x5A  SYNC1        (PANEL_SYNC1)
 *      [2] type               (panel_frame_t)
 *      [3] seq                tang dan moi frame, de doi chieu PING/PONG
 *      [4] len                so byte payload (0..PANEL_MAX_PAYLOAD)
 *      [5..] payload
 *      [5+len] crc8           CRC8 (poly 0x07) tren type,seq,len,payload
 *
 * KIEU DU LIEU TRONG PAYLOAD:
 *      u8/u16/u32 : so nguyen khong dau, little-endian
 *      i16/i32    : so nguyen co dau, little-endian
 *      str        : u8 len + len byte (khong co ky tu ket thuc, cat bot neu dai)
 *      blob       : u8 len + len byte (vi du diem mesh)
 *      nhiet do   : i16 = do C * PANEL_TEMP_SCALE
 *      vi tri     : i32 = mm * PANEL_POS_SCALE
 *      phan tram  : i16 = % * PANEL_PCT_SCALE
 */
#pragma once

#include <stdint.h>

#define PANEL_PROTO_VERSION     3     // 2: HELLO them str version; them F_ENDSTOPS + REQ_ENDSTOPS/DFU
                                      // 3: them F_SDREQ/F_SDDATA (Marlin lay sector the SD qua UART)
#define PANEL_SYNC0             0xA5
#define PANEL_SYNC1             0x5A
// LUU Y: truong `len` trong khung chi co 1 BYTE (buf[4]) => payload toi da 255 byte. Vi vay 1 sector
// 512 byte KHONG the nam trong 1 frame: PANEL_F_SDDATA chia thanh PANEL_SD_CHUNKS frame, moi frame
// PANEL_SD_CHUNK byte (xem khoi "SD qua UART" o duoi). Giu nguyen 192 cho tuong thich voi moi frame
// khac (STATUS/TRAMES/MESH...) va voi tools/ test tren may tinh.
#define PANEL_MAX_PAYLOAD       192
#define PANEL_FRAME_OVERHEAD    6                     // sync0 sync1 type seq len crc
#define PANEL_MAX_FRAME         (PANEL_MAX_PAYLOAD + PANEL_FRAME_OVERHEAD)

// He so quy doi (float -> so nguyen)
#define PANEL_TEMP_SCALE        100.0f                // do C  -> i16
#define PANEL_POS_SCALE         1000.0f               // mm    -> i32 (micromet)
#define PANEL_PCT_SCALE         100.0f                // %     -> i16
#define PANEL_VAL_SCALE         1000.0f               // gia tri chung -> i32

// ---------------------------------------------------------------------------
// Loai frame
// ---------------------------------------------------------------------------
enum panel_frame_t : uint8_t {
  // ---- Marlin -> panel (push) ----
  PANEL_F_HELLO     = 0x01,   // u16 fw_version, u8 proto, u8 flags, u8 n_heaters, u8 n_fans, str name
                              // [proto >= 2] str version (SHORT_BUILD_VERSION cua Marlin)
  PANEL_F_STATUS    = 0x02,   // u8 state, u8 flags, u8 progress_percent
  PANEL_F_TEMPS     = 0x03,   // u8 n, n * (u8 id, i16 cur, i16 tgt)
  PANEL_F_POSITION  = 0x04,   // i32 x, i32 y, i32 z, i32 e, u16 feed_mm_s*10, i16 feed_pct, i16 flow_pct
  PANEL_F_PROGRESS  = 0x05,   // u8 pct, u32 elapsed_s, u32 remaining_s, str filename
  PANEL_F_MESSAGE   = 0x06,   // u8 severity, str text
  PANEL_F_FANS      = 0x07,   // u8 n, n * (u8 idx, i16 pct)
  PANEL_F_MESH      = 0x08,   // u8 ny, u8 row, u8 nx, nx * i16 z_um   (gui tung hang mot)
  PANEL_F_FILES     = 0x09,   // u8 flags, u32 size, str name  (1 file/frame; flags=0xFF & name rong = het danh sach)
  PANEL_F_EVENT     = 0x0A,   // u8 event, u16 v1, u16 v2, str text (v1/v2 tuy event, vd TONE: freq/duration)
  PANEL_F_SETTING   = 0x0B,   // u16 id, i32 value
  PANEL_F_PROMPT    = 0x0C,   // u8 prompt_id, u8 type, str text
  PANEL_F_PONG      = 0x0D,   // u8 echo_seq
  PANEL_F_TRAM      = 0x0E,   // str: dung 1 dong ASCII "TRAM ...\\n" (dinh dang o tram_protocol.h;
                              //      2026-10-13 - port tu che do TRAM link), gui ~5Hz khi wizard chay
  PANEL_F_ENDSTOPS  = 0x0F,   // u8 n, n * (u8 id (panel_endstop_id_t), u8 hit) -- trang thai kieu M119
  PANEL_F_SDREQ     = 0x10,   // [proto >= 3] YEU CAU DOC/GHI the SD (Marlin -> panel, vi the SD nam
                              // o khe man hinh): u8 op (panel_sd_op_t), u32 sector.
                              // (op = WRITE: theo sau la PANEL_SD_CHUNKS frame nua - xem duoi.)

  // ---- panel -> Marlin ----
  PANEL_F_GCODE     = 0x40,   // str: 1 dong G-code (khong can '\\n')
  PANEL_F_REQUEST   = 0x41,   // u8 what (panel_request_t), [str tham so]
  PANEL_F_PING      = 0x42,   // u8 seq
  PANEL_F_ANSWER    = 0x43,   // u8 prompt_id, u8 answer
  PANEL_F_TRAM_CMD  = 0x44,   // u8 lenh: 'P' probe, 'D' done, 'M' doi MAX/MIN,
                              //          'R' home+probe lai, 'B' thoat wizard, 'Q' hoi trang thai
  PANEL_F_SDDATA    = 0x45,   // [proto >= 3] DU LIEU SECTOR, dung cho CA 2 CHIEU:
                              //   panel -> Marlin (tra loi lenh DOC): u8 status, u32 sector,
                              //       u8 idx (0..3), u8 n, n byte  (status != OK: chi 1 frame, n = 0)
                              //   Marlin -> panel (gui du lieu lenh GHI): u8 status (=0), u32 sector,
                              //       u8 idx (0..3), u8 n (=128), n byte
                              //   -> 1 sector 512 B = PANEL_SD_CHUNKS frame, moi frame PANEL_SD_CHUNK byte
  PANEL_F_SDACK     = 0x46    // [proto >= 3] panel -> Marlin: u8 status, u32 sector
                              //   (tra loi 1 lenh GHI sau khi da ghi xong xuong the)
};

// ---------------------------------------------------------------------------
// SD qua UART (proto >= 3): Marlin KHONG tu doc the SPI nua, moi sector duoc hoi panel.
//   1 sector 512 B = PANEL_SD_CHUNKS frame, moi frame PANEL_SD_CHUNK byte.
//   Payload 1 frame = 1 + 4 + 1 + 1 + 128 = 135 byte (<= PANEL_MAX_PAYLOAD 192).
// ---------------------------------------------------------------------------
#define PANEL_SD_SECTOR     512
#define PANEL_SD_CHUNK      128
#define PANEL_SD_CHUNKS     (PANEL_SD_SECTOR / PANEL_SD_CHUNK)   // 4
enum panel_sd_op_t : uint8_t {
  PANEL_SD_OP_READ  = 0,      // doc sector -> panel tra PANEL_SD_CHUNKS frame
  PANEL_SD_OP_WRITE = 1       // ghi sector -> Marlin gui kem PANEL_SD_CHUNKS frame, panel tra SDACK
};
enum panel_sd_status_t : uint8_t {
  PANEL_SD_ST_OK    = 0,      // thanh cong
  PANEL_SD_ST_ERR   = 1,      // loi the / sector ngoai pham vi
  PANEL_SD_ST_NOCARD= 2       // panel khong co the
};

// ---------------------------------------------------------------------------
// HELLO flags (panel biet mainboard co gi)
// ---------------------------------------------------------------------------
enum panel_hello_flags_t : uint8_t {
  PANEL_HAS_SD        = 0x01,
  PANEL_HAS_MESH      = 0x02,
  PANEL_HAS_PROBE     = 0x04,
  PANEL_HAS_FILAMENT  = 0x08,
  PANEL_HAS_BED       = 0x10,
  PANEL_HAS_CHAMBER   = 0x20,
  PANEL_HAS_POWERLOSS = 0x40,
  PANEL_HAS_TMC       = 0x80
};

// ---------------------------------------------------------------------------
// Trang thai may
// ---------------------------------------------------------------------------
enum panel_state_t : uint8_t {
  PANEL_ST_IDLE = 0,
  PANEL_ST_PRINTING,
  PANEL_ST_PAUSED,
  PANEL_ST_HOMING,
  PANEL_ST_LEVELING,
  PANEL_ST_RUNOUT,
  PANEL_ST_KILLED,
  PANEL_ST_BUSY
};

enum panel_status_flags_t : uint8_t {
  PANEL_SF_HOMED       = 0x01,   // da home it nhat 1 lan
  PANEL_SF_MEDIA       = 0x02,   // co the SD
  PANEL_SF_SD_PRINTING = 0x04,
  PANEL_SF_PAUSED      = 0x08,
  PANEL_SF_LEVELING    = 0x10,   // dang/co bed leveling
  PANEL_SF_MESH_VALID  = 0x20,
  PANEL_SF_RUNOUT      = 0x40,
  PANEL_SF_STEPPERS    = 0x80    // dong co dang giu (enabled)
};

// ---------------------------------------------------------------------------
// Su kien (PANEL_F_EVENT) -- gui ngay khi xay ra
// ---------------------------------------------------------------------------
enum panel_event_t : uint8_t {
  PANEL_EV_BOOT = 0,
  PANEL_EV_MEDIA_INSERTED,
  PANEL_EV_MEDIA_REMOVED,
  PANEL_EV_MEDIA_ERROR,
  PANEL_EV_PRINT_START,
  PANEL_EV_PRINT_PAUSE,
  PANEL_EV_PRINT_RESUME,
  PANEL_EV_PRINT_STOP,
  PANEL_EV_PRINT_DONE,
  PANEL_EV_HOMING_START,
  PANEL_EV_HOMING_DONE,
  PANEL_EV_LEVELING_START,
  PANEL_EV_LEVELING_DONE,
  PANEL_EV_STEPPERS_ON,
  PANEL_EV_STEPPERS_OFF,
  PANEL_EV_FILAMENT_RUNOUT,
  PANEL_EV_TONE,
  PANEL_EV_FACTORY_RESET,
  PANEL_EV_SETTINGS_STORED,
  PANEL_EV_SETTINGS_LOADED,
  PANEL_EV_PID_DONE,
  PANEL_EV_POWERLOSS_RESUME
};

// Muc do thong bao
enum panel_severity_t : uint8_t {
  PANEL_SEV_INFO = 0,
  PANEL_SEV_WARN,
  PANEL_SEV_ERROR,
  PANEL_SEV_FATAL
};

// ---------------------------------------------------------------------------
// Yeu cau tu panel (PANEL_F_REQUEST)
// ---------------------------------------------------------------------------
enum panel_request_t : uint8_t {
  PANEL_REQ_FULL = 0,        // gui lai HELLO + STATUS + TEMPS + POSITION + FANS + SETTINGS
  PANEL_REQ_FILES,           // liet ke file trong thu muc SD hien tai
  PANEL_REQ_FILES_UP,        // len thu muc cha (payload them str ten thu muc?)
  PANEL_REQ_FILES_DIR,       // vao thu muc (payload them str ten thu muc)
  PANEL_REQ_MESH,            // gui luoi bed mesh
  PANEL_REQ_SETTINGS,        // gui cac thong so dang hien thi
  PANEL_REQ_SAVE_SETTINGS,   // M500
  PANEL_REQ_ABORT,           // dung in ngay (M524/M410)
  PANEL_REQ_ENDSTOPS,        // gui trang thai endstop/probe (PANEL_F_ENDSTOPS) -- kieu M119
  PANEL_REQ_DFU              // nhay vao ROM DFU bootloader (chi khi may ranh; STM32_DFU_REBOOT)
};

// ---------------------------------------------------------------------------
// ID thong so (PANEL_F_SETTING) -- chi de HIEN THI/lay gia tri; muon sua thi
// panel gui G-code (M92/M203/M204/M206/M301/M304/M851/M290...) chu khong ghi truc tiep.
// ---------------------------------------------------------------------------
enum panel_setting_t : uint16_t {
  PANEL_SET_Z_OFFSET = 1,        // mm
  PANEL_SET_FEEDRATE_PCT,        // %
  PANEL_SET_FLOW_PCT,            // %  (E0)
  PANEL_SET_STEPS_MM_X,          // steps/mm
  PANEL_SET_STEPS_MM_Y,
  PANEL_SET_STEPS_MM_Z,
  PANEL_SET_STEPS_MM_E,
  PANEL_SET_MAXFEED_X,           // mm/s
  PANEL_SET_MAXFEED_Y,
  PANEL_SET_MAXFEED_Z,
  PANEL_SET_MAXFEED_E,
  PANEL_SET_MAXACCEL_X,          // mm/s^2
  PANEL_SET_MAXACCEL_Y,
  PANEL_SET_MAXACCEL_Z,
  PANEL_SET_MAXACCEL_E,
  PANEL_SET_JERK_X,
  PANEL_SET_JERK_Y,
  PANEL_SET_JERK_Z,
  PANEL_SET_JERK_E,
  PANEL_SET_PID_E_KP,
  PANEL_SET_PID_E_KI,
  PANEL_SET_PID_E_KD,
  PANEL_SET_PID_BED_KP,
  PANEL_SET_PID_BED_KI,
  PANEL_SET_PID_BED_KD,
  PANEL_SET_LINEAR_ADV_E,        // s
  PANEL_SET_JUNCTION_DEV,        // mm (khi dung junction deviation thay cho jerk)
  PANEL_SET_PROBE_OFFSET_X,      // mm
  PANEL_SET_PROBE_OFFSET_Y,
  PANEL_SET_PROBE_OFFSET_Z,
  PANEL_SET_TARGET_TEMP_E0,      // do C
  PANEL_SET_TARGET_TEMP_BED,     // do C
  PANEL_SET_FAN_PCT_E0,          // %
  PANEL_SET_MAXACCEL_PRINT,      // mm/s^2 (planner.settings.acceleration, M204 S)
  PANEL_SET_MAXACCEL_RETRACT,    // mm/s^2 (planner.settings.retract_acceleration, M204 R)
  PANEL_SET_MAXACCEL_TRAVEL,     // mm/s^2 (planner.settings.travel_acceleration, M204 T)
  PANEL_SET_TMC_CURRENT_X,       // mA (stepperX.val_mA - chi khi HAS_TRINAMIC_CONFIG)
  PANEL_SET_TMC_CURRENT_Y,       // mA
  PANEL_SET_TMC_CURRENT_Z,       // mA
  PANEL_SET_TMC_CURRENT_E,       // mA (E0)
  PANEL_SET_RUNOUT_EN            // 1 = bat cam bien het filament (runout.enabled)
  // GHI CHU: 3 preset preheat (PLA/PETG/ABS) KHONG nam o day vi ban nay khong co M145/HAS_PREHEAT.
  // Panel tu luu chung trong NVS voi "id cuc bo" 0x40xx (xem ui_lvgl.cpp khoi "PREHEAT presets").
};

// Prompt (PANEL_F_PROMPT)
enum panel_prompt_t : uint8_t {
  PANEL_PROMPT_NONE = 0,
  PANEL_PROMPT_USER_CONFIRM,     // "echo: ... please confirm" -> answer 1 = OK
  PANEL_PROMPT_FILAMENT_RUNOUT,
  PANEL_PROMPT_RESUME_PRINT,
  PANEL_PROMPT_FILAMENT_CHANGE
};

// ---------------------------------------------------------------------------
// ID trong frame TEMPS (PANEL_F_TEMPS): 0 = bed, 1..6 = E0..E5
// ---------------------------------------------------------------------------
enum panel_temp_id_t : uint8_t {
  PANEL_TEMP_BED     = 0,
  PANEL_TEMP_E0      = 1,
  PANEL_TEMP_CHAMBER = 0xFE,
  PANEL_TEMP_COOLER  = 0xFF
};

// ---------------------------------------------------------------------------
// ID trong frame ENDSTOPS (PANEL_F_ENDSTOPS): 1 byte id + 1 byte hit (0/1)
// May dung Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN thi Z_MIN chinh la chan probe
// => bao id PANEL_ES_PROBE (0), KHONG bao them PANEL_ES_Z_MIN (3).
// ---------------------------------------------------------------------------
#define PANEL_ENDSTOP_MAX 6
enum panel_endstop_id_t : uint8_t {
  PANEL_ES_PROBE = 0,
  PANEL_ES_X_MIN = 1,
  PANEL_ES_Y_MIN = 2,
  PANEL_ES_Z_MIN = 3,
  PANEL_ES_X_MAX = 4,
  PANEL_ES_Y_MAX = 5
};

// ---------------------------------------------------------------------------
// Tien ich dung chung (header-only)
// ---------------------------------------------------------------------------
static inline uint8_t panel_crc8_update(uint8_t crc, const uint8_t c) {
  crc ^= c;
  for (uint8_t i = 0; i < 8; i++)
    crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  return crc;
}

static inline uint8_t panel_crc8(const uint8_t *p, uint8_t n) {
  uint8_t crc = 0;
  while (n--) {
    crc ^= *p++;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

static inline uint16_t panel_u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static inline uint32_t panel_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline int16_t  panel_i16(const uint8_t *p) { return (int16_t)panel_u16(p); }
static inline int32_t  panel_i32(const uint8_t *p) { return (int32_t)panel_u32(p); }

static inline uint8_t panel_i16_to(const int16_t v, uint8_t *p) {
  p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((uint16_t)v >> 8);
  return 2;
}
static inline uint8_t panel_i32_to(const int32_t v, uint8_t *p) {
  p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((uint32_t)v >> 8);
  p[2] = (uint8_t)((uint32_t)v >> 16); p[3] = (uint8_t)((uint32_t)v >> 24);
  return 4;
}
static inline uint8_t panel_u16_to(const uint16_t v, uint8_t *p) {
  p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  return 2;
}
static inline uint8_t panel_u32_to(const uint32_t v, uint8_t *p) {
  p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
  return 4;
}

// ---------------------------------------------------------------------------
// Bo TAO frame -- dung chung cho CA HAI phia (Marlin va panel)
// ---------------------------------------------------------------------------
typedef struct {
  uint8_t buf[PANEL_MAX_FRAME];
  uint8_t len;      // so byte payload da ghi (buf[4] duoc cap nhat khi finish)
  uint8_t seq;      // so thu tu frame, tu tang
} panel_tx_t;

static inline void panel_tx_begin(panel_tx_t *tx, const uint8_t type) {
  tx->len = 0;
  tx->buf[0] = PANEL_SYNC0;
  tx->buf[1] = PANEL_SYNC1;
  tx->buf[2] = type;
  tx->buf[3] = tx->seq++;
  tx->buf[4] = 0;
}
static inline void panel_tx_raw(panel_tx_t *tx, const uint8_t *b, uint8_t n) {
  while (n--) if (tx->len < (PANEL_MAX_PAYLOAD)) tx->buf[5 + tx->len++] = *b++;
}
static inline void panel_tx_u8(panel_tx_t *tx, const uint8_t v) { if (tx->len < (PANEL_MAX_PAYLOAD)) tx->buf[5 + tx->len++] = v; }
static inline void panel_tx_u16(panel_tx_t *tx, const uint16_t v) { uint8_t b[2]; panel_u16_to(v, b); panel_tx_raw(tx, b, 2); }
static inline void panel_tx_i16(panel_tx_t *tx, const int16_t v) { uint8_t b[2]; panel_i16_to(v, b); panel_tx_raw(tx, b, 2); }
static inline void panel_tx_u32(panel_tx_t *tx, const uint32_t v) { uint8_t b[4]; panel_u32_to(v, b); panel_tx_raw(tx, b, 4); }
static inline void panel_tx_i32(panel_tx_t *tx, const int32_t v) { uint8_t b[4]; panel_i32_to(v, b); panel_tx_raw(tx, b, 4); }
static inline void panel_tx_str(panel_tx_t *tx, const char *s, const uint8_t maxlen) {
  uint8_t n = 0;
  if (s) while (s[n] && n < maxlen && n < (PANEL_MAX_PAYLOAD - 1)) n++;
  panel_tx_u8(tx, n);
  if (s) for (uint8_t i = 0; i < n; i++) panel_tx_u8(tx, (uint8_t)s[i]);
}
// Tra ve TONG so byte cua frame (de gui di)
static inline uint8_t panel_tx_finish(panel_tx_t *tx) {
  tx->buf[4] = tx->len;
  tx->buf[5 + tx->len] = panel_crc8(&tx->buf[2], (uint8_t)(3 + tx->len));
  return (uint8_t)(PANEL_FRAME_OVERHEAD + tx->len);
}

// ---------------------------------------------------------------------------
// Bo GIAI frame (may trang thai) -- dung chung cho CA HAI phia
// ---------------------------------------------------------------------------
enum { PANEL_RXS_SYNC0 = 0, PANEL_RXS_SYNC1, PANEL_RXS_TYPE, PANEL_RXS_SEQ, PANEL_RXS_LEN, PANEL_RXS_PAY, PANEL_RXS_CRC };

typedef struct {
  uint8_t state, type, seq, len, idx, crc;
  uint16_t frames, errors;
  uint8_t payload[PANEL_MAX_PAYLOAD];
} panel_rx_t;

static inline void panel_rx_init(panel_rx_t *rx) {
  rx->state = PANEL_RXS_SYNC0;
  rx->frames = rx->errors = 0;
}

// Tra ve true khi vua nhan du 1 frame HOP LE (rx->type/seq/len/payload san sang)
static inline bool panel_rx_byte(panel_rx_t *rx, const uint8_t c) {
  switch (rx->state) {
    case PANEL_RXS_SYNC0: if (c == PANEL_SYNC0) rx->state = PANEL_RXS_SYNC1; return false;
    case PANEL_RXS_SYNC1:
      rx->state = (c == PANEL_SYNC1) ? PANEL_RXS_TYPE : (c == PANEL_SYNC0 ? PANEL_RXS_SYNC1 : PANEL_RXS_SYNC0);
      return false;
    case PANEL_RXS_TYPE: rx->type = c; rx->crc = panel_crc8_update(0, c); rx->state = PANEL_RXS_SEQ; return false;
    case PANEL_RXS_SEQ:  rx->seq = c;  rx->crc = panel_crc8_update(rx->crc, c); rx->state = PANEL_RXS_LEN; return false;
    case PANEL_RXS_LEN:
      rx->len = c; rx->idx = 0;
      rx->crc = panel_crc8_update(rx->crc, c);
      if (rx->len > (PANEL_MAX_PAYLOAD)) { rx->errors++; rx->state = PANEL_RXS_SYNC0; return false; }
      rx->state = rx->len ? PANEL_RXS_PAY : PANEL_RXS_CRC;
      return false;
    case PANEL_RXS_PAY:
      rx->payload[rx->idx++] = c;
      rx->crc = panel_crc8_update(rx->crc, c);
      if (rx->idx >= rx->len) rx->state = PANEL_RXS_CRC;
      return false;
    case PANEL_RXS_CRC:
    default:
      rx->state = PANEL_RXS_SYNC0;
      if (c != rx->crc) { rx->errors++; return false; }
      rx->frames++;
      return true;
  }
}
