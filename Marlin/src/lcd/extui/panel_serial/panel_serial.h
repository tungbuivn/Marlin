/**
 * panel_serial.h -- ExtUI "panel_serial": Marlin day du lieu xuong man hinh rieng.
 *
 * Man hinh (lcd_panel: STM32F411/F103/F407, ESP32-2432S028R) KHONG hoi Marlin bang G-code
 * nua: Marlin chu dong gui frame nhi phan (xem panel_protocol.h) va panel chi gui lenh
 * G-code cho HANH DONG. Nho vay man hinh co du du lieu de ve moi khoi (file SD, mesh,
 * PID/steps/mm, prompt, trang thai...) ma G-code khong the lay duoc.
 *
 * Cach bat: #define PANEL_SERIAL_UI trong Configuration_adv.h
 *           + build_flags: -DUSING_HW_SERIAL3 -DPIN_SERIAL3_RX=PD9 -DPIN_SERIAL3_TX=PD8
 */
#pragma once

#include "../../../inc/MarlinConfig.h"

#if ENABLED(PANEL_SERIAL_UI)

  #include "panel_protocol.h"

  // UART noi voi panel - 2 lua chon (xem lcd_panel/README.md §2.1):
  //   1 = USART1 : PA9 (TX) / PA10 (RX) = header MKS WiFi   <-- DANG DUNG (day la cho da di day san)
  //   3 = USART3 : PD8 (TX) / PD9 (RX) = header EXP1 chan 6/5 (chi dung khi da thao han man 12864)
  // KHONG dung SERIAL_PORT_2 vi Marlin se coi do la cong host thu 2 va dien giai frame nhi phan nhu G-code.
  #ifndef PANEL_SERIAL_PORT
    #define PANEL_SERIAL_PORT 1
  #endif

  #if PANEL_SERIAL_PORT == 1
    #ifndef USING_HW_SERIAL1
      #error "PANEL_SERIAL_UI (port 1) can -DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9 trong build_flags (ini/features.ini)."
    #endif
  #elif PANEL_SERIAL_PORT == 3
    #ifndef USING_HW_SERIAL3
      #error "PANEL_SERIAL_UI (port 3) can -DUSING_HW_SERIAL3 -DPIN_SERIAL3_RX=PD9 -DPIN_SERIAL3_TX=PD8 trong build_flags."
    #endif
  #else
    #error "PANEL_SERIAL_UI chi ho tro PANEL_SERIAL_PORT 1 (USART1 PA9/PA10) hoac 3 (USART3 PD8/PD9)."
  #endif

  #define PANEL_SERIAL  MSERIAL(PANEL_SERIAL_PORT)

  #ifndef PANEL_BAUD
    #define PANEL_BAUD        250000
  #endif
  #ifndef PANEL_PUSH_MS
    #define PANEL_PUSH_MS     250        // chu ky day STATUS/TEMPS/POSITION (ms)
  #endif
  #ifndef PANEL_PROGRESS_MS
    #define PANEL_PROGRESS_MS 1000       // chu ky day PROGRESS khi dang in (ms)
  #endif
  #ifndef PANEL_TIMEOUT_MS
    #define PANEL_TIMEOUT_MS  5000       // khong nhan duoc frame nao trong bao lau thi coi nhu mat panel
  #endif
  #ifndef PANEL_STR_MAX
    #define PANEL_STR_MAX     48         // do dai toi da cua chuoi trong 1 frame
  #endif
  // So frame toi da xep trong hang doi TX (moi slot giu NGUYEN 1 frame, toi da PANEL_MAX_FRAME
  // byte). Hang doi de tx_send() khong phai bo frame khi TX buffer cua core (64..256 byte) chua
  // kip ra: 33 dong setting x 12 byte = 396 byte, o 250000 baud phai mat ~16 ms. Xem panel_serial.cpp.
  #ifndef PANEL_TXQ_SLOTS
    #define PANEL_TXQ_SLOTS   8
  #endif

  #ifndef PANEL_SD_TIMEOUT_MS
    #define PANEL_SD_TIMEOUT_MS  1000   // cho toi da 1 sector (binh thuong ~25 ms) truoc khi bao loi
  #endif

  void panel_serial_init();              // goi tu ExtUI::onStartup()
  void panel_serial_task();              // goi tu ExtUI::onIdle()

  // ---------------------------------------------------------------------------
  // SD QUA UART (PANEL_SD_OVER_UART, proto >= 3) -- xem lcd_panel/README.md muc 12
  //   The TF nam o khe MAN HINH. Marlin khong doc SPI nua: moi sector 512 B duoc hoi panel qua
  //   chinh UART nay (PANEL_F_SDREQ -> 4 frame PANEL_F_SDDATA). Nho vay KHONG can MUX SPI, khong
  //   tranh chap bus, panel nap lai firmware luc nao cung duoc.
  //   Cac ham nay CHAN (toi da `timeout_ms`) va tu bom TX/RX + nuoi watchdog trong luc cho.
  // ---------------------------------------------------------------------------
  #if ENABLED(PANEL_SD_OVER_UART)
  namespace ExtUI {
    bool     panel_serial_sd_init();                    // hoi panel: co the khong? (goi tu driver->init)
    bool     panel_serial_sd_read(uint32_t sector, uint8_t *dst, uint32_t timeout_ms);
    bool     panel_serial_sd_write(uint32_t sector, const uint8_t *src, uint32_t timeout_ms);
    bool     panel_serial_sd_present();                 // panel bao co the (cache tu lan hoi cuoi)
  }
  #endif

#endif // PANEL_SERIAL_UI
