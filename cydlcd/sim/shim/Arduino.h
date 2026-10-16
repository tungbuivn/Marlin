/**
 * Arduino.h (SHIM cho ban GIA LAP UI tren PC) -- cydlcd/sim/shim/Arduino.h
 *
 * VI SAO CO FILE NAY: cac file UI cua man hinh CYD (ui_*.cpp) goi `Serial.printf`, `millis()`,
 * `min()/max()`... cua Arduino. Ban gia lap chay tren PC (g++, khong co Arduino) nen phai tu cung
 * cap cac thu do. Thu muc `sim/shim` duoc dat TRUOC trong `-I` cua tools/run_sim.ps1 nen
 * `#include <Arduino.h>` trong code that se lay file nay.
 *
 * Day KHONG phai ban gia lap day du: chi du cho nhung gi ma khoi UI dung (xem
 * cydlcd/sim/README.md). Khong co GPIO/SPI/WiFi -- nhung thu do da bi stub o sim_stubs.cpp.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// ---- macro cua Arduino/AVR ma code UI dung ----
#ifndef PROGMEM
  #define PROGMEM
#endif
#ifndef PGM_P
  #define PGM_P const char *
#endif
#define pgm_read_byte(p)  (*(const uint8_t *)(p))
#define pgm_read_word(p)  (*(const uint16_t *)(p))
#define F(x)     (x)
#define PSTR(x)  (x)
#define IRAM_ATTR
#define ICACHE_RAM_ATTR
#define DRAM_ATTR
#define _BV(b)   (1 << (b))
#define lowByte(w)  ((uint8_t)((w) & 0xFF))
#define highByte(w) ((uint8_t)(((w) >> 8) & 0xFF))
#define bitRead(v, b)     (((v) >> (b)) & 0x01)
#define bitSet(v, b)      ((v) |= (1UL << (b)))
#define bitClear(v, b)    ((v) &= ~(1UL << (b)))

// Arduino dinh nghia min/max/abs/constrain la MACRO cho moi kieu so. Giu dung tinh than do
// (code UI duoc viet theo kieu Arduino, khong dung std::min/std::max).
#ifndef min
  #define min(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef max
  #define max(a, b) ((a) > (b) ? (a) : (b))
#endif
#ifndef abs
  #define abs(x) ((x) > 0 ? (x) : -(x))
#endif
#ifndef constrain
  #define constrain(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#endif
#ifndef round
  #define round(x) ((x) >= 0 ? (long)((x) + 0.5) : (long)((x) - 0.5))
#endif
#ifndef radians
  #define radians(d) ((d) * 0.017453292519943295769236907684886)
#endif
#ifndef degrees
  #define degrees(r) ((r) * 57.295779513082320876798154814105)
#endif

// ---------------------------------------------------------------------------
//  Serial gia: in thang ra stdout (console cua PC). Du `print/println/printf` ma UI dung.
// ---------------------------------------------------------------------------
class SimSerial {
 public:
  void begin(unsigned long) {}
  void end() {}
  int  available() { return 0; }
  int  read() { return -1; }
  void flush() { fflush(stdout); }

  size_t write(uint8_t c) { fputc(c, stdout); return 1; }
  size_t write(const uint8_t *b, size_t n) { return fwrite(b, 1, n, stdout); }
  int    availableForWrite() { return 4096; }

  void print(const char *s)          { if (s) fputs(s, stdout); }
  void print(char c)                 { fputc(c, stdout); }
  void print(int v)                  { printf("%d", v); }
  void print(unsigned int v)         { printf("%u", v); }
  void print(long v)                 { printf("%ld", v); }
  void print(unsigned long v)        { printf("%lu", v); }
  void print(double v)               { printf("%g", v); }
  void println()                     { fputc('\n', stdout); }
  void println(const char *s)        { print(s); println(); }
  void println(char c)               { print(c); println(); }
  void println(int v)                { print(v); println(); }
  void println(unsigned int v)       { print(v); println(); }
  void println(long v)               { print(v); println(); }
  void println(unsigned long v)      { print(v); println(); }
  void println(double v)             { print(v); println(); }

  void printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap);
  }
};

extern SimSerial Serial;

// ---- thoi gian: dong ho AO cua sim (xem sim_port.cpp) ----
uint32_t millis(void);
uint32_t micros(void);
void     delay(uint32_t ms);
void     delayMicroseconds(uint32_t us);
void     yield(void);
