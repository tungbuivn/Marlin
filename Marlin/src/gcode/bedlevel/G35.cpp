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
  // HAI MOC KHAC NHAU - dung ca hai, moi cai mot viec:
  //
  //  1. MOC vat ly cua truc Z = "Z-HOME" (khung Z cua lan home Z). Vi Z duoc home bang chinh
  //     probe nay (Z_SAFE_HOMING probe ngay tai TAM BAN, Z_SAFE_HOMING_X/Y_POINT), luc trigger
  //     tai tam ban firmware gan cho dau in gia tri logic
  //         Z = base_home_pos(Z_AXIS) - probe.offset.z      (set_axis_is_at_home, motion.cpp)
  //     roi probe_at_point() lai CONG probe.offset.z vao ket qua (probe.cpp), nen gia tri no
  //     tra ve tai BAT KY diem nao chinh la:
  //         chieu_cao[i] = z_probe[i] - z_home              (z_home = base_home_pos(Z_AXIS))
  //     -> KHONG can probe them tam ban (vua mat ~10s, vua cong sai so cua lan do do vao ca 4 so).
  //
  //  2. MOC DE VAN VIT = MAT PHANG TRUNG BINH cua cac goc da do (khong phai Z-home).
  //     "Chieu cao so voi Z-home" mot minh KHONG noi duoc gi ve do phang: vi du 4 goc
  //     cao 2.00 / 2.00 / 2.00 / 2.03 mm deu "cao 2mm so voi Z-home", nhin nhu ca ban bi
  //     nang 2mm - nhung thuc te 4 goc chi lech nhau co 0.03mm, tuc la gan nhu da phang.
  //     Cai can quan tam la cac goc LECH NHAU bao nhieu:
  //         delta[i] = chieu_cao[i] - trung_binh
  //     delta > 0 : goc CAO hon mat phang trung binh -> phai HA xuong
  //     delta < 0 : goc THAP hon                      -> phai NANG len
  //     Muc tieu: moi delta = 0 (4 goc cung nam tren mot mat phang).
  //
  //     Trung binh chinh la hinh chieu cua MAT PHANG BINH PHUONG-TOI-THIEU xuong tam ban
  //     (mat phang LS luon di qua trong tam cua tap diem), nen day la moc "it sai nhat":
  //       - tong cac delta = 0 va |delta| lon nhat la nho nhat co the
  //       - KHONG phu thuoc vao Z-home, vao M851 (probe.offset.z) hay lan home nao
  //         -> so lieu van dung ke ca khi Z offset chua can chuan
  //
  const float z_home = base_home_pos(Z_AXIS);

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
    // Trung binh + do trai (max-min) cua cac goc do duoc
    float h_sum = 0, h_min = 0, h_max = 0, worst_abs = 0;
    uint8_t seen = 0, z_worst = 0;
    LOOP_L_N(i, G35_PROBE_COUNT) if (z_probed[i]) {
      const float h = z_measured[i] - z_home;
      h_sum += h;
      if (!seen || h < h_min) h_min = h;
      if (!seen || h > h_max) h_max = h;
      ++seen;
    }
    const float h_mean = h_sum / n_probed, h_spread = h_max - h_min;

    SERIAL_ECHOLNPGM("G35 moc = Z-HOME (khung Z sau G28 Z)  Z=", z_home);
    SERIAL_ECHOPGM("Chieu cao TB cac goc = ");
    SERIAL_ECHO_F(h_mean, 3);
    SERIAL_ECHOPGM("mm  |  Do lech max-min = ");
    SERIAL_ECHO_F(h_spread, 3);
    SERIAL_ECHOLNPGM("mm");
    SERIAL_ECHOLNPGM("Moc de van vit = MAT PHANG TRUNG BINH. delta = chieu_cao - TB; KHONG phu thuoc Z-offset.");
    SERIAL_ECHOLNPGM("Chieu van: CW = HA goc do xuong, CCW = NANG goc do len.");
    if (n_probed < G35_PROBE_COUNT)
      SERIAL_ECHOLNPGM("CANH BAO: chi do duoc ", n_probed, "/", G35_PROBE_COUNT, " diem -> TB tinh tren ", n_probed, " diem.");

    // Tung goc: chieu + so vong vit, va do lech so voi mat phang trung binh
    LOOP_L_N(i, G35_PROBE_COUNT) {
      SERIAL_ECHOPGM("Turn ");
      SERIAL_ECHOPGM_P((char *)pgm_read_ptr(&tramming_point_name[i]));

      if (!z_probed[i]) {
        SERIAL_ECHOLNPGM(": KHONG probe duoc (loi) -> khong tinh duoc can chinh.");
        continue;
      }

      const float height = z_measured[i] - z_home,              // chieu cao so voi Z-home
                  delta  = height - h_mean;                     // > 0 = goc CAO hon mat phang TB -> phai HA xuong

      if (ABS(delta) < 0.002f) {
        // Trong nguong nhieu cua probe -> khong can van
        SERIAL_ECHOPGM(": DAT (khong can van)  | lech ");
        SERIAL_ECHO_F(delta, 3);
        SERIAL_ECHOPGM("mm");
      }
      else {
        // Chieu van: CW = HA goc do xuong, CCW = NANG goc do len (xem tramming.h)
        const float turns = tramming_turns(delta, screw_thread);
        const int full_turns = trunc(turns);
        const int minutes = trunc((turns - float(full_turns)) * 60.0f);

        SERIAL_ECHOPGM(": van ", tramming_turn_cw(delta, screw_thread) ? "CW" : "CCW", " by ", full_turns, " turns");
        if (minutes) SERIAL_ECHOPGM(" and ", minutes, " minutes");
        SERIAL_ECHOPGM("  | lech ");
        SERIAL_ECHO_F(delta, 3);
        SERIAL_ECHOPGM("mm");
      }

      if (ENABLED(REPORT_TRAMMING_MM)) {
        SERIAL_ECHOPGM("  (cao ");
        SERIAL_ECHO_F(height, 3);
        SERIAL_ECHOPGM("mm)");
      }
      SERIAL_EOL();

      if (ABS(delta) > worst_abs) { worst_abs = ABS(delta); z_worst = i; }
    }

    // Goc lech nhieu nhat -> chinh goc do truoc (kem luon chieu van)
    if (worst_abs >= 0.002f) {
      const float worst_delta = z_measured[z_worst] - z_home - h_mean;
      SERIAL_ECHOPGM("Chinh truoc: ");
      SERIAL_ECHOPGM_P((char *)pgm_read_ptr(&tramming_point_name[z_worst]));
      SERIAL_ECHOPGM(" (lech ");
      SERIAL_ECHO_F(worst_abs, 3);
      // Chuoi runtime phai nam o VI TRI GIA TRI (tham so thu 2), khong duoc de trong
      // SERIAL_ECHOLNPGM("<literal>") vi GCC se noi "\n" chi vao nhanh cuoi cua ternary.
      SERIAL_ECHOPGM("mm) -> van ", tramming_turn_cw(worst_delta, screw_thread) ? "CW" : "CCW");
      SERIAL_EOL();
    }
    else
      SERIAL_ECHOLNPGM("Ket luan: 4 goc da nam tren cung mot mat phang (|delta| < 0.002mm).");
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
