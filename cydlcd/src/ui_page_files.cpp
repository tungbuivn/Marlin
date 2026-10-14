/**
 * ui_page_files.cpp -- man FILE SD cua UI LVGL (man hinh CYD): chon file tren the de in,
 * xoa file, lam moi danh sach.
 *
 * Tach tu ui_lvgl.cpp (chang 3/4). NOI DUNG HAM DUOC DI CHUYEN NGUYEN VAN: chi bo `static`
 * o nhung ham dung cheo file (khai bao trong ui_lvgl_int.h), khong doi logic/ten/chuoi.
 *
 * Gom:
 *   - "viec cham" cua man FILE (init lai the SD sau khi module khac ghi the, xoa file qua
 *     net_sd_delete_file) - chay o nhip 400 ms trong ui_lvgl_task() ke CA khi da roi man;
 *   - roller danh sach file + 3 nut (PRINT / DIR / XOA FILE) + dong info.
 *
 * HOP DONG voi net_upload_http.cpp (2 ham `weak`): xem khoi chu thich ngay duoi day.
 */
#include "ui_lvgl.h"
#include "ui_lvgl_int.h"

#include <stdio.h>                  // snprintf
#include <string.h>                 // strcmp
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

// ===========================================================================
//  FILES  (PAGE_INN_H = 206)
//    info 17 (font mono mode 4: line_height = 17) + roller 6 dong (6*16 = 96 voi Montserrat 14
//    - chieu cao TINH THEO FONT THAT cua roller, xem page_files_create) + 3 hang nut 78
//    + 5*GAP 10 = 201 <= 206.
//  DAY MAN: 3 hang nut bi DAY XUONG DAY bang mk_spacer(grow) - truoc day roller chi 4 dong (64 px)
//  nen thua 39 px o duoi; nay roller cao 6 dong (them ~2 dong file) va nut nam sat day man.
//  (201 + 2*PAGE_PAD < CONT_H nen man nay KHONG tran -> khong hien thanh day PREV/NEXT)
// ===========================================================================
#define FILES_BTN_H   26
#define FILES_ROWS    6                 // so dong file nhin thay (truoc: 4)

static lv_obj_t *fl_roller, *fl_info;
static lv_obj_t *fl_del;                        // nut XOA FILE (tam khoa trong luc dang xoa)
static char      s_roller_buf[CYDLCD_MAX_FILES * CYDLCD_FILE_NAME_LEN + 1];
static uint32_t  s_files_sig = 0;
static uint8_t   s_files_n   = 0;

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

// FNV-1a don gian tren ten+co cua danh sach file -> biet khi nao phai tao lai roller
static uint32_t files_signature(void) {
  uint32_t h = 2166136261UL;
  for (uint8_t i = 0; i < cydlcd.n_files; i++) {
    for (const char *s = cydlcd.files[i].name; *s; s++) h = (h ^ (uint8_t)*s) * 16777619UL;
    h = (h ^ cydlcd.files[i].flags) * 16777619UL;
  }
  return h ^ ((uint32_t)cydlcd.n_files << 24);
}

static void files_rebuild(void) {
  if (!fl_roller) return;             // trang FILE da bi thu hoi -> khong con roller de ve lai
  uint8_t n = cydlcd.n_files;
  if (n > CYDLCD_MAX_FILES) n = CYDLCD_MAX_FILES;

  char *p = s_roller_buf;
  size_t left = sizeof(s_roller_buf);
  s_roller_buf[0] = '\0';

  if (!n) {
    snprintf(p, left, "(khong co file)");
  }
  else {
    for (uint8_t i = 0; i < n; i++) {
      const bool isdir = ((cydlcd.files[i].flags & 0x01U) != 0);
      // Ten RONG (Marlin khong gui ten dai va cung khong lui ve ten 8.3 - ban cu): hien ro
      // "(khong ten)" thay vi de 1 dong TRONG, vi dong trong trong nhu loi UI (gap that tren the nay:
      // mot thu muc chi co ten 8.3). Xem cydlcd_serial.cpp: files_pump() da co duong lui 8.3.
      const char *pname = cydlcd.files[i].name;
      if (!pname[0]) pname = "(khong ten)";
      const int w = snprintf(p, left, "%s%s%s", (i ? "\n" : ""), isdir ? "[D] " : "", pname);
      if (w < 0 || (size_t)w >= left) break;
      p += w;
      left -= (size_t)w;
    }
  }

  uint16_t sel = lv_roller_get_selected(fl_roller);
  if (sel >= n) sel = n ? (uint16_t)(n - 1) : 0;
  lv_roller_set_options(fl_roller, s_roller_buf, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(fl_roller, sel, LV_ANIM_OFF);
}

// Lay ten file dang chon trong roller (tra ve NULL neu khong hop le)
static const cydlcd_file_t *files_selected(void) {
  if (!fl_roller) return NULL;        // trang FILE da bi thu hoi (an toan: xem page_files_destroy)
  const uint16_t sel = lv_roller_get_selected(fl_roller);
  if (sel >= cydlcd.n_files || sel >= CYDLCD_MAX_FILES) return NULL;
  return &cydlcd.files[sel];
}

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
//  XOA FILE dang chon trong roller (M30)
// ---------------------------------------------------------------------------
static char    s_del_name[CYDLCD_FILE_NAME_LEN];   // ten file nguoi dung da xac nhan xoa
static uint8_t s_del_flags = 0;                   // flags tai luc hoi (bit0 = thu muc)

// Khoa/mo nut XOA FILE. Chi doi khi trang thai THAT SU doi (tranh invalidate moi 400 ms).
// Dung LV_STATE_DISABLED: lv_group bo qua vat disabled (lv_group.c) -> khoa ca duong encoder.
static void files_del_lock(const bool on) {
  if (!fl_del || lv_obj_has_state(fl_del, LV_STATE_DISABLED) == on) return;
  lv_obj_t *l = lv_obj_get_child(fl_del, 0);
  if (on) {
    lv_obj_add_state(fl_del, LV_STATE_DISABLED);
    if (l) { set_text(l, "DANG XOA..."); lv_obj_set_style_text_color(l, COL_DIM, 0); }
  }
  else {
    lv_obj_clear_state(fl_del, LV_STATE_DISABLED);
    if (l) { set_text(l, "XOA FILE"); lv_obj_set_style_text_color(l, COL_TXT, 0); }
  }
}

// Chay lenh xoa THAT SU - chi duoc goi tu do_confirmed() sau khi nguoi dung bam OK du 2 buoc.
void files_delete_now(void) {
  if (s_fl_job == FLJOB_DELETE) return;                 // dang xoa -> chong bam lap
  if (!s_del_name[0]) return;
  // The SD dung chung: M30 cung lam Marlin doc/ghi the -> phai doi phien upload WiFi xong.
  if (sd_host_busy()) { files_note("Dang nhan file qua WiFi - cho xong", true); return; }
  // Kiem lai co "thu muc" o day nua: hop xac nhan khong khoa duoc vung phia duoi (cham van lot),
  // nen lua chon co the da doi trong luc hop dang mo.
  if (s_del_flags & 0x01U) { files_note("Khong xoa thu muc", true); return; }

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

// Nut XOA FILE: KHONG xoa ngay ma hoi xac nhan 2 buoc (xoa file KHONG hoan tac duoc -> giong
// REBOOT DFU). Ten file (ke ca ten khong co phan mo rong - van cho phep) duoc ghi ro trong hop.
static void cb_files_delete(lv_event_t *e) {
  (void)e;
  if (s_fl_job == FLJOB_DELETE) return;                 // dang xoa -> khong mo lai hop xac nhan
  // KHONG xoa khi dang in/tam dung tu the SD: Marlin M30 goi card.closefile() TRUOC khi xoa
  // (Marlin/src/gcode/sd/M30.cpp) -> se dong luon file dang in, ban in hong ngay.
  if (files_sd_busy()) { files_note("Dang in - khong xoa file", true); return; }
  const cydlcd_file_t *f = files_selected();
  if (!f || !f->name[0]) return;
  // flags bit 0 = isDir (Marlin cydlcd_serial.cpp: tx_u8(cydlcd_files.isDir() ? 1 : 0)).
  if (f->flags & 0x01U) { files_note("Khong xoa thu muc", true); return; }

  // Chot TEN FILE ngay tai day: danh sach co the duoc Marlin day lai trong luc hop dang mo, giu
  // ten da hoi de nguoi dung xac nhan dung file nao thi xoa dung file do.
  strncpy(s_del_name, f->name, sizeof(s_del_name) - 1);
  s_del_name[sizeof(s_del_name) - 1] = '\0';
  s_del_flags = f->flags;

  char txt[80];
  snprintf(txt, sizeof(txt), "Xoa file?\n%s\nKhong the hoan tac!", s_del_name);
  ui_confirm("XOA FILE", txt, (uint8_t)A_FILES_DELETE, 2);
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
  // -> handle_frame() khong tu reset n_files, roller se van hien file vua xoa.
  cydlcd.files_done = false;
  cydlcd.n_files    = 0;
  do_action((int)A_REQ_FILES);           // xin lai danh sach -> page_files_update() ve lai roller
}

void page_files_create(lv_obj_t *p) {
  fl_info = mk_label_w(p, "SD: -", COL_TXT, PAGE_INN_W);

  fl_roller = lv_roller_create(p);
  lv_obj_set_width(fl_roller, PAGE_INN_W);
  lv_roller_set_visible_row_count(fl_roller, FILES_ROWS);
  // Chieu cao TINH THEO FONT THAT cua roller (lv_roller_set_visible_row_count() chi dat chieu cao
  // theo font LUC GOI; o day dat lai bang so nguyen lan line_height de khong co dong bi cat ngang).
  const lv_font_t *rf = lv_obj_get_style_text_font(fl_roller, LV_PART_MAIN);
  lv_obj_set_height(fl_roller, (lv_coord_t)(lv_font_get_line_height(rf) * FILES_ROWS));
  lv_roller_set_options(fl_roller, "(chua co du lieu)", LV_ROLLER_MODE_NORMAL);
  lv_obj_set_style_bg_color(fl_roller, COL_BTN, 0);
  lv_obj_set_style_bg_opa(fl_roller, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(fl_roller, COL_TXT, 0);
  lv_obj_set_style_border_width(fl_roller, 0, 0);
  lv_obj_set_style_text_color(fl_roller, lv_color_hex(0x102030), LV_PART_SELECTED);
  lv_obj_set_style_bg_color(fl_roller, COL_ACCENT, LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(fl_roller, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_add_flag(fl_roller, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  if (s_group) lv_group_add_obj(s_group, fl_roller);
  // CHAM/QUAY CHI DE CHON FILE - KHONG tu in (theo yeu cau 2026-10-15).
  // VI SAO BO: truoc day roller co `LV_EVENT_CLICKED -> cb_files_print` (hop ly voi encoder kieu cu
  // "bam lan 2 = in"), nhung tren man CAM UNG thi chi CHAM VAO danh sach la may in luon (gap that:
  // nguoi dung chi dinh keo danh sach da kich hoat ban in). Nay chi nut `PRINT` moi gui M23+M24;
  // voi encoder: quay = chon file, bam nut = vao che do chon (LVGL tu xu ly), roi bam `PRINT`.
  // (Ham cb_files_print() van dung cho nut PRINT.)

  // 3 hang nut: 4 nut tren (PRINT/DIR/UP/REFRESH) + 3 nut duoi (PAUSE/RESUME/STOP)
  // + 1 hang rieng cho XOA FILE (viec khong hoan tac duoc -> de rieng, de bam nham)
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

  // Hang 3: XOA FILE dang chon trong roller (M30). Phai xac nhan 2 buoc (xem cb_files_delete).
  lv_obj_t *r3 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, FILES_BTN_H, 0, GAP);
  fl_del = mk_btn_grow(r3, "XOA FILE", FILES_BTN_H, cb_files_delete, NULL);
}

void page_files_update(void) {
  if (!fl_roller || !fl_info) return;     // trang da bi thu hoi (an toan: xem page_files_destroy)
  char b[40];
  // Dong info: uu tien trang thai viec cham (init the / dang xoa) va ghi chu ngan, roi moi den so file.
  const bool live = (s_fl_note && s_fl_job != FLJOB_NONE);
  const bool temp = (s_fl_note && s_fl_note_ms && lv_tick_elaps(s_fl_note_ms) < FILES_NOTE_MS);
  if (live || temp) set_text(fl_info, s_fl_note);
  else {
    snprintf(b, sizeof(b), "SD: %u file%s", (unsigned)cydlcd.n_files, cydlcd.files_done ? "" : " (dang nhan)");
    set_text(fl_info, b);
  }

  const uint32_t sig = files_signature();
  if (sig != s_files_sig || cydlcd.n_files != s_files_n) {
    s_files_sig = sig;
    s_files_n   = cydlcd.n_files;
    files_rebuild();
  }
}

// Huy man FILE SD (xem quy uoc o ui_lvgl_int.h): dat NULL 3 con tro widget va XOA CACHE danh sach
// file. VI SAO phai xoa cache: page_files_update() chi goi files_rebuild() khi dau van tay
// (s_files_sig) hoac so file DOI - ma lan mo lai trang thi fl_roller la vat MOI (dang hien
// "(chua co du lieu)"), nen neu giu cache cu thi danh sach file se KHONG duoc ve lai.
// s_fl_note / s_fl_job: GIU NGUYEN - viec cham (M22/M21, M30) van phai chay tiep khi da roi trang
// (xem files_job_task), va files_job_task() tu xoa ghi chu khi xong.
void page_files_destroy(void) {
  fl_roller   = NULL;
  fl_info     = NULL;
  fl_del      = NULL;
  s_files_sig = 0;
  s_files_n   = 0xFFU;      // 0xFF = "chua biet" -> buoc page_files_update() ke tiep dung lai roller
}

#endif  // CYDLCD_USE_LVGL

