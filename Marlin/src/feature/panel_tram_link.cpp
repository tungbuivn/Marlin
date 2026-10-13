/**
 * panel_tram_link.cpp -- Link TRAMMING voi man hinh rieng lcd_panel (xem panel_tram_link.h).
 *
 *  - panel -> Marlin: 1 ky tu lenh + '\n'  ('P' probe, 'D' done, 'M' doi MAX/MIN, 'R' home+probe
 *    lai, 'B' thoat, 'Q' hoi trang thai).
 *  - Marlin -> panel: 1 dong ASCII "TRAM ...\n", gui khi trang thai DOI + dinh ky
 *    PANEL_TRAM_PUSH_MS (~5Hz).
 *
 * GOI TU DAU: panel_tram_link::tick() duoc goi trong manage_inactivity() (MarlinCore.cpp), ngay
 * TRUOC ui.update(). Ly do chon cho nay:
 *   - manage_inactivity() duoc goi tu idle(), tuc la chay lien tuc ca khi dang in lan khi cho;
 *     khong nhu loop() (co the bi ket trong 1 lan thuc thi dai) hay ui.update() (chi co khi
 *     MarlinUI duoc bien dich).
 *   - Dat TRUOC ui.update() de khi man 12864 cung dang mo wizard thi buoc vua chay duoc ve ngay.
 *   - tick() KHONG goi idle() -> khong de quy vo han; con khi tick() day 1 buoc co blocking move
 *     (probe/di chuyen) thi idle() long nhau se quay lai tick(), nhung luc do tramming_run::busy
 *     = true nen khong tien buoc lan nua (chi doc/ghi serial - vo hai).
 *
 * AI "TIEN BUOC" (tramming_run::step)?
 *   - Man 12864 dang mo wizard (ui_attached = true): menu_tramming.cpp goi step() -> tick() KHONG goi.
 *   - Man 12864 khong mo wizard (panel dieu khien): tick() goi step().
 *   => luon co dung MOT nguon day trang thai.
 */

#include "../inc/MarlinConfigPre.h"

#if BOTH(PANEL_TRAM_LINK, ASSISTED_TRAMMING_WIZARD)

#include "panel_tram_link.h"
#include "tram_protocol.h"
#include "tramming_run.h"

#include "../MarlinCore.h"          // marlin_state / IsRunning()
#include "../core/millis_t.h"
#include "../core/debug_out.h"

#include <string.h>

// Trang thai gui/di cho tramming_run.cpp
static_assert(G35_PROBE_COUNT == TRAM_PROTO_POINTS, "tram_protocol.h phai khop TRAMMING_POINT_XY (xem Configuration_adv.h).");

namespace panel_tram_link {

#define PANEL_TRAM_SERIAL MSERIAL(1)      // USART1 (PA9 TX / PA10 RX) - xem panel_tram_link.h

static bool       s_ready = false;
static char       s_rxline[16];           // lenh chi 1 ky tu -> 16 byte la du
static uint8_t    s_rxlen = 0;
static char       s_line[PANEL_TRAM_LINE_MAX];
static char       s_last[PANEL_TRAM_LINE_MAX];
static bool       s_have_last = false;
static millis_t   s_next_ms = 0;
static bool       s_force = false;        // co lenh -> gui lai ngay
static uint16_t   s_cmds = 0, s_ignored = 0;

// ---------------------------------------------------------------------------
// CHAN DOAN DAY NOI (2026-10-13) - TAM THOI, bo khi da tim xong loi.
// In ra USB CDC (MYSERIAL1) moi 2s: so BYTE nhan duoc tren USART1 RX (PA10), so dong, so lenh hop le,
// so lenh bi bo, so lan gui. Dung khi panel bao "LINK OK" nhung bam nut khong thay may chay:
//   rx_bytes = 0        -> khong co byte nao toi PA10 (day TX panel / pad RX board)
//   rx_bytes > 0, cmds=0-> co byte nhung sai dinh dang (thua ky tu, thieu '\n', sai baud)
//   cmds > 0            -> Marlin DA nhan lenh (loi nam cho khac: active/busy/queue)
//   tx > 0              -> Marlin CO gui trang thai (neu panel van 'NO LINK' thi day PA9 -> panel RX)
// ---------------------------------------------------------------------------
// MAC DINH TAT. Bat bang -DPANEL_TRAM_DIAG=1 (hoac doi so 0 -> 1 o duoi) khi can chan doan day noi.
#define PANEL_TRAM_DIAG 0
#if PANEL_TRAM_DIAG
  static uint32_t s_diag_rx_bytes = 0, s_diag_lines = 0, s_diag_sent = 0;
  static millis_t s_diag_next = 0;
#endif

// ---------------------------------------------------------------------------
// Trang thai hien tai -> struct dung chung (tram_protocol.h)
// ---------------------------------------------------------------------------

// "hint" = viec can lam o goc dang dung, tinh theo DUNG cac quy tac ma man 12864 dang dung
// (tramming_turn_cw + TRAM_DONE_MM, xem menu_tramming.cpp dong 2) nhung viet ngan gon, khong
// khoang trang de nam trong 1 token. Nho vay panel KHONG phai tu suy lai chieu van vit.
static void build_hint(tram_status_t &st) {
  if (!tramming_run::active)           { strcpy(st.hint, "-"); return; }
  if (tramming_run::all_dat)           { strcpy(st.hint, "DAT"); return; }
  if (tramming_run::state != TR_IDLE)  { strcpy(st.hint, "-"); return; }   // dang chay -> panel hien phase
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
}

static void send_status(const bool force) {
  tram_status_t st;
  build_status(st);

  const int n = tram_status_format(s_line, (int)sizeof(s_line), &st);
  if (n <= 0) return;

  // Gui khi CO DOI, hoac khi duoc yeu cau, hoac dinh ky (de panel biet link con song)
  const bool changed = !s_have_last || strcmp(s_line, s_last) != 0;
  if (!changed && !force) return;

  // Khong cho block vong lap: TX buffer day thi BO QUA va KHONG ghi nho dong nay -> lan tick sau
  // (hoac lan dinh ky sau) se gui lai, panel khong bi mat trang thai.
  if (PANEL_TRAM_SERIAL.availableForWrite() < n) return;
  PANEL_TRAM_SERIAL.write((const uint8_t *)s_line, (size_t)n);
  TERN_(PANEL_TRAM_DIAG, s_diag_sent++);

  memcpy(s_last, s_line, (size_t)n + 1U);
  s_have_last = true;
}

// ---------------------------------------------------------------------------
// Lenh tu panel
// ---------------------------------------------------------------------------
static void handle_line(const char *line) {
  const char cmd = tram_cmd_parse_exact(line);   // chi nhan dong DUNG 1 ky tu lenh
  if (!cmd) { if (line[0]) s_ignored++; return; }
  s_cmds++;

  // Trong luc probe/di chuyen (blocking) thi BO QUA lenh HANH DONG: chung se chen vao giua
  // mot lan probe -> hong trang thai. 'Q' thi van tra loi binh thuong.
  const bool can_act = !tramming_run::busy;

  switch (cmd) {
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
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void init() {
  PANEL_TRAM_SERIAL.begin(PANEL_TRAM_BAUD);
  s_rxlen = 0;
  s_have_last = false;
  s_next_ms = 0;
  s_ready = true;
  DEBUG_ECHOLNPGM("panel_tram_link: USART1 @", PANEL_TRAM_BAUD);
}

void tick() {
  // Cho setup() chay xong truoc khi mo USART1: luc setup() Marlin con init man hinh/EEPROM...
  // (USART1/PA9-PA10 khong dung chung chan voi man 12864, nhung giu thu tu nay cho chac chan
  //  va de khong bao gio dung UART khi may chua san sang.)
  if (!IsRunning()) return;
  if (!s_ready) init();

  // 1) Doc lenh tu panel (moi dong ket thuc bang '\n')
  while (PANEL_TRAM_SERIAL.available()) {
    const char c = (char)PANEL_TRAM_SERIAL.read();
    if (c == '\r') continue;
    if (c == '\n') {
      s_rxline[s_rxlen] = '\0';
      #if PANEL_TRAM_DIAG
        s_diag_lines++;
        DEBUG_ECHOLNPGM("TRAMDIAG RXLINE: [", s_rxline, "]");
      #endif
      handle_line(s_rxline);
      s_rxlen = 0;
      continue;
    }
    if (s_rxlen < sizeof(s_rxline) - 1) {
      s_rxline[s_rxlen++] = c;
      TERN_(PANEL_TRAM_DIAG, s_diag_rx_bytes++);
    }
    else { s_rxlen = 0; s_ignored++; }    // dong qua dai -> bo
  }

  // 2) Neu man 12864 khong mo wizard thi day 1 buoc cua may trang thai
  if (tramming_run::active && !tramming_run::ui_attached) tramming_run::step();

  // 3) Gui trang thai: khi doi + dinh ky ~5Hz
  const millis_t ms = millis();
  const bool due = (s_next_ms == 0) || ELAPSED(ms, s_next_ms);
  send_status(s_force || due);
  if (s_force) s_force = false;
  if (due) s_next_ms = ms + PANEL_TRAM_PUSH_MS;

  #if PANEL_TRAM_DIAG
    if (ELAPSED(ms, s_diag_next)) {
      s_diag_next = ms + 2000;
      DEBUG_ECHOLNPGM("TRAMDIAG rx_bytes=", s_diag_rx_bytes, " lines=", s_diag_lines,
                      " cmds=", s_cmds, " ign=", s_ignored, " tx=", s_diag_sent);
    }
  #endif
}

} // namespace panel_tram_link

#endif // PANEL_TRAM_LINK && ASSISTED_TRAMMING_WIZARD
