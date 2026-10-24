/**
 * cydlcd_serial.h -- ExtUI "cydlcd_serial": Marlin day du lieu xuong man hinh rieng.
 *
 * Man hinh (cydlcd: STM32F411/F103/F407, ESP32-2432S028R) KHONG hoi Marlin bang G-code
 * nua: Marlin chu dong gui frame nhi phan (xem cydlcd_protocol.h) va man hinh CYD chi gui lenh
 * G-code cho HANH DONG. Nho vay man hinh co du du lieu de ve moi khoi (file SD, mesh,
 * PID/steps/mm, prompt, trang thai...) ma G-code khong the lay duoc.
 *
 * Cach bat: #define CYDLCD_SERIAL_UI trong Configuration_adv.h
 *           + build_flags: -DUSING_HW_SERIAL3 -DPIN_SERIAL3_RX=PD9 -DPIN_SERIAL3_TX=PD8
 */
#pragma once

#include "../../../inc/MarlinConfig.h"

#if ENABLED(CYDLCD_SERIAL_UI)

  #include "cydlcd_protocol.h"

  // UART noi voi man hinh CYD - 2 lua chon (xem cydlcd/README.md §2.1):
  //   1 = USART1 : PA9 (TX) / PA10 (RX) = header MKS WiFi   <-- DANG DUNG (day la cho da di day san)
  //   3 = USART3 : PD8 (TX) / PD9 (RX) = header EXP1 chan 6/5 (chi dung khi da thao han man 12864)
  // KHONG dung SERIAL_PORT_2 vi Marlin se coi do la cong host thu 2 va dien giai frame nhi phan nhu G-code.
  #ifndef CYDLCD_SERIAL_PORT
    #define CYDLCD_SERIAL_PORT 1
  #endif

  #if CYDLCD_SERIAL_PORT == 1
    #ifndef USING_HW_SERIAL1
      #error "CYDLCD_SERIAL_UI (port 1) can -DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9 trong build_flags (ini/features.ini)."
    #endif
  #elif CYDLCD_SERIAL_PORT == 3
    #ifndef USING_HW_SERIAL3
      #error "CYDLCD_SERIAL_UI (port 3) can -DUSING_HW_SERIAL3 -DPIN_SERIAL3_RX=PD9 -DPIN_SERIAL3_TX=PD8 trong build_flags."
    #endif
  #else
    #error "CYDLCD_SERIAL_UI chi ho tro CYDLCD_SERIAL_PORT 1 (USART1 PA9/PA10) hoac 3 (USART3 PD8/PD9)."
  #endif

  #define CYDLCD_SERIAL  MSERIAL(CYDLCD_SERIAL_PORT)

  #ifndef CYDLCD_BAUD
    #define CYDLCD_BAUD        250000
  #endif
  #ifndef CYDLCD_PUSH_MS
    #define CYDLCD_PUSH_MS     250        // chu ky day STATUS/TEMPS/POSITION (ms)
  #endif
  #ifndef CYDLCD_PROGRESS_MS
    #define CYDLCD_PROGRESS_MS 1000       // chu ky day PROGRESS khi dang in (ms)
  #endif
  #ifndef CYDLCD_TIMEOUT_MS
    #define CYDLCD_TIMEOUT_MS  5000       // khong nhan duoc frame nao trong bao lau thi coi nhu mat cydlcd
  #endif
  #ifndef CYDLCD_STR_MAX
    #define CYDLCD_STR_MAX     48         // do dai toi da cua chuoi trong 1 frame
  #endif
  // So frame toi da xep trong hang doi TX (moi slot giu NGUYEN 1 frame, toi da CYDLCD_MAX_FRAME
  // byte). Hang doi de tx_send() khong phai bo frame khi TX buffer cua core (64..256 byte) chua
  // kip ra: 33 dong setting x 12 byte = 396 byte, o 250000 baud phai mat ~16 ms. Xem cydlcd_serial.cpp.
  #ifndef CYDLCD_TXQ_SLOTS
    #define CYDLCD_TXQ_SLOTS   8
  #endif

  #ifndef CYDLCD_SD_TIMEOUT_MS
    #define CYDLCD_SD_TIMEOUT_MS  1000   // cho toi da 1 sector (binh thuong ~25 ms) truoc khi bao loi
  #endif
  // 2026-10-23: so LAN THU LAI doc 1 sector khi loi/timeout truoc khi tra false cho Marlin.
  // VI SAO: 1 frame bi nhieu (CRC sai) hoac man hinh dang ban ghi the chi lam MAT mot lan doc;
  // truoc day -> Marlin bao "SD read error" giua ban in va mat ca phien. Thu lai vai lan re hon.
  // KHONG thu lai khi man hinh bao NOCARD (rut the that su) - xem cydlcd_serial_sd_read().
  #ifndef CYDLCD_SD_TRIES
    #define CYDLCD_SD_TRIES   3
  #endif
  // 2026-10-24 (yeu cau nguoi dung): READ-AHEAD / "ram dem sector".
  // Marlin van goi readBlock() cho TUNG sector, nhung driver giu mot RAM dem (FIFO) va tu xin
  // TRUOC sector ke tiep qua UART trong luc Marlin dang ban plan/motion -> phan lon readBlock()
  // chi con la memcpy (~0 ms) thay vi cho ~22 ms/sector o 250000 baud.
  //   - khong doi giao thuc, khong doi baud, khong can sua firmware man hinh CYD;
  //   - chi nap truoc khi DANG doc tuan tu (sau moi lan Marlin lay 1 sector), cache day thi thoi;
  //   - CYDLCD_SD_CACHE_N = 8 -> 8 x 512 B = 4 KB RAM.
  #ifndef CYDLCD_SD_CACHE_N
    #define CYDLCD_SD_CACHE_N      8    // so sector giu trong RAM dem
  #endif
  #ifndef CYDLCD_SD_PF_TIMEOUT_MS
    #define CYDLCD_SD_PF_TIMEOUT_MS 400 // cho toi da 1 prefetch (binh thuong ~25 ms)
  #endif

  void cydlcd_serial_init();              // goi tu ExtUI::onStartup()
  void cydlcd_serial_task();              // goi tu ExtUI::onIdle()

  // ---------------------------------------------------------------------------
  // SD QUA UART (CYDLCD_SD_OVER_UART, proto >= 3) -- xem cydlcd/README.md muc 12
  //   The TF nam o khe MAN HINH. Marlin khong doc SPI nua: moi sector 512 B duoc hoi man hinh CYD qua
  //   chinh UART nay (CYDLCD_F_SDREQ -> 4 frame CYDLCD_F_SDDATA). Nho vay KHONG can MUX SPI, khong
  //   tranh chap bus, man hinh CYD nap lai firmware luc nao cung duoc.
  //   Cac ham nay CHAN (toi da `timeout_ms`) va tu bom TX/RX + nuoi watchdog trong luc cho.
  // ---------------------------------------------------------------------------
  #if ENABLED(CYDLCD_SD_OVER_UART)
  namespace ExtUI {
    bool     cydlcd_serial_sd_init();                    // hoi cydlcd: co the khong? (goi tu driver->init)
    bool     cydlcd_serial_sd_read(uint32_t sector, uint8_t *dst, uint32_t timeout_ms);
    bool     cydlcd_serial_sd_write(uint32_t sector, const uint8_t *src, uint32_t timeout_ms);
    bool     cydlcd_serial_sd_present();                 // cydlcd bao co the (cache tu lan hoi cuoi)
  }
  #endif

#endif // CYDLCD_SERIAL_UI
