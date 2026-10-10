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
// Bed Tramming Wizard - thuat toan rieng cua du an nay (khac Marlin goc)
//
// LUONG LAM VIEC
//   1. Vao wizard: G28 (tat ca truc) -> TU DONG probe 4 goc
//      MOC CUA MOI DELTA Z LA TAM BAN. Sau moi lan home Z, goc cua khung Z nam ngay
//      tai tam ban (vi tri Z-home), nen gia tri probe tai moi goc CHINH LA "delta so voi
//      Z-home". Vi vay khong can probe rieng tam ban.
//   2. Tu dong di nozzle toi GOC CO |DELTA| LON NHAT de nguoi dung van vit goc do.
//   3. Man hinh hien delta ca 4 goc + goc dang chinh. Hai nut:
//        NEXT : home lai Z -> probe lai 4 goc -> di toi goc lech nhat  (lap lai)
//        DONE : thoat
//      Vi van vit lam Z-home doi, nen NEXT phai home lai Z truoc khi do lai.
//

#include "../../inc/MarlinConfigPre.h"

#if BOTH(HAS_MARLINUI_MENU, ASSISTED_TRAMMING_WIZARD)

#include "menu_item.h"

#include "../../feature/tramming.h"

#include "../../module/motion.h"
#include "../../module/probe.h"
#include "../../gcode/queue.h"

#if ENABLED(BLTOUCH)
  #include "../../feature/bltouch.h"
#endif

// Man hinh ve theo dai (stripe) tren LCD do hoa; cac man khac luon ve ca man
#ifndef PAGE_CONTAINS
  #define PAGE_CONTAINS(...) true
#endif

//#define DEBUG_OUT 1
#include "../../core/debug_out.h"

#define TR_IDLE   0   // dang cho nguoi dung van vit
#define TR_HOMING 1   // dang cho G28 Z xong
#define TR_PROBE  2   // dang probe 4 goc
#define TR_GOTO   3   // dang di toi goc lech nhat

static float   z_delta[G35_PROBE_COUNT];   // (mm) lech so voi TAM BAN (= Z-home)
static bool    z_ok[G35_PROBE_COUNT];      // diem nay probe duoc chua
static uint8_t worst_index,                // goc lech nhieu nhat (dang duoc chinh)
               step_index;                 // dang probe toi goc thu may
static uint8_t tram_state;                 // TR_*
static bool    tram_busy,                  // chan tai nhap: blocking move goi idle() -> screen chay lai
               exit_selected;              // false = NEXT, true = DONE

// ---------------------------------------------------------------------------
// Tien ich
// ---------------------------------------------------------------------------

// Nhan 2 ky tu [L/R][F/B] suy ra TU TOA DO (khong hard-code thu tu)
static void tram_tag(const uint8_t i, char * const out) {
  out[0] = tramming_points[i].x < X_CENTER ? 'L' : 'R';
  out[1] = tramming_points[i].y < Y_CENTER ? 'F' : 'B';
  out[2] = '\0';
}

// "+0.12" / "-0.05" / "  ---"
static void tram_fmt_delta(const uint8_t i, char * const out) {
  if (!z_ok[i]) { strcpy(out, "  ---"); return; }
  char num[9];
  dtostrf(ABS(z_delta[i]), 1, 2, num);
  snprintf_P(out, 9, PSTR("%c%s"), z_delta[i] < 0 ? '-' : '+', num);
}

// Do lech max-min giua cac diem do duoc
static float tram_spread() {
  uint8_t n = 0;
  float mn = 0, mx = 0;
  LOOP_L_N(i, G35_PROBE_COUNT) if (z_ok[i]) {
    if (!n || z_delta[i] < mn) mn = z_delta[i];
    if (!n || z_delta[i] > mx) mx = z_delta[i];
    ++n;
  }
  return n < 2 ? 0 : mx - mn;
}

static void tram_row(const uint8_t row, const char * const text) {
  const uint8_t y = LCD_ROW_Y(row);
  if (PAGE_CONTAINS(y - MENU_FONT_HEIGHT, y + 2))
    lcd_put_u8str(0, y, text);
}

// Nang len do cao an toan, di toi (x, y), roi probe
static bool tramming_probe_xy(const float x, const float y, float &out_z) {
  do_blocking_move_to_z(TERN(BLTOUCH, Z_CLEARANCE_DEPLOY_PROBE, Z_CLEARANCE_BETWEEN_PROBES));
  out_z = probe.probe_at_point(x, y, TERN0(BLTOUCH, bltouch.high_speed_mode) ? PROBE_PT_STOW : PROBE_PT_RAISE, 0, true);
  DEBUG_ECHOLNPGM("tramming_probe_xy(", x, ", ", y, ") = ", out_z);
  return !isnan(out_z);
}

// Tim goc co |delta| lon nhat roi di nozzle toi do (de nguoi dung van vit goc do)
static void tramming_goto_worst() {
  float worst = 0;
  bool found = false;
  LOOP_L_N(i, G35_PROBE_COUNT) if (z_ok[i]) {
    const float a = ABS(z_delta[i]);
    if (!found || a > worst) { worst = a; worst_index = i; found = true; }
  }
  if (!found) { DEBUG_ECHOLNPGM("tramming: khong probe duoc diem nao"); return; }

  DEBUG_ECHOLNPGM("tramming: goc lech nhat = ", worst_index, " delta=", z_delta[worst_index]);

  do_blocking_move_to_z(_MAX((float)Z_AFTER_PROBING, (float)Z_CLEARANCE_BETWEEN_PROBES));
  do_blocking_move_to_xy(tramming_points[worst_index].x, tramming_points[worst_index].y, XY_PROBE_FEEDRATE_MM_S);
}

// NEXT: home lai Z (Z-home vua doi vi vua van vit) -> probe lai 4 goc -> toi goc lech nhat
static void tramming_restart() {
  LOOP_L_N(i, G35_PROBE_COUNT) z_ok[i] = false;
  set_axis_never_homed(Z_AXIS);          // de all_axes_homed() = false cho toi khi G28 Z xong
  queue.inject(F("G28 Z"));
  tram_state = TR_HOMING;
  step_index = 0;
  ui.refresh();
}

static void tramming_exit() {
  probe.stow();
  set_axis_never_homed(Z_AXIS);          // vit da bi van -> phai home lai Z truoc khi di chuyen/in
  ui.goto_previous_screen_no_defer();
}

// ---------------------------------------------------------------------------
// Ve man hinh
// ---------------------------------------------------------------------------
static void tramming_draw() {
  ui.set_font(FONT_STATUSMENU);

  char dots[] = "...";

  // Dang chay: chi hien tien do
  if (tram_state != TR_IDLE) {
    switch (tram_state) {
      case TR_HOMING: MenuEditItemBase::draw_edit_screen(F("Re-homing Z"), dots); break;
      case TR_GOTO:   MenuEditItemBase::draw_edit_screen(F("Go to worst"), dots); break;
      default: {
        char v[8];
        snprintf_P(v, sizeof(v), PSTR("%u/%u"), uint16_t(step_index + 1), uint16_t(G35_PROBE_COUNT));
        MenuEditItemBase::draw_edit_screen(F("Probing corner"), v);
      } break;
    }
    return;
  }

  char line[28], tag[3], d0[8], d1[8];

  // Dong 0: goc dang chinh (nhan + toa do) va do lech tong
  if (z_ok[worst_index]) {
    char sp[9];
    dtostrf(tram_spread(), 1, 3, sp);
    tram_tag(worst_index, tag);
    snprintf_P(line, sizeof(line), PSTR("%s(%i,%i) SP %s"),
      tag, int(tramming_points[worst_index].x), int(tramming_points[worst_index].y), sp);
  }
  else
    strcpy(line, "---");
  tram_row(0, line);

  // Dong 1-2: delta 2 goc mot dong, theo thu tu TRAMMING_POINT_XY
  LOOP_L_N(r, 2) {
    const uint8_t i0 = r * 2, i1 = i0 + 1;
    char t0[3], t1[3];
    tram_tag(i0, t0); tram_tag(i1, t1);
    tram_fmt_delta(i0, d0); tram_fmt_delta(i1, d1);
    snprintf_P(line, sizeof(line), PSTR("%s %s   %s %s"), t0, d0, t1, d1);
    tram_row(r + 1, line);
  }

  // Dong 3-4: hai nut
  snprintf_P(line, sizeof(line), PSTR("%cNEXT (home + probe)"), exit_selected ? ' ' : '>');
  tram_row(3, line);
  snprintf_P(line, sizeof(line), PSTR("%cDONE"), exit_selected ? '>' : ' ');
  tram_row(4, line);
}

// ---------------------------------------------------------------------------
// Vong lap: 1 buoc moi khung hinh (first_page), blocking move khong lam sai trang thai
// ---------------------------------------------------------------------------
static void _lcd_tramming() {
  if (ui.should_draw()) tramming_draw();

  if (tram_busy || !ui.first_page) return;

  switch (tram_state) {
    case TR_HOMING:
      if (!all_axes_homed()) return;          // cho G28 Z chay xong
      tram_state = TR_PROBE;
      step_index = 0;
      break;

    case TR_PROBE:
      if (step_index >= G35_PROBE_COUNT) { tram_state = TR_GOTO; break; }
      tram_busy = true;
      z_ok[step_index] = tramming_probe_xy(tramming_points[step_index].x, tramming_points[step_index].y, z_delta[step_index]);
      tram_busy = false;
      ++step_index;
      break;

    case TR_GOTO:
      tram_busy = true;
      tramming_goto_worst();
      tram_busy = false;
      tram_state = TR_IDLE;
      exit_selected = false;                  // mac dinh chon NEXT
      break;

    default: {                                // TR_IDLE: nhan nut
      if (ui.encoderPosition) { ui.encoderPosition = 0; exit_selected = !exit_selected; }
      else if (ui.use_click()) {
        if (exit_selected) { tramming_exit(); return; }
        tramming_restart();
        return;
      }
      break;
    }
  }

  ui.refresh();
}

// ---------------------------------------------------------------------------
// Diem vao tu menu
// ---------------------------------------------------------------------------
void goto_tramming_wizard() {
  DEBUG_ECHOLNPGM("Screen: goto_tramming_wizard", 1);
  ui.defer_status_screen();

  LOOP_L_N(i, G35_PROBE_COUNT) z_ok[i] = false;
  worst_index = 0; step_index = 0;
  tram_busy = false; exit_selected = false;
  tram_state = TR_HOMING;                     // cho G28 (tat ca truc) xong roi probe 4 goc

  // Home tat ca truc; Z-home chinh la TAM BAN -> moc cho moi delta
  set_all_unhomed();
  queue.inject(TERN(CAN_SET_LEVELING_AFTER_G28, F("G28L0"), FPSTR(G28_STR)));

  ui.goto_screen(_lcd_tramming);
}

#endif // HAS_MARLINUI_MENU && ASSISTED_TRAMMING_WIZARD
