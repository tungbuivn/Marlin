/**
 * tramming_run.cpp -- May trang thai cua Bed Tramming Wizard (xem tramming_run.h).
 *
 * DAY LA BAN DI CHUYEN Y NGUYEN tu Marlin/src/lcd/menu/menu_tramming.cpp (ban 2026-10-12):
 * phan logic/trang thai/thuat toan duoc cat ra khoi lop UI, KHONG doi hanh vi. Nhung gi con lai
 * o menu_tramming.cpp chi la ve man hinh 12864 + doc encoder.
 *
 * LUONG LAM VIEC
 *   1. Vao wizard: G28 (tat ca truc) -> TU DONG probe 4 goc
 *      MOC CUA MOI GIA TRI PROBE LA KHUNG Z SAU KHI HOME ("Z-home") - khong can probe
 *      rieng tam ban.
 *      NHUNG CON SO HIEN TREN MAN HINH / DUNG DE VAN VIT thi tinh theo
 *      MOC = Z-MIN hoac Z-MAX cua cac goc da do (xem tram_fmt_delta + G35.cpp):
 *        che do MAX -> moc = Z-MIN: moi so >= 0 = phai HA xuong bao nhieu mm (van CCW)
 *        che do MIN -> moc = Z-MAX: moi so <= 0 = phai NANG len bao nhieu mm (van CW)
 *      Moc duoc CHOT (dong bang) sau lan do du 4 goc dau tien; PROBE lai 1 goc chi lam doi so
 *      cua chinh goc do.
 *   2. Tu dong di nozzle toi GOC "LECH NHAT" de nguoi dung van vit goc do.
 *      Goc nao la "lech nhat" do bien tramming_worst_max quyet dinh:
 *        MAX = goc CAO NHAT  |  MIN = goc THAP NHAT
 *      (xep hang theo tri tuyet doi hay theo gia tri deu cho cung ket qua, vi moi goc bi
 *       tru di cung mot moc)
 *      Vi mot goc co the het hanh trinh vit (van mai khong duoc), muc WORST cho doi
 *      ngay sang goc nguoc lai: thay vi dieu chinh max thi dieu chinh min.
 *   3. Man hinh hien do lech 4 goc + 3 nut (2026-10-12: BO PREV / NEXT / HOME+PROBE):
 *        PROBE     : probe LAI ngay tai goc dang dung, KHONG home lai -> hien con phai van bao
 *                    nhieu mm + huong van CW/CCW cho goc do
 *        MAX<->MIN : doi chien luoc (moc Z-MIN <-> Z-MAX), luu EEPROM, di ngay toi goc lech nhat
 *        DONE      : TU DI toi goc CAN CHINH ke tiep (bo qua goc MOC va cac goc da DAT) roi probe
 *                    luon goc do; khi KHONG con goc nao -> tu HOME lai Z + probe lai CA 4 GOC
 *                    (vong moi, moc moi)
 *      Vi van vit lam Z-home doi nen vong lap cu lap lai cho toi khi ca 4 goc nam tren moc.
 *
 * 2026-10-13: tach khoi MarlinUI + them duong dieu khien tu man hinh rieng lcd_panel
 *   (feature/panel_tram_link.cpp, giao thuc ASCII tren USART3). Ca 2 duong dung CHUNG
 *   trang thai trong namespace nay; chi MOT nguon duoc "tien buoc" (step):
 *     - menu 12864 dang mo wizard  -> ui_attached = true  -> menu_tramming goi step()
 *     - panel la nguon duy nhat    -> panel_tram_link::tick() goi step()
 *   => khong bao gio 2 nguon cung day trang thai.
 */

#include "../inc/MarlinConfigPre.h"

#if ENABLED(ASSISTED_TRAMMING_WIZARD)

#include "tramming_run.h"

#include "../module/motion.h"
#include "../module/probe.h"
#include "../module/settings.h"
#include "../gcode/queue.h"

#if ENABLED(BLTOUCH)
  #include "bltouch.h"
#endif

//#define DEBUG_OUT 1
#include "../core/debug_out.h"

namespace tramming_run {

// ---------------------------------------------------------------------------
// Hook UI (menu_tramming dat; link serial de NULL)
// ---------------------------------------------------------------------------
void (*refresh_hook)() = nullptr;
void (*exit_hook)() = nullptr;
void (*enter_idle_hook)() = nullptr;

// ---------------------------------------------------------------------------
// Trang thai
// ---------------------------------------------------------------------------
uint8_t state;                             // TR_*
uint8_t probe_state;                       // PR_*
uint8_t worst_index,                       // goc lech nhieu nhat (nozzle dang o day)
        step_index;                        // dang probe toi goc thu may
bool    all_dat;                           // CA 4 GOC da dat -> chi con nut BACK
bool    busy;                              // chan tai nhap: blocking move goi idle() -> screen chay lai
bool    active;                            // wizard da bat (tu begin/restart toi exit)
bool    ui_attached;                       // man 12864 dang mo wizard -> menu la driver

static float   z_delta[G35_PROBE_COUNT];   // (mm) gia tri probe GAN NHAT (moc vat ly = Z-home)
static bool    z_ok[G35_PROBE_COUNT];      // diem nay probe duoc chua
static bool    z_accepted[G35_PROBE_COUNT];// nguoi dung da bam DONE o goc nay = COI NHU XONG
                                           // (du |delta| chua vao nguong) -> khong quay lai goc do nua
static float   z_ref_min, z_ref_max;       // MOC da chot: Z-MIN / Z-MAX cua LAN DO DAU TIEN (dong bang)
static bool    z_ref_set;                  // da chot moc chua (false = dang trong lan do dau)

// ---------------------------------------------------------------------------
// Truy cap trang thai (UI / link serial)
// ---------------------------------------------------------------------------
float delta(const uint8_t i) { return z_delta[i]; }
bool  ok(const uint8_t i) { return z_ok[i]; }
bool  accepted(const uint8_t i) { return z_accepted[i]; }
bool  ref_set() { return z_ref_set; }
float ref_min() { return z_ref_min; }
float ref_max() { return z_ref_max; }

// ---------------------------------------------------------------------------
// Tien ich
// ---------------------------------------------------------------------------

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
//   - DONE khi HET goc (hoac vao lai menu) do lai ca 4 goc -> MOC MOI.
//
// Neu chua co moc (dang trong lan do dau, hoac do loi <3 goc) thi tam lay cuc tri cua cac goc da do.
static float extreme_probed(const bool want_max) {
  float v = 0;
  bool found = false;
  LOOP_L_N(i, G35_PROBE_COUNT) if (z_ok[i]) {
    if (!found || (want_max ? z_delta[i] > v : z_delta[i] < v)) { v = z_delta[i]; found = true; }
  }
  return v;
}

float ref() {
  if (!z_ref_set) return extreme_probed(!tramming_worst_max);
  return tramming_worst_max ? z_ref_min : z_ref_max;
}

// MOC dung de HIEN THI: khi CA 4 GOC DA DAT thi doi sang so voi TAM BAN (= diem home Z,
// base_home_pos) de biet moi goc CAO HON hay THAP HON tam (so DUONG = goc cao hon tam).
// (Man 12864 va panel dung cung ham nay -> 2 man hien ra so GIONG NHAU.)
float display_ref() { return all_dat ? base_home_pos(Z_AXIS) : ref(); }

// Chot moc tu cac goc vua do (lay CA Z-MIN lan Z-MAX). Chi nhan khi du >= 3 goc (nguong cua G35).
static void set_ref() {
  uint8_t n = 0;
  LOOP_L_N(i, G35_PROBE_COUNT) if (z_ok[i]) ++n;
  if (n >= 3) {
    z_ref_min = extreme_probed(false);   // goc THAP nhat
    z_ref_max = extreme_probed(true);    // goc CAO nhat
    z_ref_set = true;
  }
  DEBUG_ECHOLNPGM("tramming: chot MOC Z-MIN=", z_ref_min, " Z-MAX=", z_ref_max, " (", n, " goc)");
}

// Nang len do cao an toan, di toi (x, y), roi probe
static bool probe_xy(const float x, const float y, float &out_z) {
  do_blocking_move_to_z(TERN(BLTOUCH, Z_CLEARANCE_DEPLOY_PROBE, Z_CLEARANCE_BETWEEN_PROBES));
  out_z = probe.probe_at_point(x, y, TERN0(BLTOUCH, bltouch.high_speed_mode) ? PROBE_PT_STOW : PROBE_PT_RAISE, 0, true);
  DEBUG_ECHOLNPGM("tramming_probe_xy(", x, ", ", y, ") = ", out_z);
  return !isnan(out_z);
}

// Tim goc "lech nhat" theo che do dang chon:
//   tramming_worst_max = true  -> goc CAO NHAT (z_delta lon nhat)
//   tramming_worst_max = false -> goc THAP NHAT (z_delta nho nhat)
// roi di nozzle toi do (de nguoi dung van vit goc do).
void goto_worst() {
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
static void worst_toggle() {
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
static void probe_here_raw() {
  const uint8_t i = worst_index;

  float z = NAN;
  const bool ok_ = probe_xy(tramming_points[i].x, tramming_points[i].y, z);

  if (!ok_) { probe_state = PR_FAILED; return; }

  // Chi lay z CUOI. Khung Z khong doi giua cac lan probe (khong home lai) nen gia tri nay
  // dung de so ngay: van dung chieu -> target (delta so voi mat phang TB) GIAM, van sai -> TANG.
  // Cac goc CHUA do lai trong vong nay van dung so CU (khong xoa) cho toi khi bam DONE (het goc)
  z_delta[i] = z;
  z_ok[i] = true;
  probe_state = PR_RESULT;
}

// Di nozzle toi goc i: nang Z len do cao an toan, di XY, roi probe luon goc do.
// Phai probe lai vi sau khi vua van mot goc thi cac goc kia da lech di (so cu khong con dung)
// - co so MOI trong cung khung Z thi moi biet can van bao nhieu.
static void goto_index(const uint8_t i) {
  worst_index = i;
  DEBUG_ECHOLNPGM("tramming: di toi goc ", i);
  do_blocking_move_to_z(_MAX((float)Z_AFTER_PROBING, (float)Z_CLEARANCE_BETWEEN_PROBES));
  do_blocking_move_to_xy(tramming_points[i].x, tramming_points[i].y, XY_PROBE_FEEDRATE_MM_S);
  probe_here_raw();               // probe luon -> man hinh co so MOI cua goc nay
}

// ---------------------------------------------------------------------------
// Goc nay CON phai chinh khong?
//   - goc nguoi dung DA BAM DONE (z_accepted) thi KHONG: coi nhu xong, du delta chua vao nguong
//   - goc dinh nghia MOC (Z-MIN o che do MAX, Z-MAX o che do MIN) thi KHONG: no chinh la chuan
//   - goc da trong nguong nhieu (|delta| <= TRAM_DONE_MM = 0.01mm) thi KHONG -> coi nhu "auto DAT"
// ---------------------------------------------------------------------------
bool need_adjust(const uint8_t i) {
  if (z_accepted[i]) return false;                // nguoi dung da chap nhan goc nay
  if (!z_ok[i]) return true;                      // chua do duoc -> van phai toi kiem tra
  if (z_ref_set) {
    const bool is_ref = tramming_worst_max ? (z_delta[i] <= z_ref_min + 0.0005f)
                                           : (z_delta[i] >= z_ref_max - 0.0005f);
    if (is_ref) return false;
  }
  return ABS(z_delta[i] - ref()) > TRAM_DONE_MM;
}

// Co goc nao (trong CA 4) con phai chinh khong? false = tat ca da dat -> chi con nut BACK
bool any_need_adjust() {
  LOOP_L_N(i, G35_PROBE_COUNT) if (need_adjust(i)) return true;
  return false;
}

// Chon goc CAN CHINH ke tiep (di vong tron tu goc hien tai, bo qua goc MOC / goc da DAT).
// CHI xet 3 goc KHAC (khong quay lai chinh goc dang dung): DONE nghia la "goc nay xong roi,
// di sang goc khac". true = tim duoc (worst_index da doi), false = het goc -> home lai + probe 4 goc.
bool next_need_adjust() {
  LOOP_L_N(k, G35_PROBE_COUNT - 1) {
    const uint8_t i = uint8_t((worst_index + 1 + k) % G35_PROBE_COUNT);
    if (need_adjust(i)) { worst_index = i; return true; }
  }
  return false;
}

// ---------------------------------------------------------------------------
// Hanh dong (dung chung cho menu 12864 VA link serial)
// ---------------------------------------------------------------------------

// DONE khi het goc can chinh: home lai Z (Z-home vua doi vi vua van vit) -> probe lai CA 4 GOC
// -> di toi goc "lech nhat". Day la nut reset moc cho ca ban.
void restart() {
  LOOP_L_N(i, G35_PROBE_COUNT) { z_ok[i] = false; z_accepted[i] = false; }
  z_ref_set = false;                     // se CHOT MOC MOI sau khi do lai du 4 goc
  all_dat = false;                       // vong moi -> chua biet da dat hay chua
  probe_state = PR_NONE;
  set_axis_never_homed(Z_AXIS);          // de all_axes_homed() = false cho toi khi G28 Z xong
  queue.inject(F("G28 Z"));
  state = TR_HOMING;
  step_index = 0;
  active = true;
  refresh();
}

void exit() {
  probe.stow();
  set_axis_never_homed(Z_AXIS);          // vit da bi van -> phai home lai Z truoc khi di chuyen/in
  active = false;
  ui_attached = false;
  // Go hook UI: tu day wizard khong con thuoc man 12864 nua. Neu khong go, mot lan 'B' tu panel
  // sau nay se van goi ui.goto_previous_screen_no_defer() -> tu nhien doi man hinh 12864.
  void (*const on_exit)() = exit_hook;
  refresh_hook = nullptr;
  exit_hook = nullptr;
  enter_idle_hook = nullptr;
  if (on_exit) on_exit();
}

// Vao wizard (tu menu 12864 hoac tu lenh 'R' cua panel khi wizard chua chay):
// home TAT CA truc; Z-home chinh la TAM BAN -> moc cho moi delta.
void begin() {
  LOOP_L_N(i, G35_PROBE_COUNT) { z_ok[i] = false; z_accepted[i] = false; }
  z_ref_set = false;                     // moc se duoc CHOT sau lan do 4 goc dau tien
  all_dat = false;
  worst_index = 0; step_index = 0;
  busy = false;
  probe_state = PR_NONE;
  state = TR_HOMING;                     // cho G28 (tat ca truc) xong roi probe 4 goc
  active = true;

  set_all_unhomed();
  queue.inject(TERN(CAN_SET_LEVELING_AFTER_G28, F("G28L0"), FPSTR(G28_STR)));
}

// PROBE: probe lai goc dang dung. Goc vua probe DA DAT -> TU DI TIEP (khong can bam DONE).
void probe_here() {
  if (!active) return;
  busy = true;
  probe_here_raw();
  busy = false;
  if (!need_adjust(worst_index)) state = TR_AUTO;
}

// MAX<->MIN: doi chien luoc (moc Z-MIN <-> Z-MAX) + luu EEPROM, roi DI NGAY toi goc lech nhat
// cua che do moi VA PROBE luon goc do.
void toggle_minmax() {
  if (!active) return;
  busy = true;
  worst_toggle();
  state = TR_POINT;                      // man hinh hien "Go to point"
  refresh();
  goto_worst();
  probe_here_raw();
  busy = false;
  // Goc lech nhat cua che do moi ma da DAT => tat ca da dat -> tu di tiep / home lai
  state = need_adjust(worst_index) ? TR_IDLE : TR_AUTO;
}

// DONE = "GOC NAY XONG": danh dau goc dang dung la DA CHAP NHAN (coi nhu dat, du delta chua vao
// nguong) => lan sau khong quay lai goc nay nua, di thang toi goc can chinh ke tiep CHUA duoc
// chap nhan. Het goc -> home lai Z + probe lai CA 4 GOC (vong moi).
void done() {
  if (!active) return;
  z_accepted[worst_index] = true;
  if (next_need_adjust()) {
    busy = true;
    state = TR_POINT;                    // man hinh hien "Go to point"
    refresh();
    goto_index(worst_index);
    state = TR_IDLE;
    busy = false;
    if (enter_idle_hook) enter_idle_hook();   // UI: quay ve nut PROBE cho lan bam ke tiep
  }
  else {
    DEBUG_ECHOLNPGM("tramming: het goc can chinh -> home lai Z + probe lai 4 goc");
    restart();
  }
}

// ---------------------------------------------------------------------------
// Vong lap: 1 buoc moi khung hinh / moi lan idle, blocking move khong lam sai trang thai
// ---------------------------------------------------------------------------
void step() {
  if (busy) return;                      // dang probe / di chuyen -> khong nhan gi

  switch (state) {
    case TR_HOMING:
      if (!all_axes_homed()) return;     // cho G28 chay xong
      state = TR_PROBE;
      step_index = 0;
      break;

    case TR_PROBE:
      if (step_index >= G35_PROBE_COUNT) {
        set_ref();                       // do xong CA 4 GOC -> CHOT MOC (dong bang tu day)
        // CA 4 GOC da dat (|delta| <= 0.01mm) -> khong con gi de van: CHI HIEN NUT BACK
        // (khong tu home+probe lai nua, tranh lap vo tan)
        if (!any_need_adjust()) {
          DEBUG_ECHOLNPGM("tramming: ca 4 goc da DAT -> chi con nut BACK");
          all_dat = true;
          probe_state = PR_NONE;
          state = TR_IDLE;
          if (enter_idle_hook) enter_idle_hook();   // UI: chon san nut BACK (goc truoc khi ve lai)
          refresh();
          break;
        }
        state = TR_GOTO;
        break;
      }
      busy = true;
      z_ok[step_index] = probe_xy(tramming_points[step_index].x, tramming_points[step_index].y, z_delta[step_index]);
      busy = false;
      ++step_index;
      break;

    case TR_AUTO:
      // Goc vua probe DA DAT -> tu di tiep. Moi lan goi 1 buoc (move/probe la blocking).
      busy = true;
      if (next_need_adjust()) {
        state = TR_POINT;                // man hinh hien "Go to point"
        refresh();
        goto_index(worst_index);
        busy = false;
        // Goc vua toi: neu CUNG dat thi buoc sau di tiep, nguoc lai de nguoi dung van vit
        state = need_adjust(worst_index) ? TR_IDLE : TR_AUTO;
        if (state == TR_IDLE && enter_idle_hook) enter_idle_hook();
      }
      else {
        busy = false;
        DEBUG_ECHOLNPGM("tramming: het goc can chinh -> tu home lai Z + probe lai 4 goc");
        restart();                       // het goc -> TU home lai + probe lai CA 4 GOC
      }
      break;

    case TR_GOTO:
      busy = true;
      goto_worst();
      busy = false;
      state = TR_IDLE;
      probe_state = PR_NONE;             // chua probe lai o goc nay
      if (enter_idle_hook) enter_idle_hook();   // UI: mac dinh chon PROBE (nut dung nhieu nhat)
      refresh();
      break;

    default: break;                      // TR_IDLE: UI tu xu ly nut bam
  }
}

} // namespace tramming_run

// ---------------------------------------------------------------------------
// Diem vao cu cho lcd/menu/menu_motion.cpp ("WORST POINT = MAX"):
// chi doi chien luoc + luu EEPROM (KHONG di chuyen nozzle nhu nut MAX<->MIN trong wizard).
// ---------------------------------------------------------------------------
void tramming_worst_toggle() { tramming_run::worst_toggle(); }

#endif // ASSISTED_TRAMMING_WIZARD
