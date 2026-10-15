/**
 * logq.cpp -- hang doi log khong chan (xem logq.h).
 */
#include "logq.h"

#if defined(CYDLCD_BOARD_CYD)
  #include <Arduino.h>                  // Serial (UART0), millis()
  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
  #include <stdarg.h>
  #include <stdio.h>

  // 12 dong x 96 byte = 1,2 KB RAM - du cho cac dot log ngan (log chan doan 1 Hz + vai dong su kien).
  #define LOGQ_LINES  12
  #define LOGQ_LEN    96

  static QueueHandle_t s_q = nullptr;
  static uint32_t      s_drop = 0;

  void logq_begin(void) {
    if (!s_q) s_q = xQueueCreate(LOGQ_LINES, LOGQ_LEN);
  }

  bool logq_post(const char *fmt, ...) {
    if (!s_q) {                                   // chua begin (vd log tu setup()) -> in truc tiep
      va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
      return true;
    }
    char b[LOGQ_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    // THOI DIEM GUI = 0 (khong cho): day thi BO + dem lai. KHONG BAO GIO duoc chan task `link`.
    if (xQueueSend(s_q, b, 0) != pdTRUE) { s_drop++; return false; }
    return true;
  }

  void logq_task(uint8_t max_lines) {
    if (!s_q) return;
    char b[LOGQ_LEN];
    while (max_lines--) {
      if (xQueueReceive(s_q, b, 0) != pdTRUE) break;
      Serial.print(b);                            // b da co '\n' o cuoi (neu nguoi goi them) hoac khong
    }
  }

  uint32_t logq_dropped(void) { return s_drop; }
  uint8_t  logq_pending(void) { return s_q ? (uint8_t)uxQueueMessagesWaiting(s_q) : 0; }

#else   // ---- board khac (STM32...): khong co FreeRTOS queue -> in truc tiep nhu cu ----
  #include <Arduino.h>
  #include <stdarg.h>
  #include <stdio.h>
  void logq_begin(void) {}
  bool logq_post(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); return true;
  }
  void logq_task(uint8_t) {}
  uint32_t logq_dropped(void) { return 0; }
  uint8_t  logq_pending(void) { return 0; }
#endif
