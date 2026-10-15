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
 *   FILES   : FILES_ROWS dong file cua cydlcd.files[] (cham 1 dong = chon + to sang; lat trang
 *             bang PREV/NEXT), PRINT (M23 + M24), DIR, UP, REFRESH, PREV/NEXT, XOA FILE (M30),
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
 * (gfx4_font_text qua driver lv_font_gfxfont) CO DU dau tieng Viet, nho vay lop phu SUA
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
#include "net_upload_http.h"        // net_notify_set/clear/dump (thong bao di TRANG /status, khong hien LCD)
#include "net_upload.h"             // net_wifi_state(): trang thai WiFi de ve icon o header
#include "ui_lang.h"                // T() + ui_lang_* (da ngon ngu, xem src/ui_lang.h)
#include "ui_overlay.h"             // lop phu tien do upload (task `ui` la noi DUY NHAT ve)

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
//  Cac khai bao `weak` nam o ui_lvgl_int.h (khoi "WiFi (weak) - hop dong voi net_wifi.cpp") vi
//  man INFO / man WIFI / man STATUS (chu dBm) cung dung chung.
// ---------------------------------------------------------------------------

// Bao an toan cho UI: ham vang (weak NULL) -> tra gia tri "rong" thay vi goi con tro NULL.
// KHONG `static`: man INFO (ui_page_info.cpp) va man WIFI (ui_page_wifi.cpp) cung dung -> khai bao
// o ui_lvgl_int.h - xem khoi "WiFi (weak) + accessor an toan".
const char *wifi_ssid_now(void) {
  const char *s = net_wifi_ssid ? net_wifi_ssid() : NULL;
  return s ? s : "";
}
const char *wifi_ip_now(void) {
  const char *s = net_wifi_ip ? net_wifi_ip() : NULL;
  return s ? s : "";
}
int8_t wifi_rssi_now(void) { return net_wifi_rssi ? net_wifi_rssi() : 0; }
// Cho file khac (ui_chrome.cpp: chu dBm canh icon WiFi tren man STATUS) doc duoc RSSI: ham
// wifi_rssi_now() la `static` trong file nay nen phai co cong ra ngoai (khai bao o ui_lvgl_int.h).
int8_t ui_wifi_rssi(void) { return wifi_rssi_now(); }
// Co API WiFi khong (de trang WIFI bao "khong ho tro" thay vi hien "TAT" gay hieu nham).
bool wifi_api_ok(void) { return net_wifi_scan != NULL; }

#if UI_MONO_FONT == 4
#if UI_MONO_GFX_BPP == 4
static const GFXfont *const UI_FONT_RANGES[UI_GFX_RANGE_COUNT] =
  UI_GFX_RANGES(gfx4_font_text);
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

// TEN TRANG cho CONSOLE (`UI MENU`, `UI ENDSTOP`, `UI ?`...) - KHONG DICH, giu y nguyen nhu truoc de
// cac lenh chan doan khong phu thuoc ngon ngu dang chon (2026-10-15, da ngon ngu).
static const char *const PG_KEY[PG_COUNT] = {"STATUS", "MENU", "FEED/FLOW", "NHIET DO", "JOG",
                                             "FILE SD", "THONG SO", "BED MESH", "CUSTOM",
                                             "TRAMMING", "INFO", "ENDSTOP", "WIFI",
                                             "CAU HINH"};
// TEN TRANG HIEN TREN DONG TIEU DE (dich duoc - xem ui_lang_vi.h/ui_lang_en.h, ID UI_T_PT_*).
static const uint8_t PG_TITLE_ID[PG_COUNT] = {
  UI_T_PT_STATUS, UI_T_PT_MENU, UI_T_PT_RATE, UI_T_PT_TEMP, UI_T_PT_JOG,
  UI_T_PT_FILES, UI_T_PT_SETTINGS, UI_T_PT_MESH, UI_T_PT_CUSTOM,
  UI_T_PT_TBT, UI_T_PT_INFO, UI_T_PT_ENDSTOP, UI_T_PT_WIFI,
  UI_T_PT_SUBMENU
};
// Ten cua trang MENU CON khong lay tu bang tren: CA cay menu Configuration dung CHUNG 1 trang
// (PG_SUBMENU) nen tieu de la ten NHOM dang mo -> xem page_title() duoi day.
const char *page_title(uint8_t p) {
  if (p == PG_SUBMENU) return submenu_title();
  return (p < PG_COUNT) ? T((ui_text_t)PG_TITLE_ID[p]) : T(UI_T_PT_STATUS);
}

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
// Input device (encoder hoac cam ung) - gan trong ui_lvgl_begin(). Man WIFI dung no de KHOA/MO input
// trong luc quet mang (xem ui_page_wifi.cpp: cb_wifi_scan / wifi_page_leave).
lv_indev_t *s_indev;

// ---------------------------------------------------------------------------
//  Bien chi dung trong file nay -> VAN giu `static`
// ---------------------------------------------------------------------------
static uint8_t    s_link_state = 255;      // 255 = chua set lan nao (`link_state_now()` tra 0/1/2)
static uint32_t   s_tick_ms, s_upd_ms, s_slow_ms;
// Vi tri cuon (scroll_top, px) da luu cua TUNG trang: roi trang thi nho, vao lai thi tra lai dung cho
// dang xem. 0 = dau danh sach. (Trang MENU CON khong dung mang nay - xem submenu_save_scroll.)
// uint16_t (khong phai lv_coord_t): noi dung 1 trang chi vai tram px -> du, va DRAM cua panel da full
// (lan dau dung int32 lam linker bao overflow 48 byte).
static uint16_t   s_scroll_y[PG_COUNT];
// ---- NGAN XEP "QUAY LAI" (BACK) ----
// VI SAO: BACK phai tra ve DUNG NOI VUA ROI, khong phai luon ve MENU. Truoc day chi co 1 truong hop dac
// biet (trang THONG SO mo tu menu CONFIGURATION). Nay moi lan DOI TRANG thi day "noi vua roi" vao ngan
// xep: (trang, NHOM menu con neu dang o menu con, DONG vua bam trong nhom do). BACK lay ra va tra ve
// dung cho - kem TO SANG dong vua bam (yeu cau nguoi dung 2026-10-15: tu MOTION > "Move Axis (JOG)",
// BACK phai ve MOTION va to sang dong "Move Axis (JOG)").
#define BACK_STACK_N 6
typedef struct { uint8_t page, sm_g, sm_hi; } back_ent_t;
static back_ent_t s_back[BACK_STACK_N];
static uint8_t    s_back_n = 0;
static bool       s_back_nav = false;      // dang THUC HIEN lenh BACK -> khong day lai ngan xep

static void create_page(uint8_t p);          // forward
static void page_after_show(uint8_t p);      // forward (dinh nghia ngay truoc show_page)
// do_action() la API chung (khai bao o ui_lvgl_int.h) vi man FILE (ui_page_files.cpp) cung goi no
// (A_REQ_FILES / A_INITSD). files_delete_now() / ov_open_id() cung o file rieng - xem ui_lvgl_int.h.


// DUNG IN (M524): GUI LAI nhieu lan cho toi khi Marlin thoi trang thai in.
// VI SAO: khi hang doi lenh cua Marlin DAY (vd ban in dang `M190` cho nong ban), moi dong tu
// UART deu bi Marlin tra "busy: processing" va BO DI -> gui M524 DUNG 1 LAN la khong du (gap that
// 2026-10-15: bam STOP khong dung duoc ban in). M524 la lenh idempotent (huy in tren the) nen
// gui lai la vo hai; dung lai ngay khi trang thai khong con la "dang in / tam dung".
static uint8_t s_stop_try = 0;               // so lan con gui lai (0 = khong cho gi)
#define STOP_RETRY_TIMES 15                  // 15 x 400 ms = ~6 s

static void stop_request(void) { s_stop_try = STOP_RETRY_TIMES; }

static void stop_retry_task(void) {
  if (!s_stop_try) return;
  // CO IN TU THE SD moi gui M108+M524 (dung CO, khong dung `cydlcd.state`): HOME/PROBE (vd G35) khong
  // phai ban in nen khong duoc gui lenh huy. Xem printer_busy() (cyd_sd.cpp).
  const bool printing = (cydlcd.flags & (CYDLCD_SF_SD_PRINTING | CYDLCD_SF_PAUSED)) != 0;
  if (!printing) { s_stop_try = 0; return; }  // Marlin da dung -> xong
  // M108 TRUOC: thoat vong CHO NONG (M109/M190). Neu ban in dang cho ban nong ma khong nong duoc
  // (vd PSU tat) thi M524 mot minh KHONG du. Phia Marlin (cydlcd_serial.cpp) xu ly NGAY 2 lenh nay
  // khong qua hang doi, nen chung khong bi Marlin bo khi hang doi day.
  cydlcd_link_send_gcode("M108");
  cydlcd_link_send_gcode("M524");
  s_stop_try--;
}

void cb_action(lv_event_t *e) {
  do_action((int)(intptr_t)lv_event_get_user_data(e));
}

void do_action(const int a) {
  switch (a) {
    // BACK (giong "up" trong menu Marlin):
    //   1) dang mo lop phu SUA so  -> chi DONG hop, khong doi trang;
    //   2) dang o menu CONFIGURATION -> len NHOM CHA, het cap thi ve MENU;
    //   3) trang bang phang (THONG SO) mo TU menu CONFIGURATION -> tra ve dung menu do;
    //   4) con lai: man con -> MENU, MENU -> STATUS.
    case A_BACK:
      // Lop phu cua trang MENU CON phai duoc DONG TRUOC khi BACK (giong lop phu SUA cua man THONG SO):
      // ban phim dat ten -> hop sua PREHEAT 3-trong-1 -> lop phu SUA so. Neu khong dong truoc thi BACK
      // vua doi nhom menu vua de hop mo tren man hinh moi (loi that 2026-10-15).
      if (preheat_kb_active())   { preheat_kb_hide();   break; }
      if (preheat_edit_active()) { preheat_edit_hide(); break; }
      if (ov_close_open()) break;
      // 2) Trong menu CON: len NHOM CHA neu con cap (truoc khi dung ngan xep).
      if (s_page_act == PG_SUBMENU && submenu_parent() != SM_PARENT_NONE) {
        submenu_open(submenu_parent());
        break;
      }
      // 3) NGAN XEP: tra ve DUNG NOI VUA ROI (trang + NHOM menu con + DONG vua bam -> duoc to sang).
      if (s_back_n) {
        const back_ent_t e = s_back[--s_back_n];
        s_back_nav = true;                     // khong day lai ngan xep trong luc tra ve
        if (e.page == PG_SUBMENU) submenu_open_restore(e.sm_g, e.sm_hi);
        else                      show_page(e.page);
        s_back_nav = false;
        break;
      }
      // 4) Ngan xep rong: hanh vi cu (tu MENU -> STATUS, tu trang khac -> MENU).
      show_page(s_page_act == PG_MENU ? PG_STATUS : PG_MENU);
      break;
    case A_GOTO_MENU:     show_page(PG_MENU); break;
    case A_GOTO_TEMP:     show_page(PG_TEMP); break;
    case A_GOTO_JOG:      show_page(PG_JOG); break;
    case A_GOTO_FILES:    show_page(PG_FILES); break;
    case A_GOTO_SETTINGS: show_page(PG_SETTINGS); break;
    case A_GOTO_RATE:     show_page(PG_RATE); break;
    case A_GOTO_STATUS:   show_page(PG_STATUS); break;   // nut HOME: ve thang man STATUS
    case A_GOTO_WIFI:     show_page(PG_WIFI); break;     // icon WiFi / muc MENU: mo trang WIFI
    // (2026-10-15: A_GOTO_MPC / PG_MPC da BO - muc "Tuning (MPC/PID)" o MENU trung voi
    //  Configuration > Advanced Settings > Temperature, xem MENU_ITEMS o ui_page_menu.cpp.)
    case A_GOTO_ENDSTOP:  show_page(PG_ENDSTOP); break;
    // ---- Menu MOTION giong so do 12864 (xem docs/12864-menu-map.md, nut m20) ----
    case A_GOTO_MESH:     show_page(PG_MESH);  break;     // "Unified Bed Leveling > Bed Mesh"
    case A_GOTO_TBT:      show_page(PG_TBT);   break;     // "Tramming Wizard"
    case A_PROBE_DEPLOY:  cydlcd_link_send_gcode("M401"); break;   // Deploy Z-Probe (probe co dinh -> Marlin bao loi)
    case A_PROBE_STOW:    cydlcd_link_send_gcode("M402"); break;   // Stow Z-Probe
    case A_G29_LEVEL:     cydlcd_link_send_gcode("G29 N");  break; // Level Bed
    case A_G29_INFO:      cydlcd_link_send_gcode("G29 W");  break; // Output UBL Info
    case A_G29_UBL_MANUAL: cydlcd_link_send_gcode2("G29 I999", "G29 P2BT0"); break;  // Manually Build Mesh
    case A_UBL_T0:        cydlcd_link_send_gcode("G29 T0"); break; // Output for Host
    case A_UBL_T1:        cydlcd_link_send_gcode("G29 T1"); break; // Output for CSV
    case A_UBL_S1:        cydlcd_link_send_gcode("G29 S-1"); break;// Off Printer Backup
    // M48 Probe Test: 12864 gui "G28O\nM48 P10" (O = khong home neu da home) -> gui 2 lenh lien tiep.
    case A_M48:           cydlcd_link_send_gcode2("G28 O", "M48 P10"); break;
    // Muc "Motion" trong MENU: mo NHOM MOTION cua cay menu con (ngang cap CONFIGURATION, giong 12864).
    case A_GOTO_MOTION:   submenu_open_motion(); break;
    case A_HOME:          cydlcd_link_send_gcode("G28"); break;
    case A_HOME_X:        cydlcd_link_send_gcode("G28 X"); break;
    case A_HOME_Y:        cydlcd_link_send_gcode("G28 Y"); break;
    case A_HOME_Z:        cydlcd_link_send_gcode("G28 Z"); break;
    case A_STEPPERS_OFF:  cydlcd_link_send_gcode("M84"); break;
    case A_PAUSE:         cydlcd_link_send_gcode("M25"); break;
    case A_RESUME:        cydlcd_link_send_gcode("M24"); break;
    case A_STOP:          ui_confirm("DUNG IN", "Dung han ban in? (M524)", (uint8_t)A_STOP_NOW, 1); break;
    case A_STOP_NOW:      stop_request(); break;                   // gui lai M524 toi khi dung (xem stop_retry_task)
    case A_COOLDOWN:      cydlcd_link_send_gcode("M104 S0"); cydlcd_link_send_gcode("M140 S0"); break;
    case A_SAVE:          cydlcd_link_request((uint8_t)CYDLCD_REQ_SAVE_SETTINGS); break;
    // M501: nap lai thong so tu EEPROM vao RAM (muc "Load Settings" cua menu Configuration).
    // KHONG xac nhan 2 buoc: lenh nay chi DOC EEPROM, hoan tac duoc (sua lai roi SAVE) - khac M502.
    // Sau khi nap thi gia tri trong RAM doi -> xin Marlin gui lai bang thong so cho man hinh.
    case A_LOAD_SETTINGS: cydlcd_link_send_gcode("M501");
                          cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS); break;
    // Mo BAN PHIM dat ten vat lieu moi (muc "+ Them vat lieu" trong menu CONFIGURATION > Preheat).
    case A_PRE_ADD:       preheat_add_open(); break;
    case A_INITSD:        cydlcd_link_send_gcode2("M22", "M21"); break;   // nha the SD roi init lai
    // 2026-10-15: G34 DA TAT tren may nay (thiet ke truc Z cung - xem CYDLCD_NO_G34 trong
    // Marlin/Configuration.h). Khong con nut nao gui lenh nay, nhung giu nhanh nay de KHONG BAO GIO gui
    // G34 nua ke ca khi co code cu goi A_G34.
    case A_G34:           WIFILOG("[UI] G34 da TAT tren may nay (truc Z cung) - khong gui lenh\n"); break;
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
    // 2026-10-15: MPC autotune dau in - dung cho dong "MPC Autotune (M306 T)" trong
    // Configuration > Advanced Settings > Temperature (trang TUNING cu da bo).
    case A_MPC_TUNE: cydlcd_link_send_gcode("M306 T");       break;
    case A_PID_BED:  cydlcd_link_send_gcode("M303 E-1 S60"); break;   // PID ban in, target 60 do C

    // ---- thanh day: lui/toi 1 trang noi dung (xem bot_sync de biet khi nao co thanh nay) ----
    case A_PAGE_PREV: page_scroll(-1); break;
    case A_PAGE_NEXT: page_scroll(+1); break;

    // ---- NGON NGU (2026-10-15): 2 dong trong Configuration > Advanced Settings > Language ----
    // `ui_lang_set()` ghi NVS luon; `ui_lang_apply_ui()` dung lai chrome + trang dang mo (chu da duoc
    // LVGL copy vao nhan luc tao). Doi ngon ngu KHONG dong cham gi den Marlin.
    case A_LANG_VI: if (ui_lang_set(UI_LANG_VI)) ui_lang_apply_ui(); break;
    case A_LANG_EN: if (ui_lang_set(UI_LANG_EN)) ui_lang_apply_ui(); break;
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
    case A_FILES_DELETE:      files_delete_now();                             // xoa file SD dang chon (M30) - man FILE
      break;
    case A_PRE_DELETE:        preheat_delete_pending();   // xoa vat lieu preheat dang chon (nut "Xoa" - ui_page_preheat.cpp)
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
    case PG_SUBMENU:  page_submenu_create(s_page[p]);  break;
    default: break;
  }
  // Phong xa: neu ham create cua trang lam mat container (s_page[p] = NULL - vd page_rebuild() bi goi
  // sai thoi diem, xem ui_page_submenu.cpp) thi KHONG goi lv_obj_add_flag(NULL) -> panic.
  if (s_page[p]) lv_obj_add_flag(s_page[p], LV_OBJ_FLAG_HIDDEN);   // se duoc hien boi show_page()
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
    case PG_SUBMENU:  page_submenu_destroy();  break;
    case PG_STATUS:   break;                 // khong bao gio toi day (da return o tren)
    default: break;
  }

  lv_obj_del(pg);                            // xoa container + toan bo cay con (tra pool LVGL)
  s_page[p] = NULL;
}

// Dung lai noi dung cua 1 trang DANG MO: thu hoi (page_release) roi tao lai, sau do cap nhat tieu de /
// focus / thanh day y nhu show_page().
//
// VI SAO CAN: noi dung cua trang MENU CON (PG_SUBMENU) phu thuoc BIEN TOAN CUC `s_sm_g` (nhom dang mo)
// chu khong chi du lieu Marlin -> doi nhom ma van o trong trang thi phai dung lai noi dung. show_page()
// khong lam duoc viec nay vi no thay `p == s_page_act` nen khong thu hoi/tao lai (xem show_page).
// Dung cho CA truong hop: so thong so Marlin gui DOI -> co dong vua hien duoc -> loc lai danh sach.
void page_rebuild(uint8_t p) {
  if (p >= PG_COUNT || !s_page[p]) return;
  const bool active = (s_page_act == p);
  page_release(p);                           // -> page_<ten>_destroy() + lv_obj_del(container)
  if (!active) return;                       // goi cho trang KHONG mo: chi don cho sach
  create_page(p);
  lv_obj_clear_flag(s_page[p], LV_OBJ_FLAG_HIDDEN);
  set_text(s_title, page_title(p));
  if (s_group && s_page[p] && !focus_first_in(s_page[p]) && s_bar) lv_group_focus_obj(s_bar);
  bot_sync(true);                            // do lai chieu cao: thanh day PREV/NEXT co the doi
  page_after_show(p);                        // tra lai cho cuon CUA NHOM + focus dong vua dung
}

// Sau khi trang da HIEN va da tra lai vi tri cuon: focus + cuon toi DONG ma nguoi dung vua dung, de ho
// biet minh dang o dau trong danh sach (yeu cau nguoi dung 2026-10-15):
//   - MENU: dong da mo mot man khac (page_menu_focus_sel);
//   - MENU CON: dong NHOM vua quay lui (submenu_focus_parent) - va tra lai vi tri cuon CUA NHOM do
//     (moi nhom co danh sach dai ngan khac nhau nen phai luu theo nhom, khong theo trang).
static void page_after_show(uint8_t p) {
  if (p == PG_SUBMENU) {
    const lv_coord_t sy = submenu_scroll_y();
    if (sy > 0 && s_page[p]) {
      lv_obj_scroll_to_y(s_page[p], sy, LV_ANIM_OFF);
      bot_sync(false);
    }
    submenu_focus_parent();
  }
  else if (p == PG_MENU) {
    page_menu_focus_sel();
  }
}

void show_page(uint8_t p) {
  if (p >= PG_COUNT) p = PG_STATUS;

  const uint8_t prev = s_page_act;           // trang CU - phai doc TRUOC khi bi ghi de o cuoi ham

  // ---- 0) DAY "NOI VUA ROI" VAO NGAN XEP BACK (tru khi chinh lenh BACK dang goi show_page) ----
  // Ghi ca NHOM + DONG vua bam khi dang o menu CON, de BACK quay lai dung nhom va to sang dung dong.
  if (p != prev && !s_back_nav) {
    if (s_back_n >= BACK_STACK_N) {          // ngan xep day -> bo muc CU NHAT (giu chuoi gan nhat)
      memmove(&s_back[0], &s_back[1], sizeof(back_ent_t) * (BACK_STACK_N - 1));
      s_back_n = BACK_STACK_N - 1;
    }
    s_back[s_back_n].page  = prev;
    s_back[s_back_n].sm_g  = (prev == PG_SUBMENU) ? submenu_group() : 0;
    s_back[s_back_n].sm_hi = (prev == PG_SUBMENU) ? submenu_last_item() : 0;
    s_back_n++;
  }

  // ---- 1) Dong cac lop phu KHONG thuoc trang (con cua lv_scr_act) ----
  // Lop phu SUA thong so (ov_*) chi thuoc man SETTINGS va man MENU CON (PG_SUBMENU - xem
  // ui_page_submenu.cpp) -> doi sang trang KHAC thi dong luon. PHAI lam TRUOC khi thu hoi trang:
  // lop phu nam tren lv_scr_act nen neu con mo ma trang bi xoa thi no van nam de len trang moi
  // (xem page_settings_destroy).
  if (p != PG_SETTINGS && p != PG_SUBMENU) ov_hide();
  // HOP SUA PREHEAT 3-trong-1 mo tu khoi "Preheat" cua menu CONFIGURATION (PG_SUBMENU) -> roi menu do
  // thi dong luon; BAN PHIM dat ten vat lieu la lop phu TOAN MAN nen doi trang la phai don (giong cach
  // man WIFI don ban phim mat khau - xem wifi_page_leave).
  if (p != PG_SUBMENU) preheat_edit_hide();
  if (prev != p) preheat_kb_hide();

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
  //
  // NHO CHO DANG XEM cua trang cu (yeu cau nguoi dung 2026-10-15: "menu co phan trang, khi bam back
  // chua quay ve trang truoc do"): luu vi tri cuon TRUOC khi trang bi thu hoi; lan sau vao lai se tra
  // lai (xem cuoi show_page). Trang MENU CON luu theo NHOM (submenu_save_scroll).
  if (prev != p && prev != PG_STATUS && s_page[prev]) {
    if (prev == PG_SUBMENU) submenu_save_scroll();
    else                    s_scroll_y[prev] = lv_obj_get_scroll_top(s_page[prev]);
  }
  if (prev != p) page_release(prev);

  // ---- 3) Tao (neu chua co) + hien trang moi ----
  // MO trang MENU CON tu mot man KHAC -> luon vao tu NHOM GOC (giong Marlin: menu luon mo tu dau).
  // Khi dang O TRONG trang nay ma doi nhom (submenu_open -> page_rebuild) thi khong di qua day.
  if (p == PG_SUBMENU && prev != PG_SUBMENU) submenu_reset_root();
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

  set_text(s_title, page_title(p));
  s_page_act = p;

  // encoder: focus vao vat dau tien cua man vua mo
  // (kiem tra s_page[p] truoc: focus_first_in(NULL) se cham vao assert cua LVGL - xem create_page)
  if (s_group && s_page[p] && !focus_first_in(s_page[p]) && s_bar) lv_group_focus_obj(s_bar);

  // Thanh day: do noi dung SAU khi trang da duoc HIEN (xem canh bao o bot_sync)
  bot_sync(true);

  // TRA LAI CHO DANG XEM + FOCUS DONG VUA DUNG (yeu cau nguoi dung 2026-10-15: "menu co phan trang,
  // khi bam back chua quay ve trang truoc do, va chua highlight item da trigger open sub-menu").
  // PHAI lam SAU bot_sync(true) vi bot_sync(true) dua trang ve dau (scroll = 0) de DO chieu cao noi dung.
  // Trang MENU CON (PG_SUBMENU) khong dung mang theo trang: moi NHOM co vi tri cuon rieng.
  if (p != PG_SUBMENU && s_scroll_y[p] > 0 && s_page[p]) {
    lv_obj_scroll_to_y(s_page[p], s_scroll_y[p], LV_ANIM_OFF);
    bot_sync(false);          // cap nhat lai nhan "TRANG x/y" theo vi tri vua tra lai
  }
  page_after_show(p);
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

  ui_lang_load();                         // ngon ngu da luu (NVS "ui"/"lang") - PHAI truoc build_chrome
  build_chrome();
  // Kho preset preheat cua MAN HINH CYD (NVS "ph2"; ban cu "ph" duoc di tru mot lan) - doc 1 lan luc boot.
  // Tu 2026-10-15 danh sach nay KHONG bom vao bang thong so nua (khong con 9 dong preheat o bang
  // SETTINGS): no hien o CONFIGURATION > Preheat va sua bang hop 3-trong-1 (xem ui_page_preheat.cpp).
  preheat_load();
  show_page(PG_STATUS);

  s_tick_ms = s_upd_ms = s_slow_ms = ui_port_millis();
  lv_timer_handler();                     // ve man hinh dau tien
}

// ---- do hieu nang (`UI PERF`, xem ui_lvgl_test_perf): thoi gian lv_timer_handler() moi vong lap ----
static uint32_t s_pf_loop_n = 0, s_pf_hdl_us = 0, s_pf_hdl_max_us = 0;
static uint32_t s_pf_task_n = 0, s_pf_task_us = 0, s_pf_task_max_us = 0;

static void ui_lvgl_task_body(void);    // than cua ui_lvgl_task() (tach ra chi de do thoi gian ca ham)

// Do truc tiep ngay trong ham goi: dem so lan ui_lvgl_task() chay QUA LAU (>= 20 ms) va thoi gian lan cuoi.
// VI SAO: dong log 1 Hz o main.cpp do bang micros() va bao `ui=67 000 us` gan NHU MOI LAN, trong khi tong
// thoi gian/thu tu goi lai chi ~2 ms/lan => phai co gi do khong khop. Hai con so nay la bang chung doc lap.
static uint32_t s_pf_slow_n = 0;        // so lan >= 20 ms (tu lan doc truoc)
static uint32_t s_pf_slow_all = 0;      // tong so lan >= 20 ms
static uint32_t s_pf_last_us = 0;       // thoi gian lan goi CUOI

uint32_t ui_lvgl_perf_slow_take(void) { const uint32_t v = s_pf_slow_n; s_pf_slow_n = 0; return v; }
uint32_t ui_lvgl_perf_slow_all(void)  { return s_pf_slow_all; }
uint32_t ui_lvgl_perf_last_us(void)   { return s_pf_last_us; }

// ---- do do tre duong serial (xem ui_lvgl.h) ----
static uint32_t s_pf_link_gap_max = 0, s_pf_link_gap_slow = 0, s_pf_link_dur_max = 0, s_pf_link_n = 0;

void ui_lvgl_perf_link_call(const uint32_t gap_us, const uint32_t dur_us) {
  s_pf_link_n++;
  if (gap_us > s_pf_link_gap_max) s_pf_link_gap_max = gap_us;
  if (gap_us > 50000u) s_pf_link_gap_slow++;         // > 50 ms = coi nhu "bi gian doan" (xem tieu chi nghiem thu)
  if (dur_us > s_pf_link_dur_max) s_pf_link_dur_max = dur_us;
}

uint32_t ui_lvgl_perf_link_take(uint32_t *slow_n) {
  const uint32_t g = s_pf_link_gap_max;
  if (slow_n) *slow_n = s_pf_link_gap_slow;
  s_pf_link_gap_max = s_pf_link_gap_slow = s_pf_link_dur_max = 0;
  return g;
}

// ui_lvgl_task() = do thoi gian CA HAM (khong chi lv_timer_handler) - vong lap ben ngoai chi do bang
// millis() (do phan giai 1 ms) nen de sai; o day do bang micros() cho chac. Xem ui_lvgl_test_perf().
void ui_lvgl_task(void) {
  const uint32_t t_task = micros();
  ui_lvgl_task_body();
  const uint32_t dt_task = micros() - t_task;
  s_pf_task_n++;
  s_pf_task_us += dt_task;
  if (dt_task > s_pf_task_max_us) s_pf_task_max_us = dt_task;
  s_pf_last_us = dt_task;
  if (dt_task >= 20000u) { s_pf_slow_n++; s_pf_slow_all++; }
}

static void ui_lvgl_task_body(void) {
  // Tick cho LVGL: LVGL 8.3 chi chay lv_timer khi tick da tang, nen KHONG the dung
  // mot lv_timer goi lv_tick_inc() (timer do se khong bao gio chay -> treo).
  // Lay truc tiep tu millis() la cach dung.
  const uint32_t now = ui_port_millis();
  if (now != s_tick_ms) {
    lv_tick_inc(now - s_tick_ms);
    s_tick_ms = now;
  }

  // Do hieu nang (xem ui_lvgl_test_perf / `UI PERF`): lv_timer_handler() bao gom CA flush SPI (flush_cb
  // chay dong bo trong do) nen hieu "tong thoi gian ve" va "thoi gian SPI" la tach duoc phan LVGL vs SPI.
  const uint32_t t_hdl = micros();
  lv_timer_handler();
  const uint32_t dt_hdl = micros() - t_hdl;
  s_pf_loop_n++;
  s_pf_hdl_us += dt_hdl;
  if (dt_hdl > s_pf_hdl_max_us) s_pf_hdl_max_us = dt_hdl;

  // Hieu chuan cam ung: chay NGOAI event callback cua LVGL (blocking, tu ve bang TFT_eSPI),
  // xong xoa lai toan man hinh de LVGL ve lai tu dau.
  if (s_cal_req) {
    s_cal_req = false;
    ui_port_touch_calibrate();
    lv_obj_invalidate(lv_scr_act());
    s_tick_ms = s_upd_ms = s_slow_ms = ui_port_millis();
    return;
  }

  // 2026-10-15 (YEU CAU NGUOI DUNG voi man ENDSTOP: "doc tin hieu M115 30 lan 1 giay"):
  // Man ENDSTOP can doc nhanh hon nhieu so voi nhip chung `UI_REFRESH_MS` (150 ms), nen co DUONG RIENG:
  // goi `page_endstop_poll()` moi nhip task `ui` (30 Hz = 33 ms) - dat TRUOC cong UI_REFRESH_MS de luot
  // 150 ms khong lam mat nhip. PHAI giu KHOA TRANG THAI khi doc (nhu luot cap nhat ben duoi).
  // ⚠️ CHI goi khi `s_page_act == PG_ENDSTOP`: RA KHOI MAN la ngung monitor ngay (khong xin frame M119 nua).
  if (s_page_act == PG_ENDSTOP) {
    cydlcd_link_state_lock();
    page_endstop_poll();
    cydlcd_link_state_unlock();
  }

  if (now - s_upd_ms < UI_REFRESH_MS) return;      // UI_REFRESH_MS trong cydlcd_config.h
  s_upd_ms = now;

  // DA LUONG: giu KHOA TRANG THAI trong suot lan cap nhat nay de doc duoc mot ban "nguyen ven" (task `link`
  // publish bang mutex voi timeout 0 -> gap nay no BO QUA lan publish, du lieu van nguyen o ban rieng cua no
  // nen KHONG mat, chi cham vai ms). Doi lai: duong serial KHONG BAO GIO bi man hinh chan.
  cydlcd_link_state_lock();

  // Thanh tren = NOI DUY NHAT hien chi bao link cho moi man (tru trang STATUS co them dong
  // trang thai + khung XYZ cung bao mat link). 3 muc: TEN MAY / LINK ERR / NO LINK
  // (xem link_state_now() - "co byte toi" khac voi "doc duoc dong TRAM").
  // 2026-10-13 (yeu cau): khi link TOT thi hien TEN MAY (tu HELLO cua Marlin, khong co thi
  // CYDLCD_MACHINE_NAME) thay cho chu "LINK OK"; chi khi co loi moi hien LINK ERR / NO LINK.
  const uint8_t lst = link_state_now();
  const char *const nm = cydlcd.name[0] ? cydlcd.name : CYDLCD_MACHINE_NAME;
  set_text(s_link, lst == 1 ? nm : (lst == 2 ? T(UI_T_LINK_ERR) : T(UI_T_LINK_NONE)));
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
    stop_retry_task();          // dang cho DUNG IN -> gui lai M524 (xem stop_request)
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
      // Menu CONFIGURATION (PG_SUBMENU): cot gia tri chi doi khi Marlin gui thong so -> nhip CHAM
      // 400 ms la du (giong trang THONG SO), khong ton SPI.
      case PG_SUBMENU:  page_submenu_update();  break;
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
  ui_overlay_update();               // lop phu tien do upload (task `ui` la noi DUY NHAT ve - xem ui_overlay.h)
  cydlcd_link_state_unlock();       // nha khoa trang thai (xem cydlcd_link_state_lock o tren)
}

// ===========================================================================
//  DA LUONG (B3): hang doi LENH CONSOLE cho task `ui` + task `ui` 30 Hz
// ===========================================================================
// VI SAO: moi lenh `UI ...` tren COM8 deu cham vao LVGL (mo trang, in bo cuc, mo hop, sua thong so...). Tu khi
// task `ui` la noi DUY NHAT goi LVGL, console (task `log`/vong lap core 0) KHONG duoc thuc thi truc tiep nua
// ma phai GUI vao hang doi nay; task `ui` lay ra va goi `ui_console_dispatch()` (dinh nghia o main.cpp - giu
// nguyen chuoi lenh cu, chi doi noi chay).
#if defined(CYDLCD_BOARD_CYD)
  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
  #define UI_CMD_Q    6
  #define UI_CMD_LEN  40
  static QueueHandle_t s_cmd_q = nullptr;
  static TaskHandle_t  s_ui_task_h = nullptr;
#endif

void ui_post_cmd(const char *arg) {
#if defined(CYDLCD_BOARD_CYD)
  if (!s_cmd_q) s_cmd_q = xQueueCreate(UI_CMD_Q, UI_CMD_LEN);
  if (!s_cmd_q) return;
  char b[UI_CMD_LEN];
  snprintf(b, sizeof(b), "%s", arg ? arg : "");
  if (xQueueSend(s_cmd_q, b, 0) != pdTRUE)
    Serial.printf("[UI] CANH BAO: hang doi lenh UI day -> bo \"%s\"\n", b);
#else
  ui_console_dispatch(arg);
#endif
}

// Lay toi da `max` lenh dang cho va thuc thi (CHI task `ui` goi).
static void ui_cmd_drain(uint8_t max) {
#if defined(CYDLCD_BOARD_CYD)
  if (!s_cmd_q) return;
  char b[UI_CMD_LEN];
  while (max--) {
    if (xQueueReceive(s_cmd_q, b, 0) != pdTRUE) break;
    ui_console_dispatch(b);
  }
#endif
}

#if defined(CYDLCD_BOARD_CYD)
// Task `ui`: 30 Hz (33 ms) - nhu da chot voi nguoi dung. `ui_lvgl_task()` tu tinh tick LVGL theo `millis()`
// nen nhip 33 ms van du (LVGL refresh 30 ms, indev 20 ms) va cham cam ung tre <= 33 ms.
static void ui_cmd_task(void *) {
  for (;;) {
    ui_cmd_drain(3);
    ui_lvgl_task();
    g_hb_ui++;                                  // heartbeat cho task chan doan (xem main.cpp: g_hb_*)
    // NHIP 30 Hz (33 ms) - CHOT 2026-10-15 sau khi thu 60 Hz: nguoi dung doi lai 30 Hz (60 Hz khong can thiet,
    // va 30 Hz do duoc la ~5% CPU core 0, con 60 Hz gap doi so lan goi lv_timer_handler() ma mat nguoi khong
    // thay khac biet). Nhip nay phai KEM hoac BANG LV_DISP_DEF_REFR_PERIOD (lv_conf.h: 30 ms).
    // `ulTaskNotifyTake` thay cho delay: task khac danh thuc NGAY duoc (ui_post_cmd -> xTaskNotifyGive;
    // sau nay la ngat TOUCH_IRQ -> ui_task_wake_isr) => lenh console tra loi ngay, khong doi het nhip.
    ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(33));
  }
}

// Ngat TOUCH_IRQ (GPIO36) goi ham nay -> danh thuc task `ui` ngay. PHAI co IRAM_ATTR: ngat co the chay khi
// cache flash dang tat (vd luc ghi NVS) - khong co IRAM_ATTR la crash.
#if defined(CYDLCD_BOARD_CYD)
IRAM_ATTR void ui_task_wake_isr(void) {
  BaseType_t hp = pdFALSE;
  if (s_ui_task_h) vTaskNotifyGiveFromISR(s_ui_task_h, &hp);
  portYIELD_FROM_ISR(hp);
}
#else
void ui_task_wake_isr(void) { }
#endif

bool ui_task_start(void) {
  if (!s_cmd_q) s_cmd_q = xQueueCreate(UI_CMD_Q, UI_CMD_LEN);
  return xTaskCreatePinnedToCore(ui_cmd_task, "ui", 8192, nullptr, 5, &s_ui_task_h, 0) == pdPASS;
}
#endif

// ===========================================================================
//  API CHAN DOAN (khai bao trong ui_lvgl.h) - cho console COM7 o main.cpp
// ===========================================================================

//  VI SAO CO: khi mot trang UI lam panel treo thi khong the ngoi cham tay vao man hinh de tai hien
//  (va cung khong biet trang nao). Hai ham duoi day cho phep mo trang bang 1 dong lenh tren COM7
//  (`UI TRAMMING`, `UI SETTINGS`...) va in danh sach trang (`UI ?`) -> tai hien loi tu PC.
// ===========================================================================
uint8_t ui_lvgl_page_count(void) { return (uint8_t)PG_COUNT; }

// ===========================================================================
//  DOI NGON NGU: dung lai phan HIEN THI (2026-10-15)
// ===========================================================================
// Chu da duoc LVGL COPY vao tung nhan luc TAO trang/vat, nen sau khi doi `s_lang` phai tao lai moi thu
// co chu: chrome (nut BACK/PREV/NEXT + ten man) va NOI DUNG trang dang mo.
//   - `page_rebuild()` thu hoi + tao lai TRANG DANG MO; voi menu CON (PG_SUBMENU) no giu nguyen NHOM
//     dang mo (`s_sm_g`) nen nguoi dung khong bi nhay ve nhom goc.
//   - Lop phu dang mo (hop thoai/ban phim) co chu rieng -> dong het cho gon; doi ngon ngu la hanh dong
//     hiem nen khong can dung lai chung.
// Ngay sau do luot cap nhat (<=150 ms) se tu dat lai chu dong (ten may/LINK ERR, "TRANG x/y").
void ui_lang_apply_ui(void) {
  chrome_lang_retext();
  ov_hide();
  preheat_edit_hide();
  preheat_kb_hide();
  page_rebuild(s_page_act);
}

// `UI LANG` = in ngon ngu dang dung + danh sach; `UI LANG <so>` = doi (luu NVS luon);
// `UI LANG?` = kiem tra THIEU BAN DICH (in tung ID thieu kem chuoi tieng Viet de tim nhanh trong bang).
void ui_lvgl_test_lang(const char *arg) {
  const char *a = arg;
  while (a && *a == ' ') a++;
  if (!a || !*a) {
    Serial.printf("[UI] LANG: dang dung %u = '%s' (%s) | CYDLCD_LANG_DEFAULT=%u\n",
                  (unsigned)ui_lang_get(), ui_lang_name(ui_lang_get()), ui_lang_code(ui_lang_get()),
                  (unsigned)CYDLCD_LANG_DEFAULT);
    for (uint8_t i = 0; i < UI_LANG_COUNT; i++)
      Serial.printf("[UI]   %u = %-2s '%s' (chuoi rieng %u, dung chung ban VI %u)\n",
                    (unsigned)i, ui_lang_code(i), ui_lang_name(i),
                    (unsigned)ui_lang_str_count(i), (unsigned)ui_lang_missing(i));
    Serial.println("[UI]   doi: `UI LANG <so>` | kiem tra thieu ban dich: `UI LANG?`");
    return;
  }
  if (*a == '?') {
    Serial.printf("[UI] LANG bang chuoi: UI_T_COUNT=%u (id 1..%u)\n",
                  (unsigned)UI_T_COUNT, (unsigned)(UI_T_COUNT - 1));
    for (uint8_t l = 0; l < UI_LANG_COUNT; l++) {
      const uint16_t miss = ui_lang_missing(l);
      // Chinh sach (xem ui_lang.h): bang VI phai DAY DU; bang EN duoc thieu (dung chung ban VI).
      if (l == UI_LANG_VI)
        Serial.printf("[UI]   %s: %s (%u/%u)\n", ui_lang_code(l),
                      miss ? "THIEU - BANG GOC PHAI DAY DU" : "day du",
                      (unsigned)miss, (unsigned)(UI_T_COUNT - 1));
      else
        Serial.printf("[UI]   %s: chuoi rieng %u/%u, dung chung ban VI %u (hop le)\n",
                      ui_lang_code(l), (unsigned)ui_lang_str_count(l), (unsigned)(UI_T_COUNT - 1),
                      (unsigned)miss);
    }
    return;
  }
  unsigned v = 255;
  if (sscanf(a, "%u", &v) != 1 || v >= UI_LANG_COUNT) {
    Serial.printf("[UI] LANG: so ngon ngu khong hop le ('%s') - chi co 0..%u\n",
                  a, (unsigned)(UI_LANG_COUNT - 1));
    return;
  }
  const bool changed = ui_lang_set((uint8_t)v);
  if (changed) ui_lang_apply_ui();
  Serial.printf("[UI] LANG -> %u '%s' (%s)%s\n", (unsigned)ui_lang_get(), ui_lang_name(ui_lang_get()),
                ui_lang_code(ui_lang_get()), changed ? " - da dung lai man hinh" : " (khong doi)");
}

void ui_lvgl_show_page_name(const char *name) {
  if (!name || !name[0] || name[0] == '?') {          // khong co ten -> in danh sach
    // In KHOA (PG_KEY, khong dich) kem ten dang hien trong ngoac khi 2 thu khac nhau: lenh `UI <ten>`
    // luon nhan KHOA (on dinh giua cac ngon ngu), con man hinh thi hien ban dich.
    Serial.print("[UI] cac trang:");
    for (uint8_t i = 0; i < PG_COUNT; i++) {
      const char *disp = T((ui_text_t)PG_TITLE_ID[i]);
      if (strcmp(disp, PG_KEY[i]) == 0) Serial.printf(" %u=%s", (unsigned)i, PG_KEY[i]);
      else                              Serial.printf(" %u=%s(%s)", (unsigned)i, PG_KEY[i], disp);
    }
    Serial.println();
    return;
  }
  for (uint8_t i = 0; i < PG_COUNT; i++) {
    if (strcasecmp(name, PG_KEY[i]) == 0) {
      Serial.printf("[UI] mo trang %u: %s\n", (unsigned)i, PG_KEY[i]);
      show_page(i);
      return;
    }
  }
  Serial.printf("[UI] khong co trang '%s' (go 'UI ?' de xem danh sach)\n", name);
}

// In 1 vat + cac con cua no (de quy toi DO sau 2 - du de thay nhan NAM TRONG khung/nut, vd
// nhan FR/FL trong o cua man FEED/FLOW hay 3 o nhiet cua STATUS).
static void dump_obj(lv_obj_t *o, const uint8_t depth) {
  if (!o) return;
  char ind[8];
  uint8_t k = 0;
  for (; k < depth && k < 6; k++) ind[k] = ' ';
  ind[k] = '\0';

  const bool is_label  = lv_obj_check_type(o, &lv_label_class);
  const bool is_roller = lv_obj_check_type(o, &lv_roller_class);
  const char *cls = is_label                                ? "label"
                  : lv_obj_check_type(o, &lv_btn_class)     ? "btn"
                  : is_roller                               ? "roller"
                  : lv_obj_check_type(o, &lv_table_class)   ? "table"
                  : lv_obj_check_type(o, &lv_bar_class)     ? "bar"
                                                            : "obj";
  const char *txt = is_label  ? lv_label_get_text(o)
                  : is_roller ? lv_roller_get_options(o)
                              : "";
  // Nhan co the chua KY TU ICON (vung Private Use U+E000..U+E0FF, UTF-8 3 byte EE 80 xx - vd chevron
  // cua dong menu con) -> doi thanh "<icon E008>" cho DOC DUOC o log (dump khong ve duoc glyph).
  char txtp[80];
  {
    size_t o2 = 0;
    for (size_t i = 0; txt[i] && o2 + 14 < sizeof(txtp); ) {
      const unsigned char c = (unsigned char)txt[i];
      if (c == 0xEE && (unsigned char)txt[i + 1] == 0x80) {
        o2 += (size_t)snprintf(txtp + o2, sizeof(txtp) - o2, "<icon E%02X>", (unsigned)(unsigned char)txt[i + 2]);
        i += 3;
      }
      else txtp[o2++] = txt[i++];
    }
    txtp[o2] = '\0';
  }
  Serial.printf("[UI] %s%-6s x=%d y=%d %dx%d%s%s%s\n", ind, cls,
                (int)lv_obj_get_x(o), (int)lv_obj_get_y(o),
                (int)lv_obj_get_width(o), (int)lv_obj_get_height(o),
                lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN) ? " [AN]" : "",
                (!is_roller && txtp[0]) ? " : " : "", is_roller ? "" : txtp);
  // Roller: in NGUYEN chuoi lua chon (moi dong 1 muc) - de kiem tra danh sach file man FILE SD
  // (co lan muc "." / ".." cua FAT khong, dong dau co rong khong).
  if (is_roller && txt[0]) Serial.printf("[UI] %slua chon: <<%s>>\n", ind, txt);

  // Do sau 4 (truoc la 2): hop thoai cua lv_msgbox nam sau 3-4 cap (backdrop -> hop -> content ->
  // btnmatrix) nen do sau 2 KHONG thay duoc hang nut -> khong kiem tra duoc "nut co can giua hop
  // khong / co khoang trang khong" (yeu cau nguoi dung 2026-10-15).
  if (depth >= 4) return;
  const uint32_t n = lv_obj_get_child_cnt(o);
  for (uint32_t i = 0; i < n; i++) dump_obj(lv_obj_get_child(o, i), (uint8_t)(depth + 1));
}

// In BO CUC (toa do + kich thuoc) cac vat cua trang dang mo + trang thai thanh day.
// VI SAO CO: man hinh CYD khong co framebuffer doc nguoc (driver ve thang ra SPI - xem
// tft_ili9341.h) nen khong the "chup anh" man hinh tu PC. Khi can kiem tra mot man co bi tran,
// nut co nam sat day khong, roller cao bao nhieu dong, hay NHAN nao dang hien chu gi thi doc
// bang `UI BOX` tren COM7 (in theo cay, thut le theo do sau).
// ---------------------------------------------------------------------------
//  CHAN DOAN: ban su kien CHAM vao vat bam duoc thu k cua TRANG dang mo (`UI HIT <k>` tren COM8).
//
//  VI SAO CAN: khong the cham tay vao LCD tu PC, ma duong SU KIEN lai la phan de hong nhat (quen
//  `lv_obj_add_event_cb`, sai ma su kien, ud sai...). Lenh nay di DUNG duong su kien that
//  (LV_EVENT_CLICKED -> cb_menu_row / cb_sm_row / ...) nen kiem tra duoc tu PC, giong nhu `UI TAP`
//  cua man FILE va `UI PRE` cua man NHIET DO.
//
//  Thu tu vat duoc danh so GIONG focus_first_in() (duyet sau, theo thu tu tao) -> k = 0 la dong dau
//  tien cua trang (hoac nut dau tien tren dong tieu de neu trang khong co vat bam duoc).
// ---------------------------------------------------------------------------
static lv_obj_t *hit_nth(lv_obj_t *o, uint16_t *k) {
  const uint32_t n = lv_obj_get_child_cnt(o);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    // CHI dem vat la NUT (lv_btn): do la cac vat bam duoc THAT (mk_hit/mk_btn deu la lv_btn). Neu dem
    // moi vat co co CLICK_FOCUSABLE thi ca NHAN cung bi dem (do that 2026-10-15: `UI HIT 1` "bam" vao
    // nhan "Probe Z Offset" -> khong co gi xay ra, chi so lech het so voi dong menu dang thay tren LCD).
    if (lv_obj_check_type(c, &lv_btn_class) &&
        lv_obj_has_flag(c, LV_OBJ_FLAG_CLICK_FOCUSABLE) && !lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) {
      if ((*k)-- == 0) return c;
    }
    lv_obj_t *r = hit_nth(c, k);
    if (r) return r;
  }
  return NULL;
}

// CHAN DOAN nut BACK (`UI BACK` tren COM8): goi DUNG do_action(A_BACK) nhu bam nut BACK o goc phai
// dong tieu de -> kiem tra duoc chong man hinh (menu CONFIGURATION -> nhom cha -> MENU/STATUS) tu PC.
// CHAN DOAN thanh day: `UI NEXT` / `UI PREV` = bam nut phan trang (di dung do_action nhu bam tay).
// Dung de kiem tra "BACK co tra ve dung CHO DANG XEM khong" (xem `UI BOX`: dong `scroll=... px`).
void ui_lvgl_test_scroll(const bool next) {
  Serial.printf("[UI] %s (trang %u '%s', scroll truoc = %d px)\n", next ? "NEXT" : "PREV",
                (unsigned)s_page_act, page_title(s_page_act),
                s_page[s_page_act] ? (int)lv_obj_get_scroll_top(s_page[s_page_act]) : 0);
  do_action(next ? (int)A_PAGE_NEXT : (int)A_PAGE_PREV);
  Serial.printf("[UI] %s -> scroll = %d px\n", next ? "NEXT" : "PREV",
                s_page[s_page_act] ? (int)lv_obj_get_scroll_top(s_page[s_page_act]) : 0);
}

void ui_lvgl_test_back(void) {
  Serial.printf("[UI] BACK (tu trang %u '%s')\n", (unsigned)s_page_act, page_title(s_page_act));
  do_action((int)A_BACK);
  Serial.printf("[UI] BACK -> trang %u '%s' | lop phu: SUA=%s PREHEAT=%s BANPHIM=%s\n",
                (unsigned)s_page_act, page_title(s_page_act),
                ov_is_open() ? "MO" : "dong",
                preheat_edit_active() ? "MO" : "dong",
                preheat_kb_active() ? "MO" : "dong");
}

void ui_lvgl_test_hit(const char *arg) {
  int k = 0;
  if (arg && *arg) sscanf(arg, "%d", &k);
  if (k < 0) k = 0;
  lv_obj_t *pg = s_page[s_page_act];
  if (!pg) { Serial.println("[UI] HIT: trang dang mo khong co noi dung"); return; }
  const uint8_t page0 = s_page_act;
  // LOP PHU SUA dang mo: cac nut cua no nam tren lv_layer_top() (KHONG phai con cua trang) -> tim o do
  // truoc. Chi so: 0..3 = nut buoc 0.01/0.1/1/10, 4 = '-', 5 = '+', 6 = OK, 7 = Huy -> bam duoc CA
  // duong sua + gui M-code tu PC (khong cham duoc vao LCD). HOP SUA PREHEAT 3-trong-1 cung vay:
  // 0..2 = o NOZ/BED/FAN, 3..5 = muc buoc, 6 = '-', 7 = '+', 8 = OK, 9 = Huy, 10 = Xoa.
  lv_obj_t *root = (ov_is_open() || preheat_edit_active()) ? lv_layer_top() : pg;
  uint16_t idx = (uint16_t)k;
  lv_obj_t *o = hit_nth(root, &idx);
  if (!o) {
    Serial.printf("[UI] HIT: %s chi co %u vat bam duoc (k=%d qua lon)\n",
                  root == pg ? "trang dang mo" : "lop phu SUA", (unsigned)(k - (int)idx), k);
    return;
  }
  // In TEN cua vat sap bam (nhan dau tien cua no): de biet CHAC dang bam vao dong nao, khong phai
  // suy tu chi so (chi so dem theo thu tu tao - xem hit_nth).
  const char *nm = "-";
  if (lv_obj_get_child_cnt(o)) {
    lv_obj_t *l0 = lv_obj_get_child(o, 0);
    if (lv_obj_check_type(l0, &lv_label_class)) nm = lv_label_get_text(l0);
  }
  Serial.printf("[UI] HIT %d -> CLICKED vao vat %d '%s' cua trang %u '%s'\n",
                k, k, nm, (unsigned)page0, page_title(page0));
  lv_event_send(o, LV_EVENT_CLICKED, NULL);
  // Sau su kien: trang/tieu de co the DOI (dong menu mo man khac) hoac lop phu mo ra.
  Serial.printf("[UI] HIT -> trang %u '%s' | lop phu: SUA=%s PREHEAT=%s BANPHIM=%s\n",
                (unsigned)s_page_act, page_title(s_page_act),
                ov_is_open() ? "MO" : "dong",
                preheat_edit_active() ? "MO" : "dong",
                preheat_kb_active() ? "MO" : "dong");
}

void ui_lvgl_dump_layout(void) {
  lv_obj_t *pg = s_page[s_page_act];
  Serial.printf("[UI] trang %u '%s' | vung noi dung %dx%d | thanh day %s\n",
                (unsigned)s_page_act, page_title(s_page_act), (int)PAGE_INN_W, (int)PAGE_INN_H,
                bot_shown() ? "HIEN (noi dung tran)" : "an (noi dung gon)");
  // CHO DANG XEM + VAT DANG FOCUS (de kiem tra "BACK co tra ve dung trang/dong khong"):
  //   scroll  = scroll_top (px) - 0 la dau danh sach;
  //   focus   = chi so + chu cua vat dang focus TRONG trang (dong menu nao dang duoc to sang).
  if (pg) {
    Serial.printf("[UI] scroll=%d px", (int)lv_obj_get_scroll_top(pg));
    lv_obj_t *f = s_group ? lv_group_get_focused(s_group) : NULL;
    if (!f) Serial.println(" | focus=(khong co)");
    else {
      // Tim vat dang focus trong cay con cua trang: in CHU cua no (nhan dau tien).
      lv_obj_t *fc = f;
      while (fc && lv_obj_get_parent(fc) != pg && lv_obj_get_parent(fc) != NULL &&
             lv_obj_get_parent(fc) != lv_scr_act() && lv_obj_get_parent(fc) != lv_layer_top())
        fc = lv_obj_get_parent(fc);
      const char *ft = "-";
      if (fc && lv_obj_get_child_cnt(fc)) {
        lv_obj_t *l0 = lv_obj_get_child(fc, 0);
        if (lv_obj_check_type(l0, &lv_label_class)) ft = lv_label_get_text(l0);
      }
      int16_t idx = -1;
      if (fc) for (uint32_t i = 0; i < lv_obj_get_child_cnt(pg); i++)
                if (lv_obj_get_child(pg, i) == fc) { idx = (int16_t)i; break; }
      Serial.printf(" | focus=%d '%s'\n", (int)idx, ft);
    }
  }
  if (pg) {
    const uint32_t n = lv_obj_get_child_cnt(pg);
    for (uint32_t i = 0; i < n; i++) dump_obj(lv_obj_get_child(pg, i), 1);
  }
  else Serial.println("[UI] (trang chua duoc tao)");

  // Man FILE: in them DONG DANG CHON (chi so + ten) - de doi chieu voi dong vua cham (`UI TAP <i>`).
  if (s_page_act == PG_FILES) page_files_dump();

  // Doi chieu 2 NGUON nhiet do (PUSH frame TEMPS vs dong TRAM "TBT ..."): xem ui_lvgl_dump_temps().
  ui_lvgl_dump_temps();

  // May dang lam gi + co cho nhan file upload khong (xem ui_lvgl_dump_busy).
  ui_lvgl_dump_busy();

  // Hop nao dang mo (prompt cua Marlin / XAC NHAN / THONG BAO / PID) + noi dung cua no.
  boxes_dump();

  // LOP PHU / HOP THOAI nam tren `lv_layer_top()` (msgbox, lop phu SUA, ban phim WiFi...).
  // In luon de kiem tra vi tri: hop thong bao PHAI o GIUA man hinh (khong phai goc tren-trai).
  lv_obj_t *top = lv_layer_top();
  const uint32_t nt = top ? lv_obj_get_child_cnt(top) : 0;
  Serial.printf("[UI] lop tren (hop thoai): %u vat | man hinh %dx%d\n",
                (unsigned)nt, (int)UI_LCD_HOR_RES, (int)UI_LCD_VER_RES);
  for (uint32_t i = 0; i < nt; i++) dump_obj(lv_obj_get_child(top, i), 1);

  // LOP PHU TAO TREN `lv_scr_act()` (KHONG phai con cua trang): BAN PHIM nhap mat khau WiFi / dat ten vat
  // lieu, lop phu "DANG QUET MANG". In ra de kiem tra kich thuoc + vi tri - dac biet "ban phim co fill het
  // be ngang man hinh khong" va "hang phim cuoi co bi cat khong" (yeu cau nguoi dung 2026-10-15).
  lv_obj_t *scr = lv_scr_act();
  if (scr) {
    const uint32_t ns = lv_obj_get_child_cnt(scr);
    Serial.println("[UI] lop phu tren MAN (khong thuoc trang):");
    for (uint32_t i = 0; i < ns; i++) {
      lv_obj_t *c = lv_obj_get_child(scr, i);
      bool is_page = false;
      for (uint8_t p = 0; p < PG_COUNT; p++) if (s_page[p] == c) { is_page = true; break; }
      if (is_page) continue;
      dump_obj(c, 1);
    }
  }
}

// Mo 1 hop thoai XAC NHAN de kiem tra bo cuc (lenh `UI TESTBOX` tren COM7). Action 0xFF khong
// tuong ung A_* nao -> bam OK/CANCEL deu khong lam gi (xem do_action(): default: break).
void ui_lvgl_test_dialog(void) {
  ui_confirm("KIEM TRA", "Hop thoai nay phai nam GIUA man hinh", 0xFF, 1);
  ui_confirm_autoclose(6000);      // tu dong dong sau 6 s (khong de hop modal nam tren man)
}

// CHAN DOAN: in CA HAI nguon nhiet do ma man hinh dang co, de doi chieu - lenh `UI TEMPS` tren COM7
// (va tu dong in kem trong `UI BOX`).
//   PUSH : frame CYDLCD_F_TEMPS tu Marlin (che do PUSH - dang dung) - nhiet do THUC + target THUC;
//   TRAM : 4 token nhiet do o CUOI dong "TBT ..." (che do TRAM, gui ~5 Hz).
// VI SAO CAN: `temp_lookup()` (ui_widgets.cpp) UU TIEN nguon TRAM khi `tbt.ever` = true, nen neu dong
// TBT mang gia tri cu/0 thi man hinh se hien SAI (vd target ban in luon 0) du frame PUSH dung.
void ui_lvgl_dump_temps(void) {
  Serial.printf("[UI] PUSH: %u muc", (unsigned)cydlcd.n_temps);
  for (uint8_t i = 0; i < cydlcd.n_temps; i++)
    Serial.printf(" | id=%u cur=%d tgt=%d", (unsigned)cydlcd.temps[i].id,
                  (int)cydlcd.temps[i].cur, (int)cydlcd.temps[i].tgt);
  Serial.println();
  Serial.printf("[UI] TRAM: ever=%u linked=%u | E0 %d/%d | BED %d/%d\n",
                (unsigned)tbt.ever, (unsigned)tbt.linked,
                (int)tbt.st.t_e0, (int)tbt.st.t_e0_tgt, (int)tbt.st.t_bed, (int)tbt.st.t_bed_tgt);
  int16_t c = 0, t = 0;
  temp_lookup(CYDLCD_TEMP_E0,  &c, &t);
  Serial.printf("[UI] dang HIEN THI (temp_lookup): NOZZLE %d/%d", (int)c, (int)t);
  temp_lookup(CYDLCD_TEMP_BED, &c, &t);
  Serial.printf(" | BED %d/%d  (scale /100)\n", (int)c, (int)t);
}

// CHAN DOAN: bang MAU cua dai nhiet do (man STATUS dung cho ca icon lan 2 con so) - lenh `UI GRAD` tren COM7.
// VI SAO CAN: "khi nhiet do tang dan thi mau co chuyen dan sang do khong?" - khong the tra loi bang mat thuong
// neu khong dot nong may (va dot nong thi phai xin phep), nen in thang mau ra console: console doc duoc,
// con man hinh chi co 1 khung mau tai 1 nhiet do. Moc dau/cuoi lay tu MR_TEMP_*_MIN/MAX (ui_lvgl_int.h) -
// dung CHUNG hang so voi man STATUS nen bang nay luon khop voi cai dang hien.
void ui_lvgl_test_grad(void) {
  int16_t c = 0, t = 0, hot_c = 0, bed_c = 0;
  temp_lookup(CYDLCD_TEMP_E0,  &c, &t);
  hot_c = c;                            // giu lai truoc khi goi lan 2 (temp_lookup ghi de ca cap cur/tgt)
  temp_lookup(CYDLCD_TEMP_BED, &c, &t);
  bed_c = c;
  // temp_lookup() tra ve don vi 1/100 do C -> chia 100 cho khop dai mau (xem chu thich o page_status_update()).
  const int16_t hotd = (int16_t)(hot_c / 100), bedd = (int16_t)(bed_c / 100);
  Serial.printf("[UI] GRAD hotend %d..%d | bed %d..%d (moc giua = MR_WARM #%06lX)\n",
                MR_TEMP_HOT_MIN, MR_TEMP_HOT_MAX, MR_TEMP_BED_MIN, MR_TEMP_BED_MAX,
                (unsigned long)lv_color_to32(MR_WARM));
  Serial.printf("[UI] GRAD dang HIEN THI: hotend %d.%02d do C -> #%06lX | bed %d.%02d do C -> #%06lX\n",
                (int)(hot_c / 100), (int)(hot_c % 100),
                (unsigned long)lv_color_to32(temp_ramp(hotd, MR_TEMP_HOT_MIN, MR_TEMP_HOT_MAX, MR_HOTEND)),
                (int)(bed_c / 100), (int)(bed_c % 100),
                (unsigned long)lv_color_to32(temp_ramp(bedd, MR_TEMP_BED_MIN, MR_TEMP_BED_MAX, MR_BEDHOT)));
  // Bang quet: in ca % vi tri trong dai de doi chieu voi cong thuc, khong chi mau.
  static const int16_t hot[] = { 10, 25, 50, 75, 100, 125, 150, 175, 200, 225, 250, 260 };
  Serial.print("[UI] GRAD hotend:");
  for (uint8_t i = 0; i < sizeof(hot) / sizeof(hot[0]); i++) {
    const int32_t u = (hot[i] - MR_TEMP_HOT_MIN) * 100 / (MR_TEMP_HOT_MAX - MR_TEMP_HOT_MIN);
    Serial.printf(" %dC(%ld%%)=#%06lX", (int)hot[i], (long)(u < 0 ? 0 : (u > 100 ? 100 : u)),
                  (unsigned long)lv_color_to32(temp_ramp(hot[i], MR_TEMP_HOT_MIN, MR_TEMP_HOT_MAX, MR_HOTEND)));
  }
  static const int16_t bed[] = { 0, 5, 10, 15, 20, 25, 30, 40, 50, 60, 70 };
  Serial.print("\n[UI] GRAD bed   :");
  for (uint8_t i = 0; i < sizeof(bed) / sizeof(bed[0]); i++) {
    const int32_t u = (bed[i] - MR_TEMP_BED_MIN) * 100 / (MR_TEMP_BED_MAX - MR_TEMP_BED_MIN);
    Serial.printf(" %dC(%ld%%)=#%06lX", (int)bed[i], (long)(u < 0 ? 0 : (u > 100 ? 100 : u)),
                  (unsigned long)lv_color_to32(temp_ramp(bed[i], MR_TEMP_BED_MIN, MR_TEMP_BED_MAX, MR_BEDHOT)));
  }
  Serial.println();
}

// CHAN DOAN duong PANEL -> MARLIN (lenh `UI HELLO` tren COM7): in trang thai HELLO + so tham so/danh sach dang
// co, roi XIN LAI FULL + SETTINGS + MESH y nhu luc panel vua khoi dong lai.
// VI SAO CAN (loi that 2026-10-15): flash lai PANEL xong thi `hello_flags` MAI = 0 - panel khong bao gio nhan
// duoc HELLO du link PUSH van chay => trang BED MESH bao "khong ho tro" va `n_settings` tut ve vai muc. Lenh nay
// cho biet: (1) panel CO gui duoc REQUEST sang Marlin khong, (2) Marlin co tra loi khong. Xem them cydlcd_link.h
// (resync `s_nohello_req`) - resync chi thu 5 lan roi thoi, nen neu 5 lan do truot thi phai xin tay.
void ui_lvgl_test_hello(void) {
  Serial.printf("[UI] LINK: linked=%u hello_flags=0x%02X (MESH=%u SD=%u) | n_settings=%u n_files=%u"
                " | mesh %ux%u rows %u | tx cho=%u bo=%lu\n",
                (unsigned)cydlcd.linked, (unsigned)cydlcd.hello_flags,
                (unsigned)((cydlcd.hello_flags & CYDLCD_HAS_MESH) ? 1 : 0),
                (unsigned)((cydlcd.hello_flags & CYDLCD_HAS_SD) ? 1 : 0),
                (unsigned)cydlcd.n_settings, (unsigned)cydlcd.n_files,
                (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny, (unsigned)cydlcd.mesh_rows,
                (unsigned)cydlcd_link_tx_pending(), (unsigned long)cydlcd_link_tx_dropped());
  Serial.println("[UI] LINK: gui lai REQ_FULL + REQ_SETTINGS + REQ_MESH (cho ~2 s roi chay lai lenh nay)");
  cydlcd_link_request((uint8_t)CYDLCD_REQ_FULL);
  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);
  cydlcd_link_request((uint8_t)CYDLCD_REQ_MESH);
}

// CHAN DOAN 2 CHIEU CUA UART (lenh `UI RX` tren COM8): in so frame da nhan theo LOAI + tuoi PONG + vi tri
// dang giu. VI SAO CAN: hai chieu hong DOC LAP - da gap that 2026-10-15: Marlin van day STATUS deu (chieu
// xuong tot) nhung lenh tu man hinh (M220/G-code/REQUEST) khong toi Marlin => moi thu tren man hinh "nhu
// binh thuong" nhung bam nut khong chay, va khong co cho nao bao. Bang nay cho biet ngay ben nao hong:
//   PONG  = Marlin tra loi PING (0 lan / tuoi lon = chieu MAN HINH -> MARLIN hong, hoac day TX long),
//   POS/STATUS/TEMPS = chieu MARLIN -> MAN HINH (con so la "dang nhan duoc gi").
// Doi chieu voi `PANEL DIAG` in ra COM4 (xem cydlcd_serial.cpp) de biet frame co toi Marlin khong.
void ui_lvgl_test_rx(void) {
  const uint32_t age = cydlcd_link_pong_age_ms();
  Serial.printf("[UI] RX: linked=%u hello=0x%02X | PONG %s | mat PONG %u lan | RSSI %d\n",
                (unsigned)cydlcd.linked, (unsigned)cydlcd.hello_flags,
                (age == 0xFFFFFFFFu) ? "CHUA BAO GIO" : "co", (unsigned)cydlcd_link_pong_lost(),
                (int)net_wifi_rssi());
  if (age != 0xFFFFFFFFu) Serial.printf("[UI] RX: PONG cuoi %lu ms truoc (binh thuong < 1500 ms)\n",
                                        (unsigned long)age);
  Serial.printf("[UI] RX: HELLO=%u STATUS=%u TEMPS=%u POS=%u PROGRESS=%u FANS=%u MESH=%u FILES=%u "
                "SETTING=%u EVENT=%u PROMPT=%u PONG_F=%u TBT=%u ENDSTOP=%u\n",
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_HELLO),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_STATUS),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_TEMPS),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_POSITION),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_PROGRESS),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_FANS),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_MESH),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_FILES),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_SETTING),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_EVENT),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_PROMPT),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_PONG),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_TBT),
                (unsigned)cydlcd_link_rx_count(CYDLCD_F_ENDSTOPS));
  Serial.printf("[UI] RX: POS x=%ld y=%ld z=%ld e=%ld | FR/FL %d/%d | n_settings=%u mesh %ux%u rows %u\n",
                (long)(cydlcd.pos.x / 1000), (long)(cydlcd.pos.y / 1000), (long)(cydlcd.pos.z / 1000),
                (long)(cydlcd.pos.e / 1000), (int)cydlcd.pos.feed_pct, (int)cydlcd.pos.flow_pct,
                (unsigned)cydlcd.n_settings, (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny,
                (unsigned)cydlcd.mesh_rows);
}

// CHAN DOAN HIEU NANG man hinh (lenh `UI PERF` tren COM8): goi 2 lan cach nhau N giay - lan 1 de XOA bo dem,
// lan 2 doc so cua khoang do. VI SAO CAN: "man hinh chay cham" co the do LVGL ve lai qua nhieu (dien tich lon),
// SPI cham, doc cam ung cham, hay vong lap bi chan o cho khac. Cac so duoi day TACH RA tung phan:
//   - `lv_timer_handler`: thoi gian ve moi vong lap (bao gom CA flush SPI vi flush_cb chay dong bo trong do),
//   - `SPI` (ui_port_perf_report): so lan flush + TONG SO PIXEL da day -> biet co phai ve lai CA MAN khong
//     (1 man = UI_LCD_HOR_RES x UI_LCD_VER_RES = 76 800 px),
//   - `cam ung`: thoi gian doc XPT2046 (bit-bang) moi lan LVGL hoi (20 ms/lan).
// Doi chieu voi dong `[CYD] chang: ... ui=NN ms` (1 Hz, in o main.cpp).
void ui_lvgl_test_perf(void) {
  const uint32_t n = s_pf_loop_n, tn = s_pf_task_n;
  Serial.printf("[PERF] ui_lvgl_task: %lu vong | tb %lu us, max %lu us | tong %lu us\n",
                (unsigned long)tn, (unsigned long)(tn ? s_pf_task_us / tn : 0),
                (unsigned long)s_pf_task_max_us, (unsigned long)s_pf_task_us);
  Serial.printf("[PERF] lv_timer_handler: %lu vong lap | tb %lu us, max %lu us | tong %lu us\n",
                (unsigned long)n, (unsigned long)(n ? s_pf_hdl_us / n : 0),
                (unsigned long)s_pf_hdl_max_us, (unsigned long)s_pf_hdl_us);
  ui_port_perf_report();
  net_upload_http_perf_report();      // tach chang `http` (wifi / the TF / handleClient) - xem net_upload_http.h
  cyd_sd_perf_report();               // tach tiep phan cham vao the TF (sd_alive / clean_dir)
  {
    uint32_t slow = 0;
    const uint32_t gmax = ui_lvgl_perf_link_take(&slow);
    Serial.printf("[PERF] link (serial Marlin): gap max %lu us | so lan gap > 50 ms: %lu | so lan goi: %lu\n",
                  (unsigned long)gmax, (unsigned long)slow, (unsigned long)s_pf_link_n);
  }
  lv_mem_monitor_t lm;
  lv_mem_monitor(&lm);
  Serial.printf("[PERF] lvmem: %lu/%lu B (frag %u%%, max free %lu B) | heap %u B\n",
                (unsigned long)(lm.total_size - lm.free_size), (unsigned long)lm.total_size,
                (unsigned)lm.frag_pct, (unsigned long)lm.free_biggest_size, (unsigned)ESP.getFreeHeap());
  s_pf_loop_n = s_pf_hdl_us = s_pf_hdl_max_us = 0;
  s_pf_task_n = s_pf_task_us = s_pf_task_max_us = 0;
}

// CHAN DOAN: MAY DANG LAM GI + co cho nhan file upload khong - lenh `UI BUSY` tren COM7 (cung in kem
// trong `UI BOX`). Tra loi cau hoi "lam sao biet may in dang in?": xem `st` (trang thai Marlin bao),
// `file`/`prog` (dang in file nao) va `IN TU THE SD` (co THAT SU in tu the hay khong).
// `upload bi chan` CHI bat khi dang in tu the SD / tam dung ban in tu the (xem printer_busy() o cyd_sd.cpp):
// HOME/PROBE (vd G35) KHONG con bi coi la "dang in".
void ui_lvgl_dump_busy(void) {
  const bool sd = (cydlcd.flags & CYDLCD_SF_SD_PRINTING) != 0;
  const bool ps = (cydlcd.flags & CYDLCD_SF_PAUSED) != 0;
  const bool busy = cyd_sd_printer_busy();
  Serial.printf("[UI] MAY: st=%u (%s) fl=0x%02X file=\"%s\" prog=%u%% | IN TU THE SD=%u TAM DUNG=%u%s\n",
                (unsigned)cydlcd.state, state_name(cydlcd.state), (unsigned)cydlcd.flags,
                cydlcd.prog.file, (unsigned)cydlcd.progress_pct, (unsigned)sd, (unsigned)ps,
                (cydlcd.flags & CYDLCD_SF_LEVELING) ? " DANG LEVELING" : "");
  Serial.printf("[UI] upload: %s%s\n", busy ? "BI CHAN" : "cho nhan file",
                busy ? " (dang in / tam dung ban in TU THE SD - dung in roi gui lai)" : "");
  // Trang thai BED MESH/UBL (byte thu 4 cua CYDLCD_F_STATUS tu 2026-10-15) + kich thuoc luoi + da luu chua.
  Serial.printf("[UI] mesh: UBL %s | luoi %ux%u rows %u | %s | co MESH=%s\n",
                cydlcd.mesh_on ? "ON (dang bu)" : "OFF", (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny,
                (unsigned)cydlcd.mesh_rows, cydlcd.mesh_valid ? "da co mesh" : "CHUA co mesh",
                (cydlcd.hello_flags & CYDLCD_HAS_MESH) ? "co" : "khong");
}

// CHAN DOAN: bam nut preheat cua man TEMP tu console. Tu 2026-10-15 lenh `UI PRE ...` duoc xu ly boi
// preheat_test_cmd() (ui_page_preheat.cpp) vi preheat la DANH SACH dong (them/bot vat lieu); ham nay
// giu lai cho tuong thich (khong con duoc goi tu main.cpp).
void ui_lvgl_test_preheat(const char *arg) {
  int k = -1;
  if (arg && *arg) sscanf(arg, "%d", &k);
  if (k < 0 || k >= (int)ui_preheat_count()) {
    Serial.printf("[UI] dunghoac: UI PRE <0..%u>  (mo man 'UI NHIET DO' truoc)\n",
                  (unsigned)(ui_preheat_count() ? ui_preheat_count() - 1 : 0));
    return;
  }
  Serial.printf("[UI] bam nut preheat '%s'\n", ui_preheat_name((uint8_t)k));
  temp_test_preheat((uint8_t)k);
}

// CHAN DOAN THONG BAO trang /status - lenh `UI NOTIFY [ms] <text>` tren COM8.
// VI SAO CO: hop thong bao cua Marlin (co nut OK + tu an) nay KHONG hien tren LCD nua ma chi vao trang
// /status (xem ui_boxes.cpp) -> can 1 lenh de KIEM TRA duong do tu PC ma khong phai cho Marlin gui thong bao.
//   UI NOTIFY                 -> in thong bao dang luu
//   UI NOTIFY 0               -> xoa thong bao dang luu
//   UI NOTIFY 5000 Dang in... -> dat thong bao tu an sau 5000 ms
//   UI NOTIFY 0 giu mai       -> dat thong bao KHONG co thoi gian (giu lai den khi co cai moi)
// (Khong co tham so dau tien la so -> hieu la ttl 0 + ca chuoi con lai la text, de go nhanh.)
void ui_lvgl_test_notify(const char *arg) {
  const char *p = arg ? arg : "";
  while (*p == ' ') p++;
  if (!*p) { net_notify_dump(); return; }            // khong tham so -> in dang luu
  uint32_t ttl = 0;
  if (*p >= '0' && *p <= '9') {                       // co thoi gian -> doc so
    unsigned v = 0;
    if (sscanf(p, "%u", &v) == 1) ttl = v;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
  }
  if (!*p) {                                          // chi co so (hoac "0") -> xoa
    net_notify_clear();
    Serial.println("[UI] da xoa thong bao (trang /status se het hien)");
    return;
  }
  net_notify_set(p, 0 /* sev = info */, ttl);
  Serial.printf("[UI] dat thong bao: \"%s\" (ttl=%lu ms%s)\n", p, (unsigned long)ttl,
                ttl ? "" : " - giu mai");
}

// CHAN DOAN: ban su kien CHAM vao man FILE (lenh `UI TAP <gi>` tren COM7). Chi co y nghia khi man
// FILE dang mo. Dung de kiem tra duong "cham 1 dong -> chon dung dong do" tu PC.
void ui_lvgl_test_tap(const char *arg) {
  // 'K' = bam OK cua HOP THOAI dang mo: dung duoc o MOI man (hop prompt cua Marlin / thong bao
  // khong thuoc man FILE) nen phai kiem tra TRUOC khi doi hoi man FILE.
  const char *a = arg;
  while (a && *a == ' ') a++;
  if (a && (*a == 'K' || *a == 'k')) { boxes_test_ok(); return; }

  if (s_page_act != PG_FILES) {
    Serial.println("[UI] TAP chi dung cho man FILE (mo truoc bang 'UI FILE SD'); rieng 'UI TAP K' (bam OK hop thoai) dung duoc o moi man");
    return;
  }
  files_test_cmd(arg);
}

#endif  // CYDLCD_USE_LVGL
