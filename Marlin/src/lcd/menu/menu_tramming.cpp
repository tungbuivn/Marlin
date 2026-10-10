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
//   3. Man hinh hien delta 4 goc + 3 nut:
//        PROBE : probe LAI ngay tai goc dang dung, KHONG home lai -> hien luong thay doi
//                (de biet vua van oc co lam Z doi khong)
//        NEXT  : home lai Z -> probe lai 4 goc -> di toi goc lech nhat (lap lai)
//        DONE  : thoat
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

#define BTN_PROBE 0
#define BTN_NEXT  1
#define BTN_DONE  2
#define BTN_COUNT 3

// Ket qua lan PROBE gan nhat
#define PR_NONE     0
#define PR_CHANGED  1
#define PR_SAME     2
#define PR_FAILED   3
#define PR_MEASURED 4

static float   z_delta[G35_PROBE_COUNT];   // (mm) lech so voi TAM BAN (= Z-home)
static bool    z_ok[G35_PROBE_COUNT];      // diem nay probe duoc chua
static uint8_t worst_index,                // goc lech nhieu nhat (nozzle dang o day)
               step_index;                 // dang probe toi goc thu may
static uint8_t tram_state;                 // TR_*
static bool    tram_busy;                  // chan tai nhap: blocking move goi idle() -> screen chay lai
static uint8_t btn_sel;                    // BTN_*
static uint8_t probe_state;                // PR_*
static float   probe_change;               // (mm) luong doi Z cua lan PROBE vua roi

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

static void tram_row(const uint8_t row, const char * const text) {
  const uint8_t y = LCD_ROW_Y(row);
  if (PAGE_CONTAINS(y - MENU_FONT_HEIGHT, y + 2))
    lcd_put_u8str(0, y, text);
}

// Nang len do cao an toan neu dang thap hon - KHONG BAO GIO ha xuong
static void tramming_clearance() {
  if (current_position.z < (float)Z_CLEARANCE_BETWEEN_PROBES)
    do_blocking_move_to_z(Z_CLEARANCE_BETWEEN_PROBES);
}

// Di toi (x, y) roi probe.
// - Truoc khi di XY: CHI nang len neu dang thap hon (khong ha xuong, khong cao hon)
// - Sau khi probe: KHONG nang len (PROBE_PT_NONE). Truoc day dung PROBE_PT_RAISE nen
//   sau moi lan probe nozzle tu nhac them Z_CLEARANCE_BETWEEN_PROBES = 5mm.
//   Nay nozzle dung nguyen tai diem vua cham.
static bool tramming_probe_xy(const float x, const float y, float &out_z) {
  tramming_clearance();
  out_z = probe.probe_at_point(x, y, TERN0(BLTOUCH, bltouch.high_speed_mode) ? PROBE_PT_STOW : PROBE_PT_NONE, 0, true);
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

// PROBE: CHI probe goc dang dung. KHONG probe tam ban, KHONG home lai.
//
// MOC LA KHUNG Z CUA LAN HOME CUOI (Z-home = tam ban tai thoi diem do). Moc nay KHONG
// doi khi ban van oc - no chi doi khi home lai (nut NEXT) hoac G92. Vi vay:
//   - gia tri probe tra ve = do lech cua diem do SO VOI MOC  (dung khung Z hien tai)
//   - "doi" = gia tri moi - gia tri cu, ca hai deu so voi CUNG MOT MOC
//     -> chinh la luong oC vua siet lam diem do nhich so voi moc.
// KHONG probe lai tam ban o day: tam ban (vat ly) da doi khi van oc, nhung MOC thi khong;
// probe tam se chi ton thoi gian va lam nguoi dung tuong moc bi doi.
// (Sau moi lan NEXT, z_ok[] duoc xoa het nen khong bao gio so sanh gia tri cua 2 moc khac nhau.)
static void tramming_probe_here() {
  const uint8_t i = worst_index;
  const bool had = z_ok[i];                          // co so cu trong CUNG mot moc?
  const float before = z_delta[i];

  float z = NAN;
  const bool ok = tramming_probe_xy(tramming_points[i].x, tramming_points[i].y, z);

  if (!ok) { probe_state = PR_FAILED; return; }

  z_delta[i] = z;                                    // do lech MOI so voi moc (khung Z hien tai)
  z_ok[i] = true;

  if (!had) {                                        // chua co so cu trong moc nay
    probe_change = z;
    probe_state = PR_MEASURED;
    return;
  }

  probe_change = z - before;                         // thay doi do lech SO VOI MOC
  probe_state = ABS(probe_change) < 0.005f ? PR_SAME : PR_CHANGED;
}

// NEXT: home lai Z (Z-home vua doi vi vua van vit) -> probe lai 4 goc -> toi goc lech nhat
static void tramming_restart() {
  LOOP_L_N(i, G35_PROBE_COUNT) z_ok[i] = false;
  probe_state = PR_NONE;
  // PROBE khong nang len nua nen nozzle co the dang cham ban. Z_HOMING_HEIGHT dang TAT
  // -> G28 KHONG tu nang truoc khi chay XY toi tam, nen phai nang o day keo cao ban.
  tramming_clearance();
  set_axis_never_homed(Z_AXIS);          // de all_axes_homed() = false cho toi khi G28 Z xong
  queue.inject(F("G28 Z"));
  tram_state = TR_HOMING;
  step_index = 0;
  ui.refresh();
}

static void tramming_exit() {
  tramming_clearance();                  // roi khoi ban truoc khi thoat (nozzle co the dang cham)
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

  // Dong 0-1: delta 2 goc mot dong; '*' = goc nozzle dang dung (goc lech nhat)
  LOOP_L_N(r, 2) {
    const uint8_t i0 = r * 2, i1 = i0 + 1;
    char t0[3], t1[3];
    tram_tag(i0, t0); tram_tag(i1, t1);
    tram_fmt_delta(i0, d0); tram_fmt_delta(i1, d1);
    snprintf_P(line, sizeof(line), PSTR("%c%s %s  %c%s %s"),
      worst_index == i0 ? '*' : ' ', t0, d0,
      worst_index == i1 ? '*' : ' ', t1, d1);
    tram_row(r, line);
  }

  // Dong 2: nut PROBE + ket qua lan probe vua roi (moi so deu so voi TAM BAN)
  switch (probe_state) {
    case PR_CHANGED: {
      char num[9];
      dtostrf(probe_change, 1, 2, num);
      snprintf_P(line, sizeof(line), PSTR("%cPROBE doi %smm"), btn_sel == BTN_PROBE ? '>' : ' ', num);
    } break;
    case PR_SAME:
      snprintf_P(line, sizeof(line), PSTR("%cPROBE khong doi"), btn_sel == BTN_PROBE ? '>' : ' ');
      break;
    case PR_MEASURED: {
      char num[9];
      dtostrf(probe_change, 1, 2, num);
      snprintf_P(line, sizeof(line), PSTR("%cPROBE lech %s"), btn_sel == BTN_PROBE ? '>' : ' ', num);
    } break;
    case PR_FAILED:
      snprintf_P(line, sizeof(line), PSTR("%cPROBE loi"), btn_sel == BTN_PROBE ? '>' : ' ');
      break;
    default:
      snprintf_P(line, sizeof(line), PSTR("%cPROBE goc nay"), btn_sel == BTN_PROBE ? '>' : ' ');
      break;
  }
  tram_row(2, line);

  // Dong 3-4: NEXT / DONE
  snprintf_P(line, sizeof(line), PSTR("%cNEXT (home + probe)"), btn_sel == BTN_NEXT ? '>' : ' ');
  tram_row(3, line);
  snprintf_P(line, sizeof(line), PSTR("%cDONE"), btn_sel == BTN_DONE ? '>' : ' ');
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
      btn_sel = BTN_PROBE;                    // mac dinh chon PROBE (nut dung nhieu nhat)
      probe_state = PR_NONE;                  // chua probe lai o goc nay
      break;

    default: {                                // TR_IDLE: quay = chon nut, bam = chay
      if (ui.encoderPosition) {
        const bool up = ui.encoderPosition > 0;
        ui.encoderPosition = 0;
        btn_sel = up ? (btn_sel + 1) % BTN_COUNT : (btn_sel + BTN_COUNT - 1) % BTN_COUNT;
      }
      else if (ui.use_click()) {
        switch (btn_sel) {
          case BTN_PROBE:
            tram_busy = true;
            tramming_probe_here();
            tram_busy = false;
            break;
          case BTN_NEXT:
            tramming_restart();
            return;
          default:
            tramming_exit();
            return;
        }
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
  tram_busy = false;
  btn_sel = BTN_PROBE; probe_state = PR_NONE; probe_change = 0;
  tram_state = TR_HOMING;                     // cho G28 (tat ca truc) xong roi probe 4 goc

  // Home tat ca truc; Z-home chinh la TAM BAN -> moc cho moi delta
  set_all_unhomed();
  queue.inject(TERN(CAN_SET_LEVELING_AFTER_G28, F("G28L0"), FPSTR(G28_STR)));

  ui.goto_screen(_lcd_tramming);
}

#endif // HAS_MARLINUI_MENU && ASSISTED_TRAMMING_WIZARD
