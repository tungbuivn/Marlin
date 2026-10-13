/**
 * tramming_report.cpp -- xem tramming_report.h.
 *
 * Toan bo logic duoc CHUYEN tu cydlcd_tram_link.cpp (ban 2026-10-13), KHONG doi hanh vi:
 *   - build_hint()  : viec can lam o goc dang dung (theo dung quy tac cua man 12864);
 *   - build_status(): trang thai hien tai -> tram_status_t (goc/nhiet do);
 *   - handle_line() : 6 lenh 'P','D','M','R','B','Q' + gate busy.
 */

#include "../inc/MarlinConfigPre.h"

#if ENABLED(ASSISTED_TRAMMING_WIZARD) && EITHER(CYDLCD_SERIAL_UI, CYDLCD_TRAM_LINK)

#include "tramming_report.h"
#include "tram_protocol.h"
#include "tramming_run.h"

#include "../module/temperature.h"  // thermalManager (nhiet do gui kem trong dong TRAM)

#include <string.h>

// Trang thai gui/di cho tramming_run.cpp
static_assert(G35_PROBE_COUNT == TRAM_PROTO_POINTS, "tram_protocol.h phai khop TRAMMING_POINT_XY (xem Configuration_adv.h).");

// Co "co lenh -> gui lai ngay" (tuong duong s_force cu trong cydlcd_tram_link.cpp)
static bool s_force = false;

// ---------------------------------------------------------------------------
// Trang thai hien tai -> struct dung chung (tram_protocol.h)
// ---------------------------------------------------------------------------

// "hint" = viec can lam o goc dang dung, tinh theo DUNG cac quy tac ma man 12864 dang dung
// (tramming_turn_cw + TRAM_DONE_MM, xem menu_tramming.cpp dong 2) nhung viet ngan gon, khong
// khoang trang de nam trong 1 token. Nho vay man hinh CYD KHONG phai tu suy lai chieu van vit.
static void build_hint(tram_status_t &st) {
  if (!tramming_run::active)           { strcpy(st.hint, "-"); return; }
  if (tramming_run::all_dat)           { strcpy(st.hint, "DAT"); return; }
  if (tramming_run::state != TR_IDLE)  { strcpy(st.hint, "-"); return; }   // dang chay -> cydlcd hien phase
  if (tramming_run::probe_state == PR_FAILED) { strcpy(st.hint, "ERR"); return; }

  const uint8_t i = tramming_run::worst_index;
  if (!tramming_run::ok(i))            { strcpy(st.hint, "PROBE"); return; }

  const float delta = tramming_run::delta(i) - tramming_run::display_ref();
  if (ABS(delta) <= TRAM_DONE_MM)      { strcpy(st.hint, "DAT"); return; }

  char num[16];
  if (ABS(delta) < 0.02f)              strcpy(num, "nhe");   // 12864: "van nhe"
  else                                 tram_fmt_mm(num, ABS(delta));
  // "%.9s": so dai nhat cua tram_fmt_mm la "-9999.999" (9 ky tu) -> khong bao gio tran st.hint[16]
  snprintf(st.hint, sizeof(st.hint), "%s:%.9s",
           tramming_turn_cw(delta, TRAMMING_SCREW_THREAD) ? "CW" : "CCW", num);
}

static void build_status(tram_status_t &st) {
  st.phase    = tramming_run::state;
  st.mode_max = tramming_worst_max ? 1 : 0;
  st.idx      = tramming_run::worst_index;
  st.active   = tramming_run::active ? 1 : 0;
  st.all_dat  = tramming_run::all_dat ? 1 : 0;
  st.ref      = tramming_run::display_ref();
  LOOP_L_N(i, TRAM_PROTO_POINTS) {
    st.d[i]        = tramming_run::delta(i);
    st.ok[i]       = tramming_run::ok(i) ? 1 : 0;
    st.accepted[i] = tramming_run::accepted(i) ? 1 : 0;
  }
  build_hint(st);

  // Nhiet do (do C * 100) - dong TRAM phai gui kem, neu khong man man hinh CYD khong hien duoc nhiet do
  // nao (che do TRAM khong co frame TEMPS rieng). Kep >= 0 vi giao thuc chi nhan so KHONG dau.
  st.t_e0      = (int16_t)MAX(0.0f, thermalManager.degHotend(0) * 100.0f);
  st.t_bed     = (int16_t)MAX(0.0f, thermalManager.degBed() * 100.0f);
  st.t_e0_tgt  = (int16_t)MAX(0.0f, thermalManager.degTargetHotend(0) * 100.0f);
  st.t_bed_tgt = (int16_t)MAX(0.0f, thermalManager.degTargetBed() * 100.0f);
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
int tramming_report_line(char *buf, int len) {
  if (!buf || len < 8) return 0;
  tram_status_t st;
  build_status(st);
  return tram_status_format(buf, len, &st);
}

bool tramming_report_cmd(const char cmd) {
  // Dung lai parser cua tram_protocol.h: chi nhan DUNG 1 ky tu lenh (nhan ca chu thuong).
  const char line[2] = { cmd, '\0' };
  const char c = tram_cmd_parse_exact(line);
  if (!c) return false;

  // Trong luc probe/di chuyen (blocking) thi BO QUA lenh HANH DONG: chung se chen vao giua
  // mot lan probe -> hong trang thai. 'Q' thi van tra loi binh thuong.
  const bool can_act = !tramming_run::busy;

  switch (c) {
    case TRAM_CMD_QUERY:
      break;

    case TRAM_CMD_RESTART:
      if (can_act) {
        if (tramming_run::active) tramming_run::restart();
        else                      tramming_run::begin();   // chua chay -> bat dau wizard (home tat ca truc)
      }
      break;

    case TRAM_CMD_BACK:
      if (can_act && tramming_run::active) tramming_run::exit();
      break;

    case TRAM_CMD_PROBE:
      if (can_act) tramming_run::probe_here();              // tu kiem tra active
      break;

    case TRAM_CMD_DONE:
      if (can_act) tramming_run::done();
      break;

    case TRAM_CMD_MINMAX:
      if (can_act) tramming_run::toggle_minmax();
      break;

    default:
      break;
  }

  s_force = true;                                          // co lenh -> tra loi ngay
  return true;
}

bool tramming_report_force_take() {
  const bool f = s_force;
  s_force = false;
  return f;
}

#endif // ASSISTED_TRAMMING_WIZARD && (CYDLCD_SERIAL_UI || CYDLCD_TRAM_LINK)
