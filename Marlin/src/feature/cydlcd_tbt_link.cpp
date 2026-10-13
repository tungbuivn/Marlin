/**
 * cydlcd_tbt_link.cpp -- Link TRAMMING voi man hinh rieng cydlcd (xem cydlcd_tbt_link.h).
 *
 *  - man hinh CYD -> Marlin: 1 ky tu lenh + '\n'  ('P' probe, 'D' done, 'M' doi MAX/MIN, 'R' home+probe
 *    lai, 'B' thoat, 'Q' hoi trang thai).
 *  - Marlin -> man hinh CYD: 1 dong ASCII "TBT ...\n", gui khi trang thai DOI + dinh ky
 *    CYDLCD_TBT_PUSH_MS (~5Hz).
 *
 * GOI TU DAU: cydlcd_tbt_link::tick() duoc goi trong manage_inactivity() (MarlinCore.cpp), ngay
 * TRUOC ui.update(). Ly do chon cho nay:
 *   - manage_inactivity() duoc goi tu idle(), tuc la chay lien tuc ca khi dang in lan khi cho;
 *     khong nhu loop() (co the bi ket trong 1 lan thuc thi dai) hay ui.update() (chi co khi
 *     MarlinUI duoc bien dich).
 *   - Dat TRUOC ui.update() de khi man 12864 cung dang mo wizard thi buoc vua chay duoc ve ngay.
 *   - tick() KHONG goi idle() -> khong de quy vo han; con khi tick() day 1 buoc co blocking move
 *     (probe/di chuyen) thi idle() long nhau se quay lai tick(), nhung luc do tbt_run::busy
 *     = true nen khong tien buoc lan nua (chi doc/ghi serial - vo hai).
 *
 * AI "TIEN BUOC" (tbt_run::step)?
 *   - Man 12864 dang mo wizard (ui_attached = true): menu_tramming.cpp goi step() -> tick() KHONG goi.
 *   - Man 12864 khong mo wizard (man hinh CYD dieu khien): tick() goi step().
 *   => luon co dung MOT nguon day trang thai.
 */

#include "../inc/MarlinConfigPre.h"

#if BOTH(CYDLCD_TBT_LINK, ASSISTED_TRAMMING_WIZARD)

#include "cydlcd_tbt_link.h"
#include "tbt_protocol.h"
#include "tbt_run.h"
#include "tbt_report.h"        // dong "TBT ..." + 6 lenh, dung CHUNG voi kenh PUSH (cydlcd_serial)

#include "../MarlinCore.h"          // marlin_state / IsRunning()
#include "../core/millis_t.h"
#include "../core/debug_out.h"

#include <string.h>

namespace cydlcd_tbt_link {

#define CYDLCD_TBT_SERIAL MSERIAL(1)      // USART1 (PA9 TX / PA10 RX) - xem cydlcd_tbt_link.h

static bool       s_ready = false;
static char       s_rxline[16];           // lenh chi 1 ky tu -> 16 byte la du
static uint8_t    s_rxlen = 0;
static char       s_line[CYDLCD_TBT_LINE_MAX];
static char       s_last[CYDLCD_TBT_LINE_MAX];
static bool       s_have_last = false;
static millis_t   s_next_ms = 0;
static uint16_t   s_cmds = 0, s_ignored = 0;

// ---------------------------------------------------------------------------
// CHAN DOAN DAY NOI (2026-10-13) - TAM THOI, bo khi da tim xong loi.
// In ra USB CDC (MYSERIAL1) moi 2s: so BYTE nhan duoc tren USART1 RX (PA10), so dong, so lenh hop le,
// so lenh bi bo, so lan gui. Dung khi man hinh CYD bao "LINK OK" nhung bam nut khong thay may chay:
//   rx_bytes = 0        -> khong co byte nao toi PA10 (day TX man hinh CYD / pad RX board)
//   rx_bytes > 0, cmds=0-> co byte nhung sai dinh dang (thua ky tu, thieu '\n', sai baud)
//   cmds > 0            -> Marlin DA nhan lenh (loi nam cho khac: active/busy/queue)
//   tx > 0              -> Marlin CO gui trang thai (neu man hinh CYD van 'NO LINK' thi day PA9 -> man hinh CYD RX)
// ---------------------------------------------------------------------------
// MAC DINH TAT. Bat bang -DCYDLCD_TBT_DIAG=1 (hoac doi so 0 -> 1 o duoi) khi can chan doan day noi.
#define CYDLCD_TBT_DIAG 0
#if CYDLCD_TBT_DIAG
  static uint32_t s_diag_rx_bytes = 0, s_diag_lines = 0, s_diag_sent = 0;
  static millis_t s_diag_next = 0;
#endif

// ---------------------------------------------------------------------------
// Gui trang thai (dong "TBT ..." do tbt_report tao - xem tbt_report.h/.cpp)
// ---------------------------------------------------------------------------
static void send_status(const bool force) {
  const int n = tbt_report_line(s_line, (int)sizeof(s_line));
  if (n <= 0) return;

  // Gui khi CO DOI, hoac khi duoc yeu cau, hoac dinh ky (de man hinh CYD biet link con song)
  const bool changed = !s_have_last || strcmp(s_line, s_last) != 0;
  if (!changed && !force) return;

  // KHONG dung `availableForWrite() < n` o day!
  //   TX ring buffer cua core STM32 (HardwareSerial) chi co SERIAL_TX_BUFFER_SIZE = 64 byte.
  //   1 dong "TBT ..." dai ~63 byte khi wizard IDLE nhung ~69..75 byte khi co hint CW/CCW/phase
  //   -> dung `availableForWrite() < n` thi dong dai hon buffer bi BO QUA VINH VIEN (khong bao gio
  //   duoc gui lai). Da gap that 2026-10-13: link chay luc wizard IDLE, CHET ngay khi wizard bat dau
  //   probe/di chuyen (man hinh CYD bao NO LINK) trong khi chieu man hinh CYD -> Marlin van tot (bam nut may van chay).
  //   Ghi tung byte: write() tu cho den khi buffer con cho (ca dong ~3 ms @250000 baud - qua ngan de
  //   anh huong vong lap/watchdog), va KHONG bao gio bi bo qua.
  for (int i = 0; i < n; i++) CYDLCD_TBT_SERIAL.write((uint8_t)s_line[i]);
  TERN_(CYDLCD_TBT_DIAG, s_diag_sent++);

  memcpy(s_last, s_line, (size_t)n + 1U);
  s_have_last = true;
}

// ---------------------------------------------------------------------------
// Lenh tu man hinh CYD (6 lenh + gate busy nam trong tbt_report.cpp, dung chung voi cydlcd_serial)
// ---------------------------------------------------------------------------
static void handle_line(const char *line) {
  const char cmd = tbt_cmd_parse_exact(line);   // chi nhan dong DUNG 1 ky tu lenh
  if (!cmd) { if (line[0]) s_ignored++; return; }
  s_cmds++;
  tbt_report_cmd(cmd);                      // 'Q' luon xu ly; lenh khac bi busy chan
}

// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------
void init() {
  CYDLCD_TBT_SERIAL.begin(CYDLCD_TBT_BAUD);
  s_rxlen = 0;
  s_have_last = false;
  s_next_ms = 0;
  s_ready = true;
  DEBUG_ECHOLNPGM("cydlcd_tbt_link: USART1 @", CYDLCD_TBT_BAUD);
}

void tick() {
  // Cho setup() chay xong truoc khi mo USART1: luc setup() Marlin con init man hinh/EEPROM...
  // (USART1/PA9-PA10 khong dung chung chan voi man 12864, nhung giu thu tu nay cho chac chan
  //  va de khong bao gio dung UART khi may chua san sang.)
  if (!IsRunning()) return;
  if (!s_ready) init();

  // 1) Doc lenh tu man hinh CYD (moi dong ket thuc bang '\n')
  while (CYDLCD_TBT_SERIAL.available()) {
    const char c = (char)CYDLCD_TBT_SERIAL.read();
    if (c == '\r') continue;
    if (c == '\n') {
      s_rxline[s_rxlen] = '\0';
      #if CYDLCD_TBT_DIAG
        s_diag_lines++;
        DEBUG_ECHOLNPGM("TBTDIAG RXLINE: [", s_rxline, "]");
      #endif
      handle_line(s_rxline);
      s_rxlen = 0;
      continue;
    }
    if (s_rxlen < sizeof(s_rxline) - 1) {
      s_rxline[s_rxlen++] = c;
      TERN_(CYDLCD_TBT_DIAG, s_diag_rx_bytes++);
    }
    else { s_rxlen = 0; s_ignored++; }    // dong qua dai -> bo
  }

  // 2) Neu man 12864 khong mo wizard thi day 1 buoc cua may trang thai
  if (tbt_run::active && !tbt_run::ui_attached) tbt_run::step();

  // 3) Gui trang thai: khi doi + dinh ky ~5Hz
  const millis_t ms = millis();
  const bool due = (s_next_ms == 0) || ELAPSED(ms, s_next_ms);
  send_status(tbt_report_force_take() || due);
  if (due) s_next_ms = ms + CYDLCD_TBT_PUSH_MS;

  #if CYDLCD_TBT_DIAG
    if (ELAPSED(ms, s_diag_next)) {
      s_diag_next = ms + 2000;
      DEBUG_ECHOLNPGM("TBTDIAG rx_bytes=", s_diag_rx_bytes, " lines=", s_diag_lines,
                      " cmds=", s_cmds, " ign=", s_ignored, " tx=", s_diag_sent);
    }
  #endif
}

} // namespace cydlcd_tbt_link

#endif // CYDLCD_TBT_LINK && ASSISTED_TRAMMING_WIZARD
