/**
 * tbt_link.h -- Phia MAN HINH CYD: link TRAMMING (ASCII) voi Marlin - xem tbt_protocol.h.
 *
 * DUNG CHUNG UART voi cydlcd_link (CYDLCD_MARLIN_SERIAL: Serial2 tren CYD, `Serial` tren STM32):
 *   - cydlcd_link.cpp GIU quyen doc UART (nhan frame PUSH) va goi tbt_link_feed() cho TUNG byte;
 *     byte nao khong thuoc dong "TBT ..." thi duoc tra ve nguyen ven cho bo giai frame PUSH
 *     (khong bao gio mat byte - xem chu thich trong tbt_link.cpp).
 *   - tbt_link_send() ghi lenh 1 ky tu xuong cung UART do.
 *
 * Khi Marlin bat CYDLCD_TBT_LINK thi tren day KHONG co frame PUSH nao (2 che do loai tru nhau,
 * xem Marlin/src/feature/cydlcd_tbt_link.h), nen khong so nham lan. cydlcd_link_task() cung TU TAT
 * PING khi da thay dong TBT dau tien (tbt_link_active()) - de byte nhi phan cua PING khong lot
 * vao bo giai lenh ASCII cua Marlin.
 *
 * Vi sao khong dung frame nhi phan nhu cydlcd_protocol.h: link nay chi can vai con so + vai lenh,
 * ma ASCII thi doc duoc bang Serial Monitor (go loi khong can thiet bi) va KHONG doi hoi ExtUI.
 * Nho vay man 12864 + MarlinUI (Tramming Wizard, Z ALIGN MANUAL...) van con nguyen.
 */
#pragma once

#include <stdint.h>
#include "tbt_protocol.h"

#define TBT_LINK_TIMEOUT_MS       2000    // wizard IDLE: Marlin day 5 Hz -> 2 s khong thay dong = mat link
#define TBT_LINK_BUSY_TIMEOUT_MS 20000    // wizard dang HOMING/PROBING/GOTO/AUTO: Marlin BLOCK vai giay
                                           // (probe/di chuyen khong goi idle()) -> dung timeout DAI hon,
                                           // neu khong se bao "NO LINK" SAI trong luc may dang lam viec
#define TBT_LINK_QUERY_MS         1000    // im lang >= 1 s -> chu dong hoi lai bang 'Q' (Marlin tra loi
                                           // ngay o tick ke tiep; chieu MAN HINH CYD -> Marlin khong phu thuoc timeout)
#define TBT_LINK_RX_ACTIVE_MS     3000    // con byte toi trong 3 s => day noi VAN SONG (du khong doc duoc dong)
#define TBT_LINK_LINE_MAX    128     // du cho 1 dong "TBT ..." (~85 byte)

typedef struct {
  tbt_status_t st;                   // trang thai wizard Marlin gui xuong
  uint32_t last_rx_ms;                // lan cuoi nhan duoc 1 dong TBT
  uint32_t last_byte_ms;              // lan cuoi nhan duoc 1 BYTE bat ky (ke ca rac)
  uint32_t lines, errors;             // so dong nhan duoc / so dong loi
  uint32_t raw_bytes;                 // TONG so byte UART nhan duoc (chan doan day noi)
  bool     ever;                      // da tung nhan duoc dong TBT nao chua
  bool     linked;                    // con nhan duoc dong TBT (cap nhat trong tbt_link_task)
  bool     rx_active;                 // con BYTE toi trong TBT_LINK_RX_ACTIVE_MS (day noi song)
} tbt_link_data_t;

extern tbt_link_data_t tbt;
// CONG BO trang thai TRAMMING cho UI: CHI goi tu state_publish() (khi da giu khoa trang thai).
void tbt_publish_locked(void);

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void tbt_link_begin(void);           // reset trang thai (cydlcd_link_begin goi)
void tbt_link_task(void);            // cap nhat co `linked` (goi lien tuc trong loop())

// cydlcd_link.cpp goi ham nay cho TUNG byte nhan duoc tu UART.
//   - Neu byte thuoc dong "TBT ...": xu ly ngay, tra ve 0 (khong ai khac duoc dung byte do).
//   - Neu khong: tra ve so byte CAN DUA cho bo giai frame PUSH, ghi vao out[] (0..4 byte).
//     (Cac byte 'T','B','T' dang "giu de do" se duoc tra lai day du khi day khong phai "TBT"
//      -> KHONG BAO GIO lam mat byte cua frame PUSH.)
uint8_t tbt_link_feed(uint8_t c, uint8_t *out);

bool tbt_link_active(void);          // da vao che do TRAM (tung nhan duoc dong TBT)
void tbt_link_send(char cmd);        // gui 1 lenh sang Marlin (tu dong them '\n')

// Che do PUSH (CYDLCD_SERIAL_UI, 2026-10-13): dong "TBT ..." duoc Marlin gui trong frame
// CYDLCD_F_TBT (cydlcd_link.cpp doc roi goi ham nay). Khong dung bo "sniff" byte.
void tbt_link_line(const char *line);
