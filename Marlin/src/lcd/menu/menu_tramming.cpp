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
// Bed Tramming Wizard
//
// Khac Marlin goc o 2 diem:
//
//  1) MOC CUA MOI DELTA Z LA TAM BAN (vi tri Z-home), khong phai mot goc nao.
//     Sau moi lan home Z, goc khung Z nam tai tam ban, nen "lech so voi tam" chinh la
//     "lech so voi Z-home". Khi vao wizard, tam ban duoc probe truoc de lam moc.
//
//  2) Co muc "Re-home Z + probe": van bat ky vit nao cung lam Z-home doi, nen sau khi
//     van phai home lai Z. Muc nay: NHO VI TRI HIEN TAI -> G28 Z -> QUAY VE DUNG VI TRI
//     DA NHO -> PROBE LAI ngay tai do (va moc tam ban tro thanh 0).
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

//#define DEBUG_OUT 1
#include "../../core/debug_out.h"

static float z_measured[G35_PROBE_COUNT];
static Flags<G35_PROBE_COUNT> z_isvalid;
static uint8_t tram_index = 0;

static float z_center = NAN;              // (mm) Z tai TAM BAN = moc cua moi delta Z
static float rehome_x, rehome_y;          // Vi tri duoc nho truoc khi home lai Z
static bool  rehome_running = false,      // Chan tai nhap man hinh cho (blocking move goi idle)
             center_probing = false;

#if HAS_LEVELING
  #include "../../feature/bedlevel/bedlevel.h"
#endif

static void tramming_wizard_menu();

// Nang len do cao an toan, di toi (x, y), roi probe. Tra ve true neu do duoc.
static bool tramming_probe_xy(const float x, const float y, float &out_z) {
  do_blocking_move_to_z(TERN(BLTOUCH, Z_CLEARANCE_DEPLOY_PROBE, Z_CLEARANCE_BETWEEN_PROBES));
  // Stow after each point with BLTouch "HIGH SPEED" mode for push-pin safety
  out_z = probe.probe_at_point(x, y, TERN0(BLTOUCH, bltouch.high_speed_mode) ? PROBE_PT_STOW : PROBE_PT_RAISE, 0, true);
  move_to_tramming_wait_pos();
  DEBUG_ECHOLNPGM("tramming_probe_xy(", x, ", ", y, ") = ", out_z);
  return !isnan(out_z);
}

// Probe TAM BAN (= vi tri Z-home): moc cho MOI delta Z
static bool probe_bed_center() {
  return tramming_probe_xy((float)Z_SAFE_HOMING_X_POINT, (float)Z_SAFE_HOMING_Y_POINT, z_center);
}

static bool probe_single_point() {
  const bool v = tramming_probe_xy(tramming_points[tram_index].x, tramming_points[tram_index].y, z_measured[tram_index]);
  z_isvalid.set(tram_index, v);
  return v;
}

static void _menu_single_probe() {
  DEBUG_ECHOLNPGM("Screen: single probe screen Arg:", tram_index);
  START_MENU();
  STATIC_ITEM(MSG_BED_TRAMMING, SS_LEFT);
  // Delta so voi TAM BAN (vi tri Z-home), khong phai so voi mot goc nao
  STATIC_ITEM_F(F("Delta vs center"), SS_LEFT,
    (!isnan(z_center) && z_isvalid[tram_index]) ? ftostr42_52(z_measured[tram_index] - z_center) : "---");
  ACTION_ITEM(MSG_UBL_BC_INSERT2, []{ if (probe_single_point()) ui.refresh(); });
  ACTION_ITEM(MSG_BUTTON_DONE, ui.goto_previous_screen);
  END_MENU();
}

//
// Man hinh cho: G28 Z xong -> quay ve dung vi tri da nho -> probe lai
//
static void _lcd_rehome() {
  if (ui.should_draw()) MenuItem_static::draw(1, F("Re-homing Z ..."));

  if (rehome_running || !all_axes_homed()) return;   // Cho G28 Z chay xong
  rehome_running = true;

  // Quay ve dung vi tri da nho
  do_blocking_move_to_z(Z_CLEARANCE_BETWEEN_PROBES);
  do_blocking_move_to_xy(rehome_x, rehome_y, XY_PROBE_FEEDRATE_MM_S);

  // Probe lai ngay tai do
  float z = NAN;
  const bool ok = tramming_probe_xy(rehome_x, rehome_y, z);

  // Sau khi home lai Z, goc khung Z = tam ban => tam ban = 0. Cac so do cu da doi moc
  // nen bi xoa; rieng diem vua probe duoc cap nhat lai.
  z_center = 0.0f;
  z_isvalid.reset();
  LOOP_L_N(i, G35_PROBE_COUNT) {
    if (ABS(tramming_points[i].x - rehome_x) < 0.5f && ABS(tramming_points[i].y - rehome_y) < 0.5f) {
      z_measured[i] = z;
      z_isvalid.set(i, ok);
    }
  }

  rehome_running = false;
  ui.goto_screen(tramming_wizard_menu);
}

//
// ACTION: nho vi tri hien tai -> home lai Z (man hinh cho se quay ve + probe lai)
//
static void tramming_rehome() {
  rehome_x = current_position.x;
  rehome_y = current_position.y;
  set_axis_never_homed(Z_AXIS);        // de all_axes_homed() = false cho toi khi G28 Z xong
  queue.inject(F("G28 Z"));
  ui.defer_status_screen();
  ui.goto_screen(_lcd_rehome);
}

static void tramming_wizard_menu() {
  START_MENU();
  STATIC_ITEM(MSG_SELECT_ORIGIN);

  // Moc: do Z tai TAM BAN (vi tri Z-home) truoc
  ACTION_ITEM_F(F("Probe center (Z-home)"), []{ if (probe_bed_center()) ui.refresh(); });

  // Draw a menu item for each tramming point
  for (tram_index = 0; tram_index < G35_PROBE_COUNT; tram_index++)
    SUBMENU_F(FPSTR(pgm_read_ptr(&tramming_point_name[tram_index])), _menu_single_probe);

  // Van vit lam Z-home doi -> home lai Z, quay ve dung cho cu va probe lai
  ACTION_ITEM_F(F("Re-home Z + probe"), tramming_rehome);

  ACTION_ITEM(MSG_BUTTON_DONE, []{
    probe.stow(); // Stow before exiting Tramming Wizard
    ui.goto_previous_screen_no_defer();
  });
  END_MENU();
}

// Init the wizard and enter the submenu
void goto_tramming_wizard() {
  DEBUG_ECHOLNPGM("Screen: goto_tramming_wizard", 1);
  ui.defer_status_screen();

  // Initialize measured point flags
  z_isvalid.reset();
  z_center = NAN;

  // Inject G28, wait for homing to complete,
  set_all_unhomed();
  queue.inject(TERN(CAN_SET_LEVELING_AFTER_G28, F("G28L0"), FPSTR(G28_STR)));

  ui.goto_screen([]{
    _lcd_draw_homing();
    if (all_axes_homed() && !center_probing) {
      // Probe TAM BAN truoc: do la moc cho MOI delta Z
      center_probing = true;
      probe_bed_center();
      center_probing = false;
      ui.goto_screen(tramming_wizard_menu);
    }
  });
}

#endif // HAS_MARLINUI_MENU && ASSISTED_TRAMMING_WIZARD
