/**
 * cydlcd -- link to Marlin: plain G-code over one UART.
 *
 * Protocol (nothing custom, so ANY Marlin works, and any MKS/BTT "TFT" style MAN HINH CYD works
 * the same way -- see cydlcd/README.md):
 *
 *   MAN HINH CYD -> Marlin                  Marlin -> MAN HINH CYD
 *   ---------------------------------------------------------------
 *   M105                             ok T:210.3 /210.0 B:60.1 /60.0
 *   M114                             X:152.30 Y:152.30 Z:10.00 E:0.00
 *   M27                              SD printing byte 12345/678900
 *   M115                             FIRMWARE_NAME:Marlin 2.1.x ...
 *   G28 / M104 / M140 / M25 / M24 / M524 / G1 ...
 *                                    ok            (command finished)
 *                                    echo:...      (message for the user)
 *                                    Error:...     (or "!!")
 *
 * The MAN HINH CYD POLLS; Marlin never has to push anything. That keeps Marlin's side to
 * "enable a spare serial port", with no UI code compiled in.
 */
#pragma once

#include <Arduino.h>
#include "cydlcd_config.h"

#define CYDLCD_MSG_LEN 34

struct CydlcdState {
  float nozzle = 0, nozzle_target = 0;
  float bed = 0, bed_target = 0;
  float x = 0, y = 0, z = 0;
  uint8_t progress = 0;          // 0..100
  bool printing = false;
  bool paused = false;
  bool link_ok = false;
  char message[CYDLCD_MSG_LEN] = "dang ket noi...";
  char firmware[24] = "";
};

class MarlinLink {
public:
  void begin();
  void loop();

  const CydlcdState &state() const { return _st; }

  // Queue one line to Marlin (max 4 pending, each < 40 chars). Returns false when full.
  bool send(const char *cmd);
  bool send(const __FlashStringHelper *cmd);

  uint32_t lastOkMs() const { return _last_ok_ms; }

private:
  CydlcdState _st;
  char _rx[96];
  uint8_t _rx_len = 0;
  char _tx[4][40];
  uint8_t _tx_head = 0, _tx_tail = 0, _tx_count = 0;

  uint32_t _last_temp = 0, _last_pos = 0, _last_prog = 0, _last_ok_ms = 0;
  bool _m115_sent = false;

  void push(const char *cmd);
  void handleLine(char *line);
  static float parseFloat(const char *s);
  static bool findValue(const char *line, const char *key, float &out);   // "X:12.3"
  static bool tokenFloat(const char *after, float &out);                  // "12.3 /..."
};
