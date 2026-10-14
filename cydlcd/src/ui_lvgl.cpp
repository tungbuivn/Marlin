/**
 * ui_lvgl.cpp -- UI LVGL 8.3/8.4 cho MAN HINH CYD (du lieu lay tu frame PUSH cua Marlin, xem cydlcd_link.h).
 *
 * BO CUC MAN STATUS: lay Y NGUYEN toa do tu UI TFT ILI9341 cua Marlin
 *   Marlin/src/lcd/tft/ui_320x240.cpp  (MarlinUI::draw_status_screen(), cung 320x240):
 *     - 3 o 64x100 tai x = 21 / 127 / 234   (x = (320/3 - 64)/2 + 320*i/3): dau in, ban in, quat
 *         target o y=8, "icon" o y=18 (Marlin dung bitmap 64x64), nhiet do hien tai o y=72
 *     - hang XYZ: khung (4, 103, 312, 24), nhan X/Y/Z trong khung (x = 10 / 127 / 219,
 *       gia tri can phai ket thuc o x = 68 / 185 / 301)
 *     - FR (70, 136, 80, 32) va FL (170, 136, 80, 32)
 *     - nut SD 64x64 o (0, 130)  va  nut SETTINGS 64x64 o (256, 130)  <- "nut setting ben phai"
 *     - thoi gian in (96, 176, 128, 20), thanh tien do (4, 198, 312, 9),
 *       dong thong bao/trang thai (0, 216, 320, 20)
 *     Mau theo Marlin/src/lcd/tft/tft_color.h: nen #1E156E, COLD/FAN aqua, HOTEND scarlet,
 *     HEATED_BED dark-orange, AXIS_HOMED trang, AXIS_NOT_HOMED vang, RATE_100 xanh la,
 *     RATE_ALTERED vang, PRINT_TIME aqua, PROGRESS_BAR xanh duong.
 *   Icon 64x64: DA PORT THAT tu bitmap GREYSCALE4 cua Marlin -> sinh ra ui_lvgl_icons.h bang
 *   tools/make_lvgl_icons.py (xem muc 5.2 trong cydlcd/README.md), ve bang LVGL (LV_USE_IMG).
 *   (Ban cu chi ve "bang" khung + nhan NOZ/BED/FAN/SD/SET - nay KHONG dung nua.)
 *
 * Cac man khac (TEMP/JOG/FILES/SETTINGS/MESH/CUSTOM/INFO) nam duoi 1 MAN MENU kieu Marlin:
 *   STATUS --(nut SETTINGS)--> MENU --(chon dong)--> man con --(BACK)--> MENU --(BACK)--> STATUS.
 *   Cac o tren STATUS cung la nut: dau in/ban in/quat -> TEMP (co hang FAN), hang XYZ -> JOG,
 *   SD -> FILES, FR/FL -> SETTINGS, SETTINGS -> MENU.
 *   (Bo thanh nav 8 nut o duoi: no chiem 64 px va lam STATUS khac han man TFT cua Marlin.)
 *
 * Kien truc:
 *   - 1 screen duy nhat. STATUS dung kin 320x240 theo bo cuc Marlin (khong co dong tieu de).
 *     MOI MAN KHAC (ke ca MENU) deu nam duoi MOT dong tieu de chung o tren:
 *         [ ten man ] ............ [ LINK / NO LINK ]  [ BACK ]
 *     => nut BACK LUON o goc PHAI dong tieu de, khong ve BACK rieng tung man (do ton dien tich).
 *     BACK = di len 1 cap: man con -> MENU -> STATUS (man goc khong co BACK).
 *   - Moi man la 1 container (lv_obj) TAO 1 LAN khi man duoc mo lan dau, sau do chi
 *     an/hien (LV_OBJ_FLAG_HIDDEN) va cap nhat text. Khong tao lai widget moi vong lap.
 *   - Bo cuc bang FLEX (LV_USE_FLEX), khong dung toa do y cung: container = flex COLUMN,
 *     moi hang = flex ROW, nut can chia deu thi lv_obj_set_flex_grow(). Moi kich thuoc suy ra
 *     tu UI_LCD_HOR_RES/VER_RES + ROW_H (1 dong chu montserrat_14 = 16 px) nen doi man hinh
 *     chi phai xem lai cac hang so o duoi.
 *   - 1 lv_group chung cho encoder: LVGL 8.3 tu bo qua vat bi HIDDEN khi di chuyen focus
 *     (lv_group.c: "Hidden objects don't receive focus"), nen khong can group rieng cho
 *     tung man.
 *   - Nut bam: xoa co LV_OBJ_FLAG_SCROLLABLE de encoder BAM 1 LAN la an ngay
 *     (lv_indev.c: vat "editable or scrollable" thi lan bam dau chi vao che do edit).
 *
 * Cac man:
 *   STATUS  : THEO MARLIN ui_320x240 (nhiet dau in / ban in / quat + XYZ + FR/FL + SD + SETTINGS
 *             + thoi gian + thanh tien do + dong trang thai).
 *   MENU    : menu doc kieu Marlin, mo bang nut SETTINGS tren STATUS.
 *   TEMP    : +/- 1/10 do C cho nozzle & bed -> M104 S... / M140 S..., hang FAN -> M106 S...,
 *             1 hang preset PLA 200/60 | PETG 230/60 | ABS 240/60 (M104 + M140), COOLDOWN.
 *   JOG     : chon truc X/Y/Z, nut -0.1/+0.1/-1/+1/-10/+10 mm -> G91 + G1 <truc><d> F600 + G90,
 *             nut HOME ALL -> G28.
 *   FILES   : lv_roller danh sach cydlcd.files[], PRINT (M23 + M24), DIR, UP, REFRESH,
 *             PAUSE (M25), RESUME (M24), STOP (M524).
 *   SETTINGS: bang thong so cydlcd_setting(CYDLCD_SET_*), cham 1 dong -> lop phu -/+/OK de SUA
 *             (gui M92/M203/M201/M205/M851/M220/M221/M301/M304/M106/M104/M140) +
 *             SAVE EEPROM (CYDLCD_REQ_SAVE_SETTINGS).
 *   MESH    : bang luoi bed mesh (cydlcd.mesh[col][row]), o cao = xanh, o thap = do,
 *             REFRESH -> CYDLCD_REQ_MESH; khong co CYDLCD_HAS_MESH thi bao "khong ho tro".
 *   CUSTOM  : 2 nut nhu menu cu cua Marlin: G34 Bed (G34 Q99) + Init SD (M22 roi M21).
 *   INFO    : 11 dong thong tin THAT (khong co nut REFRESH FULL nua): FW/BOARD/UI(LVGL + font
 *             mono)/WIFI/PROTO/MARLIN/LINK/RX-ERR (2 co mot chieu)/SD + mesh/SYS (uptime + heap).
 *   TRAM    : Bed Tramming Wizard cua Marlin (4 so do lech + huong dan van vit + 4 nut
 *             PROBE / MAX<->MIN / BACK / DONE). KHONG dung frame PUSH: Marlin chi can bat
 *             #define CYDLCD_TBT_LINK la du (link ASCII tren USART1 - xem tbt_link.h), nho vay
 *             man 12864 + menu MarlinUI (Tramming Wizard, Z ALIGN MANUAL...) VAN CON NGUYEN.
 *
 * Prompt cua Marlin (cydlcd.prompt_active) -> lv_msgbox OK/CANCEL -> cydlcd_link_answer().
 * Thong bao (cydlcd.message) -> lv_msgbox theo cydlcd.message_sev, tu dong dong sau 6s.
 *
 * LUU Y: chu tren man hinh PHAI la ASCII, vi font montserrat_14 cua LVGL khong co dau
 * tieng Viet (U+1EA0..U+1EF9). NGOAI LE: che do UI_MONO_FONT 4 dung font mono GFX TU SINH
 * (gfx4_font_robotomono qua driver lv_font_gfxfont) CO DU dau tieng Viet, nho vay lop phu SUA
 * trong SETTINGS + hang PREHEAT moi hien duoc chu co dau; chu co dau van viet bang ma UTF-8
 * (\x..) de file nguon giu duoc ASCII thuan.
 * Che do UI_MONO_FONT 0 chi dung Montserrat -> KHONG hien duoc dau (xem vn_font()).
 * Khong dung %f khi format so: fmt_scaled() in so nguyen da nhan 10/100/1000.
 */
#include "ui_lvgl.h"

#include "ui_lvgl_int.h"            // cau hinh + khai bao dung chung (xem chu thich trong file)

#if CYDLCD_USE_LVGL

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <Arduino.h>                // Serial.printf cho log tu kiem tra font (che do 4)

#include "cyd_sd.h"        // cyd_sd_ready() cho icon the SD (the o khe man hinh)
#include "net_upload.h"             // net_wifi_state(): trang thai WiFi de ve icon o header

// Trang thai WiFi cho icon o dong tieu de. net_upload.h la "hop dong giao dien" voi nhanh module
// WiFi (net_wifi.cpp): 0 = WiFi tat/chua cau hinh, 1 = dang ket noi, 2 = da ket noi.
//
// VI SAO phai khai bao lai voi `weak`: module do CO THE CHUA duoc build (ho dang lam song song,
// hoac board khac khong co WiFi). Voi `weak`:
//   - thieu dinh nghia -> symbol tro thanh "weak undefined", ELF cho phep -> KHONG loi link,
//     icon chi hien mau xam (trang thai 0);
//   - module xuat hien -> tu dong bind vao dinh nghia that (extern "C" nhu trong net_upload.h).
// BAT BUOC phai co `weak`: neu khai bao thuong thi `-Wall` se canh bao
//   "the address of 'net_wifi_state' will never be NULL [-Waddress]"
// ngay tai cho ta kiem tra con tro truoc khi goi (da kiem chung bang chinh toolchain xtensa).
extern "C" uint8_t net_wifi_state(void) __attribute__((weak));

// GOM 1 CHO: ca icon o header (wifi_icon_mark) lan dong "WIFI:" o man INFO deu goi ham nay.
uint8_t wifi_state_now(void) {
  // Module WiFi co the CHUA duoc build -> con tro ham = NULL -> coi nhu trang thai 0.
  return net_wifi_state ? net_wifi_state() : 0;
}

uint8_t sd_state_now(void) {
#if CYDLCD_SD_SHARED
  // THE DUNG CHUNG (mux SPI, xem cydlcd_config.h): the nam trong khe CYD nhung do MARLIN mount
  // (man hinh chi gianh bus khi nhan file). Vi vay trang thai that su lay tu co CYDLCD_SF_MEDIA
  // trong frame PUSH cua Marlin - KHONG tu mount the de kiem (se gianh bus cua Marlin).
  return (cydlcd.flags & CYDLCD_SF_MEDIA) ? SD_ST_OK : SD_ST_NO;
#else
  return cyd_sd_ready() ? SD_ST_OK : SD_ST_NO;
#endif
}



// ---------------------------------------------------------------------------
//  HOP DONG voi nhanh firmware: WiFi (tach rieng khoi BT - xem net_upload.h).
//  Linkage `extern "C"`: net_wifi_state() da duoc khai bao trong net_upload.h trong khoi
//  extern "C" (dinh nghia trong net_wifi.cpp voi linkage C nhu khai bao trong net_wifi.h), nen
//  cac ham WiFi phai cung linkage C moi bind dung ten.

//  `weak`: nhanh firmware CHUA merge -> con tro NULL -> trang WIFI van MO DUOC va bao
//  "khong ho tro" (khong crash, khong loi link).
// ---------------------------------------------------------------------------
extern "C" const char *net_wifi_ssid(void) __attribute__((weak));
extern "C" const char *net_wifi_ip(void) __attribute__((weak));
extern "C" int8_t      net_wifi_rssi(void) __attribute__((weak));
extern "C" int         net_wifi_scan(void) __attribute__((weak));
extern "C" const char *net_wifi_scan_ssid(int i) __attribute__((weak));
extern "C" int8_t      net_wifi_scan_rssi(int i) __attribute__((weak));
extern "C" bool        net_wifi_connect(const char *ssid, const char *pass, bool save) __attribute__((weak));
extern "C" void        net_wifi_disconnect(bool forget) __attribute__((weak));

// Bao an toan cho UI: ham vang (weak NULL) -> tra gia tri "rong" thay vi goi con tro NULL.
static const char *wifi_ssid_now(void) {
  const char *s = net_wifi_ssid ? net_wifi_ssid() : NULL;
  return s ? s : "";
}
static const char *wifi_ip_now(void) {
  const char *s = net_wifi_ip ? net_wifi_ip() : NULL;
  return s ? s : "";
}
static int8_t wifi_rssi_now(void) { return net_wifi_rssi ? net_wifi_rssi() : 0; }
// Cho file khac (ui_chrome.cpp: chu dBm canh icon WiFi tren man STATUS) doc duoc RSSI: ham
// wifi_rssi_now() la `static` trong file nay nen phai co cong ra ngoai (khai bao o ui_lvgl_int.h).
int8_t ui_wifi_rssi(void) { return wifi_rssi_now(); }
// Co API WiFi khong (de trang WIFI bao "khong ho tro" thay vi hien "TAT" gay hieu nham).
bool wifi_api_ok(void) { return net_wifi_scan != NULL; }

#if UI_MONO_FONT == 4
#if UI_MONO_GFX_BPP == 4
static const GFXfont *const UI_FONT_RANGES[UI_GFX_RANGE_COUNT] =
  UI_GFX_RANGES(gfx4_font_robotomono);
#else
static const GFXfont *const UI_FONT_RANGES[UI_GFX_RANGE_COUNT] =
  UI_GFX_RANGES(gfx_font_robotomono);
#endif

// Dien trong ui_lvgl_begin() (sau lv_init). Neu NULL (font loi) thi UI_MONO_FACE lui ve
// Montserrat: mat dau tieng Viet nhung KHONG mat het chu - duong lui an toan.
// KHONG con `static`: ui_widgets.cpp cung dung UI_MONO_FACE -> phai la MOT bien duy nhat
// (khai bao `extern` trong ui_lvgl_int.h).
const lv_font_t *s_gfx_mono = NULL;
#endif


// Port day pixel ra LCD lam viec truc tiep tren buffer 16-bit (xem ui_lvgl_port.cpp)
static_assert(LV_COLOR_DEPTH == 16, "UI nay chi ho tro LV_COLOR_DEPTH 16");
static_assert(sizeof(lv_color_t) == 2, "lv_color_t phai la 2 byte");


// ===========================================================================
//  Man hinh
// ===========================================================================

static const char *const PG_TITLE[PG_COUNT] = {"STATUS", "MENU", "FEED/FLOW", "NHIET DO", "JOG",
                                               "FILE SD", "THONG SO", "BED MESH", "CUSTOM",
                                               "TRAMMING", "INFO", "ENDSTOP", "WIFI"};

// ---------------------------------------------------------------------------
//  Bien toan cuc DUNG CHUNG giua cac file: bo `static` + khai bao `extern` o ui_lvgl_int.h
//  (chi dinh nghia MOT lan o day).
// ---------------------------------------------------------------------------
lv_obj_t   *s_page[PG_COUNT];              // container noi dung tung man (NULL = chua tao)
lv_obj_t   *s_bar;                         // dong tieu de chung: [ten man] ... [LINK][BACK] (an o STATUS)
lv_obj_t   *s_title, *s_link;
lv_obj_t   *s_btn_pause, *s_btn_stop;      // P9: 2 nut nho chi hien khi dang in tu the SD
lv_group_t *s_group;
uint8_t     s_page_act = PG_STATUS;

// ---------------------------------------------------------------------------
//  Bien chi dung trong file nay -> VAN giu `static`
// ---------------------------------------------------------------------------
static uint8_t    s_link_state = 255;      // 255 = chua set lan nao (`link_state_now()` tra 0/1/2)
static uint32_t   s_tick_ms, s_upd_ms, s_slow_ms;

static void create_page(uint8_t p);          // forward
// do_action() la API chung (khai bao o ui_lvgl_int.h) vi man FILE (ui_page_files.cpp) cung goi no
// (A_REQ_FILES / A_INITSD). files_delete_now() / ov_open_id() cung o file rieng - xem ui_lvgl_int.h.


void cb_action(lv_event_t *e) {
  do_action((int)(intptr_t)lv_event_get_user_data(e));
}

void do_action(const int a) {
  switch (a) {
    // BACK: tu man con -> MENU, tu MENU -> STATUS (giong "up" trong menu Marlin)
    case A_BACK:          show_page(s_page_act == PG_MENU ? PG_STATUS : PG_MENU); break;
    case A_GOTO_MENU:     show_page(PG_MENU); break;
    case A_GOTO_TEMP:     show_page(PG_TEMP); break;
    case A_GOTO_JOG:      show_page(PG_JOG); break;
    case A_GOTO_FILES:    show_page(PG_FILES); break;
    case A_GOTO_SETTINGS: show_page(PG_SETTINGS); break;
    case A_GOTO_RATE:     show_page(PG_RATE); break;
    case A_GOTO_STATUS:   show_page(PG_STATUS); break;   // nut HOME: ve thang man STATUS
    case A_GOTO_WIFI:     show_page(PG_WIFI); break;     // icon WiFi / muc MENU: mo trang WIFI
    case A_GOTO_ENDSTOP:  show_page(PG_ENDSTOP); break;
    case A_HOME:          cydlcd_link_send_gcode("G28"); break;
    case A_HOME_X:        cydlcd_link_send_gcode("G28 X"); break;
    case A_HOME_Y:        cydlcd_link_send_gcode("G28 Y"); break;
    case A_HOME_Z:        cydlcd_link_send_gcode("G28 Z"); break;
    case A_STEPPERS_OFF:  cydlcd_link_send_gcode("M84"); break;
    case A_PAUSE:         cydlcd_link_send_gcode("M25"); break;
    case A_RESUME:        cydlcd_link_send_gcode("M24"); break;
    case A_STOP:          ui_confirm("DUNG IN", "Dung han ban in? (M524)", (uint8_t)A_STOP_NOW, 1); break;
    case A_STOP_NOW:      cydlcd_link_send_gcode("M524"); break;
    case A_COOLDOWN:      cydlcd_link_send_gcode("M104 S0"); cydlcd_link_send_gcode("M140 S0"); break;
    case A_SAVE:          cydlcd_link_request((uint8_t)CYDLCD_REQ_SAVE_SETTINGS); break;
    case A_INITSD:        cydlcd_link_send_gcode2("M22", "M21"); break;   // nha the SD roi init lai
    case A_G34:           cydlcd_link_send_gcode("G34 Q99"); break;
    case A_REQ_FILES:     cydlcd_link_request((uint8_t)CYDLCD_REQ_FILES); break;
    case A_REQ_UP:        cydlcd_link_request((uint8_t)CYDLCD_REQ_FILES_UP); break;
    case A_REQ_MESH:      cydlcd_link_request((uint8_t)CYDLCD_REQ_MESH); break;
    case A_REQ_SETTINGS:  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS); break;
    case A_REQ_ENDSTOPS:  cydlcd_link_request((uint8_t)CYDLCD_REQ_ENDSTOPS); break;

    // ---- P1 UBL ----
    case A_G29_P1:   cydlcd_link_send_gcode("G29 P1");   break;   // do mesh
    case A_G29_FILL: cydlcd_link_send_gcode("G29 P3T0"); break;   // dien o trong bang gia tri gan dung
    case A_G29_FT:   cydlcd_link_send_gcode("G29 P4RT"); break;   // sua tung diem (fine tune)
    case A_G29_ON:   cydlcd_link_send_gcode("G29 A");    break;   // bat UBL
    case A_G29_OFF:  cydlcd_link_send_gcode("G29 D");    break;   // tat UBL
    case A_G29_SAVE: cydlcd_link_send_gcode("G29 S0");   break;   // luu mesh vao slot 0

    // ---- P2/P8 ----
    case A_EDIT_PROBE_Z: ov_open_id(CYDLCD_SET_PROBE_OFFSET_Z); break;   // mo lop phu sua M851 Z
    // M502 tra thong so trong RAM ve MAC DINH cua firmware -> XAC NHAN 2 BUOC (giong REBOOT DFU)
    // vi bam nham roi bam tiep SAVE EEPROM se ghi de vinh vien len chip EEPROM I2C.
    case A_EEPROM_RESET: ui_confirm("M502 RESET",
                                    "Tra thong so trong RAM ve MAC DINH?\nChi ghi vao EEPROM neu sau do bam SAVE",
                                    (uint8_t)A_EEPROM_RESET, 2); break;
    case A_REBOOT_DFU:   ui_confirm("REBOOT DFU", "Nhay vao ROM DFU bootloader?", (uint8_t)A_REBOOT_DFU, 2); break;
    case A_FILAMENT_CHANGE: cydlcd_link_send_gcode("M600 B0"); break;   // doi filament, khong lui dai
    case A_RUNOUT_TOGGLE: break;                                        // xu ly rieng (xem cb_runout_toggle)

    // ---- P5 tuning ----
    case A_MPC_TUNE: cydlcd_link_send_gcode("M306 T");       break;   // MPC autotune dau in
    case A_PID_BED:  cydlcd_link_send_gcode("M303 E-1 S60"); break;   // PID ban in, target 60 do C

    // ---- thanh day: lui/toi 1 trang noi dung (xem bot_sync de biet khi nao co thanh nay) ----
    case A_PAGE_PREV: page_scroll(-1); break;
    case A_PAGE_NEXT: page_scroll(+1); break;
    default: break;
  }
}

// Nut M502 RESET va Reboot DFU can chay lenh THAT SU sau khi nguoi dung xac nhan du so buoc
// (A_EEPROM_RESET/A_REBOOT_DFU o tren chi mo hop xac nhan).
void do_confirmed(const uint8_t act) {
  switch (act) {
    case A_EEPROM_RESET:
      cydlcd_link_send_gcode("M502");
      cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);
      break;
    case A_REBOOT_DFU:
      cydlcd_link_request((uint8_t)CYDLCD_REQ_DFU);     // Marlin: STM32_DFU_REBOOT
      break;
    case A_FILES_DELETE:
      files_delete_now();                             // xoa file SD dang chon (M30) - man FILE
      break;
    case A_WIFI_FORGET:
      if (net_wifi_disconnect) net_wifi_disconnect(true);   // xoa cau hinh WiFi (quen NVS)
      break;
    default:
      do_action((int)act);                            // cac lenh khac (vd M524)
      break;
  }
}


// ===========================================================================
//  MESH -- luoi bed mesh dung HET be rong + 2 hang nut UBL (G29)
//    info 17 + luoi 124 + hang nut 28 + hang nut 28 + 3*GAP 6 = 203 <= PAGE_INN_H 206
//  Da BO theo yeu cau:
//    - nut REFRESH (Marlin TU day lai luoi khi mesh doi / khi link hoi phuc; enum A_REQ_MESH
//      van giu lai trong do_action() cho gon, hien khong con nut nao dung),
//    - 3 dong chu thich mau o cot phai ("xanh = cao" / "do = thap" / "+-0.05mm"),
//    - dong chu thich duoi luoi ("- o ghi mm - REFRESH = xin lai luoi").
//  Nho bo cot phai, luoi rong tu 214 -> 312 px (o rong hon ~45%) va cao 104 -> 124 px (o cao
//  hon ~19%: voi luoi 7x7 la 30x15 -> 44x18 px). Tong 203 <= 206 nen KHONG tran -> thanh day
//  PREV/NEXT khong hien (bot_sync van chay dung nhu moi man khac).
// ===========================================================================
#define MESH_TABLE_H 124
#define MESH_TABLE_W PAGE_INN_W                         // 312: luoi dung het be rong
#define MESH_CELL_W  38                                 // be rong toi thieu 1 o: du "-0.20" (33 px) + 2*2 pad

static lv_obj_t *me_table, *me_info, *me_note;
static uint8_t   s_mesh_nx = 0, s_mesh_ny = 0;

// Nguong to mau: |z| > 0.05mm thi to xanh (cao) / do (thap)
#define MESH_HI_UM  50
#define MESH_LO_UM (-50)

// LVGL 8.3: LV_EVENT_DRAW_PART_BEGIN cho o cua bang -> dsc->id = chi so o
// (id = row * so_cot + col). Vat lieu to mau rieng tung o.
static void mesh_draw_cb(lv_event_t *e) {
  lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
  if (!dsc || dsc->part != LV_PART_ITEMS || !dsc->rect_dsc) return;

  lv_obj_t *tbl = lv_event_get_target(e);
  const uint16_t cols = lv_table_get_col_cnt(tbl);
  if (!cols) return;

  const uint32_t row = dsc->id / cols;
  const uint32_t col = dsc->id % cols;
  if (col >= cydlcd.mesh_nx || row >= cydlcd.mesh_ny) return;

  const int32_t z = cydlcd.mesh[col][row];          // cydlcd_link.cpp: mesh[cot][hang]
  lv_color_t bg = COL_BTN;
  if (z > MESH_HI_UM)      bg = lv_color_hex(0x1E6B3A);   // xanh: diem cao
  else if (z < MESH_LO_UM) bg = lv_color_hex(0x7A2626);   // do  : diem thap
  dsc->rect_dsc->bg_color = bg;
  dsc->rect_dsc->bg_opa   = LV_OPA_COVER;
}

static void page_mesh_create(lv_obj_t *p) {
  if (!(cydlcd.hello_flags & CYDLCD_HAS_MESH)) {
    mk_spacer(p, 1, 30, 0);                      // day chu thich xuong giua vung noi dung
    me_note = mk_label_w(p, "khong ho tro (CYDLCD_HAS_MESH=0)", COL_WARN, PAGE_INN_W);
    (void)me_note;
    return;
  }

  me_info = mk_label_w(p, "mesh -", COL_TXT, PAGE_INN_W);

  // Luoi la CON TRUC TIEP cua trang (da bo cot nut ben phai) -> rong het 312 px.
  me_table = lv_table_create(p);
  lv_obj_set_size(me_table, MESH_TABLE_W, MESH_TABLE_H);
  lv_table_set_col_cnt(me_table, 1);
  lv_table_set_row_cnt(me_table, 1);
  lv_obj_set_style_bg_color(me_table, COL_BG, 0);
  lv_obj_set_style_bg_opa(me_table, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(me_table, COL_DIM, 0);
  lv_obj_set_style_border_width(me_table, 1, 0);
  lv_obj_set_style_pad_all(me_table, 0, 0);
  lv_obj_set_style_text_color(me_table, COL_TXT, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(me_table, COL_BTN, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(me_table, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_border_color(me_table, COL_DIM, LV_PART_ITEMS);
  lv_obj_set_style_border_width(me_table, 1, LV_PART_ITEMS);
  lv_obj_set_style_pad_all(me_table, 2, LV_PART_ITEMS);
  lv_obj_add_event_cb(me_table, mesh_draw_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
  if (s_group) lv_group_add_obj(s_group, me_table);       // de encoder co the focus/cuon

  // Da bo: nut REFRESH + 3 dong chu thich mau (cot phai) va dong chu thich duoi luoi.

  // P1 - UBL: 2 hang x 3 nut (G29 P1 / P3T0 / P4RT roi bat-tat UBL + luu slot 0)
  lv_obj_t *ru1 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 28, 0, GAP);
  mk_btn_grow(ru1, "G29 P1",   28, cb_action, (void *)(intptr_t)A_G29_P1);
  mk_btn_grow(ru1, "G29 P3T0", 28, cb_action, (void *)(intptr_t)A_G29_FILL);
  mk_btn_grow(ru1, "G29 P4RT", 28, cb_action, (void *)(intptr_t)A_G29_FT);

  lv_obj_t *ru2 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 28, 0, GAP);
  mk_btn_grow(ru2, "UBL ON",   28, cb_action, (void *)(intptr_t)A_G29_ON);
  mk_btn_grow(ru2, "UBL OFF",  28, cb_action, (void *)(intptr_t)A_G29_OFF);
  mk_btn_grow(ru2, "SAVE S0",  28, cb_action, (void *)(intptr_t)A_G29_SAVE);
}

static void page_mesh_update(void) {
  if (!me_table) return;

  // So cot/hang doi -> tao lai khung bang (chi khi kich thuoc luoi doi)
  if (cydlcd.mesh_nx && cydlcd.mesh_ny && (cydlcd.mesh_nx != s_mesh_nx || cydlcd.mesh_ny != s_mesh_ny)) {
    s_mesh_nx = cydlcd.mesh_nx;
    s_mesh_ny = cydlcd.mesh_ny;
    lv_table_set_col_cnt(me_table, s_mesh_nx);
    lv_table_set_row_cnt(me_table, s_mesh_ny);
    // O rong = chia deu be rong bang; luoi nhieu cot thi giu toi thieu MESH_CELL_W
    // (bang tu cuon ngang: lv_table bao kich thuoc noi dung = tong be rong cac cot)
    const lv_coord_t tw  = lv_obj_get_width(me_table);
    const lv_coord_t fit = (lv_coord_t)((tw > 0 ? tw : MESH_TABLE_W) / (s_mesh_nx ? s_mesh_nx : 1));
    const lv_coord_t cw  = (fit > MESH_CELL_W) ? fit : MESH_CELL_W;
    lv_table_set_col_width(me_table, 0, cw);
    for (uint16_t c = 1; c < s_mesh_nx; c++) lv_table_set_col_width(me_table, c, cw);
  }

  char b[64], a[12];
  int32_t vmin = 0, vmax = 0;
  bool first = true;
  for (uint8_t r = 0; r < cydlcd.mesh_ny; r++) {
    for (uint8_t c = 0; c < cydlcd.mesh_nx; c++) {
      const int32_t z = cydlcd.mesh[c][r];
      if (first) { vmin = vmax = z; first = false; }
      if (z < vmin) vmin = z;
      if (z > vmax) vmax = z;
      fmt_scaled(a, sizeof(a), z, 1000, 2);
      const char *old = lv_table_get_cell_value(me_table, r, c);
      if (!old || strcmp(old, a) != 0) lv_table_set_cell_value(me_table, r, c, a);
    }
  }

  char b1[12], b2[12];
  fmt_scaled(b1, sizeof(b1), vmin, 1000, 2);
  fmt_scaled(b2, sizeof(b2), vmax, 1000, 2);
  snprintf(b, sizeof(b), "mesh %ux%u  rows %u  %s..%s%s", (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny,
           (unsigned)cydlcd.mesh_rows, b1, b2, cydlcd.mesh_valid ? "  valid" : "  -");
  set_text(me_info, b);
}

// Huy man BED MESH (xem quy uoc o ui_lvgl_int.h): dat NULL bang/nhan va QUEN kich thuoc luoi da ve.
// VI SAO phai quen s_mesh_nx/s_mesh_ny: page_mesh_update() chi dat lai so cot/hang khi kich thuoc luoi
// DOI so voi gia tri da nho - ma bang moi luon duoc tao 1 cot x 1 hang, nen neu giu cache cu thi
// luoi se KHONG duoc dung lai (bang 1x1 = khong hien gi).
void page_mesh_destroy(void) {
  me_table = NULL;
  me_info  = NULL;
  me_note  = NULL;
  s_mesh_nx = 0;
  s_mesh_ny = 0;
}

// ===========================================================================
//  CUSTOM -- G34/Init SD (nhu menu cu) + P4 endstop M119 + P6 filament + P8 DFU
//  (MENU da het cho: 10 dong x 22 = 220 > 206 nen cac muc moi nam o day)
// ===========================================================================
static lv_obj_t *cu_runout;                     // nut toggle M412 (P6)

// P6: bat/tat cam bien het filament. Trang thai dau lay tu SETTING RUNOUT_EN, sau do
// dao moi lan bam (Marlin khong day lai ngay trang thai nay trong frame STATUS).
static bool s_runout_on = true;

// To lai nut theo s_runout_on. Chi ghi mau khi THAT SU doi (giong jog_axis_mark/ov_marks): ham nay
// duoc goi moi chu ky cap nhat cua man CUSTOM (400 ms) va lv_obj_set_style_bg_color() luon
// invalidate vat -> goi vo dieu kien se ton bang thong SPI.
static void runout_mark(void) {
  if (!cu_runout) return;
  set_text(lv_obj_get_child(cu_runout, 0), s_runout_on ? "M412: BAT (S1)" : "M412: TAT (S0)");
  const lv_color_t want = s_runout_on ? COL_OK : COL_BTN;
  if (lv_obj_get_style_bg_color(cu_runout, 0).full != want.full)
    lv_obj_set_style_bg_color(cu_runout, want, 0);
}

static void cb_runout_toggle(lv_event_t *e) {
  (void)e;
  s_runout_on = !s_runout_on;
  cydlcd_link_send_gcode(s_runout_on ? "M412 S1" : "M412 S0");
  runout_mark();
}

static void page_custom_create(lv_obj_t *p) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 40, 0, GAP);
  mk_btn_grow(r, "G34 Bed", 40, cb_action, (void *)(intptr_t)A_G34);
  mk_btn_grow(r, "Init SD", 40, cb_action, (void *)(intptr_t)A_INITSD);
  mk_btn_grow(r, "ENDSTOP", 40, cb_action, (void *)(intptr_t)A_GOTO_ENDSTOP);   // P4 -> man M119

  // P6: doi filament (M600) + cong tac cam bien het filament (M412)
  lv_obj_t *rf = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 30, 0, GAP);
  mk_btn_grow(rf, "M600 B0", 30, cb_action, (void *)(intptr_t)A_FILAMENT_CHANGE);
  cu_runout = mk_btn_grow(rf, "M412: BAT (S1)", 30, cb_runout_toggle, NULL);

  // P8: nhay vao ROM DFU bootloader (2 buoc xac nhan)
  lv_obj_t *rd = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 30, 0, GAP);
  mk_btn_grow(rd, "REBOOT DFU", 30, cb_action, (void *)(intptr_t)A_REBOOT_DFU);

  mk_label_w(p, "G34 Q99 - can bang 2 truc Z", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "M600 = doi filament (khong lui dai)", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "M412 = cam bien het filament ON/OFF", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "REBOOT DFU = nap firmware qua USB", COL_DIM, PAGE_INN_W);
  // Tong: 40 + 30 + 30 + 4*16 + 6*GAP = 176 <= 206
}

static void page_custom_update(void) {
  if (!cu_runout) return;                // trang da bi thu hoi (an toan: xem page_custom_destroy)
  // Trang thai M412: dong bo lan dau tu SETTING (neu Marlin co gui CYDLCD_SET_RUNOUT_EN)
  static bool got = false;
  if (!got) {
    const int32_t v = cydlcd_setting(CYDLCD_SET_RUNOUT_EN, -1);
    if (v >= 0) { got = true; s_runout_on = (v != 0); }
  }
  // Ve lai nhan/nem theo trang thai dang giu: BAT BUOC sau khi trang duoc TAO LAI (bi thu hoi roi mo
  // lai) vi nut moi luon bat dau bang nhan "M412: BAT (S1)" + nen COL_BTN, con s_runout_on thi giu
  // lua chon cu cua nguoi dung. runout_mark() chi ghi style khi mau THAT SU doi -> re (khong
  // invalidate moi 400 ms).
  runout_mark();
}

// Huy man CUSTOM (xem quy uoc o ui_lvgl_int.h): dat NULL nut M412. Container trang do show_page()
// xoa bang lv_obj_del() nen khong xoa widget o day. s_runout_on: GIU NGUYEN (lua chon cua nguoi
// dung) - page_custom_update() to lai nut MOI theo dung trang thai do (xem runout_mark).
void page_custom_destroy(void) {
  cu_runout = NULL;
}

// ===========================================================================
//  ENDSTOP (P4) -- trang thai kieu M119 tu frame CYDLCD_F_ENDSTOPS
//  6 o: PROBE / X min / Y min / Z min / X max / Y max (id theo cydlcd_endstop_id_t)
//  May dung Z_MIN_PROBE_USES_Z_MIN_ENDSTOP_PIN thi chi bao PROBE (id 0), Z min luon "mo".
//  Chieu cao: 16 + 6*20 + 30 + 7*GAP = 180 <= 206
// ===========================================================================
#define ES_ROW_H 20
#define ES_NAME_W 120

static const uint8_t ES_ID[CYDLCD_ENDSTOP_MAX] = {
  CYDLCD_ES_PROBE, CYDLCD_ES_X_MIN, CYDLCD_ES_Y_MIN, CYDLCD_ES_Z_MIN, CYDLCD_ES_X_MAX, CYDLCD_ES_Y_MAX
};
static const char *const ES_NAME[CYDLCD_ENDSTOP_MAX] = {
  "PROBE", "X min", "Y min", "Z min", "X max", "Y max"
};
static lv_obj_t *es_val[CYDLCD_ENDSTOP_MAX], *es_info;

static void page_endstop_create(lv_obj_t *p) {
  es_info = mk_label_w(p, "M119: -", COL_DIM, PAGE_INN_W);
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) {
    lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, ES_ROW_H, 0, GAP);
    mk_label_w(r, ES_NAME[i], COL_TXT, ES_NAME_W);
    es_val[i] = mk_label_w(r, "-", COL_DIM, PAGE_INN_W - ES_NAME_W - GAP);
  }
  mk_btn(p, "REFRESH (M119)", PAGE_INN_W, 30, cb_action, (void *)(intptr_t)A_REQ_ENDSTOPS);
  cydlcd_link_request((uint8_t)CYDLCD_REQ_ENDSTOPS);       // doc ngay khi mo man
}

static void page_endstop_update(void) {
  if (!es_info) return;                  // trang da bi thu hoi (an toan: xem page_endstop_destroy)
  char b[40];
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) {
    const bool hit = (cydlcd.es_hit[ES_ID[i]] != 0);
    set_text(es_val[i], hit ? "CHAM (triggered)" : "mo (open)");
    lv_obj_set_style_text_color(es_val[i], hit ? COL_ERR : COL_OK, 0);
  }
  snprintf(b, sizeof(b), "M119: %u o - bam REFRESH de doc lai", (unsigned)cydlcd.n_endstops);
  set_text(es_info, b);
}

// Huy man ENDSTOP (xem quy uoc o ui_lvgl_int.h): dat NULL 6 nhan gia tri + dong info. Container
// trang do show_page() xoa bang lv_obj_del() nen khong xoa widget o day.
void page_endstop_destroy(void) {
  es_info = NULL;
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) es_val[i] = NULL;
}

// ===========================================================================
//  INFO -- 11 dong thong tin THAT (moi dong 1 label rong het PAGE_INN_W)
//
//  Chieu cao: 11 dong * 17 px (font mono che do 4: line_height 17) + 10 * INFO_ROW_GAP(1) = 197
//  <= PAGE_INN_H 206 -> KHONG tran, thanh day PREV/NEXT khong hien. (Neu line_height la 16 - che
//  do font khac - thi 186, van thua cho.)
//
//  Truoc day: cot 7 dong cao CO DINH 124 + nut REFRESH FULL + 1 dong chu thich. Da bo nut va dong
//  chu thich (theo yeu cau) va bo luon KHUNG CAO CO DINH: nay cac label la CON TRUC TIEP cua
//  trang, ma mk_page da bat LV_OBJ_FLAG_SCROLLABLE -> noi dung dai hon man thi trang TU CUON duoc
//  va bot_sync() se hien thanh day phan trang. Khong tu chan cuon o dau.
// ===========================================================================
#define INFO_ROW_GAP 1     // GAP chuan cua mk_page la 2: 11 dong se thanh 207 > 206 (cat 1 px)

static lv_obj_t *in_fw, *in_board, *in_ui, *in_font, *in_wifi, *in_proto, *in_marlin, *in_stat,
                *in_rx, *in_sd, *in_sys;

static void page_info_create(lv_obj_t *p) {
  // Rieng man nay cach nhau 1 px (11 dong) - xem INFO_ROW_GAP.
  lv_obj_set_style_pad_row(p, INFO_ROW_GAP, 0);

  // Thu tu tao = thu tu hien (mk_page la flex COLUMN).
  in_fw     = mk_label_w(p, "FW: -",     COL_TXT, PAGE_INN_W);
  in_board  = mk_label_w(p, "BOARD: -",  COL_TXT, PAGE_INN_W);
  in_ui     = mk_label_w(p, "UI: -",     COL_DIM, PAGE_INN_W);
  in_font   = mk_label_w(p, "FONT: -",   COL_DIM, PAGE_INN_W);
  in_wifi   = mk_label_w(p, "WIFI: -",   COL_DIM, PAGE_INN_W);
  in_proto  = mk_label_w(p, "PROTO: -",  COL_DIM, PAGE_INN_W);
  in_marlin = mk_label_w(p, "MARLIN: -", COL_DIM, PAGE_INN_W);
  in_stat   = mk_label_w(p, "LINK: -",   COL_DIM, PAGE_INN_W);
  in_rx     = mk_label_w(p, "RX: -",     COL_DIM, PAGE_INN_W);
  in_sd     = mk_label_w(p, "SD: -",     COL_DIM, PAGE_INN_W);
  in_sys    = mk_label_w(p, "SYS: -",    COL_DIM, PAGE_INN_W);
}

static void page_info_update(void) {
  if (!in_fw) return;                    // trang da bi thu hoi (an toan: xem page_info_destroy)
  char b[64], t[16];

  // Ten may: o che do PUSH lay tu goi HELLO cua Marlin. O che do TRAM khong co HELLO
  // (link chi gui dong "TBT ...") nen dung ten khai trong cydlcd_config.h.
  snprintf(b, sizeof(b), "FW: %s", cydlcd.name[0] ? cydlcd.name
           : (tbt.ever ? CYDLCD_MACHINE_NAME " (TBT link)" : "(chua co HELLO)"));
  set_text(in_fw, b);

  // Board + do phan giai + so heater/quat MA MARLIN BAO trong HELLO (chua co HELLO thi thoi,
  // khong in so 0 gia).
  int n = snprintf(b, sizeof(b), "BOARD: %s %dx%d", CYDLCD_NAME, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  if (n > 0 && (size_t)n < sizeof(b) && (cydlcd.n_heaters || cydlcd.n_fans_max))
    snprintf(b + n, sizeof(b) - (size_t)n, " h%u f%u",
             (unsigned)cydlcd.n_heaters, (unsigned)cydlcd.n_fans_max);
  set_text(in_board, b);

  // Phien ban LVGL THAT (macro cua lvgl.h - truoc day ghi cung "LVGL 8.3", sai vi dang la 8.4)
  // + kieu dau vao dang dung (encoder/touch) + che do font (4 = mono GFX, 0 = Montserrat).
#if UI_MONO_FONT == 4
  snprintf(b, sizeof(b), "UI: LVGL %d.%d %s mono4", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
           ui_port_input_is_encoder() ? "encoder" : "touch");
#else
  snprintf(b, sizeof(b), "UI: LVGL %d.%d %s montserrat", LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR,
           ui_port_input_is_encoder() ? "encoder" : "touch");
#endif
  set_text(in_ui, b);

  // Che do 4: bpp + so dai glyph + ket qua TU KIEM TRA driver luc khoi dong
  // (s_gfx_mono NULL = driver loi -> UI dang dung lui ve Montserrat 14).
  // Che do 0: chi Montserrat -> noi ro KHONG co dau tieng Viet.
#if UI_MONO_FONT == 4
  snprintf(b, sizeof(b), "FONT: mono4 %dbpp %d dai %s", UI_MONO_GFX_BPP, (int)UI_GFX_RANGE_COUNT,
           s_gfx_mono ? "OK" : "LOI->MS14");
#else
  snprintf(b, sizeof(b), "FONT: Montserrat (khong dau)");
#endif
  set_text(in_font, b);

  // Trang thai WiFi - CUNG NGUON voi icon o header (wifi_state_now(), xem dau file).
  static const char *const WIFI_TXT[3] = { "OFF", "DANG KET NOI", "DA KET NOI" };
  const uint8_t ws = wifi_state_now();
  snprintf(b, sizeof(b), "WIFI: %s", WIFI_TXT[ws <= 2 ? ws : 0]);
  set_text(in_wifi, b);

  // Phien ban protocol PUSH: cua Marlin (gui trong HELLO) / cua MAN HINH CYD + co nang luc (hello_flags).
  snprintf(b, sizeof(b), "PROTO: %u/%u cap 0x%02X", (unsigned)cydlcd.proto,
           (unsigned)CYDLCD_PROTO_VERSION, (unsigned)cydlcd.hello_flags);
  set_text(in_proto, b);

  // marlin_fw (u16 trong HELLO) + SHORT_BUILD_VERSION neu Marlin gui (proto >= 2).
  if (cydlcd.marlin_ver[0])
    snprintf(b, sizeof(b), "MARLIN: 0x%04X %s", (unsigned)cydlcd.marlin_fw, cydlcd.marlin_ver);
  else
    snprintf(b, sizeof(b), "MARLIN: 0x%04X", (unsigned)cydlcd.marlin_fw);
  set_text(in_marlin, b);

  // Link (PUSH/TRAM) + trang thai may.
  snprintf(b, sizeof(b), "LINK: %s %s %s", link_ok() ? "OK" : "NO",
           tbt.ever ? "(TBT)" : (cydlcd.linked ? "(PUSH)" : ""), state_name(cydlcd.state));
  set_text(in_stat, b);

  // RX/ERR: 2 CO MOT CHIEU (cydlcd_link.h) - khong con bo dem nen KHONG BAO GIO tran:
  //   rx_frames = da nhan duoc it nhat 1 frame hop le; rx_errors = da tung gap frame loi.
  snprintf(b, sizeof(b), "RX: %s  ERR: %s", cydlcd.rx_frames ? "OK" : "--",
           cydlcd.rx_errors ? "co" : "khong");
  set_text(in_rx, b);

  // Danh sach file SD + luoi bed mesh (ca 2 deu lay tu du lieu Marlin da gui).
  if (cydlcd.mesh_valid)
    snprintf(b, sizeof(b), "SD: %u file  mesh %ux%u", (unsigned)cydlcd.n_files,
             (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny);
  else
    snprintf(b, sizeof(b), "SD: %u file  mesh -", (unsigned)cydlcd.n_files);
  set_text(in_sd, b);

  // Thoi gian chay cua MAN HINH CYD + heap tu do (chi ESP32 moi co ESP.getFreeHeap()).
  fmt_time(t, sizeof(t), ui_port_millis() / 1000U);
#if defined(ARDUINO_ARCH_ESP32)
  snprintf(b, sizeof(b), "SYS: up %s heap %lu", t, (unsigned long)ESP.getFreeHeap());
#else
  snprintf(b, sizeof(b), "SYS: up %s", t);
#endif
  set_text(in_sys, b);
}

// Huy man INFO (xem quy uoc o ui_lvgl_int.h): dat NULL 11 nhan. Container trang do show_page()
// xoa bang lv_obj_del() nen khong xoa widget o day. Khong co trang thai rieng phai tra ve ban dau:
// moi dong deu duoc page_info_update() dung lai tu du lieu cua Marlin / cua man hinh.
void page_info_destroy(void) {
  in_fw = NULL; in_board = NULL; in_ui = NULL; in_font = NULL; in_wifi = NULL;
  in_proto = NULL; in_marlin = NULL; in_stat = NULL; in_rx = NULL; in_sd = NULL; in_sys = NULL;
}

// ===========================================================================
//  WIFI -- trang quan ly WiFi (mo tu: icon WiFi o header / man STATUS, hoac muc "WiFi" trong MENU)
//
//  BO CUC (PAGE_INN_H = 206; font mono line_height 17; gap GAP = 2):
//    WiFi: <trang thai> <RSSI>    17
//    SSID: <ssid>                 17
//    IP: <ip>                     17
//    [ MAT KHAU: ... ]            26
//    [ roller "-58dBm <ssid>" ]   68   (4 dong, cuon duoc)
//    [QUET][KET NOI][NGAT][XOA]   26
//    => 17*3 + 26 + 68 + 26 + 5*GAP(10) = 181 <= 206 -> KHONG tran (thanh day khong hien).
//
//  API WiFi deu `weak` (xem khoi khai bao o dau file): thieu module -> trang VAN MO DUOC, hien
//  "KHONG HO TRO", moi nut khong lam gi (khong crash, khong loi link).
//  Nut QUET goi net_wifi_scan() BLOCKING ~2-4 s -> hien lop phu "DANG QUET MANG..." + KHOA input
//  (lv_indev_enable) trong luc quet (giong cach nhanh firmware lam o pha init), xong moi mo lai.
// ===========================================================================
#define WIFI_ROLLER_H  (4 * 17)        // 68 = 4 dong x line_height 17 (mono mode 4)
#define WIFI_SCAN_SHOW 8               // toi da 8 mang vao roller (o 68 px van cuon duoc)
#define WIFI_PASS_MAX  64

static lv_obj_t *wf_state, *wf_ssid, *wf_ip, *wf_pass, *wf_roller;
static char       s_wifi_pass[WIFI_PASS_MAX] = "";  // mat khau da nhap (KHONG hien ra man hinh)
static bool       s_wifi_busy = false;              // dang quet -> khoa moi callback cua trang
static lv_indev_t *s_indev = NULL;                  // de khoa/mo input khi quet (gan o ui_lvgl_begin)

// ---- lop phu "DANG QUET..." (tao 1 lan roi an/hien) ----
static lv_obj_t *s_wf_ov, *s_wf_ov_lbl;

static void wifi_ov_show(const bool on, const char *txt) {
  if (!s_wf_ov) {
    s_wf_ov = lv_obj_create(lv_scr_act());
    lv_obj_set_pos(s_wf_ov, 0, 0);
    lv_obj_set_size(s_wf_ov, UI_LCD_HOR_RES, UI_LCD_VER_RES);
    lv_obj_set_style_bg_color(s_wf_ov, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_wf_ov, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_wf_ov, 0, 0);
    lv_obj_set_style_radius(s_wf_ov, 0, 0);
    lv_obj_set_style_pad_all(s_wf_ov, 0, 0);
    lv_obj_clear_flag(s_wf_ov, LV_OBJ_FLAG_SCROLLABLE);
    s_wf_ov_lbl = mk_label_mid(s_wf_ov, 0, 104, UI_LCD_HOR_RES, "", COL_TXT);
    lv_obj_add_flag(s_wf_ov, LV_OBJ_FLAG_HIDDEN);
  }
  if (txt) set_text(s_wf_ov_lbl, txt);
  if (on) lv_obj_clear_flag(s_wf_ov, LV_OBJ_FLAG_HIDDEN);
  else    lv_obj_add_flag(s_wf_ov, LV_OBJ_FLAG_HIDDEN);
}

// ---- lop phu NHAP MAT KHAU: TAO khi bam nut "MAT KHAU...", XOA ngay khi dong (tra pool LVGL) ----
// CHI co khi env bat ban phim: `src/lv_conf_cydlcd.h` (env CYD) dat LV_USE_KEYBOARD/TEXTAREA = 1.
// Env STM32 LVGL dung `lv_conf.h` (de 0) -> khong co ban phim: nut MAT KHAU chi bao "nhap qua COM7"
// (xem nhanh #else) chu KHONG lam hong build.
#if LV_USE_KEYBOARD && LV_USE_TEXTAREA
static lv_obj_t *s_kb_bd, *s_kb_ta, *s_kb_kb;

static void wifi_kb_close(void) {
  if (s_kb_bd) {
    lv_obj_del_async(s_kb_bd);      // xoa trong event cua chinh no -> phai dung _async
    s_kb_bd = NULL; s_kb_ta = NULL; s_kb_kb = NULL;
  }
  refocus_page();                   // tra focus ve man WIFI
}

static void cb_wifi_kb(lv_event_t *e) {
  switch (lv_event_get_code(e)) {
    case LV_EVENT_READY:            // phim "v" (dong y) tren ban phim
      if (s_kb_ta) {
        const char *t = lv_textarea_get_text(s_kb_ta);
        snprintf(s_wifi_pass, sizeof(s_wifi_pass), "%s", t ? t : "");
      }
      wifi_kb_close();
      break;
    case LV_EVENT_CANCEL:           // phim "x" (huy)
      wifi_kb_close();
      break;
    default: break;
  }
}

static void cb_wifi_pass(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || s_kb_bd) return;
  s_kb_bd = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(s_kb_bd, 0, 0);
  lv_obj_set_size(s_kb_bd, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  lv_obj_set_style_bg_color(s_kb_bd, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(s_kb_bd, LV_OPA_70, 0);
  lv_obj_set_style_border_width(s_kb_bd, 0, 0);
  lv_obj_set_style_radius(s_kb_bd, 0, 0);
  lv_obj_set_style_pad_all(s_kb_bd, 0, 0);
  lv_obj_clear_flag(s_kb_bd, LV_OBJ_FLAG_SCROLLABLE);

  s_kb_ta = lv_textarea_create(s_kb_bd);
  lv_obj_set_pos(s_kb_ta, 4, 4);
  lv_obj_set_size(s_kb_ta, UI_LCD_HOR_RES - 8, 34);
  lv_textarea_set_one_line(s_kb_ta, true);
  lv_textarea_set_password_mode(s_kb_ta, true);       // hien ****
  lv_textarea_set_max_length(s_kb_ta, WIFI_PASS_MAX - 1);
  if (s_wifi_pass[0]) lv_textarea_set_text(s_kb_ta, s_wifi_pass);
  lv_obj_add_event_cb(s_kb_ta, cb_wifi_kb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_kb_ta, cb_wifi_kb, LV_EVENT_CANCEL, NULL);

  s_kb_kb = lv_keyboard_create(s_kb_bd);
  lv_obj_set_pos(s_kb_kb, 4, 42);
  lv_obj_set_size(s_kb_kb, UI_LCD_HOR_RES - 8, UI_LCD_VER_RES - 46);
  lv_keyboard_set_textarea(s_kb_kb, s_kb_ta);
  lv_obj_add_event_cb(s_kb_kb, cb_wifi_kb, LV_EVENT_READY, NULL);
  lv_obj_add_event_cb(s_kb_kb, cb_wifi_kb, LV_EVENT_CANCEL, NULL);
  if (s_group) { lv_group_add_obj(s_group, s_kb_kb); lv_group_focus_obj(s_kb_kb); }
}
#else
// Khong co ban phim trong cau hinh LVGL cua env nay -> nut MAT KHAU khong lam gi (van bam duoc,
// khong crash); mat khau nhap qua console COM7 / dung mang da luu trong NVS.
static void cb_wifi_pass(lv_event_t *e) { (void)e; }
#endif

// Do danh sach mang vao roller. Dinh dang 1 dong: "-58dBm <ssid>" (RSSI TRUOC de lay SSID = phan
// sau dau cach DAU TIEN - SSID co the chua dau cach nen khong the cat tu cuoi).
static void wifi_fill_roller(const int n) {
  if (!wf_roller) return;             // trang WIFI da bi thu hoi (xem page_wifi_destroy)
  char buf[WIFI_SCAN_SHOW * 44];      // tren STACK (~352 B) - chi dung 1 lan ngay sau khi quet
  char *p = buf;
  size_t left = sizeof(buf);
  buf[0] = '\0';
  int cnt = (n > WIFI_SCAN_SHOW) ? WIFI_SCAN_SHOW : n;
  if (cnt < 0) cnt = 0;
  for (int i = 0; i < cnt; i++) {
    const char *ssid = net_wifi_scan_ssid ? net_wifi_scan_ssid(i) : NULL;
    if (!ssid) ssid = "?";
    const int rssi = net_wifi_scan_rssi ? (int)net_wifi_scan_rssi(i) : 0;
    const int w = snprintf(p, left, "%s%ddBm %s", (i ? "\n" : ""), rssi, ssid);
    if (w < 0 || (size_t)w >= left) break;
    p += w; left -= (size_t)w;
  }
  if (!cnt) snprintf(buf, sizeof(buf), "(khong tim thay mang)");
  lv_roller_set_options(wf_roller, buf, LV_ROLLER_MODE_NORMAL);
  lv_roller_set_selected(wf_roller, 0, LV_ANIM_OFF);
}

// QUET MANG: uu tien duong KHONG CHAN (net_wifi_scan_begin/poll). Duong cu (dong bo) chi con la
// duong lui khi firmware khong co API async.
//
// ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¯ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¸ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚ÂÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â¦ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â ÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¯ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¸ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â CANH BAO (2026-10-14): quet dong bo CHAN vong lap 2-8 s (DO THAT: 8,2 s voi 30 mang) ->
// trong luc do MAN HINH CYD KHONG tra loi sector the SD cho Marlin (Marlin bao "SD read error" va DUNG BAN
// IN) va khong phuc vu web server. Vi vay: (1) TU CHOI quet khi Marlin dang in; (2) UI dung duong
// ASYNC de vong lap van chay.
// Ngoai ra: roi trang WIFI KHONG dung tÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â»ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Âºi WiFi (xem wifi_page_leave) - chi QUET/NGAT/XOA moi doi WiFi.
extern "C" bool cyd_sd_printer_busy(void) __attribute__((weak));
extern "C" int  net_wifi_scan_begin(void) __attribute__((weak));
extern "C" int  net_wifi_scan_poll(void) __attribute__((weak));
extern "C" void net_wifi_scan_cancel(void) __attribute__((weak));
extern "C" bool net_wifi_scanning(void) __attribute__((weak));
static bool net_wifi_scanning_weak(void) { return net_wifi_scanning && net_wifi_scanning(); }
// Tam dung mDNS trong luc quet (xem net_upload_http.h) - giam nguy co panic khi interface "down"
extern "C" bool net_upload_mdns_pause(bool pause) __attribute__((weak));


static void cb_wifi_scan(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || !wifi_api_ok() || !wf_roller) return;   // !wf_roller: trang da bi thu hoi
  if (cyd_sd_printer_busy && cyd_sd_printer_busy()) {
    // Quet lam WiFi roi kenh (va truoc day con CHAN vong lap) -> Marlin mat the giua ban in.
    lv_roller_set_options(wf_roller, "(Marlin dang in - khong quet mang)", LV_ROLLER_MODE_NORMAL);
    WIFILOG("[WIFI] TU CHOI quet: Marlin dang in\n");
    return;
  }
  if (net_wifi_scan_begin && net_wifi_scan_poll) {
    // DUONG MOI: quet KHONG CHAN - vong lap van chay (tra loi Marlin + web server + ve man hinh).
    // Tat mDNS trong luc quet (interface se "down" khi radio roi kenh -> mDNS de panic).
    if (net_upload_mdns_pause) net_upload_mdns_pause(true);
    if (net_wifi_scan_begin() != 0) {
      if (net_upload_mdns_pause) net_upload_mdns_pause(false);
      return;                                        // dang quet roi -> bo qua
    }
    s_wifi_busy = true;
    wifi_ov_show(true, "DANG QUET MANG...");
    return;                                          // ket qua lay o page_wifi_update()
  }
  // Duong lui (firmware khong co API async): quet dong bo 2-8 s + khoa input
  s_wifi_busy = true;
  wifi_ov_show(true, "DANG QUET MANG...");
  if (s_indev) lv_indev_enable(s_indev, false);        // KHOA cam ung + encoder
  for (uint8_t k = 0; k < 3; k++) lv_timer_handler();  // ve lop phu TRUOC khi goi ham blocking
  const uint32_t t0 = millis();
  const int n = net_wifi_scan();                       // BLOCKING 2-8 s (duong lui)
  WIFILOG("[WIFI] QUET dong bo xong sau %lu ms\n", (unsigned long)(millis() - t0));
  wifi_fill_roller(n);
  wifi_ov_show(false, NULL);
  if (s_indev) lv_indev_enable(s_indev, true);         // MO KHOA
  s_wifi_busy = false;
  refocus_page();
}

// KET NOI: lay SSID dang chon trong roller + mat khau da nhap; luu NVS (save = true).
// KHONG cho o day: net_wifi_connect() tra ve ngay, trang thai se tu doi qua page_wifi_update().
static void cb_wifi_connect(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || !net_wifi_connect || !wf_roller) return;
  char line[48] = "";
  lv_roller_get_selected_str(wf_roller, line, sizeof(line));
  const char *sp = strchr(line, ' ');
  const char *ssid = (sp && sp[1]) ? (sp + 1) : "";
  if (!ssid[0] || line[0] == '(') return;              // chua chon mang nao
  net_wifi_connect(ssid, s_wifi_pass, true);
}

static void cb_wifi_disconnect(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || !net_wifi_disconnect) return;
  net_wifi_disconnect(false);                          // ngat nhung GIU cau hinh
}

// XOA CAU HINH: khong hoan tac duoc -> xac nhan 2 buoc (giong M502/REBOOT DFU/XOA FILE)
static void cb_wifi_forget(lv_event_t *e) {
  (void)e;
  if (s_wifi_busy || !net_wifi_disconnect) return;
  ui_confirm("XOA WIFI", "Xoa cau hinh WiFi da luu (NVS)?\nLan sau phai nhap lai mat khau!",
             (uint8_t)A_WIFI_FORGET, 2);
}

// Dong moi thu CUA TRANG WIFI khi roi trang: ban phim nhap mat khau + lop phu "DANG QUET...".
// (Ca 2 vat deu la con cua lv_scr_act() chu KHONG phai con cua trang, nen neu khong don thi chung
//  van nam tren man hinh o trang khac.) KHONG dung tÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â»ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Âºi WiFi: roi trang khong ngat/ket noi lai gi ca.
static void wifi_page_leave(void) {
#if LV_USE_KEYBOARD && LV_USE_TEXTAREA
  if (s_kb_bd) {
    // Ngoai event cua chinh no -> dung lv_obj_del (khong can _async). LVGL tu go vat khoi group.
    lv_obj_del(s_kb_bd);
    s_kb_bd = NULL; s_kb_ta = NULL; s_kb_kb = NULL;
    WIFILOG("[WIFI] roi trang: da dong ban phim mat khau\n");
  }
#endif
  if (s_wf_ov) lv_obj_add_flag(s_wf_ov, LV_OBJ_FLAG_HIDDEN);
  // Dang quet KHONG CHAN ma roi trang -> huy phien quet (khong de no chay ngam)
  if (s_wifi_busy && net_wifi_scan_cancel && net_wifi_scanning_weak()) {
    net_wifi_scan_cancel();
    WIFILOG("[WIFI] roi trang: da huy phien quet\n");
  }
  if (s_wifi_busy && net_upload_mdns_pause) net_upload_mdns_pause(false);   // bat lai mDNS
  s_wifi_busy = false;
  if (s_indev) lv_indev_enable(s_indev, true);   // mo khoa cho chac (truong hop duong lui)
}

static void page_wifi_create(lv_obj_t *p) {
  wf_state = mk_label_w(p, "WiFi: -", COL_TXT, PAGE_INN_W);
  wf_ssid  = mk_label_w(p, "SSID: -", COL_DIM, PAGE_INN_W);
  wf_ip    = mk_label_w(p, "IP: -",   COL_DIM, PAGE_INN_W);

  wf_pass = mk_btn(p, "MAT KHAU...", PAGE_INN_W, 26, cb_wifi_pass, NULL);

  wf_roller = lv_roller_create(p);
  lv_obj_set_width(wf_roller, PAGE_INN_W);
  lv_roller_set_visible_row_count(wf_roller, 4);
  lv_obj_set_height(wf_roller, WIFI_ROLLER_H);
  lv_roller_set_options(wf_roller, "(bam QUET de tim mang)", LV_ROLLER_MODE_NORMAL);
  lv_obj_set_style_bg_color(wf_roller, COL_BTN, 0);
  lv_obj_set_style_bg_opa(wf_roller, LV_OPA_COVER, 0);
  lv_obj_set_style_text_color(wf_roller, COL_TXT, 0);
  lv_obj_set_style_border_width(wf_roller, 0, 0);
  lv_obj_set_style_text_color(wf_roller, lv_color_hex(0x102030), LV_PART_SELECTED);
  lv_obj_set_style_bg_color(wf_roller, COL_ACCENT, LV_PART_SELECTED);
  lv_obj_set_style_bg_opa(wf_roller, LV_OPA_COVER, LV_PART_SELECTED);
  lv_obj_add_flag(wf_roller, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  if (s_group) lv_group_add_obj(s_group, wf_roller);

  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 26, 0, GAP);
  mk_btn_grow(r, "QUET",    26, cb_wifi_scan, NULL);
  mk_btn_grow(r, "KET NOI", 26, cb_wifi_connect, NULL);
  mk_btn_grow(r, "NGAT",    26, cb_wifi_disconnect, NULL);
  mk_btn_grow(r, "XOA",     26, cb_wifi_forget, NULL);
}

static void page_wifi_update(void) {
  if (!wf_state || !wf_roller || !wf_pass) return;   // trang da bi thu hoi (xem page_wifi_destroy)

  // Dang quet mang (duong KHONG CHAN): hoi ket qua moi nhip 400 ms. Xong -> do vao roller + mo khoa.
  if (s_wifi_busy && net_wifi_scan_poll && net_wifi_scan_begin) {
    const int r = net_wifi_scan_poll();
    if (r == -1) return;                       // con dang quet -> giu nguyen lop phu
    if (r >= 0) wifi_fill_roller(r);
    else        lv_roller_set_options(wf_roller, "(quet that bai)", LV_ROLLER_MODE_NORMAL);
    if (net_upload_mdns_pause) net_upload_mdns_pause(false);   // bat lai mDNS sau khi quet
    wifi_ov_show(false, NULL);
    s_wifi_busy = false;
    refocus_page();
  }

  char b[64];
  if (!wifi_api_ok()) {                                // nhanh firmware chua merge API WiFi
    set_text(wf_state, "WiFi: KHONG HO TRO (chua co module)");
    set_text(wf_ssid, "");
    set_text(wf_ip, "");
    set_text(lv_obj_get_child(wf_pass, 0), "MAT KHAU: -");
    return;
  }
  static const char *const WST[3] = { "TAT", "DANG KET NOI", "DA KET NOI" };
  const uint8_t st = wifi_state_now();
  const int8_t  rs = wifi_rssi_now();
  if (st == 2 && rs) snprintf(b, sizeof(b), "WiFi: %s  %d dBm", WST[2], (int)rs);
  else               snprintf(b, sizeof(b), "WiFi: %s", WST[st <= 2 ? st : 0]);
  set_text(wf_state, b);

  const char *ssid = wifi_ssid_now();
  const char *ip   = wifi_ip_now();
  snprintf(b, sizeof(b), "SSID: %s", ssid[0] ? ssid : "-");
  set_text(wf_ssid, b);
  snprintf(b, sizeof(b), "IP: %s", ip[0] ? ip : "-");
  set_text(wf_ip, b);

  // Nut mat khau: CHI hien do dai (khong lo mat khau ra man hinh). set_text tu so sanh -> cache.
  lv_obj_t *pl = lv_obj_get_child(wf_pass, 0);
  if (pl) {
#if LV_USE_KEYBOARD && LV_USE_TEXTAREA
    if (s_wifi_pass[0]) snprintf(b, sizeof(b), "MAT KHAU: **** (%u)", (unsigned)strlen(s_wifi_pass));
    else                snprintf(b, sizeof(b), "MAT KHAU: (bam de nhap)");
#else
    // Env khong bat ban phim (xem #if o cb_wifi_pass): noi ro duong nhap mat khau khac.
    snprintf(b, sizeof(b), "MAT KHAU: nhap qua COM7 (khong co ban phim)");
#endif
    set_text(pl, b);
  }
}

// Huy man WIFI (xem quy uoc o ui_lvgl_int.h): dat NULL 3 nhan + nut mat khau + roller, va DON ban
// phim nhap mat khau + lop phu "DANG QUET..." (ca 2 la con cua lv_scr_act, KHONG phai con cua trang)
// + huy phien quet khong chan dang chay. show_page() da goi wifi_page_leave() truoc khi thu hoi
// trang; goi lai o day la vo hai (idempotent) va giu cho ham huy tu no an toan.
// s_wifi_pass: GIU NGUYEN (mat khau nguoi dung da nhap - khong phai trang thai widget).
void page_wifi_destroy(void) {
  wifi_page_leave();
  wf_state  = NULL;
  wf_ssid   = NULL;
  wf_ip     = NULL;
  wf_pass   = NULL;
  wf_roller = NULL;
}

// ===========================================================================
//  TRAMMING -- dieu khien Bed Tramming Wizard cua Marlin qua link ASCII (tbt_link.h)
//
//  KHONG dung ExtUI/CYDLCD_SERIAL_UI: phia Marlin chi can 1 dong #define CYDLCD_TBT_LINK
//  (xem Marlin/src/feature/cydlcd_tbt_link.h). Nho vay menu 12864 + MarlinUI van con nguyen.
//
//  Bo cuc (tong 168 <= PAGE_INN_H 206):
//    dong 1 : MODE + phase + goc dang dung .......... LINK / NO LINK
//    2 dong : 2 goc mot dong, mm so voi moc, dau '*' = goc nozzle dang dung
//    dong 4 : huong dan van vit (Marlin tinh san: "CW 0.150" / "CCW nhe" / "DAT")
//    dong 5 : 4 nut PROBE | MAX/MIN | BACK | DONE
//    2 dong : chu thich
// ===========================================================================
static lv_obj_t *tr_head, *tr_corner[TBT_PROTO_POINTS], *tr_hint;

// Ten 4 goc - PHAI theo dung thu tu TRAMMING_POINT_XY trong Marlin/Configuration_adv.h:
//   0 = Back-Right (280,285)  1 = Back-Left (25,285)  2 = Front-Left (25,25)  3 = Front-Right (280,25)
static const char *const TBT_NAME[TBT_PROTO_POINTS] = { "BR", "BL", "FL", "FR" };

// Bam 1 trong 4 nut -> gui dung 1 ky tu lenh xuong Marlin (xem tbt_protocol.h)
static void cb_tbt_cmd(lv_event_t *e) {
  const char c = (char)(intptr_t)lv_event_get_user_data(e);
  tbt_link_send(c);
  // BACK = thoat man nay (giong nut BACK cua wizard tren man 12864): bao Marlin thoat wizard
  // (stow probe + danh dau Z chua home) roi quay ve MENU.
  if (c == TBT_CMD_BACK) show_page(PG_MENU);
}

// " BR  -0.012" / "*FL    ---" : 1 goc, mm so voi moc (Marlin gui san gia tri da tinh)
static void tbt_fmt_corner(char *b, size_t n, const uint8_t i) {
  char num[12];
  if (tbt.st.ok[i]) {
    const float d = tbt.st.d[i];
    fmt_scaled(num, sizeof(num), (int32_t)(d * 1000.0f + (d < 0.0f ? -0.5f : 0.5f)), 1000, 3);
  }
  else snprintf(num, sizeof(num), "  ---");
  snprintf(b, n, "%c%s %s", (tbt.st.active && tbt.st.idx == i) ? '*' : ' ', TBT_NAME[i], num);
}

static void page_tbt_create(lv_obj_t *p) {
  // KHONG con nhan "LINK OK / NO LINK" o day nua: chi bao link nam o THANH TREN (xem link_ok()).
  lv_obj_t *rh = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 20, 0, GAP);
  tr_head = mk_label_w(rh, "-", COL_TXT, PAGE_INN_W);
  lv_obj_set_flex_grow(tr_head, 1);

  for (uint8_t r = 0; r < 2; r++) {
    lv_obj_t *row = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 24, 0, GAP);
    for (uint8_t k = 0; k < 2; k++) {
      const uint8_t i = (uint8_t)(r * 2 + k);
      tr_corner[i] = mk_label_w(row, "-", COL_TXT, 100);
      lv_obj_set_flex_grow(tr_corner[i], 1);
    }
  }

  tr_hint = mk_label_w(p, "NO LINK", COL_ACCENT, PAGE_INN_W);

  // R = home + probe lai ca 4 goc. Marlin tu bat dau wizard neu chua chay (tbt_run::begin)
  // nen nut nay la diem KHOI DONG cua man Tramming tren MAN HINH CYD.
  lv_obj_t *r0 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 38, 0, GAP);
  mk_btn_grow(r0, "HOME + PROBE 4 GOC", 38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_RESTART);

  lv_obj_t *rb = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 38, 0, GAP);
  mk_btn_grow(rb, "PROBE",   38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_PROBE);
  mk_btn_grow(rb, "MAX/MIN", 38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_MINMAX);
  mk_btn_grow(rb, "BACK",    38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_BACK);
  mk_btn_grow(rb, "DONE",    38, cb_tbt_cmd, (void *)(intptr_t)TBT_CMD_DONE);

  mk_label_w(p, "DONE = goc xong | MAX/MIN = doi moc", COL_DIM, PAGE_INN_W);
}

static void page_tbt_update(void) {
  if (!tr_head) return;                  // trang da bi thu hoi (an toan: xem page_tbt_destroy)
  char b[44];

  // ---- dong 1: MODE + phase + goc dang dung | LINK ----
  if (!tbt.st.active)
    snprintf(b, sizeof(b), "%s  READY (bam HOME + PROBE)", tbt.st.mode_max ? "MAX" : "MIN");
  else
    snprintf(b, sizeof(b), "%s  %s  goc %u", tbt.st.mode_max ? "MAX" : "MIN",
             tbt_phase_name(tbt.st.phase), (unsigned)(tbt.st.idx + 1));
  set_text(tr_head, b);

  // 4 goc: gia tri mm so voi moc (Marlin da tinh)
  for (uint8_t i = 0; i < TBT_PROTO_POINTS; i++) {
    tbt_fmt_corner(b, sizeof(b), i);
    set_text(tr_corner[i], b);
    lv_color_t col = COL_TXT;
    if (tbt.st.active && tbt.st.idx == i) col = COL_ACCENT;      // goc nozzle dang dung
    else if (tbt.st.accepted[i])           col = COL_DIM;         // da bam DONE
    else if (tbt.st.ok[i])                 col = COL_OK;          // da do duoc
    lv_obj_set_style_text_color(tr_corner[i], col, 0);
  }

  // ---- dong huong dan: Marlin gui token "CW:0.150" / "CCW:nhe" / "DAT" / "PROBE" / "ERR" / "-"
  lv_color_t hcol = COL_ACCENT;
  if (!tbt.linked) {
    snprintf(b, sizeof(b), "chua co du lieu (CYDLCD_TBT_LINK?)");
    hcol = COL_ERR;
  }
  else if (tbt.st.phase != TBT_PH_IDLE) {
    snprintf(b, sizeof(b), "%s...", tbt_phase_name(tbt.st.phase));
  }
  else if (tbt.st.all_dat) {
    snprintf(b, sizeof(b), "DAT - lech so voi tam (chi con BACK)");
    hcol = COL_OK;
  }
  else {
    // "CW:0.150" -> "CW 0.150"; "CCW:nhe" -> "CCW nhe"
    const char *h = tbt.st.hint[0] ? tbt.st.hint : "-";
    uint8_t k = 0;
    for (; h[k] && k < sizeof(b) - 1; k++) b[k] = (h[k] == ':') ? ' ' : h[k];
    b[k] = '\0';
    if (h[0] == 'D') hcol = COL_OK;                                // DAT
    else if (h[0] == 'E') hcol = COL_ERR;                          // ERR
  }
  set_text(tr_hint, b);
  lv_obj_set_style_text_color(tr_hint, hcol, 0);
}

// Huy man TRAMMING (xem quy uoc o ui_lvgl_int.h): dat NULL nhan dau + 4 nhan goc + dong huong dan.
// Container trang do show_page() xoa bang lv_obj_del() nen khong xoa widget o day. Du lieu cua
// wizard (tbt.st/tbt.linked...) KHONG phai cua trang: giu nguyen, Marlin van gui tiep dong "TBT ...".
void page_tbt_destroy(void) {
  tr_head = NULL;
  tr_hint = NULL;
  for (uint8_t i = 0; i < TBT_PROTO_POINTS; i++) tr_corner[i] = NULL;
}

// ===========================================================================
//  Tao man / chuyen man
// ===========================================================================
static void create_page(uint8_t p) {
  // STATUS dung kin 320x240 theo bo cuc Marlin (khong co dong tieu de);
  // cac man khac (ke ca MENU) deu nam duoi dong tieu de chung -> nut BACK luon o goc phai.
  s_page[p] = (p == PG_STATUS) ? mk_page_full() : mk_page(lv_scr_act());
  switch (p) {
    case PG_STATUS:   page_status_create(s_page[p]);   break;
    case PG_MENU:     page_menu_create(s_page[p]);     break;
    case PG_RATE:     page_rate_create(s_page[p]);     break;
    case PG_TEMP:     page_temp_create(s_page[p]);     break;
    case PG_JOG:      page_jog_create(s_page[p]);      break;
    case PG_FILES:    page_files_create(s_page[p]);    break;
    case PG_SETTINGS: page_settings_create(s_page[p]); break;
    case PG_MESH:     page_mesh_create(s_page[p]);     break;
    case PG_CUSTOM:   page_custom_create(s_page[p]);   break;
    case PG_TBT:     page_tbt_create(s_page[p]);     break;
    case PG_INFO:     page_info_create(s_page[p]);     break;
    case PG_ENDSTOP:  page_endstop_create(s_page[p]);  break;
    case PG_WIFI:     page_wifi_create(s_page[p]);     break;
    default: break;
  }
  lv_obj_add_flag(s_page[p], LV_OBJ_FLAG_HIDDEN);      // se duoc hien boi show_page()
}

// ---------------------------------------------------------------------------
//  THU HOI TRANG (page recycling): tra bo nho cua trang CU ve pool LVGL.
//
//  VI SAO: moi trang chi duoc tao khi mo lan dau (create_page) va TRUOC DAY khong bao gio duoc
//  giai phong -> mo dan 13 trang la het pool 48 KB, LVGL assert (LV_USE_ASSERT_NULL/MALLOC +
//  handler while(1) trong cydlcd_assert.cpp) -> panel TREO IM LANG. Nay trang CU bi thu hoi truoc
//  khi hien trang moi -> chi con TRANG STATUS + TRANG DANG MO.
// ---------------------------------------------------------------------------

// `o` co nam trong cay con cua `root` khong (ke ca chinh no). Dung de biet con tro focus cua encoder
// co dang nam trong trang sap bi xoa khong.
static bool obj_in_tree(lv_obj_t *o, const lv_obj_t *root) {
  while (o) {
    if (o == root) return true;
    o = lv_obj_get_parent(o);
  }
  return false;
}

// Thu hoi 1 trang: goi page_<ten>_destroy() (dat NULL con tro widget) roi xoa container.
//
//  - GIU LAI TRANG STATUS: khong bao gio thu hoi (xem chu thich o show_page).
//  - KHONG tu xoa tung widget con: lv_obj_del(container) xoa ca cay con, tu go chung khoi `s_group`
//    (lv_obj_destructor -> lv_group_remove_obj) va tu refocus vat khac.
//  - Don con tro focus TRUOC khi xoa: `on_focus_changed()` (ui_widgets.cpp) nho vat focus truoc do o
//    `s_focus_prev`; khi xoa vat dang focus thi LVGL tu refocus va goi lai focus_cb, tuc la
//    focus_mark() se ghi style vao vat DANG BI HUY (spec_attr chua duoc giai phong nen khong crash,
//    nhung la ghi vo nghia). Doi focus sang nhan TEN MAN (con cua dong tieu de, luon song) truoc ->
//    moi thu duoc go don sach se.
static void page_release(uint8_t p) {
  if (p >= PG_COUNT) return;
  if (p == PG_STATUS) return;              // trang goc: GIU LAI (xem show_page)
  lv_obj_t *pg = s_page[p];
  if (!pg) return;                         // chua tung duoc tao -> khong co gi de thu hoi

  if (s_group && s_title) {
    lv_obj_t *f = lv_group_get_focused(s_group);
    if (f && obj_in_tree(f, pg)) lv_group_focus_obj(s_title);
  }

  switch (p) {
    case PG_MENU:     page_menu_destroy();     break;
    case PG_RATE:     page_rate_destroy();     break;
    case PG_TEMP:     page_temp_destroy();     break;
    case PG_JOG:      page_jog_destroy();      break;
    case PG_FILES:    page_files_destroy();    break;
    case PG_SETTINGS: page_settings_destroy(); break;
    case PG_MESH:     page_mesh_destroy();     break;
    case PG_CUSTOM:   page_custom_destroy();   break;
    case PG_TBT:      page_tbt_destroy();      break;
    case PG_INFO:     page_info_destroy();     break;
    case PG_ENDSTOP:  page_endstop_destroy();  break;
    case PG_WIFI:     page_wifi_destroy();     break;
    case PG_STATUS:   break;                 // khong bao gio toi day (da return o tren)
    default: break;
  }

  lv_obj_del(pg);                            // xoa container + toan bo cay con (tra pool LVGL)
  s_page[p] = NULL;
}

void show_page(uint8_t p) {
  if (p >= PG_COUNT) p = PG_STATUS;

  const uint8_t prev = s_page_act;           // trang CU - phai doc TRUOC khi bi ghi de o cuoi ham

  // ---- 1) Dong cac lop phu KHONG thuoc trang (con cua lv_scr_act) ----
  // Lop phu SUA thong so (ov_*) chi thuoc man SETTINGS -> doi man thi dong luon. PHAI lam TRUOC khi
  // thu hoi trang: lop phu nam tren lv_scr_act nen neu con mo ma trang SETTINGS bi xoa thi no van
  // nam de len trang moi (xem page_settings_destroy).
  if (p != PG_SETTINGS) ov_hide();

  // Roi trang WIFI -> don ban phim nhap mat khau + lop phu quet (con cua lv_scr_act, khong phai
  // con cua trang) va KHONG dung toi WiFi. Vao trang WIFI -> chi ghi log (khong tu quet mang).
  if (p != PG_WIFI) wifi_page_leave();
  else if (prev != PG_WIFI) WIFILOG("[WIFI] vao trang WIFI (chi doc trang thai, khong quet)\n");

  // ---- 2) THU HOI TRANG CU (page recycling - xem page_release) ----
  // Chi thu hoi khi THUC SU doi trang; trang dang mo (p) KHONG bi thu hoi. Lam TRUOC create_page()
  // de dinh bo nho chi la (trang cu + trang moi) chu khong cong don qua nhieu lan mo.
  // GIU LAI TRANG STATUS: day la trang goc, la noi MOI nut BACK tro ve, va 3 vat trang thai tren no
  // (icon WiFi + chu dBm + the SD) duoc cap nhat dinh ky tu ui_lvgl_task() cho MOI trang -> thu hoi
  // no lam tang nguy co nhap nhay ma loi ich khong dang.
  if (prev != p) page_release(prev);

  // ---- 3) Tao (neu chua co) + hien trang moi ----
  if (!s_page[p]) create_page(p);

  for (uint8_t i = 0; i < PG_COUNT; i++) {
    if (!s_page[i]) continue;
    if (i == p) lv_obj_clear_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_add_flag(s_page[i], LV_OBJ_FLAG_HIDDEN);
  }

  // Dong tieu de (co nut BACK o goc phai) hien o MOI man tru STATUS (man goc, khong co BACK)
  const bool chrome = (p != PG_STATUS);
  if (s_bar) {
    if (chrome) lv_obj_clear_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_add_flag(s_bar, LV_OBJ_FLAG_HIDDEN);
  }

  // (Lop phu SUA thong so va viec don trang WIFI da duoc lam o BUOC 1 - TRUOC khi thu hoi trang cu.)

  // MO man FILE -> tu nha + init lai the SD (M22/M21) ROI MOI xin danh sach (xem files_init_sd
  // va FILES_JOB_DELAY_MS). VI SAO dat o day: show_page() chi chay 1 lan moi lan VAO man, nen
  // KHONG the gui lap moi 400 ms (se nghen UART va giat man hinh). Truong hop dung: module WiFi
  // (module ESP3D - da xoa 2026-10-14) ghi thang len the SD qua SPI dung chung -> Marlin phai duoc init lai the
  // moi doc/in duoc, truoc day nguoi dung phai tu bam "Init SD" truoc khi in.
  if (p == PG_FILES) files_init_sd();

  set_text(s_title, PG_TITLE[p]);
  s_page_act = p;

  // encoder: focus vao vat dau tien cua man vua mo
  if (s_group && !focus_first_in(s_page[p]) && s_bar) lv_group_focus_obj(s_bar);

  // Thanh day: do noi dung SAU khi trang da duoc HIEN (xem canh bao o bot_sync)
  bot_sync(true);
}

// ===========================================================================
//  API
// ===========================================================================
// So DONG cua vung dem ve LVGL (1 buffer, flush dong bo trong ui_port_flush_cb).
// 40 -> 20 dong: nhuong ~12,8 KB DRAM TINH cho heap, vi Bluedroid Classic (BT SPP/SDP) can
// ~100 KB heap va truoc day chi con ~22 KB -> SPP server khong len duoc (Windows: 10108
// "No such service is known"). Ve theo dai 20 dong thay vi 40: them vai lan flush SPI,
// cham hon khong dang ke (khong dung DMA, van 1 buffer nhu cu).
#define LV_DRAW_BUF_LINES 20

void ui_lvgl_begin(void) {
  lv_init();

#if UI_MONO_FONT == 4
  // Font mono tu sinh: tao descriptor + tu kiem tra, in ket qua ra Serial (COM7, 115200) MOT LAN.
  // Nap xong chi can doc log la biet driver tra dung du lieu hay khong:
  //   [GFXFONT] line_height=17 base_line=3 ... | bpp=4 (16 muc xam, bitmap da kieu LVGL ...)
  //   [GFXFONT] 'A' -> OK box=8x9 adv_w=8 ofs=(0,0) bpp=4 bytes=36 | checksum8=... first8=...
  // Neu dong nao ghi "KHONG TRA DUOC GLYPH" hoac "BITMAP NULL" thi driver co van de.
  s_gfx_mono = lv_font_gfxfont_get_ex(UI_FONT_RANGES, UI_GFX_RANGE_COUNT, UI_GFX_LAYOUT);
  if (!s_gfx_mono) {
    Serial.printf("[UI] UI_MONO_FONT 4: lv_font_gfxfont_get_ex() tra NULL -> lui ve Montserrat "
                  "(mat dau tieng Viet, KHONG mat het chu)\n");
  }
  lv_font_gfxfont_selftest(s_gfx_mono);
#endif

  ui_port_display_init();

  // Vung dem ve cua LVGL: UI_LCD_HOR_RES x LV_DRAW_BUF_LINES pixel (1 buffer, flush dong bo).
  // GIAM tu 40 -> 20 dong de NHUONG DRAM TINH cho Bluedroid Classic (BT SPP/SDP can ~100 KB heap;
  // truoc day chi con ~22 KB heap nen SPP server khong len duoc - Windows bao 10108 "No such
  // service is known"). 320 * 20 * 2 B = 12 800 B thay vi 25 600 B -> tra ~12,8 KB cho heap.
  // Ve theo dai 20 dong thay vi 40: chi them vai lan flush SPI, cham hon khong dang ke.
  static lv_color_t s_buf[UI_LCD_HOR_RES * LV_DRAW_BUF_LINES];
  static lv_disp_draw_buf_t s_draw_buf;
  lv_disp_draw_buf_init(&s_draw_buf, s_buf, NULL, UI_LCD_HOR_RES * LV_DRAW_BUF_LINES);

  static lv_disp_drv_t s_disp_drv;
  lv_disp_drv_init(&s_disp_drv);
  s_disp_drv.hor_res  = UI_LCD_HOR_RES;
  s_disp_drv.ver_res  = UI_LCD_VER_RES;
  s_disp_drv.flush_cb = ui_port_flush_cb;
  s_disp_drv.draw_buf = &s_draw_buf;
  lv_disp_drv_register(&s_disp_drv);

  // group chung cho encoder (an/hien man -> LVGL tu bo qua vat bi HIDDEN)
  s_group = lv_group_create();
  lv_group_set_focus_cb(s_group, on_focus_changed);
  lv_group_set_wrap(s_group, true);

  ui_port_input_init();

  static lv_indev_drv_t s_indev_drv;
  lv_indev_drv_init(&s_indev_drv);
  s_indev_drv.type    = ui_port_input_is_encoder() ? LV_INDEV_TYPE_ENCODER : LV_INDEV_TYPE_POINTER;
  s_indev_drv.read_cb = ui_port_read_cb;
  lv_indev_t *indev = lv_indev_drv_register(&s_indev_drv);
  if (indev && ui_port_input_is_encoder()) lv_indev_set_group(indev, s_group);
  s_indev = indev;                        // + de KHOA/MO input khi quet WiFi (xem cb_wifi_scan)

  build_chrome();
  // Kho preset preheat cua MAN HINH CYD (NVS) - doc 1 lan luc boot roi bom vao bang thong so de trang
  // SETTINGS hien/sua duoc. Phai sau cydlcd_link_begin() (xem main.cpp) vi cydlcd.settings[] moi co.
  preheat_load();
  preheat_inject();
  show_page(PG_STATUS);

  s_tick_ms = s_upd_ms = s_slow_ms = ui_port_millis();
  lv_timer_handler();                     // ve man hinh dau tien
}

void ui_lvgl_task(void) {
  // Tick cho LVGL: LVGL 8.3 chi chay lv_timer khi tick da tang, nen KHONG the dung
  // mot lv_timer goi lv_tick_inc() (timer do se khong bao gio chay -> treo).
  // Lay truc tiep tu millis() la cach dung.
  const uint32_t now = ui_port_millis();
  if (now != s_tick_ms) {
    lv_tick_inc(now - s_tick_ms);
    s_tick_ms = now;
  }

  lv_timer_handler();

  // Hieu chuan cam ung: chay NGOAI event callback cua LVGL (blocking, tu ve bang TFT_eSPI),
  // xong xoa lai toan man hinh de LVGL ve lai tu dau.
  if (s_cal_req) {
    s_cal_req = false;
    ui_port_touch_calibrate();
    lv_obj_invalidate(lv_scr_act());
    s_tick_ms = s_upd_ms = s_slow_ms = ui_port_millis();
    return;
  }

  if (now - s_upd_ms < UI_REFRESH_MS) return;      // UI_REFRESH_MS trong cydlcd_config.h
  s_upd_ms = now;

  // Thanh tren = NOI DUY NHAT hien chi bao link cho moi man (tru trang STATUS co them dong
  // trang thai + khung XYZ cung bao mat link). 3 muc: TEN MAY / LINK ERR / NO LINK
  // (xem link_state_now() - "co byte toi" khac voi "doc duoc dong TRAM").
  // 2026-10-13 (yeu cau): khi link TOT thi hien TEN MAY (tu HELLO cua Marlin, khong co thi
  // CYDLCD_MACHINE_NAME) thay cho chu "LINK OK"; chi khi co loi moi hien LINK ERR / NO LINK.
  const uint8_t lst = link_state_now();
  const char *const nm = cydlcd.name[0] ? cydlcd.name : CYDLCD_MACHINE_NAME;
  set_text(s_link, lst == 1 ? nm : (lst == 2 ? "LINK ERR" : "NO LINK"));
  if (lst != s_link_state) {
    s_link_state = lst;
    lv_obj_set_style_text_color(s_link, lst == 1 ? COL_OK : (lst == 2 ? COL_ACCENT : COL_ERR), 0);
  }

  // P9: 2 nut PAUSE/STOP nho o dong tieu de, CHI hien khi dang in tu the SD
  if (s_btn_pause && s_btn_stop) {
    const bool printing = ((cydlcd.flags & CYDLCD_SF_SD_PRINTING) != 0);
    if (printing) {
      lv_obj_clear_flag(s_btn_pause, LV_OBJ_FLAG_HIDDEN);
      lv_obj_clear_flag(s_btn_stop,  LV_OBJ_FLAG_HIDDEN);
    }
    else {
      lv_obj_add_flag(s_btn_pause, LV_OBJ_FLAG_HIDDEN);
      lv_obj_add_flag(s_btn_stop,  LV_OBJ_FLAG_HIDDEN);
    }
  }

  switch (s_page_act) {
    case PG_STATUS: page_status_update(); break;
    case PG_MENU:   page_menu_update();   break;
    case PG_RATE:   page_rate_update();   break;
    case PG_TEMP:   page_temp_update();   break;
    case PG_JOG:    page_jog_update();    break;
    case PG_TBT:    page_tbt_update();    break;
    case PG_INFO:    page_info_update();    break;
    case PG_ENDSTOP: page_endstop_update(); break;
    default: break;
  }

  // man nang (bang/danh sach): cap nhat cham hon cho do ton SPI
  if (now - s_slow_ms >= 400U) {
    s_slow_ms = now;

    // Icon WiFi (header + man STATUS) + chu dBm canh no va mau icon THE (man STATUS): nhip CHAM
    // (~400 ms), chi ghi style/chu khi trang thai (hoac so dBm) DOI. Nam ngoai switch(s_page_act)
    // vi icon thuoc dong tieu de / man STATUS.
    wifi_icon_mark();
    wifi_rssi_mark();
    status_sd_icon_mark();
    // Tien do nhan file qua BT (hien o s_link): cung nhip cham, chi doi chuoi khi gia tri doi.

    switch (s_page_act) {
      case PG_FILES:    page_files_update();    break;
      case PG_SETTINGS: page_settings_update(); break;
      case PG_MESH:     page_mesh_update();     break;
      case PG_CUSTOM:   page_custom_update();   break;
      case PG_WIFI:     page_wifi_update();     break;
      default: break;
    }
    // Noi dung man co the doi do (vd SETTINGS loc lai / MENU doi gia tri) -> cap nhat nhan thanh day.
    // Chi chay o nhip CHAM (400 ms) va KHONG do lai chieu cao trang (xem bot_sync): do lai se lam
    // LVGL kep `scroll_y` -> vua bam NEXT da bi nhay ve dau. Muon do lai thi phai roi man roi vao lai.
    bot_sync(false);

    // Viec cham cua man FILE (init the SD / xoa file): chay CA KHI da roi man FILE, de lenh da
    // gui van duoc hoan tat (xin lai danh sach + mo khoa nut XOA). Xem FILES_JOB_DELAY_MS.
    files_job_task();
  }

  boxes_update();
}

// ===========================================================================
//  API CHAN DOAN (khai bao trong ui_lvgl.h) - cho console COM7 o main.cpp
//
//  VI SAO CO: khi mot trang UI lam panel treo thi khong the ngoi cham tay vao man hinh de tai hien
//  (va cung khong biet trang nao). Hai ham duoi day cho phep mo trang bang 1 dong lenh tren COM7
//  (`UI TRAMMING`, `UI SETTINGS`...) va in danh sach trang (`UI ?`) -> tai hien loi tu PC.
// ===========================================================================
uint8_t ui_lvgl_page_count(void) { return (uint8_t)PG_COUNT; }

void ui_lvgl_show_page_name(const char *name) {
  if (!name || !name[0] || name[0] == '?') {          // khong co ten -> in danh sach
    Serial.print("[UI] cac trang:");
    for (uint8_t i = 0; i < PG_COUNT; i++) Serial.printf(" %u=%s", (unsigned)i, PG_TITLE[i]);
    Serial.println();
    return;
  }
  for (uint8_t i = 0; i < PG_COUNT; i++) {
    if (strcasecmp(name, PG_TITLE[i]) == 0) {
      Serial.printf("[UI] mo trang %u: %s\n", (unsigned)i, PG_TITLE[i]);
      show_page(i);
      return;
    }
  }
  Serial.printf("[UI] khong co trang '%s' (go 'UI ?' de xem danh sach)\n", name);
}

#endif  // CYDLCD_USE_LVGL
