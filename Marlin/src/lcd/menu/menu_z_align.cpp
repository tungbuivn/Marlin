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

//
// Manual Z Stepper Align (Z1 / Z2 / Z3 bang tay) - man hinh rieng, kieu nhu G35
//
// VI SAO CAN:
//   Vi tri probe KHONG trung vi tri truc Z. Dich Z1 (hoac Z2, Z3) len/xuong mot luong
//   x thi diem probe chi len/xuong mot luong KHAC (ti le don bay, va ti le nay con doi
//   theo do nghieng cua gantry). Nen muon diem probe len/xuong dung y muon thi phai lam
//   vong lap: probe -> dich mot chut -> probe lai -> dich tiep...
//
// CACH DUNG (2 che do, doi nhau bang nut bam):
//   MOVE  (vd "Z2 MOVE") : quay encoder = dich Z2 dung 0.01mm moi nac
//                          bam encoder  = sang che do PROBE
//   PROBE (vd "Z2 PROBE"): bam encoder  = probe diem so 2 (bam lai duoc nhieu lan)
//                          quay encoder = sang truc ke tiep (Z3, roi EXIT)
//   EXIT                 : bam = thoat (tu dong home lai Z), quay = quay ve Z1
//
// Dong dau hien "SP x.xxx" = do lech max-min giua cac diem da probe - day chinh la con so
// can dua ve 0 (giong "DEV" cua man hinh G34). Gia tri tung diem chi co y nghia TUONG DOI:
// khung toa do Z thay doi moi khi dich tay, nen hay nhin do lech va muc thay doi.
//
// AN TOAN:
//   - moi nac chi 0.01mm; mot lan quay nhanh bi chan o ZA_MAX_DETENTS nac
//   - CHI duoc ha xuong toi da ZA_DOWN_BUDGET (1mm) ke tu lan probe gan nhat. Muon ha
//     tiep phai probe lai. Day dung la vong lap ma cong cu nay sinh ra de lam, dong thoi
//     ngan dau in di xuong qua gan mat ban khi chua do lai.
//

#include "../../inc/MarlinConfigPre.h"

#if BOTH(HAS_MARLINUI_MENU, Z_STEPPER_AUTO_ALIGN)

#include "menu_item.h"

#include "../../module/motion.h"
#include "../../module/stepper.h"
#include "../../module/probe.h"
#include "../../feature/z_stepper_align.h"
#include "../../gcode/queue.h"

// Man hinh ve theo dai (stripe) tren LCD do hoa; cac man khac luon ve ca man
#ifndef PAGE_CONTAINS
  #define PAGE_CONTAINS(...) true
#endif

#define ZA_MOVE_SCALE    0.01f    // (mm) moi nac encoder
#define ZA_MOVE_FEEDRATE 600      // (mm/phut) toc do khi dich tay = 10mm/s
#define ZA_MAX_DETENTS   50       // Tran so nac trong MOT lan quay (= 0.5mm)
#define ZA_DOWN_BUDGET   1.0f     // (mm) duoc ha toi da 1mm ke tu lan probe gan nhat

static float   za_measured[NUM_Z_STEPPERS];   // (mm) ket qua probe gan nhat
static float   za_nudge[NUM_Z_STEPPERS];      // (mm) tong da dich tay ke tu lan probe do
static bool    za_valid[NUM_Z_STEPPERS];      // diem nay da probe duoc chua
static float   za_base_z;                     // current_position.z sau lan probe gan nhat
static uint8_t za_sel;                        // 0..NUM_Z_STEPPERS-1 = truc Z, NUM_Z_STEPPERS = EXIT
static bool    za_armed;                      // false = MOVE, true = PROBE

// ---------------------------------------------------------------------------

static void za_reset() {
  LOOP_L_N(k, NUM_Z_STEPPERS) {
    za_measured[k] = 0;
    za_nudge[k] = 0;
    za_valid[k] = false;
  }
  za_base_z = current_position.z;
  za_sel = 0;
  za_armed = false;
}

// Do lech max-min giua cac diem da probe
static const char* za_spread_str() {
  static char s[9];
  uint8_t n = 0;
  float mn = 0, mx = 0;
  LOOP_L_N(k, NUM_Z_STEPPERS) if (za_valid[k]) {
    if (!n || za_measured[k] < mn) mn = za_measured[k];
    if (!n || za_measured[k] > mx) mx = za_measured[k];
    ++n;
  }
  if (n < 2) { strcpy(s, "---"); return s; }
  dtostrf(mx - mn, 1, 3, s);
  return s;
}

static const char* za_state_str() {
  static char s[14];
  if (za_sel >= NUM_Z_STEPPERS)
    strcpy(s, "EXIT");
  else
    snprintf_P(s, sizeof(s), PSTR("Z%u %s"), uint16_t(za_sel + 1), za_armed ? "PROBE" : "MOVE");
  return s;
}

// ---------------------------------------------------------------------------
// Dich DUY NHAT mot truc Z (Z1 / Z2 / Z3) - cung cach G34 lam khi bu sai so
// ---------------------------------------------------------------------------
static void za_apply_nudge(const int8_t dir, const float amount) {
  if (za_sel >= NUM_Z_STEPPERS || amount <= 0) return;

  const float from_z = current_position.z;
  float target = from_z + dir * amount;

  // Chan ha xuong: toi da ZA_DOWN_BUDGET ke tu lan probe gan nhat
  if (dir < 0) {
    const float floor_z = za_base_z - ZA_DOWN_BUDGET;
    if (target < floor_z) target = floor_z;
  }

  if (target == from_z) return;

  stepper.set_separate_multi_axis(true);
  stepper.set_all_z_lock(true, za_sel);                    // chi de mo Z(za_sel + 1)
  do_blocking_move_to_z(target, MMM_TO_MMS(ZA_MOVE_FEEDRATE));
  stepper.set_all_z_lock(false);
  stepper.set_separate_multi_axis(false);

  za_nudge[za_sel] += current_position.z - from_z;         // luong THUC SU da dich
}

// ---------------------------------------------------------------------------
// Probe diem cua truc dang chon
// ---------------------------------------------------------------------------
static void za_do_probe() {
  if (za_sel >= NUM_Z_STEPPERS) return;

  const uint8_t k = za_sel;

  // Nang len do cao an toan neu dang thap hon (KHONG BAO GIO ha xuong o buoc nay,
  // de khong dam vao vat dang in tren ban)
  if (current_position.z < (float)Z_CLEARANCE_BETWEEN_PROBES)
    do_blocking_move_to_z(Z_CLEARANCE_BETWEEN_PROBES);

  const float z = probe.probe_at_point(
    DIFF_TERN(HAS_HOME_OFFSET, z_stepper_align.xy[k], xy_pos_t(home_offset)),
    PROBE_PT_RAISE, 0, true
  );

  if (isnan(z)) {
    za_valid[k] = false;
    ui.set_status(F("Z ALIGN: probe loi"));
    return;
  }

  za_measured[k] = z;
  za_base_z = current_position.z;   // sau probe dau in da duoc nang len Z_CLEARANCE
  za_valid[k] = true;
  za_nudge[k] = 0;                  // moc lai tu day
  ui.set_status(F("Z ALIGN: da probe"));
}

// ---------------------------------------------------------------------------
// Ve man hinh (5 dong: trang thai + do lech, Z1, Z2, Z3, huong dan)
// ---------------------------------------------------------------------------
static void za_draw_row(const uint8_t row, const char * const text) {
  const uint8_t y = LCD_ROW_Y(row);
  if (PAGE_CONTAINS(y - MENU_FONT_HEIGHT, y + 2))
    lcd_put_u8str(0, y, text);
}

static void za_draw() {
  ui.set_font(FONT_STATUSMENU);

  char line[28];

  // Dong 0: dang o truc nao / che do nao + do lech giua cac diem
  snprintf_P(line, sizeof(line), PSTR("%-10s SP %s"), za_state_str(), za_spread_str());
  za_draw_row(0, line);

  // Mot dong cho moi truc Z: '>' = dang chon | ket qua probe | da dich tay bao nhieu
  LOOP_L_N(k, NUM_Z_STEPPERS) {
    char v[10], n[10];
    if (za_valid[k]) dtostrf(za_measured[k], 6, 3, v);
    else             strcpy(v, "   ---");
    dtostrf(za_nudge[k], 6, 3, n);
    snprintf_P(line, sizeof(line), PSTR("%cZ%u %s %s"),
      za_sel == k ? '>' : ' ', uint16_t(k + 1), v, n);
    za_draw_row(k + 1, line);
  }

  // Dong cuoi: huong dan theo che do dang o
  #if NUM_Z_STEPPERS <= 3
    {
      const uint8_t y = LCD_ROW_Y(NUM_Z_STEPPERS + 1);
      if (PAGE_CONTAINS(y - MENU_FONT_HEIGHT, y + 2))
        lcd_put_u8str_P(0, y,
          za_sel >= NUM_Z_STEPPERS ? PSTR("CLICK EXIT TURN BACK") :
          za_armed                 ? PSTR("CLICK PROBE TURN NEXT") :
                                     PSTR("TURN .01 CLICK PROBE")
        );
    }
  #endif
}

// ---------------------------------------------------------------------------
// Thoat: tra stepper ve trang thai binh thuong va home lai Z
// ---------------------------------------------------------------------------
static void za_exit() {
  stepper.set_all_z_lock(false);
  stepper.set_separate_multi_axis(false);
  IF_DISABLED(TOUCH_MI_PROBE, probe.stow());

  // Sau khi dich tay tung truc Z thi khung Z cua firmware khong con dung nua
  // (gantry da nghieng so voi luc home) -> home lai Z truoc khi lam viec khac.
  set_axis_never_homed(Z_AXIS);
  queue.inject(F("G28Z"));
  ui.set_status(F("Z ALIGN: xong, home lai Z"));

  ui.goto_previous_screen_no_defer();
}

static void _menu_z_align() {
  if (ui.should_draw()) za_draw();

  // --- Bam nut encoder ---
  if (ui.use_click()) {
    if (za_sel >= NUM_Z_STEPPERS) {          // EXIT
      za_exit();
      return;
    }
    if (!za_armed)                           // MOVE -> PROBE
      za_armed = true;
    else {                                   // PROBE -> probe that, roi ve MOVE
      za_do_probe();
      za_armed = false;
    }
    ui.refresh();
    return;
  }

  // --- Quay encoder ---
  if (ui.encoderPosition) {
    if (za_sel < NUM_Z_STEPPERS && !za_armed) {
      // MOVE: moi nac = 0.01mm
      int32_t d = ui.encoderPosition;
      ui.encoderPosition = 0;
      LIMIT(d, -ZA_MAX_DETENTS, ZA_MAX_DETENTS);
      za_apply_nudge(d > 0 ? 1 : -1, ZA_MOVE_SCALE * float(ABS(d)));
    }
    else {
      // PROBE / EXIT: quay = sang o ke tiep (Z1 -> Z2 -> Z3 -> EXIT -> Z1)
      ui.encoderPosition = 0;
      za_sel = (za_sel + 1) % (NUM_Z_STEPPERS + 1);
      za_armed = false;
    }
    ui.refresh();
  }
}

// ---------------------------------------------------------------------------
// Diem vao tu menu (Motion > Z ALIGN MANUAL)
// ---------------------------------------------------------------------------
static void za_enter() {
  // Nang len do cao an toan truoc khi di XY (sau khi home, dau in dang o mat ban).
  // Chi NANG, khong ha - de khong dam vao vat dang in.
  if (current_position.z < (float)Z_CLEARANCE_BETWEEN_PROBES)
    do_blocking_move_to_z(Z_CLEARANCE_BETWEEN_PROBES);
  za_base_z = current_position.z;
  ui.goto_screen(_menu_z_align);
}

void goto_z_align_manual() {
  za_reset();
  ui.defer_status_screen();

  if (!all_axes_homed()) {
    queue.inject(FPSTR(G28_STR));
    ui.goto_screen([]{
      _lcd_draw_homing();
      if (all_axes_homed()) za_enter();
    });
    return;
  }

  za_enter();
}

#endif // HAS_MARLINUI_MENU && Z_STEPPER_AUTO_ALIGN
