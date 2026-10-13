/**
 * panel_link.cpp -- Phia PANEL: nhan frame push tu Marlin, gui G-code len (xem panel_link.h).
 */
#include "panel_link.h"

#include <string.h>
#include "panel_port.h"
#include "tram_link.h"        // duong TRAMMING (ASCII) dung CHUNG UART - xem tram_link.h

panel_data_t panel;

static panel_rx_t  s_rx;
static panel_tx_t  s_tx;
static uint32_t    s_next_ping_ms = 0;
static bool        s_want_full = true;         // xin FULL ngay khi lien ket duoc

// ---------------------------------------------------------------------------
// Gui frame (frame cua panel rat ngan: PING 7 byte, GCODE/RQUEST < 40 byte)
// ---------------------------------------------------------------------------
static void send_frame(const uint8_t total) {
  if (total) PANEL_MARLIN_SERIAL.write(s_tx.buf, total);
}

void panel_link_ping() {
  panel_tx_begin(&s_tx, PANEL_F_PING);
  panel_tx_u8(&s_tx, ++s_tx.seq);
  send_frame(panel_tx_finish(&s_tx));
  
}

void panel_link_send_gcode(const char *line) {
  if (!line || !*line) return;
  panel_tx_begin(&s_tx, PANEL_F_GCODE);
  panel_tx_str(&s_tx, line, PANEL_MAX_PAYLOAD - 1);
  send_frame(panel_tx_finish(&s_tx));
}

void panel_link_send_gcode2(const char *a, const char *b) {
  panel_link_send_gcode(a);
  if (b && *b) panel_link_send_gcode(b);
}

void panel_link_request(const uint8_t what) {
  panel_tx_begin(&s_tx, PANEL_F_REQUEST);
  panel_tx_u8(&s_tx, what);
  send_frame(panel_tx_finish(&s_tx));
}

void panel_link_request_dir(const char *dirname) {
  panel_tx_begin(&s_tx, PANEL_F_REQUEST);
  panel_tx_u8(&s_tx, (uint8_t)PANEL_REQ_FILES_DIR);
  panel_tx_str(&s_tx, dirname, PANEL_FILE_NAME_LEN);
  send_frame(panel_tx_finish(&s_tx));
}

void panel_link_answer(const uint8_t answer) {
  if (!panel.prompt_active) return;
  panel_tx_begin(&s_tx, PANEL_F_ANSWER);
  panel_tx_u8(&s_tx, panel.prompt_id);
  panel_tx_u8(&s_tx, answer);
  send_frame(panel_tx_finish(&s_tx));
  panel.prompt_active = false;
}

void panel_link_send_tram_cmd(char cmd) {
  if (!cmd) return;
  panel_tx_begin(&s_tx, PANEL_F_TRAM_CMD);
  panel_tx_u8(&s_tx, (uint8_t)cmd);
  send_frame(panel_tx_finish(&s_tx));
}

int32_t panel_setting(const uint16_t id, const int32_t def) {
  for (uint8_t i = 0; i < panel.n_settings; i++)
    if (panel.settings[i].id == id) return panel.settings[i].value;
  return def;
}

// ---------------------------------------------------------------------------
// Nhan frame
// ---------------------------------------------------------------------------
// Doc chuoi trong payload: payload[off] = do dai
static uint8_t str_at(const uint8_t *p, const uint8_t len, const uint8_t off, char *dst, const uint8_t dstlen) {
  if (off >= len) { dst[0] = '\0'; return 0; }
  uint8_t n = p[off];
  if (n > (uint8_t)(len - off - 1)) n = (uint8_t)(len - off - 1);
  if (n > (uint8_t)(dstlen - 1)) n = (uint8_t)(dstlen - 1);
  memcpy(dst, &p[off + 1], n);
  dst[n] = '\0';
  return (uint8_t)(n + 1);
}

static void handle_frame() {
  const uint8_t *p = s_rx.payload;
  const uint8_t  n = s_rx.len;
  panel.last_rx_ms = panel_millis();
  panel.linked = true;

  switch ((panel_frame_t)s_rx.type) {

    case PANEL_F_HELLO:
      if (n >= 6) {
        panel.marlin_fw   = (uint16_t)(p[0] | (p[1] << 8));
        panel.proto       = p[2];
        panel.hello_flags = p[3];
        panel.n_heaters   = p[4];
        panel.n_fans_max  = p[5];
        str_at(p, n, 6, panel.name, sizeof(panel.name));
      }
      // Sau HELLO xin ngay du lieu day du (ten file, thong so, mesh)
      panel.files_pending = true;
      panel_link_request(PANEL_REQ_SETTINGS);
      if (panel.hello_flags & PANEL_HAS_MESH) panel_link_request(PANEL_REQ_MESH);
      break;

    case PANEL_F_STATUS:
      if (n >= 3) { panel.state = p[0]; panel.flags = p[1]; panel.progress_pct = p[2]; }
      break;

    case PANEL_F_TEMPS: {
      panel.n_temps = 0;
      if (n >= 1) {
        uint8_t cnt = p[0], off = 1;
        for (uint8_t i = 0; i < cnt && off + 4 < n && panel.n_temps < 8; i++, off += 5) {
          panel.temps[panel.n_temps].id  = p[off];
          panel.temps[panel.n_temps].cur = (int16_t)(p[off + 1] | (p[off + 2] << 8));
          panel.temps[panel.n_temps].tgt = (int16_t)(p[off + 3] | (p[off + 4] << 8));
          panel.n_temps++;
        }
      }
      break;
    }

    case PANEL_F_POSITION:
      if (n >= 22) {
        panel.pos.x = panel_i32(&p[0]);
        panel.pos.y = panel_i32(&p[4]);
        panel.pos.z = panel_i32(&p[8]);
        panel.pos.e = panel_i32(&p[12]);
        // p[16..17] = feed mm/s*10 (chua dung), roi moi den 2 phan tram
        panel.pos.feed_pct = panel_i16(&p[18]);
        panel.pos.flow_pct = panel_i16(&p[20]);
      }
      break;

    case PANEL_F_PROGRESS:
      if (n >= 9) {
        panel.prog.pct         = p[0];
        panel.prog.elapsed_s   = panel_u32(&p[1]);
        panel.prog.remaining_s = panel_u32(&p[5]);
        str_at(p, n, 9, panel.prog.file, sizeof(panel.prog.file));
      }
      break;

    case PANEL_F_TRAM: {
      // 1 dong ASCII "TRAM ..." (che do PUSH, 2026-10-13) -> dua thang vao bo giai cua tram_link,
      // KHONG qua bo "sniff" byte (bo do chi dung cho duong UART tho cua PANEL_TRAM_LINK).
      char t[TRAM_LINK_LINE_MAX];
      str_at(p, n, 0, t, sizeof(t));
      tram_link_line(t);
      break;
    }

    case PANEL_F_MESSAGE:
      if (n >= 1) {
        panel.message_sev = p[0];
        str_at(p, n, 1, panel.message, sizeof(panel.message));
        panel.message_ms = panel_millis();
      }
      break;

    case PANEL_F_FANS: {
      panel.n_fans = 0;
      if (n >= 1) {
        const uint8_t cnt = p[0];
        uint8_t off = 1;
        for (uint8_t i = 0; i < cnt && off + 2 < n && panel.n_fans < 4; i++, off += 3) {
          panel.fans[panel.n_fans].idx = p[off];
          panel.fans[panel.n_fans].pct = (int16_t)(p[off + 1] | (p[off + 2] << 8));
          panel.n_fans++;
        }
      }
      break;
    }

    case PANEL_F_SETTING:
      if (n >= 6) {
        const uint16_t id = (uint16_t)(p[0] | (p[1] << 8));
        const int32_t  v  = panel_i32(&p[2]);
        bool found = false;
        for (uint8_t i = 0; i < panel.n_settings; i++)
          if (panel.settings[i].id == id) { panel.settings[i].value = v; found = true; break; }
        if (!found && panel.n_settings < PANEL_SETTINGS_MAX) {
          panel.settings[panel.n_settings].id = id;
          panel.settings[panel.n_settings].value = v;
          panel.n_settings++;
        }
      }
      break;

    case PANEL_F_MESH:
      if (n >= 3) {
        panel.mesh_ny = p[0];
        const uint8_t row = p[1];
        panel.mesh_nx = p[2];
        if (row < PANEL_MESH_MAX && panel.mesh_nx <= PANEL_MESH_MAX) {
          const uint8_t cnt = (uint8_t)((n - 3) / 2);
          for (uint8_t c = 0; c < cnt && c < panel.mesh_nx; c++)
            panel.mesh[c][row] = panel_i16(&p[3 + c * 2]);
          panel.mesh_rows++;
          panel.mesh_valid = true;
        }
      }
      break;

    case PANEL_F_FILES:
      if (n >= 5) {
        if (p[0] == 0xFF) { panel.files_done = true; panel.files_pending = false; }
        else {
          if (panel.files_done) { panel.files_done = false; panel.n_files = 0; }   // bat dau danh sach moi
          if (panel.n_files < PANEL_MAX_FILES) {
            panel.files[panel.n_files].flags = p[0];
            str_at(p, n, 5, panel.files[panel.n_files].name, PANEL_FILE_NAME_LEN);
            panel.n_files++;
          }
        }
      }
      break;

    case PANEL_F_EVENT:
      if (n >= 5) {
        panel.event    = p[0];
        panel.event_v1 = (uint16_t)(p[1] | (p[2] << 8));
        panel.event_v2 = (uint16_t)(p[3] | (p[4] << 8));
        panel.event_ms = panel_millis();
        // Su kien lam thay doi du lieu -> xin lai cho chac
        switch ((panel_event_t)panel.event) {
          case PANEL_EV_PRINT_START:
          case PANEL_EV_PRINT_STOP:
          case PANEL_EV_MEDIA_INSERTED:
          case PANEL_EV_MEDIA_REMOVED:
            panel.files_done = false; panel.files_pending = true;
            break;
          case PANEL_EV_LEVELING_DONE:
            panel.mesh_rows = 0;
            panel_link_request(PANEL_REQ_MESH);
            break;
          case PANEL_EV_SETTINGS_LOADED:
          case PANEL_EV_FACTORY_RESET:
            panel.n_settings = 0;
            panel_link_request(PANEL_REQ_SETTINGS);
            panel_link_request(PANEL_REQ_MESH);
            break;
          default: break;
        }
      }
      break;

    case PANEL_F_PROMPT:
      if (n >= 2) {
        panel.prompt_id   = p[0];
        panel.prompt_type = p[1];
        str_at(p, n, 2, panel.prompt_text, sizeof(panel.prompt_text));
        panel.prompt_active = true;
      }
      break;

    case PANEL_F_PONG:
      break;                                   // da nhan duoc gi -> linked = true

    default:
      break;
  }
  panel.rx_frames = s_rx.frames;
  panel.rx_errors = s_rx.errors;
}

// ---------------------------------------------------------------------------
// API chung
// ---------------------------------------------------------------------------
void panel_link_begin(const uint32_t baud) {
  PANEL_LINK_BEGIN(baud);
  panel_rx_init(&s_rx);
  memset(&panel, 0, sizeof(panel));
  tram_link_begin();                 // duong TRAMMING: reset trang thai (khong mo lai UART)
  s_want_full = true;
  s_next_ping_ms = 0;
}

bool panel_link_connected() { return panel.linked; }

void panel_link_task() {
  uint8_t fwd[4];
  while (PANEL_MARLIN_SERIAL.available()) {
    const uint8_t c = (uint8_t)PANEL_MARLIN_SERIAL.read();
    // Duong TRAMMING (Marlin: PANEL_TRAM_LINK) dung CHUNG UART nay: tram_link_feed() tra ve 0
    // khi byte thuoc dong "TRAM ...", nguoc lai tra ve so byte (0..4) can dua cho bo giai frame
    // PUSH - cac byte 'T','R','A' dang giu de do duoc tra lai day du nen KHONG mat byte nao.
    //
    // 2026-10-13: che do PUSH (PANEL_SERIAL_UI) gui dong TRAM trong frame PANEL_F_TRAM -> payload
    // CUA FRAME chua chinh chuoi "TRAM ...". Neu van chay bo sniff thi no se "an" cac byte do lam
    // frame hong (da gap that: byte tang ~500 B/s nhung panel.linked=0, rx_frames=0).
    // => Da nhan duoc frame PUSH hop le dau tien (panel.rx_frames > 0) thi TAT sniff VINH VIEN.
    uint8_t n;
    if (panel.rx_frames) { fwd[0] = c; n = 1; }
    else                 n = tram_link_feed(c, fwd);
    for (uint8_t k = 0; k < n; k++)
      if (panel_rx_byte(&s_rx, fwd[k])) handle_frame();
  }

  tram_link_task();                  // cap nhat co `tram.linked`

  const uint32_t now = panel_millis();

  if (panel.linked && (now - panel.last_rx_ms) > PANEL_LINK_TIMEOUT_MS) {
    panel.linked = false;
    s_want_full = true;                        // khi noi lai thi xin lai tu dau
  }

  // PING 1s/lan: vua de kiem tra lien ket, vua de Marlin biet panel con song.
  // KHONG gui khi dang o che do TRAM: byte nhi phan cua frame PING co the lot vao bo giai
  // lenh ASCII cua Marlin (panel_tram_link). O che do TRAM, Marlin tu gui dong TRAM dinh ky
  // nen khong can PING.
  if (!tram_link_active() && (!s_next_ping_ms || now >= s_next_ping_ms)) {
    s_next_ping_ms = now + PANEL_LINK_PING_MS;
    panel_link_ping();
  }

  if (s_want_full && panel.linked) {
    s_want_full = false;
    panel.files_done = false;
    panel.n_files = 0;
    panel_link_request(PANEL_REQ_FULL);
    panel_link_request(PANEL_REQ_FILES);
  }
}
