/**
 * test_panel_link.cpp -- Kiem thu giao thuc PUSH tren MAY TINH (khong can board).
 *
 *   - Dung DUNG bo tao/giai frame trong src/panel_protocol.h (cung code voi Marlin va panel).
 *   - Nap truc tiep src/panel_link.cpp voi mot UART gia (PanelSerialStub) => kiem luon
 *     duong nhan + phan tich frame cua panel.
 *
 * Chay:  pwsh lcd_panel/tools/run_host_test.ps1
 */
#define PANEL_HOST_TEST 1

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../src/panel_link.h"
#include "../src/panel_port.h"

// ---------------------------------------------------------------------------
// UART gia + dong ho gia
// ---------------------------------------------------------------------------
static uint8_t  rxq[1024]; static int rx_head = 0, rx_tail = 0;
static uint8_t  txlog[4096]; static int tx_len = 0;
static uint32_t g_ms = 0;

PanelSerialStub PANEL_MARLIN_SERIAL;
int    PanelSerialStub::available() { return rx_head - rx_tail; }
int    PanelSerialStub::read() { return rx_head > rx_tail ? rxq[rx_tail++] : -1; }
size_t PanelSerialStub::write(const uint8_t *b, size_t l) { memcpy(txlog + tx_len, b, l); tx_len += (int)l; return l; }
size_t PanelSerialStub::write(uint8_t c) { txlog[tx_len++] = c; return 1; }
int    PanelSerialStub::availableForWrite() { return 1024; }
uint32_t panel_millis() { return g_ms; }

// Nap truc tiep code cua panel de test (sau khi da co stub)
#include "../src/panel_link.cpp"

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
  panel_link_task();
}

// Gui 1 frame "tu Marlin" (dung dung bo tao frame cua Marlin)
static panel_tx_t MT;   // transmitter gia cua Marlin
static void from_marlin(const uint8_t type) {
  const uint8_t total = panel_tx_finish(&MT);
  feed(MT.buf, total);
}

// Giai frame panel gui ra (trong txlog): tim frame 'type', copy payload ra ngoai
static bool tx_find_frame(const uint8_t type, uint8_t *out, uint8_t *out_len, const uint8_t maxlen) {
  panel_rx_t rx; panel_rx_init(&rx);
  for (int i = 0; i < tx_len; i++)
    if (panel_rx_byte(&rx, txlog[i]) && rx.type == type) {
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
  printf("== Test giao thuc panel_serial (host) ==\n");
  panel_link_begin(250000);
  clear_tx();
  MT.seq = 0;

  // ---------------- 1. HELLO ----------------
  panel_tx_begin(&MT, PANEL_F_HELLO);
  panel_tx_u16(&MT, 0x0102);                 // fw_version
  panel_tx_u8(&MT, PANEL_PROTO_VERSION);     // proto
  panel_tx_u8(&MT, PANEL_HAS_SD | PANEL_HAS_MESH | PANEL_HAS_BED);
  panel_tx_u8(&MT, 1);                       // n_heaters
  panel_tx_u8(&MT, 2);                       // n_fans
  panel_tx_str(&MT, "Voron300", 24);
  from_marlin(0);
  CHECK(panel.linked, "HELLO: panel.linked = true");
  CHECK(panel.marlin_fw == 0x0102, "HELLO: marlin_fw = 0x0102");
  CHECK(panel.proto == PANEL_PROTO_VERSION, "HELLO: proto dung");
  CHECK((panel.hello_flags & PANEL_HAS_MESH) != 0, "HELLO: co co HAS_MESH");
  CHECK(strcmp(panel.name, "Voron300") == 0, "HELLO: ten may 'Voron300'");

  // ---------------- 2. STATUS / TEMPS / POSITION ----------------
  panel_tx_begin(&MT, PANEL_F_STATUS);
  panel_tx_u8(&MT, PANEL_ST_PRINTING); panel_tx_u8(&MT, PANEL_SF_HOMED | PANEL_SF_SD_PRINTING); panel_tx_u8(&MT, 42);
  from_marlin(0);
  CHECK(panel.state == PANEL_ST_PRINTING && panel.progress_pct == 42, "STATUS: state + progress");

  panel_tx_begin(&MT, PANEL_F_TEMPS);
  panel_tx_u8(&MT, 2);
  panel_tx_u8(&MT, PANEL_TEMP_E0); panel_tx_i16(&MT, 21050); panel_tx_i16(&MT, 21000);
  panel_tx_u8(&MT, PANEL_TEMP_BED); panel_tx_i16(&MT, 6012); panel_tx_i16(&MT, 6000);
  from_marlin(0);
  CHECK(panel.n_temps == 2, "TEMPS: 2 muc");
  CHECK(panel.temps[0].id == PANEL_TEMP_E0 && panel.temps[0].cur == 21050 && panel.temps[0].tgt == 21000, "TEMPS: E0 210.50/210.00");
  CHECK(panel.temps[1].id == PANEL_TEMP_BED && panel.temps[1].cur == 6012, "TEMPS: BED 60.12");
  CHECK(panel_celsius(panel.temps[0].cur) > 210.4f && panel_celsius(panel.temps[0].cur) < 210.6f, "TEMPS: doi ra do C");

  panel_tx_begin(&MT, PANEL_F_POSITION);
  panel_tx_i32(&MT, 152300); panel_tx_i32(&MT, -45300); panel_tx_i32(&MT, 10000); panel_tx_i32(&MT, 1200);
  panel_tx_u16(&MT, 0); panel_tx_i16(&MT, 10000); panel_tx_i16(&MT, 9500);
  from_marlin(0);
  CHECK(panel.pos.x == 152300 && panel.pos.y == -45300 && panel.pos.z == 10000, "POSITION: X/Y/Z (co so am)");
  CHECK(panel.pos.feed_pct == 10000 && panel.pos.flow_pct == 9500, "POSITION: feed/flow 100%/95%");
  CHECK(panel_mm(panel.pos.y) < -45.2f && panel_mm(panel.pos.y) > -45.4f, "POSITION: doi ra mm");

  // ---------------- 3. PROGRESS ----------------
  panel_tx_begin(&MT, PANEL_F_PROGRESS);
  panel_tx_u8(&MT, 37); panel_tx_u32(&MT, 3600); panel_tx_u32(&MT, 5400);
  panel_tx_str(&MT, "voron_bracket.gcode", 31);
  from_marlin(0);
  CHECK(panel.prog.pct == 37 && panel.prog.elapsed_s == 3600 && panel.prog.remaining_s == 5400, "PROGRESS: % + thoi gian");
  CHECK(strcmp(panel.prog.file, "voron_bracket.gcode") == 0, "PROGRESS: ten file");

  // ---------------- 4. FANS / SETTING / MESSAGE / PROMPT ----------------
  panel_tx_begin(&MT, PANEL_F_FANS);
  panel_tx_u8(&MT, 2); panel_tx_u8(&MT, 0); panel_tx_i16(&MT, 7500); panel_tx_u8(&MT, 1); panel_tx_i16(&MT, 0);
  from_marlin(0);
  CHECK(panel.n_fans == 2 && panel.fans[0].idx == 0 && panel.fans[0].pct == 7500, "FANS: fan0 = 75%");

  panel.n_settings = 0;
  panel_tx_begin(&MT, PANEL_F_SETTING); panel_tx_u16(&MT, PANEL_SET_STEPS_MM_E);  panel_tx_i32(&MT, 415000); from_marlin(0);
  panel_tx_begin(&MT, PANEL_F_SETTING); panel_tx_u16(&MT, PANEL_SET_PID_E_KP);    panel_tx_i32(&MT, 21500);  from_marlin(0);
  panel_tx_begin(&MT, PANEL_F_SETTING); panel_tx_u16(&MT, PANEL_SET_STEPS_MM_E);  panel_tx_i32(&MT, 420000); from_marlin(0);
  CHECK(panel.n_settings == 2, "SETTING: trung id thi ghi de (khong them moi)");
  CHECK(panel_setting(PANEL_SET_STEPS_MM_E, 0) == 420000, "SETTING: steps/mm E = 420");
  CHECK(panel_setting(PANEL_SET_PID_E_KP, 0) == 21500, "SETTING: PID Kp = 21.5");

  panel_tx_begin(&MT, PANEL_F_MESSAGE); panel_tx_u8(&MT, PANEL_SEV_ERROR); panel_tx_str(&MT, "Het filament", 47); from_marlin(0);
  CHECK(panel.message_sev == PANEL_SEV_ERROR && strcmp(panel.message, "Het filament") == 0, "MESSAGE: muc loi + noi dung");

  panel_tx_begin(&MT, PANEL_F_PROMPT); panel_tx_u8(&MT, 0); panel_tx_u8(&MT, PANEL_PROMPT_USER_CONFIRM); panel_tx_str(&MT, "Tiep tuc?", 47); from_marlin(0);
  CHECK(panel.prompt_active && panel.prompt_type == PANEL_PROMPT_USER_CONFIRM, "PROMPT: dang cho tra loi");

  // Tra loi prompt -> phai gui frame ANSWER len Marlin
  clear_tx();
  panel_link_answer(1);
  uint8_t abuf[8]; uint8_t alen = 0;
  CHECK(tx_find_frame(PANEL_F_ANSWER, abuf, &alen, sizeof(abuf)) && alen == 2 && abuf[1] == 1, "ANSWER: gui len Marlin (answer=1)");
  CHECK(!panel.prompt_active, "ANSWER: da tat prompt");

  // ---------------- 5. MESH ----------------
  panel.mesh_rows = 0; panel.mesh_valid = false;
  for (uint8_t r = 0; r < 3; r++) {
    panel_tx_begin(&MT, PANEL_F_MESH);
    panel_tx_u8(&MT, 3); panel_tx_u8(&MT, r); panel_tx_u8(&MT, 2);
    panel_tx_i16(&MT, (int16_t)(-100 * (r + 1)));
    panel_tx_i16(&MT, (int16_t)(100 * (r + 1)));
    from_marlin(0);
  }
  CHECK(panel.mesh_ny == 3 && panel.mesh_nx == 2 && panel.mesh_rows == 3, "MESH: 3 hang x 2 cot");
  CHECK(panel.mesh[0][0] == -100 && panel.mesh[1][2] == 300, "MESH: gia tri tung diem");

  // ---------------- 6. FILES ----------------
  panel.n_files = 0; panel.files_done = false;
  panel_tx_begin(&MT, PANEL_F_FILES); panel_tx_u8(&MT, 0x01); panel_tx_i32(&MT, 0); panel_tx_str(&MT, "PROJECTS", 31); from_marlin(0);
  panel_tx_begin(&MT, PANEL_F_FILES); panel_tx_u8(&MT, 0x00); panel_tx_i32(&MT, 0); panel_tx_str(&MT, "cube.gcode", 31); from_marlin(0);
  panel_tx_begin(&MT, PANEL_F_FILES); panel_tx_u8(&MT, 0xFF); panel_tx_i32(&MT, 0); panel_tx_str(&MT, "", 31); from_marlin(0);
  CHECK(panel.n_files == 2, "FILES: 2 muc");
  CHECK((panel.files[0].flags & 1) && !(panel.files[1].flags & 1), "FILES: thu muc/file dung co");
  CHECK(strcmp(panel.files[1].name, "cube.gcode") == 0, "FILES: ten file");
  CHECK(panel.files_done && !panel.files_pending, "FILES: da het danh sach");

  // ---------------- 7. Chiu loi: rac, CRC sai, len qua lon ----------------
  {
    const uint16_t before = s_rx.errors;
    const uint8_t junk[] = { 0x00, 0xA5, 0x11, 0x5A, 0xFF };
    feed(junk, sizeof(junk));
    CHECK(panel.state == PANEL_ST_PRINTING, "LOI: rac truoc frame khong lam hong trang thai");

    panel_tx_begin(&MT, PANEL_F_STATUS);
    panel_tx_u8(&MT, PANEL_ST_IDLE); panel_tx_u8(&MT, 0); panel_tx_u8(&MT, 0);
    uint8_t total = panel_tx_finish(&MT);
    MT.buf[total - 1] ^= 0xFF;                       // pha CRC
    feed(MT.buf, total);
    CHECK(panel.state == PANEL_ST_PRINTING, "LOI: CRC sai -> bo qua frame");
    CHECK(s_rx.errors > before, "LOI: co dem loi CRC");

    const uint8_t badlen[] = { PANEL_SYNC0, PANEL_SYNC1, PANEL_F_STATUS, 0, 250 };
    feed(badlen, sizeof(badlen));
    CHECK(s_rx.errors > before, "LOI: len qua lon -> dem loi");

    // Sau loi, frame hop le tiep theo van phai nhan duoc (tu dong dong bo lai)
    panel_tx_begin(&MT, PANEL_F_STATUS);
    panel_tx_u8(&MT, PANEL_ST_PAUSED); panel_tx_u8(&MT, 0); panel_tx_u8(&MT, 55);
    from_marlin(0);
    CHECK(panel.state == PANEL_ST_PAUSED && panel.progress_pct == 55, "LOI: frame hop le sau do van nhan duoc");
  }

  // ---------------- 8. Panel gui lenh ----------------
  clear_tx();
  panel_link_send_gcode("G28");
  panel_link_send_gcode("M104 S210");
  panel_link_request_dir("PROJECTS");
  {
    uint8_t found_gcode = 0, found_dir = 0;
    panel_rx_t rx; panel_rx_init(&rx);
    char txt[64];
    for (int i = 0; i < tx_len; i++)
      if (panel_rx_byte(&rx, txlog[i])) {
        if (rx.type == PANEL_F_GCODE) {
          memcpy(txt, rx.payload + 1, rx.payload[0]); txt[rx.payload[0]] = 0;
          if (!strcmp(txt, "G28") || !strcmp(txt, "M104 S210")) found_gcode++;
        }
        if (rx.type == PANEL_F_REQUEST && rx.payload[0] == PANEL_REQ_FILES_DIR) {
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
  panel_link_task();
  {
    uint8_t pbuf[8]; uint8_t plen = 0;
    CHECK(tx_find_frame(PANEL_F_PING, pbuf, &plen, sizeof(pbuf)), "PING: tu gui 1s/lan");
  }
  g_ms += PANEL_LINK_TIMEOUT_MS + 10;
  panel_link_task();
  CHECK(!panel.linked, "LINK: qua timeout -> mat lien ket");
  {
    panel_tx_begin(&MT, PANEL_F_PONG); panel_tx_u8(&MT, 7); from_marlin(0);
    CHECK(panel.linked, "LINK: nhan lai frame -> co lien ket");
  }

  // ---------------- 10. Event ----------------
  clear_tx();
  panel_tx_begin(&MT, PANEL_F_EVENT); panel_tx_u8(&MT, PANEL_EV_TONE); panel_tx_u16(&MT, 1000); panel_tx_u16(&MT, 200); panel_tx_str(&MT, "", 0); from_marlin(0);
  CHECK(panel.event == PANEL_EV_TONE && panel.event_v1 == 1000 && panel.event_v2 == 200, "EVENT: TONE 1000Hz/200ms");

  printf("== %d kiem tra, %d loi ==\n", checks, fails);
  return fails ? 1 : 0;
}
