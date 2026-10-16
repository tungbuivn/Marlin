/**
 * tbt_link.cpp -- Phia MAN HINH CYD: link TRAMMING (ASCII) voi Marlin - xem tbt_link.h.
 *
 * VI SAO "SNIFF" DONG THAY VI TU DOC UART:
 *   UART nay (CYDLCD_MARLIN_SERIAL) do cydlcd_link.cpp so huu (no giai frame PUSH cua ExtUI
 *   cydlcd_serial). tbt_link_feed() duoc goi cho TUNG byte ngay trong vong doc do, nen 2 duong
 *   khong bao gio tranh nhau doc.
 *
 *   KHONG BAO GIO MAT BYTE CUA FRAME PUSH: khi doc thay 'T' (co the mo dau "TBT") thi cac byte
 *   'T','B','T' duoc GIU LAI tam; neu day khong phai "TBT" thi ham tra ve DAY DU cac byte dang
 *   giu + byte vua doc (toi da 4) de cydlcd_link dua tiep cho bo giai frame PUSH.
 *   Ngoai ra "TBT" chi duoc coi la mo dau dong khi byte truoc do la '\n' (dau dong) -> mot day
 *   "TBT" nam lung chung trong payload cua frame PUSH khong lam hong frame.
 *
 *   Hai che do loai tru nhau: Marlin chi gui frame PUSH khi bat CYDLCD_SERIAL_UI, con link nay
 *   chi chay khi bat CYDLCD_TBT_LINK (xem Marlin/src/feature/cydlcd_tbt_link.h).
 */
#include "tbt_link.h"

#include <string.h>
#include "cydlcd_port.h"
#include "cydlcd_link.h"     // cydlcd_link_send_tbt_cmd() + `cydlcd` (che do PUSH, 2026-10-13)

tbt_link_data_t tbt;          // BAN CONG KHAI (nguoi doc: UI...)
static tbt_link_data_t s_tbt;  // BAN GOC - chi task `link` ghi (xem tbt_publish_locked)

// Trang thai bo "sniff"
enum { TS_IDLE = 0, TS_T, TS_B, TS_T2, TS_LINE };
static uint8_t s_state = TS_IDLE;
static bool    s_bol = true;          // byte truoc do la '\n' (hoac chua doc gi) = dau dong
static char    s_line[TBT_LINK_LINE_MAX];
static uint8_t s_len = 0;

void tbt_link_begin(void) {
  memset(&tbt, 0, sizeof(tbt));
  s_state = TS_IDLE;
  s_bol = true;
  s_len = 0;
}

bool tbt_link_active(void) { return s_tbt.ever; }

// Man Marlin co nhung luc BLOCK vai giay (probe/di chuyen giua cac goc/home Z) - trong luc do cac dong
// "TBT" co the ngung du LINK VAN TOT. Vi vay:
//   1) neu trang thai CUOI cho thay wizard dang BAN (active + phase != IDLE) thi dung timeout DAI hon
//      -> khong bao "NO LINK" sai khi may dang probe/di chuyen (yeu cau nguoi dung 2026-10-13);
//   2) chu dong hoi lai bang 'Q' khi im lang >= 1 s: chieu MAN HINH CYD -> Marlin khong phu thuoc timeout va
//      Marlin se gui lai trang thai ngay o tick ke tiep (cydlcd_tbt_link: TBT_CMD_QUERY -> s_force).
void tbt_link_task(void) {
  if (!s_tbt.ever) { s_tbt.linked = false; }

  const uint32_t now = cydlcd_millis();
  const uint32_t age = now - s_tbt.last_rx_ms;

  // "Con byte toi" = day noi VAN SONG, du byte do co doc duoc thanh dong TBT hay khong.
  // Dung de phan biet 2 kieu hong: (a) khong co byte nao (dut day/sai cong/board tat),
  //                               (b) co byte ma khong thanh dong (sai baud/khung, nhieu, day loi).
  s_tbt.rx_active = (now - s_tbt.last_byte_ms) <= TBT_LINK_RX_ACTIVE_MS;

  if (s_tbt.ever) {
    const bool busy_phase = s_tbt.st.active && s_tbt.st.phase != TBT_PH_IDLE;
    s_tbt.linked = age <= (busy_phase ? TBT_LINK_BUSY_TIMEOUT_MS : TBT_LINK_TIMEOUT_MS);
  }

  // Hoi lai khi im lang - CO GIOI HAN 1 lan / TBT_LINK_QUERY_MS (neu khong se gui 'Q' moi vong loop)
  static uint32_t s_query_ms = 0;
  if (s_tbt.ever && age >= TBT_LINK_QUERY_MS && (now - s_query_ms) >= TBT_LINK_QUERY_MS) {
    s_query_ms = now;
    tbt_link_send(TBT_CMD_QUERY);
  }
}

void tbt_link_send(char cmd) {
  if (!cmd) return;
  // Che do PUSH (CYDLCD_SERIAL_UI): lenh PHAI di trong frame CYDLCD_F_TBT_CMD - ghi ASCII tho vao UART
  // se lam bo giai frame cua Marlin hieu sai (no cho frame nhi phan, khong cho dong lenh).
  if (cydlcd.linked) { cydlcd_link_send_tbt_cmd(cmd); return; }
  CYDLCD_MARLIN_SERIAL.write((uint8_t)cmd);
  CYDLCD_MARLIN_SERIAL.write((uint8_t)'\n');
}

// Xu ly 1 dong da du (dung chung cho ca 2 duong: sniff byte cua CYDLCD_TBT_LINK va frame CYDLCD_F_TBT)
static void tbt_parse_line(const char *line) {
  if (tbt_status_parse(line, &s_tbt.st)) {
    s_tbt.last_rx_ms = cydlcd_millis();
    s_tbt.lines++;
    s_tbt.ever = true;
    s_tbt.linked = true;
  }
  else s_tbt.errors++;
}

// Che do PUSH: cydlcd_link.cpp doc frame CYDLCD_F_TBT roi goi ham nay (khong byte-nao di qua sniffer).
void tbt_link_line(const char *line) {
  if (!line || !*line) return;
  s_tbt.raw_bytes += (uint32_t)strlen(line);
  s_tbt.last_byte_ms = cydlcd_millis();
  tbt_parse_line(line);
}

// Tra ve so byte can dua cho bo giai frame PUSH (ghi vao out[], toi da 4 byte).
// 0 = byte nay (va cac byte dang giu) da thuoc dong "TBT ..." -> khong dua cho ai nua.
uint8_t tbt_link_feed(uint8_t c, uint8_t *out) {
  s_tbt.raw_bytes++;                     // chan doan: dem MOI byte UART nhan duoc (ke ca rac)
  s_tbt.last_byte_ms = cydlcd_millis();   // ... va ghi nho lan cuoi co byte -> s_tbt.rx_active
  switch (s_state) {

    case TS_IDLE:
      if (s_bol && c == 'T') { s_state = TS_T; return 0; }    // co the la mo dau "TBT"
      out[0] = c;
      s_bol = (c == '\n');
      return 1;

    case TS_T:
      if (c == 'B') { s_state = TS_B; return 0; }
      s_state = TS_IDLE;                                      // khong phai "TBT": tra lai CA 'T'
      out[0] = 'T'; out[1] = c;
      s_bol = (c == '\n');
      return 2;

    case TS_B:
      if (c == 'T') { s_state = TS_T2; return 0; }
      s_state = TS_IDLE;
      out[0] = 'T'; out[1] = 'B'; out[2] = c;
      s_bol = (c == '\n');
      return 3;

    case TS_T2:
      // Da du "TBT" (3 chu) -> phan con lai (ke ca dau cach) thuoc dong trang thai.
      s_state = TS_LINE;
      s_len = 3;
      memcpy(s_line, "TBT", 3);
      if (s_len < TBT_LINK_LINE_MAX - 1) s_line[s_len++] = (char)c;
      if (c == '\n') { s_line[s_len] = '\0'; s_state = TS_IDLE; s_len = 0; s_bol = true; tbt_parse_line(s_line); }
      return 0;

    default: {                            // TS_LINE: dang gom 1 dong
      if (s_len < TBT_LINK_LINE_MAX - 1) s_line[s_len++] = (char)c;
      if (c == '\n') {
        s_line[s_len] = '\0';
        s_state = TS_IDLE;
        s_len = 0;
        s_bol = true;                     // dong moi bat dau
        tbt_parse_line(s_line);
      }
      else if (s_len >= TBT_LINK_LINE_MAX - 1) {   // dong qua dai -> bo
        s_state = TS_IDLE;
        s_len = 0;
        s_bol = false;
        s_tbt.errors++;
      }
      return 0;                           // byte nay thuoc dong TBT
    }
  }
}

// CONG BO `s_tbt` -> `tbt` (ban cong khai cho UI doc). CHI duoc goi tu `state_publish()` (cydclcd_link.cpp)
// khi DA giu khoa trang thai -> nho vay UI khong bao gio doc phai ban ghi dang do (xem ke-hoach da luong).
void tbt_publish_locked(void) { memcpy(&tbt, &s_tbt, sizeof(tbt)); }