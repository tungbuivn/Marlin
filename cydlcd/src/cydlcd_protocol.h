/**
 * cydlcd_protocol.h -- Giao thuc day du lieu (PUSH) giua Marlin va MCU man hinh.
 *
 *   Marlin (ExtUI "cydlcd_serial", src/lcd/extui/cydlcd_serial)  <--UART-->  cydlcd
 *
 * VI SAO PUSH, KHONG PHAI G-CODE:
 *   Man hinh co MCU rieng KHONG doc duoc bien noi bo cua Marlin. Neu MAN HINH CYD tu hoi bang
 *   G-code (M105/M114/M27) thi chi lay duoc vai con so, con rat nhieu "khoi" khong the
 *   dien: danh sach file SD, luoi bed mesh, thong so PID/steps/mm/jerk, trang thai
 *   endstop/probe, prompt cua nguoi dung, ten file dang in... (MKS TFT lam dung kieu
 *   G-code nay va phai them hang loat M-code rieng — xem makerbase-mks/MKS-TFT*-Firmware.)
 *   => Marlin CHU DONG gui frame nhi phan xuong MAN HINH CYD; MAN HINH CYD chi gui lenh G-code cho
 *      HANH DONG (G28/M104/M140/M24...) chu khong dung G-code de VE.
 *
 * FILE DUNG CHUNG: ban y nguyen van nam o
 *      Marlin/src/lcd/extui/cydlcd_serial/cydlcd_protocol.h   (Marlin)
 *      cydlcd/src/cydlcd_protocol.h                        (MAN HINH CYD)
 *   Sua file nay thi phai copy y nguyen sang file kia (header chi dung <stdint.h>).
 *
 * KHUNG TRUYEN (little-endian):
 *      [0] 0xA5  SYNC0        (CYDLCD_SYNC0)
 *      [1] 0x5A  SYNC1        (CYDLCD_SYNC1)
 *      [2] type               (cydlcd_frame_t)
 *      [3] seq                tang dan moi frame, de doi chieu PING/PONG
 *      [4] len                so byte payload (0..CYDLCD_MAX_PAYLOAD)
 *      [5..] payload
 *      [5+len] crc8           CRC8 (poly 0x07) tren type,seq,len,payload
 *
 * KIEU DU LIEU TRONG PAYLOAD:
 *      u8/u16/u32 : so nguyen khong dau, little-endian
 *      i16/i32    : so nguyen co dau, little-endian
 *      str        : u8 len + len byte (khong co ky tu ket thuc, cat bot neu dai)
 *      blob       : u8 len + len byte (vi du diem mesh)
 *      nhiet do   : i16 = do C * CYDLCD_TEMP_SCALE
 *      vi tri     : i32 = mm * CYDLCD_POS_SCALE
 *      phan tbt  : i16 = % * CYDLCD_PCT_SCALE
 */
#pragma once

#include <stdint.h>

#define CYDLCD_PROTO_VERSION     4     // 2: HELLO them str version; 3: SD qua UART; 4: F_FILES them ten 8.3
                                      // 3: them F_SDREQ/F_SDDATA (Marlin lay sector the SD qua UART)
#define CYDLCD_SYNC0             0xA5
#define CYDLCD_SYNC1             0x5A
// LUU Y: truong `len` trong khung chi co 1 BYTE (buf[4]) => payload toi da 255 byte. Vi vay 1 sector
// 512 byte KHONG the nam trong 1 frame: CYDLCD_F_SDDATA chia thanh CYDLCD_SD_CHUNKS frame, moi frame
// CYDLCD_SD_CHUNK byte (xem khoi "SD qua UART" o duoi). Giu nguyen 192 cho tuong thich voi moi frame
// khac (STATUS/TRAMES/MESH...) va voi tools/ test tren may tinh.
#define CYDLCD_MAX_PAYLOAD       192
#define CYDLCD_FRAME_OVERHEAD    6                     // sync0 sync1 type seq len crc
#define CYDLCD_MAX_FRAME         (CYDLCD_MAX_PAYLOAD + CYDLCD_FRAME_OVERHEAD)

// He so quy doi (float -> so nguyen)
#define CYDLCD_TEMP_SCALE        100.0f                // do C  -> i16
#define CYDLCD_POS_SCALE         1000.0f               // mm    -> i32 (micromet)
#define CYDLCD_PCT_SCALE         100.0f                // %     -> i16
#define CYDLCD_VAL_SCALE         1000.0f               // gia tri chung -> i32

// ---------------------------------------------------------------------------
// Loai frame
// ---------------------------------------------------------------------------
enum cydlcd_frame_t : uint8_t {
  // ---- Marlin -> MAN HINH CYD (push) ----
  CYDLCD_F_HELLO     = 0x01,   // u16 fw_version, u8 proto, u8 flags, u8 n_heaters, u8 n_fans, str name
                              // [proto >= 2] str version (SHORT_BUILD_VERSION cua Marlin)
  CYDLCD_F_STATUS    = 0x02,   // u8 state, u8 flags, u8 progress_percent
  CYDLCD_F_TEMPS     = 0x03,   // u8 n, n * (u8 id, i16 cur, i16 tgt)
  CYDLCD_F_POSITION  = 0x04,   // i32 x, i32 y, i32 z, i32 e, u16 feed_mm_s*10, i16 feed_pct, i16 flow_pct
  CYDLCD_F_PROGRESS  = 0x05,   // u8 pct, u32 elapsed_s, u32 remaining_s, str filename
  CYDLCD_F_MESSAGE   = 0x06,   // u8 severity, str text
  CYDLCD_F_FANS      = 0x07,   // u8 n, n * (u8 idx, i16 pct)
  CYDLCD_F_MESH      = 0x08,   // u8 ny, u8 row, u8 nx, nx * i16 z_um   (gui tung hang mot)
  CYDLCD_F_FILES     = 0x09,   // u8 flags, u32 size, str name  (1 file/frame; flags=0xFF & name rong = het danh sach)
  CYDLCD_F_EVENT     = 0x0A,   // u8 event, u16 v1, u16 v2, str text (v1/v2 tuy event, vd TONE: freq/duration)
  CYDLCD_F_SETTING   = 0x0B,   // u16 id, i32 value
  CYDLCD_F_PROMPT    = 0x0C,   // u8 prompt_id, u8 type, str text
  CYDLCD_F_PONG      = 0x0D,   // u8 echo_seq
  CYDLCD_F_TBT      = 0x0E,   // str: dung 1 dong ASCII "TBT ...\\n" (dinh dang o tbt_protocol.h;
                              //      2026-10-13 - port tu che do TRAM link), gui ~5Hz khi wizard chay
  CYDLCD_F_ENDSTOPS  = 0x0F,   // u8 n, n * (u8 id (cydlcd_endstop_id_t), u8 hit) -- trang thai kieu M119
  CYDLCD_F_SDREQ     = 0x10,   // [proto >= 3] YEU CAU DOC/GHI the SD (Marlin -> cydlcd, vi the SD nam
                              // o khe man hinh): u8 op (cydlcd_sd_op_t), u32 sector.
                              // (op = WRITE: theo sau la CYDLCD_SD_CHUNKS frame nua - xem duoi.)

  // ---- MAN HINH CYD -> Marlin ----
  CYDLCD_F_GCODE     = 0x40,   // str: 1 dong G-code (khong can '\\n')
  CYDLCD_F_REQUEST   = 0x41,   // u8 what (cydlcd_request_t), [str tham so]
  CYDLCD_F_PING      = 0x42,   // u8 seq
  CYDLCD_F_ANSWER    = 0x43,   // u8 prompt_id, u8 answer
  CYDLCD_F_TBT_CMD  = 0x44,   // u8 lenh: 'P' probe, 'D' done, 'M' doi MAX/MIN,
                              //          'R' home+probe lai, 'B' thoat wizard, 'Q' hoi trang thai
  CYDLCD_F_SDDATA    = 0x45,   // [proto >= 3] DU LIEU SECTOR, dung cho CA 2 CHIEU:
                              //   MAN HINH CYD -> Marlin (tra loi lenh DOC): u8 status, u32 sector,
                              //       u8 idx (0..3), u8 n, n byte  (status != OK: chi 1 frame, n = 0)
                              //   Marlin -> MAN HINH CYD (gui du lieu lenh GHI): u8 status (=0), u32 sector,
                              //       u8 idx (0..3), u8 n (=128), n byte
                              //   -> 1 sector 512 B = CYDLCD_SD_CHUNKS frame, moi frame CYDLCD_SD_CHUNK byte
  CYDLCD_F_SDACK     = 0x46    // [proto >= 3] cydlcd -> Marlin: u8 status, u32 sector
                              //   (tra loi 1 lenh GHI sau khi da ghi xong xuong the)
};

// ---------------------------------------------------------------------------
// SD qua UART (proto >= 3): Marlin KHONG tu doc the SPI nua, moi sector duoc hoi MAN HINH CYD.
//   1 sector 512 B = CYDLCD_SD_CHUNKS frame, moi frame CYDLCD_SD_CHUNK byte.
//   Payload 1 frame = 1 + 4 + 1 + 1 + 128 = 135 byte (<= CYDLCD_MAX_PAYLOAD 192).
// ---------------------------------------------------------------------------
#define CYDLCD_SD_SECTOR     512
#define CYDLCD_SD_CHUNK      128
#define CYDLCD_SD_CHUNKS     (CYDLCD_SD_SECTOR / CYDLCD_SD_CHUNK)   // 4
enum cydlcd_sd_op_t : uint8_t {
  CYDLCD_SD_OP_READ  = 0,      // doc sector -> cydlcd tra CYDLCD_SD_CHUNKS frame
  CYDLCD_SD_OP_WRITE = 1       // ghi sector -> Marlin gui kem CYDLCD_SD_CHUNKS frame, cydlcd tra SDACK
};
enum cydlcd_sd_status_t : uint8_t {
  CYDLCD_SD_ST_OK    = 0,      // thanh cong
  CYDLCD_SD_ST_ERR   = 1,      // loi the / sector ngoai pham vi
  CYDLCD_SD_ST_NOCARD= 2       // cydlcd khong co the
};

// ---------------------------------------------------------------------------
// HELLO flags (MAN HINH CYD biet mainboard co gi)
// ---------------------------------------------------------------------------
enum cydlcd_hello_flags_t : uint8_t {
  CYDLCD_HAS_SD        = 0x01,
  CYDLCD_HAS_MESH      = 0x02,
  CYDLCD_HAS_PROBE     = 0x04,
  CYDLCD_HAS_FILAMENT  = 0x08,
  CYDLCD_HAS_BED       = 0x10,
  CYDLCD_HAS_CHAMBER   = 0x20,
  CYDLCD_HAS_POWERLOSS = 0x40,
  CYDLCD_HAS_TMC       = 0x80
};

// ---------------------------------------------------------------------------
// Trang thai may
// ---------------------------------------------------------------------------
enum cydlcd_state_t : uint8_t {
  CYDLCD_ST_IDLE = 0,
  CYDLCD_ST_PRINTING,
  CYDLCD_ST_PAUSED,
  CYDLCD_ST_HOMING,
  CYDLCD_ST_LEVELING,
  CYDLCD_ST_RUNOUT,
  CYDLCD_ST_KILLED,
  CYDLCD_ST_BUSY
};

enum cydlcd_status_flags_t : uint8_t {
  CYDLCD_SF_HOMED       = 0x01,   // da home it nhat 1 lan
  CYDLCD_SF_MEDIA       = 0x02,   // co the SD
  CYDLCD_SF_SD_PRINTING = 0x04,
  CYDLCD_SF_PAUSED      = 0x08,
  CYDLCD_SF_LEVELING    = 0x10,   // dang/co bed leveling
  CYDLCD_SF_MESH_VALID  = 0x20,
  CYDLCD_SF_RUNOUT      = 0x40,
  CYDLCD_SF_STEPPERS    = 0x80    // dong co dang giu (enabled)
};

// ---------------------------------------------------------------------------
// Su kien (CYDLCD_F_EVENT) -- gui ngay khi xay ra
// ---------------------------------------------------------------------------
enum cydlcd_event_t : uint8_t {
  CYDLCD_EV_BOOT = 0,
  CYDLCD_EV_MEDIA_INSERTED,
  CYDLCD_EV_MEDIA_REMOVED,
  CYDLCD_EV_MEDIA_ERROR,
  CYDLCD_EV_PRINT_START,
  CYDLCD_EV_PRINT_PAUSE,
  CYDLCD_EV_PRINT_RESUME,
  CYDLCD_EV_PRINT_STOP,
  CYDLCD_EV_PRINT_DONE,
  CYDLCD_EV_HOMING_START,
  CYDLCD_EV_HOMING_DONE,
  CYDLCD_EV_LEVELING_START,
  CYDLCD_EV_LEVELING_DONE,
  CYDLCD_EV_STEPPERS_ON,
  CYDLCD_EV_STEPPERS_OFF,
  CYDLCD_EV_FILAMENT_RUNOUT,
  CYDLCD_EV_TONE,
  CYDLCD_EV_FACTORY_RESET,
  CYDLCD_EV_SETTINGS_STORED,
  CYDLCD_EV_SETTINGS_LOADED,
  CYDLCD_EV_PID_DONE,
  CYDLCD_EV_POWERLOSS_RESUME
};

// Muc do thong bao
enum cydlcd_severity_t : uint8_t {
  CYDLCD_SEV_INFO = 0,
  CYDLCD_SEV_WARN,
  CYDLCD_SEV_ERROR,
  CYDLCD_SEV_FATAL
};

// ---------------------------------------------------------------------------
// Yeu cau tu MAN HINH CYD (CYDLCD_F_REQUEST)
// ---------------------------------------------------------------------------
enum cydlcd_request_t : uint8_t {
  CYDLCD_REQ_FULL = 0,        // gui lai HELLO + STATUS + TEMPS + POSITION + FANS + SETTINGS
  CYDLCD_REQ_FILES,           // liet ke file trong thu muc SD hien tai
  CYDLCD_REQ_FILES_UP,        // len thu muc cha (payload them str ten thu muc?)
  CYDLCD_REQ_FILES_DIR,       // vao thu muc (payload them str ten thu muc)
  CYDLCD_REQ_MESH,            // gui luoi bed mesh
  CYDLCD_REQ_SETTINGS,        // gui cac thong so dang hien thi
  CYDLCD_REQ_SAVE_SETTINGS,   // M500
  CYDLCD_REQ_ABORT,           // dung in ngay (M524/M410)
  CYDLCD_REQ_ENDSTOPS,        // gui trang thai endstop/probe (CYDLCD_F_ENDSTOPS) -- kieu M119
  CYDLCD_REQ_DFU              // nhay vao ROM DFU bootloader (chi khi may ranh; STM32_DFU_REBOOT)
};

// ---------------------------------------------------------------------------
// ID thong so (CYDLCD_F_SETTING) -- chi de HIEN THI/lay gia tri; muon sua thi
// MAN HINH CYD gui G-code (M92/M203/M204/M206/M301/M304/M851/M290...) chu khong ghi truc tiep.
// ---------------------------------------------------------------------------
enum cydlcd_setting_t : uint16_t {
  CYDLCD_SET_Z_OFFSET = 1,        // mm
  CYDLCD_SET_FEEDRATE_PCT,        // %
  CYDLCD_SET_FLOW_PCT,            // %  (E0)
  CYDLCD_SET_STEPS_MM_X,          // steps/mm
  CYDLCD_SET_STEPS_MM_Y,
  CYDLCD_SET_STEPS_MM_Z,
  CYDLCD_SET_STEPS_MM_E,
  CYDLCD_SET_MAXFEED_X,           // mm/s
  CYDLCD_SET_MAXFEED_Y,
  CYDLCD_SET_MAXFEED_Z,
  CYDLCD_SET_MAXFEED_E,
  CYDLCD_SET_MAXACCEL_X,          // mm/s^2
  CYDLCD_SET_MAXACCEL_Y,
  CYDLCD_SET_MAXACCEL_Z,
  CYDLCD_SET_MAXACCEL_E,
  CYDLCD_SET_JERK_X,
  CYDLCD_SET_JERK_Y,
  CYDLCD_SET_JERK_Z,
  CYDLCD_SET_JERK_E,
  CYDLCD_SET_PID_E_KP,
  CYDLCD_SET_PID_E_KI,
  CYDLCD_SET_PID_E_KD,
  CYDLCD_SET_PID_BED_KP,
  CYDLCD_SET_PID_BED_KI,
  CYDLCD_SET_PID_BED_KD,
  CYDLCD_SET_LINEAR_ADV_E,        // s
  CYDLCD_SET_JUNCTION_DEV,        // mm (khi dung junction deviation thay cho jerk)
  CYDLCD_SET_PROBE_OFFSET_X,      // mm
  CYDLCD_SET_PROBE_OFFSET_Y,
  CYDLCD_SET_PROBE_OFFSET_Z,
  CYDLCD_SET_TARGET_TEMP_E0,      // do C
  CYDLCD_SET_TARGET_TEMP_BED,     // do C
  CYDLCD_SET_FAN_PCT_E0,          // %
  CYDLCD_SET_MAXACCEL_PRINT,      // mm/s^2 (planner.settings.acceleration, M204 S)
  CYDLCD_SET_MAXACCEL_RETRACT,    // mm/s^2 (planner.settings.retract_acceleration, M204 R)
  CYDLCD_SET_MAXACCEL_TRAVEL,     // mm/s^2 (planner.settings.travel_acceleration, M204 T)
  CYDLCD_SET_TMC_CURRENT_X,       // mA (stepperX.val_mA - chi khi HAS_TRINAMIC_CONFIG)
  CYDLCD_SET_TMC_CURRENT_Y,       // mA
  CYDLCD_SET_TMC_CURRENT_Z,       // mA
  CYDLCD_SET_TMC_CURRENT_E,       // mA (E0)
  CYDLCD_SET_RUNOUT_EN            // 1 = bat cam bien het filament (runout.enabled)
  // GHI CHU: 3 preset preheat (PLA/PETG/ABS) KHONG nam o day vi ban nay khong co M145/HAS_PREHEAT.
  // MAN HINH CYD tu luu chung trong NVS voi "id cuc bo" 0x40xx (xem ui_preheat.cpp va ui_page_settings.cpp - bang SET_ROWS).
};

// Prompt (CYDLCD_F_PROMPT)
enum cydlcd_prompt_t : uint8_t {
  CYDLCD_PROMPT_NONE = 0,
  CYDLCD_PROMPT_USER_CONFIRM,     // "echo: ... please confirm" -> answer 1 = OK
  CYDLCD_PROMPT_FILAMENT_RUNOUT,
  CYDLCD_PROMPT_RESUME_PRINT,
  CYDLCD_PROMPT_FILAMENT_CHANGE
};

// ---------------------------------------------------------------------------
// ID trong frame TEMPS (CYDLCD_F_TEMPS): 0 = bed, 1..6 = E0..E5
// ---------------------------------------------------------------------------
enum cydlcd_temp_id_t : uint8_t {
  CYDLCD_TEMP_BED     = 0,
  CYDLCD_TEMP_E0      = 1,
  CYDLCD_TEMP_CHAMBER = 0xFE,
  CYDLCD_TEMP_COOLER  = 0xFF
};

// ---------------------------------------------------------------------------
// ID trong frame ENDSTOPS (CYDLCD_F_ENDSTOPS): 1 byte id + 1 byte hit (0/1)
// May dung Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN thi Z_MIN chinh la chan probe
// => bao id CYDLCD_ES_PROBE (0), KHONG bao them CYDLCD_ES_Z_MIN (3).
// ---------------------------------------------------------------------------
#define CYDLCD_ENDSTOP_MAX 6
enum cydlcd_endstop_id_t : uint8_t {
  CYDLCD_ES_PROBE = 0,
  CYDLCD_ES_X_MIN = 1,
  CYDLCD_ES_Y_MIN = 2,
  CYDLCD_ES_Z_MIN = 3,
  CYDLCD_ES_X_MAX = 4,
  CYDLCD_ES_Y_MAX = 5
};

// ---------------------------------------------------------------------------
// Tien ich dung chung (header-only)
// ---------------------------------------------------------------------------
static inline uint8_t cydlcd_crc8_update(uint8_t crc, const uint8_t c) {
  crc ^= c;
  for (uint8_t i = 0; i < 8; i++)
    crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  return crc;
}

static inline uint8_t cydlcd_crc8(const uint8_t *p, uint8_t n) {
  uint8_t crc = 0;
  while (n--) {
    crc ^= *p++;
    for (uint8_t i = 0; i < 8; i++)
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x07) : (uint8_t)(crc << 1);
  }
  return crc;
}

static inline uint16_t cydlcd_u16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8)); }
static inline uint32_t cydlcd_u32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline int16_t  cydlcd_i16(const uint8_t *p) { return (int16_t)cydlcd_u16(p); }
static inline int32_t  cydlcd_i32(const uint8_t *p) { return (int32_t)cydlcd_u32(p); }

static inline uint8_t cydlcd_i16_to(const int16_t v, uint8_t *p) {
  p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((uint16_t)v >> 8);
  return 2;
}
static inline uint8_t cydlcd_i32_to(const int32_t v, uint8_t *p) {
  p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((uint32_t)v >> 8);
  p[2] = (uint8_t)((uint32_t)v >> 16); p[3] = (uint8_t)((uint32_t)v >> 24);
  return 4;
}
static inline uint8_t cydlcd_u16_to(const uint16_t v, uint8_t *p) {
  p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  return 2;
}
static inline uint8_t cydlcd_u32_to(const uint32_t v, uint8_t *p) {
  p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8);
  p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
  return 4;
}

// ---------------------------------------------------------------------------
// Bo TAO frame -- dung chung cho CA HAI phia (Marlin va MAN HINH CYD)
// ---------------------------------------------------------------------------
typedef struct {
  uint8_t buf[CYDLCD_MAX_FRAME];
  uint8_t len;      // so byte payload da ghi (buf[4] duoc cap nhat khi finish)
  uint8_t seq;      // so thu tu frame, tu tang
} cydlcd_tx_t;

static inline void cydlcd_tx_begin(cydlcd_tx_t *tx, const uint8_t type) {
  tx->len = 0;
  tx->buf[0] = CYDLCD_SYNC0;
  tx->buf[1] = CYDLCD_SYNC1;
  tx->buf[2] = type;
  tx->buf[3] = tx->seq++;
  tx->buf[4] = 0;
}
static inline void cydlcd_tx_raw(cydlcd_tx_t *tx, const uint8_t *b, uint8_t n) {
  while (n--) if (tx->len < (CYDLCD_MAX_PAYLOAD)) tx->buf[5 + tx->len++] = *b++;
}
static inline void cydlcd_tx_u8(cydlcd_tx_t *tx, const uint8_t v) { if (tx->len < (CYDLCD_MAX_PAYLOAD)) tx->buf[5 + tx->len++] = v; }
static inline void cydlcd_tx_u16(cydlcd_tx_t *tx, const uint16_t v) { uint8_t b[2]; cydlcd_u16_to(v, b); cydlcd_tx_raw(tx, b, 2); }
static inline void cydlcd_tx_i16(cydlcd_tx_t *tx, const int16_t v) { uint8_t b[2]; cydlcd_i16_to(v, b); cydlcd_tx_raw(tx, b, 2); }
static inline void cydlcd_tx_u32(cydlcd_tx_t *tx, const uint32_t v) { uint8_t b[4]; cydlcd_u32_to(v, b); cydlcd_tx_raw(tx, b, 4); }
static inline void cydlcd_tx_i32(cydlcd_tx_t *tx, const int32_t v) { uint8_t b[4]; cydlcd_i32_to(v, b); cydlcd_tx_raw(tx, b, 4); }
static inline void cydlcd_tx_str(cydlcd_tx_t *tx, const char *s, const uint8_t maxlen) {
  uint8_t n = 0;
  if (s) while (s[n] && n < maxlen && n < (CYDLCD_MAX_PAYLOAD - 1)) n++;
  cydlcd_tx_u8(tx, n);
  if (s) for (uint8_t i = 0; i < n; i++) cydlcd_tx_u8(tx, (uint8_t)s[i]);
}
// Tra ve TONG so byte cua frame (de gui di)
static inline uint8_t cydlcd_tx_finish(cydlcd_tx_t *tx) {
  tx->buf[4] = tx->len;
  tx->buf[5 + tx->len] = cydlcd_crc8(&tx->buf[2], (uint8_t)(3 + tx->len));
  return (uint8_t)(CYDLCD_FRAME_OVERHEAD + tx->len);
}

// ---------------------------------------------------------------------------
// Bo GIAI frame (may trang thai) -- dung chung cho CA HAI phia
// ---------------------------------------------------------------------------
enum { CYDLCD_RXS_SYNC0 = 0, CYDLCD_RXS_SYNC1, CYDLCD_RXS_TYPE, CYDLCD_RXS_SEQ, CYDLCD_RXS_LEN, CYDLCD_RXS_PAY, CYDLCD_RXS_CRC };

typedef struct {
  uint8_t state, type, seq, len, idx, crc;
  uint16_t frames, errors;
  uint8_t payload[CYDLCD_MAX_PAYLOAD];
} cydlcd_rx_t;

static inline void cydlcd_rx_init(cydlcd_rx_t *rx) {
  rx->state = CYDLCD_RXS_SYNC0;
  rx->frames = rx->errors = 0;
}

// Tra ve true khi vua nhan du 1 frame HOP LE (rx->type/seq/len/payload san sang)
static inline bool cydlcd_rx_byte(cydlcd_rx_t *rx, const uint8_t c) {
  switch (rx->state) {
    case CYDLCD_RXS_SYNC0: if (c == CYDLCD_SYNC0) rx->state = CYDLCD_RXS_SYNC1; return false;
    case CYDLCD_RXS_SYNC1:
      rx->state = (c == CYDLCD_SYNC1) ? CYDLCD_RXS_TYPE : (c == CYDLCD_SYNC0 ? CYDLCD_RXS_SYNC1 : CYDLCD_RXS_SYNC0);
      return false;
    case CYDLCD_RXS_TYPE: rx->type = c; rx->crc = cydlcd_crc8_update(0, c); rx->state = CYDLCD_RXS_SEQ; return false;
    case CYDLCD_RXS_SEQ:  rx->seq = c;  rx->crc = cydlcd_crc8_update(rx->crc, c); rx->state = CYDLCD_RXS_LEN; return false;
    case CYDLCD_RXS_LEN:
      rx->len = c; rx->idx = 0;
      rx->crc = cydlcd_crc8_update(rx->crc, c);
      if (rx->len > (CYDLCD_MAX_PAYLOAD)) { rx->errors++; rx->state = CYDLCD_RXS_SYNC0; return false; }
      rx->state = rx->len ? CYDLCD_RXS_PAY : CYDLCD_RXS_CRC;
      return false;
    case CYDLCD_RXS_PAY:
      rx->payload[rx->idx++] = c;
      rx->crc = cydlcd_crc8_update(rx->crc, c);
      if (rx->idx >= rx->len) rx->state = CYDLCD_RXS_CRC;
      return false;
    case CYDLCD_RXS_CRC:
    default:
      rx->state = CYDLCD_RXS_SYNC0;
      if (c != rx->crc) { rx->errors++; return false; }
      rx->frames++;
      return true;
  }
}
