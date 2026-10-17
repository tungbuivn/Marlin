/**
 * tbt_report.cpp -- xem tbt_report.h.
 *
 * Toan bo logic duoc CHUYEN tu cydlcd_tbt_link.cpp (ban 2026-10-13), KHONG doi hanh vi:
 *   - build_hint()  : viec can lam o goc dang dung (theo dung quy tac cua man 12864);
 *   - build_status(): trang thai hien tai -> tbt_status_t (goc/nhiet do);
 *   - handle_line() : 6 lenh 'P','D','M','R','B','Q' + gate busy.
 */

#include "../inc/MarlinConfigPre.h"

#if ENABLED(ASSISTED_TRAMMING_WIZARD) && EITHER(CYDLCD_SERIAL_UI, CYDLCD_TBT_LINK)

#include "tbt_report.h"
#include "tbt_protocol.h"
#include "tbt_run.h"

#include "../module/temperature.h"  // thermalManager (nhiet do gui kem trong dong TBT)

#include <string.h>

// Trang thai gui/di cho tbt_run.cpp
static_assert(G35_PROBE_COUNT == TBT_PROTO_POINTS, "tbt_protocol.h phai khop TRAMMING_POINT_XY (xem Configuration_adv.h).");

// Co "co lenh -> gui lai ngay" (tuong duong s_force cu trong cydlcd_tbt_link.cpp)
static bool s_force = false;

// ---------------------------------------------------------------------------
// Trang thai hien tai -> struct dung chung (tbt_protocol.h)
// ---------------------------------------------------------------------------

// "hint" = viec can lam o goc dang dung, tinh theo DUNG cac quy tac ma man 12864 dang dung
// (tramming_turn_cw + TBT_DONE_MM, xem menu_tramming.cpp dong 2) nhung viet ngan gon, khong
// khoang trang de nam trong 1 token. Nho vay man hinh CYD KHONG phai tu suy lai chieu van vit.
static void build_hint(tbt_status_t &st) {
  if (!tbt_run::active)           { strcpy(st.hint, "-"); return; }
  if (tbt_run::all_dat)           { strcpy(st.hint, "DAT"); return; }
  if (tbt_run::state != TR_IDLE)  { strcpy(st.hint, "-"); return; }   // dang chay -> cydlcd hien phase
  if (tbt_run::probe_state == PR_FAILED) { strcpy(st.hint, "ERR"); return; }

  const uint8_t i = tbt_run::worst_index;
  if (!tbt_run::ok(i))            { strcpy(st.hint, "PROBE"); return; }

  // 2026-10-17: PHAI tru `ref()` (moc theo che do: MAX->Z-MAX, MIN->Z-MIN), KHONG phai
  // `display_ref()` (moc hien thi = z-home). Truoc day dung display_ref() nen khi display_ref()
  // doi thanh z-home thi `delta` bien thanh r_i (khoang cach toi tam ban) -> so mm VA chieu van sai.
  const float delta = tbt_run::delta(i) - tbt_run::ref();
  if (ABS(delta) <= TBT_DONE_MM)      { strcpy(st.hint, "DAT"); return; }

  char num[16];
  if (ABS(delta) < 0.02f)              strcpy(num, "nhe");   // 12864: "van nhe"
  else                                 tbt_fmt_mm(num, ABS(delta));
  // "%.9s": so dai nhat cua tbt_fmt_mm la "-9999.999" (9 ky tu) -> khong bao gio tran st.hint[16]
  snprintf(st.hint, sizeof(st.hint), "%s:%.9s",
           tramming_turn_cw(delta, TRAMMING_SCREW_THREAD) ? "CW" : "CCW", num);
}

static void build_status(tbt_status_t &st) {
  st.phase    = tbt_run::state;
  st.mode_max = tramming_worst_max ? 1 : 0;
  st.idx      = tbt_run::worst_index;
  st.active   = tbt_run::active ? 1 : 0;
  st.all_dat  = tbt_run::all_dat ? 1 : 0;
  st.auto_on  = tbt_run::auto_move ? 1 : 0;
  st.ref      = tbt_run::display_ref();
  LOOP_L_N(i, TBT_PROTO_POINTS) {
    st.d[i]        = tbt_run::delta(i);
    st.ok[i]       = tbt_run::ok(i) ? 1 : 0;
    st.accepted[i] = tbt_run::accepted(i) ? 1 : 0;
  }
  build_hint(st);

  // Nhiet do (do C * 100) - dong TBT phai gui kem, neu khong man man hinh CYD khong hien duoc nhiet do
  // nao (che do TRAM khong co frame TEMPS rieng). Kep >= 0 vi giao thuc chi nhan so KHONG dau.
  st.t_e0      = (int16_t)MAX(0.0f, thermalManager.degHotend(0) * 100.0f);
  st.t_bed     = (int16_t)MAX(0.0f, thermalManager.degBed() * 100.0f);
  st.t_e0_tgt  = (int16_t)MAX(0.0f, thermalManager.degTargetHotend(0) * 100.0f);
  st.t_bed_tgt = (int16_t)MAX(0.0f, thermalManager.degTargetBed() * 100.0f);
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
int tbt_report_line(char *buf, int len) {
  if (!buf || len < 8) return 0;
  tbt_status_t st;
  build_status(st);
  return tbt_status_format(buf, len, &st);
}

bool tbt_report_cmd(const char cmd) {
  // Dung lai parser cua tbt_protocol.h: chi nhan DUNG 1 ky tu lenh (nhan ca chu thuong).
  const char line[2] = { cmd, '\0' };
  const char c = tbt_cmd_parse_exact(line);
  if (!c) return false;

  // Trong luc probe/di chuyen (blocking) thi BO QUA lenh HANH DONG: chung se chen vao giua
  // mot lan probe -> hong trang thai. 'Q' thi van tra loi binh thuong.
  const bool can_act = !tbt_run::busy;

  switch (c) {
    case TBT_CMD_QUERY:
      break;

    case TBT_CMD_RESTART:
      if (can_act) {
        if (tbt_run::active) tbt_run::restart();
        else                      tbt_run::begin();   // chua chay -> bat dau wizard (home tat ca truc)
      }
      break;

    case TBT_CMD_BACK:
      if (can_act && tbt_run::active) tbt_run::exit();
      break;

    case TBT_CMD_PROBE:
      if (can_act) tbt_run::probe_here();
      break;
    // 2026-10-17: bam nhan goc tren man hinh CYD -> di chuyen toi goc do
    case TBT_CMD_GOTO0: case TBT_CMD_GOTO1: case TBT_CMD_GOTO2: case TBT_CMD_GOTO3:
      if (can_act) tbt_run::goto_corner((uint8_t)(c - TBT_CMD_GOTO0));
      break;

    case TBT_CMD_AUTO:              // 2026-10-17: bat/tat tu dong di chuyen
      if (can_act) tbt_run::toggle_auto();
      break;

    case TBT_CMD_DONE:
      if (can_act) tbt_run::done();
      break;

    case TBT_CMD_MINMAX:
      if (can_act) tbt_run::toggle_minmax();
      break;

    default:
      break;
  }

  s_force = true;                                          // co lenh -> tra loi ngay
  return true;
}

bool tbt_report_force_take() {
  const bool f = s_force;
  s_force = false;
  return f;
}

#endif // ASSISTED_TRAMMING_WIZARD && (CYDLCD_SERIAL_UI || CYDLCD_TBT_LINK)
