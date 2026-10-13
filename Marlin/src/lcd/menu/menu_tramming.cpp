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
// Bed Tramming Wizard - LOP UI cua man 12864 (MarlinUI).
//
// 2026-10-13: THUAT TOAN DA DUOC TACH sang Marlin/src/feature/tramming_run.cpp/.h de
//   man hinh rieng cydlcd (feature/cydlcd_tram_link.cpp, ASCII tren USART3) dung CHUNG
//   mot trang thai. File nay chi con:
//     - ve man hinh (4 so do lech + huong dan van vit + 3 nut);
//     - doc encoder / nut bam;
//     - goi tramming_run::step() 1 lan moi khung hinh khi wizard dang chay.
//   HANH VI KHONG DOI so voi ban cu (moc Z-MIN/Z-MAX chot 1 lan, nguong auto-DAT 0.01mm,
//   tu di tiep khi DAT, DONE = goc nay xong, BACK = thoat, ca 4 DAT -> hien delta so voi
//   tam + chi con nut BACK). Xem khoi chu thich dai o tramming_run.cpp.
//
// MAN HINH (21 ky tu x 4 dong):
//   dong 0-1: do lech 2 goc mot dong, dau '*' = goc nozzle dang dung
//   dong 2  : huong dan van vit (CW/CCW + so mm) hoac ket qua lan PROBE vua roi
//   dong 3  : ">MAX->MIN >BACK >DONE" (hoac chi ">BACK" khi ca 4 goc da dat)
//

#include "../../inc/MarlinConfigPre.h"

#if BOTH(HAS_MARLINUI_MENU, ASSISTED_TRAMMING_WIZARD)

#include "menu_item.h"

#include "../../feature/tramming.h"       // tramming_points, tramming_worst_max, tramming_turn_cw
#include "../../feature/tramming_run.h"   // may trang thai dung chung voi man hinh rieng cydlcd

#include "../../module/motion.h"          // base_home_pos(Z_AXIS)

// Man hinh ve theo dai (stripe) tren LCD do hoa; cac man khac luon ve ca man
#ifndef PAGE_CONTAINS
  #define PAGE_CONTAINS(...) true
#endif

//#define DEBUG_OUT 1
#include "../../core/debug_out.h"

// 2026-10-12: bo nut PREV / NEXT / HOME+PROBE -> con 4 nut:
//   - DONE tu di toi goc CAN CHINH ke tiep (bo qua goc MOC va cac goc da DAT) + probe luon goc do
//   - khi khong con goc nao -> tu HOME lai Z + probe lai CA 4 GOC (vong moi, moc moi)
//   - BACK: thoat ve man hinh chinh
// "DAT" (auto-done) khi |delta| <= TRAM_DONE_MM (0.01mm) - nguong nhieu cua probe.
#define BTN_PROBE 0   // probe lai goc dang dung
#define BTN_WORST 1   // doi chien luoc: MAX (moc = Z-MIN) <-> MIN (moc = Z-MAX)
#define BTN_BACK  2   // thoat ve man hinh chinh
#define BTN_DONE  3   // di toi goc can chinh ke tiep; het goc -> home lai + probe lai 4 goc
#define BTN_COUNT 4

static uint8_t btn_sel;                    // BTN_* (chi man 12864 dung)

// ---------------------------------------------------------------------------
// Hook ma tramming_run goi (thay cho ui.refresh() / ui.goto_previous_screen... trong code cu)
// ---------------------------------------------------------------------------
static void tramming_on_refresh()   { ui.refresh(); }
static void tramming_on_exit()      { ui.goto_previous_screen_no_defer(); }
// Vua chuyen sang TR_IDLE: chon san nut dung nhu code cu
//   - ca 4 goc da dat -> chi con BACK
//   - con goc phai chinh -> PROBE (nut dung nhieu nhat)
static void tramming_on_enter_idle() { btn_sel = tramming_run::all_dat ? BTN_BACK : BTN_PROBE; }

// ---------------------------------------------------------------------------
// Tien ich ve
// ---------------------------------------------------------------------------

// Nhan 2 ky tu [L/R][F/B] suy ra TU TOA DO (khong hard-code thu tu)
static void tram_tag(const uint8_t i, char * const out) {
  out[0] = tramming_points[i].x < X_CENTER ? 'L' : 'R';
  out[1] = tramming_points[i].y < Y_CENTER ? 'F' : 'B';
  out[2] = '\0';
}

// "+0.023" / "-0.008" / "  ---" : do lech so voi MOC = Z-MIN (che do MAX) hoac Z-MAX (che do MIN).
// Che do MAX: moi so >= 0 -> phai HA xuong (van CCW). Che do MIN: moi so <= 0 -> phai NANG len (van CW).
// 3 so le (0.001mm) khi |lech| < 1mm vi sai so that su chi vai phan tram mm; >= 1mm thi 2 so le
// la du va con giu duoc dong hien thi rong 21 ky tu (buffer 12 byte luon du).
static void tram_fmt_delta(const uint8_t i, const float mean, char * const out) {
  if (!tramming_run::ok(i)) { strcpy(out, "  ---"); return; }
  const float d = constrain(tramming_run::delta(i) - mean, -9999.0f, 9999.0f);  // chot an toan cho buffer
  dtostrf(d, 1, ABS(d) >= 1 ? 2 : 3, out);
}

static void tram_row(const uint8_t row, const char * const text) {
  const uint8_t y = LCD_ROW_Y(row);
  if (PAGE_CONTAINS(y - MENU_FONT_HEIGHT, y + 2))
    lcd_put_u8str(0, y, text);
}

// ---------------------------------------------------------------------------
// Ve man hinh
// ---------------------------------------------------------------------------
static void tramming_draw() {
  ui.set_font(FONT_STATUSMENU);

  char dots[] = "...";

  // Dang chay: chi hien tien do
  if (tramming_run::state != TR_IDLE) {
    switch (tramming_run::state) {
      case TR_HOMING: MenuEditItemBase::draw_edit_screen(F("Re-homing Z"), dots); break;
      case TR_GOTO:   MenuEditItemBase::draw_edit_screen(F("Go to worst"), dots); break;
      case TR_POINT:  MenuEditItemBase::draw_edit_screen(F("Go to point"), dots); break;
      case TR_AUTO:   MenuEditItemBase::draw_edit_screen(F("Auto next"), dots); break;
      default: {
        char v[8];
        snprintf_P(v, sizeof(v), PSTR("%u/%u"), uint16_t(tramming_run::step_index + 1), uint16_t(G35_PROBE_COUNT));
        MenuEditItemBase::draw_edit_screen(F("Probing corner"), v);
      } break;
    }
    return;
  }

  char line[40], d0[12], d1[12];

  // Dong 0-1: do lech 2 goc mot dong.
  //   Binh thuong: so voi MOC (Z-MIN hoac Z-MAX cua lan do dau - dong bang).
  //   Khi CA 4 GOC DA DAT: hien delta-Z so voi TAM BAN (= diem home Z, base_home_pos) de biet
  //   moi goc CAO HON hay THAP HON tam (so DUONG = goc cao hon tam).
  const float ref = tramming_run::display_ref();
  LOOP_L_N(r, 2) {
    const uint8_t i0 = r * 2, i1 = i0 + 1;
    char t0[3], t1[3];
    tram_tag(i0, t0); tram_tag(i1, t1);
    tram_fmt_delta(i0, ref, d0);
    tram_fmt_delta(i1, ref, d1);
    snprintf_P(line, sizeof(line), PSTR("%c%s %s %c%s %s"),
      tramming_run::worst_index == i0 ? '*' : ' ', t0, d0,
      tramming_run::worst_index == i1 ? '*' : ' ', t1, d1);
    tram_row(r, line);
  }

  // Dong 2: nut PROBE + ket qua lan probe vua roi.
  //   PR_RESULT: "CW con +0.013" = van tiep CW, con lech +0.013mm (target) - chi lay z CUOI,
  //              khong cong don. Van dung chieu thi so nay GIAM, van sai thi TANG.
  //   PR_NONE  : chua probe lai -> "CCW 0.150mm" (huong + so MM can van)
  //   "DAT" khi |delta| < 0.002mm (duoi nguong nhieu). Tat ca in 3 so le (0.001mm).
  if (tramming_run::all_dat) {
    // CA 4 GOC da dat -> khong con gi de van: chi con nut BACK.
    // 4 so o 2 dong tren luc nay la delta-Z SO VOI TAM BAN (diem home Z):
    // duong = goc CAO hon tam, am = goc THAP hon tam.
    snprintf_P(line, sizeof(line), PSTR("DAT - lech so voi tam"));
    tram_row(2, line);
  }
  else switch (tramming_run::probe_state) {
    case PR_RESULT: {
      // Chi lay z CUOI: delta so voi MOC = con phai van bao nhieu mm nua de goc do ve moc.
      // Van dung chieu -> so nay giam; van sai chieu -> so nay tang.
      const float delta = tramming_run::delta(tramming_run::worst_index) - ref;
      if (ABS(delta) <= TRAM_DONE_MM)
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
      const uint8_t i = tramming_run::worst_index;
      if (!tramming_run::ok(i))
        snprintf_P(line, sizeof(line), PSTR("%cPROBE goc nay"), btn_sel == BTN_PROBE ? '>' : ' ');
      else {
        const float delta = tramming_run::delta(i) - ref;    // ref = MOC da tinh o tren
        if (ABS(delta) <= TRAM_DONE_MM)
          snprintf_P(line, sizeof(line), PSTR("%cPROBE DAT"), btn_sel == BTN_PROBE ? '>' : ' ');
        else if (ABS(delta) < 0.02f)             // duoi 2/100 mm -> chi can van nhe
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

  // Dong 3: doi chien luoc MAX <-> MIN | BACK (thoat) | DONE (di tiep goc can chinh / home lai)
  // 21 ky tu: ">MAX->MIN >BACK >DONE"
  if (tramming_run::all_dat)
    snprintf_P(line, sizeof(line), PSTR("%cBACK"), btn_sel == BTN_BACK ? '>' : ' ');
  else
    snprintf_P(line, sizeof(line), PSTR("%c%s->%s %cBACK %cDONE"),
      btn_sel == BTN_WORST ? '>' : ' ',
      tramming_worst_max ? "MAX" : "MIN", tramming_worst_max ? "MIN" : "MAX",
      btn_sel == BTN_BACK ? '>' : ' ',
      btn_sel == BTN_DONE ? '>' : ' ');
  tram_row(3, line);
}

// ---------------------------------------------------------------------------
// Vong lap man hinh: ve + nhan encoder/nut + 1 buoc cua may trang thai
// ---------------------------------------------------------------------------
static void _lcd_tramming() {
  if (ui.should_draw()) tramming_draw();

  if (tramming_run::busy) return;           // dang probe / di chuyen -> khong nhan gi

  // TR_IDLE: nhan encoder + click MOI VONG LAP (khong doi first_page).
  // Cac trang thai TU DONG (homing / probe tung goc / di chuyen) thi moi khung hinh chi tien 1 buoc.
  //
  // Vi sao phai tach: truoc 2026-10-12 toan bo phan nhan input nam sau `!ui.first_page`, nen
  //   (a) input chi duoc nhan o lan ve DAU TIEN cua moi chu ky refresh, va
  //   (b) vi dung `else if`, mot xung encoder con lai (ke ca nhieu nho) lam CU BAM bi bo qua
  //       trong khung hinh do -> bam lan 1 khong an, phai bam lan 2.
  if (tramming_run::state != TR_IDLE && !ui.first_page) return;

  if (tramming_run::state == TR_IDLE) {
    if (tramming_run::all_dat) {
      ui.encoderPosition = 0;               // chi con BACK -> khong cho chon muc khac
      btn_sel = BTN_BACK;
    }
    else if (ui.encoderPosition) {
      const bool up = ui.encoderPosition > 0;
      ui.encoderPosition = 0;
      btn_sel = up ? (btn_sel + 1) % BTN_COUNT : (btn_sel + BTN_COUNT - 1) % BTN_COUNT;
      ui.refresh();                         // ve lai ngay -> thay muc dang chon
    }
    // KHONG dung `else`: cu bam khong duoc phep bi xung encoder con lai chan (loi "phai bam 2 lan").
    if (ui.use_click()) {
      switch (btn_sel) {
        case BTN_PROBE: tramming_run::probe_here(); break;
        case BTN_WORST: tramming_run::toggle_minmax(); break;
        case BTN_DONE:  tramming_run::done(); break;
        case BTN_BACK:
        default:        tramming_run::exit(); return;
      }
    }
  }
  else {
    // TR_HOMING / TR_PROBE / TR_GOTO / TR_POINT / TR_AUTO: 1 buoc moi khung hinh.
    // (Khi bat CYDLCD_TRAM_LINK ma man 12864 nay dang mo thi CHINH CHO NAY la nguon "tien buoc"
    //  duy nhat; cydlcd_tram_link::tick() chi tien khi ui_attached = false - xem tramming_run.cpp.)
    tramming_run::step();
  }

  ui.refresh();
}

// ---------------------------------------------------------------------------
// Diem vao tu menu
// ---------------------------------------------------------------------------
void goto_tramming_wizard() {
  DEBUG_ECHOLNPGM("Screen: goto_tramming_wizard", 1);
  ui.defer_status_screen();

  // Noi hook: tu day tramming_run goi nguoc lai UI cua man 12864 (giong code cu)
  tramming_run::refresh_hook = tramming_on_refresh;
  tramming_run::exit_hook = tramming_on_exit;
  tramming_run::enter_idle_hook = tramming_on_enter_idle;
  tramming_run::ui_attached = true;         // man 12864 la driver -> cydlcd khong tu tien buoc

  btn_sel = BTN_PROBE;

  // Home tat ca truc; Z-home chinh la TAM BAN -> moc cho moi delta
  tramming_run::begin();

  ui.goto_screen(_lcd_tramming);
}

#endif // HAS_MARLINUI_MENU && ASSISTED_TRAMMING_WIZARD
