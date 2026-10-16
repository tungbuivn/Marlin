/**
 * sim_main.cpp -- chuong trinh chay ban GIA LAP UI (man hinh cydlcd) TREN PC, khong can board.
 *
 * Cach chay (xem cydlcd/sim/README.md):
 *     pwsh cydlcd/tools/run_sim.ps1                 ; build + chay kich ban MAC DINH (chup moi trang)
 *     pwsh cydlcd/tools/run_sim.ps1 -Script <file>  ; chay kich ban tu file
 *
 * Kich ban la file text, moi dong 1 lenh:
 *     state basic|offline|home   ; nap du lieu gia (nhiet, vi tri, mesh, file, thong so...)
 *     page STATUS|MENU|JOG|...   ; mo trang (dung dung ui_lvgl_show_page_name cua firmware)
 *     hit <k>                    ; "bam" nut thu k cua trang (di dung duong su kien that)
 *     tap <x> <y>                ; cham vao toa do (di qua indev -> nhu cham tay)
 *     settle <ms>                ; cho dong ho AO chay (LVGL ve xong)
 *     shot <ten.bmp>             ; ghi anh man hinh hien tai
 *     echo <chu>                 ; in ra console
 *     # ...                      ; chu thich
 *
 * VI SAO DUNG UI THAT: ban gia lap KHONG ve lai UI - no build dung cac file `src/ui_*.cpp` cua
 * firmware (xem tools/run_sim.ps1) nen bo cuc/mau/font/menu la THAT. Chi lop phan cung (man hinh,
 * cam ung, the SD, WiFi) duoc thay bang stub (sim_port.cpp / sim_stubs.cpp).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <direct.h>

#include "lvgl.h"
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"
#include "cydlcd_link.h"
#include "cydlcd_protocol.h"
#include "ui_colortest.h"       // bang mau cho `colortest`/`checkcolor`
#include "sim.h"

// ===========================================================================
//  Trang thai MAY GIA (nap vao `cydlcd` - ban cong khai ma moi trang UI doc)
// ===========================================================================
static void state_basic(void) {
  memset(&cydlcd, 0, sizeof(cydlcd));
  cydlcd.linked = true;
  cydlcd.proto = CYDLCD_PROTO_VERSION;
  cydlcd.hello_flags = CYDLCD_HAS_SD | CYDLCD_HAS_MESH | CYDLCD_HAS_PROBE | CYDLCD_HAS_BED | CYDLCD_HAS_TMC;
  cydlcd.marlin_fw = 0x0102;
  cydlcd.n_heaters = 1;
  cydlcd.n_fans_max = 3;
  strcpy(cydlcd.name, "Voron 300");
  strcpy(cydlcd.marlin_ver, "2.1.2");
  cydlcd.state = CYDLCD_ST_PRINTING;
  cydlcd.flags = CYDLCD_SF_HOMED | CYDLCD_SF_STEPPERS | CYDLCD_SF_MEDIA | CYDLCD_SF_SD_PRINTING
               | CYDLCD_SF_LEVELING | CYDLCD_SF_MESH_VALID;
  cydlcd.progress_pct = 43;

  // nhiet: CYDLCD_TEMP_E0 = 1, CYDLCD_TEMP_BED = 0 (don vi do C x100)
  cydlcd.n_temps = 2;
  cydlcd.temps[0].id = CYDLCD_TEMP_E0;  cydlcd.temps[0].cur = 21050; cydlcd.temps[0].tgt = 21000;
  cydlcd.temps[1].id = CYDLCD_TEMP_BED; cydlcd.temps[1].cur =  6012; cydlcd.temps[1].tgt =  6000;
  cydlcd.n_fans = 1;
  cydlcd.fans[0].idx = 0; cydlcd.fans[0].pct = 128;

  // vi tri (mm x1000) - co y = 285,98 de kiem tra "so co de len nhan X/Y/Z khong"
  cydlcd.pos.x = 285980; cydlcd.pos.y = 12340; cydlcd.pos.z = 5600; cydlcd.pos.e = 12345;
  cydlcd.pos.feed_pct = 100; cydlcd.pos.flow_pct = 95;

  cydlcd.prog.pct = 43; cydlcd.prog.elapsed_s = 3725; cydlcd.prog.remaining_s = 4210;
  strcpy(cydlcd.prog.file, "V300_CUBE.GCO");

  // luoi UBL 7x7 (don vi: micromet, giong frame CYDLCD_F_MESH)
  cydlcd.mesh_nx = cydlcd.mesh_ny = 7; cydlcd.mesh_rows = 7;
  cydlcd.mesh_valid = true; cydlcd.mesh_on = true;
  for (uint8_t r = 0; r < 7; r++)
    for (uint8_t c = 0; c < 7; c++)
      cydlcd.mesh[r][c] = (int16_t)(((int)r - 3) * 12 + ((int)c - 3) * 7);

  // danh sach file SD
  cydlcd.n_files = 5;
  strcpy(cydlcd.files[0].name, "PROJECTS");     strcpy(cydlcd.files[0].sfn, "PROJECTS");  cydlcd.files[0].flags = 0x01;
  strcpy(cydlcd.files[1].name, "V300_CUBE.GCO"); strcpy(cydlcd.files[1].sfn, "V300_C~1.GCO"); cydlcd.files[1].flags = 0;
  strcpy(cydlcd.files[2].name, "BENCHY_60MM.GCO"); strcpy(cydlcd.files[2].sfn, "BENCHY~1.GCO"); cydlcd.files[2].flags = 0;
  strcpy(cydlcd.files[3].name, "cydlcd_tmp");   strcpy(cydlcd.files[3].sfn, "CYDLCD~1");   cydlcd.files[3].flags = 0x01;
  strcpy(cydlcd.files[4].name, "CALI_FLOW.GCO"); strcpy(cydlcd.files[4].sfn, "CALI_F~1.GCO"); cydlcd.files[4].flags = 0;
  cydlcd.files_done = true;

  // endstop (kieu M119): may nay bao PROBE + X min + Y min
  cydlcd.n_endstops = 3;
  cydlcd.es_present = (uint8_t)((1u << CYDLCD_ES_PROBE) | (1u << CYDLCD_ES_X_MIN) | (1u << CYDLCD_ES_Y_MIN));
  cydlcd.es_hit[CYDLCD_ES_X_MIN] = 1;

  // bang thong so (id -> gia tri THO, thang do theo tung dong - xem ui_page_settings.cpp SET_ROWS)
  struct { uint16_t id; int32_t v; } S[] = {
    { CYDLCD_SET_Z_OFFSET, 750 },   { CYDLCD_SET_FEEDRATE_PCT, 100 }, { CYDLCD_SET_FLOW_PCT, 95 },
    { CYDLCD_SET_STEPS_MM_X, 80 },  { CYDLCD_SET_STEPS_MM_Y, 80 },    { CYDLCD_SET_STEPS_MM_Z, 400 },
    { CYDLCD_SET_STEPS_MM_E, 420 }, { CYDLCD_SET_MAXFEED_X, 300 },    { CYDLCD_SET_MAXFEED_Y, 300 },
    { CYDLCD_SET_MAXFEED_Z, 10 },   { CYDLCD_SET_MAXFEED_E, 120 },    { CYDLCD_SET_MAXACCEL_X, 3000 },
    { CYDLCD_SET_MAXACCEL_Y, 3000 },{ CYDLCD_SET_MAXACCEL_Z, 100 },   { CYDLCD_SET_MAXACCEL_E, 5000 },
    { CYDLCD_SET_JERK_X, 10 },      { CYDLCD_SET_JERK_Y, 10 },        { CYDLCD_SET_JERK_Z, 4 },
    { CYDLCD_SET_JERK_E, 5 },       { CYDLCD_SET_PID_E_KP, 2150 },    { CYDLCD_SET_PID_E_KI, 320 },
    { CYDLCD_SET_PID_E_KD, 3600 },  { CYDLCD_SET_LINEAR_ADV_E, 50 },  { CYDLCD_SET_PROBE_OFFSET_X, 0 },
    { CYDLCD_SET_PROBE_OFFSET_Y, 25000 }, { CYDLCD_SET_PROBE_OFFSET_Z, 750 }, { CYDLCD_SET_TARGET_TEMP_E0, 210 },
    { CYDLCD_SET_TARGET_TEMP_BED, 60 },   { CYDLCD_SET_FAN_PCT_E0, 50 },
    { CYDLCD_SET_TMC_CURRENT_X, 600 }, { CYDLCD_SET_TMC_CURRENT_Y, 600 }, { CYDLCD_SET_TMC_CURRENT_Z, 600 },
    { CYDLCD_SET_TMC_CURRENT_E, 600 }, { CYDLCD_SET_TMC_CURRENT_Z2, 600 }, { CYDLCD_SET_TMC_CURRENT_Z3, 600 },
    { CYDLCD_SET_RUNOUT_EN, 1 },    { CYDLCD_SET_FADE_HEIGHT, 940 },
    { CYDLCD_SET_MIN_FEEDRATE, 5 }, { CYDLCD_SET_MIN_TRAVEL_FEEDRATE, 10 },
    { CYDLCD_SET_HOME_OFFSET_X, 0 },{ CYDLCD_SET_HOME_OFFSET_Y, 0 },
    { CYDLCD_SET_FILAMENT_DIA, 1750 }, { CYDLCD_SET_FILAMENT_VOL, 0 },
    { CYDLCD_SET_FC_LOAD, 0 },      { CYDLCD_SET_FC_UNLOAD, 0 },
  };
  for (unsigned i = 0; i < sizeof(S) / sizeof(S[0]) && cydlcd.n_settings < CYDLCD_SETTINGS_MAX; i++) {
    cydlcd.settings[cydlcd.n_settings].id = S[i].id;
    cydlcd.settings[cydlcd.n_settings].value = S[i].v;
    cydlcd.n_settings++;
  }

  strcpy(cydlcd.message, "Sap het filament (M412)");
  cydlcd.message_sev = CYDLCD_SEV_WARN;
  cydlcd.message_ms = sim_millis();
}

// Trang thai "khong co link" (de xem man hinh bao NO LINK / LINK ERR).
static void state_offline(void) {
  memset(&cydlcd, 0, sizeof(cydlcd));
  cydlcd.linked = false;
  cydlcd.state = CYDLCD_ST_IDLE;
  strcpy(cydlcd.name, "Voron 300");
}

// Trang thai "dang home" (dung cho man JOG/STATUS khi can doi trang thai).
static void state_home(void) {
  state_basic();
  cydlcd.state = CYDLCD_ST_HOMING;
  cydlcd.flags = CYDLCD_SF_STEPPERS;
  cydlcd.progress_pct = 0;
  cydlcd.n_files = 0;
  cydlcd.files_done = true;
}

// ===========================================================================
//  Kich ban
// ===========================================================================
static int  s_fails = 0;                        // so kiem tra TU DONG that bai (exit code = so nay)
char        g_sim_outdir[512] = "cydlcd/sim/out";   // xem sim.h (sim_port.cpp can de tu ghi anh COLORTEST)

static void out_path(char *dst, size_t n, const char *name) {
  snprintf(dst, n, "%s/%s", g_sim_outdir, name);
}

// Ten file an toan: 'FEED/FLOW' -> 'FEED-FLOW', 'NHIET DO' -> 'NHIET-DO'
static void safe_name(char *dst, size_t n, const char *s) {
  size_t i = 0;
  for (; s[i] && i + 1 < n; i++) {
    const char c = s[i];
    dst[i] = (c == '/' || c == '\\' || c == ' ' || c == ':') ? '-' : c;
  }
  dst[i] = '\0';
}

static void do_shot(const char *name) {
  char p[640];
  out_path(p, sizeof(p), name);
  if (sim_dump_bmp(p)) {
    // Dem pixel "co muc" de biet trang CO VE GI khong (trang trang = loi ve/loi du lieu).
    const uint16_t bg = sim_bg_color();
    const int ink = sim_ink_count(0, 0, 319, 239, bg);
    printf("[SIM] anh: %s | pixel co muc: %d%s\n", p, ink, (ink < 500) ? "  <-- CANH BAO: gan nhu TRANG!" : "");
    if (ink < 500) s_fails++;
  }
}

// ---------------------------------------------------------------------------
//  KIEM TRA TU DONG hang X/Y/Z cua man STATUS: nhan truc va SO khong duoc dinh vao nhau.
//
//  Day la HOI QUY cho loi nguoi dung bao 2026-10-16: "tai truc X 285.98 thi so 2 de vao nhan X"
//  (nhan dat tai x=LX, gia tri can phai trong hop [LX..LX+VW] => hop bat dau NGAY tai nhan; font
//  Montserrat 16 rong hon font cua man 12864 nen so de len nhan).
//  Cach kiem: doc framebuffer, tach cac "nhom cot co muc" trong dai y cua hang XYZ. Nhan 'X' la nhom
//  thu nhat, so la nhom thu hai; KHOANG TRONG giua 2 nhom phai >= 3 px.
// ---------------------------------------------------------------------------
static void check_xyz(const char *which) {
  const uint16_t bg = sim_bg_color();
  const int y0 = MR_XYZ_Y + 3, y1 = MR_XYZ_Y + MR_XYZ_H - 3;
  printf("[SIM] --- kiem hang XYZ (%s) | dai y=%d..%d ---\n", which ? which : "", y0, y1);
  for (uint8_t i = 0; i < 3; i++) {
    const int x0 = MR_XYZ_X + MR_XYZ_LX(i);
    const int x1 = MR_XYZ_X + MR_XYZ_VR(i);
    int st[8], en[8];
    const int n = sim_ink_runs(x0, y0, x1, y1, 3, st, en, 8);
    printf("[SIM]   %c: vung x=%d..%d | %d nhom:", "XYZ"[i], x0, x1, n);
    for (int k = 0; k < n && k < 8; k++) printf(" [%d..%d]", st[k], en[k]);
    if (n >= 2) {
      const int gap = st[1] - en[0] - 1;
      printf(" | khe nhan<->so = %d px %s\n", gap, (gap >= 3) ? "OK" : "<-- DE NHAU!");
      if (gap < 3) s_fails++;
    }
    else {
      // 1 nhom = nhan va so DINH LIEN nhau (chinh la loi), 0 nhom = khong ve gi.
      printf(" | %s\n", (n == 0) ? "<-- KHONG VE GI (loi)" : "<-- nhan va so DINH LIEN (loi!)");
      s_fails++;
    }
    (void)bg;
  }
}

// ---------------------------------------------------------------------------
//  KIEM TRA DUONG MAU (lenh kich ban `checkcolor`)
//
//  Doc lai CHINH cac file .bmp ma `colortest` vua ghi (o giua man hinh) roi so voi #RRGGBB trong bang
//  mau. Day la bang chung: duong "UI -> LVGL RGB565 -> bo ghi BMP" la DUNG, nen neu man hinh THAT khac
//  thi loi nam o panel/TFT_eSPI (`TFT_RGB_ORDER` hoac dao mau), khong phai o code UI.
//  Sai so cho phep 8/kenh: RGB565 chi co 5-6 bit/kenh nen mau #1E156E luu xuong 565 roi doi nguoc se
//  lech vai don vi (do la BAN CHAT cua 565, khong phai loi).
// ---------------------------------------------------------------------------
static void check_color(void) {
  const int cx = UI_LCD_HOR_RES / 2, cy = UI_LCD_VER_RES / 2;
  printf("[SIM] --- kiem duong mau (%u mau, doc lai tu file .bmp) ---\n", (unsigned)UI_COLORTEST_N);
  for (uint8_t i = 0; i < UI_COLORTEST_N; i++) {
    char path[640];
    snprintf(path, sizeof(path), "%s/colortest-%u-%s.bmp", g_sim_outdir, (unsigned)i, ui_colortest_bars[i].name);
    int r = -1, g = -1, b = -1;
    const uint32_t want = ui_colortest_bars[i].rgb888;
    const int wr = (want >> 16) & 0xFF, wg = (want >> 8) & 0xFF, wb = want & 0xFF;
    if (!sim_bmp_get(path, cx, cy, &r, &g, &b)) {
      printf("[SIM]   %-7s <-- KHONG doc duoc anh %s\n", ui_colortest_bars[i].name, path);
      s_fails++;
      continue;
    }
    const bool ok = (abs(r - wr) <= 8) && (abs(g - wg) <= 8) && (abs(b - wb) <= 8);
    printf("[SIM]   %-7s trong anh = #%02X%02X%02X | phai la #%06lX | %s\n",
           ui_colortest_bars[i].name, r, g, b, (unsigned long)want, ok ? "OK" : "<-- SAI!");
    if (!ok) s_fails++;
  }
  printf("[SIM]   (neu cac dong tren deu OK thi duong mau cua UI + bo ghi BMP la DUNG)\n");
}

static void run_line(char *line) {
  char *hash = strchr(line, '#');                 // chu thich cuoi dong
  if (hash) *hash = '\0';
  while (*line == ' ' || *line == '\t') line++;
  const size_t len = strlen(line);
  while (len && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' ')) line[len - 1] = '\0';
  if (!*line) return;

  char cmd[32] = {0}, a1[256] = {0}, a2[64] = {0};
  sscanf(line, "%31s %255s %63s", cmd, a1, a2);

  if (!strcmp(cmd, "state")) {
    if (!strcmp(a1, "offline"))      state_offline();
    else if (!strcmp(a1, "home"))    state_home();
    // `state paused`: dang in tu the SD NHUNG da TAM DUNG -> de kiem tra nut PAUSE doi thanh RESUME o
    // man STATUS (xem page_status_update()). `state`/`basic` = dang in binh thuong.
    else if (!strcmp(a1, "paused")) { state_basic(); cydlcd.flags |= CYDLCD_SF_PAUSED; }
    else                             state_basic();
    printf("[SIM] state = %s\n", a1[0] ? a1 : "basic");
  }
  else if (!strcmp(cmd, "page"))  { ui_lvgl_show_page_name(a1); }
  else if (!strcmp(cmd, "hit"))   { const int k = atoi(a1); printf("[SIM] hit %d\n", k); ui_lvgl_test_hit(a1); }
  else if (!strcmp(cmd, "tap"))   { sim_tap(atoi(a1), atoi(a2)); printf("[SIM] tap %d,%d\n", atoi(a1), atoi(a2)); }
  else if (!strcmp(cmd, "settle")){ sim_settle((uint32_t)atoi(a1)); }
  else if (!strcmp(cmd, "shot"))  { do_shot(a1); }
  // `fan <pct>`: dat % QUAT gia (cydlcd.fans[0].pct = pct * 100). Dung de kiem tra VONG QUAY cua icon
  // quat o trang STATUS (toc do quay ti le voi %) — xem FAN_FRAME_MS_MIN/MAX o ui_page_status.cpp.
  // Cach kiem: shot lien tiep moi 150 ms (dung nhip UI_REFRESH_MS), dem so lan ANH DOI -> ra chu ky
  // doi khung, doi chieu voi cong thuc step = FAN_FRAME_MS_MAX - (MAX-MIN)*pct/100.
  else if (!strcmp(cmd, "fan")) {
    cydlcd.n_fans = 1;
    cydlcd.fans[0].idx = 0;
    cydlcd.fans[0].pct = (int16_t)(atoi(a1) * 100);
    printf("[SIM] fan = %s%%\n", a1);
  }
  // `alert <sev> <chu>`: bom 1 thong bao voi MUC DO bat ky. Gia tri THUC cua enum (cydlcd_protocol.h):
  //   0 = INFO, 1 = WARN, 2 = ERROR, 3 = FATAL   (KHONG co NOTICE; ngoai dai -> hien "THONG BAO").
  // Dung de kiem tra CYD xu ly tung muc the nao - dac biet 4 = FATAL, dung nhu Marlin goi
  // onPrinterKilled() -> push_message(CYDLCD_SEV_FATAL, ...) khi may bi halt.
  // Xem ui_boxes.cpp: hien tai MOI muc deu di vao trang /status (CYDLCD_MSG_BOX_ON_LCD = 0).
  else if (!strcmp(cmd, "alert")) {
    cydlcd.message_sev = (uint8_t)atoi(a1);
    // Lay PHAN CON LAI cua dong lam chu (co khoang trang) chu KHONG dung a2 (sscanf chi lay 1 tu).
    const char *p = line;
    for (uint8_t sp = 0; *p && sp < 2; p++) if (*p == ' ') sp++;
    if (*p == '"') p++;                       // bo dau ngoac kep neu co
    size_t n = strlen(p);
    if (n && p[n - 1] == '"') n--;            // va bo dau dong o cuoi
    snprintf(cydlcd.message, sizeof(cydlcd.message), "%.*s", (int)n, p);
    cydlcd.message_ms = sim_millis();
    printf("[SIM] alert sev=%s : \"%s\"\n", a1, cydlcd.message);
  }
  else if (!strcmp(cmd, "checkxyz")) { check_xyz(a1); }
  else if (!strcmp(cmd, "colortest")) { ui_port_colortest_raw(); }
  else if (!strcmp(cmd, "checkcolor")) { check_color(); }
  // `pos <x_mm> <y_mm> <z_mm>`: doi toa do gia (de thu so DAI, vd -1234.56 - xem scenario test-xyz-dai.txt)
  else if (!strcmp(cmd, "pos")) {
    char a3[64] = {0};
    sscanf(line, "%*s %255s %63s %63s", a1, a2, a3);
    cydlcd.pos.x = (int32_t)(atof(a1) * 1000.0);
    cydlcd.pos.y = (int32_t)(atof(a2) * 1000.0);
    cydlcd.pos.z = (int32_t)(atof(a3) * 1000.0);
    printf("[SIM] pos = %s / %s / %s mm\n", a1, a2, a3);
  }
  else if (!strcmp(cmd, "echo"))  { printf("[SIM] %s\n", a1); }
  else if (!strcmp(cmd, "ui"))    { ui_console_dispatch(line + 3); }
  else printf("[SIM] lenh khong hieu: %s\n", line);
}

// Kich ban MAC DINH: chup anh TAT CA cac trang (de doi chieu bo cuc).
static void run_default(void) {
  static const char *const PAGES[] = {
    "STATUS", "MENU", "FEED/FLOW", "NHIET DO", "JOG", "FILE SD", "THONG SO",
    "BED MESH", "CUSTOM", "TRAMMING", "INFO", "ENDSTOP", "WIFI", "CAU HINH"
  };
  const unsigned n = (unsigned)(sizeof(PAGES) / sizeof(PAGES[0]));
  for (unsigned i = 0; i < n; i++) {
    char file[64], safe[32];
    safe_name(safe, sizeof(safe), PAGES[i]);
    snprintf(file, sizeof(file), "%02u-%s.bmp", i, safe);
    ui_lvgl_show_page_name(PAGES[i]);
    sim_settle(400);
    do_shot(file);
    if (!strcmp(PAGES[i], "STATUS")) check_xyz("trang STATUS");
  }
  // Vai buoc TUONG TAC de kiem tra lop phu/hop thoai (nhu bam tay).
  ui_lvgl_show_page_name("MENU");
  sim_settle(200);
  printf("[SIM] --- thu bam dong menu ---\n");
  ui_lvgl_test_hit("1");
  sim_settle(300);
  do_shot("90-menu-hit1.bmp");
  ui_lvgl_show_page_name("STATUS");
  sim_settle(300);
  do_shot("91-status-cuoi.bmp");
  check_xyz("trang STATUS (cuoi)");
}

int main(int argc, char **argv) {
  const char *script = nullptr;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--out") && i + 1 < argc)       { snprintf(g_sim_outdir, sizeof(g_sim_outdir), "%s", argv[++i]); }
    else if (!strcmp(argv[i], "--script") && i + 1 < argc){ script = argv[++i]; }
    else if (argv[i][0] != '-')                           { script = argv[i]; }
  }
  _mkdir(g_sim_outdir);
  printf("[SIM] ban gia lap UI cydlcd | thu muc anh: %s\n", g_sim_outdir);

  state_basic();
  ui_lvgl_begin();          // tao LVGL + chrome + mo trang STATUS (code THAT cua firmware)
  state_basic();            // nap lai du lieu gia (begin() da goi preheat_load/ui_lang_load)
  sim_settle(300);

  if (script) {
    FILE *f = fopen(script, "r");
    if (!f) { printf("[SIM] KHONG mo duoc kich ban '%s'\n", script); return 2; }
    char line[512];
    while (fgets(line, sizeof(line), f)) run_line(line);
    fclose(f);
  }
  else run_default();

  printf("[SIM] xong. Kiem tra tu dong that bai: %d\n", s_fails);
  return s_fails;
}
