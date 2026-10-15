/**
 * cydlcd_link.cpp -- Phia MAN HINH CYD: nhan frame push tu Marlin, gui G-code len (xem cydlcd_link.h).
 */
#include "cydlcd_link.h"

#include <string.h>
#include "cydlcd_port.h"
#include "tbt_link.h"        // duong TRAMMING (ASCII) dung CHUNG UART - xem tbt_link.h
#include "cyd_sd.h"        // doc sector tho cho Marlin (che do CYDLCD_SD_UART)
#include "logq.h"          // log KHONG CHAN: task `link` khong duoc `Serial.print` (xem logq.h)
#include "ui_lvgl.h"       // ui_lvgl_perf_link_call(): do do tre duong serial (chi board CYD co LVGL)

// Log chan doan cho phan the SD (chi tren board that; ban chay tren may tinh khong co Serial)
// TU 2026-10-15 (da luong): KHONG in truc tiep nua ma day vao hang doi (`logq_post` - timeout 0).
// LY DO: in 700 byte o 115200 mat ~60 ms; do duoc `link gap max 63 549 us` khi log in truc tiep trong
// task `link` => duong serial Marlin bi chan moi giay. Xem logq.h + ke-hoach-da-luong.md.
#if defined(CYDLCD_BOARD_CYD)
  #define SDLOG(...)   do { logq_post(__VA_ARGS__); } while (0)
#else
  #define SDLOG(...)   do { } while (0)
#endif

// ===========================================================================
//  TRANG THAI: ban RIENG cua task `link` (s_src) va ban CONG KHAI cho nguoi doc (cydlcd)
// ===========================================================================
// DA LUONG (2026-10-15, xem cydlcd/docs/ke-hoach-da-luong.md): task `link` la CHU SO HUU duy nhat cua trang
// thai - ghi vao `s_src` (khong khoa, khong cho ai), roi "publish" sang `cydlcd` (ban ma MOI code khac dang
// doc, nho vay khong phai sua cho nao doc). Publish dung mutex voi `xSemaphoreTake(..., 0)`:
//  - nguoi doc (task `ui`, core 0) giu khoa trong luc doc nhieu truong lien tiep;
//  - task `link` KHONG BAO GIO cho: khoa dang bi giu thi BO QUA lan publish nay (du lieu van nguyen o
//    `s_src`, lan sau publish lai) => duong serial khong bao gio bi man hinh chan.
cydlcd_data_t cydlcd;                  // ban CONG KHAI (moi code khac doc)
static cydlcd_data_t s_src;            // ban GOC, chi task `link` ghi

#if defined(CYDLCD_BOARD_CYD)
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/semphr.h"
  static SemaphoreHandle_t s_state_mtx = nullptr;
  static volatile bool     s_pub_pending = false;   // co thay doi chua publish duoc (khoa dang bi giu)
  static void link_task_fn(void *);
#endif

// Publish `s_src` -> `cydlcd` (KHONG BAO GIO cho khoa). Goi o cuoi moi vong `cydlcd_link_task()`.
static void state_publish(void) {
#if defined(CYDLCD_BOARD_CYD)
  if (!s_state_mtx) { memcpy(&cydlcd, &s_src, sizeof(cydlcd)); return; }   // chua tao khoa (luc setup)
  if (xSemaphoreTake(s_state_mtx, 0) == pdTRUE) {
    memcpy(&cydlcd, &s_src, sizeof(cydlcd));
    tbt_publish_locked();               // trang thai TRAMMING (`tbt`) cung duoc publish trong CUNG khoa nay
    s_pub_pending = false;
    xSemaphoreGive(s_state_mtx);
  }
  else s_pub_pending = true;          // nguoi doc dang doc -> lan sau publish (khong mat du lieu)
#else
  memcpy(&cydlcd, &s_src, sizeof(cydlcd));
#endif
}

void cydlcd_link_state_lock(void) {
#if defined(CYDLCD_BOARD_CYD)
  if (s_state_mtx) xSemaphoreTake(s_state_mtx, pdMS_TO_TICKS(20));
#endif
}
void cydlcd_link_state_unlock(void) {
#if defined(CYDLCD_BOARD_CYD)
  if (s_state_mtx) xSemaphoreGive(s_state_mtx);
#endif
}

static cydlcd_rx_t  s_rx;
static cydlcd_tx_t  s_tx;
static uint32_t    s_next_ping_ms = 0;
static uint8_t     s_hello_count = 0;           // so lan nhan HELLO (>= 2 = Marlin da restart - xem handle_frame)
static bool        s_want_full = true;         // xin FULL ngay khi lien ket duoc
// RESYNC "co link nhung chua tung nhan HELLO" (xem cydlcd_link_task): dem so lan da xin lai + moc thoi gian.
static uint8_t     s_nohello_req = 0;
static uint32_t    s_nohello_ms = 0;
// PONG: Marlin tra loi moi frame PING. VI SAO CAN DO: Marlin -> man hinh co the chay tot trong khi chieu
// NGUOC LAI (lenh tu man hinh) chet han (da gap that 2026-10-15: bam nut / `M220` gui tu man hinh khong
// chay, ma khong co cho nao bao). PONG la dau hieu DUY NHAT phia man hinh de biet Marlin CON NHAN duoc lenh.
static uint32_t    s_pong_ms = 0;               // lan cuoi nhan PONG
static uint32_t    s_pong_canh_bao_ms = 0;      // lan cuoi in canh bao "khong co PONG"
static uint16_t    s_pong_lost = 0;             // so lan phat hien mat PONG (chan doan)
static uint16_t    s_rx_type[0x20];             // dem frame NHAN DUOC theo LOAI - chi 0x01..0x0F (man hinh
                                                // khong bao gio nhan type >= 0x40: do la chieu GUI di).
                                                // Giam tu 0x50 -> 0x20 vi DRAM tinh (dram0) cua ESP32 da kin.
// Anh cua bo dem loi 16-bit trong cydlcd_protocol.h (cydlcd_rx_t.errors) o lan doc truoc, de biet
// "VUA co loi moi" ma khong phai sua cydlcd_protocol.h (file do phai giu byte-identical voi ban
// Marlin). Chi so sanh KHAC NHAU, khong cong don: bo dem 16-bit co the tran ve 0 sau 65 536 loi
// nhung 65535 -> 0 van la "khac" -> van bat duoc co s_src.rx_errors.
static uint16_t    s_err_seen = 0;

// ---------------------------------------------------------------------------
// Gui frame (frame cua MAN HINH CYD rat ngan: PING 7 byte, GCODE/RQUEST < 40 byte)
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  HANG DOI TX: khong bao gio CHAN vong lap khi ghi ra Marlin.
//
//  VI SAO: CYDLCD_MARLIN_SERIAL.write(buf, n) la ham CHAN - no doi cho den khi TX ring (64 byte)
//  trong. Neu Marlin tam thoi KHONG doc UART (dang ban trong mot vong cho, hang doi day, hoac
//  firmware treo) thi write() treo VO HAN -> vong lap man hinh DUNG -> watchdog chan doan
//  (cyd_diag_task: >=15 s thi esp_restart) khoi dong lai man hinh. Trieu chung nguoi dung thay:
//  "bam STOP de huy ban in thi LCD RESTART" (gap that 2026-10-15, luc Marlin dang in + cho nong
//  ban in nen hang doi day, moi lenh tu COM7/UART deu bi Marlin tra "busy: processing").
//
//  CACH LAM: chi ghi THANG khi (a) hang doi RONG va (b) TX ring du cho CA FRAME (khong cat frame
//  giua duong). Con lai thi COPY frame vao hang doi nho roi trai dan o nhip sau (cydlcd_link_tx_task).
//  Hang doi day -> BO frame + dem lai (xem s_tx_drop) - bo con hon treo vong lap.
// ---------------------------------------------------------------------------
#define CYDLCD_TXQ_FRAMES 4
static uint8_t  s_txq[CYDLCD_TXQ_FRAMES][CYDLCD_MAX_FRAME];
static uint8_t  s_txq_len[CYDLCD_TXQ_FRAMES];
static uint8_t  s_txq_head = 0, s_txq_n = 0;
static uint32_t s_tx_drop = 0;                  // so frame bi bo khi hang doi day (chan doan)

static void send_frame(const uint8_t total) {
  if (!total) return;
  if (!s_txq_n && (int)CYDLCD_MARLIN_SERIAL.availableForWrite() >= (int)total) {
    CYDLCD_MARLIN_SERIAL.write(s_tx.buf, total);       // du cho -> gui thang (khong cho)
    return;
  }
  if (s_txq_n >= CYDLCD_TXQ_FRAMES) { s_tx_drop++; return; }
  const uint8_t i = (uint8_t)((s_txq_head + s_txq_n) % CYDLCD_TXQ_FRAMES);
  memcpy(s_txq[i], s_tx.buf, total);                   // PHAI copy: s_tx.buf dung lai ngay sau day
  s_txq_len[i] = total;
  s_txq_n++;
}

// Trai hang doi TX. Goi moi vong lap (cydlcd_link_task) - khi Marlin doc lai thi hang doi tat ngay.
void cydlcd_link_tx_task(void) {
  while (s_txq_n) {
    const uint8_t l = s_txq_len[s_txq_head];
    if ((int)CYDLCD_MARLIN_SERIAL.availableForWrite() < (int)l) break;
    CYDLCD_MARLIN_SERIAL.write(s_txq[s_txq_head], l);
    s_txq_head = (uint8_t)((s_txq_head + 1) % CYDLCD_TXQ_FRAMES);
    s_txq_n--;
  }
}

// So frame da bi bo vi hang doi TX day (chan doan: `TXQ?` o main.cpp).
uint32_t cydlcd_link_tx_dropped(void) { return s_tx_drop; }
// So frame dang cho trong hang doi TX (0 = da gui het).
uint8_t cydlcd_link_tx_pending(void) { return s_txq_n; }

// ---- chan doan chieu MARLIN -> MAN HINH (khai bao o cydlcd_link.h; lenh `UI RX`) ----
uint16_t cydlcd_link_rx_count(const uint8_t type) { return (type < 0x20) ? s_rx_type[type] : 0; }
uint32_t cydlcd_link_pong_age_ms(void) {
  return s_pong_ms ? (uint32_t)(cydlcd_millis() - s_pong_ms) : 0xFFFFFFFFu;
}
uint16_t cydlcd_link_pong_lost(void) { return s_pong_lost; }

// Gui frame BAT BUOC (du lieu the SD / SDACK): CHO cho TX ring toi da timeout_ms, KHONG BAO GIO bo
// frame. VI SAO RIENG DUONG THE SD: Marlin doc 1 sector 512 B = 4 frame; mat DUY NHAT 1 frame la
// hong ca sector (Marlin bao "SD read error" va dung ban in). Nguoc lai, frame trang thai/PING thi
// BO DUOC (nhip sau gui lai) nen di qua hang doi khong-chan o tren.
// PHAI day HET hang doi truoc khi ghi truc tiep de khong dao THU TU frame.
static bool send_frame_wait(const uint8_t total, const uint32_t timeout_ms) {
  if (!total) return true;
  const uint32_t t0 = cydlcd_millis();
  while (s_txq_n) {
    cydlcd_link_tx_task();
    if (s_txq_n && (cydlcd_millis() - t0) > timeout_ms) return false;
  }
  uint8_t sent = 0;
  while (sent < total) {
    const int room = CYDLCD_MARLIN_SERIAL.availableForWrite();
    if (room > 0) {
      uint8_t k = (uint8_t)(total - sent);
      if (k > (uint8_t)room) k = (uint8_t)room;
      CYDLCD_MARLIN_SERIAL.write(s_tx.buf + sent, k);
      sent = (uint8_t)(sent + k);
    }
    else if ((cydlcd_millis() - t0) > timeout_ms) return false;   // tran: coi nhu mat frame
  }
  return true;
}
#define CYDLCD_SD_TX_WAIT_MS 250u       // 1 frame ~136 B @250000 baud mat ~5 ms -> 250 ms la rong rai

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
      send_frame_wait(cydlcd_tx_finish(&s_tx), CYDLCD_SD_TX_WAIT_MS);
      return;                                  // loi -> chi 1 frame
    }
    send_frame_wait(cydlcd_tx_finish(&s_tx), CYDLCD_SD_TX_WAIT_MS);
  }
}

void cydlcd_link_send_sd_ack(const uint8_t status, const uint32_t sector) {
  cydlcd_tx_begin(&s_tx, CYDLCD_F_SDACK);
  cydlcd_tx_u8(&s_tx, status);
  cydlcd_tx_u32(&s_tx, sector);
  // SDACK cung la frame BAT BUOC: Marlin cho ack nay moi coi lenh GHI xong -> khong duoc bo.
  send_frame_wait(cydlcd_tx_finish(&s_tx), CYDLCD_SD_TX_WAIT_MS);
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
  if (!s_src.prompt_active) return;
  cydlcd_tx_begin(&s_tx, CYDLCD_F_ANSWER);
  cydlcd_tx_u8(&s_tx, s_src.prompt_id);
  cydlcd_tx_u8(&s_tx, answer);
  send_frame(cydlcd_tx_finish(&s_tx));
  s_src.prompt_active = false;
}

void cydlcd_link_send_tbt_cmd(char cmd) {
  if (!cmd) return;
  cydlcd_tx_begin(&s_tx, CYDLCD_F_TBT_CMD);
  cydlcd_tx_u8(&s_tx, (uint8_t)cmd);
  send_frame(cydlcd_tx_finish(&s_tx));
}

int32_t cydlcd_setting(const uint16_t id, const int32_t def) {
  for (uint8_t i = 0; i < s_src.n_settings; i++)
    if (s_src.settings[i].id == id) return s_src.settings[i].value;
  return def;
}

// Ghi 1 thong so vao bang `s_src.settings[]` (them neu chua co). Dung cho cac thong so do CHINH MAN HINH CYD
// quan ly - Marlin khong bao gio gui chung, vd 9 preset preheat (id 0x40xx, xem ui_preheat.cpp).
// Marlin gui frame CYDLCD_F_SETTING thi di qua duong rieng o handle_frame (logic y het).
void cydlcd_setting_set(const uint16_t id, const int32_t value) {
  for (uint8_t i = 0; i < s_src.n_settings; i++)
    if (s_src.settings[i].id == id) { s_src.settings[i].value = value; return; }
  if (s_src.n_settings < CYDLCD_SETTINGS_MAX) {
    s_src.settings[s_src.n_settings].id = id;
    s_src.settings[s_src.n_settings].value = value;
    s_src.n_settings++;
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

// Ten THU MUC HE THONG (danh sach o CYDLCD_HIDE_DIRS, ngan cach dau phay; khong phan biet hoa/thuong).
// Chi xet THU MUC, khong xet FILE (file ten "BTUP" van hien binh thuong).
//   cydlcd_tmp = thu muc file tam cua MAN HINH (cyd_sd.cpp tu tao khi nhan file qua WiFi);
//   BTUP       = thu muc cu cua duong upload qua BLUETOOTH (da xoa khoi firmware 2026-10-14).
// LUU Y: day chi la "co phai muc he thong" - danh sach Marlin gui xuong VAN GIU NGUYEN DU
// (s_src.files[] + s_src.n_files), chi MAN HINH an khi hien (ui_page_files.cpp: files_view_build()).
bool cydlcd_hidden_dir(const char *name, const uint8_t flags) {
  if (!(flags & 0x01U) || !name || !name[0]) return false;
  const char *list = CYDLCD_HIDE_DIRS;
  while (*list) {
    while (*list == ',' || *list == ' ') list++;
    if (!*list) break;
    const char *comma = strchr(list, ',');
    const size_t len = comma ? (size_t)(comma - list) : strlen(list);
    if (strlen(name) == len && strncasecmp(name, list, len) == 0) return true;
    if (!comma) break;
    list = comma + 1;
  }
  return false;
}

static void handle_frame() {
  const uint8_t *p = s_rx.payload;
  const uint8_t  n = s_rx.len;
  s_src.last_rx_ms = cydlcd_millis();
  s_src.linked = true;
  if (s_rx.type < 0x20) s_rx_type[s_rx.type]++;    // chan doan: dem frame theo loai (xem UI RX)

  switch ((cydlcd_frame_t)s_rx.type) {

    case CYDLCD_F_HELLO:
      // HELLO = Marlin vua KHOI DONG (hoac vua reset/panic). Dem lai va LOG RO khi day la lan thu >= 2:
      // day la dau hieu DUY NHAT phia man hinh de biet "Marlin bi restart" (Marlin khong gui ly do
      // reset qua kenh nay) - dung de chan doan cac loi kieu "bam STOP thi Marlin restart".
      if (++s_hello_count > 1)
        SDLOG("[UP] *** MARLIN VUA KHOI DONG LAI (HELLO lan %u) - truoc do link=%u, ly do reset khong ro ***\n",
              (unsigned)s_hello_count, (unsigned)s_src.linked);
      s_src.marlin_ver[0] = '\0';
      if (n >= 6) {
        s_src.marlin_fw   = (uint16_t)(p[0] | (p[1] << 8));
        s_src.proto       = p[2];
        s_src.hello_flags = p[3];
        s_src.n_heaters   = p[4];
        s_src.n_fans_max  = p[5];
        const uint8_t used = str_at(p, n, 6, s_src.name, sizeof(s_src.name));
        // Proto >= 2: Marlin gui them 1 str SHORT_BUILD_VERSION ngay SAU `name`.
        // Doc kieu "neu con byte thi doc" -> HELLO cua Marlin ban cu (proto 1) van doc duoc:
        // het payload thi chuoi rong, khong loi gi.
        if ((uint8_t)(6 + used) < n)
          str_at(p, n, (uint8_t)(6 + used), s_src.marlin_ver, sizeof(s_src.marlin_ver));
      }
      // Sau HELLO xin ngay du lieu day du (ten file, thong so, mesh)
      s_src.files_pending = true;
      cydlcd_link_request(CYDLCD_REQ_SETTINGS);
      if (s_src.hello_flags & CYDLCD_HAS_MESH) cydlcd_link_request(CYDLCD_REQ_MESH);
      break;

    case CYDLCD_F_STATUS:
      if (n >= 3) { s_src.state = p[0]; s_src.flags = p[1]; s_src.progress_pct = p[2]; }
      // Byte thu 4 (2026-10-15): mesh/leveling dang BAT hay TAT. Marlin cu chi gui 3 byte -> giu gia tri
      // cu (khong doan bua); chi cap nhat khi frame co du 4 byte.
      if (n >= 4) s_src.mesh_on = (p[3] != 0);
      break;

    case CYDLCD_F_TEMPS: {
      s_src.n_temps = 0;
      if (n >= 1) {
        uint8_t cnt = p[0], off = 1;
        for (uint8_t i = 0; i < cnt && off + 4 < n && s_src.n_temps < 8; i++, off += 5) {
          s_src.temps[s_src.n_temps].id  = p[off];
          s_src.temps[s_src.n_temps].cur = (int16_t)(p[off + 1] | (p[off + 2] << 8));
          s_src.temps[s_src.n_temps].tgt = (int16_t)(p[off + 3] | (p[off + 4] << 8));
          s_src.n_temps++;
        }
      }
      break;
    }

    case CYDLCD_F_POSITION:
      if (n >= 22) {
        s_src.pos.x = cydlcd_i32(&p[0]);
        s_src.pos.y = cydlcd_i32(&p[4]);
        s_src.pos.z = cydlcd_i32(&p[8]);
        s_src.pos.e = cydlcd_i32(&p[12]);
        // p[16..17] = feed mm/s*10 (chua dung), roi moi den 2 phan tbt
        s_src.pos.feed_pct = cydlcd_i16(&p[18]);
        s_src.pos.flow_pct = cydlcd_i16(&p[20]);
      }
      break;

    case CYDLCD_F_PROGRESS:
      if (n >= 9) {
        s_src.prog.pct         = p[0];
        s_src.prog.elapsed_s   = cydlcd_u32(&p[1]);
        s_src.prog.remaining_s = cydlcd_u32(&p[5]);
        str_at(p, n, 9, s_src.prog.file, sizeof(s_src.prog.file));
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
        s_src.message_sev = p[0];
        str_at(p, n, 1, s_src.message, sizeof(s_src.message));
        s_src.message_ms = cydlcd_millis();
      }
      break;

    case CYDLCD_F_FANS: {
      s_src.n_fans = 0;
      if (n >= 1) {
        const uint8_t cnt = p[0];
        uint8_t off = 1;
        for (uint8_t i = 0; i < cnt && off + 2 < n && s_src.n_fans < 4; i++, off += 3) {
          s_src.fans[s_src.n_fans].idx = p[off];
          s_src.fans[s_src.n_fans].pct = (int16_t)(p[off + 1] | (p[off + 2] << 8));
          s_src.n_fans++;
        }
      }
      break;
    }

    case CYDLCD_F_ENDSTOPS: {
      // u8 n, n * (u8 id, u8 hit). Xoa truoc roi nap -> id nao Marlin khong bao (vi du Z_MIN
      // khi da bao la PROBE) coi nhu khong cham.
      s_src.n_endstops = 0;
      for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) s_src.es_hit[i] = 0;
      if (n >= 1) {
        const uint8_t cnt = p[0];
        uint8_t off = 1;
        for (uint8_t i = 0; i < cnt && off + 1 < n; i++, off += 2) {
          const uint8_t id = p[off];
          if (id < CYDLCD_ENDSTOP_MAX) s_src.es_hit[id] = p[off + 1] ? 1 : 0;
          if (s_src.n_endstops < CYDLCD_ENDSTOP_MAX) s_src.n_endstops++;
        }
      }
      break;
    }

    case CYDLCD_F_SETTING:
      if (n >= 6) {
        const uint16_t id = (uint16_t)(p[0] | (p[1] << 8));
        const int32_t  v  = cydlcd_i32(&p[2]);
        bool found = false;
        for (uint8_t i = 0; i < s_src.n_settings; i++)
          if (s_src.settings[i].id == id) { s_src.settings[i].value = v; found = true; break; }
        if (!found && s_src.n_settings < CYDLCD_SETTINGS_MAX) {
          s_src.settings[s_src.n_settings].id = id;
          s_src.settings[s_src.n_settings].value = v;
          s_src.n_settings++;
        }
      }
      break;

    case CYDLCD_F_MESH:
      if (n >= 3) {
        s_src.mesh_ny = p[0];
        const uint8_t row = p[1];
        s_src.mesh_nx = p[2];
        if (row < CYDLCD_MESH_MAX && s_src.mesh_nx <= CYDLCD_MESH_MAX) {
          const uint8_t cnt = (uint8_t)((n - 3) / 2);
          // Hang 0 = BAT DAU 1 luoi moi -> dem lai tu dau. Neu khong, moi lan Marlin day lai ca luoi (sau
          // reboot / sau G29 / khi link hoi phuc) thi `mesh_rows` CONG DON mai (da thay "7x7 rows 14").
          if (row == 0) s_src.mesh_rows = 0;
          for (uint8_t c = 0; c < cnt && c < s_src.mesh_nx; c++)
            s_src.mesh[c][row] = cydlcd_i16(&p[3 + c * 2]);
          s_src.mesh_rows++;
          s_src.mesh_valid = true;
        }
      }
      break;

    case CYDLCD_F_FILES:
      if (n >= 5) {
        if (p[0] == 0xFF) { s_src.files_done = true; s_src.files_pending = false; }
        else {
          if (s_src.files_done) { s_src.files_done = false; s_src.n_files = 0; }   // bat dau danh sach moi
          if (s_src.n_files < CYDLCD_MAX_FILES) {
            cydlcd_file_t *f = &s_src.files[s_src.n_files];
            f->flags = p[0];
            const uint8_t used = str_at(p, n, 5, f->name, CYDLCD_FILE_NAME_LEN);
            // Proto >= 4: Marlin gui them 1 str = TEN 8.3 (shortFilename) ngay SAU `name`.
            // VI SAO: ten dai (LFN) bi SdFat cat o 26 ky tu -> `M23 <ten dai>` khong khop duoc ten that
            // tren the (bam PRINT khong in duoc). Ten 8.3 thi luon khop -> man hinh gui lenh bang sfn.
            // Doc kieu "neu con byte thi doc" -> frame cua Marlin ban cu (proto 3) van doc duoc.
            f->sfn[0] = '\0';
            if ((uint8_t)(5 + used) < n) str_at(p, n, (uint8_t)(5 + used), f->sfn, sizeof(f->sfn));
            // GIU NGUYEN CA DANH SACH Marlin gui (ke ca thu muc he thong): day la du lieu Marlin dung
            // de in/xoa nen phai du va dung chi so; MAN HINH tu an khi hien (xem cydlcd_hidden_dir).
            s_src.n_files++;
          }
        }
      }
      break;

    case CYDLCD_F_EVENT:
      if (n >= 5) {
        s_src.event    = p[0];
        s_src.event_v1 = (uint16_t)(p[1] | (p[2] << 8));
        s_src.event_v2 = (uint16_t)(p[3] | (p[4] << 8));
        s_src.event_ms = cydlcd_millis();
        // Su kien lam thay doi du lieu -> xin lai cho chac
        switch ((cydlcd_event_t)s_src.event) {
          case CYDLCD_EV_PRINT_START:
          case CYDLCD_EV_PRINT_STOP:
          case CYDLCD_EV_MEDIA_INSERTED:
          case CYDLCD_EV_MEDIA_REMOVED:
            s_src.files_done = false; s_src.files_pending = true;
            break;
          case CYDLCD_EV_LEVELING_DONE:
            s_src.mesh_rows = 0;
            cydlcd_link_request(CYDLCD_REQ_MESH);
            break;
          case CYDLCD_EV_PID_DONE:
            // Marlin da chay xong M303 (v1 = result_t: 0 = thanh cong). UI doc moc nay de
            // hien hop "da xong" mot lan (so sanh voi lan truoc), khong hien lai mai.
            s_src.pid_done_ms = cydlcd_millis();
            break;
          case CYDLCD_EV_SETTINGS_LOADED:
          case CYDLCD_EV_FACTORY_RESET:
            s_src.n_settings = 0;
            cydlcd_link_request(CYDLCD_REQ_SETTINGS);
            cydlcd_link_request(CYDLCD_REQ_MESH);
            break;
          default: break;
        }
      }
      break;

    case CYDLCD_F_PROMPT:
      if (n >= 2) {
        s_src.prompt_id   = p[0];
        s_src.prompt_type = p[1];
        str_at(p, n, 2, s_src.prompt_text, sizeof(s_src.prompt_text));
        s_src.prompt_active = true;
      }
      break;

    case CYDLCD_F_PONG:
      s_pong_ms = cydlcd_millis();               // Marlin CON NHAN duoc lenh tu man hinh (xem s_pong_ms)
      break;

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
  s_src.rx_frames = true;
}

// ---------------------------------------------------------------------------
// API chung
// ---------------------------------------------------------------------------
void cydlcd_link_begin(const uint32_t baud) {
#if defined(CYDLCD_BOARD_CYD)
  // Tao KHOA trang thai TRUOC khi bat UART/task (task `link` publish ngay o vong dau).
  if (!s_state_mtx) s_state_mtx = xSemaphoreCreateMutex();
#endif
  CYDLCD_LINK_BEGIN(baud);
  cydlcd_rx_init(&s_rx);
  // Xoa CA ban goc (s_src) LAN ban cong khai (cydlcd): 2 CO s_src.rx_frames / s_src.rx_errors ve false.
  memset(&s_src, 0, sizeof(s_src));
  memset(&cydlcd, 0, sizeof(cydlcd));
  s_err_seen = 0;                      // anh bo dem loi cua cydlcd_protocol.h cung phai ve 0
  tbt_link_begin();                 // duong TRAMMING: reset trang thai (khong mo lai UART)
  s_want_full = true;
  s_next_ping_ms = 0;
  memset(s_rx_type, 0, sizeof(s_rx_type));   // chan doan: dem lai tu dau
  s_pong_ms = 0; s_pong_canh_bao_ms = 0; s_pong_lost = 0;
}

bool cydlcd_link_connected() { return s_src.linked; }

// YEU CAU tu task khac (khong duoc tu ghi vao trang thai): xoa danh sach file de xin lai tu dau.
// VI SAO: 6 cho ngoai task `link` (cyd_sd.cpp, ui_page_files.cpp) dang ghi thang `cydlcd.files_done=false;
// n_files=0;` - voi da luong thi do la ghi tran vao du lieu cua task khac (va se bi lan publish sau ghi de).
// Nay chi BAT CO (khong khoa, khong cho) roi task `link` tu lam o vong ke tiep.
static volatile bool s_reset_files_req = false;
void cydlcd_link_reset_files(void) { s_reset_files_req = true; }

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
    // frame hong (da gap that: byte tang ~500 B/s nhung s_src.linked=0, khong nhan duoc frame nao).
    // => Da nhan duoc frame PUSH hop le dau tien thi TAT sniff VINH VIEN. Dung CO MOT CHIEU
    //    s_src.rx_frames (KHONG dung bo dem nhu truoc: bo dem tran ve 0 se bat lai sniff giua chung).
    uint8_t n;
    if (s_src.rx_frames) { fwd[0] = c; n = 1; }
    else                 n = tbt_link_feed(c, fwd);
    for (uint8_t k = 0; k < n; k++)
      if (cydlcd_rx_byte(&s_rx, fwd[k])) handle_frame();
  }

  // BAT CO LOI MOT CHIEU: cydlcd_protocol.h tang `errors` khi CRC sai hoac payload qua dai
  // (cydlcd_rx_byte), nhung bo dem do la uint16_t nen khong dung lam tong duoc. Ta chi can biet
  // "da tung co loi" -> so sanh voi gia tri lan truoc, khac la bat co (tran cung khong sai).
  if (s_rx.errors != s_err_seen) { s_err_seen = s_rx.errors; s_src.rx_errors = true; }

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

  if (s_src.linked && (now - s_src.last_rx_ms) > CYDLCD_LINK_TIMEOUT_MS) {
    s_src.linked = false;
    s_want_full = true;                        // khi noi lai thi xin lai tu dau
  }

  // PING 1s/lan: vua de kiem tra lien ket (PONG), vua de Marlin biet MAN HINH CYD con song.
  // 2026-10-15 (sua loi "bam nut tren man hinh ma may khong chay" + "bed mesh mat du lieu"): TRUOC DAY
  // KHONG gui PING khi `tbt_link_active()` (= da nhan dong TBT dau tien) voi ly do "byte nhi phan cua PING
  // co the lot vao bo giai ASCII cua Marlin (che do CYDLCD_TBT_LINK)". Nhung che do PUSH (dang dung) gui
  // dong TBT trong FRAME nhi phan (CYDLCD_F_TBT) -> `tbt.ever` = true NGAY tu frame dau => PING bi tat VINH
  // VIEN. Hau qua: Marlin khong bao gio biet man hinh con song, va man hinh khong bao gio biet Marlin co nhan
  // duoc lenh khong (ben Marlin ket RX ma khong ai bao - xem pong_task()).
  // Nay chi BO PING khi THAT SU dang o che do TRAM ASCII tho: da nhan dong TBT MA CHUA bao gio nhan duoc
  // frame PUSH hop le nao (`s_src.rx_frames` = false). Che do PUSH: nhan frame dau tien la PING bat lai.
  if (!(tbt_link_active() && !s_src.rx_frames) && (!s_next_ping_ms || now >= s_next_ping_ms)) {
    s_next_ping_ms = now + CYDLCD_LINK_PING_MS;
    cydlcd_link_ping();
  }

  if (s_want_full && s_src.linked) {
    s_want_full = false;
    s_src.files_done = false;
    s_src.n_files = 0;
    cydlcd_link_request(CYDLCD_REQ_FULL);
    cydlcd_link_request(CYDLCD_REQ_FILES);
  }

  // ---------------------------------------------------------------------------
  // RESYNC KHI "CO LINK NHUNG CHUA TUNG NHAN HELLO" - loi that 2026-10-15 (nguoi dung: "bed mesh mat het
  // du lieu roi, vao thay trang").
  // VI SAO: nap firmware PANEL TRUOC roi moi nap Marlin -> luc Marlin khoi dong va gui HELLO thi panel dang
  // ban (hoac vua boot) nen BO LO. Sau do frame STATUS den deu -> `linked = true` NGAY => nhanh
  // `s_want_full` (chi bat khi MAT link) khong chay => panel khong bao gio biet `hello_flags` (co SD/mesh/
  // probe) => trang BED MESH bao "khong ho tro" VA **khong xin lai luoi** (luoi 0x0) du Marlin van giu
  // nguyen mesh trong EEPROM/slot (do lai bang `M420 V`: 7x7 gia tri that, slot 0).
  // 2026-10-15 (lan 2): ban dau chi xin lai **5 lan** roi THOI -> do la duong "mat mesh vinh vien": chi can
  // 5 lan do truot (Marlin dang ban doc the SD -> frame REQUEST bi HOAN roi BO, xem sd_defer() ben Marlin)
  // la panel het co hoi xin lai. Nay XIN MAI: 2 s/lan trong 10 s dau, sau do 10 s/lan (co dem + log thua dan),
  // va khi MAT link thi bo dem ve 0. Chi khi nao nhan duoc HELLO moi thoi.
  // ---------------------------------------------------------------------------
  if (s_src.linked && !s_src.hello_flags) {
    const uint32_t gap = (s_nohello_req < 6) ? 2000u : 10000u;
    if (!s_nohello_req) { s_nohello_req = 1; s_nohello_ms = now; }
    else if ((uint32_t)(now - s_nohello_ms) >= gap) {
      const uint16_t truoc = s_nohello_req;
      s_nohello_ms = now;
      s_nohello_req++;                                  // dem 8-bit -> tran: coi nhu 1 (khong bao gio "thoi xin")
      if (s_nohello_req == 0) s_nohello_req = 1;
      // Log thua dan: 5 lan dau in het, sau do chi in moi 6 lan (1 phut/lan) cho khoi ngap log.
      if (truoc <= 5 || (truoc % 6) == 0)
        SDLOG("[UP] co link nhung CHUA co HELLO -> xin lai FULL (lan %u, cach %lu ms)\n",
              (unsigned)truoc, (unsigned long)gap);
      cydlcd_link_request((uint8_t)CYDLCD_REQ_FULL);
    }
  }
  else if (!s_src.linked) s_nohello_req = 0;

  // ---------------------------------------------------------------------------
  // CANH BAO "MARLIN KHONG TRA LOI" - do PONG (loi that 2026-10-15: lenh tu man hinh khong chay).
  // PING gui 1 s/lan, Marlin tra PONG ngay (khong qua hang doi) nen binh thuong `s_pong_ms` luon moi.
  // Qua 5 s khong co PONG trong khi VAN nhan duoc frame tu Marlin (`linked`) = chieu man hinh -> Marlin
  // hong (RX cua Marlin ket, day sai chan, ...): bao ra log (khong the bao tren man hinh vi man hinh
  // khong co cho hien). Mat link han thi khong bao (khong phai loi PONG).
  // ---------------------------------------------------------------------------
  if (s_src.linked) {
    if (!s_pong_ms) s_pong_ms = now;                    // chua tung co PONG -> bat dau dem tu day
    if ((uint32_t)(now - s_pong_ms) > 5000u) {
      if (!s_pong_canh_bao_ms || (uint32_t)(now - s_pong_canh_bao_ms) >= 10000u) {
        s_pong_canh_bao_ms = now;
        s_pong_lost++;
        SDLOG("[UP] CANH BAO: khong nhan duoc PONG tu Marlin > 5 s (van co frame trang thai) -> "
              "LENH GUI TU MAN HINH CO THE KHONG CHAY. Kiem tra day TX cua man hinh -> RX cua mainboard "
              "hoac reset mainboard (lan %u).\n", (unsigned)s_pong_lost);
      }
    }
    else { s_pong_canh_bao_ms = 0; }
  }
  else { s_pong_ms = 0; s_pong_canh_bao_ms = 0; }

  // Trai hang doi TX (frame chua gui duoc vi TX ring day). Goi o CUOI: cac frame sinh ra o tren
  // (PING / REQUEST) van duoc trai ngay trong vong lap nay neu con cho.
  cydlcd_link_tx_task();

  // YEU CAU TU TASK KHAC (khong duoc tu ghi vao trang thai): xoa danh sach file de xin lai tu dau.
  if (s_reset_files_req) {
    s_reset_files_req = false;
    s_src.files_done = false;
    s_src.n_files    = 0;
  }

  // CONG BO trang thai ra ban `cydlcd` cho moi task khac doc (KHONG BAO GIO cho khoa - xem state_publish).
  state_publish();
}

// ---------------------------------------------------------------------------
// DA LUONG: task `link` - chay MOT MINH tren core 1, uu tien cao nhat cua firmware.
// ---------------------------------------------------------------------------
// VI SAO TACH: `loop()` (core 0) con phai lam WiFi/WebServer (upload 20-30 s), the TF (mount 2 s), LVGL
// (ve toi 76 ms/khung) va log (UART0 60 ms moi dong) - trong 1 luong thi MOI thu do deu chan duong serial
// (do duoc `link gap max 63 549 us`). Tach ra + core rieng => du man hinh lam gi cung khong dung toi.
// Vong lap: `vTaskDelay(1)` (1 tick = 1 ms) -> phuc vu UART moi 1 ms; ring RX 1024 B du ~41 ms @250 kbaud
// nen khong bao gio mat byte. Uu tien 20 (> moi task khac cua firmware: ui 5, net 4, log 2, diag 1).
#if defined(CYDLCD_BOARD_CYD)
static void link_task_fn(void *) {
  uint32_t prev_end_us = 0;
  for (;;) {
    const uint32_t t0 = micros();
    cydlcd_link_task();
    const uint32_t dur = (uint32_t)(micros() - t0);
    const uint32_t now_us = micros();
    // DO DO TRE DUONG SERIAL (tieu chi nghiem thu cua ke hoach da luong): `gap` = tu luc vong truoc KET THUC
    // den luc vong nay BAT DAU = 1 tick (vTaskDelay(1)) + thoi gian bi task khac chiem CPU. Voi core 1 chi co
    // task nay thi gap phai ~1-2 ms (tieu chi: < 50 ms).
    ui_lvgl_perf_link_call(prev_end_us ? (uint32_t)(now_us - prev_end_us) : 0, dur);
    prev_end_us = now_us;
    g_hb_link++;                        // heartbeat cho task chan doan (xem main.cpp: g_hb_*)
    vTaskDelay(1);
  }
}

bool cydlcd_link_start_task(void) {
  if (!s_state_mtx) s_state_mtx = xSemaphoreCreateMutex();
  if (!s_state_mtx) { SDLOG("[UP] ERR: khong tao duoc khoa trang thai\n"); return false; }
  // Core 1 (APP_CPU), uu tien 20, stack 4096: task nay chi parse frame + tra loi sector, khong ve/khong WiFi.
  const BaseType_t ok = xTaskCreatePinnedToCore(link_task_fn, "link", 4096, nullptr, 20, nullptr, 1);
  if (ok != pdPASS) { SDLOG("[UP] ERR: khong tao duoc task link\n"); return false; }
  // Cho task `link` chay 1 vong dau (state_publish dau tien) truoc khi vong lap chinh doc trang thai.
  vTaskDelay(pdMS_TO_TICKS(5));
  return true;
}
#endif
