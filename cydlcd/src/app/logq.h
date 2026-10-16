/**
 * logq.h -- hang doi LOG khong chan (da luong: task `link` khong bao gio duoc in ra Serial).
 *
 * VI SAO CAN (do that 2026-10-15, xem cydlcd/docs/ke-hoach-da-luong.md):
 *   `Serial` cua man hinh CYD la UART0 @115200 -> 1 dong log ~70-90 byte mat ~6-8 ms, con khoi log 1 Hz
 *   (~700 byte) mat ~60 ms. Do duoc: `[PERF] link ... gap max 63 549 us` - tuc duong serial Marlin bi chan
 *   ~63 ms moi giay CHI VI IN LOG. Voi da luong, task `link` (core 1) khong duoc phep `Serial.print`:
 *   no chi day chuoi vao hang doi (timeout 0, khong cho), task khac (core 0) in ra.
 *
 * LUAT: `logq_post()` KHONG BAO GIO chan - hang doi day thi BO dong do va dem lai (log mat con hon mat
 * duong serial). Dung giong chuoi printf: `logq_post("[UP] x=%d\n", x)`.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>

// Tao hang doi (goi 1 lan trong setup(), truoc khi tao cac task khac).
void logq_begin(void);
// Day 1 dong log (printf-style, toi da 96 byte). Khong chan; tra false neu hang doi day (da bo).
bool logq_post(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
// In cac dong dang cho ra Serial - CHI goi tu task `log`/vong lap core 0. In toi da `max_lines` dong/lan
// de khong giu core 0 qua lau.
void logq_task(uint8_t max_lines);
// Chan doan: so dong da bo (hang doi day) va so dong dang cho.
uint32_t logq_dropped(void);
uint8_t  logq_pending(void);
