/**
 * ui_page_files.cpp -- man FILE SD cua UI LVGL (man hinh CYD): chon file tren the de in,
 * xoa file, lam moi danh sach.
 *
 * Tach tu ui_lvgl.cpp (chang 3/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 * Gom:
 *   - "viec cham" cua man FILE (init lai the SD sau khi module khac ghi the, xoa file/thu muc qua
 *     net_sd_delete_file / net_sd_delete_dir) - chay o nhip 400 ms trong ui_lvgl_task() ke CA khi da
 *     roi man;
 *   - FILES_ROWS dong file (moi dong la 1 NUT: cham = chon + to sang) + 3 hang nut
 *     (PRINT/DIR/UP/REFRESH, PAUSE/RESUME/STOP, PREV/XOA/NEXT) + dong info.
 *   - Nut `XOA` xoa CA file LAN thu muc: file -> FATFS cua man hinh (the o khe CYD) hoac M30;
 *     thu muc -> FATFS cua man hinh va CHI khi thu muc RONG (M30 cua Marlin khong xoa duoc thu muc).
 *
 * HOP DONG voi net_upload_http.cpp (2 ham `weak`): xem khoi chu thich ngay duoi day.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf
#include <stdlib.h>                 // atoi (lenh chan doan UI TAP)
#include <string.h>                 // strcmp, strncpy
#include <Arduino.h>                // Serial.printf (WIFILOG - xem ui_lvgl_int.h)

#if CYDLCD_USE_LVGL

// HOP DONG voi net_upload_http.cpp (duong upload qua WiFi): true = MAN HINH CYD DANG nhan/ghi file va
// dang GIU bus SPI cua the SD DUNG CHUNG voi Marlin. Trong luc do MOI lenh lam MARLIN doc the
// (M23/M24 = in file, M30 = xoa, M22/M21 = init lai the, xin danh sach file) PHAI bi chan: neu
// Marlin clock SPI cung luc MAN HINH CYD dang noi vao the thi du lieu doc ra se hong.
// PHAI `extern "C"` (xem canh bao o tren): khai bao khong co extern "C" thi ten bi mang hoa ->
// khong bao gio bind (ham luon NULL => khong chan duoc gi - sai am tham).
extern "C" bool net_upload_active(void) __attribute__((weak));
static bool sd_host_busy(void) { return net_upload_active && net_upload_active(); }

// HOP DONG voi net_upload_http.cpp: XOA 1 file NGAY TREN MAN HINH CYD (che do CYDLCD_SD_UART - the nam o
// khe man hinh, MAN HINH CYD giu the, con Marlin chay SDCARD_READONLY nen lenh M30 se bi tu choi).
// Tra true = da xoa xong; UI PHAI tu bao Marlin mount lai the (files_init_sd() -> M22/M21 qua UART).
// Vang mat (mode khac) -> con tro NULL -> UI roi ve duong M30 nhu cu.
extern "C" bool net_sd_delete_file(const char *name) __attribute__((weak));

// HOP DONG voi net_upload_http.cpp: XOA 1 THU MUC RONG + DEM MUC CON cua thu muc (xem net_upload_http.h).
// VI SAO KHONG DUNG M30 CHO THU MUC: `M30` -> CardReader::removeFile() -> SdBaseFile::remove() ->
// truncate(0), va ham nay KHONG xoa duoc thu muc (Marlin/src/sd/SdBaseFile.cpp: "Reasons for failure
// include the file read-only, is a directory"). The dang o khe man hinh CYD nen man hinh tu xoa bang
// FATFS (`remove()` -> rmdir, chi xoa duoc thu muc RONG).
extern "C" bool net_sd_delete_dir(const char *name, bool *not_empty) __attribute__((weak));
extern "C" int  net_sd_dir_count(const char *name) __attribute__((weak));

// ===========================================================================
//  FILES  (vung noi dung 206 px = CONT_H 214 - 2*PAGE_PAD 4)
//    info 17 (font mono mode 4: line_height = 17)
//    + khung danh sach FILES_ROWS dong: 5*18 + 4*GAP(2) = 98
//    + 3 hang nut 78 + 5*GAP 10 = 203 <= 206 (con 3 px cho vat dem GROW).
//  DAY MAN: 3 hang nut bi DAY XUONG DAY bang mk_spacer(grow) -> danh sach file nam gon o tren.
//  KHONG CUON: danh sach la FILES_ROWS NUT CO DINH, lat trang bang 2 nut PREV/NEXT o hang nut cuoi.
//
//  VI SAO BO ROLLER (yeu cau nguoi dung 2026-10-15: "danh sach file khong cuon nua, rat kho cuon,
//  hay dung phan trang next prev va cham vao file/thu muc nao thi highlight cho do"):
//   1) Cuon bang NGON TAY tren roller rat kho: phai keo dung cu ly moi doi duoc dong dang chon,
//      cham nhe thi khong an -> nguoi dung tuong man hinh hong.
//   2) LOI NANG HON: cham vao 1 dong cua roller KHONG lam `lv_roller_get_selected()` doi theo dong
//      vua cham (roller chi doi lua chon khi KEO roi thang lai). Vi vay `files_selected()` tra ve
//      dong DANG CHON CU -> bam XOA FILE tren 1 file lai di xoa/thu muc dang chon truoc do, gap
//      that: nguoi dung chon 1 file ma man hinh bao "Khong xoa thu muc".
//      Nay moi dong la 1 nut: cham vao dong nao la `s_sel` = dung dong do, va dong do duoc to sang.
// ===========================================================================
#define FILES_BTN_H   26                // cao 1 hang nut (PRINT... / PAUSE... / PREV...)
#define FILES_ROWS    5                 // so dong file trong 1 TRANG
#define FILES_ROW_H   18                // cao 1 dong file (= 1 nut chon; >= line_height 17 cua font)

static lv_obj_t *fl_info;                        // dong "SD: n file  T1/2" (hoac ghi chu tam thoi)
static lv_obj_t *fl_rows;                        // khung flex COLUMN chua FILES_ROWS dong file
static lv_obj_t *fl_row[FILES_ROWS];             // FILES_ROWS nut dong file
static lv_obj_t *fl_del;                         // nut XOA FILE (tam khoa trong luc dang xoa)
static lv_obj_t *fl_prev, *fl_next;              // lat trang danh sach file
static uint32_t  s_files_sig = 0;                // dau van tay cua danh sach da ve
static uint8_t   s_files_n   = 0;                // so file cua lan ve truoc
// Dong dang CHON: luu CHI SO TUYET DOI trong cydlcd.files[] (KHONG phai chi so trong trang) + TEN
// cua no. Ten dung de giu lua chon qua cac lan danh sach DOI (REFRESH / xoa xong): thu tu file do
// FAT/Marlin quyet dinh nen chi so cu co the tro sang file KHAC.
static uint8_t   s_sel = 0;
static uint8_t   s_pg  = 0;                      // trang dang xem (0-based)
static char      s_sel_name[CYDLCD_FILE_NAME_LEN];

// ---------------------------------------------------------------------------
//  Viec CHAM cua man FILE (init the SD + xoa file): chay o nhip 400 ms trong ui_lvgl_task(),
//  KHONG phu thuoc man dang mo (nguoi dung roi man van phai xong viec roi moi xin danh sach).
//
//  VI SAO PHAI CO DO TRE: lenh G-code (M22/M21, M30) di qua HANG DOI cua Marlin
//  (cydlcd_serial.cpp: queue.enqueue_one_now()), con frame REQUEST lai duoc Marlin xu ly NGAY
//  trong ham nhan frame (do_request()). Xin danh sach ngay sau khi gui lenh se nhan duoc danh
//  sach CU (file chua kip xoa / the chua kip init lai).
// ---------------------------------------------------------------------------
enum { FLJOB_NONE = 0, FLJOB_INIT, FLJOB_DELETE };
#define FILES_JOB_DELAY_MS 800U          // 500..1000 ms: du cho 2 lenh M22+M21 hoac 1 lenh M30
#define FILES_NOTE_MS     2500U          // ghi chu ngan (vd "Khong xoa thu muc") tu tat sau 2.5 s

static uint8_t     s_fl_job    = FLJOB_NONE;
static uint32_t    s_fl_job_ms = 0;
static const char *s_fl_note   = NULL;    // ghi chu tam thoi thay cho dong "SD: n file"
static uint32_t    s_fl_note_ms = 0;      // moc bat dau ghi chu TAM (0 = ghi chu cua viec cham)

// Ghi chu o dong info. temp = true -> tu tat sau FILES_NOTE_MS (bao loi),
// temp = false -> hien cho toi khi viec cham xong (trang thai dang lam).
static void files_note(const char *txt, const bool temp) {
  s_fl_note    = txt;
  s_fl_note_ms = temp ? lv_tick_get() : 0;
}

static void files_job_start(const uint8_t job) {
  s_fl_job    = job;
  s_fl_job_ms = lv_tick_get();
}

// So file THAT SU dung duoc (Marlin co the gui qua CYDLCD_MAX_FILES -> chan lai cho an toan)
static uint8_t files_count(void) {
  return (cydlcd.n_files > CYDLCD_MAX_FILES) ? (uint8_t)CYDLCD_MAX_FILES : cydlcd.n_files;
}

// Tong so TRANG danh sach (luon >= 1 de mau so cua "T x/y" khac 0)
static uint8_t files_npages(void) {
  const uint8_t n = files_count();
  const uint8_t p = (uint8_t)((n + FILES_ROWS - 1) / FILES_ROWS);
  return p ? p : 1;
}

// FNV-1a don gian tren ten+co cua danh sach file -> biet khi nao phai ve lai cac dong
static uint32_t files_signature(void) {
  uint32_t h = 2166136261UL;
  for (uint8_t i = 0; i < files_count(); i++) {
    for (const char *s = cydlcd.files[i].name; *s; s++) h = (h ^ (uint8_t)*s) * 16777619UL;
    h = (h ^ cydlcd.files[i].flags) * 16777619UL;
  }
  return h ^ ((uint32_t)cydlcd.n_files << 24);
}

// Chu cua 1 dong: "[D] ten" cho thu muc. Ten RONG (Marlin khong gui ten dai va cung khong lui ve
// ten 8.3 - ban cu): hien ro "(khong ten)" thay vi de 1 dong TRONG, vi dong trong trong nhu loi UI
// (gap that tren the nay: mot thu muc chi co ten 8.3). Xem cydlcd_serial.cpp: files_pump() da co
// duong lui 8.3.
static void files_row_text(const uint8_t i, char *b, const size_t n) {
  const char *nm = cydlcd.files[i].name;
  if (!nm[0]) nm = "(khong ten)";
  snprintf(b, n, "%s%s", (cydlcd.files[i].flags & 0x01U) ? "[D] " : "", nm);
}

// Khoa/mo 1 nut theo kieu "khong bam duoc": LV_STATE_DISABLED KHONG chi de to mo -
//   - lv_indev.c:412 chan luon su kien CHAM vao vat disabled,
//   - lv_group.c:434 bo qua vat disabled khi di chuyen focus (chan ca duong encoder),
// nen day la cach khoa THAT SU (khac han viec chi doi mau chu). Chi ghi khi trang thai DOI.
static void files_btn_enable(lv_obj_t *b, const bool on) {
  if (!b) return;
  if (lv_obj_has_state(b, LV_STATE_DISABLED) == !on) return;   // da dung trang thai -> thoi
  lv_obj_t *l = lv_obj_get_child(b, 0);
  if (on) lv_obj_clear_state(b, LV_STATE_DISABLED);
  else    lv_obj_add_state(b, LV_STATE_DISABLED);
  if (l) lv_obj_set_style_text_color(l, on ? COL_TXT : COL_DIM, 0);
}

// Khoa/mo 2 nut lat trang theo TRANG dang xem (goi ngay sau moi lan doi trang, khong doi nhip 400 ms
// cua page_files_update() - de nut khoa/mo dung luc nguoi dung vua bam).
static void files_nav_mark(void) {
  const uint8_t np = files_npages();
  files_btn_enable(fl_prev, s_pg > 0);
  files_btn_enable(fl_next, (uint8_t)(s_pg + 1) < np);
}

// To lai tung dong cua TRANG dang xem: dong nao ngoai danh sach -> AN; dong DANG CHON -> nen
// COL_ACCENT + chu dam (giong dong dang chon cua roller cu), con lai -> nen nut / chu thuong.
static void files_rows_mark(void) {
  const uint8_t n = files_count();
  for (uint8_t i = 0; i < FILES_ROWS; i++) {
    lv_obj_t *r = fl_row[i];
    if (!r) continue;                              // trang FILE da bi thu hoi
    const uint8_t idx = (uint8_t)(s_pg * FILES_ROWS + i);
    if (idx >= n) { lv_obj_add_flag(r, LV_OBJ_FLAG_HIDDEN); continue; }
    lv_obj_clear_flag(r, LV_OBJ_FLAG_HIDDEN);
    lv_obj_t *l = lv_obj_get_child(r, 0);
    if (!l) continue;
    char b[CYDLCD_FILE_NAME_LEN + 6];
    files_row_text(idx, b, sizeof(b));
    set_text(l, b);                                // set_text() tu bo qua khi chu khong doi
    const bool on = (idx == s_sel);
    lv_obj_set_style_bg_color(r, on ? COL_ACCENT : COL_BTN, 0);
    lv_obj_set_style_text_color(l, on ? lv_color_hex(0x102030) : COL_TXT, 0);
  }
}

// Ve lai danh sach tu dau: GIU LUA CHON THEO TEN (danh sach vua doi), dua con tro ve trang chua
// dong dang chon, khoa/mo 2 nut PREV/NEXT theo trang. Goi khi trang vua tao lai hoac danh sach DOI.
static void files_rows_reload(void) {
  if (!fl_rows) return;                            // trang FILE da bi thu hoi -> khong co dong nao
  const uint8_t n = files_count();
  const uint8_t old = s_sel;

  if (s_sel_name[0]) {
    uint8_t found = 0xFFU;
    for (uint8_t i = 0; i < n; i++)
      if (strcmp(cydlcd.files[i].name, s_sel_name) == 0) { found = i; break; }
    s_sel = (found != 0xFFU) ? found : 0;          // ten cu khong con (vua bi xoa) -> ve dong dau
  }
  if (!n) s_sel = 0;
  else if (s_sel >= n) s_sel = (uint8_t)(n - 1);
  s_pg = (uint8_t)(s_sel / FILES_ROWS);

  if (n) {
    strncpy(s_sel_name, cydlcd.files[s_sel].name, sizeof(s_sel_name) - 1);
    s_sel_name[sizeof(s_sel_name) - 1] = '\0';
  }
  else s_sel_name[0] = '\0';

  // Log khi danh sach DOI lam DONG DANG CHON doi theo (chi so cu -> chi so moi): de phan biet voi
  // 1 lan CHAM that (xem log trong cb_files_row).
  if (s_sel != old)
    WIFILOG("FILES: danh sach doi -> chon lai #%u <<%s>>\n", (unsigned)s_sel, s_sel_name);

  files_rows_mark();
  files_nav_mark();
}

// Lat 1 trang danh sach (dir > 0 = NEXT, dir < 0 = PREV). Dung lai o 2 dau.
static void files_page_move(const int8_t dir) {
  const uint8_t np = files_npages();
  int p = (int)s_pg + ((dir > 0) ? 1 : -1);
  if (p < 0) p = 0;
  if (p > (int)np - 1) p = (int)np - 1;
  if ((uint8_t)p == s_pg) return;                  // dang o dau/cuoi -> khong lam gi
  s_pg = (uint8_t)p;
  files_rows_mark();
  files_nav_mark();
  WIFILOG("FILES: trang %u/%u (dong dang chon %u/%u)\n", (unsigned)(s_pg + 1), (unsigned)np,
          (unsigned)(s_sel + 1), (unsigned)files_count());
}

// Lay file dang CHON (dong da to sang). Tra NULL khi danh sach rong / chi so ngoai pham vi.
// DUNG CHUNG cho PRINT / DIR / XOA FILE -> moi lenh tac dong DUNG dong nguoi dung da cham.
static const cydlcd_file_t *files_selected(void) {
  const uint8_t n = files_count();
  if (!n || s_sel >= n) return NULL;
  return &cydlcd.files[s_sel];
}

// Cham 1 dong: CHI CHON (to sang) - KHONG in, KHONG vao thu muc (theo yeu cau: chi nut PRINT moi in).
static void cb_files_row(lv_event_t *e) {
  const uint8_t i = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  const uint8_t idx = (uint8_t)(s_pg * FILES_ROWS + i);
  if (idx >= files_count() || idx == s_sel) return;
  s_sel = idx;
  strncpy(s_sel_name, cydlcd.files[idx].name, sizeof(s_sel_name) - 1);
  s_sel_name[sizeof(s_sel_name) - 1] = '\0';
  files_rows_mark();
  // Log moi lan CHON file: de doi chieu voi dong vua cham that (`UI TAP <n>` hoac tay cham man hinh).
  WIFILOG("FILES: cham dong %u -> chon file #%u <<%s>>%s\n", (unsigned)i, (unsigned)idx,
          cydlcd.files[idx].name, (cydlcd.files[idx].flags & 0x01U) ? " [thu muc]" : "");
}

static void cb_files_prev(lv_event_t *e) { (void)e; files_page_move(-1); }
static void cb_files_next(lv_event_t *e) { (void)e; files_page_move(+1); }

static void files_do(const bool force_dir) {
  // The SD DUNG CHUNG: dang nhan file qua WiFi thi MAN HINH CYD dang giu bus SPI -> khong duoc gui lenh
  // lam Marlin doc the (M23/M24). Xem sd_host_busy().
  if (sd_host_busy()) { files_note("Dang nhan file qua WiFi - cho xong", true); return; }
  const cydlcd_file_t *f = files_selected();
  if (!f || !f->name[0]) return;
  if (force_dir || (f->flags & 0x01U)) {
    cydlcd_link_request_dir(f->name);            // vao thu muc
  }
  else {
    char cmd[CYDLCD_FILE_NAME_LEN + 6];
    snprintf(cmd, sizeof(cmd), "M23 %s", f->name);
    cydlcd_link_send_gcode(cmd);
    cydlcd_link_send_gcode("M24");               // chon file roi in luon
  }
}

static void cb_files_print(lv_event_t *e) { (void)e; files_do(false); }
static void cb_files_dir(lv_event_t *e)   { (void)e; files_do(true); }

// ---------------------------------------------------------------------------
//  INIT LAI THE SD (M22 = nha media, M21 = init media) - TAI SU DUNG A_INITSD co san.
//  VI SAO: truoc day module WiFi (ESP3D) ghi THANG len the SD cua MKS Monster8 qua
//  bus SPI dung chung. Sau khi module do ghi file, Marlin PHAI duoc init lai the moi doc/in
//  duoc; truoc day nguoi dung phai tu bam nut "Init SD" (man CUSTOM) truoc khi in.
//  Dung cho CA 2 duong: tu dong khi MO man FILE (xem show_page) va nut REFRESH.
//
//  AN TOAN KHI DANG IN: KHONG gui M22/M21 neu dang in/dang tam dung tu the SD. Marlin chi chan
//  M22 bang IS_SD_PRINTING(), con M21 = card.mount() thi KHONG chan: no dong `root` + init lai
//  driver SD -> file dang in bi mat. Truong hop nay chi XIN LAI DANH SACH (nhu REFRESH cu).
// ---------------------------------------------------------------------------
static bool files_sd_busy(void) {
  return (cydlcd.flags & (CYDLCD_SF_SD_PRINTING | CYDLCD_SF_PAUSED)) != 0;
}

void files_init_sd(void) {
  // The SD DUNG CHUNG: dang nhan file qua WiFi thi TUYET DOI khong xin danh sach / M22-M21
  // (Marlin se doc the trong luc MAN HINH CYD dang clock SPI -> hong du lieu). Cho phien upload xong.
  if (sd_host_busy()) {
    files_note("Dang nhan file qua WiFi - cho xong", true);
    return;
  }
  if (files_sd_busy()) {
    files_note("Dang in - khong init lai the", true);
    cydlcd.files_done = false;
    cydlcd.n_files    = 0;
    do_action((int)A_REQ_FILES);
    return;
  }
  files_note("DANG KHOI TAO THE...", false);
  files_job_start(FLJOB_INIT);
  do_action((int)A_INITSD);              // = cydlcd_link_send_gcode2("M22", "M21")
}

// Nut REFRESH: lam LUON ca M22/M21 roi moi xin danh sach (khong chi xin danh sach).
// Ly do: nguoi dung vua upload file bang module WiFi xong va muon thay file moi NGAY - Marlin
// chi thay file moi sau khi the SD duoc init lai.
static void cb_files_refresh(lv_event_t *e) { (void)e; files_init_sd(); }

// ---------------------------------------------------------------------------
//  XOA dang chon: FILE (M30 hoac FATFS cua man hinh) hoac THU MUC RONG (chi FATFS cua man hinh)
// ---------------------------------------------------------------------------
static char    s_del_name[CYDLCD_FILE_NAME_LEN];   // ten file/thu muc nguoi dung da xac nhan xoa
static uint8_t s_del_flags = 0;                   // flags tai luc hoi (bit0 = thu muc)

// Khoa/mo nut XOA trong luc dang xoa (xem files_btn_enable: LV_STATE_DISABLED la khoa THAT SU).
static void files_del_lock(const bool on) {
  if (!fl_del || lv_obj_has_state(fl_del, LV_STATE_DISABLED) == on) return;
  lv_obj_t *l = lv_obj_get_child(fl_del, 0);
  if (on) {
    lv_obj_add_state(fl_del, LV_STATE_DISABLED);
    if (l) { set_text(l, "DANG XOA..."); lv_obj_set_style_text_color(l, COL_DIM, 0); }
  }
  else {
    lv_obj_clear_state(fl_del, LV_STATE_DISABLED);
    if (l) { set_text(l, "XOA"); lv_obj_set_style_text_color(l, COL_TXT, 0); }
  }
}

// Chay lenh xoa THAT SU - chi duoc goi tu do_confirmed() sau khi nguoi dung bam OK du 2 buoc.
// Xoa duoc CA file LAN thu muc: thu muc chi xoa duoc khi RONG (FATFS).
void files_delete_now(void) {
  if (s_fl_job == FLJOB_DELETE) return;                 // dang xoa -> chong bam lap
  if (!s_del_name[0]) return;
  // The SD dung chung: moi lenh xoa deu lam Marlin doc/ghi the -> phai doi phien upload WiFi xong.
  if (sd_host_busy()) { files_note("Dang nhan file qua WiFi - cho xong", true); return; }

  // ---- THU MUC: chi xoa duoc khi RONG (va chi xoa duoc TU MAN HINH - xem net_sd_delete_dir) ----
  if (s_del_flags & 0x01U) {
    bool not_empty = false;
    if (net_sd_delete_dir && net_sd_delete_dir(s_del_name, &not_empty)) {
      files_note("da xoa thu muc rong", false);
      files_init_sd();                                  // bao Marlin mount lai the + xin lai danh sach
      return;
    }
    // Khong xoa duoc: phan biet "con muc con" (khong mat gi) voi loi khac -> nguoi dung biet phai lam gi.
    files_note(not_empty ? "Thu muc con file - xoa file ben trong truoc"
                         : "Khong xoa duoc thu muc", true);
    return;
  }

  // ---- FILE ----
  // THE QUA UART (P1): MAN HINH CYD giu the -> TU XOA, roi bao Marlin mount lai the (files_init_sd gui
  // M22+M21 -> Marlin doc lai FAT qua UART) va xin lai danh sach. Khong dung M30 vi Marlin dang
  // chay SDCARD_READONLY (M30 se bao loi).
  if (net_sd_delete_file && net_sd_delete_file(s_del_name)) {
    files_note("da xoa file", false);
    files_init_sd();
    return;
  }

  char cmd[CYDLCD_FILE_NAME_LEN + 6];
  snprintf(cmd, sizeof(cmd), "M30 %s", s_del_name);
  cydlcd_link_send_gcode(cmd);
  files_note("dang xoa...", false);
  files_job_start(FLJOB_DELETE);
  files_del_lock(true);                                 // tam khoa nut toi khi danh sach duoc refresh
}

// Nut XOA: KHONG xoa ngay ma hoi xac nhan 2 buoc (xoa KHONG hoan tac duoc -> giong REBOOT DFU).
// Ten (file hoac thu muc) duoc ghi ro trong hop, kem canh bao rieng cho thu muc (chi xoa duoc khi RONG).
static void cb_files_delete(lv_event_t *e) {
  (void)e;
  if (s_fl_job == FLJOB_DELETE) return;                 // dang xoa -> khong mo lai hop xac nhan
  // KHONG xoa khi dang in/tam dung tu the SD: Marlin M30 goi card.closefile() TRUOC khi xoa
  // (Marlin/src/gcode/sd/M30.cpp) -> se dong luon file dang in, ban in hong ngay.
  if (files_sd_busy()) { files_note("Dang in - khong xoa file", true); return; }
  const cydlcd_file_t *f = files_selected();
  if (!f || !f->name[0]) return;
  // flags bit 0 = isDir (Marlin cydlcd_serial.cpp: tx_u8(cydlcd_files.isDir() ? 1 : 0)).
  const bool isdir = ((f->flags & 0x01U) != 0);

  // Chot TEN ngay tai day: danh sach co the duoc Marlin day lai trong luc hop dang mo, giu
  // ten da hoi de nguoi dung xac nhan dung file/thu muc nao thi xoa dung cai do.
  strncpy(s_del_name, f->name, sizeof(s_del_name) - 1);
  s_del_name[sizeof(s_del_name) - 1] = '\0';
  s_del_flags = f->flags;

  char txt[96];
  if (isdir)
    snprintf(txt, sizeof(txt), "Xoa THU MUC?\n%s\n(chi xoa duoc khi RONG)", s_del_name);
  else
    snprintf(txt, sizeof(txt), "Xoa file?\n%s\nKhong the hoan tac!", s_del_name);
  ui_confirm("XOA", txt, (uint8_t)A_FILES_DELETE, 2);
}

// Viec cham chay o nhip 400 ms (xem ui_lvgl_task): het do tre thi xin lai danh sach.
// NULL-SAFE khi trang FILE da bi THU HOI (page recycling): moi lenh o day (mo khoa nut XOA + xin lai
// danh sach) deu khong cham vao widget cua trang, va files_del_lock() tu bo qua khi fl_del = NULL ->
// viec cham van hoan tat, chi bo phan cap nhat widget (xem page_files_destroy).
void files_job_task(void) {
  if (s_fl_job == FLJOB_NONE) return;
  if (lv_tick_elaps(s_fl_job_ms) < FILES_JOB_DELAY_MS) return;

  s_fl_job    = FLJOB_NONE;
  s_fl_note   = NULL;                    // tra dong info ve "SD: n file"
  s_fl_note_ms = 0;
  // Mo khoa nut XOA khi MOI viec cham xong - ke ca khi viec cham la INIT: nguoi dung co the vua
  // xac nhan xoa vua bam REFRESH (job sau thay job truoc), khong duoc de nut ket o trang thai khoa.
  files_del_lock(false);

  // Xoa dem TRUOC khi xin: neu thu muc chi con 0 file thi Marlin chi gui frame KET THUC (0xFF)
  // -> handle_frame() khong tu reset n_files, danh sach se van hien file vua xoa.
  cydlcd.files_done = false;
  cydlcd.n_files    = 0;
  do_action((int)A_REQ_FILES);           // xin lai danh sach -> page_files_update() ve lai cac dong
}

void page_files_create(lv_obj_t *p) {
  fl_info = mk_label_w(p, "SD: -", COL_TXT, PAGE_INN_W);

  // Khung danh sach: FILES_ROWS dong CO DINH, KHONG cuon (phan trang bang PREV/NEXT).
  // Cao = FILES_ROWS*FILES_ROW_H + (FILES_ROWS-1)*GAP: khop DUNG so dong ben trong nen khung khong
  // bao gio tu cuon (khung la mk_flex -> da clear SCROLLABLE).
  fl_rows = mk_flex(p, LV_FLEX_FLOW_COLUMN, PAGE_INN_W,
                    (lv_coord_t)(FILES_ROWS * FILES_ROW_H + (FILES_ROWS - 1) * GAP), 0, GAP);
  for (uint8_t i = 0; i < FILES_ROWS; i++) {
    lv_obj_t *r = mk_btn(fl_rows, "", PAGE_INN_W, FILES_ROW_H, cb_files_row, (void *)(intptr_t)i);
    // Nen cua dong do CHINH NO quan ly (chon = COL_ACCENT) nen phai dat LV_OBJ_FLAG_USER_1: focus_mark()
    // chi ve VIEN cho vat co co nay, khong ghi de nen -> dau "dang chon" khong bi focus xoa mat
    // (xem focus_mark() o ui_widgets.cpp - cung ly do voi cac nut truc o man JOG).
    lv_obj_add_flag(r, LV_OBJ_FLAG_USER_1);
    // Chu canh TRAI (ten file doc de hon chu canh giua) + CAT BANG "..." khi ten dai:
    // KHONG dung che do cuon vong nhu fl_info vi FILES_ROWS nhan cung cuon = FILES_ROWS animator
    // chay lien tuc, ton CPU vo ich (ten file toi da 32 ky tu x 8 px = 256 < 312 px nen it khi cat).
    lv_obj_t *l = lv_obj_get_child(r, 0);
    if (l) {
      lv_obj_set_width(l, PAGE_INN_W - 6);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
      lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_LEFT, 0);
      lv_obj_align(l, LV_ALIGN_LEFT_MID, 3, 0);
    }
    fl_row[i] = r;
  }

  // 3 hang nut: 4 nut tren (PRINT/DIR/UP/REFRESH) + 3 nut giua (PAUSE/RESUME/STOP)
  // + 1 hang cuoi (PREV / XOA FILE / NEXT).
  // Vat dem GROW truoc hang nut: hut het cho trong con lai -> 3 hang nut luon nam SAT DAY man
  // (nho vay phan tren danh tron cho danh sach file - xem khoi chu thich dau file).
  mk_spacer(p, 0, 0, 1);

  lv_obj_t *r1 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, FILES_BTN_H, 0, GAP);
  mk_btn_grow(r1, "PRINT",   FILES_BTN_H, cb_files_print, NULL);
  mk_btn_grow(r1, "DIR",     FILES_BTN_H, cb_files_dir, NULL);
  mk_btn_grow(r1, "UP",      FILES_BTN_H, cb_action, (void *)(intptr_t)A_REQ_UP);
  // REFRESH = init lai the SD (M22/M21) ROI MOI xin danh sach - xem cb_files_refresh()
  mk_btn_grow(r1, "REFRESH", FILES_BTN_H, cb_files_refresh, NULL);

  lv_obj_t *r2 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, FILES_BTN_H, 0, GAP);
  mk_btn_grow(r2, "PAUSE",  FILES_BTN_H, cb_action, (void *)(intptr_t)A_PAUSE);
  mk_btn_grow(r2, "RESUME", FILES_BTN_H, cb_action, (void *)(intptr_t)A_RESUME);
  mk_btn_grow(r2, "STOP (M524)", FILES_BTN_H, cb_action, (void *)(intptr_t)A_STOP);

  // Hang cuoi: [PREV] [XOA] [NEXT]. PREV/NEXT lat TUNG TRANG danh sach (khong cuon tay).
  // XOA nam giua 2 nut lat trang: XOA duoc CA FILE LAN THU MUC (thu muc chi xoa duoc khi RONG) va la
  // thao tac KHONG hoan tac duoc -> phai xac nhan 2 buoc (xem cb_files_delete); 2 nut lat trang la
  // vo hai nen dat canh nhau khong lam tang rui ro bam nham.
  lv_obj_t *r3 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, FILES_BTN_H, 0, GAP);
  fl_prev = mk_btn_grow(r3, "PREV", FILES_BTN_H, cb_files_prev, NULL);
  fl_del  = mk_btn_grow(r3, "XOA", FILES_BTN_H, cb_files_delete, NULL);
  fl_next = mk_btn_grow(r3, "NEXT", FILES_BTN_H, cb_files_next, NULL);
}

void page_files_update(void) {
  if (!fl_rows || !fl_info) return;     // trang da bi thu hoi (an toan: xem page_files_destroy)

  const uint32_t sig = files_signature();
  if (sig != s_files_sig || cydlcd.n_files != s_files_n) {
    s_files_sig = sig;
    s_files_n   = cydlcd.n_files;
    files_rows_reload();                // danh sach DOI -> giu lua chon theo ten roi ve lai cac dong
  }

  const uint8_t n  = files_count();
  const uint8_t np = files_npages();
  // Dong info: uu tien trang thai viec cham (init the / dang xoa) va ghi chu ngan, roi moi den so file.
  // So trang chi hien khi danh sach co nhieu hon 1 trang (man gon thi khong them chu thua).
  // Ten file dang CHON luon hien o cuoi (" >ten"): khi lat sang trang khac thi khong dong nao duoc to
  // sang nua, nen day la cho DUY NHAT con cho biet dang chon file nao (truoc khi bam PRINT/XOA FILE).
  // Ten dai hon o -> fl_info tu cuon vong (mk_label_w), khong bi cat cut.
  char b[96], sel[CYDLCD_FILE_NAME_LEN + 6];
  sel[0] = '\0';
  if (n) files_row_text(s_sel, sel, sizeof(sel));      // "[D] ten" hoac "(khong ten)"
  const char *sn = sel[0] ? " >" : "";
  if (np > 1)
    snprintf(b, sizeof(b), "SD: %u file%s T%u/%u%s%s", (unsigned)n,
             cydlcd.files_done ? "" : " (dang nhan)", (unsigned)(s_pg + 1), (unsigned)np, sn, sel);
  else
    snprintf(b, sizeof(b), "SD: %u file%s%s%s", (unsigned)n,
             cydlcd.files_done ? "" : " (dang nhan)", sn, sel);

  const bool live = (s_fl_note && s_fl_job != FLJOB_NONE);
  const bool temp = (s_fl_note && s_fl_note_ms && lv_tick_elaps(s_fl_note_ms) < FILES_NOTE_MS);
  if (live || temp) set_text(fl_info, s_fl_note);
  else set_text(fl_info, b);

  // PREV/NEXT: nut nao khong con gi de lat thi KHOA THAT SU (xem files_btn_enable) - khong de nguoi
  // dung bam mai ma khong hieu vi sao khong co gi xay ra. Chi ghi khi trang thai doi.
  files_nav_mark();
}

// Huy man FILE SD (xem quy uoc o ui_lvgl_int.h): dat NULL cac con tro widget va XOA CACHE danh sach
// file. VI SAO phai xoa cache: page_files_update() chi goi files_rows_reload() khi dau van tay
// (s_files_sig) hoac so file DOI - ma lan mo lai trang thi cac dong la vat MOI (chu rong), nen neu
// giu cache cu thi danh sach file se KHONG duoc ve lai.
// s_fl_note / s_fl_job: GIU NGUYEN - viec cham (M22/M21, M30) van phai chay tiep khi da roi trang
// (xem files_job_task), va files_job_task() tu xoa ghi chu khi xong.
// s_sel / s_pg / s_sel_name: GIU NGUYEN - lan mo lai trang van dung dong dang chon (files_rows_reload
// tu kiem tra lai ten file va dua con tro ve dung trang).
void page_files_destroy(void) {
  fl_info = NULL;
  fl_rows = NULL;
  fl_del  = NULL;
  fl_prev = NULL;
  fl_next = NULL;
  for (uint8_t i = 0; i < FILES_ROWS; i++) fl_row[i] = NULL;
  s_files_sig = 0;
  s_files_n   = 0xFFU;      // 0xFF = "chua biet" -> buoc page_files_update() ke tiep dung lai danh sach
}

// ---------------------------------------------------------------------------
//  CHAN DOAN (khong anh huong logic) - de kiem tra man FILE tu PC qua COM7.
//  VI SAO CAN: man hinh CYD khong co framebuffer doc nguoc (driver ve thang ra SPI) nen khong the
//  "chup anh" man hinh, va cung khong the cham tay vao man khi dang ngoi o PC. Hai ham duoi day
//  cho phep: (1) ban su kien CHAM vao 1 dong de kiem tra duong chon file, (2) in ra dong nao
//  dang duoc CHON - dung de kiem chung loi "chon 1 file ma bao Khong xoa thu muc".
// ---------------------------------------------------------------------------
// Ban su kien CLICKED vao 1 nut (di DUNG duong su kien that, nhu khi co nguoi cham vao nut do).
static void files_test_click(lv_obj_t *b, const char *name) {
  if (!b) { Serial.printf("[UI] FILES: khong co nut %s\n", name); return; }
  lv_event_send(b, LV_EVENT_CLICKED, NULL);
}

// Lenh chan doan `UI TAP <gi>` tren COM7:
//   <so 0..4> : ban CHAM vao dong thu <so> CUA TRANG dang xem (kiem tra chon dung dong + to sang);
//   P / N     : ban cham vao nut PREV / NEXT (kiem tra lat trang);
//   D         : ban cham vao nut XOA (kiem tra dong nao bi coi la gi); hop xac nhan (neu mo) TU DONG
//               dong sau 4 s -> khong de hop modal nam tren man.
//   K         : bam nut OK cua hop XAC NHAN dang mo (2 lan = chay lenh xoa that - xem boxes_test_ok);
//   C         : DEM MUC CON cua thu muc dang chon (KHONG xoa gi) - de biet truoc thu muc co RONG khong.
//   (khong tham so): chi in trang thai (giong `UI BOX`).
void files_test_cmd(const char *arg) {
  if (!fl_rows) { Serial.println("[UI] FILES: trang chua duoc tao (mo truoc bang 'UI FILE SD')"); return; }
  while (arg && *arg == ' ') arg++;
  if (!arg || !*arg) { page_files_dump(); return; }

  char c = *arg;
  if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');   // khong can ctype.h
  if (c == 'P') { files_page_move(-1); page_files_dump(); return; }
  if (c == 'N') { files_page_move(+1); page_files_dump(); return; }
  if (c == 'D') {
    files_test_click(fl_del, "XOA");
    ui_confirm_autoclose(8000);          // vo hai khi khong co hop nao duoc mo
    page_files_dump();
    return;
  }
  if (c == 'K') { boxes_test_ok(); page_files_dump(); return; }
  if (c == 'C') {
    const cydlcd_file_t *f = files_selected();
    if (!f || !f->name[0]) { Serial.println("[UI] FILES: chua chon gi"); return; }
    if (!(f->flags & 0x01U)) { Serial.printf("[UI] FILES: <<%s>> la FILE (khong phai thu muc)\n", f->name); return; }
    const int n = net_sd_dir_count ? net_sd_dir_count(f->name) : -1;
    Serial.printf("[UI] FILES: thu muc <<%s>> co %d muc con -> %s\n", f->name, n,
                  (n < 0) ? "KHONG mo duoc (khong co the / khong ho tro)"
                          : (n ? "CON muc con nen KHONG xoa duoc (chi xoa thu muc RONG)"
                               : "RONG nen xoa duoc"));
    return;
  }

  const int i = atoi(arg);
  if (i < 0 || i >= (int)FILES_ROWS) {
    Serial.printf("[UI] FILES: dung 'UI TAP <dong 0..%u>' | P (trang truoc) | N (trang sau) | D (nut XOA) | K (OK hop xac nhan) | C (dem muc con)\n",
                  (unsigned)(FILES_ROWS - 1));
    return;
  }
  if (lv_obj_has_flag(fl_row[i], LV_OBJ_FLAG_HIDDEN)) {
    Serial.printf("[UI] FILES: dong %d dang AN (ngoai danh sach)\n", i);
    return;
  }
  files_test_click(fl_row[i], "dong file");
  page_files_dump();
}

// In trang thai danh sach FILE (goi tu `UI BOX` khi dang o man FILE): so file, trang dang xem, DONG
// DANG CHON (chi so TUYET DOI + ten + co thu muc) va TUNG DONG dang hien (chu THAT tren widget +
// dong nao dang to nen COL_ACCENT) - nho vay kiem chung duoc ca phan "cham vao dong nao thi dong do
// sang" chu khong chi doc lai du lieu cua Marlin.
void page_files_dump(void) {
  if (!fl_rows) { Serial.println("[UI] FILES: trang chua duoc tao"); return; }
  const cydlcd_file_t *f = files_selected();
  Serial.printf("[UI] FILES: %u file | trang %u/%u | chon #%u <<%s>> (s_sel_name=\"%s\") | PREV=%s NEXT=%s\n",
                (unsigned)files_count(), (unsigned)(s_pg + 1), (unsigned)files_npages(),
                (unsigned)s_sel, f ? f->name : "", s_sel_name,
                (fl_prev && lv_obj_has_state(fl_prev, LV_STATE_DISABLED)) ? "khoa" : "mo",
                (fl_next && lv_obj_has_state(fl_next, LV_STATE_DISABLED)) ? "khoa" : "mo");
  for (uint8_t i = 0; i < FILES_ROWS; i++) {
    lv_obj_t *r = fl_row[i];
    if (!r) continue;
    lv_obj_t *l = lv_obj_get_child(r, 0);
    const char *txt = l ? lv_label_get_text(l) : "";
    const bool hidden = lv_obj_has_flag(r, LV_OBJ_FLAG_HIDDEN);
    const uint8_t idx = (uint8_t)(s_pg * FILES_ROWS + i);
    const bool lit = (lv_obj_get_style_bg_color(r, 0).full == COL_ACCENT.full);   // dang to sang?
    Serial.printf("[UI]   dong %u -> file #%u%s%s%s : \"%s\"\n", (unsigned)i, (unsigned)idx,
                  (idx == s_sel) ? " [CHON]" : "", lit ? " [NEN SANG]" : "",
                  hidden ? " [AN]" : "", txt);
  }
}

#endif  // CYDLCD_USE_LVGL
