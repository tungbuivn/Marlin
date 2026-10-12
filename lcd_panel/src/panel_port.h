/**
 * panel_port.h -- lop mong ngan cach phan cung (UART + millis) de panel_link.cpp
 *                 co the chay CA tren board LAN tren may tinh (unit test).
 *
 *  - Tren board: dung PANEL_MARLIN_SERIAL trong panel_config.h (Serial / Serial2).
 *  - Tren may tinh (PANEL_HOST_TEST): dung stub trong tools/test_panel_link.cpp.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef PANEL_HOST_TEST

  // Stub do chuong trinh test cung cap
  struct PanelSerialStub {
    void begin(uint32_t) {}
    int  available();
    int  read();
    size_t write(const uint8_t *buf, size_t len);
    size_t write(uint8_t c);
    int  availableForWrite();
  };
  extern PanelSerialStub PANEL_MARLIN_SERIAL;
  uint32_t panel_millis();
  #define PANEL_LINK_BEGIN(baud) PANEL_MARLIN_SERIAL.begin(baud)

#else

  #include <Arduino.h>
  #include "panel_config.h"
  #define panel_millis() millis()

#endif

#ifndef PANEL_LINK_TIMEOUT_MS
  #define PANEL_LINK_TIMEOUT_MS 3000    // khong nhan duoc frame nao trong bao lau -> coi nhu mat lien ket
#endif
#ifndef PANEL_LINK_PING_MS
  #define PANEL_LINK_PING_MS    1000    // chu ky gui PING
#endif
