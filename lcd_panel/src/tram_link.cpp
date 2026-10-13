/**
 * tram_link.cpp -- Phia PANEL: link TRAMMING (ASCII) voi Marlin - xem tram_link.h.
 *
 * VI SAO "SNIFF" DONG THAY VI TU DOC UART:
 *   UART nay (PANEL_MARLIN_SERIAL) do panel_link.cpp so huu (no giai frame PUSH cua ExtUI
 *   panel_serial). tram_link_feed() duoc goi cho TUNG byte ngay trong vong doc do, nen 2 duong
 *   khong bao gio tranh nhau doc.
 *
 *   KHONG BAO GIO MAT BYTE CUA FRAME PUSH: khi doc thay 'T' (co the mo dau "TRAM") thi cac byte
 *   'T','R','A' duoc GIU LAI tam; neu day khong phai "TRAM" thi ham tra ve DAY DU cac byte dang
 *   giu + byte vua doc (toi da 4) de panel_link dua tiep cho bo giai frame PUSH.
 *   Ngoai ra "TRAM" chi duoc coi la mo dau dong khi byte truoc do la '\n' (dau dong) -> mot day
 *   "TRAM" nam lung chung trong payload cua frame PUSH khong lam hong frame.
 *
 *   Hai che do loai tru nhau: Marlin chi gui frame PUSH khi bat PANEL_SERIAL_UI, con link nay
 *   chi chay khi bat PANEL_TRAM_LINK (xem Marlin/src/feature/panel_tram_link.h).
 */
#include "tram_link.h"

#include <string.h>
#include "panel_port.h"

tram_link_data_t tram;

// Trang thai bo "sniff"
enum { TS_IDLE = 0, TS_T, TS_R, TS_A, TS_LINE };
static uint8_t s_state = TS_IDLE;
static bool    s_bol = true;          // byte truoc do la '\n' (hoac chua doc gi) = dau dong
static char    s_line[TRAM_LINK_LINE_MAX];
static uint8_t s_len = 0;

void tram_link_begin(void) {
  memset(&tram, 0, sizeof(tram));
  s_state = TS_IDLE;
  s_bol = true;
  s_len = 0;
}

bool tram_link_active(void) { return tram.ever; }

// Man Marlin co nhung luc BLOCK vai giay (probe/di chuyen giua cac goc/home Z) - trong luc do cac dong
// "TRAM" co the ngung du LINK VAN TOT. Vi vay:
//   1) neu trang thai CUOI cho thay wizard dang BAN (active + phase != IDLE) thi dung timeout DAI hon
//      -> khong bao "NO LINK" sai khi may dang probe/di chuyen (yeu cau nguoi dung 2026-10-13);
//   2) chu dong hoi lai bang 'Q' khi im lang >= 1 s: chieu panel -> Marlin khong phu thuoc timeout va
//      Marlin se gui lai trang thai ngay o tick ke tiep (panel_tram_link: TRAM_CMD_QUERY -> s_force).
void tram_link_task(void) {
  if (!tram.ever) { tram.linked = false; return; }

  const uint32_t now = panel_millis();
  const uint32_t age = now - tram.last_rx_ms;
  const bool busy_phase = tram.st.active && tram.st.phase != TRAM_PH_IDLE;
  tram.linked = age <= (busy_phase ? TRAM_LINK_BUSY_TIMEOUT_MS : TRAM_LINK_TIMEOUT_MS);

  // Hoi lai khi im lang - CO GIOI HAN 1 lan / TRAM_LINK_QUERY_MS (neu khong se gui 'Q' moi vong loop)
  static uint32_t s_query_ms = 0;
  if (age >= TRAM_LINK_QUERY_MS && (now - s_query_ms) >= TRAM_LINK_QUERY_MS) {
    s_query_ms = now;
    tram_link_send(TRAM_CMD_QUERY);
  }
}

void tram_link_send(char cmd) {
  if (!cmd) return;
  PANEL_MARLIN_SERIAL.write((uint8_t)cmd);
  PANEL_MARLIN_SERIAL.write((uint8_t)'\n');
}

// Tra ve so byte can dua cho bo giai frame PUSH (ghi vao out[], toi da 4 byte).
// 0 = byte nay (va cac byte dang giu) da thuoc dong "TRAM ..." -> khong dua cho ai nua.
uint8_t tram_link_feed(uint8_t c, uint8_t *out) {
  tram.raw_bytes++;                     // chan doan: dem MOI byte UART nhan duoc (ke ca rac)
  switch (s_state) {

    case TS_IDLE:
      if (s_bol && c == 'T') { s_state = TS_T; return 0; }    // co the la mo dau "TRAM"
      out[0] = c;
      s_bol = (c == '\n');
      return 1;

    case TS_T:
      if (c == 'R') { s_state = TS_R; return 0; }
      s_state = TS_IDLE;                                      // khong phai "TRAM": tra lai CA 'T'
      out[0] = 'T'; out[1] = c;
      s_bol = (c == '\n');
      return 2;

    case TS_R:
      if (c == 'A') { s_state = TS_A; return 0; }
      s_state = TS_IDLE;
      out[0] = 'T'; out[1] = 'R'; out[2] = c;
      s_bol = (c == '\n');
      return 3;

    case TS_A:
      if (c == 'M') { s_state = TS_LINE; s_len = 4; memcpy(s_line, "TRAM", 4); return 0; }
      s_state = TS_IDLE;
      out[0] = 'T'; out[1] = 'R'; out[2] = 'A'; out[3] = c;
      s_bol = (c == '\n');
      return 4;

    default: {                            // TS_LINE: dang gom 1 dong
      if (s_len < TRAM_LINK_LINE_MAX - 1) s_line[s_len++] = (char)c;
      if (c == '\n') {
        s_line[s_len] = '\0';
        s_state = TS_IDLE;
        s_len = 0;
        s_bol = true;                     // dong moi bat dau
        if (tram_status_parse(s_line, &tram.st)) {
          tram.last_rx_ms = panel_millis();
          tram.lines++;
          tram.ever = true;
          tram.linked = true;
        }
        else tram.errors++;
      }
      else if (s_len >= TRAM_LINK_LINE_MAX - 1) {   // dong qua dai -> bo
        s_state = TS_IDLE;
        s_len = 0;
        s_bol = false;
        tram.errors++;
      }
      return 0;                           // byte nay thuoc dong TRAM
    }
  }
}
