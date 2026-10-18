/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#pragma once

#include "../inc/MarlinConfig.h"
#include "../module/probe.h"

#if !WITHIN(TRAMMING_SCREW_THREAD, 30, 51) || TRAMMING_SCREW_THREAD % 10 > 1
  #error "TRAMMING_SCREW_THREAD must be equal to 30, 31, 40, 41, 50, or 51."
#endif

constexpr xy_pos_t tramming_points[] = TRAMMING_POINT_XY;

//
// Diem "lech nhat" (worst) ma Tramming Wizard dua dau in toi de van vit:
//   true  = MAX - goc CAO NHAT  (chinh goc cao xuong)
//   false = MIN - goc THAP NHAT (nang goc thap len)
//
// Doi khi vit cua goc MAX (hoac MIN) da het hanh trinh, van mai khong duoc nua -> doi sang
// goc nguoc lai: "thay vi dieu chinh max thi dieu chinh min".
//
// Bien nay la tuy chon THEO PHIEN: KHONG luu EEPROM (xem tbt_run.cpp: worst_toggle), nen moi
// lan khoi dong lai ve mac dinh MAX.
//
extern bool tramming_worst_max;

//
// CHIEU VAN VIT - CHUAN XAC DINH BANG TEST CASE CUA NGUOI DUNG (2026-10-17):
//
//   input  : br 1.25 | bl 1.33 | fr 0.52 | fl 0.73   (bl la moc = Z-MAX)
//   goc lech nhat : fr, delta = 0.52 - 1.33 = -0.81
//   yeu cau        : van CW
//   br            : delta = 1.25 - 1.33 = -0.08   -> cung van CW
//
// => delta AM (goc THAP hon moc)  -> CW
//    delta DUONG (goc CAO hon moc) -> CCW
//    (ren trai: khai bao ma S le 31/41/51 -> tu dong dao chieu)
//
// GHI CHU: 2026-10-17 co mot lan sua SAI cho rang "CW = ha goc xuong" nen doi thanh CW khi delta
// duong; test case tren da chung minh nguoc lai. Nay giu dung nhu ban goc.
inline bool tramming_turn_cw(const float delta, const uint8_t screw_thread) {
  const bool lower_cw = (screw_thread & 1);    // ren phai tren may nay (M4 = 40 -> 0)
  return (delta > 0) == lower_cw;              // delta <= 0 -> CW
}

// Buoc ren (mm/vong) theo ma S: S30=M3, S40=M4, S50=M5.
inline float tramming_pitch(const uint8_t screw_thread) {
  constexpr float threads_factor[] = { 0.5f, 0.7f, 0.8f };   // M3, M4, M5 (mm/vong)
  return threads_factor[(screw_thread - 30) / 10];
}

// SO MM CAN CHINH - tu 2026-10-12 hien thi theo MM, khong phai "vong".
// Chinh vit di mot doan X mm thi goc do len/xuong dung X mm, nen so mm can chinh CHINH LA
// |delta| (KHONG chia cho buoc ren). Ham duoi day giu lai chi de tham chieu / tinh nguoc.
//
// So vong can van cho do lech delta (mm) - luon duong; chieu lay tu tramming_turn_cw()
inline float tramming_turns(const float delta, const uint8_t screw_thread) {
  return ABS(delta) / tramming_pitch(screw_thread);
}

#define G35_PROBE_COUNT COUNT(tramming_points)
static_assert(WITHIN(G35_PROBE_COUNT, 3, 6), "TRAMMING_POINT_XY requires between 3 and 6 XY positions.");

#define VALIDATE_TRAMMING_POINT(N) static_assert(N >= G35_PROBE_COUNT || Probe::build_time::can_reach(tramming_points[N]), \
  "TRAMMING_POINT_XY point " STRINGIFY(N) " is not reachable with the default NOZZLE_TO_PROBE offset and PROBING_MARGIN.")
VALIDATE_TRAMMING_POINT(0); VALIDATE_TRAMMING_POINT(1); VALIDATE_TRAMMING_POINT(2); VALIDATE_TRAMMING_POINT(3); VALIDATE_TRAMMING_POINT(4); VALIDATE_TRAMMING_POINT(5);

extern const char point_name_1[], point_name_2[], point_name_3[]
  #ifdef TRAMMING_POINT_NAME_4
    , point_name_4[]
    #ifdef TRAMMING_POINT_NAME_5
      , point_name_5[]
      #ifdef TRAMMING_POINT_NAME_6
        , point_name_6[]
      #endif
    #endif
  #endif
;

#define _NR_TRAM_NAMES 2
#ifdef TRAMMING_POINT_NAME_3
  #undef _NR_TRAM_NAMES
  #define _NR_TRAM_NAMES 3
  #ifdef TRAMMING_POINT_NAME_4
    #undef _NR_TRAM_NAMES
    #define _NR_TRAM_NAMES 4
    #ifdef TRAMMING_POINT_NAME_5
      #undef _NR_TRAM_NAMES
      #define _NR_TRAM_NAMES 5
      #ifdef TRAMMING_POINT_NAME_6
        #undef _NR_TRAM_NAMES
        #define _NR_TRAM_NAMES 6
      #endif
    #endif
  #endif
#endif
static_assert(_NR_TRAM_NAMES >= G35_PROBE_COUNT, "Define enough TRAMMING_POINT_NAME_s for all TRAMMING_POINT_XY entries.");
#undef _NR_TRAM_NAMES

extern PGM_P const tramming_point_name[];

#ifdef ASSISTED_TRAMMING_WAIT_POSITION
  void move_to_tramming_wait_pos();
#else
  inline void move_to_tramming_wait_pos() {}
#endif
