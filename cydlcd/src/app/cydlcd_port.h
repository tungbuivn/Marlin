/**
 * cydlcd_port.h -- lop mong ngan cach phan cung (UART + millis) de cydlcd_link.cpp
 *                 co the chay CA tren board LAN tren may tinh (unit test).
 *
 *  - Tren board: dung CYDLCD_MARLIN_SERIAL trong cydlcd_config.h (Serial / Serial2).
 *  - Tren may tinh (CYDLCD_HOST_TEST): dung stub trong tools/test_cydlcd_link.cpp.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef CYDLCD_HOST_TEST

  // Stub do chuong trinh test cung cap
  struct CydlcdSerialStub {
    void begin(uint32_t) {}
    int  available();
    int  read();
    size_t write(const uint8_t *buf, size_t len);
    size_t write(uint8_t c);
    int  availableForWrite();
  };
  extern CydlcdSerialStub CYDLCD_MARLIN_SERIAL;
  uint32_t cydlcd_millis();
  #define CYDLCD_LINK_BEGIN(baud) CYDLCD_MARLIN_SERIAL.begin(baud)

#else

  #include <Arduino.h>
  #include "cydlcd_config.h"
  #define cydlcd_millis() millis()

#endif

#ifndef CYDLCD_LINK_TIMEOUT_MS
  #define CYDLCD_LINK_TIMEOUT_MS 3000    // khong nhan duoc frame nao trong bao lau -> coi nhu mat lien ket
#endif
#ifndef CYDLCD_LINK_PING_MS
  #define CYDLCD_LINK_PING_MS    1000    // chu ky gui PING
#endif
