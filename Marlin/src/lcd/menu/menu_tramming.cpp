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
//      MOC CUA MOI GIA TRI PROBE LA KHUNG Z SAU KHI HOME ("Z-home") - khong can probe
//      rieng tam ban.
//      NHUNG CON SO HIEN TREN MAN HINH / DUNG DE VAN VIT thi tinh theo
//      MOC = Z-MIN hoac Z-MAX cua cac goc da do (xem tram_fmt_delta + G35.cpp):
//        che do MAX -> moc = Z-MIN: moi so >= 0 = phai HA xuong bao nhieu mm (van CCW)
//        che do MIN -> moc = Z-MAX: moi so <= 0 = phai NANG len bao nhieu mm (van CW)
//      Moc duoc CHOT (dong bang) sau lan do du 4 goc dau tien; PROBE lai 1 goc chi lam doi so
//      cua chinh goc do.
//   2. Tu dong di nozzle toi GOC "LECH NHAT" de nguoi dung van vit goc do.
//      Goc nao la "lech nhat" do bien tramming_worst_max quyet dinh:
//        MAX = goc CAO NHAT  |  MIN = goc THAP NHAT
//      (xep hang theo tri tuyet doi hay theo gia tri deu cho cung ket qua, vi moi goc bi
//       tru di cung mot moc)
//      Vi mot goc co the het hanh trinh vit (van mai khong duoc), muc WORST cho doi
//      ngay sang goc nguoc lai: thay vi dieu chinh max thi dieu chinh min.
//   3. Man hinh hien do lech 4 goc + 6 nut:
//        PROBE : probe LAI ngay tai goc dang dung, KHONG home lai -> hien TARGET con lai
//                (z cuoi, 3 so le) + huong van CW/CCW cho goc do
//        PREV  : lui 1 goc (vong tron) + probe luon goc do -> TU CHON lai nut PROBE
//        NEXT  : toi goc ke tiep (vong tron) + probe luon goc do -> TU CHON lai nut PROBE
//                -> van xong goc nay thi NEXT sang goc khac van tiep, khong phai home lai
//        MAX<->MIN : doi goc dang chinh (cao nhat <-> thap nhat), luu EEPROM, di ngay toi goc moi
//        HOME+PROBE: home lai Z + probe lai CA 4 GOC (khung Z moi) + toi goc lech nhat
//        DONE  : thoat
//      Vi van vit lam Z-home doi, nen sau khi van xong nen bam HOME+PROBE de do lai ca 4 goc.
//

#include "../../inc/MarlinConfigPre.h"

#if BOTH(HAS_MARLINUI_MENU, ASSISTED_TRAMMING_WIZARD)

#include "menu_item.h"

#include "../../feature/tramming.h"

#include "../../module/motion.h"
#include "../../module/probe.h"
#include "../../module/settings.h"
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
#define TR_POINT  4   // dang di toi goc ke tiep / goc truoc (nut PREV/NEXT)

#define BTN_PROBE 0   // probe lai goc dang dung
#define BTN_PREV  1   // lui 1 goc (vong tron) + probe
#define BTN_NEXT  2   // toi goc ke tiep (vong tron) + probe
#define BTN_WORST 3   // doi goc dang chinh: MAX (cao nhat) <-> MIN (thap nhat)
#define BTN_HOME  4   // home lai Z + probe lai CA 4 GOC (khung Z moi) + toi goc lech nhat
#define BTN_DONE  5   // thoat
#define BTN_COUNT 6

// Ket qua lan PROBE gan nhat
#define PR_NONE   0   // chua probe lai o goc nay -> hien huong van (CW/CCW + so mm)
#define PR_RESULT 1   // da probe lai -> hien TARGET con lai (z cuoi) + huong van
#define PR_FAILED 2

static float   z_delta[G35_PROBE_COUNT];   // (mm) gia tri probe GAN NHAT (moc vat ly = Z-home)
static bool    z_ok[G35_PROBE_COUNT];      // diem nay probe duoc chua
static uint8_t worst_index,                // goc lech nhieu nhat (nozzle dang o day)
               step_index;                 // dang probe toi goc thu may
static uint8_t tram_state;                 // TR_*
static bool    tram_busy;                  // chan tai nhap: blocking move goi idle() -> screen chay lai
static uint8_t btn_sel;                    // BTN_*
static uint8_t probe_state;                // PR_*
static float   z_ref_min, z_ref_max;       // MOC da chot: Z-MIN / Z-MAX cua LAN DO DAU TIEN (dong bang)
static bool    z_ref_set;                  // da chot moc chua (false = dang trong lan do dau)

// ---------------------------------------------------------------------------
// Tien ich
// ---------------------------------------------------------------------------

// Nhan 2 ky tu [L/R][F/B] suy ra TU TOA DO (khong hard-code thu tu)
static void tram_tag(const uint8_t i, char * const out) {
  out[0] = tramming_points[i].x < X_CENTER ? 'L' : 'R';
  out[1] = tramming_points[i].y < Y_CENTER ? 'F' : 'B';
  out[2] = '\0';
}

// MOC SO SANH = Z-MIN hoac Z-MAX cua cac goc da do (KHONG phai trung binh):
//
//   tramming_worst_max = true  (dang chinh goc CAO NHAT) -> MOC = Z-MIN (goc thap nhat)
//        so cua moi goc = z_i - Z_MIN >= 0  => "phai HA xuong bao nhieu mm"  (van CCW)
//   tramming_worst_max = false (dang chinh goc THAP NHAT) -> MOC = Z-MAX (goc cao nhat)
//        so cua moi goc = z_i - Z_MAX <= 0  => "phai NANG len bao nhieu mm"  (van CW)
//
// Chien luoc MOT CHIEU: hoac chi HA cac goc cao xuong bang goc thap nhat, hoac chi NANG cac goc
// thap len bang goc cao nhat -> khong bao gio phai vua nang vua ha, va khong dung tới Z-home.
//
// Ca Z-MIN lan Z-MAX duoc CHOT (dong bang) ngay sau khi do xong CA 4 GOC lan dau (sau khi home):
//   - doi MAX <-> MIN chi la doi lay Z-MIN hay Z-MAX lam moc, KHONG do lai gi.
//   - PROBE lai 1 goc CHI lam doi SO CUA GOC DO; 3 goc kia GIU NGUYEN (vi MOC khong doi).
//     (Truoc 2026-10-12 moc tinh lai moi lan ve man hinh -> probe 1 goc lam ca 3 so kia dich theo.)
//   - HOME+PROBE (hoac vao lai menu) do lai ca 4 goc -> MOC MOI.
//
// Neu chua co moc (dang trong lan do dau, hoac do loi <3 goc) thi tam lay cuc tri cua cac goc da do.
static float tramming_extreme_probed(const bool want_max) {
  float v = 0;
  bool found = false;
  LOOP_L_N(i, G35_PROBE_COUNT) if (z_ok[i]) {
    if (!found || (want_max ? z_delta[i] > v : z_delta[i] < v)) { v = z_delta[i]; found = true; }
  }
  return v;
}

static float tramming_ref() {
  if (!z_ref_set) return tramming_extreme_probed(!tramming_worst_max);
  return tramming_worst_max ? z_ref_min : z_ref_max;
}

// Chot moc tu cac goc vua do (lay CA Z-MIN lan Z-MAX). Chi nhan khi du >= 3 goc (nguong cua G35).
static void tramming_set_ref() {
  uint8_t n = 0;
  LOOP_L_N(i, G35_PROBE_COUNT) if (z_ok[i]) ++n;
  if (n >= 3) {
    z_ref_min = tramming_extreme_probed(false);   // goc THAP nhat
    z_ref_max = tramming_extreme_probed(true);    // goc CAO nhat
    z_ref_set = true;
  }
  DEBUG_ECHOLNPGM("tramming: chot MOC Z-MIN=", z_ref_min, " Z-MAX=", z_ref_max, " (", n, " goc)");
}

// "+0.023" / "-0.008" / "  ---" : do lech so voi MOC = Z-MIN (che do MAX) hoac Z-MAX (che do MIN).
// Che do MAX: moi so >= 0 -> phai HA xuong (van CCW). Che do MIN: moi so <= 0 -> phai NANG len (van CW).
// 3 so le (0.001mm) khi |lech| < 1mm vi sai so that su chi vai phan tram mm; >= 1mm thi 2 so le
// la du va con giu duoc dong hien thi rong 21 ky tu (buffer 12 byte luon du).
static void tram_fmt_delta(const uint8_t i, const float mean, char * const out) {
  if (!z_ok[i]) { strcpy(out, "  ---"); return; }
  const float d = constrain(z_delta[i] - mean, -9999.0f, 9999.0f);  // chot an toan cho buffer
  dtostrf(d, 1, ABS(d) >= 1 ? 2 : 3, out);
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

// Tim goc "lech nhat" theo che do dang chon:
//   tramming_worst_max = true  -> goc CAO NHAT (z_delta lon nhat)
//   tramming_worst_max = false -> goc THAP NHAT (z_delta nho nhat)
// roi di nozzle toi do (de nguoi dung van vit goc do).
static void tramming_goto_worst() {
  float best = 0;
  bool found = false;
  LOOP_L_N(i, G35_PROBE_COUNT) if (z_ok[i]) {
    const float d = z_delta[i];
    if (!found || (tramming_worst_max ? d > best : d < best)) { best = d; worst_index = i; found = true; }
  }
  if (!found) { DEBUG_ECHOLNPGM("tramming: khong probe duoc diem nao"); return; }

  DEBUG_ECHOLNPGM("tramming: goc lech nhat = ", worst_index, " (", tramming_worst_max ? "MAX" : "MIN", ") delta=", z_delta[worst_index]);

  do_blocking_move_to_z(_MAX((float)Z_AFTER_PROBING, (float)Z_CLEARANCE_BETWEEN_PROBES));
  do_blocking_move_to_xy(tramming_points[worst_index].x, tramming_points[worst_index].y, XY_PROBE_FEEDRATE_MM_S);
}

// Doi goc "lech nhat" giua MAX (goc cao nhat) va MIN (goc thap nhat) + LUU NGAY vao EEPROM
// de nho cho cac lan sau (dung khi vit cua goc dang chinh da het hanh trinh, khong van
// tiep duoc nua thi chuyen sang chinh goc nguoc lai).
void tramming_worst_toggle() {
  tramming_worst_max = !tramming_worst_max;
  settings.save();
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

  float z = NAN;
  const bool ok = tramming_probe_xy(tramming_points[i].x, tramming_points[i].y, z);

  if (!ok) { probe_state = PR_FAILED; return; }

  // Chi lay z CUOI. Khung Z khong doi giua cac lan probe (khong home lai) nen gia tri nay
  // dung de so ngay: van dung chieu -> target (delta so voi mat phang TB) GIAM, van sai -> TANG.
  // Cac goc CHUA do lai trong vong nay van dung so CU (khong xoa) cho toi khi bam HOME+PROBE.
  z_delta[i] = z;
  z_ok[i] = true;
  probe_state = PR_RESULT;
}

// Di nozzle toi goc KE TIEP (dir = +1) hoac goc TRUOC (dir = -1), di vong tron qua 4 goc,
// roi PROBE LUON goc do. Phai probe lai vi sau khi vua van mot goc thi cac goc kia da lech di
// (so cu khong con dung) - co so MOI trong cung khung Z thi moi biet can van bao nhieu.
static void tramming_goto_point(const int8_t dir) {
  worst_index = uint8_t((int8_t(worst_index) + int8_t(G35_PROBE_COUNT) + dir) % int8_t(G35_PROBE_COUNT));

  DEBUG_ECHOLNPGM("tramming: di toi goc ", worst_index, (dir > 0 ? " (NEXT)" : " (PREV)"));

  do_blocking_move_to_z(_MAX((float)Z_AFTER_PROBING, (float)Z_CLEARANCE_BETWEEN_PROBES));
  do_blocking_move_to_xy(tramming_points[worst_index].x, tramming_points[worst_index].y, XY_PROBE_FEEDRATE_MM_S);

  tramming_probe_here();          // probe luon -> man hinh co target MOI cua goc nay
}

// HOME+PROBE: home lai Z (Z-home vua doi vi vua van vit) -> probe lai CA 4 GOC (khung Z moi)
// -> di toi goc "lech nhat". Day la nut reset moc cho ca ban.
static void tramming_restart() {
  LOOP_L_N(i, G35_PROBE_COUNT) z_ok[i] = false;
  z_ref_set = false;                     // se CHOT MOC MOI sau khi do lai du 4 goc
  probe_state = PR_NONE;
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
      case TR_POINT:  MenuEditItemBase::draw_edit_screen(F("Go to point"), dots); break;
      default: {
        char v[8];
        snprintf_P(v, sizeof(v), PSTR("%u/%u"), uint16_t(step_index + 1), uint16_t(G35_PROBE_COUNT));
        MenuEditItemBase::draw_edit_screen(F("Probing corner"), v);
      } break;
    }
    return;
  }

  char line[40], d0[12], d1[12];

  // Dong 0-1: do lech 2 goc mot dong, so voi MOC (Z-MIN hoac Z-MAX cua lan do dau - dong bang)
  const float ref = tramming_ref();
  LOOP_L_N(r, 2) {
    const uint8_t i0 = r * 2, i1 = i0 + 1;
    char t0[3], t1[3];
    tram_tag(i0, t0); tram_tag(i1, t1);
    tram_fmt_delta(i0, ref, d0);
    tram_fmt_delta(i1, ref, d1);
    snprintf_P(line, sizeof(line), PSTR("%c%s %s %c%s %s"),
      worst_index == i0 ? '*' : ' ', t0, d0,
      worst_index == i1 ? '*' : ' ', t1, d1);
    tram_row(r, line);
  }

  // Dong 2: nut PROBE + ket qua lan probe vua roi.
  //   PR_RESULT: "CW con +0.013" = van tiep CW, con lech +0.013mm (target) - chi lay z CUOI,
  //              khong cong don. Van dung chieu thi so nay GIAM, van sai thi TANG.
  //   PR_NONE  : chua probe lai -> "CCW 0.150mm" (huong + so MM can van)
  //   "DAT" khi |delta| < 0.002mm (duoi nguong nhieu). Tat ca in 3 so le (0.001mm).
  switch (probe_state) {
    case PR_RESULT: {
      // Chi lay z CUOI: delta so voi MOC = con phai van bao nhieu mm nua de goc do ve moc.
      // Van dung chieu -> so nay giam; van sai chieu -> so nay tang.
      const float delta = z_delta[worst_index] - ref;
      if (ABS(delta) < 0.002f)
        snprintf_P(line, sizeof(line), PSTR("%cPROBE DAT"), btn_sel == BTN_PROBE ? '>' : ' ');
      else {
        char num[12];
        dtostrf(delta, 1, ABS(delta) >= 10 ? 2 : 3, num);
        snprintf_P(line, sizeof(line), PSTR("%cPROBE %s con %s"),
          btn_sel == BTN_PROBE ? '>' : ' ', tramming_turn_cw(delta, TRAMMING_SCREW_THREAD) ? "CW" : "CCW", num);
      }
    } break;
    case PR_FAILED:
      snprintf_P(line, sizeof(line), PSTR("%cPROBE loi"), btn_sel == BTN_PROBE ? '>' : ' ');
      break;
    default: {
      // Chua probe lai o goc nay -> huong dan luon cho goc dang dung:
      //   "CCW 0.150mm" = van CCW 0.150 mm (CCW = ha goc xuong, CW = nang goc len).
      //   So can chinh la MM, KHONG phai vong: vit di X mm => goc len/xuong X mm.
      const uint8_t i = worst_index;
      if (!z_ok[i])
        snprintf_P(line, sizeof(line), PSTR("%cPROBE goc nay"), btn_sel == BTN_PROBE ? '>' : ' ');
      else {
        const float delta = z_delta[i] - ref;    // ref = MOC da tinh o tren
        if (ABS(delta) < 0.002f)
          snprintf_P(line, sizeof(line), PSTR("%cPROBE DAT"), btn_sel == BTN_PROBE ? '>' : ' ');
        else if (ABS(delta) < 0.01f)             // duoi 1/100 mm -> chi can van nhe
          snprintf_P(line, sizeof(line), PSTR("%cPROBE %s nhe"),
            btn_sel == BTN_PROBE ? '>' : ' ', tramming_turn_cw(delta, TRAMMING_SCREW_THREAD) ? "CW" : "CCW");
        else {
          char num[10];
          dtostrf(ABS(delta), 1, 3, num);        // 0.150 = can chinh 0.150 mm
          snprintf_P(line, sizeof(line), PSTR("%cPROBE %s %smm"),
            btn_sel == BTN_PROBE ? '>' : ' ', tramming_turn_cw(delta, TRAMMING_SCREW_THREAD) ? "CW" : "CCW", num);
        }
      }
    } break;
  }
  tram_row(2, line);

  // Dong 3: PREV | NEXT | doi goc dang chinh (MAX <-> MIN)  - 3 muc, dung 21 ky tu
  snprintf_P(line, sizeof(line), PSTR("%cPREV %cNEXT %c%s->%s"),
    btn_sel == BTN_PREV ? '>' : ' ', btn_sel == BTN_NEXT ? '>' : ' ',
    btn_sel == BTN_WORST ? '>' : ' ',
    tramming_worst_max ? "MAX" : "MIN", tramming_worst_max ? "MIN" : "MAX");
  tram_row(3, line);

  // Dong 4: HOME+PROBE (home lai Z + probe lai ca 4 goc) | DONE (thoat)
  snprintf_P(line, sizeof(line), PSTR("%cHOME+PROBE %cDONE"),
    btn_sel == BTN_HOME ? '>' : ' ', btn_sel == BTN_DONE ? '>' : ' ');
  tram_row(4, line);
}

// ---------------------------------------------------------------------------
// Vong lap: 1 buoc moi khung hinh (first_page), blocking move khong lam sai trang thai
// ---------------------------------------------------------------------------
static void _lcd_tramming() {
  if (ui.should_draw()) tramming_draw();

  if (tram_busy) return;                    // dang probe / di chuyen -> khong nhan gi

  // TR_IDLE: nhan encoder + click MOI VONG LAP (khong doi first_page).
  // Cac trang thai TU DONG (homing / probe tung goc / di chuyen) thi moi khung hinh chi tien 1 buoc.
  //
  // Vi sao phai tach: truoc 2026-10-12 toan bo phan nhan input nam sau `!ui.first_page`, nen
  //   (a) input chi duoc nhan o lan ve DAU TIEN cua moi chu ky refresh, va
  //   (b) vi dung `else if`, mot xung encoder con lai (ke ca nhieu nho) lam CU BAM bi bo qua
  //       trong khung hinh do -> bam lan 1 khong an, phai bam lan 2.
  if (tram_state != TR_IDLE && !ui.first_page) return;

  switch (tram_state) {
    case TR_HOMING:
      if (!all_axes_homed()) return;          // cho G28 Z chay xong
      tram_state = TR_PROBE;
      step_index = 0;
      break;

    case TR_PROBE:
      if (step_index >= G35_PROBE_COUNT) {
        tramming_set_ref();                   // do xong CA 4 GOC -> CHOT MOC (dong bang tu day)
        tram_state = TR_GOTO;
        break;
      }
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
        ui.refresh();                         // ve lai ngay -> thay muc dang chon
      }
      // KHONG dung `else`: cu bam khong duoc phep bi xung encoder con lai chan (loi "phai bam 2 lan").
      if (ui.use_click()) {
        switch (btn_sel) {
          case BTN_PROBE:
            tram_busy = true;
            tramming_probe_here();
            tram_busy = false;
            break;
          case BTN_PREV:
          case BTN_NEXT: {
            // Lui / toi goc ke tiep (vong tron) + probe luon goc do.
            // Sau khi sang goc khac thi TU CHON lai nut PROBE (2026-10-12, theo yeu cau):
            // bam them 1 lan la probe lai goc vua toi, khong vo tinh di tiep.
            const int8_t dir = (btn_sel == BTN_NEXT) ? 1 : -1;
            tram_busy = true;
            tram_state = TR_POINT;                // man hinh hien "Go to point"
            ui.refresh();
            tramming_goto_point(dir);
            tram_state = TR_IDLE;
            tram_busy = false;
            btn_sel = BTN_PROBE;                  // <- chon san PROBE cho lan bam ke tiep
          } break;
          case BTN_HOME:
            // Home lai Z + probe lai CA 4 GOC (khung Z moi) + di toi goc lech nhat
            tramming_restart();
            return;
          case BTN_WORST:
            // Doi MAX <-> MIN + luu EEPROM, roi DI NGAY toi goc moi VA PROBE luon goc do
            // (de goc co dau '*' luon la so MOI).
            tram_busy = true;
            tramming_worst_toggle();
            tram_state = TR_POINT;                // man hinh hien "Go to point"
            ui.refresh();
            tramming_goto_worst();
            tramming_probe_here();
            tram_state = TR_IDLE;
            tram_busy = false;
            break;
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
  z_ref_set = false;                          // moc se duoc CHOT sau lan do 4 goc dau tien
  worst_index = 0; step_index = 0;
  tram_busy = false;
  btn_sel = BTN_PROBE; probe_state = PR_NONE;
  tram_state = TR_HOMING;                     // cho G28 (tat ca truc) xong roi probe 4 goc

  // Home tat ca truc; Z-home chinh la TAM BAN -> moc cho moi delta
  set_all_unhomed();
  queue.inject(TERN(CAN_SET_LEVELING_AFTER_G28, F("G28L0"), FPSTR(G28_STR)));

  ui.goto_screen(_lcd_tramming);
}

#endif // HAS_MARLINUI_MENU && ASSISTED_TRAMMING_WIZARD
