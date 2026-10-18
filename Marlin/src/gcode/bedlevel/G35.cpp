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

#include "../../inc/MarlinConfig.h"

#if ENABLED(ASSISTED_TRAMMING)

#include "../gcode.h"
#include "../../module/planner.h"
#include "../../module/probe.h"
#include "../../feature/bedlevel/bedlevel.h"

#if HAS_MULTI_HOTEND
  #include "../../module/tool_change.h"
#endif

#if ENABLED(BLTOUCH)
  #include "../../feature/bltouch.h"
#endif

#define DEBUG_OUT ENABLED(DEBUG_LEVELING_FEATURE)
#include "../../core/debug_out.h"

//
// Define tramming point names.
//

#include "../../feature/tramming.h"

/**
 * G35: Read bed corners to help adjust bed screws
 *
 *   S<screw_thread>
 *
 * Screw thread: 30 - Clockwise M3
 *               31 - Counter-Clockwise M3
 *               40 - Clockwise M4
 *               41 - Counter-Clockwise M4
 *               50 - Clockwise M5
 *               51 - Counter-Clockwise M5
 *
 * MOC CUA MOI CHIEU CAO (Z) LA "Z-HOME" - khung Z cua lan G28 Z, khong phai mot lan
 * probe tam ban rieng. Con SO VONG VIT tinh theo delta so voi MAT PHANG TRUNG BINH cua
 * cac goc da do (xem giai thich trong than G35()).
 **/
void GcodeSuite::G35() {
  DEBUG_SECTION(log_G35, "G35", DEBUGGING(LEVELING));

  if (DEBUGGING(LEVELING)) log_machine_info();

  float z_measured[G35_PROBE_COUNT] = { 0 };

  const uint8_t screw_thread = parser.byteval('S', TRAMMING_SCREW_THREAD);
  if (!WITHIN(screw_thread, 30, 51) || screw_thread % 10 > 1) {
    SERIAL_ECHOLNPGM("?(S)crew thread must be 30, 31, 40, 41, 50, or 51.");
    return;
  }

  // Wait for planner moves to finish!
  planner.synchronize();

  // Disable the leveling matrix before auto-aligning
  #if HAS_LEVELING
    #if ENABLED(RESTORE_LEVELING_AFTER_G35)
      const bool leveling_was_active = planner.leveling_active;
    #endif
    set_bed_leveling_enabled(false);
  #endif

  #if ENABLED(CNC_WORKSPACE_PLANES)
    workspace_plane = PLANE_XY;
  #endif

  // Always home with tool 0 active
  #if HAS_MULTI_HOTEND
    const uint8_t old_tool_index = active_extruder;
    tool_change(0, true);
  #endif

  // Disable duplication mode on homing
  TERN_(HAS_DUPLICATION_MODE, set_duplication_enabled(false));

  // Home only Z axis when X and Y is trusted, otherwise all axes, if needed before this procedure
  if (!all_axes_trusted()) process_subcommands_now(F("G28Z"));


  //
  // Probe tat ca cac goc. Goc nao probe loi thi GHI NHAN roi di tiep (khong bo ca phep do):
  // mot lan probe loi thuong chi la tam thoi, 3 diem con lai van du de dung mat phang.
  //
  bool z_probed[G35_PROBE_COUNT] = { false };
  uint8_t n_probed = 0;

  LOOP_L_N(i, G35_PROBE_COUNT) {

    // In BLTOUCH HS mode, the probe travels in a deployed state.
    // Users of G35 might have a badly misaligned bed, so raise Z by the
    // length of the deployed pin (BLTOUCH stroke < 7mm)

    // Unsure if this is even required. The probe seems to lift correctly after probe done.
    do_blocking_move_to_z(SUM_TERN(BLTOUCH, Z_CLEARANCE_BETWEEN_PROBES, bltouch.z_extra_clearance()));
    const float z_probed_height = probe.probe_at_point(tramming_points[i], PROBE_PT_RAISE, 0, true);

    if (isnan(z_probed_height)) {
      SERIAL_ECHOPGM("G35 probe FAILED at point ", i + 1, " (");
      SERIAL_ECHOPGM_P((char *)pgm_read_ptr(&tramming_point_name[i]));
      SERIAL_CHAR(')');
      SERIAL_ECHOLNPGM_P(SP_X_STR, tramming_points[i].x, SP_Y_STR, tramming_points[i].y);
      continue;
    }

    if (DEBUGGING(LEVELING)) {
      DEBUG_ECHOPGM("Probing point ", i + 1, " (");
      DEBUG_ECHOF(FPSTR(pgm_read_ptr(&tramming_point_name[i])));
      DEBUG_CHAR(')');
      DEBUG_ECHOLNPGM_P(SP_X_STR, tramming_points[i].x, SP_Y_STR, tramming_points[i].y, SP_Z_STR, z_probed_height);
    }

    z_measured[i] = z_probed_height;
    z_probed[i] = true;
    ++n_probed;
  }

  if (n_probed < 3)
    SERIAL_ECHOLNPGM("G35 aborted: chi probe duoc ", n_probed, "/", G35_PROBE_COUNT, " diem (can it nhat 3).");
  else {
    int z_worst=0;
    LOOP_L_N(i, G35_PROBE_COUNT) {
      const float h = z_measured[i];
      
     
     if (tramming_worst_max) {
        if (h<=z_measured[z_worst]) {
          z_worst = i;
        }
      } else {
         if (h>=z_measured[z_worst]) {
          z_worst = i;
        }
      }
    }
    
    


   
  }

  // Restore the active tool after homing
  #if HAS_MULTI_HOTEND
    if (old_tool_index != 0) tool_change(old_tool_index, DISABLED(PARKING_EXTRUDER)); // Fetch previous toolhead if not PARKING_EXTRUDER
  #endif

  #if BOTH(HAS_LEVELING, RESTORE_LEVELING_AFTER_G35)
    set_bed_leveling_enabled(leveling_was_active);
  #endif

  // Stow the probe, as the last call to probe.probe_at_point(...) left
  // the probe deployed if it was successful.
  probe.stow();

  move_to_tramming_wait_pos();

  // After this operation the Z position needs correction
  set_axis_never_homed(Z_AXIS);
}

#endif // ASSISTED_TRAMMING
