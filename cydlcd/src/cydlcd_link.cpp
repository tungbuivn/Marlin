/**
 * cydlcd_link.cpp -- Phia MAN HINH CYD: nhan frame push tu Marlin, gui G-code len (xem cydlcd_link.h).
 */
#include "cydlcd_link.h"

#include <string.h>
#include "cydlcd_port.h"
#include "tbt_link.h"        // duong TRAMMING (ASCII) dung CHUNG UART - xem tbt_link.h
#include "cyd_sd.h"        // doc sector tho cho Marlin (che do CYDLCD_SD_UART)

// Log chan doan cho phan the SD (chi tren board that; ban chay tren may tinh khong co Serial)
#if defined(CYDLCD_BOARD_CYD)
  #define SDLOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define SDLOG(...)   do { } while (0)
#endif

cydlcd_data_t cydlcd;

static cydlcd_rx_t  s_rx;
static cydlcd_tx_t  s_tx;
static uint32_t    s_next_ping_ms = 0;
static bool        s_want_full = true;         // xin FULL ngay khi lien ket duoc
// Anh cua bo dem loi 16-bit trong cydlcd_protocol.h (cydlcd_rx_t.errors) o lan doc truoc, de biet
// "VUA co loi moi" ma khong phai sua cydlcd_protocol.h (file do phai giu byte-identical voi ban
// Marlin). Chi so sanh KHAC NHAU, khong cong don: bo dem 16-bit co the tran ve 0 sau 65 536 loi
// nhung 65535 -> 0 van la "khac" -> van bat duoc co cydlcd.rx_errors.
static uint16_t    s_err_seen = 0;

// ---------------------------------------------------------------------------
// Gui frame (frame cua MAN HINH CYD rat ngan: PING 7 byte, GCODE/RQUEST < 40 byte)
// ---------------------------------------------------------------------------
static void send_frame(const uint8_t total) {
  if (total) CYDLCD_MARLIN_SERIAL.write(s_tx.buf, total);
}

#if CYDLCD_SD_UART
// ---------------------------------------------------------------------------
// May chu the SD cho Marlin (proto >= 3): tra 1 sector 512 B = CYDLCD_SD_CHUNKS frame, moi frame
// CYDLCD_SD_CHUNK byte. Ly do chia manh: truong `len` cua khung chi co 1 BYTE (max 255).
//   status != OK -> gui DUNG 1 frame (n = 0) de Marlin biet ngay ma khong doi du 4 manh.
//
// CHIEU GHI (P3): Marlin gui CYDLCD_F_SDREQ{op=WRITE, sector} roi gui 4 frame CYDLCD_F_SDDATA
// (moi frame 128 B); MAN HINH CYD ghep lai, ghi xuong the bang SD.writeRAW() roi tra CYDLCD_F_SDACK.
// Neu thieu manh (mat goi) qua CYDLCD_SD_WR_TIMEOUT_MS thi HUY phien (Marlin se bao loi ghi).
// ---------------------------------------------------------------------------
static uint8_t s_sd_sector_buf[CYDLCD_SD_SECTOR];   // buffer tinh: khong dung stack cua vong lap chinh

// Phien GHI dang cho du lieu
static uint8_t  s_wr_buf[CYDLCD_SD_SECTOR];
static uint32_t s_wr_sector_ms = 0;
static uint32_t s_wr_sector = 0;
static uint8_t  s_wr_have = 0;
static bool     s_wr_active = false;

// Neu thieu manh (mat goi UART) qua bay lau thi HUY phien (Marlin se bao loi ghi, khong bi treo).
#ifndef CYDLCD_SD_WR_TIMEOUT_MS
  #define CYDLCD_SD_WR_TIMEOUT_MS 600u
#endif

void cydlcd_link_send_sd_data(const uint8_t status, const uint32_t sector, const uint8_t *data) {
  for (uint8_t idx = 0; idx < CYDLCD_SD_CHUNKS; idx++) {
    cydlcd_tx_begin(&s_tx, CYDLCD_F_SDDATA);
    cydlcd_tx_u8(&s_tx, status);
    cydlcd_tx_u32(&s_tx, sector);
    cydlcd_tx_u8(&s_tx, idx);
    if (status == CYDLCD_SD_ST_OK && data) {
      cydlcd_tx_u8(&s_tx, CYDLCD_SD_CHUNK);
      cydlcd_tx_raw(&s_tx, data + (uint16_t)idx * CYDLCD_SD_CHUNK, CYDLCD_SD_CHUNK);
    }
    else {
      cydlcd_tx_u8(&s_tx, 0);
      send_frame(cydlcd_tx_finish(&s_tx));
      return;                                  // loi -> chi 1 frame
    }
    send_frame(cydlcd_tx_finish(&s_tx));
  }
}

void cydlcd_link_send_sd_ack(const uint8_t status, const uint32_t sector) {
  cydlcd_tx_begin(&s_tx, CYDLCD_F_SDACK);
  cydlcd_tx_u8(&s_tx, status);
  cydlcd_tx_u32(&s_tx, sector);
  send_frame(cydlcd_tx_finish(&s_tx));
}

// Nhan 1 manh du lieu cua lenh GHI. Du 4 manh -> ghi xuong the + tra SDACK.
void cydlcd_link_sd_write_chunk(const uint8_t *p, const uint8_t n) {
  if (!s_wr_active) return;
  s_wr_sector_ms = cydlcd_millis();                  // con manh den -> gia han
  if (n == 0) return;
  // p[0..4] = status(0) + sector(u32) -> da kiem o handle_frame, p[5] = idx, p[6] = n, p[7..] = du lieu
  const uint8_t idx = p[5];
  const uint8_t cnt = p[6];
  if (idx >= CYDLCD_SD_CHUNKS || cnt > CYDLCD_SD_CHUNK) return;
  if ((uint16_t)n < (uint16_t)(7 + cnt)) return;    // frame thieu du lieu
  memcpy(s_wr_buf + (uint16_t)idx * CYDLCD_SD_CHUNK, &p[7], cnt);
  s_wr_have |= (uint8_t)(1u << idx);
  if (s_wr_have == (uint8_t)((1u << CYDLCD_SD_CHUNKS) - 1u)) {
    const bool ok = cyd_sd_raw_write(s_wr_sector, s_wr_buf);
    cydlcd_link_send_sd_ack(ok ? CYDLCD_SD_ST_OK : CYDLCD_SD_ST_ERR, s_wr_sector);
    s_wr_active = false;
  }
}

void cydlcd_link_sd_write_begin(const uint32_t sector) {
  s_wr_sector = sector;
  s_wr_have = 0;
  s_wr_active = true;
  s_wr_sector_ms = cydlcd_millis();
  // KHONG ack o day: Marlin gui ngay 4 frame du lieu, MAN HINH CYD tra SDACK sau khi ghi xong.
}
#endif  // CYDLCD_SD_UART

void cydlcd_link_ping() {
  cydlcd_tx_begin(&s_tx, CYDLCD_F_PING);
  cydlcd_tx_u8(&s_tx, ++s_tx.seq);
  send_frame(cydlcd_tx_finish(&s_tx));
  
}

void cydlcd_link_send_gcode(const char *line) {
  if (!line || !*line) return;
  cydlcd_tx_begin(&s_tx, CYDLCD_F_GCODE);
  cydlcd_tx_str(&s_tx, line, CYDLCD_MAX_PAYLOAD - 1);
  send_frame(cydlcd_tx_finish(&s_tx));
}

void cydlcd_link_send_gcode2(const char *a, const char *b) {
  cydlcd_link_send_gcode(a);
  if (b && *b) cydlcd_link_send_gcode(b);
}

void cydlcd_link_request(const uint8_t what) {
  cydlcd_tx_begin(&s_tx, CYDLCD_F_REQUEST);
  cydlcd_tx_u8(&s_tx, what);
  send_frame(cydlcd_tx_finish(&s_tx));
}

void cydlcd_link_request_dir(const char *dirname) {
  cydlcd_tx_begin(&s_tx, CYDLCD_F_REQUEST);
  cydlcd_tx_u8(&s_tx, (uint8_t)CYDLCD_REQ_FILES_DIR);
  cydlcd_tx_str(&s_tx, dirname, CYDLCD_FILE_NAME_LEN);
  send_frame(cydlcd_tx_finish(&s_tx));
}

void cydlcd_link_answer(const uint8_t answer) {
  if (!cydlcd.prompt_active) return;
  cydlcd_tx_begin(&s_tx, CYDLCD_F_ANSWER);
  cydlcd_tx_u8(&s_tx, cydlcd.prompt_id);
  cydlcd_tx_u8(&s_tx, answer);
  send_frame(cydlcd_tx_finish(&s_tx));
  cydlcd.prompt_active = false;
}

void cydlcd_link_send_tbt_cmd(char cmd) {
  if (!cmd) return;
  cydlcd_tx_begin(&s_tx, CYDLCD_F_TBT_CMD);
  cydlcd_tx_u8(&s_tx, (uint8_t)cmd);
  send_frame(cydlcd_tx_finish(&s_tx));
}

int32_t cydlcd_setting(const uint16_t id, const int32_t def) {
  for (uint8_t i = 0; i < cydlcd.n_settings; i++)
    if (cydlcd.settings[i].id == id) return cydlcd.settings[i].value;
  return def;
}

// Ghi 1 thong so vao bang `cydlcd.settings[]` (them neu chua co). Dung cho cac thong so do CHINH MAN HINH CYD
// quan ly - Marlin khong bao gio gui chung, vd 9 preset preheat (id 0x40xx, xem ui_preheat.cpp).
// Marlin gui frame CYDLCD_F_SETTING thi di qua duong rieng o handle_frame (logic y het).
void cydlcd_setting_set(const uint16_t id, const int32_t value) {
  for (uint8_t i = 0; i < cydlcd.n_settings; i++)
    if (cydlcd.settings[i].id == id) { cydlcd.settings[i].value = value; return; }
  if (cydlcd.n_settings < CYDLCD_SETTINGS_MAX) {
    cydlcd.settings[cydlcd.n_settings].id = id;
    cydlcd.settings[cydlcd.n_settings].value = value;
    cydlcd.n_settings++;
  }
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
  cydlcd.last_rx_ms = cydlcd_millis();
  cydlcd.linked = true;

  switch ((cydlcd_frame_t)s_rx.type) {

    case CYDLCD_F_HELLO:
      cydlcd.marlin_ver[0] = '\0';
      if (n >= 6) {
        cydlcd.marlin_fw   = (uint16_t)(p[0] | (p[1] << 8));
        cydlcd.proto       = p[2];
        cydlcd.hello_flags = p[3];
        cydlcd.n_heaters   = p[4];
        cydlcd.n_fans_max  = p[5];
        const uint8_t used = str_at(p, n, 6, cydlcd.name, sizeof(cydlcd.name));
        // Proto >= 2: Marlin gui them 1 str SHORT_BUILD_VERSION ngay SAU `name`.
        // Doc kieu "neu con byte thi doc" -> HELLO cua Marlin ban cu (proto 1) van doc duoc:
        // het payload thi chuoi rong, khong loi gi.
        if ((uint8_t)(6 + used) < n)
          str_at(p, n, (uint8_t)(6 + used), cydlcd.marlin_ver, sizeof(cydlcd.marlin_ver));
      }
      // Sau HELLO xin ngay du lieu day du (ten file, thong so, mesh)
      cydlcd.files_pending = true;
      cydlcd_link_request(CYDLCD_REQ_SETTINGS);
      if (cydlcd.hello_flags & CYDLCD_HAS_MESH) cydlcd_link_request(CYDLCD_REQ_MESH);
      break;

    case CYDLCD_F_STATUS:
      if (n >= 3) { cydlcd.state = p[0]; cydlcd.flags = p[1]; cydlcd.progress_pct = p[2]; }
      break;

    case CYDLCD_F_TEMPS: {
      cydlcd.n_temps = 0;
      if (n >= 1) {
        uint8_t cnt = p[0], off = 1;
        for (uint8_t i = 0; i < cnt && off + 4 < n && cydlcd.n_temps < 8; i++, off += 5) {
          cydlcd.temps[cydlcd.n_temps].id  = p[off];
          cydlcd.temps[cydlcd.n_temps].cur = (int16_t)(p[off + 1] | (p[off + 2] << 8));
          cydlcd.temps[cydlcd.n_temps].tgt = (int16_t)(p[off + 3] | (p[off + 4] << 8));
          cydlcd.n_temps++;
        }
      }
      break;
    }

    case CYDLCD_F_POSITION:
      if (n >= 22) {
        cydlcd.pos.x = cydlcd_i32(&p[0]);
        cydlcd.pos.y = cydlcd_i32(&p[4]);
        cydlcd.pos.z = cydlcd_i32(&p[8]);
        cydlcd.pos.e = cydlcd_i32(&p[12]);
        // p[16..17] = feed mm/s*10 (chua dung), roi moi den 2 phan tbt
        cydlcd.pos.feed_pct = cydlcd_i16(&p[18]);
        cydlcd.pos.flow_pct = cydlcd_i16(&p[20]);
      }
      break;

    case CYDLCD_F_PROGRESS:
      if (n >= 9) {
        cydlcd.prog.pct         = p[0];
        cydlcd.prog.elapsed_s   = cydlcd_u32(&p[1]);
        cydlcd.prog.remaining_s = cydlcd_u32(&p[5]);
        str_at(p, n, 9, cydlcd.prog.file, sizeof(cydlcd.prog.file));
      }
      break;

    case CYDLCD_F_TBT: {
      // 1 dong ASCII "TBT ..." (che do PUSH, 2026-10-13) -> dua thang vao bo giai cua tbt_link,
      // KHONG qua bo "sniff" byte (bo do chi dung cho duong UART tho cua CYDLCD_TBT_LINK).
      char t[TBT_LINK_LINE_MAX];
      str_at(p, n, 0, t, sizeof(t));
      tbt_link_line(t);
      break;
    }

    case CYDLCD_F_MESSAGE:
      if (n >= 1) {
        cydlcd.message_sev = p[0];
        str_at(p, n, 1, cydlcd.message, sizeof(cydlcd.message));
        cydlcd.message_ms = cydlcd_millis();
      }
      break;

    case CYDLCD_F_FANS: {
      cydlcd.n_fans = 0;
      if (n >= 1) {
        const uint8_t cnt = p[0];
        uint8_t off = 1;
        for (uint8_t i = 0; i < cnt && off + 2 < n && cydlcd.n_fans < 4; i++, off += 3) {
          cydlcd.fans[cydlcd.n_fans].idx = p[off];
          cydlcd.fans[cydlcd.n_fans].pct = (int16_t)(p[off + 1] | (p[off + 2] << 8));
          cydlcd.n_fans++;
        }
      }
      break;
    }

    case CYDLCD_F_ENDSTOPS: {
      // u8 n, n * (u8 id, u8 hit). Xoa truoc roi nap -> id nao Marlin khong bao (vi du Z_MIN
      // khi da bao la PROBE) coi nhu khong cham.
      cydlcd.n_endstops = 0;
      for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) cydlcd.es_hit[i] = 0;
      if (n >= 1) {
        const uint8_t cnt = p[0];
        uint8_t off = 1;
        for (uint8_t i = 0; i < cnt && off + 1 < n; i++, off += 2) {
          const uint8_t id = p[off];
          if (id < CYDLCD_ENDSTOP_MAX) cydlcd.es_hit[id] = p[off + 1] ? 1 : 0;
          if (cydlcd.n_endstops < CYDLCD_ENDSTOP_MAX) cydlcd.n_endstops++;
        }
      }
      break;
    }

    case CYDLCD_F_SETTING:
      if (n >= 6) {
        const uint16_t id = (uint16_t)(p[0] | (p[1] << 8));
        const int32_t  v  = cydlcd_i32(&p[2]);
        bool found = false;
        for (uint8_t i = 0; i < cydlcd.n_settings; i++)
          if (cydlcd.settings[i].id == id) { cydlcd.settings[i].value = v; found = true; break; }
        if (!found && cydlcd.n_settings < CYDLCD_SETTINGS_MAX) {
          cydlcd.settings[cydlcd.n_settings].id = id;
          cydlcd.settings[cydlcd.n_settings].value = v;
          cydlcd.n_settings++;
        }
      }
      break;

    case CYDLCD_F_MESH:
      if (n >= 3) {
        cydlcd.mesh_ny = p[0];
        const uint8_t row = p[1];
        cydlcd.mesh_nx = p[2];
        if (row < CYDLCD_MESH_MAX && cydlcd.mesh_nx <= CYDLCD_MESH_MAX) {
          const uint8_t cnt = (uint8_t)((n - 3) / 2);
          for (uint8_t c = 0; c < cnt && c < cydlcd.mesh_nx; c++)
            cydlcd.mesh[c][row] = cydlcd_i16(&p[3 + c * 2]);
          cydlcd.mesh_rows++;
          cydlcd.mesh_valid = true;
        }
      }
      break;

    case CYDLCD_F_FILES:
      if (n >= 5) {
        if (p[0] == 0xFF) { cydlcd.files_done = true; cydlcd.files_pending = false; }
        else {
          if (cydlcd.files_done) { cydlcd.files_done = false; cydlcd.n_files = 0; }   // bat dau danh sach moi
          if (cydlcd.n_files < CYDLCD_MAX_FILES) {
            cydlcd.files[cydlcd.n_files].flags = p[0];
            str_at(p, n, 5, cydlcd.files[cydlcd.n_files].name, CYDLCD_FILE_NAME_LEN);
            cydlcd.n_files++;
          }
        }
      }
      break;

    case CYDLCD_F_EVENT:
      if (n >= 5) {
        cydlcd.event    = p[0];
        cydlcd.event_v1 = (uint16_t)(p[1] | (p[2] << 8));
        cydlcd.event_v2 = (uint16_t)(p[3] | (p[4] << 8));
        cydlcd.event_ms = cydlcd_millis();
        // Su kien lam thay doi du lieu -> xin lai cho chac
        switch ((cydlcd_event_t)cydlcd.event) {
          case CYDLCD_EV_PRINT_START:
          case CYDLCD_EV_PRINT_STOP:
          case CYDLCD_EV_MEDIA_INSERTED:
          case CYDLCD_EV_MEDIA_REMOVED:
            cydlcd.files_done = false; cydlcd.files_pending = true;
            break;
          case CYDLCD_EV_LEVELING_DONE:
            cydlcd.mesh_rows = 0;
            cydlcd_link_request(CYDLCD_REQ_MESH);
            break;
          case CYDLCD_EV_PID_DONE:
            // Marlin da chay xong M303 (v1 = result_t: 0 = thanh cong). UI doc moc nay de
            // hien hop "da xong" mot lan (so sanh voi lan truoc), khong hien lai mai.
            cydlcd.pid_done_ms = cydlcd_millis();
            break;
          case CYDLCD_EV_SETTINGS_LOADED:
          case CYDLCD_EV_FACTORY_RESET:
            cydlcd.n_settings = 0;
            cydlcd_link_request(CYDLCD_REQ_SETTINGS);
            cydlcd_link_request(CYDLCD_REQ_MESH);
            break;
          default: break;
        }
      }
      break;

    case CYDLCD_F_PROMPT:
      if (n >= 2) {
        cydlcd.prompt_id   = p[0];
        cydlcd.prompt_type = p[1];
        str_at(p, n, 2, cydlcd.prompt_text, sizeof(cydlcd.prompt_text));
        cydlcd.prompt_active = true;
      }
      break;

    case CYDLCD_F_PONG:
      break;                                   // da nhan duoc gi -> linked = true

    #if CYDLCD_SD_UART
      case CYDLCD_F_SDREQ: {
        // Marlin hoi 1 sector 512 B cua the SD (the nam o khe CYD - xem README muc 12).
        //   op = READ : tra loi NGAY bang 4 frame CYDLCD_F_SDDATA.
        //   op = WRITE: mo phien cho du lieu; Marlin gui tiep 4 frame CYDLCD_F_SDDATA (xem
        //               cydlcd_link_sd_write_chunk - tra CYDLCD_F_SDACK sau khi ghi xong).
        if (n >= 5) {
          const uint8_t  op     = p[0];
          const uint32_t sector = cydlcd_u32(&p[1]);
          if (op == CYDLCD_SD_OP_READ) {
            const bool ok = cyd_sd_raw_read(sector, s_sd_sector_buf);
            cydlcd_link_send_sd_data(ok ? CYDLCD_SD_ST_OK : CYDLCD_SD_ST_ERR, sector, ok ? s_sd_sector_buf : nullptr);
          }
          else {
            cydlcd_link_sd_write_begin(sector);
          }
        }
        break;
      }

      case CYDLCD_F_SDDATA: {
        // Chi co nghia khi dang trong phien GHI (chieu MAN HINH CYD -> Marlin la du lieu DOC, khong toi day)
        if (n >= 7) cydlcd_link_sd_write_chunk(p, n);
        break;
      }
    #endif

    default:
      break;
  }
  // handle_frame() CHI duoc goi khi cydlcd_rx_byte() da kiem CRC OK -> day la 1 frame HOP LE.
  // Bat CO MOT CHIEU (khong bao gio ve false): vua de man INFO hien "RX: OK", vua de TAT SNIFFER
  // TRAM vinh vien trong cydlcd_link_task() - xem cydlcd_data_t.rx_frames.
  cydlcd.rx_frames = true;
}

// ---------------------------------------------------------------------------
// API chung
// ---------------------------------------------------------------------------
void cydlcd_link_begin(const uint32_t baud) {
  CYDLCD_LINK_BEGIN(baud);
  cydlcd_rx_init(&s_rx);
  memset(&cydlcd, 0, sizeof(cydlcd));    // xoa ca 2 CO cydlcd.rx_frames / cydlcd.rx_errors ve false
  s_err_seen = 0;                      // anh bo dem loi cua cydlcd_protocol.h cung phai ve 0
  tbt_link_begin();                 // duong TRAMMING: reset trang thai (khong mo lai UART)
  s_want_full = true;
  s_next_ping_ms = 0;
}

bool cydlcd_link_connected() { return cydlcd.linked; }

void cydlcd_link_task() {
  uint8_t fwd[4];
  while (CYDLCD_MARLIN_SERIAL.available()) {
    const uint8_t c = (uint8_t)CYDLCD_MARLIN_SERIAL.read();
    // Duong TRAMMING (Marlin: CYDLCD_TBT_LINK) dung CHUNG UART nay: tbt_link_feed() tra ve 0
    // khi byte thuoc dong "TBT ...", nguoc lai tra ve so byte (0..4) can dua cho bo giai frame
    // PUSH - cac byte 'T','R','A' dang giu de do duoc tra lai day du nen KHONG mat byte nao.
    //
    // 2026-10-13: che do PUSH (CYDLCD_SERIAL_UI) gui dong TBT trong frame CYDLCD_F_TBT -> payload
    // CUA FRAME chua chinh chuoi "TBT ...". Neu van chay bo sniff thi no se "an" cac byte do lam
    // frame hong (da gap that: byte tang ~500 B/s nhung cydlcd.linked=0, khong nhan duoc frame nao).
    // => Da nhan duoc frame PUSH hop le dau tien thi TAT sniff VINH VIEN. Dung CO MOT CHIEU
    //    cydlcd.rx_frames (KHONG dung bo dem nhu truoc: bo dem tran ve 0 se bat lai sniff giua chung).
    uint8_t n;
    if (cydlcd.rx_frames) { fwd[0] = c; n = 1; }
    else                 n = tbt_link_feed(c, fwd);
    for (uint8_t k = 0; k < n; k++)
      if (cydlcd_rx_byte(&s_rx, fwd[k])) handle_frame();
  }

  // BAT CO LOI MOT CHIEU: cydlcd_protocol.h tang `errors` khi CRC sai hoac payload qua dai
  // (cydlcd_rx_byte), nhung bo dem do la uint16_t nen khong dung lam tong duoc. Ta chi can biet
  // "da tung co loi" -> so sanh voi gia tri lan truoc, khac la bat co (tran cung khong sai).
  if (s_rx.errors != s_err_seen) { s_err_seen = s_rx.errors; cydlcd.rx_errors = true; }

  #if CYDLCD_SD_UART
    // Phien GHI bi bo do: het gio cho du lieu -> huy (Marlin se het cho va bao loi ghi).
    if (s_wr_active && (cydlcd_millis() - s_wr_sector_ms) > CYDLCD_SD_WR_TIMEOUT_MS) {
      s_wr_active = false;
      // Ghi log chan doan (chi tren board that)
      SDLOG("[SD] huy phien GHI sector %lu (thieu du lieu)\n", (unsigned long)s_wr_sector);
    }
  #endif

  tbt_link_task();                  // cap nhat co `tbt.linked`

  const uint32_t now = cydlcd_millis();

  if (cydlcd.linked && (now - cydlcd.last_rx_ms) > CYDLCD_LINK_TIMEOUT_MS) {
    cydlcd.linked = false;
    s_want_full = true;                        // khi noi lai thi xin lai tu dau
  }

  // PING 1s/lan: vua de kiem tra lien ket, vua de Marlin biet MAN HINH CYD con song.
  // KHONG gui khi dang o che do TRAM: byte nhi phan cua frame PING co the lot vao bo giai
  // lenh ASCII cua Marlin (cydlcd_tbt_link). O che do TRAM, Marlin tu gui dong TBT dinh ky
  // nen khong can PING.
  if (!tbt_link_active() && (!s_next_ping_ms || now >= s_next_ping_ms)) {
    s_next_ping_ms = now + CYDLCD_LINK_PING_MS;
    cydlcd_link_ping();
  }

  if (s_want_full && cydlcd.linked) {
    s_want_full = false;
    cydlcd.files_done = false;
    cydlcd.n_files = 0;
    cydlcd_link_request(CYDLCD_REQ_FULL);
    cydlcd_link_request(CYDLCD_REQ_FILES);
  }
}
