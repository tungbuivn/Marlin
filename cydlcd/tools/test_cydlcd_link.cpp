/**
 * test_cydlcd_link.cpp -- Kiem thu giao thuc PUSH tren MAY TINH (khong can board).
 *
 *   - Dung DUNG bo tao/giai frame trong src/cydlcd_protocol.h (cung code voi Marlin va man hinh CYD).
 *   - Nap truc tiep src/cydlcd_link.cpp voi mot UART gia (CydlcdSerialStub) => kiem luon
 *     duong nhan + phan tich frame cua man hinh CYD.
 *
 * Chay:  pwsh cydlcd/tools/run_host_test.ps1
 */
#define CYDLCD_HOST_TEST 1

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/cydlcd_link.h"
#include "../src/cydlcd_port.h"

// ---------------------------------------------------------------------------
// UART gia + dong ho gia
// ---------------------------------------------------------------------------
static uint8_t  rxq[1024]; static int rx_head = 0, rx_tail = 0;
static uint8_t  txlog[4096]; static int tx_len = 0;
static uint32_t g_ms = 0;

CydlcdSerialStub CYDLCD_MARLIN_SERIAL;
int    CydlcdSerialStub::available() { return rx_head - rx_tail; }
int    CydlcdSerialStub::read() { return rx_head > rx_tail ? rxq[rx_tail++] : -1; }
size_t CydlcdSerialStub::write(const uint8_t *b, size_t l) { memcpy(txlog + tx_len, b, l); tx_len += (int)l; return l; }
size_t CydlcdSerialStub::write(uint8_t c) { txlog[tx_len++] = c; return 1; }
int    CydlcdSerialStub::availableForWrite() { return 1024; }
uint32_t cydlcd_millis() { return g_ms; }

// Nap truc tiep code cua man hinh CYD de test (sau khi da co stub)
#include "../src/tbt_link.cpp"       // duong TBT (ASCII, Bed Tramming Wizard) dung chung UART voi frame PUSH
#include "../src/cydlcd_link.cpp"

// ---------------------------------------------------------------------------
// Khung test
// ---------------------------------------------------------------------------
static int checks = 0, fails = 0;
static void CHECK(const bool ok, const char *what) {
  checks++;
  if (!ok) { fails++; printf("  [FAIL] %s\n", what); }
  else printf("  [ ok ] %s\n", what);
}

static void feed(const uint8_t *b, const int n) {
  for (int i = 0; i < n; i++) rxq[rx_head++ & 1023] = b[i];
  cydlcd_link_task();
}

// Gui 1 frame "tu Marlin" (dung dung bo tao frame cua Marlin)
static cydlcd_tx_t MT;   // transmitter gia cua Marlin
static void from_marlin(const uint8_t type) {
  const uint8_t total = cydlcd_tx_finish(&MT);
  feed(MT.buf, total);
}

// Giai frame man hinh CYD gui ra (trong txlog): tim frame 'type', copy payload ra ngoai
static bool tx_find_frame(const uint8_t type, uint8_t *out, uint8_t *out_len, const uint8_t maxlen) {
  cydlcd_rx_t rx; cydlcd_rx_init(&rx);
  for (int i = 0; i < tx_len; i++)
    if (cydlcd_rx_byte(&rx, txlog[i]) && rx.type == type) {
      uint8_t n = rx.len < maxlen ? rx.len : maxlen;
      if (out) memcpy(out, rx.payload, n);
      if (out_len) *out_len = n;
      return true;
    }
  return false;
}

static void clear_tx() { tx_len = 0; }

// ---------------------------------------------------------------------------
int main() {
  printf("== Test giao thuc cydlcd_serial (host) ==\n");
  cydlcd_link_begin(250000);
  clear_tx();
  MT.seq = 0;

  // ---------------- 1. HELLO ----------------
  cydlcd_tx_begin(&MT, CYDLCD_F_HELLO);
  cydlcd_tx_u16(&MT, 0x0102);                 // fw_version
  cydlcd_tx_u8(&MT, CYDLCD_PROTO_VERSION);     // proto
  cydlcd_tx_u8(&MT, CYDLCD_HAS_SD | CYDLCD_HAS_MESH | CYDLCD_HAS_BED);
  cydlcd_tx_u8(&MT, 1);                       // n_heaters
  cydlcd_tx_u8(&MT, 2);                       // n_fans
  cydlcd_tx_str(&MT, "Voron300", 24);
  from_marlin(0);
  CHECK(cydlcd.linked, "HELLO: cydlcd.linked = true");
  CHECK(cydlcd.marlin_fw == 0x0102, "HELLO: marlin_fw = 0x0102");
  CHECK(cydlcd.proto == CYDLCD_PROTO_VERSION, "HELLO: proto dung");
  CHECK((cydlcd.hello_flags & CYDLCD_HAS_MESH) != 0, "HELLO: co co HAS_MESH");
  CHECK(strcmp(cydlcd.name, "Voron300") == 0, "HELLO: ten may 'Voron300'");

  // ---------------- 2. STATUS / TEMPS / POSITION ----------------
  cydlcd_tx_begin(&MT, CYDLCD_F_STATUS);
  cydlcd_tx_u8(&MT, CYDLCD_ST_PRINTING); cydlcd_tx_u8(&MT, CYDLCD_SF_HOMED | CYDLCD_SF_SD_PRINTING); cydlcd_tx_u8(&MT, 42);
  from_marlin(0);
  CHECK(cydlcd.state == CYDLCD_ST_PRINTING && cydlcd.progress_pct == 42, "STATUS: state + progress");

  cydlcd_tx_begin(&MT, CYDLCD_F_TEMPS);
  cydlcd_tx_u8(&MT, 2);
  cydlcd_tx_u8(&MT, CYDLCD_TEMP_E0); cydlcd_tx_i16(&MT, 21050); cydlcd_tx_i16(&MT, 21000);
  cydlcd_tx_u8(&MT, CYDLCD_TEMP_BED); cydlcd_tx_i16(&MT, 6012); cydlcd_tx_i16(&MT, 6000);
  from_marlin(0);
  CHECK(cydlcd.n_temps == 2, "TEMPS: 2 muc");
  CHECK(cydlcd.temps[0].id == CYDLCD_TEMP_E0 && cydlcd.temps[0].cur == 21050 && cydlcd.temps[0].tgt == 21000, "TEMPS: E0 210.50/210.00");
  CHECK(cydlcd.temps[1].id == CYDLCD_TEMP_BED && cydlcd.temps[1].cur == 6012, "TEMPS: BED 60.12");
  CHECK(cydlcd_celsius(cydlcd.temps[0].cur) > 210.4f && cydlcd_celsius(cydlcd.temps[0].cur) < 210.6f, "TEMPS: doi ra do C");

  cydlcd_tx_begin(&MT, CYDLCD_F_POSITION);
  cydlcd_tx_i32(&MT, 152300); cydlcd_tx_i32(&MT, -45300); cydlcd_tx_i32(&MT, 10000); cydlcd_tx_i32(&MT, 1200);
  cydlcd_tx_u16(&MT, 0); cydlcd_tx_i16(&MT, 10000); cydlcd_tx_i16(&MT, 9500);
  from_marlin(0);
  CHECK(cydlcd.pos.x == 152300 && cydlcd.pos.y == -45300 && cydlcd.pos.z == 10000, "POSITION: X/Y/Z (co so am)");
  CHECK(cydlcd.pos.feed_pct == 10000 && cydlcd.pos.flow_pct == 9500, "POSITION: feed/flow 100%/95%");
  CHECK(cydlcd_mm(cydlcd.pos.y) < -45.2f && cydlcd_mm(cydlcd.pos.y) > -45.4f, "POSITION: doi ra mm");

  // ---------------- 3. PROGRESS ----------------
  cydlcd_tx_begin(&MT, CYDLCD_F_PROGRESS);
  cydlcd_tx_u8(&MT, 37); cydlcd_tx_u32(&MT, 3600); cydlcd_tx_u32(&MT, 5400);
  cydlcd_tx_str(&MT, "voron_bracket.gcode", 31);
  from_marlin(0);
  CHECK(cydlcd.prog.pct == 37 && cydlcd.prog.elapsed_s == 3600 && cydlcd.prog.remaining_s == 5400, "PROGRESS: % + thoi gian");
  CHECK(strcmp(cydlcd.prog.file, "voron_bracket.gcode") == 0, "PROGRESS: ten file");

  // ---------------- 4. FANS / SETTING / MESSAGE / PROMPT ----------------
  cydlcd_tx_begin(&MT, CYDLCD_F_FANS);
  cydlcd_tx_u8(&MT, 2); cydlcd_tx_u8(&MT, 0); cydlcd_tx_i16(&MT, 7500); cydlcd_tx_u8(&MT, 1); cydlcd_tx_i16(&MT, 0);
  from_marlin(0);
  CHECK(cydlcd.n_fans == 2 && cydlcd.fans[0].idx == 0 && cydlcd.fans[0].pct == 7500, "FANS: fan0 = 75%");

  cydlcd.n_settings = 0;
  cydlcd_tx_begin(&MT, CYDLCD_F_SETTING); cydlcd_tx_u16(&MT, CYDLCD_SET_STEPS_MM_E);  cydlcd_tx_i32(&MT, 415000); from_marlin(0);
  cydlcd_tx_begin(&MT, CYDLCD_F_SETTING); cydlcd_tx_u16(&MT, CYDLCD_SET_PID_E_KP);    cydlcd_tx_i32(&MT, 21500);  from_marlin(0);
  cydlcd_tx_begin(&MT, CYDLCD_F_SETTING); cydlcd_tx_u16(&MT, CYDLCD_SET_STEPS_MM_E);  cydlcd_tx_i32(&MT, 420000); from_marlin(0);
  CHECK(cydlcd.n_settings == 2, "SETTING: trung id thi ghi de (khong them moi)");
  CHECK(cydlcd_setting(CYDLCD_SET_STEPS_MM_E, 0) == 420000, "SETTING: steps/mm E = 420");
  CHECK(cydlcd_setting(CYDLCD_SET_PID_E_KP, 0) == 21500, "SETTING: PID Kp = 21.5");

  cydlcd_tx_begin(&MT, CYDLCD_F_MESSAGE); cydlcd_tx_u8(&MT, CYDLCD_SEV_ERROR); cydlcd_tx_str(&MT, "Het filament", 47); from_marlin(0);
  CHECK(cydlcd.message_sev == CYDLCD_SEV_ERROR && strcmp(cydlcd.message, "Het filament") == 0, "MESSAGE: muc loi + noi dung");

  cydlcd_tx_begin(&MT, CYDLCD_F_PROMPT); cydlcd_tx_u8(&MT, 0); cydlcd_tx_u8(&MT, CYDLCD_PROMPT_USER_CONFIRM); cydlcd_tx_str(&MT, "Tiep tuc?", 47); from_marlin(0);
  CHECK(cydlcd.prompt_active && cydlcd.prompt_type == CYDLCD_PROMPT_USER_CONFIRM, "PROMPT: dang cho tra loi");

  // Tra loi prompt -> phai gui frame ANSWER len Marlin
  clear_tx();
  cydlcd_link_answer(1);
  uint8_t abuf[8]; uint8_t alen = 0;
  CHECK(tx_find_frame(CYDLCD_F_ANSWER, abuf, &alen, sizeof(abuf)) && alen == 2 && abuf[1] == 1, "ANSWER: gui len Marlin (answer=1)");
  CHECK(!cydlcd.prompt_active, "ANSWER: da tat prompt");

  // ---------------- 5. MESH ----------------
  cydlcd.mesh_rows = 0; cydlcd.mesh_valid = false;
  for (uint8_t r = 0; r < 3; r++) {
    cydlcd_tx_begin(&MT, CYDLCD_F_MESH);
    cydlcd_tx_u8(&MT, 3); cydlcd_tx_u8(&MT, r); cydlcd_tx_u8(&MT, 2);
    cydlcd_tx_i16(&MT, (int16_t)(-100 * (r + 1)));
    cydlcd_tx_i16(&MT, (int16_t)(100 * (r + 1)));
    from_marlin(0);
  }
  CHECK(cydlcd.mesh_ny == 3 && cydlcd.mesh_nx == 2 && cydlcd.mesh_rows == 3, "MESH: 3 hang x 2 cot");
  CHECK(cydlcd.mesh[0][0] == -100 && cydlcd.mesh[1][2] == 300, "MESH: gia tri tung diem");

  // ---------------- 6. FILES ----------------
  cydlcd.n_files = 0; cydlcd.files_done = false;
  cydlcd_tx_begin(&MT, CYDLCD_F_FILES); cydlcd_tx_u8(&MT, 0x01); cydlcd_tx_i32(&MT, 0); cydlcd_tx_str(&MT, "PROJECTS", 31); from_marlin(0);
  cydlcd_tx_begin(&MT, CYDLCD_F_FILES); cydlcd_tx_u8(&MT, 0x00); cydlcd_tx_i32(&MT, 0); cydlcd_tx_str(&MT, "cube.gcode", 31); from_marlin(0);
  cydlcd_tx_begin(&MT, CYDLCD_F_FILES); cydlcd_tx_u8(&MT, 0xFF); cydlcd_tx_i32(&MT, 0); cydlcd_tx_str(&MT, "", 31); from_marlin(0);
  CHECK(cydlcd.n_files == 2, "FILES: 2 muc");
  CHECK((cydlcd.files[0].flags & 1) && !(cydlcd.files[1].flags & 1), "FILES: thu muc/file dung co");
  CHECK(strcmp(cydlcd.files[1].name, "cube.gcode") == 0, "FILES: ten file");
  CHECK(cydlcd.files_done && !cydlcd.files_pending, "FILES: da het danh sach");

  // ---------------- 7. Chiu loi: rac, CRC sai, len qua lon ----------------
  {
    const uint16_t before = s_rx.errors;
    const uint8_t junk[] = { 0x00, 0xA5, 0x11, 0x5A, 0xFF };
    feed(junk, sizeof(junk));
    CHECK(cydlcd.state == CYDLCD_ST_PRINTING, "LOI: rac truoc frame khong lam hong trang thai");

    cydlcd_tx_begin(&MT, CYDLCD_F_STATUS);
    cydlcd_tx_u8(&MT, CYDLCD_ST_IDLE); cydlcd_tx_u8(&MT, 0); cydlcd_tx_u8(&MT, 0);
    uint8_t total = cydlcd_tx_finish(&MT);
    MT.buf[total - 1] ^= 0xFF;                       // pha CRC
    feed(MT.buf, total);
    CHECK(cydlcd.state == CYDLCD_ST_PRINTING, "LOI: CRC sai -> bo qua frame");
    CHECK(s_rx.errors > before, "LOI: co dem loi CRC");

    const uint8_t badlen[] = { CYDLCD_SYNC0, CYDLCD_SYNC1, CYDLCD_F_STATUS, 0, 250 };
    feed(badlen, sizeof(badlen));
    CHECK(s_rx.errors > before, "LOI: len qua lon -> dem loi");

    // Sau loi, frame hop le tiep theo van phai nhan duoc (tu dong dong bo lai)
    cydlcd_tx_begin(&MT, CYDLCD_F_STATUS);
    cydlcd_tx_u8(&MT, CYDLCD_ST_PAUSED); cydlcd_tx_u8(&MT, 0); cydlcd_tx_u8(&MT, 55);
    from_marlin(0);
    CHECK(cydlcd.state == CYDLCD_ST_PAUSED && cydlcd.progress_pct == 55, "LOI: frame hop le sau do van nhan duoc");
  }

  // ---------------- 8. man hinh CYD gui lenh ----------------
  clear_tx();
  cydlcd_link_send_gcode("G28");
  cydlcd_link_send_gcode("M104 S210");
  cydlcd_link_request_dir("PROJECTS");
  {
    uint8_t found_gcode = 0, found_dir = 0;
    cydlcd_rx_t rx; cydlcd_rx_init(&rx);
    char txt[64];
    for (int i = 0; i < tx_len; i++)
      if (cydlcd_rx_byte(&rx, txlog[i])) {
        if (rx.type == CYDLCD_F_GCODE) {
          memcpy(txt, rx.payload + 1, rx.payload[0]); txt[rx.payload[0]] = 0;
          if (!strcmp(txt, "G28") || !strcmp(txt, "M104 S210")) found_gcode++;
        }
        if (rx.type == CYDLCD_F_REQUEST && rx.payload[0] == CYDLCD_REQ_FILES_DIR) {
          memcpy(txt, rx.payload + 2, rx.payload[1]); txt[rx.payload[1]] = 0;
          if (!strcmp(txt, "PROJECTS")) found_dir = 1;
        }
      }
    CHECK(found_gcode == 2, "GUI: 2 lenh G-code gui dung");
    CHECK(found_dir == 1, "GUI: REQUEST FILES_DIR + ten thu muc");
  }

  // ---------------- 9. PING/PONG + mat lien ket ----------------
  clear_tx();
  g_ms += 1000;
  cydlcd_link_task();
  {
    uint8_t pbuf[8]; uint8_t plen = 0;
    CHECK(tx_find_frame(CYDLCD_F_PING, pbuf, &plen, sizeof(pbuf)), "PING: tu gui 1s/lan");
  }
  g_ms += CYDLCD_LINK_TIMEOUT_MS + 10;
  cydlcd_link_task();
  CHECK(!cydlcd.linked, "LINK: qua timeout -> mat lien ket");
  {
    cydlcd_tx_begin(&MT, CYDLCD_F_PONG); cydlcd_tx_u8(&MT, 7); from_marlin(0);
    CHECK(cydlcd.linked, "LINK: nhan lai frame -> co lien ket");
  }

  // ---------------- 10. Event ----------------
  clear_tx();
  cydlcd_tx_begin(&MT, CYDLCD_F_EVENT); cydlcd_tx_u8(&MT, CYDLCD_EV_TONE); cydlcd_tx_u16(&MT, 1000); cydlcd_tx_u16(&MT, 200); cydlcd_tx_str(&MT, "", 0); from_marlin(0);
  CHECK(cydlcd.event == CYDLCD_EV_TONE && cydlcd.event_v1 == 1000 && cydlcd.event_v2 == 200, "EVENT: TONE 1000Hz/200ms");

  // ---------------- 11. TBT: dinh dang + doc lai dong trang thai (ASCII) ----------------
  {
    char b[16];
    tbt_fmt_mm(b, 0.0f);      CHECK(!strcmp(b, "0.000"), "TBT fmt: 0 -> '0.000'");
    tbt_fmt_mm(b, 0.15f);     CHECK(!strcmp(b, "0.150"), "TBT fmt: 0.15 -> '0.150'");
    tbt_fmt_mm(b, -1.2346f);  CHECK(!strcmp(b, "-1.235"), "TBT fmt: -1.2346 -> '-1.235' (lam tron am)");
    tbt_fmt_mm(b, -1.2344f);  CHECK(!strcmp(b, "-1.234"), "TBT fmt: -1.2344 -> '-1.234'");
    tbt_fmt_mm(b, -0.005f);   CHECK(!strcmp(b, "-0.005"), "TBT fmt: -0.005 -> '-0.005' (giu dau am)");
    tbt_fmt_mm(b, 12.3456f);  CHECK(!strcmp(b, "12.346"), "TBT fmt: 12.3456 -> '12.346'");

    tbt_status_t st; memset(&st, 0, sizeof(st));
    st.phase = TBT_PH_PROBING; st.mode_max = 1; st.idx = 2; st.ref = 12.345f;
    st.d[0] = 0.021f; st.ok[0] = 1;
    st.d[1] = -0.005f; st.ok[1] = 1; st.accepted[1] = 1;
    st.ok[2] = 0;                                  // goc chua probe -> "---.---"
    st.d[3] = 1.5f;   st.ok[3] = 1;
    st.active = 1; st.all_dat = 0;
    strcpy(st.hint, "CCW:0.021");
    st.t_e0 = 21050; st.t_bed = 6000; st.t_e0_tgt = 21000; st.t_bed_tgt = 6000;   // xem CHECK nhiet do o duoi

    char line[160];
    const int n = tbt_status_format(line, sizeof(line), &st);
    printf("  [info] dong TBT: %s", line);
    CHECK(n > 18 && line[n - 1] == '\n', "TBT format: co '\\n' o cuoi");

    tbt_status_t p; memset(&p, 0, sizeof(p));
    CHECK(tbt_status_parse(line, &p), "TBT parse: doc lai duoc dong vua tao");
    CHECK(p.phase == TBT_PH_PROBING && p.mode_max == 1 && p.idx == 2, "TBT parse: phase/mode/idx");
    CHECK(p.ref > 12.34f && p.ref < 12.35f, "TBT parse: ref (mm)");
    CHECK(p.ok[0] && p.d[0] > 0.020f && p.d[0] < 0.022f, "TBT parse: d0 + ok0");
    CHECK(p.accepted[1] == 1 && p.accepted[0] == 0, "TBT parse: co/khong co DONE");
    CHECK(!p.ok[2] && p.ok[3], "TBT parse: goc chua probe = '---.---'");
    CHECK(p.active == 1 && p.all_dat == 0, "TBT parse: active/all_dat");
    CHECK(strcmp(p.hint, "CCW:0.021") == 0, "TBT parse: hint");

    // 4 token NHIET DO (do C * 100) o CUOI dong: token cuoi dinh lien '\n' -> PHAI doc duoc ca
    // target BAN IN. Day la test hoi quy cho loi 2026-10-15: bo tach token cu (chi coi ' '/'\t'/'\r'
    // la dau phan cach) an nham '\n' vao token cuoi => t_bed_tgt LUON = 0 => man hinh CYD hien
    // "BED <nhiet> / 0 C" du Marlin da dat target 60.
    CHECK(p.t_e0 == 21050 && p.t_bed == 6000 && p.t_e0_tgt == 21000 && p.t_bed_tgt == 6000,
          "TBT parse: 4 token nhiet do (ke ca t_bed_tgt dinh '\\n')");

    // Dong theo dinh dang TOI THIEU (thieu 3 field mo rong) van phai doc duoc
    tbt_status_t m;
    CHECK(tbt_status_parse("TBT 0 1 0 0.000 0.010 -0.020 0.000 0.000 0 0 1 0\n", &m),
          "TBT parse: ban toi thieu (thieu active/all_dat/hint)");
    CHECK(m.active == 0 && m.all_dat == 0 && m.hint[0] == '\0', "TBT parse: field thieu -> mac dinh rong");

    // Dong loi
    CHECK(!tbt_status_parse("TBTX 0 1 0 0 0 0 0 0 0 0 0 0\n", &m), "TBT parse: ten sai -> false");
    CHECK(!tbt_status_parse("HELLO 1 2 3\n", &m), "TBT parse: khong phai TBT -> false");
    CHECK(!tbt_status_parse("TBT 0 1 0\n", &m), "TBT parse: thieu field -> false");

    // Lenh 1 ky tu (phia Marlin dung ban _exact -> byte rac khong the giai thanh lenh)
    CHECK(tbt_cmd_parse_exact("P") == 'P', "TBT cmd: 'P'");
    CHECK(tbt_cmd_parse_exact("d\n") == 'D', "TBT cmd: 'd' -> 'D'");
    CHECK(tbt_cmd_parse_exact("") == 0, "TBT cmd: dong rong -> 0");
    CHECK(tbt_cmd_parse_exact("PX") == 0, "TBT cmd: 'PX' -> 0 (khong nhan lenh rac)");
    CHECK(tbt_cmd_parse_exact("R \n") == 'R', "TBT cmd: 'R ' -> 'R'");
    CHECK(tbt_cmd_parse("Q") == 'Q', "TBT cmd: ban long 'Q' = 'Q'");
  }

  // ---------------- 12. TBT: nhan dong tren CUNG UART voi frame PUSH ----------------
  {
    tbt_link_begin();

    // (a) 'T','B','T' duoc giu tam; lech thi phai tra lai DAY DU cho bo giai frame PUSH
    uint8_t f[4];
    CHECK(tbt_link_feed('T', f) == 0, "TBT feed: 'T' (dau dong) duoc giu tam");
    CHECK(tbt_link_feed('B', f) == 0, "TBT feed: 'B' duoc giu tam");
    CHECK(tbt_link_feed('X', f) == 3 && f[0] == 'T' && f[1] == 'B' && f[2] == 'X',
          "TBT feed: lech -> tra lai day du 'TBX' (khong mat byte)");

    // (b) 1 dong TBT ca dong phai bi tieu thu, trang thai phai cap nhat
    const char *l = "TBT 2 1 3 10.000 0.100 -0.200 0.000 0.000 0 0 0 0 1 0 CCW:0.100\n";
    uint8_t leaked = 0;
    for (const char *q = l; *q; q++) leaked = (uint8_t)(leaked + tbt_link_feed((uint8_t)*q, f));
    CHECK(leaked == 0, "TBT feed: ca dong TBT bi tieu thu (0 byte lot ra)");
    CHECK(tbt.lines == 1 && tbt.ever && tbt.linked, "TBT feed: nhan duoc 1 dong -> linked");
    CHECK(tbt.st.phase == TBT_PH_PROBING && tbt.st.idx == 3, "TBT feed: phase/idx");
    CHECK(tbt.st.ok[0] && !tbt.st.ok[2], "TBT feed: ok[] tung goc");

    // (c) Frame PUSH co payload chua 'T' VA chuoi "TBT" VAN phai nguyen ven
    //     (chuoi nam giua payload, khong o dau dong -> khong bi bo sniff an)
    cydlcd.n_files = 0; cydlcd.files_done = false;
    cydlcd_tx_begin(&MT, CYDLCD_F_FILES);
    cydlcd_tx_u8(&MT, 0x01); cydlcd_tx_i32(&MT, 0);
    cydlcd_tx_str(&MT, "TBT PROJECTS", 31);
    {
      const uint8_t total = cydlcd_tx_finish(&MT);
      feed(MT.buf, total);
    }
    CHECK(cydlcd.n_files == 1 && !strcmp(cydlcd.files[0].name, "TBT PROJECTS"),
          "TBT feed: frame PUSH van nguyen ven ('T'/'TBT' trong payload khong bi an");

    // (d) O che do TBT thi KHONG gui PING nua (tranh byte nhi phan lot vao parser ASCII cua Marlin)
    clear_tx();
    g_ms += 2000;
    cydlcd_link_task();
    {
      uint8_t pbuf[8]; uint8_t plen = 0;
      CHECK(!tx_find_frame(CYDLCD_F_PING, pbuf, &plen, sizeof(pbuf)), "TBT feed: da o che do TBT -> khong PING");
    }
  }

  printf("== %d kiem tra, %d loi ==\n", checks, fails);
  return fails ? 1 : 0;
}
