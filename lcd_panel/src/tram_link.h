/**
 * tram_link.h -- Phia PANEL: link TRAMMING (ASCII) voi Marlin - xem tram_protocol.h.
 *
 * DUNG CHUNG UART voi panel_link (PANEL_MARLIN_SERIAL: Serial2 tren CYD, `Serial` tren STM32):
 *   - panel_link.cpp GIU quyen doc UART (nhan frame PUSH) va goi tram_link_feed() cho TUNG byte;
 *     byte nao khong thuoc dong "TRAM ..." thi duoc tra ve nguyen ven cho bo giai frame PUSH
 *     (khong bao gio mat byte - xem chu thich trong tram_link.cpp).
 *   - tram_link_send() ghi lenh 1 ky tu xuong cung UART do.
 *
 * Khi Marlin bat PANEL_TRAM_LINK thi tren day KHONG co frame PUSH nao (2 che do loai tru nhau,
 * xem Marlin/src/feature/panel_tram_link.h), nen khong so nham lan. panel_link_task() cung TU TAT
 * PING khi da thay dong TRAM dau tien (tram_link_active()) - de byte nhi phan cua PING khong lot
 * vao bo giai lenh ASCII cua Marlin.
 *
 * Vi sao khong dung frame nhi phan nhu panel_protocol.h: link nay chi can vai con so + vai lenh,
 * ma ASCII thi doc duoc bang Serial Monitor (go loi khong can thiet bi) va KHONG doi hoi ExtUI.
 * Nho vay man 12864 + MarlinUI (Tramming Wizard, Z ALIGN MANUAL...) van con nguyen.
 */
#pragma once

#include <stdint.h>
#include "tram_protocol.h"

#define TRAM_LINK_TIMEOUT_MS       2000    // wizard IDLE: Marlin day 5 Hz -> 2 s khong thay dong = mat link
#define TRAM_LINK_BUSY_TIMEOUT_MS 20000    // wizard dang HOMING/PROBING/GOTO/AUTO: Marlin BLOCK vai giay
                                           // (probe/di chuyen khong goi idle()) -> dung timeout DAI hon,
                                           // neu khong se bao "NO LINK" SAI trong luc may dang lam viec
#define TRAM_LINK_QUERY_MS         1000    // im lang >= 1 s -> chu dong hoi lai bang 'Q' (Marlin tra loi
                                           // ngay o tick ke tiep; chieu panel -> Marlin khong phu thuoc timeout)
#define TRAM_LINK_RX_ACTIVE_MS     3000    // con byte toi trong 3 s => day noi VAN SONG (du khong doc duoc dong)
#define TRAM_LINK_LINE_MAX    128     // du cho 1 dong "TRAM ..." (~85 byte)

typedef struct {
  tram_status_t st;                   // trang thai wizard Marlin gui xuong
  uint32_t last_rx_ms;                // lan cuoi nhan duoc 1 dong TRAM
  uint32_t last_byte_ms;              // lan cuoi nhan duoc 1 BYTE bat ky (ke ca rac)
  uint32_t lines, errors;             // so dong nhan duoc / so dong loi
  uint32_t raw_bytes;                 // TONG so byte UART nhan duoc (chan doan day noi)
  bool     ever;                      // da tung nhan duoc dong TRAM nao chua
  bool     linked;                    // con nhan duoc dong TRAM (cap nhat trong tram_link_task)
  bool     rx_active;                 // con BYTE toi trong TRAM_LINK_RX_ACTIVE_MS (day noi song)
} tram_link_data_t;

extern tram_link_data_t tram;

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void tram_link_begin(void);           // reset trang thai (panel_link_begin goi)
void tram_link_task(void);            // cap nhat co `linked` (goi lien tuc trong loop())

// panel_link.cpp goi ham nay cho TUNG byte nhan duoc tu UART.
//   - Neu byte thuoc dong "TRAM ...": xu ly ngay, tra ve 0 (khong ai khac duoc dung byte do).
//   - Neu khong: tra ve so byte CAN DUA cho bo giai frame PUSH, ghi vao out[] (0..4 byte).
//     (Cac byte 'T','R','A' dang "giu de do" se duoc tra lai day du khi day khong phai "TRAM"
//      -> KHONG BAO GIO lam mat byte cua frame PUSH.)
uint8_t tram_link_feed(uint8_t c, uint8_t *out);

bool tram_link_active(void);          // da vao che do TRAM (tung nhan duoc dong TRAM)
void tram_link_send(char cmd);        // gui 1 lenh sang Marlin (tu dong them '\n')

// Che do PUSH (PANEL_SERIAL_UI, 2026-10-13): dong "TRAM ..." duoc Marlin gui trong frame
// PANEL_F_TRAM (panel_link.cpp doc roi goi ham nay). Khong dung bo "sniff" byte.
void tram_link_line(const char *line);
