/**
 * cydlcd_tram_link.h -- Link TRAMMING voi man hinh rieng cydlcd (ASCII tren USART3).
 *
 * VI SAO CAN: man hinh CYD/F407 (cydlcd) muon hien + dieu khien duoc Bed Tramming Wizard
 * ma KHONG bat CYDLCD_SERIAL_UI (ExtUI). Bat ExtUI se tat man 12864 va MAT toan bo menu
 * MarlinUI (Tramming Wizard, Z ALIGN MANUAL...). Link nay chi chiem 1 UART va vai KB flash,
 * KHONG dung ExtUI -> menu 12864 giu nguyen.
 *   Xem docs/issues/0001-cydlcd-serial-ui-tat-custom-menu.md.
 *
 * CACH BAT (mac dinh TAT, 1 dong):
 *     them  "#define CYDLCD_TRAM_LINK"  vao Configuration.h  (hoac #define trong man hinh
 *     tram_protocol.h nay), hoac them -DCYDLCD_TRAM_LINK vao build_flags cua env mks_monster8.
 * Khi TAT: module nay khong duoc bien dich, khong chiem UART nao, khong ton flash.
 * (Build flag USART1 nam trong ini/features.ini, chi duoc them khi CYDLCD_TRAM_LINK bat.)
 *
 * UART: USART1 = MSerial1, PA9 (TX) / PA10 (RX) tren MKS Monster8 V2 - chan header MKS WiFi
 *   (`//#define WIFI_SERIAL 1 // USART1` dang comment trong pins_MKS_MONSTER8_V2.h, va
 *   KHONG co PA9/PA10 nao khac duoc dung trong pins file).
 *   VI SAO KHONG DUNG USART3 (PD8/PD9): PD8 = EXP1_06 = NEOPIXEL_PIN (den RGB/man hinh 12864)
 *   va PD9 = EXP1_05 = LCD_RESET_PIN cua MKS_MINI_12864_V3 -> se phai tat NEOPIXEL_LED va
 *   dung vao chan reset cua man 12864. USART3 con lai cung het chan TX roi (PB10 = HEATER_BED,
 *   PC10 = SD_SCK). USART1/PA9-PA10 thi hoan toan ranh.
 *   => KHONG dung den PD8/NeoPixel, KHONG phai sua Configuration.h/Configuration_adv.h.
 *
 *   Build flag can thiet (ini/features.ini, key CYDLCD_TRAM_LINK - chi them khi bat):
 *     -DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9
 *
 * NOI DAY (cheo TX<->RX, chung GND):
 *     man hinh CYD GPIO22 (TX) -> PA10 (USART1_RX)      [header MKS WiFi]
 *     man hinh CYD GPIO35 (RX) <- PA9  (USART1_TX)      [header MKS WiFi]
 *     GND man hinh CYD         --  GND mainboard
 *   Panel gui bang Serial2 (xem cydlcd/src/cydlcd_config.h: CYDLCD_UART_TX/RX = 22/35).
 *
 * GIAO THUC: xem tram_protocol.h (ban y nguyen o ca Marlin va cydlcd).
 */
#pragma once

#include "../inc/MarlinConfig.h"

// Mac dinh TAT (xem "CACH BAT" o tren). Bo comment dong duoi day cung duoc, nhung cach
// gon nhat la them 1 dong #define CYDLCD_TRAM_LINK vao Configuration.h.
#ifndef CYDLCD_TRAM_LINK
  //#define CYDLCD_TRAM_LINK
#endif

#if ENABLED(CYDLCD_TRAM_LINK) && DISABLED(ASSISTED_TRAMMING_WIZARD)
  #error "CYDLCD_TRAM_LINK dieu khien Bed Tramming Wizard -> can #define ASSISTED_TRAMMING_WIZARD (Configuration_adv.h)."
#endif
#if BOTH(CYDLCD_TRAM_LINK, CYDLCD_SERIAL_UI)
  #error "CYDLCD_TRAM_LINK va CYDLCD_SERIAL_UI deu la 'man hinh rieng' -> chi bat MOT trong hai."
#endif
#if BOTH(CYDLCD_TRAM_LINK, HAS_TFT_LVGL_UI)
  #error "CYDLCD_TRAM_LINK dung MSerial1/USART1 -> xung dot voi TFT LVGL UI (MKS)."
#endif
#if BOTH(CYDLCD_TRAM_LINK, WIFI_SERIAL)
  #error "CYDLCD_TRAM_LINK dung USART1 (PA9/PA10) giong WIFI_SERIAL -> phai comment WIFI_SERIAL (pins_MKS_MONSTER8_V2.h)."
#endif
#if ENABLED(CYDLCD_TRAM_LINK) && !defined(USING_HW_SERIAL1)
  #error "CYDLCD_TRAM_LINK can -DUSING_HW_SERIAL1 -DPIN_SERIAL1_RX=PA10 -DPIN_SERIAL1_TX=PA9 (ini/features.ini, key CYDLCD_TRAM_LINK)."
#endif

#if BOTH(CYDLCD_TRAM_LINK, ASSISTED_TRAMMING_WIZARD)

  #ifndef CYDLCD_TRAM_BAUD
    #define CYDLCD_TRAM_BAUD BAUDRATE       // cydlcd (cydlcd) cung dung 250000
  #endif
  #ifndef CYDLCD_TRAM_PUSH_MS
    #define CYDLCD_TRAM_PUSH_MS 200         // chu ky gui trang thai khi khong doi (~5Hz)
  #endif
  #ifndef CYDLCD_TRAM_LINE_MAX
    #define CYDLCD_TRAM_LINE_MAX 128        // du cho 1 dong "TRAM ..." (thuc te ~85 byte)
  #endif

  namespace cydlcd_tram_link {
    void init();                           // mo USART3 (goi tu tick() lan dau, sau setup())
    void tick();                           // goi tu manage_inactivity() (xem MarlinCore.cpp)
  }

#else

  // Khi TAT: van phai co khai bao de TERN_(CYDLCD_TRAM_LINK, cydlcd_tram_link::tick()) bien dich
  // duoc o MOI cau hinh (MarlinCore.cpp). Than ham rong -> trinh bien dich toi uu sach.
  namespace cydlcd_tram_link {
    inline void init() {}
    inline void tick() {}
  }

#endif
