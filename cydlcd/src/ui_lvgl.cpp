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

#if CYDLCD_USE_LVGL

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <Arduino.h>                // Serial.printf cho log tu kiem tra font (che do 4)

#include "cydlcd_link.h"
#include "cyd_sd.h"        // cyd_sd_ready() cho icon the SD (the o khe man hinh)
#include "tbt_link.h"              // Bed Tramming Wizard cua Marlin (link ASCII, xem tbt_link.h)
#include "ui_lvgl_icons.h"          // icon 64x64/32x32 chuyen tu bitmap cua Marlin
#include "lv_font_gfxfont.h"        // driver font 1-bit Adafruit GFXfont (che do UI_MONO_FONT 4)
#include "net_upload.h"             // net_wifi_state(): trang thai WiFi de ve icon o header

// Log chan doan cua UI tren board that (COM7, 115200). Dat NGAY DAY de moi phan trong file dung duoc
// (truoc day dinh nghia trong khoi trang WIFI o duoi -> cac khoi phia tren khong thay).
#if defined(CYDLCD_BOARD_CYD)
  #define WIFILOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define WIFILOG(...)   do { } while (0)
#endif

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
static uint8_t wifi_state_now(void) {
  // Module WiFi co the CHUA duoc build -> con tro ham = NULL -> coi nhu trang thai 0.
  return net_wifi_state ? net_wifi_state() : 0;
}

// Trang thai THE SD (the nam o khe MAN HINH, xem cyd_sd.h) cho icon THE NHO tren man STATUS.
// Trang thai THE TF da chuan hoa ve 3 muc - de phan biet "khong co the" voi "khong biet":
//   SD_ST_NONE = khong co accessor  -> icon XAM (khong phai loi)
//   SD_ST_NO   = the chua mount / loi           -> icon DO
//   SD_ST_OK   = the dang dung duoc             -> icon XANH
enum { SD_ST_NONE = 0, SD_ST_NO, SD_ST_OK };
static uint8_t sd_state_now(void) {
#if CYDLCD_SD_SHARED
  // THE DUNG CHUNG (mux SPI, xem cydlcd_config.h): the nam trong khe CYD nhung do MARLIN mount
  // (man hinh chi gianh bus khi nhan file). Vi vay trang thai that su lay tu co CYDLCD_SF_MEDIA
  // trong frame PUSH cua Marlin - KHONG tu mount the de kiem (se gianh bus cua Marlin).
  return (cydlcd.flags & CYDLCD_SF_MEDIA) ? SD_ST_OK : SD_ST_NO;
#else
  return cyd_sd_ready() ? SD_ST_OK : SD_ST_NO;
#endif
}

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
// Co API WiFi khong (de trang WIFI bao "khong ho tro" thay vi hien "TAT" gay hieu nham).
static bool wifi_api_ok(void) { return net_wifi_scan != NULL; }

// ---------------------------------------------------------------------------
//  Font ICON cho header + man STATUS: gfx4_font_icons.h/.c (ban NHO, 14 px) va
//  gfx4_font_icons_big.h/.c (ban TO, box <= 30 px - cho icon WiFi tren man STATUS).
//  Ca 2 ban dung CUNG codepoint (U+E000 wifi / U+E001 wifi_off / U+E002 3 vach) nen chuoi UTF-8
//  khong doi, chi khac co chu.
//    U+E000 = wifi          U+E001 = wifi_off        (ban nho: bpp4, <= 14x12)
//  DUONG LUI (BAT BUOC, tung ban doc lap): nhanh font co the CHUA merge -> __has_include quyet
//  dinh. Thieu ban nho -> header dung CHAM TRON; thieu ban to -> icon STATUS dung ban nho (14 px);
//  thieu ca 2 -> cham tron. Build luon SUCCESS.
//
//  KHONG include "gfx4_font_icons*.h" o day: mang bitmap/glyph trong header la `static` va header
//  chi duoc include o DUNG file .c tuong ung (include them se nhan doi du lieu flash).
//  Thay vao do dung ham tien loi cua chinh ho (dinh nghia trong .c, linkage C):
//      const lv_font_t *gfx4_font_icons_get(void);
//      const lv_font_t *gfx4_font_icons_big_get(void);
// ---------------------------------------------------------------------------
#if defined(__has_include)
  #if __has_include("gfx4_font_icons.h")
    #define UI_HAVE_ICON_FONT 1
  #endif
  #if __has_include("gfx4_font_icons_big.h")
    #define UI_HAVE_ICON_FONT_BIG 1
  #endif
#endif
#if !defined(UI_HAVE_ICON_FONT)
  #define UI_HAVE_ICON_FONT 0
#endif
#if !defined(UI_HAVE_ICON_FONT_BIG)
  #define UI_HAVE_ICON_FONT_BIG 0
#endif

#if UI_HAVE_ICON_FONT
  // Tra descriptor lv_font_t cua font icon (ban NHO - dung cho icon o header 26 px).
  // Driver lv_font_gfxfont nho san descriptor (goi lai khong cap phat them); xem
  // LV_FONT_GFXFONT_MAX_FONTS trong lv_font_gfxfont.c (phai >= so font gfxfont dung dong thoi).
  extern "C" const lv_font_t *gfx4_font_icons_get(void);
#endif
#if UI_HAVE_ICON_FONT_BIG
  // Ban TO cua cung bo glyph (man STATUS: icon WiFi to bang icon FR/FL 32x32).
  extern "C" const lv_font_t *gfx4_font_icons_big_get(void);
#endif

// Chuoi UTF-8 cua cac glyph - GIONG NHAU o ca 2 co font (ban nho va ban to), nen khong phu thuoc
// viec file font nao co mat. (U+E004/U+E005 'the nho'/'the loi' co trong font nhung KHONG dung:
// man STATUS da co san icon the 64x64 cua Marlin, chi doi mau - xem status_sd_icon_mark().)
#define WIFI_GLYPH_ON    "\xEE\x80\x80"   // U+E000 wifi        -> DA ket noi
#define WIFI_GLYPH_CONN  "\xEE\x80\x82"   // U+E002 3 vach      -> DANG ket noi
#define WIFI_GLYPH_OFF   "\xEE\x80\x81"   // U+E001 wifi_off    -> WiFi tat / chua cau hinh
#define HOME_GLYPH       "\xEE\x80\x87"   // U+E007 ngoi nha    -> nut HOME o dau thanh tieu de

// Font co DAU tieng Viet: chi con font MONO GFX TU SINH (che do 4) - xem tools/ttf2gfxfont.py
// + lv_font_gfxfont.c. KHONG con font nao sinh bang lv_font_conv: ca 3 file font do da XOA khoi
// repo (che do 1 va 2 da thu tren may that deu MAT CHU; che do 3 chua lam).
//
// Chon font cho UI (chi con 2 che do):
//   4 = font MONO GFX TU SINH (MAC DINH): gfx4_font_robotomono qua driver lv_font_gfxfont, gan
//       TUNG nhan/nut (xem mono_obj). CO DU DAU tieng Viet (5 dai roi rac - xem UI_GFX_RANGES).
//       Ten man (s_title) giu Montserrat DOI CHUNG. Do phan giai mau: UI_MONO_GFX_BPP o duoi.
//   0 = chi Montserrat 14 mac dinh cua LVGL (du phong khi khong muon font GFX).
//       >>> KHONG HIEN DUOC DAU TIENG VIET: Montserrat khong co U+1EA0..U+1EF9, va vi da xoa het
//       font sinh bang lv_font_conv nen KHONG con font nao co dau -> vn_font()/vn_font_btn() thanh
//       no-op va 3 nhan co dau trong lop phu SUA ("Gia tri" / "Buoc" / "Huy") se ra O VUONG.
//       Muon co dau tieng Viet thi PHAI dung che do 4.
// Cho phep ep tu ngoai (-DUI_MONO_FONT=0) de kiem tra che do 0 ma khong phai sua file.
#ifndef UI_MONO_FONT
  #define UI_MONO_FONT 4
#endif

// Do phan giai cua font mono che do 4 (xem lv_font_gfxfont.h):
//   4 = 4 bit/pixel, 16 muc xam -> CO antialias (dang dung; chu dep hon han 1-bit).
//       Header gfx4_font_robotomono.h KHONG phai Adafruit GFXfont: bitmap da ghi theo dung
//       chuoi bit lien tuc cua LVGL nen driver tra con tro THANG (khong repack, khong ton RAM).
//   1 = 1 bit/pixel, Adafruit GFXfont (gfx_font_robotomono.h) -> driver phai repack.
//       Giu lai lam PHUONG AN DU PHONG (da nghiem thu tren may that, chu bi rang nhung chac chan chay).
#define UI_MONO_GFX_BPP 4

#if UI_MONO_FONT == 4
// ---------------------------------------------------------------------------
//  Che do 4: font mono tu sinh.
//  - 5 dai roi rac (khong the la 1 bang 0x20..0x1EF9: ton ~55 KB vo ich).
//  - Moi dai la mot bang glyph rieng; lv_font_gfxfont_get_ex() tra tuan tu.
//  - line_height/base_line lay tu du lieu glyph cua CA 5 dai (glyph tieng Viet cao hon ASCII).
//  - Ca 2 ban font (1bpp va 4bpp) cung size 13 nen co CUNG so do: xAdvance 8, yAdvance 17,
//    base_line 3 -> doi qua lai khong lam xo lech bo cuc UI. Chi 1 ban duoc LINK vao
//    (ban khong dung bi linker GC khoi flash).
// ---------------------------------------------------------------------------
#if UI_MONO_GFX_BPP == 4
  #define UI_GFX_LAYOUT LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4
  extern const GFXfont gfx4_font_robotomono_ascii;  // U+0020..U+00FF
  extern const GFXfont gfx4_font_robotomono_vn1;    // U+0102..U+0103  A(breve) a(breve)
  extern const GFXfont gfx4_font_robotomono_vn2;    // U+0110..U+0111  D/ d/ gach ngang
  extern const GFXfont gfx4_font_robotomono_vn3;    // U+01A0..U+01B1  O/ o/ U/ u/ moc
  extern const GFXfont gfx4_font_robotomono_vn4;    // U+1EA0..U+1EF9  to hop dau tieng Viet
#elif UI_MONO_GFX_BPP == 1
  #define UI_GFX_LAYOUT LV_FONT_GFXFONT_LAYOUT_ADAFRUIT_BPP1
  extern const GFXfont gfx_font_robotomono_ascii;
  extern const GFXfont gfx_font_robotomono_vn1;
  extern const GFXfont gfx_font_robotomono_vn2;
  extern const GFXfont gfx_font_robotomono_vn3;
  extern const GFXfont gfx_font_robotomono_vn4;
#else
  #error "UI_MONO_GFX_BPP chi nhan 1 hoac 4"
#endif

#define UI_GFX_RANGE_COUNT 5
#define UI_GFX_RANGES(prefix) { \
    &prefix##_ascii, &prefix##_vn1, &prefix##_vn2, &prefix##_vn3, &prefix##_vn4 }

#if UI_MONO_GFX_BPP == 4
static const GFXfont *const UI_FONT_RANGES[UI_GFX_RANGE_COUNT] =
  UI_GFX_RANGES(gfx4_font_robotomono);
#else
static const GFXfont *const UI_FONT_RANGES[UI_GFX_RANGE_COUNT] =
  UI_GFX_RANGES(gfx_font_robotomono);
#endif

// Dien trong ui_lvgl_begin() (sau lv_init). Neu NULL (font loi) thi UI_MONO_FACE lui ve
// Montserrat: mat dau tieng Viet nhung KHONG mat het chu - duong lui an toan.
static const lv_font_t *s_gfx_mono = NULL;
#endif

// <<< DOI FONT MONO O DUY NHAT KHOI NAY >>>
#if UI_MONO_FONT == 4
  #define UI_MONO_FACE (s_gfx_mono ? s_gfx_mono : &lv_font_montserrat_14)
#else
  #define UI_MONO_FACE (&lv_font_montserrat_14)   // che do 0: Montserrat (KHONG co dau tieng Viet)
#endif

#if UI_MONO_FONT == 4
  // Che do 4: gan font mono GFX cho TUNG nhan/nut (khong dat o cap man hinh).
  #define mono_obj(o) lv_obj_set_style_text_font((o), UI_MONO_FACE, 0)
#else
  // Che do 0: dung font mac dinh cua LVGL (Montserrat) - khong doi gi.
  #define mono_obj(o) ((void)0)
#endif

// Port day pixel ra LCD lam viec truc tiep tren buffer 16-bit (xem ui_lvgl_port.cpp)
static_assert(LV_COLOR_DEPTH == 16, "UI nay chi ho tro LV_COLOR_DEPTH 16");
static_assert(sizeof(lv_color_t) == 2, "lv_color_t phai la 2 byte");

// ===========================================================================
//  Mau -- DONG NHAT theo Marlin/src/lcd/tft/tft_color.h (moi man dung cung bang mau)
// ===========================================================================
#define MR_BG      lv_color_hex(0x1E156E)   // COLOR_BACKGROUND
#define MR_COLD    lv_color_hex(0x00E5FF)   // Material 3 dark cyan A400 (12.07 -> 9.84:1, do choi hon)
#define MR_HOTEND  lv_color_hex(0xFF8A80)   // Material 3 dark red A100: 3.95 -> 6.70:1 (truoc la chu thuong < 4.5)
#define MR_BEDHOT  lv_color_hex(0xFFB74D)   // Material 3 dark amber 300: 6.49 -> 8.74:1
#define MR_AXIS    lv_color_hex(0xFFFFFF)   // COLOR_AXIS_HOMED / COLOR_CONTROL_ENABLED
#define MR_NOAXIS  lv_color_hex(0xFFFF00)   // COLOR_AXIS_NOT_HOMED / COLOR_MENU_TEXT
#define MR_RATE100 lv_color_hex(0x81C784)   // Material 3 dark green 300: 11.68 -> 7.52:1
#define MR_RATEALT lv_color_hex(0xFFD54F)   // Material 3 dark amber 300: 16.18 -> 10.73:1
#define MR_TIME    lv_color_hex(0x00E5FF)   // Material 3 dark cyan A400
#define MR_MSG     lv_color_hex(0xFFD54F)   // Material 3 dark amber 300
#define MR_PROGBAR lv_color_hex(0x00E5FF)   // Material 3 dark cyan A400: xanh tren den chi 2.44:1 -> 8.44:1
#define MR_PROGBG  lv_color_hex(0x2A2080)   // Material 3 dark: nen rong = mau nut (truoc la den tuyet doi)
#define MR_FRAME   lv_color_hex(0xE6E1E5)   // Material 3 dark on-surface
#define MR_MENU    lv_color_hex(0xFFD54F)   // Material 3 dark amber 300
#define MR_MENUVAL lv_color_hex(0xE6E1E5)   // Material 3 dark on-surface: 15.13 -> 11.72:1
#define MR_SELBG   lv_color_hex(0x4F378B)   // Material 3 dark primary-container: chu trang 7.22:1
#define MR_DISABLED lv_color_hex(0x9E9AA7)  // Material 3 dark: chu mo 3.83/3.29 -> 5.50:1 (truoc < 4.5)

// Cac man con dung cung bang mau nay (khong con theme rieng)
#define COL_BG     MR_BG
#define COL_BAR    lv_color_hex(0x160C52)   // dong tieu de: dam hon nen
#define COL_TXT    MR_MENUVAL
#define COL_DIM    lv_color_hex(0xCAC4D0)   // Material 3 dark on-surface-variant: 6.53 -> 8.88:1
#define COL_BTN    lv_color_hex(0x2A2080)   // nen nut
#define COL_ACCENT MR_COLD                   // focus / diem nhan
#define COL_WARN   MR_RATEALT
#define COL_ERR    MR_HOTEND
#define COL_OK     MR_RATE100

// ===========================================================================
//  Man hinh
// ===========================================================================
enum {
  PG_STATUS = 0, PG_MENU, PG_RATE, PG_TEMP, PG_JOG, PG_FILES, PG_SETTINGS, PG_MESH, PG_CUSTOM,
  PG_TBT, PG_INFO, PG_ENDSTOP,
  PG_WIFI,                 // trang WIFI (mo tu icon WiFi o header/man STATUS hoac muc "WiFi" MENU)
  PG_COUNT
};

static const char *const PG_TITLE[PG_COUNT] = {"STATUS", "MENU", "FEED/FLOW", "NHIET DO", "JOG",
                                               "FILE SD", "THONG SO", "BED MESH", "CUSTOM",
                                               "TRAMMING", "INFO", "ENDSTOP", "WIFI"};

// ===========================================================================
//  Hinh hoc 320x240
//  A) Man STATUS: toa do y nguyen tu Marlin/src/lcd/tft/ui_320x240.cpp
//  B) Cac man con: 1 dong tieu de (BAR_H) + noi dung flex (PAGE_INN_*)
// ===========================================================================
// --- A) STATUS (Marlin) ---
#define MR_W         UI_LCD_HOR_RES                       // 320
#define MR_H         UI_LCD_VER_RES                       // 240
#define MR_CELL_W    64                                   // o nhiet/quat: 64x100
#define MR_CELL_H    100
#define MR_CELL_X(i) ((lv_coord_t)(21 + 106 * (int)(i)))  // 3 o: 21 / 127 / 234
#define MR_TGT_Y     8                                    // target temp (Marlin: y=8)
#define MR_ICON_Y    18                                   // icon 64x64 (Marlin: add_image(0,18,..))
#define MR_ICON_SZ   64
#define MR_FAN_ICON_Y 10                                  // icon quat (Marlin: add_image(0,10,..))
#define MR_CUR_Y     72                                   // nhiet do hien tai (Marlin: y=72)
#define MR_RATE_ICON_Y 0                                  // icon FR/FL 32x32 (Marlin: add_image(0,0,..))
#define MR_XYZ_X     4                                    // canvas(4, 103, 312, 24)
#define MR_XYZ_Y     103
#define MR_XYZ_W     312
#define MR_XYZ_H     24
#define MR_XYZ_TY    3
#define MR_XYZ_LX(i) ((lv_coord_t)((i) == 0 ? 10 : ((i) == 1 ? 127 : 219)))
#define MR_XYZ_VW(i) ((lv_coord_t)((i) == 0 ? 58 : ((i) == 1 ? 58 : 82)))   // can phai -> 68 / 185 / 301
#define MR_RATE_Y    136                                  // canvas(70/170, 136, 80, 32)
#define MR_RATE_W    80
#define MR_RATE_H    32
#define MR_FR_X      70
#define MR_FL_X      170
#define MR_TILE_Y    130                                  // nut SD (0,130) va SETTINGS (256,130), 64x64
#define MR_TILE_SZ   64
#define MR_SD_X      0
#define MR_SET_X     256
#define MR_TIME_X    96                                   // canvas(96, 176, 128, 20)
#define MR_TIME_Y    176
#define MR_TIME_W    128
#define MR_PCT_X     226                                  // % tien do (cho trong 224..256)
#define MR_PCT_W     30
#define MR_BAR_X     4                                    // canvas(4, 198, 312, 9)
#define MR_BAR_Y     198
#define MR_BAR_W     312
#define MR_BAR_H     9
#define MR_MSG_X     0                                    // canvas(0, 216, 320, 20)
#define MR_MSG_Y     216
#define MR_MSG_W     320

// --- B) cac man con ---
#define BAR_H      26                                  // dong tieu de: [ten man] ... [LINK][BACK]
#define BOT_H      22                                  // thanh trang thai duoi: [PREV] [TRANG x/y] [NEXT]
#define BOT_Y      (UI_LCD_VER_RES - BOT_H)            // 218
#define CONT_Y     BAR_H
#define CONT_H     (UI_LCD_VER_RES - BAR_H)             // 214 (192 khi phai nhuong cho cho thanh day)
#define PAGE_PAD   4                                    // padding trong vung noi dung
#define PAGE_INN_W (UI_LCD_HOR_RES - 2 * PAGE_PAD)      // 312: be rong dung duoc
#define PAGE_INN_H (CONT_H - 2 * PAGE_PAD)              // 206: TONG chieu cao moi man phai <= so nay
#define ROW_H      16                                   // 1 dong chu montserrat_14
#define GAP        2                                    // khoang cach giua cac hang / doi tuong

// --- C) man MENU (menu doc kieu Marlin; cac dong do FLEX cua mk_page tu xep doc) ---
#define MENU_ROW_H  22                                  // 20 px mo dong + 2 px khoang

static lv_obj_t  *s_page[PG_COUNT];        // container noi dung tung man (NULL = chua tao)
static lv_obj_t  *s_bar;                   // dong tieu de chung: [ten man] ... [LINK][BACK] (an o STATUS)
static lv_obj_t  *s_title, *s_link;
// Thanh day (sticky nhu dong tieu de): [PREV] [TRANG x/y] [NEXT]. CHI hien khi noi dung
// cua man dang mo BI TRAN theo chieu doc (xem bot_sync()); man nao gon thi giu nguyen bo cuc 206 px.
static lv_obj_t  *s_bot, *s_bot_prev, *s_bot_next, *s_bot_info;
static bool       s_bot_shown = false;      // trang thai hien tai cua s_bot (tranh goi an/hien vo ich)
// Trang thai hien/an cua 2 nut PREV/NEXT: bit0 = PREV, bit1 = NEXT. 0xFF = chua set lan nao.
// Nut nao KHONG the di theo huong do thi AN HAN (khong lam mo), va chi doi CO khi trang thai doi.
static uint8_t    s_bot_nav = 0xFF;
static lv_obj_t  *s_btn_pause, *s_btn_stop; // P9: 2 nut nho chi hien khi dang in tu the SD
static lv_group_t *s_group;
static lv_obj_t  *s_focus_prev;            // doi tuong dang focus (de ve khung)
static uint8_t    s_page_act = PG_STATUS;
static uint8_t    s_link_state = 255;      // 255 = chua set lan nao (`link_state_now()` tra 0/1/2)
static uint32_t   s_tick_ms, s_upd_ms, s_slow_ms;

// "Co link voi Marlin" - CO HAI giao thuc LOAI TRU nhau (PUSH: cydlcd.linked / TRAM: tbt.linked)
// nen phai xet CA HAI. Dung ham nay o MOI cho hien thi link (thanh tren + trang STATUS).
// Chinh sach hien thi (2026-10-13, theo yeu cau): chi bao link chi nam o
//   1) THANH TREN (moi man), va 2) dong trang thai + khung XYZ cua trang STATUS (home).
// Cac man khac KHONG hien lai nua (truoc day man TRAMMING va MENU cung hien -> nhieu cho mau thuan).
static bool link_ok(void) { return cydlcd.linked || tbt.linked; }

// Trang thai link 3 muc - "co byte toi" KHONG du de coi la link tot (co the la byte rac do sai baud/
// nhieu/day loi), nhung no phan biet duoc 2 kieu hong khac nhau:
//   0 = NO LINK  : khong co byte nao  -> dut day / sai cong / board tat
//   1 = LINK OK  : doc duoc dong "TBT ..." hop le
//   2 = LINK ERR : CO byte toi nhung khong thanh dong hop le -> sai baud / sai khung / nhieu
static uint8_t link_state_now(void) {
  if (link_ok()) return 1;
  return tbt.rx_active ? 2 : 0;
}

// ===========================================================================
//  Format so/chuoi (khong dung %f)
// ===========================================================================
// In `raw` (da nhan `scale`) thanh chuoi thap phan `dec` chu so sau dau cham.
//   vi du fmt_scaled(b, n, 21050, 100, 1) -> "210.5"
//           fmt_scaled(b, n, -120,  1000, 2) -> "-0.12"
static void fmt_scaled(char *b, size_t n, int32_t raw, uint32_t scale, uint8_t dec) {
  if (!scale) scale = 1;
  if (dec > 3) dec = 3;
  const bool neg = (raw < 0);
  const uint32_t v = neg ? (uint32_t)(-(raw + 1)) + 1U : (uint32_t)raw;   // tranh tran INT32_MIN
  uint32_t ip = v / scale;
  const uint32_t rem = v % scale;

  uint32_t mul = 1;
  for (uint8_t i = 0; i < dec; i++) mul *= 10U;
  uint32_t frac = (rem * mul + scale / 2U) / scale;                       // co lam tron
  if (frac >= mul) { frac -= mul; ip++; }

  switch (dec) {
    case 0:  snprintf(b, n, "%s%lu", neg ? "-" : "", (unsigned long)ip); break;
    case 1:  snprintf(b, n, "%s%lu.%01lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
    case 2:  snprintf(b, n, "%s%lu.%02lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
    default: snprintf(b, n, "%s%lu.%03lu", neg ? "-" : "", (unsigned long)ip, (unsigned long)frac); break;
  }
}

// 12:34 hoac 1:02:03
static void fmt_time(char *b, size_t n, uint32_t s) {
  if (s >= 3600U)
    snprintf(b, n, "%lu:%02lu:%02lu", (unsigned long)(s / 3600U),
             (unsigned long)((s / 60U) % 60U), (unsigned long)(s % 60U));
  else
    snprintf(b, n, "%02lu:%02lu", (unsigned long)(s / 60U), (unsigned long)(s % 60U));
}

static const char *state_name(uint8_t st) {
  switch (st) {
    case CYDLCD_ST_IDLE:     return "IDLE";
    case CYDLCD_ST_PRINTING: return "PRINTING";
    case CYDLCD_ST_PAUSED:   return "PAUSED";
    case CYDLCD_ST_HOMING:   return "HOMING";
    case CYDLCD_ST_LEVELING: return "LEVELING";
    case CYDLCD_ST_RUNOUT:   return "RUNOUT";
    case CYDLCD_ST_KILLED:   return "KILLED";
    case CYDLCD_ST_BUSY:     return "BUSY";
    default:                return "?";
  }
}

static void temp_lookup(const uint8_t id, int16_t *cur, int16_t *tgt) {
  *cur = 0; *tgt = 0;
  // Che do TRAM (CYDLCD_TBT_LINK): KHONG co frame PUSH nang -> nhiet do duoc gui kem trong dong
  // "TBT ..." (4 token cuoi, 2026-10-13, xem tbt_protocol.h). cydlcd.temps[] se rong nen phai
  // lay tu day, neu khong moi o nhiet deu hien 0.
  if (tbt.ever) {
    if (id == CYDLCD_TEMP_E0)  { *cur = tbt.st.t_e0;  *tgt = tbt.st.t_e0_tgt;  return; }
    if (id == CYDLCD_TEMP_BED) { *cur = tbt.st.t_bed; *tgt = tbt.st.t_bed_tgt; return; }
  }
  for (uint8_t i = 0; i < cydlcd.n_temps; i++)
    if (cydlcd.temps[i].id == id) { *cur = cydlcd.temps[i].cur; *tgt = cydlcd.temps[i].tgt; return; }
}

// ===========================================================================
//  Helper tao widget
// ===========================================================================
// Chi set text khi khac noi dung cu -> khong nhap nhay, khong ve lai vo ich
static void set_text(lv_obj_t *lbl, const char *txt) {
  if (!lbl || !txt) return;
  const char *old = lv_label_get_text(lbl);
  if (old && strcmp(old, txt) == 0) return;
  lv_label_set_text(lbl, txt);
}

// Gan font CO DAU tieng Viet cho DUNG 1 nhan (khong bao gio gan cho container/nut/page:
// font thay the co line_height 17 > Montserrat 16 nen gan tran ra ca cay se lam chu bi cat
// trong cac hang/nut cao co dinh -> nut "khong co text").
// Che do 4: font mono GFX tu sinh CO du dau tieng Viet (U+1EA0..U+1EF9) -> gan cho nhan co dau.
// Che do 0: KHONG con font nao co dau (da xoa het font sinh bang lv_font_conv) -> no-op; hau qua:
//           3 nhan co dau o lop phu SUA se ra O VUONG. Muon co dau thi dung che do 4.
static void vn_font(lv_obj_t *label) {
#if UI_MONO_FONT == 4
  if (label) lv_obj_set_style_text_font(label, UI_MONO_FACE, 0);   // mono GFX: co dau tieng Viet
#else
  (void)label;                                    // che do 0: khong co font nao co dau
#endif
}

// Gan font co dau cho nhan NAM TRONG 1 nut (mk_btn tao dung 1 label con)
static void vn_font_btn(lv_obj_t *btn) {
  if (btn) vn_font(lv_obj_get_child(btn, 0));
}

static void style_flat(lv_obj_t *o, lv_color_t bg) {
  lv_obj_set_style_bg_color(o, bg, 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

// Nhan 1 dong: rong theo noi dung (dat trong flex thi layout tu xep cho)
static lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, lv_color_t col) {
  lv_obj_t *l = lv_label_create(parent);
  mono_obj(l);                                   // che do 2: mono cho TUNG nhan
  lv_obj_set_style_text_color(l, col, 0);
  lv_label_set_text(l, txt);
  return l;
}

// Nhan 1 dong, be rong CO DINH: chu dai hon thi tu cuon vong (khong bi cat cut)
static lv_obj_t *mk_label_w(lv_obj_t *parent, const char *txt, lv_color_t col, lv_coord_t w) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_width(l, w);
  lv_label_set_long_mode(l, LV_LABEL_LONG_SCROLL_CIRCULAR);
  return l;
}

// Container trong suot de xep con bang FLEX.
//   flow = LV_FLEX_FLOW_COLUMN (xep doc) hoac LV_FLEX_FLOW_ROW (xep ngang)
//   pad  = padding trong; gap = khoang cach giua cac con (ca 2 chieu)
// Hang ROW can giua theo chieu doc (nut thap hon hang van nam giua).
static lv_obj_t *mk_flex(lv_obj_t *parent, lv_flex_flow_t flow, lv_coord_t w, lv_coord_t h,
                         lv_coord_t pad, lv_coord_t gap) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, pad, 0);
  lv_obj_set_style_pad_row(o, gap, 0);
  lv_obj_set_style_pad_column(o, gap, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  // KHONG cho nhan cham: lv_obj mac dinh CLICKABLE, ma day chi la khung trong suot de xep con ->
  // de no "nuot" cham thi cham vao vung trong cua trang se khong roi xuong duoc TRANG (xem
  // cb_page_bg_click: cham vung trong -> ve man STATUS).
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_flex_flow(o, flow);
  // Hang ROW: can giua theo chieu doc (nut thap hon hang van nam giua hang).
  lv_obj_set_flex_align(o, LV_FLEX_ALIGN_START,
                        (flow == LV_FLEX_FLOW_ROW) ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START,
                        (flow == LV_FLEX_FLOW_ROW) ? LV_FLEX_ALIGN_CENTER : LV_FLEX_ALIGN_START);
  return o;
}

// Vat dem trong suot: grow > 0 thi chiem het cho trong con lai cua hang flex
// (dung de day cac vat khac ve mot phia).
static lv_obj_t *mk_spacer(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, uint8_t grow) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_radius(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);   // vat dem trong suot: cham phai roi xuong trang
  if (grow) lv_obj_set_flex_grow(o, grow);
  return o;
}

static void focus_mark(lv_obj_t *o, const bool on) {
  if (!o) return;
  // Vat co LV_OBJ_FLAG_USER_1 = vat TU QUAN LY NEN (xem jg_axis[] o man JOG): nen cua no dang
  // dung de danh dau "DANG CHON" (COL_ACCENT). Focus cua nhung vat nay CHI duoc ve VIEN.
  // LY DO (loi nguoi dung bao): focus va "dang chon" la 2 trang thai KHAC NHAU, KHONG duoc ghi
  // de nhau. Truoc day focus_mark() doi CA NEN (COL_ACCENT <-> COL_BTN); khi focus roi khoi nut
  // truc dang chon (sang nut buoc / nut di chuyen) no tra nen nut truc ve COL_BTN -> mat dau
  // "truc dang chon" du s_axis van dung. Nay: "chon" = NEN, "focus" = VIEN.
  const bool own_bg = lv_obj_has_flag(o, LV_OBJ_FLAG_USER_1);
  lv_obj_set_style_border_width(o, on ? 2 : 0, 0);
  // Nut truc khi focus co the dang to nen COL_ACCENT -> vien COL_ACCENT se chim vao nen,
  // nen dung mau sang (MR_FRAME) de van nhin thay focus.
  lv_obj_set_style_border_color(o, own_bg ? MR_FRAME : COL_ACCENT, 0);
  // Vat co nen dac (nut, dong menu, bang) -> doi sang COLOR_SELECTION_BG nhu dong dang chon cua Marlin
  if (lv_obj_get_style_bg_opa(o, 0) == LV_OPA_COVER && !own_bg)
    lv_obj_set_style_bg_color(o, on ? MR_SELBG : COL_BTN, 0);
}

// Ve khung cho vat dang focus (encoder hoac cham) -> nguoi dung biet dang o dau
static void on_focus_changed(lv_group_t *g) {
  lv_obj_t *f = lv_group_get_focused(g);
  if (s_focus_prev && s_focus_prev != f) focus_mark(s_focus_prev, false);
  if (f) focus_mark(f, true);
  s_focus_prev = f;
}

// Nut bam: co dinh kich thuoc, xoa SCROLLABLE (de encoder bam 1 lan la chay),
// tu dong vao group va tu canh chu vao giua.
static lv_obj_t *mk_btn(lv_obj_t *parent, const char *txt, lv_coord_t w, lv_coord_t h,
                        lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_set_style_bg_color(b, COL_BTN, 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_t *l = lv_label_create(b);
  mono_obj(l);                                   // che do 2: mono cho nhan trong nut
  lv_obj_set_style_text_color(l, COL_TXT, 0);
  lv_label_set_text(l, txt);
  lv_obj_center(l);
  if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  if (s_group) lv_group_add_obj(s_group, b);
  return b;
}

// Nut chia deu be rong trong 1 hang flex (khong can biet truoc be rong tung nut)
static lv_obj_t *mk_btn_grow(lv_obj_t *parent, const char *txt, lv_coord_t h,
                             lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = mk_btn(parent, txt, 0, h, cb, ud);
  lv_obj_set_flex_grow(b, 1);
  return b;
}

// Vung BAM trong suot co toa do tuyet doi (man STATUS/MENU khong dung flex).
// cb == NULL: khong vao group (dung cho "bang icon" nam trong 1 vung bam khac).
static lv_obj_t *mk_hit(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                        lv_event_cb_t cb, void *ud) {
  lv_obj_t *b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_set_style_radius(b, 3, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  if (cb) {
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
    if (s_group) lv_group_add_obj(s_group, b);
  }
  else lv_obj_clear_flag(b, LV_OBJ_FLAG_CLICKABLE);     // de click roi xuong vung bam cha
  return b;
}

// Icon cua Marlin (LVGL ALPHA_4BIT): mau ve bang recolor -> doi theo trang thai
static lv_obj_t *mk_icon(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const lv_img_dsc_t *src,
                         lv_color_t col) {
  lv_obj_t *im = lv_img_create(parent);
  lv_img_set_src(im, src);
  lv_obj_set_pos(im, x, y);
  lv_obj_clear_flag(im, LV_OBJ_FLAG_CLICKABLE);       // click roi xuong vung bam cha
  lv_obj_clear_flag(im, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_img_recolor(im, col, 0);
  lv_obj_set_style_img_recolor_opa(im, LV_OPA_COVER, 0);
  return im;
}

// Doi mau icon (Marlin doi mau bitmap theo nhiet do / trang thai)
static void icon_color(lv_obj_t *im, const lv_color_t col) {
  if (!im) return;
  lv_obj_set_style_img_recolor(im, col, 0);
  lv_obj_set_style_img_recolor_opa(im, LV_OPA_COVER, 0);
}

// Nhan 1 dong, can phai trong be rong w (Marlin can so theo toa do x phai)
static lv_obj_t *mk_label_right(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                                const char *txt, lv_color_t col) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_RIGHT, 0);
  return l;
}

// Nhan 1 dong can giua trong be rong w
static lv_obj_t *mk_label_mid(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                              const char *txt, lv_color_t col) {
  lv_obj_t *l = mk_label(parent, txt, col);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_width(l, w);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  return l;
}

// Man kin 320x240, KHONG flex (con dat bang toa do tuyet doi) + khong padding.
// Nen dung COLOR_BACKGROUND cua Marlin (2 man nay theo bo cuc Marlin).
static lv_obj_t *mk_page_full(void) {
  lv_obj_t *p = lv_obj_create(lv_scr_act());
  lv_obj_set_pos(p, 0, 0);
  lv_obj_set_size(p, UI_LCD_HOR_RES, UI_LCD_VER_RES);
  lv_obj_set_style_bg_color(p, MR_BG, 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_radius(p, 0, 0);
  lv_obj_set_style_pad_all(p, 0, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  return p;
}

// Focus vao vat bam duoc dau tien trong 1 nhanh (de encoder bat dau o vat dau man moi)
static bool focus_first_in(lv_obj_t *o) {
  const uint32_t n = lv_obj_get_child_cnt(o);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *c = lv_obj_get_child(o, i);
    if (lv_obj_has_flag(c, LV_OBJ_FLAG_CLICK_FOCUSABLE) && !lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) {
      lv_group_focus_obj(c);
      return true;
    }
    if (focus_first_in(c)) return true;
  }
  return false;
}

// Cham vao VUNG TRONG cua 1 trang CON -> ve thang man STATUS (yeu cau nguoi dung).
// Co che LVGL: vat KHONG CLICKABLE (nhan - lv_label tu clear co; khung trong suot - da clear trong
// mk_flex/mk_spacer; mk_icon/mk_hit(cb=NULL)) bi BO QUA khi tim vat nhan cham -> su kien roi xuong
// vat CHA, tuc la roi vao handler nay. Vat CO CLICKABLE (nut, roller, bang, mk_hit co cb) duoc
// tim thay TRUOC nen van hoat dong binh thuong. Su kien CLICKED KHONG tu bubble len cha
// (lv_event.c: event_is_bubbled chi cho CHILD_CREATED/DELETED hoac khi bat LV_OBJ_FLAG_EVENT_BUBBLE)
// -> bam nut KHONG lam chay handler nay, khong bi "tuot" khoi trang.
// DANH DOI (da bao truoc): cham HUT ra vung trong la THOAT khoi trang dang mo. Muon quay lai nhu cu:
// bo DUY NHAT 1 dong `lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);` trong mk_page() duoi day.
static void show_page(uint8_t p);              // forward (khoi forward chinh nam duoi do_action)
static void cb_page_bg_click(lv_event_t *e) {
  (void)e;
  show_page(PG_STATUS);
}

// Container noi dung cua 1 man: flex COLUMN, padding PAGE_PAD, cach nhau GAP.
// Con nao muon an het cho trong con lai thi them lv_obj_set_flex_grow(con, 1).
// CHI dung cho cac trang CON; man STATUS dung mk_page_full() nen khong dinh "cham vung trong".
static lv_obj_t *mk_page(lv_obj_t *parent) {
  lv_obj_t *p = lv_obj_create(parent);
  lv_obj_set_pos(p, 0, CONT_Y);
  lv_obj_set_size(p, UI_LCD_HOR_RES, CONT_H);
  lv_obj_set_style_bg_opa(p, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(p, 0, 0);
  lv_obj_set_style_radius(p, 0, 0);
  lv_obj_set_style_pad_all(p, PAGE_PAD, 0);
  lv_obj_set_style_pad_row(p, GAP, 0);
  lv_obj_set_style_pad_column(p, GAP, 0);
  // Man con duoc phep CUON DOC: noi dung dai hon vung nhin thi thanh day (xem bot_sync)
  // hien ra de phan trang bang PREV/NEXT; khong ve thanh cuon cho do roi man hinh.
  lv_obj_add_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scroll_dir(p, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(p, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(p, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  // CHAM VAO VUNG TRONG -> ve thang man STATUS (xem cb_page_bg_click ngay tren).
  // Muon tat hanh vi nay: bo DUY NHAT dong add_flag duoi day (handler cu the giu lai, vo hai).
  lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(p, cb_page_bg_click, LV_EVENT_CLICKED, NULL);
  return p;
}

// ===========================================================================
//  Hanh dong (G-code / request)
// ===========================================================================
enum {
  A_NONE = 0,
  A_BACK, A_PAUSE, A_RESUME, A_STOP, A_SAVE, A_INITSD, A_G34, A_HOME,
  A_REQ_FILES, A_REQ_UP, A_REQ_MESH, A_REQ_SETTINGS, A_COOLDOWN,
  A_GOTO_MENU, A_GOTO_TEMP, A_GOTO_JOG, A_GOTO_FILES, A_GOTO_SETTINGS, A_GOTO_RATE,
  A_GOTO_STATUS,        // nut HOME o header: ve THANG man STATUS (khac A_BACK = lui 1 cap)
  A_GOTO_WIFI,          // icon WiFi (header/STATUS) hoac muc "WiFi" trong MENU -> trang WIFI
  A_WIFI_FORGET,        // XOA CAU HINH WiFi (da xac nhan 2 buoc) -> net_wifi_disconnect(true)
  // P1 - UBL bed leveling (G29)
  A_G29_P1, A_G29_FILL, A_G29_FT, A_G29_ON, A_G29_OFF, A_G29_SAVE,
  // P2/P7 - JOG (tat dong co, home tung truc, sua probe Z bang lop phu)
  A_STEPPERS_OFF, A_HOME_X, A_HOME_Y, A_HOME_Z, A_EDIT_PROBE_Z,
  // P3/P8/P9 - EEPROM + xac nhan + DFU (A_STOP_NOW la buoc da xac nhan cua A_STOP)
  A_EEPROM_RESET, A_STOP_NOW, A_REBOOT_DFU,
  // P4/P6 - endstop M119 + filament (M600/M412)
  A_GOTO_ENDSTOP, A_REQ_ENDSTOPS, A_FILAMENT_CHANGE, A_RUNOUT_TOGGLE,
  // P5 - tuning tren man TEMP
  A_MPC_TUNE, A_PID_BED,
  // Man FILE: xoa file SD dang chon (chi chay SAU khi xac nhan 2 buoc - xem do_confirmed)
  A_FILES_DELETE,
  // Thanh day: phan trang noi dung man dang mo (xem page_scroll/bot_sync)
  A_PAGE_PREV, A_PAGE_NEXT
};

// user_data cua nut: (heater << 8) | (int8)delta
#define TEMP_ADJ(h, d) ((void *)(intptr_t)(((unsigned)(h) << 8) | (unsigned)(uint8_t)(int8_t)(d)))

static void show_page(uint8_t p);            // forward
static void create_page(uint8_t p);          // forward
static void page_scroll(const int8_t dir);   // forward (phan trang noi dung, dinh nghia duoi MENU)
static void bot_sync(const bool remeasure);   // forward (dong bo thanh day, dinh nghia duoi MENU)
static void files_rebuild(void);             // forward
static void files_delete_now(void);          // forward (nut XOA FILE, dinh nghia o man FILES)
static void status_icons_create(lv_obj_t *p, lv_obj_t *sd_icon);  // forward (icon WiFi + mau icon the)
static void refocus_page(void);              // forward (dinh nghia o cuoi file)
static void do_action(const int a);          // forward
static void ui_confirm(const char *title, const char *txt, uint8_t action, uint8_t steps);  // forward
static void ov_open_id(const uint16_t id);   // forward (lop phu sua so, dinh nghia o man SETTINGS)

static void cb_action(lv_event_t *e) {
  do_action((int)(intptr_t)lv_event_get_user_data(e));
}

static void do_action(const int a) {
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
static void do_confirmed(const uint8_t act) {
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
//  STATUS -- TOA DO Y NGUYEN tu Marlin/src/lcd/tft/ui_320x240.cpp
//   3 o nhiet/quat 64x100 tai x = 21/127/234 + hang XYZ + FR/FL + SD + SETTINGS
//   + thoi gian (96,176) + thanh tien do (4,198) + dong trang thai (0,216)
//   Icon 64x64 cua Marlin (bitmap GREYSCALE4) da port that vao ui_lvgl_icons.h.
// ===========================================================================
#define ST_CELLS 3                     // 0 = dau in (E0), 1 = ban in (BED), 2 = quat (FAN)

static lv_obj_t *st_cell[ST_CELLS], *st_icon[ST_CELLS];
static lv_obj_t *st_tgt[ST_CELLS], *st_cur[ST_CELLS];
static lv_obj_t *st_axis_val[3], *st_xyz_frame;
static lv_obj_t *st_rate_val[2], *st_rate_icon[2];
static lv_obj_t *st_time, *st_pct, *st_bar, *st_msg;

// 1 o 64x100: [target] / [icon 64x64] / [gia tri]; ca o la 1 nut (bam -> man tuong ung)
// Thu tu tao: nut -> ICON -> 2 nhan, de nhan ve DE LEN icon (giong Marlin: text ve sau image).
static void status_cell(lv_obj_t *p, const uint8_t i, const lv_img_dsc_t *icon, const uint8_t act) {
  lv_obj_t *c = mk_hit(p, MR_CELL_X(i), 0, MR_CELL_W, MR_CELL_H,
                       cb_action, (void *)(intptr_t)act);
  st_cell[i] = c;
  st_icon[i] = mk_icon(c, 0, MR_ICON_Y, icon, MR_COLD);
  st_tgt[i]  = mk_label_mid(c, 0, MR_TGT_Y, MR_CELL_W, "-", MR_COLD);
  st_cur[i]  = mk_label_mid(c, 0, MR_CUR_Y, MR_CELL_W, "-", MR_COLD);
}

static void page_status_create(lv_obj_t *p) {
  // ---- 3 o: dau in (E0) / ban in (BED) / quat (FAN) ----
  status_cell(p, 0, &ui_icon_hotend, A_GOTO_TEMP);
  status_cell(p, 1, &ui_icon_bed,    A_GOTO_TEMP);
  status_cell(p, 2, &ui_icon_fan,    A_GOTO_TEMP);
  lv_obj_set_pos(st_icon[2], 0, MR_FAN_ICON_Y);        // Marlin ve icon quat o y=10

  // ---- hang XYZ: khung (4, 103, 312, 24) ----
  // Khung ve rieng (de doi mau theo LINK) + vung bam trong suot dat de len tren
  st_xyz_frame = lv_obj_create(p);
  lv_obj_set_pos(st_xyz_frame, MR_XYZ_X, MR_XYZ_Y);
  lv_obj_set_size(st_xyz_frame, MR_XYZ_W, MR_XYZ_H);
  lv_obj_clear_flag(st_xyz_frame, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(st_xyz_frame, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_opa(st_xyz_frame, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(st_xyz_frame, 1, 0);
  lv_obj_set_style_border_color(st_xyz_frame, MR_AXIS, 0);
  lv_obj_set_style_radius(st_xyz_frame, 0, 0);
  lv_obj_set_style_pad_all(st_xyz_frame, 0, 0);

  lv_obj_t *xyz_hit = mk_hit(p, MR_XYZ_X, MR_XYZ_Y, MR_XYZ_W, MR_XYZ_H,
                             cb_action, (void *)(intptr_t)A_GOTO_JOG);
  static const char *const AX_L[3] = {"X", "Y", "Z"};
  for (uint8_t i = 0; i < 3; i++) {
    lv_obj_t *n = mk_label(xyz_hit, AX_L[i], MR_AXIS);
    lv_obj_set_pos(n, MR_XYZ_LX(i), MR_XYZ_TY);
    st_axis_val[i] = mk_label_right(xyz_hit, MR_XYZ_LX(i), MR_XYZ_TY, MR_XYZ_VW(i),
                                    "-", MR_AXIS);
  }

  // ---- FR (70,136,80,32) va FL (170,136,80,32): icon 32x32 + gia tri o x=32,y=6 ----
  static const lv_img_dsc_t *const RATE_ICON[2] = { &ui_icon_feedrate, &ui_icon_flowrate };
  const lv_coord_t rate_x[2] = {MR_FR_X, MR_FL_X};
  for (uint8_t i = 0; i < 2; i++) {
    lv_obj_t *b = mk_hit(p, rate_x[i], MR_RATE_Y, MR_RATE_W, MR_RATE_H,
                         cb_action, (void *)(intptr_t)A_GOTO_RATE);
    st_rate_icon[i] = mk_icon(b, 0, MR_RATE_ICON_Y, RATE_ICON[i], MR_RATE100);
    st_rate_val[i]  = mk_label(b, "100%", MR_RATE100);
    lv_obj_set_pos(st_rate_val[i], 32, 6);
  }

  // ---- nut SD (0,130) va nut SETTINGS (256,130): icon 64x64 nhu Marlin ----
  lv_obj_t *sd = mk_hit(p, MR_SD_X, MR_TILE_Y, MR_TILE_SZ, MR_TILE_SZ,
                        cb_action, (void *)(intptr_t)A_GOTO_FILES);
  // Icon THE co SAN cua Marlin: giu nguyen bitmap 64x64, chi DOI MAU theo trang thai the TF cua
  // MAN HINH CYD (xem status_sd_icon_mark()). Truoc day mau co dinh MR_TIME (khong cho nao khac doi).
  lv_obj_t *sd_icon = mk_icon(sd, 0, 0, &ui_icon_sd, MR_TIME);

  lv_obj_t *set = mk_hit(p, MR_SET_X, MR_TILE_Y, MR_TILE_SZ, MR_TILE_SZ,
                         cb_action, (void *)(intptr_t)A_GOTO_MENU);
  mk_icon(set, 0, 0, &ui_icon_settings, MR_FRAME);

  // ---- thoi gian in (96,176,128,20) + % (cho trong 224..256) ----
  st_time = mk_label_mid(p, MR_TIME_X, MR_TIME_Y, MR_TIME_W, "-", MR_TIME);
  st_pct  = mk_label(p, "0%", MR_TIME);
  lv_obj_set_pos(st_pct, MR_PCT_X, MR_TIME_Y);

  // ---- thanh tien do (4, 198, 312, 9): khung trang + ruot xanh (nhu Marlin) ----
  st_bar = lv_bar_create(p);
  lv_obj_set_pos(st_bar, MR_BAR_X, MR_BAR_Y);
  lv_obj_set_size(st_bar, MR_BAR_W, MR_BAR_H);
  lv_bar_set_range(st_bar, 0, 100);
  lv_bar_set_value(st_bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(st_bar, MR_PROGBG, 0);
  lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(st_bar, 1, 0);
  lv_obj_set_style_border_color(st_bar, MR_FRAME, 0);
  lv_obj_set_style_radius(st_bar, 0, 0);
  lv_obj_set_style_pad_all(st_bar, 1, 0);
  lv_obj_set_style_bg_color(st_bar, MR_PROGBAR, LV_PART_INDICATOR);
  lv_obj_set_style_bg_opa(st_bar, LV_OPA_COVER, LV_PART_INDICATOR);
  lv_obj_set_style_radius(st_bar, 0, LV_PART_INDICATOR);

  // ---- dong trang thai / ten file (0, 216, 320, 20) ----
  st_msg = mk_label_w(p, "-", MR_MSG, MR_MSG_W);
  lv_obj_set_pos(st_msg, MR_MSG_X, MR_MSG_Y);
  lv_obj_set_style_text_align(st_msg, LV_TEXT_ALIGN_CENTER, 0);

  // ---- 2 icon trang thai: WiFi (giua o the va khoi FR/FL) + luu lai icon THE co san cua Marlin
  // de to mau theo trang thai the TF cua MAN HINH CYD (xem khoi "Icon trang thai WiFi" o cuoi file).
  // Tao CUOI CUNG de ve de len tren; la con cua trang STATUS nen tu an/hien theo man.
  status_icons_create(p, sd_icon);
}

static void page_status_update(void) {
  char b[48], a1[12], a2[12], t1[10], t2[10];
  int16_t cur = 0, tgt = 0;
  const bool homed = (cydlcd.flags & CYDLCD_SF_HOMED) != 0;

  // ---- 3 o: mau + icon theo nhiet do (Marlin: >= 50 do C moi doi mau/icon) ----
  for (uint8_t i = 0; i < ST_CELLS; i++) {
    lv_color_t col = MR_COLD;
    if (i == 2) {                                     // quat: icon theo toc do, chi 1 dong "%"
      uint8_t fp = 0;
      if (cydlcd.n_fans) fp = (uint8_t)(cydlcd.fans[0].pct / 100);
      snprintf(b, sizeof(b), "%u%%", (unsigned)fp);
      set_text(st_cur[i], b);
      set_text(st_tgt[i], "");
      lv_img_set_src(st_icon[i], (fp >= 50) ? &ui_icon_fan_fast : (fp ? &ui_icon_fan_slow : &ui_icon_fan));
    }
    else {                                            // dau in / ban in: so nguyen do C
      temp_lookup(i == 0 ? CYDLCD_TEMP_E0 : CYDLCD_TEMP_BED, &cur, &tgt);
      if (cur >= 50) col = (i == 0) ? MR_HOTEND : MR_BEDHOT;
      if (i == 1) lv_img_set_src(st_icon[i], (tgt > 0) ? &ui_icon_bed_heated : &ui_icon_bed);
      fmt_scaled(a1, sizeof(a1), cur, 100, 0);
      fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
      set_text(st_cur[i], a1);
      set_text(st_tgt[i], a2);
    }
    icon_color(st_icon[i], col);
    lv_obj_set_style_text_color(st_cur[i], col, 0);
    lv_obj_set_style_text_color(st_tgt[i], col, 0);
  }

  // ---- XYZ: chua home -> "?" (Marlin nhay "?" mau AXIS_NOT_HOMED) ----
  const int32_t posv[3] = { cydlcd.pos.x, cydlcd.pos.y, cydlcd.pos.z };
  for (uint8_t i = 0; i < 3; i++) {
    if (!homed) {
      set_text(st_axis_val[i], "?");
      lv_obj_set_style_text_color(st_axis_val[i], MR_NOAXIS, 0);
    }
    else {
      fmt_scaled(a1, sizeof(a1), posv[i], 1000, 2);
      set_text(st_axis_val[i], a1);
      lv_obj_set_style_text_color(st_axis_val[i], MR_AXIS, 0);
    }
  }
  lv_obj_set_style_border_color(st_xyz_frame, link_ok() ? MR_AXIS : COL_ERR, 0);

  // ---- FR / FL: xanh la khi 100%, vang khi khac (Marlin) ----
  const uint8_t rate[2] = { (uint8_t)(cydlcd.pos.feed_pct / 100), (uint8_t)(cydlcd.pos.flow_pct / 100) };
  for (uint8_t i = 0; i < 2; i++) {
    const lv_color_t col = (rate[i] == 100) ? MR_RATE100 : MR_RATEALT;
    snprintf(b, sizeof(b), "%u%%", (unsigned)rate[i]);
    set_text(st_rate_val[i], b);
    lv_obj_set_style_text_color(st_rate_val[i], col, 0);
    icon_color(st_rate_icon[i], col);
  }

  // ---- thoi gian: da in / con lai ----
  fmt_time(t1, sizeof(t1), cydlcd.prog.elapsed_s);
  fmt_time(t2, sizeof(t2), cydlcd.prog.remaining_s);
  if (cydlcd.prog.remaining_s) snprintf(b, sizeof(b), "%s/%s", t1, t2);
  else snprintf(b, sizeof(b), "%s", t1);
  set_text(st_time, b);

  // ---- % + thanh tien do ----
  const uint8_t pct = (cydlcd.progress_pct > 100) ? 100 : cydlcd.progress_pct;
  snprintf(b, sizeof(b), "%u%%", (unsigned)pct);
  set_text(st_pct, b);
  if (lv_bar_get_value(st_bar) != (int32_t)pct) lv_bar_set_value(st_bar, (int32_t)pct, LV_ANIM_OFF);

  // ---- dong trang thai: mat lien ket > "STATE  ten file" ----
  if (!link_ok()) {
    set_text(st_msg, tbt.rx_active ? "LINK ERR (byte khong hop le)" : "NO LINK");
    lv_obj_set_style_text_color(st_msg, COL_ERR, 0);
  }
  else {
    if (cydlcd.prog.file[0]) snprintf(b, sizeof(b), "%s  %s", state_name(cydlcd.state), cydlcd.prog.file);
    else snprintf(b, sizeof(b), "%s", state_name(cydlcd.state));
    set_text(st_msg, b);
    lv_obj_set_style_text_color(st_msg, MR_MSG, 0);
  }
}

// ===========================================================================
//  TEMP: +/- roi gui M104/M140 (gia tri nen lay tu target Marlin dang bao)
//  Moi nhom 1 hang: [ten] [-] [-] [gia tri] [+] [+]  -> tong 134 <= 142
// ===========================================================================
static void cb_temp_adj(lv_event_t *e) {
  const intptr_t v = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  heater = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const int8_t   delta  = (int8_t)(uint8_t)((unsigned)v & 0xFFU);
  char cmd[16];

  // heater 2 = QUAT: gui M106 S0..255 (nhu o FAN tren man STATUS cua Marlin)
  if (heater == 2) {
    int32_t pct = 0;
    if (cydlcd.n_fans) pct = (int32_t)(cydlcd.fans[0].pct / 100);
    pct += delta;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    snprintf(cmd, sizeof(cmd), "M106 S%ld", (long)((pct * 255 + 50) / 100));
    cydlcd_link_send_gcode(cmd);
    return;
  }

  int16_t cur = 0, tgt = 0;
  temp_lookup(heater ? CYDLCD_TEMP_BED : CYDLCD_TEMP_E0, &cur, &tgt);
  int32_t next = (int32_t)(tgt / 100) + (int32_t)delta;      // do C nguyen
  const int32_t hi = heater ? 120 : 300;
  if (next < 0) next = 0;
  if (next > hi) next = hi;

  snprintf(cmd, sizeof(cmd), heater ? "M140 S%ld" : "M104 S%ld", (long)next);
  cydlcd_link_send_gcode(cmd);
}

#define TP_ROW_H   30                                  // 1 hang nhom nhiet do
#define TP_LBL_W   60                                  // "NOZZLE" / "BED"
#define TP_BTN_BIG 32                                  // nut -10 / +10
#define TP_BTN_SML 28                                  // nut -1 / +1

// Nhan "cur / set" cua man TEMP (tao trong page_temp_create)
static lv_obj_t *tp_noz, *tp_bed, *tp_fan;

// 1 hang cho 1 heater: ten | -10 | -1 | gia tri (grow) | +1 | +10
static void temp_row(lv_obj_t *p, const char *name, uint8_t heater, lv_obj_t **val) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, TP_ROW_H, 0, GAP);
  mk_label_w(r, name, COL_TXT, TP_LBL_W);
  mk_btn(r, "-10", TP_BTN_BIG, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(heater, -10));
  mk_btn(r, "-1",  TP_BTN_SML, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(heater, -1));
  *val = mk_label_w(r, "--.- / -- C", COL_ACCENT, 100);
  lv_obj_set_flex_grow(*val, 1);                   // gia tri an het cho con lai (~118 px)
  mk_btn(r, "+1",  TP_BTN_SML, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(heater, +1));
  mk_btn(r, "+10", TP_BTN_BIG, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(heater, +10));
}

// Hang FAN: OFF | -10 | gia tri (grow) | +10 | MAX  -> M106 S0..255
static void fan_row(lv_obj_t *p) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, TP_ROW_H, 0, GAP);
  mk_label_w(r, "FAN", COL_TXT, TP_LBL_W);
  mk_btn(r, "OFF", TP_BTN_BIG, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(2, -100));
  mk_btn(r, "-10", TP_BTN_SML, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(2, -10));
  tp_fan = mk_label_w(r, "-%", MR_COLD, 100);
  lv_obj_set_flex_grow(tp_fan, 1);
  mk_btn(r, "+10", TP_BTN_SML, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(2, +10));
  mk_btn(r, "MAX", TP_BTN_BIG, TP_ROW_H - 2, cb_temp_adj, TEMP_ADJ(2, +100));
}

// ===========================================================================
//  PREHEAT presets (PLA/PETG/ABS) -- MAN HINH CYD TU LUU (NVS), khong phai Marlin (2026-10-14)
//
//  VI SAO KHONG DUNG M145 CUA MARLIN: ban nay KHONG co menu MarlinUI 12864 nen
//  `HAS_PREHEAT = 0` (Conditionals_post.h: `HAS_TEMPERATURE && ANY(HAS_MARLINUI_MENU, ...)`) =>
//    - `M145` KHONG duoc bien dich (gui lenh do se la "Unknown command"),
//    - bang preheat khong nam trong `settings_t` (khong co trong EEPROM).
//  Bat lai HAS_PREHEAT thi `settings_t` DOI KICH THUOC -> version/CRC EEPROM doi -> Marlin nap
//  MAC DINH => MAT M851 (Z offset), PID, M92/M201/M203/M205/M906 va MESH UBL dang luu. Vi vay preset
//  duoc giu trong NVS CUA MAN HINH CYD (namespace "ph") va van hien/sua duoc o trang SETTINGS nho "id cuc
//  bo" 0x40xx (Marlin chi dung id 1..~50 nen khong trung).
//
//  Nut PLA/PETG/ABS o man TEMP dung CHINH cac gia tri nay (khong con hardcode 200/230/240).
//  Duong ghi NVS chi co tren CYD (ESP32 co Preferences) - board khac: chi giu trong RAM.
// ===========================================================================
#define CYDLCD_PRE_COUNT   3
#define CYDLCD_PRE_ID_BASE 0x4000     // id = BASE + 1 .. BASE + 9 (H,B,F x 3 preset)

static const char *const PRE_LABEL[CYDLCD_PRE_COUNT] = { "PLA", "PETG", "ABS" };
// Mac dinh = dung Configuration.h cua may nay (PREHEAT_1/2/3: 200/60, 230/60, 240/60; FAN_SPEED 0)
static const uint16_t PRE_H_DEF[CYDLCD_PRE_COUNT] = { 200, 230, 240 };
static const uint16_t PRE_B_DEF[CYDLCD_PRE_COUNT] = {  60,  60,  60 };
static const uint8_t  PRE_F_DEF[CYDLCD_PRE_COUNT] = {   0,   0,   0 };

static uint16_t s_pre_h[CYDLCD_PRE_COUNT] = { 200, 230, 240 };
static uint16_t s_pre_b[CYDLCD_PRE_COUNT] = {  60,  60,  60 };
static uint8_t  s_pre_f[CYDLCD_PRE_COUNT] = {   0,   0,   0 };

// Gioi han khi sua (giong M145 cua Marlin: hotend [extrude_min..max], bed [BED_MINTEMP..MAX]).
#define PRE_H_MAX 300
#define PRE_B_MAX 120
#define PRE_F_MAX 255

#if defined(CYDLCD_BOARD_CYD)
  #include <Preferences.h>
  #define PRE_NVS_NS "ph"
#endif

static void preheat_load(void) {
#if defined(CYDLCD_BOARD_CYD)
  Preferences p;
  if (p.begin(PRE_NVS_NS, true)) {                   // read-only
    char k[4];
    for (uint8_t i = 0; i < CYDLCD_PRE_COUNT; i++) {
      snprintf(k, sizeof(k), "h%u", (unsigned)i);
      s_pre_h[i] = (uint16_t)p.getUShort(k, PRE_H_DEF[i]);
      snprintf(k, sizeof(k), "b%u", (unsigned)i);
      s_pre_b[i] = (uint16_t)p.getUShort(k, PRE_B_DEF[i]);
      snprintf(k, sizeof(k), "f%u", (unsigned)i);
      s_pre_f[i] = (uint8_t)p.getUChar(k, PRE_F_DEF[i]);
    }
    p.end();
  }
#endif
  // Chan gia tri rac (NVS hong / ghi tu ban khac)
  for (uint8_t i = 0; i < CYDLCD_PRE_COUNT; i++) {
    if (s_pre_h[i] > PRE_H_MAX) s_pre_h[i] = PRE_H_DEF[i];
    if (s_pre_b[i] > PRE_B_MAX) s_pre_b[i] = PRE_B_DEF[i];
    if (s_pre_f[i] > PRE_F_MAX) s_pre_f[i] = PRE_F_DEF[i];
  }
  WIFILOG("[PRE] preset: %s %u/%u/%u | %s %u/%u/%u | %s %u/%u/%u  (nozzle/bed/fan, sua o SETTINGS)\n",
          PRE_LABEL[0], (unsigned)s_pre_h[0], (unsigned)s_pre_b[0], (unsigned)s_pre_f[0],
          PRE_LABEL[1], (unsigned)s_pre_h[1], (unsigned)s_pre_b[1], (unsigned)s_pre_f[1],
          PRE_LABEL[2], (unsigned)s_pre_h[2], (unsigned)s_pre_b[2], (unsigned)s_pre_f[2]);
}

static void preheat_save(void) {
#if defined(CYDLCD_BOARD_CYD)
  Preferences p;
  if (p.begin(PRE_NVS_NS, false)) {                  // read-write
    char k[4];
    for (uint8_t i = 0; i < CYDLCD_PRE_COUNT; i++) {
      snprintf(k, sizeof(k), "h%u", (unsigned)i); p.putUShort(k, s_pre_h[i]);
      snprintf(k, sizeof(k), "b%u", (unsigned)i); p.putUShort(k, s_pre_b[i]);
      snprintf(k, sizeof(k), "f%u", (unsigned)i); p.putUChar(k, s_pre_f[i]);
    }
    p.end();
    WIFILOG("[PRE] da luu 3 preset preheat vao NVS (%s)\n", PRE_NVS_NS);
  }
#endif
}

// Bom 9 gia tri vao bang `cydlcd.settings[]` de trang SETTINGS hien/sua duoc (xem cydlcd_setting_set).
// Goi lai duoc nhieu lan (idempotent) - Marlin KHONG bao gio gui cac id nay.
static void preheat_inject(void) {
  const uint8_t before = cydlcd.n_settings;
  for (uint8_t i = 0; i < CYDLCD_PRE_COUNT; i++) {
    const uint16_t base = (uint16_t)(CYDLCD_PRE_ID_BASE + 1 + i * 3);
    cydlcd_setting_set(base,     (int32_t)s_pre_h[i] * 100);   // scale 100 (do C)
    cydlcd_setting_set(base + 1, (int32_t)s_pre_b[i] * 100);
    cydlcd_setting_set(base + 2, (int32_t)s_pre_f[i]);         // scale 1  (0..255)
  }
  // Chi in khi bang THAT SU duoc them dong (tranh spam: ham nay chay moi lan cap nhat trang).
  if (cydlcd.n_settings != before)
    WIFILOG("[PRE] da them 9 dong preset vao bang SETTINGS (%u -> %u dong)\n",
            (unsigned)before, (unsigned)cydlcd.n_settings);
}

// Preset preheat: gia tri lay tu KHO CUA MAN HINH CYD (NVS, sua duoc o trang SETTINGS - xem khoi "PREHEAT
// presets" o phan SETTINGS). Mac dinh ban dau = Configuration.h (PLA 200/60 - PETG 230/60 - ABS 240/60).
// KHONG gui M106 khi FAN = 0: Marlin khong dung FAN_SPEED cua preheat, gui M106 S0 se TAT quat ngoai y.
#define TP_PRE_H    28                                 // nut preset cao 28 >= 26 (du de cham)
#define TP_TUNE_H   28                                 // hang tuning (M306/M303/SAVE)
#define TP_PRE_PLA  0
#define TP_PRE_PETG 1
#define TP_PRE_ABS  2

static char      s_profile[8] = "";                    // profile dang chon ("" = chua chon -> ---)
static lv_obj_t *tp_prof;                              // nhan DONG DAU: "Profile: <ten>"
static lv_obj_t *tp_pre_btn[CYDLCD_PRE_COUNT];          // 3 nut PLA/PETG/ABS (nhan doi theo preset)
static char      s_pre_txt[CYDLCD_PRE_COUNT][16];       // nhan dang hien (de chi ve khi doi)

static void cb_temp_preheat(lv_event_t *e) {
  const uint8_t k = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (k >= CYDLCD_PRE_COUNT) return;

  char cmd[20];
  snprintf(cmd, sizeof(cmd), "M104 S%u", (unsigned)s_pre_h[k]);
  cydlcd_link_send_gcode(cmd);
  snprintf(cmd, sizeof(cmd), "M140 S%u", (unsigned)s_pre_b[k]);
  cydlcd_link_send_gcode(cmd);
  if (s_pre_f[k] > 0) {                                // 0 = khong dong tÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â»ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Âºi quat (xem ghi chu tren)
    snprintf(cmd, sizeof(cmd), "M106 S%u", (unsigned)s_pre_f[k]);
    cydlcd_link_send_gcode(cmd);
  }
  // Nho profile dang chon -> hien o DONG DAU (khong luu NVS). KHONG hien hop/dialog nao.
  snprintf(s_profile, sizeof(s_profile), "%s", PRE_LABEL[k]);
  // Target nhiet lay tu frame TEMPS/SETTING -> xin gui lai cho o nhiet khop ngay
  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);
}

// Doi nhan 3 nut theo preset hien co (chi ghi khi chuoi doi -> khong day SPI vo ich).
static void tp_pre_sync(void) {
  for (uint8_t k = 0; k < CYDLCD_PRE_COUNT; k++) {
    char t[16];
    snprintf(t, sizeof(t), "%s %u/%u", PRE_LABEL[k], (unsigned)s_pre_h[k], (unsigned)s_pre_b[k]);
    if (strcmp(t, s_pre_txt[k]) == 0) continue;
    snprintf(s_pre_txt[k], sizeof(s_pre_txt[k]), "%s", t);
    if (tp_pre_btn[k]) set_text(lv_obj_get_child(tp_pre_btn[k], 0), t);
  }
}

static void page_temp_create(lv_obj_t *p) {
  // DONG DAU: profile dang chon (bam PLA/PETG/ABS o duoi se doi dong nay)
  tp_prof = mk_label_w(p, "Profile: ---", MR_MENU, PAGE_INN_W);

  temp_row(p, "NOZZLE", 0, &tp_noz);
  temp_row(p, "BED",    1, &tp_bed);
  fan_row(p);

  // 1 hang preset: PLA | PETG | ABS (moi nut gui M104 + M140 theo preset da luu, nhu menu preheat
  // cua Marlin). Nhan nut duoc ve lai khi doi gia tri trong SETTINGS -> xem tp_pre_sync().
  lv_obj_t *rp = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, TP_PRE_H, 0, GAP);
  tp_pre_btn[0] = mk_btn_grow(rp, "PLA",  TP_PRE_H, cb_temp_preheat, (void *)(intptr_t)TP_PRE_PLA);
  tp_pre_btn[1] = mk_btn_grow(rp, "PETG", TP_PRE_H, cb_temp_preheat, (void *)(intptr_t)TP_PRE_PETG);
  tp_pre_btn[2] = mk_btn_grow(rp, "ABS",  TP_PRE_H, cb_temp_preheat, (void *)(intptr_t)TP_PRE_ABS);
  for (uint8_t k = 0; k < CYDLCD_PRE_COUNT; k++) s_pre_txt[k][0] = '\0';   // buoc tp_pre_sync ve lai
  tp_pre_sync();

  mk_btn(p, "COOLDOWN", PAGE_INN_W, 26, cb_action, (void *)(intptr_t)A_COOLDOWN);

  // P5 - tuning: MPC autotune dau in | PID ban in (target 60) | luu EEPROM
  lv_obj_t *rt = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, TP_TUNE_H, 0, GAP);
  mk_btn_grow(rt, "M306 T",      TP_TUNE_H, cb_action, (void *)(intptr_t)A_MPC_TUNE);
  mk_btn_grow(rt, "M303 E-1 60", TP_TUNE_H, cb_action, (void *)(intptr_t)A_PID_BED);
  mk_btn_grow(rt, "SAVE EEPROM", TP_TUNE_H, cb_action, (void *)(intptr_t)A_SAVE);

  // Tong chieu cao: 16 + 30*3 + 28 + 26 + 28 + 6*GAP = 200 <= PAGE_INN_H (206)
}

static void page_temp_update(void) {
  char b[40], a1[12], a2[12];
  int16_t cur = 0, tgt = 0;

  tp_pre_sync();                       // nhan PLA/PETG/ABS doi khi preset duoc sua o SETTINGS

  snprintf(b, sizeof(b), "Profile: %s", s_profile[0] ? s_profile : "---");
  set_text(tp_prof, b);

  temp_lookup(CYDLCD_TEMP_E0, &cur, &tgt);
  fmt_scaled(a1, sizeof(a1), cur, 100, 1);
  fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
  snprintf(b, sizeof(b), "%s / %s C", a1, a2);
  set_text(tp_noz, b);

  temp_lookup(CYDLCD_TEMP_BED, &cur, &tgt);
  fmt_scaled(a1, sizeof(a1), cur, 100, 1);
  fmt_scaled(a2, sizeof(a2), tgt, 100, 0);
  snprintf(b, sizeof(b), "%s / %s C", a1, a2);
  set_text(tp_bed, b);

  const uint8_t fp = cydlcd.n_fans ? (uint8_t)(cydlcd.fans[0].pct / 100) : 0;
  snprintf(b, sizeof(b), "%u%%", (unsigned)fp);
  set_text(tp_fan, b);
}

// ===========================================================================
//  FEED/FLOW (M220/M221) -- o FR / FL tren man STATUS mo vao day, dung nhu Marlin:
//  2 o do chinh la 2 trinh chinh feedrate / flowrate
// ===========================================================================
#define RT_ROW_H   34
#define RT_LBL_W   52
#define RT_BTN_W   40

static lv_obj_t *rt_fr, *rt_fl;

// user_data: (which << 8) | delta   (which 0 = feed -> M220, 1 = flow -> M221)
#define RATE_ADJ(w, d) ((void *)(intptr_t)(((unsigned)(w) << 8) | (unsigned)(uint8_t)(int8_t)(d)))

static void cb_rate_adj(lv_event_t *e) {
  const intptr_t v     = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  which = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const int8_t   delta = (int8_t)(uint8_t)((unsigned)v & 0xFFU);

  const int32_t cur = cydlcd_setting(which ? CYDLCD_SET_FLOW_PCT : CYDLCD_SET_FEEDRATE_PCT, 10000) / 100;
  int32_t next = cur + delta;
  if (next < 10) next = 10;                 // duoi 10% thi vo nghia
  if (next > 999) next = 999;

  char cmd[16];
  snprintf(cmd, sizeof(cmd), which ? "M221 S%ld" : "M220 S%ld", (long)next);
  cydlcd_link_send_gcode(cmd);
  // Marlin doi that nhung so tren man lay tu goi SETTING -> xin gui lai cho khoi cu
  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);
}

static void cb_rate_reset(lv_event_t *e) {
  (void)e;
  cydlcd_link_send_gcode2("M220 S100", "M221 S100");
  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);
}

static void rate_row(lv_obj_t *p, const char *name, uint8_t which, const lv_img_dsc_t *icon,
                     lv_obj_t **val) {
  lv_obj_t *r = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, RT_ROW_H, 0, GAP);
  mk_icon(r, 0, 0, icon, MR_RATE100);
  mk_label_w(r, name, COL_TXT, RT_LBL_W);
  mk_btn(r, "-10", RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, -10));
  mk_btn(r, "-1",  RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, -1));
  *val = mk_label_w(r, "100%", MR_RATE100, 72);
  lv_obj_set_flex_grow(*val, 1);
  mk_btn(r, "+1",  RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, +1));
  mk_btn(r, "+10", RT_BTN_W, RT_ROW_H - 2, cb_rate_adj, RATE_ADJ(which, +10));
}

static void page_rate_create(lv_obj_t *p) {
  rate_row(p, "FEED", 0, &ui_icon_feedrate, &rt_fr);
  rate_row(p, "FLOW", 1, &ui_icon_flowrate, &rt_fl);
  mk_btn(p, "RESET 100% (M220/M221 S100)", PAGE_INN_W, 34, cb_rate_reset, NULL);
  mk_label_w(p, "M220 S<n> = feed rate, M221 S<n> = flow rate", COL_DIM, PAGE_INN_W);
  mk_label_w(p, "gui ngay khi bam +/-  (xem o FR/FL tren STATUS)", COL_DIM, PAGE_INN_W);
}

static void page_rate_update(void) {
  char b[24];
  const int32_t fr = cydlcd_setting(CYDLCD_SET_FEEDRATE_PCT, 10000) / 100;
  const int32_t fl = cydlcd_setting(CYDLCD_SET_FLOW_PCT, 10000) / 100;
  snprintf(b, sizeof(b), "%ld%%", (long)fr);
  set_text(rt_fr, b);
  lv_obj_set_style_text_color(rt_fr, (fr == 100) ? MR_RATE100 : MR_RATEALT, 0);
  snprintf(b, sizeof(b), "%ld%%", (long)fl);
  set_text(rt_fl, b);
  lv_obj_set_style_text_color(rt_fl, (fl == 100) ? MR_RATE100 : MR_RATEALT, 0);
}

// ===========================================================================
//  JOG  (30 + 16 + 32 + 28 + 34 + 28 + 16 + 6*GAP = 196 <= 206)
//  P2: them buoc nho 0.01 / 0.05 mm; P7: M84 + G28 tung truc + M851 Z (lop phu sua)
// ===========================================================================
static uint8_t  s_axis = 0;                      // 0=X 1=Y 2=Z
static lv_obj_t *jg_axis[3];
// 0..4 tuong ung JOG_STEP[0..4]; JOG_STEP[step] la chuoi di kem trong lenh G1
static const char *const JOG_STEP[5] = {"0.01", "0.05", "0.1", "1", "10"};   // mm

#define JG_ROW_H   30
#define JG_STEP_H  32
#define JG_FINE_H  28
#define JG_BTN_H   28

// Danh dau TRUC DANG CHON bang NEN COL_ACCENT - DOC LAP HOAN TOAN voi focus.
//  - "dang chon" (s_axis)  = NEN nut: COL_ACCENT
//  - "focus"    (encoder/cham) = VIEN 2 px (xem focus_mark + LV_OBJ_FLAG_USER_1)
// Nho tach 2 kenh nay nen focus chuyen sang nut buoc / nut di chuyen KHONG con ghi de nen
// cua nut truc -> dau "truc dang chon" khong bao gio mat (loi nguoi dung bao truoc day).
static void jog_axis_mark(void) {
  for (uint8_t i = 0; i < 3; i++) {
    if (!jg_axis[i]) continue;
    const lv_color_t want = (i == s_axis) ? COL_ACCENT : COL_BTN;
    // Chi set khi mau THAT SU doi: lv_obj_set_style_bg_color() luon invalidate vat, goi vo
    // dieu kien moi 150 ms se lam man JOG nhap nhay va ton bang thong SPI.
    if (lv_obj_get_style_bg_color(jg_axis[i], 0).full != want.full)
      lv_obj_set_style_bg_color(jg_axis[i], want, 0);
  }
}

static void cb_jog_axis(lv_event_t *e) {
  const uint8_t a = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (a > 2) return;
  s_axis = a;
  jog_axis_mark();
}

// user_data: (chi so buoc << 8) | (1 = am)
static void cb_jog_move(lv_event_t *e) {
  const intptr_t v = (intptr_t)lv_event_get_user_data(e);
  const uint8_t  step = (uint8_t)(((unsigned)v >> 8) & 0xFFU);
  const bool     neg  = (((unsigned)v & 0xFFU) != 0);
  if (step > 4) return;

  char cmd[24];
  // Toc do nhich: X/Y = 3000 mm/phut = 50 mm/s (bang HOMING_FEEDRATE X/Y cho de so sanh),
  // Z giu 600 mm/phut = 10 mm/s (= tran M203 Z10, vit me nen phai cham).
  // Truoc day dung F600 cho ca 3 truc -> nhich X/Y cham hon home 5 lan (2026-10-13).
  const unsigned f = (s_axis == 2) ? 600U : 3000U;
  snprintf(cmd, sizeof(cmd), "G1 %c%s%s F%u", "XYZ"[s_axis], neg ? "-" : "", JOG_STEP[step], f);
  // G91 ... G90: luon dong lai bang G90 de mot dong bi mat khong de may o che do tuong doi
  cydlcd_link_send_gcode("G91");
  cydlcd_link_send_gcode(cmd);
  cydlcd_link_send_gcode("G90");
}

#define JOG_ARG(step, neg) ((void *)(intptr_t)(((unsigned)(step) << 8) | (unsigned)((neg) ? 1U : 0U)))

static void page_jog_create(lv_obj_t *p) {
  lv_obj_t *ra = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_ROW_H, 0, GAP);
  mk_label_w(ra, "TRUC", COL_DIM, 44);
  jg_axis[0] = mk_btn_grow(ra, "X", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)0);
  jg_axis[1] = mk_btn_grow(ra, "Y", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)1);
  jg_axis[2] = mk_btn_grow(ra, "Z", JG_ROW_H - 2, cb_jog_axis, (void *)(intptr_t)2);
  // 3 nut truc TU QUAN LY NEN (LV_OBJ_FLAG_USER_1): focus_mark() chi ve VIEN cho chung, KHONG doi
  // nen -> nen COL_ACCENT cua truc dang chon khong bi ghi de khi focus sang nut buoc/di chuyen.
  for (uint8_t i = 0; i < 3; i++) lv_obj_add_flag(jg_axis[i], LV_OBJ_FLAG_USER_1);
  jog_axis_mark();                                // to nhat truc dang chon (s_axis = 0 luc tao man)

  mk_label_w(p, "DI CHUYEN (mm)", COL_DIM, PAGE_INN_W);

  lv_obj_t *rs = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_STEP_H, 0, GAP);
  mk_btn_grow(rs, "-0.1", JG_STEP_H - 2, cb_jog_move, JOG_ARG(2, true));
  mk_btn_grow(rs, "+0.1", JG_STEP_H - 2, cb_jog_move, JOG_ARG(2, false));
  mk_btn_grow(rs, "-1",   JG_STEP_H - 2, cb_jog_move, JOG_ARG(3, true));
  mk_btn_grow(rs, "+1",   JG_STEP_H - 2, cb_jog_move, JOG_ARG(3, false));
  mk_btn_grow(rs, "-10",  JG_STEP_H - 2, cb_jog_move, JOG_ARG(4, true));
  mk_btn_grow(rs, "+10",  JG_STEP_H - 2, cb_jog_move, JOG_ARG(4, false));

  // P2: buoc nho (0.01 / 0.05 mm) - dung cho can chinh tinh
  lv_obj_t *rf = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_FINE_H, 0, GAP);
  mk_btn_grow(rf, "-0.01", JG_FINE_H - 2, cb_jog_move, JOG_ARG(0, true));
  mk_btn_grow(rf, "+0.01", JG_FINE_H - 2, cb_jog_move, JOG_ARG(0, false));
  mk_btn_grow(rf, "-0.05", JG_FINE_H - 2, cb_jog_move, JOG_ARG(1, true));
  mk_btn_grow(rf, "+0.05", JG_FINE_H - 2, cb_jog_move, JOG_ARG(1, false));

  mk_btn(p, "HOME ALL (G28)", PAGE_INN_W, 34, cb_action, (void *)(intptr_t)A_HOME);

  // P7 + P2: tat dong co, home tung truc, sua probe Z (mo lop phu M851 Z)
  lv_obj_t *rb = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, JG_BTN_H, 0, GAP);
  mk_btn_grow(rb, "M84",    JG_BTN_H, cb_action, (void *)(intptr_t)A_STEPPERS_OFF);
  mk_btn_grow(rb, "G28 X",  JG_BTN_H, cb_action, (void *)(intptr_t)A_HOME_X);
  mk_btn_grow(rb, "G28 Y",  JG_BTN_H, cb_action, (void *)(intptr_t)A_HOME_Y);
  mk_btn_grow(rb, "G28 Z",  JG_BTN_H, cb_action, (void *)(intptr_t)A_HOME_Z);
  mk_btn_grow(rb, "M851 Z", JG_BTN_H, cb_action, (void *)(intptr_t)A_EDIT_PROBE_Z);

  mk_label_w(p, "bam = G91 + G1 <truc><d> F3000 (X/Y) / F600 (Z) + G90", COL_DIM, PAGE_INN_W);
}

static void page_jog_update(void) {
  // Ve LAI dau truc dang chon o MOI chu ky cap nhat (khong chi luc bam): neu co ai do doi style
  // (hoac doi man roi quay lai) thi nen COL_ACCENT van duoc khoi phuc ngay trong ~150 ms.
  jog_axis_mark();
}

// ===========================================================================
//  FILES  (PAGE_INN_H = 206)
//    info 17 (font mono mode 4: line_height = 17) + roller 64
//    + hang1 26 + hang2 26 + hang3 (XOA FILE) 26 + 4*GAP 8 = 167 <= 206
//    (truoc khi them nut XOA FILE: 17 + 64 + 26 + 26 + 3*GAP = 139)
//  (167 + 2*PAGE_PAD = 175 < 206 nen man nay KHONG tran -> khong hien thanh day PREV/NEXT)
// ===========================================================================
#define FILES_BTN_H   26
#define FILES_ROLLER_H (4 * ROW_H)      // 64: 4 dong nhin thay (< 104)

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
      const int w = snprintf(p, left, "%s%s%s", (i ? "\n" : ""), isdir ? "[D] " : "",
                             cydlcd.files[i].name);
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

static void files_init_sd(void) {
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
static void files_delete_now(void) {
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
static void files_job_task(void) {
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

static void page_files_create(lv_obj_t *p) {
  fl_info = mk_label_w(p, "SD: -", COL_TXT, PAGE_INN_W);

  fl_roller = lv_roller_create(p);
  lv_obj_set_width(fl_roller, PAGE_INN_W);
  lv_roller_set_visible_row_count(fl_roller, 4);
  lv_obj_set_height(fl_roller, FILES_ROLLER_H);
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
  // Voi encoder: bam lan 1 = vao che do edit (xoay chon file), bam lan 2 = in file dang chon
  lv_obj_add_event_cb(fl_roller, cb_files_print, LV_EVENT_CLICKED, NULL);

  // 3 hang nut: 4 nut tren (PRINT/DIR/UP/REFRESH) + 3 nut duoi (PAUSE/RESUME/STOP)
  // + 1 hang rieng cho XOA FILE (viec khong hoan tac duoc -> de rieng, de bam nham)
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

static void page_files_update(void) {
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

// ===========================================================================
//  SETTINGS (bang thong so Marlin gui + SUA duoc: cham 1 dong -> lop phu -/+/OK)
//  Bang (cao SET_TABLE_H) + 1 hang nut UP | DOWN | SAVE EEPROM + 1 dong info <= 206
// ===========================================================================
typedef struct {
  uint16_t    id;
  const char *name;
  uint16_t    scale;     // CYDLCD_VAL_SCALE / CYDLCD_TEMP_SCALE / CYDLCD_PCT_SCALE
  uint8_t     dec;
} set_row_t;

static const set_row_t SET_ROWS[] = {
  // KHONG co dong "Z offset": CYDLCD_SET_Z_OFFSET va CYDLCD_SET_PROBE_OFFSET_Z cung la
  // getZOffset_mm() (probe.offset.z) -> se trung 2 dong, va sua bang M206 Z se ghi vao
  // HOME OFFSET (sai cho Voron Tap). Chi giu "Probe Z mm" -> M851 Z.
  { CYDLCD_SET_PROBE_OFFSET_Z,"Probe Z mm",    1000, 3 },
  { CYDLCD_SET_PROBE_OFFSET_X,"Probe X mm",    1000, 2 },
  { CYDLCD_SET_PROBE_OFFSET_Y,"Probe Y mm",    1000, 2 },
  { CYDLCD_SET_FEEDRATE_PCT,  "Feed %",         100, 0 },
  { CYDLCD_SET_FLOW_PCT,      "Flow %",         100, 0 },
  { CYDLCD_SET_FAN_PCT_E0,    "Fan E0 %",       100, 0 },
  { CYDLCD_SET_TARGET_TEMP_E0,"Target E0 C",    100, 0 },
  { CYDLCD_SET_TARGET_TEMP_BED,"Target bed C",  100, 0 },
  // PREHEAT presets (MAN HINH CYD tu luu trong NVS - xem khoi o tren): do C / do C / 0..255
  { CYDLCD_PRE_ID_BASE + 1, "PLA nozzle C",     100, 0 },
  { CYDLCD_PRE_ID_BASE + 2, "PLA bed C",        100, 0 },
  { CYDLCD_PRE_ID_BASE + 3, "PLA fan",            1, 0 },
  { CYDLCD_PRE_ID_BASE + 4, "PETG nozzle C",    100, 0 },
  { CYDLCD_PRE_ID_BASE + 5, "PETG bed C",       100, 0 },
  { CYDLCD_PRE_ID_BASE + 6, "PETG fan",           1, 0 },
  { CYDLCD_PRE_ID_BASE + 7, "ABS nozzle C",     100, 0 },
  { CYDLCD_PRE_ID_BASE + 8, "ABS bed C",        100, 0 },
  { CYDLCD_PRE_ID_BASE + 9, "ABS fan",            1, 0 },
  { CYDLCD_SET_STEPS_MM_X,    "Steps X",       1000, 2 },
  { CYDLCD_SET_STEPS_MM_Y,    "Steps Y",       1000, 2 },
  { CYDLCD_SET_STEPS_MM_Z,    "Steps Z",       1000, 2 },
  { CYDLCD_SET_STEPS_MM_E,    "Steps E",       1000, 2 },
  { CYDLCD_SET_MAXFEED_X,     "Max feed X",    1000, 1 },
  { CYDLCD_SET_MAXFEED_Y,     "Max feed Y",    1000, 1 },
  { CYDLCD_SET_MAXFEED_Z,     "Max feed Z",    1000, 1 },
  { CYDLCD_SET_MAXFEED_E,     "Max feed E",    1000, 1 },
  { CYDLCD_SET_MAXACCEL_X,    "Max accel X",   1000, 0 },
  { CYDLCD_SET_MAXACCEL_Y,    "Max accel Y",   1000, 0 },
  { CYDLCD_SET_MAXACCEL_Z,    "Max accel Z",   1000, 0 },
  { CYDLCD_SET_MAXACCEL_E,    "Max accel E",   1000, 0 },
  { CYDLCD_SET_JERK_X,        "Jerk X",        1000, 1 },
  { CYDLCD_SET_JERK_Y,        "Jerk Y",        1000, 1 },
  { CYDLCD_SET_JERK_Z,        "Jerk Z",        1000, 1 },
  { CYDLCD_SET_JERK_E,        "Jerk E",        1000, 1 },
  { CYDLCD_SET_JUNCTION_DEV,  "Junction dev",  1000, 3 },
  { CYDLCD_SET_LINEAR_ADV_E,  "Linear adv E",  1000, 3 },
  { CYDLCD_SET_PID_E_KP,      "PID E Kp",      1000, 2 },
  { CYDLCD_SET_PID_E_KI,      "PID E Ki",      1000, 3 },
  { CYDLCD_SET_PID_E_KD,      "PID E Kd",      1000, 2 },
  { CYDLCD_SET_PID_BED_KP,    "PID bed Kp",    1000, 2 },
  { CYDLCD_SET_PID_BED_KI,    "PID bed Ki",    1000, 3 },
  { CYDLCD_SET_PID_BED_KD,    "PID bed Kd",    1000, 2 },
  // P8: gia toc M204 + dong TMC M906
  { CYDLCD_SET_MAXACCEL_PRINT,   "Accel print",   1000, 0 },
  { CYDLCD_SET_MAXACCEL_RETRACT, "Accel retract", 1000, 0 },
  { CYDLCD_SET_MAXACCEL_TRAVEL,  "Accel travel",  1000, 0 },
  { CYDLCD_SET_TMC_CURRENT_X,    "TMC I X mA",    1000, 0 },
  { CYDLCD_SET_TMC_CURRENT_Y,    "TMC I Y mA",    1000, 0 },
  { CYDLCD_SET_TMC_CURRENT_Z,    "TMC I Z mA",    1000, 0 },
  { CYDLCD_SET_TMC_CURRENT_E,    "TMC I E mA",    1000, 0 }
};
#define SET_NROWS ((uint8_t)(sizeof(SET_ROWS) / sizeof(SET_ROWS[0])))

// Bang 124 + info 16 + 2 hang nut (30 + 28) + 3*GAP = 206 <= PAGE_INN_H
#define SET_TABLE_H 124                              // bang (cuon duoc) - xem tong chieu cao tren
#define SET_COL0_W  190
#define SET_COL1_W  (PAGE_INN_W - SET_COL0_W - 4)

static lv_obj_t *se_table, *se_info;
static uint8_t   se_idx[SET_NROWS];   // CHI so cac dong CO gia tri: loc bo cac dong ma ban build
static uint8_t   se_n = 0;            // nay khong gui (Junction dev / Linear adv / PID... -> trong)

static void cb_scroll_table(lv_event_t *e) {
  const lv_coord_t d = (lv_coord_t)(intptr_t)lv_event_get_user_data(e);
  if (se_table) lv_obj_scroll_by(se_table, 0, d, LV_ANIM_OFF);
}

// Loc: chi giu nhung dong ma MAN HINH CYD DA NHAN duoc gia tri tu Marlin (2026-10-13, yeu cau nguoi dung)
static void settings_build_list(void) {
  se_n = 0;
  for (uint8_t i = 0; i < SET_NROWS; i++)
    if (cydlcd_setting(SET_ROWS[i].id, INT32_MIN) != INT32_MIN) se_idx[se_n++] = i;
}

static void settings_fill_table(void) {
  lv_table_set_row_cnt(se_table, (uint16_t)(se_n + 1));
  for (uint8_t k = 0; k < se_n; k++)
    lv_table_set_cell_value(se_table, (uint16_t)(k + 1), 0, SET_ROWS[se_idx[k]].name);
}

// ---------------------------------------------------------------------------
//  Lop phu SUA 1 dong: cham vao 1 dong cua bang -> hien khung nho ngay TREN bang
//  gom ten thong so + gia tri dang sua + 4 NUT BUOC 0.01 | 0.1 | 1 | 10
//  + 2 nut - | + de ap buoc do vao gia tri + OK | Huy.
//  Buoc nguoi dung chon la don vi HIEN THI -> quy ve raw = buoc * scale, va KHONG nho hon
//  1 don vi hien thi cuoi cua dong (scale / 10^dec); nho vay gia tri luon nam dung so le
//  hien thi (khong bao gio co nhieu chu so thap phan hon dong do). Moi tinh toan la so nguyen.
//  Bam OK: gui M-code dung voi .id (xem se_send_mcode) roi xin Marlin gui lai thong so.
//  Chu trong lop phu dung font mono GFX co dau tieng Viet (che do 4 - xem vn_font()); o che do 0
//  thi cac nhan co dau nay se ra O VUONG (Montserrat khong co U+1EA0..U+1EF9).
// ---------------------------------------------------------------------------
#define OV_W       260                              // khung noi dung (giua man 320)
#define OV_H       112
#define OV_BTN_H   28                               // nut >= 26 px cho de cham
#define OV_LINE_H  20                               // 1 dong chu cua font Segoe (line_height 18)
#define OV_PAD     4
#define OV_VCAP    150                              // be rong o gia tri
#define OV_RAW_LIM 100000000L                       // tran raw (du moi thong so o day)
#define OV_ADJ(d)  ((void *)(intptr_t)(int8_t)(d))
#define OV_STEP(d) ((void *)(intptr_t)(d))

static const uint32_t OV_TEN_POW[4] = {1, 10, 100, 1000};

// 4 muc buoc, don vi HIEN THI nhan 100 (giu so nguyen: 0.01 -> 1)
static const int32_t OV_STEP100[4] = { 1, 10, 100, 1000 };
static const char *const OV_STEP_TXT[4] = { "0.01", "0.1", "1", "10" };

static lv_obj_t *ov_bd, *ov_box, *ov_name, *ov_val, *ov_hint, *ov_minus;
static lv_obj_t *ov_step_btn[4];
static uint8_t  ov_step_k = 0;   // muc buoc dang chon (0..3)
static uint8_t  ov_i = 0;        // chi so THAT trong SET_ROWS[] (khong phai chi so dong dang hien)
static int32_t  ov_raw = 0;      // gia tri dang sua (raw = gia tri hien thi * scale)
static bool     ov_open = false;
static bool     ov_rep = false;  // da nhay do GIU nut -> bo qua lan CLICKED luc nhac tay

// 1 don vi HIEN THI cuoi cung cua dong (raw) = scale / 10^dec
static int32_t ov_quantum(const uint8_t i) {
  const uint8_t  dec = (SET_ROWS[i].dec > 3) ? 3 : SET_ROWS[i].dec;
  const uint32_t sc  = SET_ROWS[i].scale ? SET_ROWS[i].scale : 1;
  const uint32_t q   = sc / OV_TEN_POW[dec];
  return (int32_t)(q ? q : 1);
}

// Buoc ap dung (raw) = buoc dang chon * scale, nhung khong nho hon 1 don vi hien thi cuoi
static int32_t ov_step(const uint8_t i) {
  const uint32_t sc = SET_ROWS[i].scale ? SET_ROWS[i].scale : 1;
  const int32_t  q  = ov_quantum(i);
  const int32_t  st = (int32_t)(((int64_t)OV_STEP100[ov_step_k] * (int64_t)sc) / 100);
  return (st > q) ? st : q;
}

// Muc buoc gan nhat voi 10^(-dec) cua dong (dec 2/3 -> 0.01 - dec 1 -> 0.1 - dec 0 -> 1)
static uint8_t ov_step_default(const uint8_t i) {
  const uint8_t dec = (SET_ROWS[i].dec > 3) ? 3 : SET_ROWS[i].dec;
  return (dec >= 2) ? 0 : (uint8_t)(2 - dec);
}

// To nut buoc dang chon (giong cach jg_axis[] to COL_ACCENT).
// Cung quy uoc voi jg_axis[] man JOG: "dang chon" = NEN COL_ACCENT, "focus" = VIEN (focus_mark
// bo qua doi nen voi vat co LV_OBJ_FLAG_USER_1 -> focus roi di KHONG tra nen nut ve COL_BTN).
static void ov_marks(void) {
  for (uint8_t k = 0; k < 4; k++) {
    if (!ov_step_btn[k]) continue;
    const lv_color_t want = (k == ov_step_k) ? COL_ACCENT : COL_BTN;
    // Chi set khi mau THAT SU doi: ham nay duoc goi moi chu ky cap nhat cua man SETTINGS
    // (400 ms) va lv_obj_set_style_bg_color() luon invalidate vat -> goi vo dieu kien se
    // lam lop phu nhap nhay va ton bang thong SPI.
    if (lv_obj_get_style_bg_color(ov_step_btn[k], 0).full != want.full)
      lv_obj_set_style_bg_color(ov_step_btn[k], want, 0);
  }
}

// Tim dong khai bao cua 1 id (de in so dung so le nhu bang dang hien)
static const set_row_t *se_find(const uint16_t id) {
  for (uint8_t i = 0; i < SET_NROWS; i++)
    if (SET_ROWS[i].id == id) return &SET_ROWS[i];
  return NULL;
}

static void se_fmt_id(char *b, size_t n, const uint16_t id, const int32_t raw) {
  const set_row_t *r = se_find(id);
  fmt_scaled(b, n, raw, r ? r->scale : 1000, r ? r->dec : 2);
}

// Ky tu truc cho nhom id LIEN TIEP X Y Z E (tra 0 neu id khong thuoc nhom).
// n = so phan tu cua nhom (4 cho X/Y/Z/E, 3 cho probe X/Y/Z) - phai kiem tra dung so
// luong vi cac nhom nay nam lien nhau trong enum cydlcd_setting_t.
static char se_axis_char(const uint16_t id, const uint16_t first, const uint8_t n) {
  const uint16_t d = (uint16_t)(id - first);
  return (d < n) ? "XYZE"[d] : (char)0;
}

// Gui M-code ung voi 1 dong (dung gia tri 'raw' dang sua). false = id khong ho tro sua.
static bool se_send_mcode(const uint8_t i, const int32_t raw) {
  const uint16_t id = SET_ROWS[i].id;
  char v[16], a[16], b[16], c[16], cmd[64];
  char ax;

  se_fmt_id(v, sizeof(v), id, raw);

  if ((ax = se_axis_char(id, CYDLCD_SET_STEPS_MM_X, 4)) != 0)
    snprintf(cmd, sizeof(cmd), "M92 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_MAXFEED_X, 4)) != 0)
    snprintf(cmd, sizeof(cmd), "M203 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_MAXACCEL_X, 4)) != 0)
    snprintf(cmd, sizeof(cmd), "M201 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_JERK_X, 4)) != 0)
    snprintf(cmd, sizeof(cmd), "M205 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_PROBE_OFFSET_X, 3)) != 0)
    snprintf(cmd, sizeof(cmd), "M851 %c%s", ax, v);
  else if ((ax = se_axis_char(id, CYDLCD_SET_TMC_CURRENT_X, 4)) != 0)   // P8: M906 X/Y/Z/E <mA>
    snprintf(cmd, sizeof(cmd), "M906 %c%s", ax, v);
  else if (id == CYDLCD_SET_MAXACCEL_PRINT)      // P8: M204 P (gia toc in)
    snprintf(cmd, sizeof(cmd), "M204 P%s", v);
  else if (id == CYDLCD_SET_MAXACCEL_RETRACT)    //     M204 R (gia toc lui dao)
    snprintf(cmd, sizeof(cmd), "M204 R%s", v);
  else if (id == CYDLCD_SET_MAXACCEL_TRAVEL)     //     M204 T (gia toc di chuyen)
    snprintf(cmd, sizeof(cmd), "M204 T%s", v);
  else if (id == CYDLCD_SET_FEEDRATE_PCT)
    snprintf(cmd, sizeof(cmd), "M220 S%s", v);
  else if (id == CYDLCD_SET_FLOW_PCT)
    snprintf(cmd, sizeof(cmd), "M221 S%s", v);
  else if (id == CYDLCD_SET_TARGET_TEMP_E0)
    snprintf(cmd, sizeof(cmd), "M104 S%s", v);
  else if (id == CYDLCD_SET_TARGET_TEMP_BED)
    snprintf(cmd, sizeof(cmd), "M140 S%s", v);
  else if (id == CYDLCD_SET_FAN_PCT_E0) {
    // % -> 0..255 (lam tron). raw o day da nhan 100 (CYDLCD_PCT_SCALE)
    int32_t s = (raw * 255 + 5000) / 10000;
    if (s < 0) s = 0; else if (s > 255) s = 255;
    snprintf(cmd, sizeof(cmd), "M106 S%ld", (long)s);
  }
  else if (id >= CYDLCD_SET_PID_E_KP && id <= CYDLCD_SET_PID_E_KD) {
    // M301 P/I/D: gui CA 3 gia tri, cho nao dang sua thi lay gia tri moi
    int32_t kp = cydlcd_setting(CYDLCD_SET_PID_E_KP, 0);
    int32_t ki = cydlcd_setting(CYDLCD_SET_PID_E_KI, 0);
    int32_t kd = cydlcd_setting(CYDLCD_SET_PID_E_KD, 0);
    if (id == CYDLCD_SET_PID_E_KP) kp = raw;
    else if (id == CYDLCD_SET_PID_E_KI) ki = raw;
    else kd = raw;
    se_fmt_id(a, sizeof(a), CYDLCD_SET_PID_E_KP, kp);
    se_fmt_id(b, sizeof(b), CYDLCD_SET_PID_E_KI, ki);
    se_fmt_id(c, sizeof(c), CYDLCD_SET_PID_E_KD, kd);
    snprintf(cmd, sizeof(cmd), "M301 P%s I%s D%s", a, b, c);
  }
  else if (id >= CYDLCD_SET_PID_BED_KP && id <= CYDLCD_SET_PID_BED_KD) {
    int32_t kp = cydlcd_setting(CYDLCD_SET_PID_BED_KP, 0);
    int32_t ki = cydlcd_setting(CYDLCD_SET_PID_BED_KI, 0);
    int32_t kd = cydlcd_setting(CYDLCD_SET_PID_BED_KD, 0);
    if (id == CYDLCD_SET_PID_BED_KP) kp = raw;
    else if (id == CYDLCD_SET_PID_BED_KI) ki = raw;
    else kd = raw;
    se_fmt_id(a, sizeof(a), CYDLCD_SET_PID_BED_KP, kp);
    se_fmt_id(b, sizeof(b), CYDLCD_SET_PID_BED_KI, ki);
    se_fmt_id(c, sizeof(c), CYDLCD_SET_PID_BED_KD, kd);
    snprintf(cmd, sizeof(cmd), "M304 P%s I%s D%s", a, b, c);
  }
  else if (id > CYDLCD_PRE_ID_BASE && id <= (uint16_t)(CYDLCD_PRE_ID_BASE + CYDLCD_PRE_COUNT * 3)) {
    // PREHEAT preset: KHONG gui M-code nao (Marlin khong co M145 trong build nay) - ghi vao NVS
    // CUA MAN HINH CYD roi bom lai vao bang de bang + nut o man TEMP thay gia tri moi ngay.
    const uint8_t k   = (uint8_t)(id - CYDLCD_PRE_ID_BASE - 1);   // 0..8
    const uint8_t pre = (uint8_t)(k / 3);                        // 0..2 (PLA/PETG/ABS)
    const uint8_t fld = (uint8_t)(k % 3);                        // 0 hotend, 1 bed, 2 fan
    int32_t v = raw;
    if (fld == 2) {
      if (v < 0) v = 0; else if (v > PRE_F_MAX) v = PRE_F_MAX;
      s_pre_f[pre] = (uint8_t)v;
    } else {
      const int32_t mx = (fld == 0) ? PRE_H_MAX : PRE_B_MAX;
      v = v / 100;                                     // raw = do C * 100 (scale 100) -> do C
      if (v < 0) v = 0; else if (v > mx) v = mx;
      if (fld == 0) s_pre_h[pre] = (uint16_t)v; else s_pre_b[pre] = (uint16_t)v;
    }
    preheat_save();
    preheat_inject();
    WIFILOG("[PRE] %s: nozzle=%u bed=%u fan=%u (da luu NVS)\n", PRE_LABEL[pre],
            (unsigned)s_pre_h[pre], (unsigned)s_pre_b[pre], (unsigned)s_pre_f[pre]);
    return true;
  }
  else return false;      // id khac (Junction dev / Linear adv...) -> khong gui gi

  cydlcd_link_send_gcode(cmd);
  return true;
}

// Hien lai gia tri dang sua + buoc dang ap dung.
// Chu co dau duoc viet bang MA UTF-8 (\x..) de file nguon van la ASCII thuan - xem ghi chu
// "font tieng Viet" o dau file. Hai chuoi o day la "Gia tri: <v>" va "Buoc <buoc>".
static void ov_refresh(void) {
  char a[16], b[40];
  fmt_scaled(a, sizeof(a), ov_raw, SET_ROWS[ov_i].scale, SET_ROWS[ov_i].dec);
  snprintf(b, sizeof(b), "Gi\xC3\xA1 tr\xE1\xBB\x8B: %s", a);     // "Gia tri: <v>" (co dau)
  set_text(ov_val, b);
  fmt_scaled(a, sizeof(a), ov_step(ov_i), SET_ROWS[ov_i].scale, SET_ROWS[ov_i].dec);
  snprintf(b, sizeof(b), "B\xC6\xB0\xE1\xBB\x9B\x63 %s", a);      // "Buoc <buoc>" (co dau)
  set_text(ov_hint, b);
}

static void ov_hide(void) {
  ov_open = false;
  ov_rep  = false;
  if (ov_bd)  lv_obj_add_flag(ov_bd, LV_OBJ_FLAG_HIDDEN);
  if (ov_box) lv_obj_add_flag(ov_box, LV_OBJ_FLAG_HIDDEN);
}

static void ov_close(void) {
  if (!ov_open) return;
  ov_hide();
  refocus_page();                     // tra focus ve nut dau tien cua man SETTINGS
}

static void cb_se_ov_adj(lv_event_t *e) {
  const lv_event_code_t code = lv_event_get_code(e);
  const int8_t d = (int8_t)(intptr_t)lv_event_get_user_data(e);
  if (code == LV_EVENT_PRESSED) { ov_rep = false; return; }
  if (code == LV_EVENT_LONG_PRESSED_REPEAT) { ov_rep = true; }   // GIU nut -> nhay lien tuc
  else if (ov_rep) { ov_rep = false; return; }                   // bo qua CLICKED luc nhac tay
  if (!ov_open) return;

  const int32_t st = ov_step(ov_i);
  int64_t nv = (int64_t)ov_raw + (int64_t)d * (int64_t)st;
  if (nv >  (int64_t)OV_RAW_LIM) nv =  (int64_t)OV_RAW_LIM;
  if (nv < -(int64_t)OV_RAW_LIM) nv = -(int64_t)OV_RAW_LIM;
  ov_raw = (int32_t)nv;
  ov_refresh();
}

static void cb_se_ov_ok(lv_event_t *e) {
  (void)e;
  if (!ov_open) return;
  const uint8_t i = ov_i;
  const int32_t v = ov_raw;
  ov_hide();
  se_send_mcode(i, v);                                  // id khong ho tro -> khong gui gi
  cydlcd_link_request((uint8_t)CYDLCD_REQ_SETTINGS);      // xin lai de bang tu khop gia tri
  refocus_page();
}

static void cb_se_ov_cancel(lv_event_t *e) { (void)e; ov_close(); }

// Chon buoc: to lai nut dang chon + cap nhat dong "Buoc ..." (khong doi gia tri dang sua)
static void cb_se_ov_step(lv_event_t *e) {
  const uint8_t k = (uint8_t)(intptr_t)lv_event_get_user_data(e);
  if (k > 3) return;
  ov_step_k = k;
  ov_marks();
  if (ov_open) ov_refresh();
}

static void ov_create(void) {
  lv_obj_t *scr = lv_scr_act();

  // Nen mo: nuot moi lan cham ra NGOAI khung (khong cho roi xuong bang ben duoi)
  ov_bd = lv_obj_create(scr);
  lv_obj_set_pos(ov_bd, 0, CONT_Y);
  lv_obj_set_size(ov_bd, UI_LCD_HOR_RES, CONT_H);
  lv_obj_set_style_bg_color(ov_bd, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_opa(ov_bd, LV_OPA_70, 0);
  lv_obj_set_style_border_width(ov_bd, 0, 0);
  lv_obj_set_style_radius(ov_bd, 0, 0);
  lv_obj_set_style_pad_all(ov_bd, 0, 0);
  lv_obj_clear_flag(ov_bd, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ov_bd, LV_OBJ_FLAG_HIDDEN);

  ov_box = lv_obj_create(scr);
  lv_obj_set_pos(ov_box, (UI_LCD_HOR_RES - OV_W) / 2, (lv_coord_t)(CONT_Y + (CONT_H - OV_H) / 2));
  lv_obj_set_size(ov_box, OV_W, OV_H);
  lv_obj_set_style_bg_color(ov_box, COL_BAR, 0);
  lv_obj_set_style_bg_opa(ov_box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(ov_box, COL_ACCENT, 0);
  lv_obj_set_style_border_width(ov_box, 1, 0);
  lv_obj_set_style_radius(ov_box, 3, 0);
  lv_obj_set_style_pad_all(ov_box, OV_PAD, 0);
  lv_obj_set_style_pad_row(ov_box, GAP, 0);
  lv_obj_clear_flag(ov_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(ov_box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(ov_box, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  // KHONG dat HIDDEN o day: neu tao con trong luc cha dang an thi LVGL khong tinh layout
  // (align/flex) cho nhanh do -> nut rong 0 px = "khong co text". ov_open_row() se hien sau.

  // Dong 1: ten thong so (ten tieng Anh lay tu SET_ROWS, khong co dau)
  ov_name = mk_label_w(ov_box, "-", MR_MENU, OV_W - 2 * OV_PAD);

  // Dong 2: "Gia tri: <so>" (xanh la) + "Buoc <buoc>" (xam) - xem ov_refresh()
  lv_obj_t *rv = mk_flex(ov_box, LV_FLEX_FLOW_ROW, OV_W - 2 * OV_PAD, OV_LINE_H, 0, GAP);
  ov_val  = mk_label_w(rv, "-", COL_OK, OV_VCAP);
  ov_hint = mk_label_w(rv, "", COL_DIM, OV_W - 2 * OV_PAD - OV_VCAP - GAP);
  vn_font(ov_val);              // 2 nhan nay co dau tieng Viet -> dung font Segoe
  vn_font(ov_hint);

  // Dong 3: 4 NUT BUOC co dinh - nut dang chon duoc to COL_ACCENT (nhu jg_axis[])
  lv_obj_t *rs = mk_flex(ov_box, LV_FLEX_FLOW_ROW, OV_W - 2 * OV_PAD, OV_BTN_H, 0, GAP);
  for (uint8_t k = 0; k < 4; k++)
    ov_step_btn[k] = mk_btn_grow(rs, OV_STEP_TXT[k], OV_BTN_H, cb_se_ov_step, OV_STEP(k));
  // 4 nut buoc TU QUAN LY NEN (LV_OBJ_FLAG_USER_1): giong 3 nut truc man JOG - focus_mark()
  // chi ve VIEN (mau sang MR_FRAME) cho chung, KHONG doi nen -> nen COL_ACCENT cua buoc dang
  // chon khong bi ghi de khi focus chuyen sang nut khac trong lop phu.
  for (uint8_t k = 0; k < 4; k++) lv_obj_add_flag(ov_step_btn[k], LV_OBJ_FLAG_USER_1);

  // Dong 4: - | + | OK | Huy
  lv_obj_t *rb = mk_flex(ov_box, LV_FLEX_FLOW_ROW, OV_W - 2 * OV_PAD, OV_BTN_H, 0, GAP);
  ov_minus = mk_btn_grow(rb, "-",   OV_BTN_H, cb_se_ov_adj, OV_ADJ(-1));
  lv_obj_t *plus = mk_btn_grow(rb, "+", OV_BTN_H, cb_se_ov_adj, OV_ADJ(+1));
  mk_btn_grow(rb, "OK",  OV_BTN_H, cb_se_ov_ok, NULL);
  lv_obj_t *huy = mk_btn_grow(rb, "H\xE1\xBB\xA7y", OV_BTN_H, cb_se_ov_cancel, NULL);  // "Huy"
  vn_font_btn(huy);             // chu co dau: chi doi font cho nhan BEN TRONG nut

  // Giu nut de tang nhanh: nhan them PRESSED + LONG_PRESSED_REPEAT cho 2 nut -/+
  lv_obj_t *adj[2] = { ov_minus, plus };
  const int8_t dir[2] = { -1, +1 };
  for (uint8_t k = 0; k < 2; k++) {
    lv_obj_add_event_cb(adj[k], cb_se_ov_adj, LV_EVENT_PRESSED, OV_ADJ(dir[k]));
    lv_obj_add_event_cb(adj[k], cb_se_ov_adj, LV_EVENT_LONG_PRESSED_REPEAT, OV_ADJ(dir[k]));
  }
}

// Mo lop phu cho 1 dong THAT trong SET_ROWS (i = chi so trong SET_ROWS[])
static void ov_open_edit(const uint8_t i) {
  if (i >= SET_NROWS) return;
  ov_i   = i;
  ov_raw = cydlcd_setting(SET_ROWS[ov_i].id, 0);
  if (!ov_box) ov_create();
  ov_step_k = ov_step_default(ov_i);      // buoc gan nhat voi 10^(-dec) cua dong
  set_text(ov_name, SET_ROWS[ov_i].name);
  ov_marks();
  ov_refresh();
  ov_open = true;
  ov_rep  = false;
  lv_obj_clear_flag(ov_bd, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(ov_box, LV_OBJ_FLAG_HIDDEN);
  lv_obj_update_layout(ov_box);           // ep tinh layout/flex ngay (khong doi vong refresh)
  if (s_group && ov_minus) lv_group_focus_obj(ov_minus);
}

// Mo lop phu cho dong dang HIEN thu k (0..se_n-1); chi so THAT lay qua se_idx[]
static void ov_open_row(const uint8_t k) {
  if (k >= se_n) return;
  ov_open_edit(se_idx[k]);
}

// P2: mo lop phu theo ID thong so (vi du nut "M851 Z" o man JOG) - khong can qua bang
static void ov_open_id(const uint16_t id) {
  const set_row_t *r = se_find(id);
  if (r) ov_open_edit((uint8_t)(r - SET_ROWS));
}

// Cham 1 o cua bang -> mo lop phu (hang 0 la dong tieu de nen bo qua)
static void cb_se_cell(lv_event_t *e) {
  (void)e;
  uint16_t row = 0, col = 0;
  if (!se_table) return;
  lv_table_get_selected_cell(se_table, &row, &col);
  if (row < 1 || row > (uint16_t)se_n) return;
  ov_open_row((uint8_t)(row - 1));
}

static void page_settings_create(lv_obj_t *p) {
  settings_build_list();
  se_info = mk_label_w(p, "Marlin gui: 0 thong so", COL_DIM, PAGE_INN_W);

  se_table = lv_table_create(p);
  lv_obj_set_size(se_table, PAGE_INN_W, SET_TABLE_H);
  lv_table_set_col_cnt(se_table, 2);
  lv_table_set_col_width(se_table, 0, SET_COL0_W);
  lv_table_set_col_width(se_table, 1, SET_COL1_W);
  lv_table_set_cell_value(se_table, 0, 0, "THONG SO");
  lv_table_set_cell_value(se_table, 0, 1, "GIA TRI");
  settings_fill_table();

  lv_obj_set_style_bg_color(se_table, COL_BG, 0);
  lv_obj_set_style_bg_opa(se_table, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(se_table, COL_DIM, 0);
  lv_obj_set_style_border_width(se_table, 1, 0);
  lv_obj_set_style_pad_all(se_table, 0, 0);
  lv_obj_set_style_text_color(se_table, COL_TXT, LV_PART_ITEMS);
  lv_obj_set_style_bg_color(se_table, COL_BTN, LV_PART_ITEMS);
  lv_obj_set_style_bg_opa(se_table, LV_OPA_COVER, LV_PART_ITEMS);
  lv_obj_set_style_border_color(se_table, COL_DIM, LV_PART_ITEMS);
  lv_obj_set_style_border_width(se_table, 1, LV_PART_ITEMS);
  lv_obj_set_style_pad_all(se_table, 2, LV_PART_ITEMS);
  lv_obj_set_style_text_color(se_table, COL_ACCENT, LV_PART_ITEMS | LV_STATE_FOCUSED);
  // Cham 1 o -> mo lop phu sua (-/+/OK). LVGL 8.3 gui VALUE_CHANGED khi NHAC tay (khong keo)
  lv_obj_add_event_cb(se_table, cb_se_cell, LV_EVENT_VALUE_CHANGED, NULL);

  // 1 HANG NUT o DAY bang, trai -> phai: UP | DOWN | SAVE EEPROM (2026-10-13)
  lv_obj_t *rb = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 30, 0, GAP);
  mk_btn_grow(rb, "UP",          28, cb_scroll_table, (void *)(intptr_t)(-48));
  mk_btn_grow(rb, "DOWN",        28, cb_scroll_table, (void *)(intptr_t)(48));
  mk_btn_grow(rb, "SAVE EEPROM", 28, cb_action, (void *)(intptr_t)A_SAVE);

  // M502 RESET: tra thong so trong RAM ve mac dinh cua firmware. XAC NHAN 2 BUOC (xem do_action).
  // Da BO nut "M501 LOAD" (2026-10-13, theo yeu cau): M501 chi doc lai thong so tu EEPROM vao RAM
  // (hoan tac thay doi chua luu) chu khong "load" tu file nao -> nhan "LOAD" gay hieu nham.
  lv_obj_t *r2 = mk_flex(p, LV_FLEX_FLOW_ROW, PAGE_INN_W, 28, 0, GAP);
  mk_btn_grow(r2, "M502 RESET (ve mac dinh)", 28, cb_action, (void *)(intptr_t)A_EEPROM_RESET);
}

static void page_settings_update(void) {
  char b[40], a[16];
  // 9 dong preheat do MAN HINH CYD tu quan ly (khong phai Marlin) -> bom lai truoc khi dung danh sach,
  // de sau M501/M502 (Marlin xoa bang) hay luc moi ket noi chung van con.
  preheat_inject();
  // Neu so thong so nhan duoc doi (HELLO moi / bam lai) -> dung lai danh sach dong co gia tri
  static uint8_t last_n = 255;
  if (last_n != cydlcd.n_settings) {
    last_n = cydlcd.n_settings;
    settings_build_list();
    settings_fill_table();
  }

  for (uint8_t k = 0; k < se_n; k++) {
    const uint8_t i = se_idx[k];
    const int32_t v = cydlcd_setting(SET_ROWS[i].id, INT32_MIN);
    fmt_scaled(a, sizeof(a), v, SET_ROWS[i].scale, SET_ROWS[i].dec);
    // chi set khi khac -> khong nhay + khong day SPI vo ich
    const char *old = lv_table_get_cell_value(se_table, (uint16_t)(k + 1), 1);
    if (!old || strcmp(old, a) != 0)
      lv_table_set_cell_value(se_table, (uint16_t)(k + 1), 1, a);
  }
  snprintf(b, sizeof(b), "cham 1 dong de SUA - %u/%u thong so", (unsigned)se_n, (unsigned)SET_NROWS);
  set_text(se_info, b);

  // Lop phu SUA co the dang mo -> ve lai dau "buoc dang chon" moi chu ky (giong jog_axis_mark()
  // o man JOG): focus va "dang chon" la 2 trang thai doc lap, ov_marks() chi ghi khi mau doi.
  // Chay ca khi lop phu dang AN: ov_step_btn[] khong doi, khong ton gi them.
  ov_marks();
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

// ===========================================================================
//  CUSTOM -- G34/Init SD (nhu menu cu) + P4 endstop M119 + P6 filament + P8 DFU
//  (MENU da het cho: 10 dong x 22 = 220 > 206 nen cac muc moi nam o day)
// ===========================================================================
static lv_obj_t *cu_runout;                     // nut toggle M412 (P6)

// P6: bat/tat cam bien het filament. Trang thai dau lay tu SETTING RUNOUT_EN, sau do
// dao moi lan bam (Marlin khong day lai ngay trang thai nay trong frame STATUS).
static bool s_runout_on = true;

static void runout_mark(void) {
  if (!cu_runout) return;
  set_text(lv_obj_get_child(cu_runout, 0), s_runout_on ? "M412: BAT (S1)" : "M412: TAT (S0)");
  lv_obj_set_style_bg_color(cu_runout, s_runout_on ? COL_OK : COL_BTN, 0);
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
  // Trang thai M412: dong bo lan dau tu SETTING (neu Marlin co gui CYDLCD_SET_RUNOUT_EN)
  static bool got = false;
  if (!got) {
    const int32_t v = cydlcd_setting(CYDLCD_SET_RUNOUT_EN, -1);
    if (v >= 0) { got = true; s_runout_on = (v != 0); runout_mark(); }
  }
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
  char b[40];
  for (uint8_t i = 0; i < CYDLCD_ENDSTOP_MAX; i++) {
    const bool hit = (cydlcd.es_hit[ES_ID[i]] != 0);
    set_text(es_val[i], hit ? "CHAM (triggered)" : "mo (open)");
    lv_obj_set_style_text_color(es_val[i], hit ? COL_ERR : COL_OK, 0);
  }
  snprintf(b, sizeof(b), "M119: %u o - bam REFRESH de doc lai", (unsigned)cydlcd.n_endstops);
  set_text(es_info, b);
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
  if (s_wifi_busy || !wifi_api_ok()) return;
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
  if (!wf_state) return;

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

// ===========================================================================
//  MENU -- menu doc kieu Marlin (nut SETTINGS tren man STATUS mo vao day)
//  Moi dong: [ten vang] .......... [gia tri trang]; dong la 1 nut (encoder bam = mo)
// ===========================================================================
#define MENU_ROWS 11
typedef struct { const char *name; uint8_t page; } menu_item_t;
static const menu_item_t MENU_ITEMS[MENU_ROWS] = {
  { "Temperature",     PG_TEMP },
  { "Motion (JOG)",    PG_JOG },
  { "Feed/Flow rate",  PG_RATE },
  { "SD card",         PG_FILES },
  { "Bed mesh",        PG_MESH },
  { "Custom G34/SD",   PG_CUSTOM },
  { "Settings",        PG_SETTINGS },
  { "Tramming wizard", PG_TBT },
  { "Info",            PG_INFO },
  { "Endstops (M119)", PG_ENDSTOP },
  // Muc 11: WIFI (mo tu day HOAC bam icon WiFi o header/man STATUS).
  // MENU gio 11 muc + dong "Calibrate touch" = 12 hang x 22 px = 264 > 206 -> thanh day
  // PREV/NEXT xuat hien va phan trang (TRUOC DAY da vay voi 10+1 hang = 242 px, nen khong doi gi).
  { "WiFi",            PG_WIFI }
};
static lv_obj_t *mn_val[MENU_ROWS];
static lv_obj_t *mn_val_cal;                 // dong "Calibrate touch" (chi co tren board co touch)

static void cb_menu_row(lv_event_t *e) {
  show_page((uint8_t)(intptr_t)lv_event_get_user_data(e));
}

// "Calibrate touch": chay trinh hieu chuan 4 goc cua port (blocking, tu ve bang TFT_eSPI),
// sau do xoa toan bo man hinh de LVGL ve lai tu dau.
//
// CHONG BAM NHAM: phai GIU TAY >= CAL_TOUCH_HOLD_MS roi NHAC moi vao; 1 lan cham ngan
// (lo bam trong MENU) khong lam gi. Ly do phai chan: trinh hieu chuan la blocking, moi goc
// CHI CAN 1 lan cham la duoc nhan, va no GHI NGAY vao NVS -> bam nham co the lam hong cam ung
// (khi do chi con duong giu tay luc cap dien de vao lai).
#define CAL_TOUCH_HOLD_MS 1500
static uint32_t s_cal_hold_ms = 0;      // >0 = dang giu tay tren dong nay
static bool     s_cal_req     = false;  // da giu du -> ui_lvgl_task() se chay hieu chuan

static void cb_cal_touch(lv_event_t *e) {
  switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
      s_cal_hold_ms = ui_port_millis();
      if (!s_cal_hold_ms) s_cal_hold_ms = 1;               // 0 = "khong giu"
      break;

    case LV_EVENT_RELEASED: {
      const uint32_t held = s_cal_hold_ms ? (ui_port_millis() - s_cal_hold_ms) : 0;
      s_cal_hold_ms = 0;
      if (held >= CAL_TOUCH_HOLD_MS) s_cal_req = true;     // du lau -> cho chay o ui_lvgl_task()
      break;
    }

    // Keo/scroll lam mat diem bam -> coi nhu huy giu
    case LV_EVENT_PRESS_LOST:  s_cal_hold_ms = 0; break;

    default: break;                                        // LV_EVENT_CLICKED (cham ngan) -> bo qua
  }
}

//  MENU la 1 man BINH THUONG: co dong tieu de chung o tren (ten man + LINK + BACK ben phai),
//  cac dong menu do FLEX cua mk_page() tu xep doc (khong can toa do y).
static void page_menu_create(lv_obj_t *p) {
  // 10 dong menu + (tren board co touch) 1 dong "Calibrate touch" = 11 dong x 20 px = 220 > 206,
  // nen MENU luon co thanh day de phan trang (xem bot_sync; trang bi thu nho con 192 px -> 184 trong).
  // mk_page() dat pad_row = GAP cho moi man, o day phai de 0 thi moi du (cac dong la o dac lien
  // nhau - giong danh sach menu cua Marlin; dong dang chon duoc to sang + vien xanh).
  lv_obj_set_style_pad_row(p, 0, 0);

  for (uint8_t i = 0; i < MENU_ROWS; i++) {
    lv_obj_t *r = mk_hit(p, 0, 0, PAGE_INN_W, MENU_ROW_H - 2,
                         cb_menu_row, (void *)(intptr_t)MENU_ITEMS[i].page);
    lv_obj_set_style_bg_color(r, COL_BTN, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_t *n = mk_label(r, MENU_ITEMS[i].name, MR_MENU);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, 0);
    mn_val[i] = mk_label(r, "", MR_MENUVAL);
    lv_obj_align(mn_val[i], LV_ALIGN_RIGHT_MID, -4, 0);
  }

  // Dong hieu chuan cam ung: chi hien tren board co touch (CYD).
  // PHAI GIU 1.5s (xem cb_cal_touch) -> 3 event rieng, khong dung CLICKED cua mk_hit.
  if (ui_port_touch_cal_available()) {
    lv_obj_t *r = mk_hit(p, 0, 0, PAGE_INN_W, MENU_ROW_H - 2, cb_cal_touch, NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_PRESSED,    NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_RELEASED,   NULL);
    lv_obj_add_event_cb(r, cb_cal_touch, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_set_style_bg_color(r, COL_BTN, 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_t *n = mk_label(r, "Calibrate touch (giu 1.5s)", MR_MENU);
    lv_obj_align(n, LV_ALIGN_LEFT_MID, 4, 0);
    mn_val_cal = mk_label(r, "4 goc", MR_MENUVAL);
    lv_obj_align(mn_val_cal, LV_ALIGN_RIGHT_MID, -4, 0);
  }

  // KHONG ve nut BACK o day nua: da co nut BACK o goc phai dong tieu de (build_chrome)
}

static void page_menu_update(void) {
  char b[24];
  // gia tri ben phai tung dong (giong cot "value" trong menu Marlin)
  int16_t cur = 0, tgt = 0;
  temp_lookup(CYDLCD_TEMP_E0, &cur, &tgt);
  char a1[12];
  fmt_scaled(a1, sizeof(a1), cur, 100, 0);
  snprintf(b, sizeof(b), "%s C", a1);
  set_text(mn_val[0], b);

  snprintf(b, sizeof(b), "X%u Y%u Z%u",
           (unsigned)((cydlcd.flags & CYDLCD_SF_HOMED) ? 1 : 0),
           (unsigned)((cydlcd.flags & CYDLCD_SF_HOMED) ? 1 : 0),
           (unsigned)((cydlcd.flags & CYDLCD_SF_HOMED) ? 1 : 0));
  set_text(mn_val[1], b);

  snprintf(b, sizeof(b), "%ld/%ld%%",
           (long)(cydlcd_setting(CYDLCD_SET_FEEDRATE_PCT, 10000) / 100),
           (long)(cydlcd_setting(CYDLCD_SET_FLOW_PCT, 10000) / 100));
  set_text(mn_val[2], b);

  snprintf(b, sizeof(b), "%u file%s", (unsigned)cydlcd.n_files, cydlcd.files_done ? "" : "...");
  set_text(mn_val[3], b);

  if (cydlcd.hello_flags & CYDLCD_HAS_MESH)
    snprintf(b, sizeof(b), "%ux%u", (unsigned)cydlcd.mesh_nx, (unsigned)cydlcd.mesh_ny);
  else snprintf(b, sizeof(b), "n/a");
  set_text(mn_val[4], b);

  set_text(mn_val[5], "G34/M22");
  snprintf(b, sizeof(b), "%u/33", (unsigned)cydlcd.n_settings);
  set_text(mn_val[6], b);

  // Tramming wizard: hien trang thai WIZARD (khong phai chi bao link - chi bao link o THANH TREN).
  // Chua co dong TBT nao / mat link thi ghi ro de biet duong nao chua thong.
  if (!link_ok()) snprintf(b, sizeof(b), "%s", tbt.ever ? "mat link" : "chua bat");
  else if (!tbt.st.active) snprintf(b, sizeof(b), "san sang");
  else snprintf(b, sizeof(b), "%s %s", tbt.st.mode_max ? "MAX" : "MIN", tbt_phase_name(tbt.st.phase));
  set_text(mn_val[7], b);

  // Duong dang dung (PUSH / TRAM / chua co) - KHONG hien lai trang thai link o day,
  // vi chi bao link chi nam o THANH TREN (xem link_ok()).
  snprintf(b, sizeof(b), "%s", tbt.ever ? "TBT" : (cydlcd.linked ? "PUSH" : "-"));
  set_text(mn_val[8], b);

  // Muc 10 "Endstops (M119)": man chi hien ket qua M119 nen o day chi ghi lenh da gui
  set_text(mn_val[9], "M119");
  // Muc 11 "WiFi": hien trang thai ngan (TAT / DANG KET NOI / DA KET NOI / khong ho tro)
  if (!wifi_api_ok()) set_text(mn_val[10], "n/a");
  else {
    static const char *const WST[3] = { "TAT", "dang noi", "DA NOI" };
    const uint8_t ws = wifi_state_now();
    set_text(mn_val[10], WST[ws <= 2 ? ws : 0]);
  }
  // Dong "Calibrate touch": thuong hien nguon gia tri (NVS / mac dinh); khi dang GIU tay thi
  // hien tien do de nguoi dung biet da du 1.5s chua.
  if (s_cal_hold_ms)
    set_text(mn_val_cal, (ui_port_millis() - s_cal_hold_ms) >= CAL_TOUCH_HOLD_MS ? "NHAC TAY" : "dang giu");
  else
    set_text(mn_val_cal, ui_port_touch_cal_is_saved() ? "NVS" : "mac dinh");
}

// ===========================================================================
//  Thanh day: phan trang noi dung man dang mo (PREV / TRANG x/y / NEXT)
// ===========================================================================
// CHIEU (da kiem chung bang ma nguon LVGL 8.4, khong phai doan):
//   lv_obj_get_scroll_top(o) = -o->spec_attr->scroll.y            (lv_obj_scroll.c:128)
//   lv_obj_scroll_to_y(o, y) dat scroll.y = -y  =>  scroll_top = y
//   lv_obj_scroll_by(o, 0, dy) cong thang vao scroll.y (lv_obj_scroll.c:334 dat anim
//       tu -sy den -sy+dy, anim ghi scroll.y = v)  =>  scroll_top_moi = scroll_top_cu - dy
//   => muon di XUONG (scroll_top TANG) phai truyen dy = -vis. Code cu truyen +vis cho NEXT
//      nen scroll_top GIAM: NEXT lai di VE DAU danh sach (dung nhu nguoi dung bao
//      "next ... cuon nguoc"). Da sua: dung lv_obj_scroll_to_y() voi dich TINH TRUOC.
//   PREV = ve phia TRUOC (dau danh sach, scroll_top giam) | NEXT = ve phia SAU (scroll_top tang).
// Ngoai ra lv_obj_scroll_by() la ban KHONG kep bien (co the tran ra khoang trang o 2 dau);
// lv_obj_scroll_to_y() di qua ban bounded nen tu dung lai o dau/cuoi.
//
// FOCUS: lv_btn (moi dong menu deu la lv_btn) co LV_OBJ_FLAG_SCROLL_ON_FOCUS (lv_btn.c:67) va
// LVGL tu goi lv_obj_scroll_to_view_recursive() khi vat nhan duoc su kien FOCUSED
// (lv_obj.c:800-803). Neu sau khi cuon ma con tro encoder VAN o dong cua trang CU thi
// lan di chuyen focus ke tiep se keo ca trang VE cho thay dong do -> nhin nhu vua cuon nguoc.
// Vi vay sau khi cuon phai chuyen focus sang vat focusable DAU TIEN nam trong vung nhin MOI.
// Tim vat focusable DAU TIEN (theo thu tu cay) dang GIAO voi vung nhin MOI [band_lo, band_hi]
// (toa do tuyet doi tren man).
// "Focusable" = CO TRONG GROUP, chu KHONG phai co LV_OBJ_FLAG_CLICK_FOCUSABLE: lv_group_focus_obj()
// chi doi hoi vat nam trong group (no duyet danh sach obj_ll cua group - lv_group.c:235), va co
// vat duoc them vao group ma khong co co do (vd bang SETTINGS: `lv_group_add_obj(s_group, me_table)`).
static lv_obj_t *find_focus_in_band(lv_obj_t *root, lv_coord_t band_lo, lv_coord_t band_hi)
{
  const uint32_t n = lv_obj_get_child_cnt(root);
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t *c = lv_obj_get_child(root, i);
    if (!c || lv_obj_has_flag(c, LV_OBJ_FLAG_HIDDEN)) continue;
    if (s_group && lv_obj_get_group(c) == s_group) {
      // lv_obj_get_coords() tra toa do TUYET DOI tren man (LVGL da cong scroll) - dung luon,
      // khong can biet vat long bao nhieu cap. band_* cung la toa do tuyet doi.
      lv_area_t a;
      lv_obj_get_coords(c, &a);
      if (a.y2 > band_lo && a.y1 < band_hi) return c;     // giao voi vung nhin MOI
    }
    lv_obj_t *r = find_focus_in_band(c, band_lo, band_hi);
    if (r) return r;
  }
  return NULL;
}

// Cuon noi dung man dang mo them 1 "trang" (dung lai o 2 dau). dir > 0 = NEXT (xuong cuoi),
// dir < 0 = PREV (ve dau). Dung chung cho MOI man con (khong rieng MENU).
static void page_scroll(const int8_t dir) {
  lv_obj_t *p = s_page[s_page_act];
  if (!p || s_page_act == PG_STATUS) return;
  const lv_coord_t vis = lv_obj_get_height(p) - 2 * PAGE_PAD;
  if (vis <= 0) return;

  const lv_coord_t top = lv_obj_get_scroll_top(p);
  const lv_coord_t all = top + lv_obj_get_scroll_bottom(p);   // tong doan cuon duoc (co dinh)
  lv_coord_t want = top + ((dir > 0) ? vis : -vis);
  if (want < 0)   want = 0;                                   // da o dau -> khong cuon them
  if (want > all) want = all;                                 // da o cuoi -> khong cuon them
  if (want == top) return;

  lv_obj_scroll_to_y(p, want, LV_ANIM_ON);                    // scroll_top = want (chinh xac)

  // Vat nao dang o trong vung nhin MOI? Tinh theo toa do NOI DUNG (khong doi theo scroll):
  //   content_ofs = coords.y1 - p->coords.y1 + scroll_top_hien_tai
  // Sau khi cuon, vat nam trong khung khi content_ofs thuoc [want, want + chieu cao trang].
  // (Phai tinh tay vi scroll dang chay ANIMATION nen coords chua cap nhat.)
  if (!s_group) return;
  const lv_coord_t shift = top - want;                        // doi coords CU -> coords SAU khi cuon
  lv_area_t pa;
  lv_obj_get_coords(p, &pa);
  const lv_coord_t band_lo = pa.y1 - shift;
  const lv_coord_t band_hi = pa.y2 - shift;
  lv_obj_t *nf = find_focus_in_band(p, band_lo, band_hi);
  if (!nf || nf == lv_group_get_focused(s_group)) return;     // khong tim duoc -> giu nguyen focus

  // lv_group_focus_obj() se ban FOCUSED -> LVGL goi lv_obj_scroll_to_view_recursive(nf) theo
  // toa do CU cua nf, tuc la keo trang NGUOC LAI. Tam bo co roi bat lai de tranh.
  const bool sof = lv_obj_has_flag(nf, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  if (sof) lv_obj_clear_flag(nf, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
  lv_group_focus_obj(nf);          // => on_focus_changed() cap nhat s_focus_prev + ve lai khung
  if (sof) lv_obj_add_flag(nf, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
}

// Bat/tat thanh day theo noi dung THUC TE cua man dang mo:
//  - man nao noi dung da nam gon trong vung nhin => KHONG hien thanh day (giu nguyen bo cuc 206 px nhu cu)
//  - man nao tran => thu nho vung noi dung di BOT_H va hien thanh day de phan trang
// Goi o CUOI show_page() (luc trang da HIEN) va dinh ky trong ui_lvgl_task() (~400 ms).
// `remeasure = true`: CHI goi khi VUA VAO man (xem show_page). Luc do trang dang o dau danh sach
// (scroll = 0) nen co the tam tra chieu cao ve CONT_H de do, roi thu lai - khong lam nhay vi tri cuon.
// `remeasure = false`: chi cap nhat nhan/mo nut, KHONG doi chieu cao trang: doi chieu cao se lam
// LVGL KEP `scroll_y` ve muc toi da cu => moi 400 ms trang tu nhay ve dau, bam NEXT khong giu duoc.
static void bot_sync(const bool remeasure) {
  if (!s_bot) return;
  const uint8_t p = s_page_act;
  lv_obj_t *pg = s_page[p];
  const bool chrome = (p != PG_STATUS);          // STATUS khong co header/thanh day
  if (!pg || !chrome) {
    // STATUS dung kin 320x240 -> khong doi kich thuoc trang, chi dam bao thanh day dang AN
    if (s_bot_shown) s_bot_shown = false;
    lv_obj_add_flag(s_bot, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  if (remeasure) {
    // PHAI do khi trang dang HIEN (LVGL bo qua tinh layout cho vat HIDDEN).
    // Do o CHIEU CAO DAY DU (CONT_H) truoc: neu noi dung da gon trong 206 px thi khong can
    // thanh day; do o 192 px thi noi dung ngan cung bi coi la "tran" gia.
    lv_obj_scroll_to_y(pg, 0, LV_ANIM_OFF);            // vao man -> xem tu dau
    if (lv_obj_get_height(pg) != CONT_H) {
      lv_obj_set_height(pg, CONT_H);
      lv_obj_update_layout(pg);
    }
    // LVGL cong ca pad_bottom vao `scroll_bottom` => noi dung vua khop 206 px van bao tran ~4 px.
    // Vi vay phai so voi nguong PAGE_PAD (khong dung > 0), neu khong cac man dang gon se bi hien thanh day.
    const lv_coord_t ovf = lv_obj_get_scroll_top(pg) + lv_obj_get_scroll_bottom(pg);
    const bool need = (ovf > PAGE_PAD);
    if (need) lv_obj_set_height(pg, CONT_H - BOT_H);   // 192: nhuong 22 px cho thanh day
    lv_obj_update_layout(pg);
    if (need != s_bot_shown) {
      s_bot_shown = need;
      if (need) lv_obj_clear_flag(s_bot, LV_OBJ_FLAG_HIDDEN);
      else      lv_obj_add_flag(s_bot, LV_OBJ_FLAG_HIDDEN);
    }
  }

  const bool need = s_bot_shown;
  if (!need) return;

  // nhan "TRANG x/y" + AN HAN nut nao khong con gi de cuon (truoc day chi lam mo bang OPA_40 ->
  // nguoi dung van tuong la bam duoc). Thanh day la flex ROW [PREV][nhan grow=1][NEXT]: LVGL bo
  // qua vat HIDDEN khi xep flex nen nhan tu gian ra, con lai van nam sat bien (trai/phai).
  // `top + bot` = TONG doan cuon duoc (khong doi theo vi tri dang dung) nen so trang dem theo
  // moc nhay tung trang, khop voi trang thai hien/an cua 2 nut (o dau: top=0, o day: bot=0).
  const lv_coord_t vis = lv_obj_get_height(pg) - 2 * PAGE_PAD;
  const lv_coord_t top = lv_obj_get_scroll_top(pg), bot = lv_obj_get_scroll_bottom(pg);
  const int npg = (vis > 0) ? (1 + (int)((top + bot + vis - 1) / vis)) : 1;
  const int cur = (vis > 0) ? (1 + (int)((top       + vis - 1) / vis)) : 1;
  char b[24]; snprintf(b, sizeof(b), "TRANG %d/%d", cur, npg);
  set_text(s_bot_info, b);

  // Chi doi CO khi trang thai doi (bit0 = PREV hien, bit1 = NEXT hien) -> khong relayout moi 400 ms.
  // nav == 0 (ca 2 cung khong the di) khong the toi day: thanh day chi hien khi noi dung TRAN
  // (ovf > PAGE_PAD), ma khi do thi top > 0 hoac bot > 0. Van chan them cho chac.
  const uint8_t nav = (uint8_t)((top > 0 ? 1U : 0U) | (bot > 0 ? 2U : 0U));
  if (nav && nav != s_bot_nav) {
    s_bot_nav = nav;
    if (nav & 1U) lv_obj_clear_flag(s_bot_prev, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag(s_bot_prev, LV_OBJ_FLAG_HIDDEN);
    if (nav & 2U) lv_obj_clear_flag(s_bot_next, LV_OBJ_FLAG_HIDDEN);
    else          lv_obj_add_flag(s_bot_next, LV_OBJ_FLAG_HIDDEN);
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
    default: break;
  }
  lv_obj_add_flag(s_page[p], LV_OBJ_FLAG_HIDDEN);      // se duoc hien boi show_page()
}

static void show_page(uint8_t p) {
  if (p >= PG_COUNT) p = PG_STATUS;
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

  // Lop phu SUA thong so chi thuoc man SETTINGS -> doi man thi dong luon
  if (p != PG_SETTINGS) ov_hide();

  // Roi trang WIFI -> don ban phim nhap mat khau + lop phu quet (con cua lv_scr_act, khong phai
  // con cua trang) va KHONG dung tÃƒÆ’Ã†â€™Ãƒâ€ Ã¢â‚¬â„¢ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â¡ÃƒÆ’Ã†â€™ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â»ÃƒÆ’Ã†â€™Ãƒâ€šÃ‚Â¢ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã…Â¡Ãƒâ€šÃ‚Â¬ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Âºi WiFi. Vao trang WIFI -> chi ghi log (khong tu quet mang).
  if (p != PG_WIFI) wifi_page_leave();
  else if (s_page_act != PG_WIFI) WIFILOG("[WIFI] vao trang WIFI (chi doc trang thai, khong quet)\n");

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
//  Icon trang thai WiFi: 1 icon o HEADER (font nho 14 px) + 1 icon TO tren man STATUS.
//   wifi_state_now(): 0 = WiFi tat / chua cau hinh / khong co module
//                     1 = DANG ket noi (ke ca dang thu lai) -> DO
//                     2 = DA ket noi                         -> XANH
//   sd_state_now()  : 0 = chua co module (weak NULL) -> XAM, 1 = khong co the/loi -> DO,
//                     2 = the dang dung duoc -> XANH
//  Font: ban NHO (gfx4_font_icons_get) cho header; ban TO (gfx4_font_icons_big_get) cho STATUS.
//  Thieu font nao thi lui ve ban con lai, thieu ca 2 -> CHAM TRON 12x12 (build van chay).
// ===========================================================================
// ---------------------------------------------------------------------------
//  Be rong CO DINH cua cac vat tren thanh tieu de (xem bang tinh o build_chrome):
//    LINK_LBL_W : o ten may / trang thai link. Phai >= 64 px vi "LINK ERR" = 8 ky tu mono x 8 px;
//                 ten may dai hon thi TU CUON trong o (khong day nut nao ra ngoai).
//    TITLE_W    : o TEN MAN. Ten dai nhat la "FEED/FLOW" (~81 px voi Montserrat 14) nen 84 px la
//                 du; o nay CO DINH nen do dai ten khong lam xo lech thanh tieu de.
//    HOME_BTN_W : nut icon ngoi nha (glyph 12x10, xAdvance 14) - 26 x 20 nhu 2 nut II/[].
// ---------------------------------------------------------------------------
#define LINK_LBL_W   64                // truoc la 78 (nhuong cho cho nut HOME + o ten man)
#define TITLE_W      84
#define HOME_BTN_W   26
// Duong lui (khong co font icon, hoac vang ban to): CHAM TRON 12x12. Dinh nghia VO DIEU KIEN vi
// mk_wifi_icon() kiem tra font luc CHAY (co the co ban nho ma khong co ban to).
#define WIFI_ICON_SZ 12
// "LINK ERR" chi can 8x8 = 64 px (font mono advance 8) nen 78/80 px van hien du; ten may dai hon
// thi tu cuon vong nhu truoc (xem mk_label_w).
static lv_obj_t  *s_wifi;                  // glyph o HEADER (hoac hinh tron khi chua co font icon)
static lv_obj_t  *s_st_sd_icon;            // icon THE 64x64 CO SAN cua Marlin (o SD man STATUS)
static lv_obj_t  *s_st_wifi;               // glyph WiFi TO tren man STATUS (ban to 30 px)
static uint8_t    s_wifi_drawn = 0xFF;     // trang thai WiFi dang ve (dung CHUNG cho ca 2 noi)
static uint8_t    s_sd_icon_st = 0xFE;     // mau icon the dang to (0xFE = chua to lan nao)

// Font icon dung cho 1 vat: big=true -> ban TO (man STATUS), false -> ban NHO (header).
// Tra NULL khi KHONG co ban font tuong ung -> noi goi se ve cham tron (duong lui).
static const lv_font_t *wifi_icon_font(const bool big) {
#if UI_HAVE_ICON_FONT_BIG
  if (big) return gfx4_font_icons_big_get();
#else
  (void)big;                                 // khong co ban to -> luon dung ban nho
#endif
#if UI_HAVE_ICON_FONT
  return gfx4_font_icons_get();
#else
  return NULL;
#endif
}

// Vat chi bao: KHONG cho bam (khong nhan touch) va KHONG vao group encoder.
// f != NULL -> label dung font icon f; f == NULL -> cham tron 12x12 (duong lui).
static lv_obj_t *mk_wifi_icon(lv_obj_t *parent, const lv_font_t *f) {
  lv_obj_t *o;
  if (f) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, COL_DIM, 0);              // mac dinh: xam = WiFi tat
    lv_label_set_text(l, WIFI_GLYPH_OFF);
    // KHONG dat be rong: label tu co theo glyph cua font (ban nho: 14x12; ban to: <= 30 px).
    lv_obj_clear_flag(l, LV_OBJ_FLAG_SCROLLABLE);
    o = l;
  }
  else {
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, WIFI_ICON_SZ, WIFI_ICON_SZ);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(c, COL_DIM, 0);      // mac dinh: xam = WiFi tat (KHONG phai mau loi)
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_shadow_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    o = c;
  }
  // BAM ICON WIFI -> mo TRANG WIFI (truoc day icon khong bam duoc). Vao group nen encoder cung
  // bam duoc. Vat nay nho (14x12 o header / 30x25 o STATUS) nen KHONG chan vung bam nao khac:
  // o header nam trong hang flex, o STATUS nam trong khe x 64..95 (khong de o SD / o nhiet).
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_add_event_cb(o, cb_action, LV_EVENT_CLICKED, (void *)(intptr_t)A_GOTO_WIFI);
  if (s_group) lv_group_add_obj(s_group, o);
  return o;
}

// Ap trang thai WiFi (0..2) len 1 vat: mau + glyph. NULL-safe, dung CHUNG cho icon o header (font
// nho) va icon tren man STATUS (font to) - 2 noi luon cung trang thai nen chi can 1 cache.
// Vat co the la LABEL (glyph) hoac lv_obj (cham tron duong lui) -> kiem kieu luc chay de dat dung
// thuoc tinh (mau chu cho label / mau nen cho cham). Dat ca 2 loai mau la vo hai voi loai con lai.
static void wifi_glyph_apply(lv_obj_t *o, const uint8_t st) {
  if (!o) return;
  // 1 = dang ket noi -> DO, 2 = da ket noi -> XANH, 0 = tat / chua cau hinh -> XAM.
  const lv_color_t col = (st == 2) ? COL_OK : ((st == 1) ? COL_ERR : COL_DIM);
  lv_obj_set_style_text_color(o, col, 0);        // label: mau chu
  lv_obj_set_style_bg_color(o, col, 0);          // cham tron: mau nen
  // Glyph chi doi voi label: 2 = wifi (fan dac), 1 = 3 vach (dang thu), 0 = wifi_off (gach cheo).
  if (lv_obj_check_type(o, &lv_label_class))
    lv_label_set_text(o, (st == 2) ? WIFI_GLYPH_ON : ((st == 1) ? WIFI_GLYPH_CONN : WIFI_GLYPH_OFF));
}

// Cap nhat icon WiFi. Goi o nhip CHAM cua ui_lvgl_task (~400 ms), KHONG phai moi vong lap, va
// CHI ghi style khi trang thai DOI (giong s_bot_nav / jog_axis_mark) -> khong invalidate lien tuc.
// Giu icon LUON HIEN (khong an khi trang thai 0) de flex cua dong tieu de khong phai xep lai moi
// lan WiFi bat/tat; mau xam mo the hien "khong co WiFi", khong phai "loi".
static void wifi_icon_mark(void) {
  const uint8_t st = wifi_state_now();       // cung nguon voi dong "WIFI:" o man INFO
  if (st == s_wifi_drawn) return;
  s_wifi_drawn = st;
  wifi_glyph_apply(s_wifi, st);              // icon o header
  wifi_glyph_apply(s_st_wifi, st);           // icon tren man STATUS (cung 1 cache)
}

// ===========================================================================
//  Icon WiFi TO tren man STATUS (font icon BAN TO 30 px - to bang icon FR/FL 32x32).
//
//  So do THAT cua font to (nhanh font do): size 30, xAdvance 30, yAdvance 25, base_line 0;
//  box net 28x20 (wifi) / 28x23 (wifi_off) / 30x21 (3-vach, 1-vach) -> label sinh ra la 30x25 px.
//  KHONG duoc dat lv_obj_set_height() cung cho label: driver day net len MEP TREN hop dong, chi
//  hop khit net moi canh giua dung (o day lai con dung lv_obj_set_pos nen khong can dat gi).
//
//  DAI DAT ICON: x 64..95, y 168..197 (rong 32, CAO 30 - gioi han cung):
//    - icon THE (0,130,64,64) -> x 0..63                | nhan thoi gian (96,176,128,20) -> x >= 96
//    - bitmap icon FEED nam o y 136..167                | thanh tien do (4,198,312,9) -> y >= 198
//  => dat 32 px se de len FEED (2 px) hoac thanh tien do (2 px) -> dung y 168..197, cao toi da 30.
//  Toa do chon: (65,170) -> hop 65..94 x 170..194 (nam gon trong dai, cach FEED 3 px, cach thanh
//  tien do 4 px); y duoc TINH tu chieu cao that (f->line_height) de canh giua: ban to 25 -> 170;
//  ban nho 12 -> 177; cham tron 12 -> 177. Man STATUS khong dung flex (mk_page_full).
// ===========================================================================
#define ST_WIFI_X        65              // ngay ben phai icon the (ket thuc x = 63)
#define ST_ICON_BAND_TOP 168             // dai trong: 168..197 (cao 30)
#define ST_ICON_BAND_H   30

// Trang thai dac biet: dang in / tam dung tu the SD -> GIU NGUYEN mau cu cua icon the.
#define SD_ICON_KEEP 0xFF

// Tao vat trang thai cho man STATUS - goi 1 LAN trong page_status_create() (xem forward decl).
//  - sd_icon: icon THE CO SAN cua Marlin (o SD) - chi DOI MAU, khong ve them gi.
//  - icon WiFi: tao moi bang FONT TO (ban nho neu chua co ban to), canh giua trong dai 168..197.
// Invalidate ca 2 cache de nhip cham ke tiep ve dung trang thai ngay.
static void status_icons_create(lv_obj_t *p, lv_obj_t *sd_icon) {
  s_st_sd_icon = sd_icon;
  const lv_font_t *f = wifi_icon_font(true);        // ban TO; NULL -> cham tron / ban nho
  s_st_wifi = mk_wifi_icon(p, f);
  // Chieu cao that: label = line_height cua font; cham tron = WIFI_ICON_SZ.
  const lv_coord_t gh = f ? f->line_height : WIFI_ICON_SZ;
  lv_coord_t y = (lv_coord_t)(ST_ICON_BAND_TOP + (ST_ICON_BAND_H - gh) / 2);
  if (y < ST_ICON_BAND_TOP) y = ST_ICON_BAND_TOP;    // an toan: khong bao gio vuot len tren
  lv_obj_set_pos(s_st_wifi, ST_WIFI_X, y);

  s_sd_icon_st = 0xFE;      // 0xFE = chua to lan nao (0xFF danh cho "dang in -> giu mau cu")
  s_wifi_drawn = 0xFF;
}

// Doi mau icon THE co san theo trang thai the TF cua MAN HINH CYD (nhip cham 400 ms, chi ghi khi doi -
// icon la bitmap 64x64 nen moi lan doi mau la ve lai 64x64 pixel qua SPI).
//
// THU TU UU TIEN:
//   1) Dang IN / TAM DUNG tu the SD (CYDLCD_SF_SD_PRINTING | CYDLCD_SF_PAUSED): GIU NGUYEN mau cu
//      (MR_TIME - dung nhu truoc day, vi luc do the dang duoc Marlin doc lien tuc nen trang thai
//      the MAN HINH CYD khong con la thong tin chinh; va day cung la hanh vi cu, tranh doi hinh).
//   2) Khi ranh: to theo the TF (cyd_sd_ready(), the o khe man hinh):
//        OK (the dung duoc)            -> COL_OK  (xanh)
//        co module nhung chua mount/loi -> COL_ERR (do)
//        accessor vang / khong co module (weak NULL) -> COL_DIM (xam, KHONG phai do)
static void status_sd_icon_mark(void) {
  uint8_t st;
  if (cydlcd.flags & (CYDLCD_SF_SD_PRINTING | CYDLCD_SF_PAUSED)) st = SD_ICON_KEEP;
  else                                                        st = sd_state_now();
  if (st == s_sd_icon_st) return;
  s_sd_icon_st = st;
  icon_color(s_st_sd_icon, (st == SD_ST_OK)   ? COL_OK
                         : (st == SD_ST_NO)   ? COL_ERR
                         : (st == SD_ST_NONE) ? COL_DIM
                                              : MR_TIME);   // SD_ICON_KEEP -> mau cu
}

// ===========================================================================
//  Dong tieu de cho cac man con:
//    [HOME] [ten man] ...... [ten may] [wifi] [II] [[]] [BACK]
//  (STATUS khong dung thanh nay; MENU dung nhung co bo cuc rieng cho phan noi dung)
//
//  BE RONG CO DINH (pad 4+4, gap 3 moi khe - xem pad_column o duoi):
//    (a) binh thuong (II/[] AN - flex bo qua vat HIDDEN):  6 vat, 5 khe
//        8 + 5*3 + (26 + TITLE_W + 64 + 14 + 48) = 8 + 15 + 236 = 259 -> spacer = 61 px
//    (b) dang in (II/[] HIEN): 8 vat, 7 khe
//        8 + 7*3 + (26 + TITLE_W + 64 + 14 + 26 + 26 + 48) = 8 + 21 + 288 = 317 <= 320 -> spacer = 3
//    => CA 2 truong hop deu <= 320 px: BACK va HOME KHONG BAO GIO bi day ra ngoai.
//  TITLE_W va LINK_LBL_W la be rong CO DINH nen tong tren la con so chac chan (khong phu thuoc
//  do dai ten man / ten may); chu dai hon thi TU CUON trong o cua no.
// ===========================================================================
static void build_chrome(void) {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  s_bar = lv_obj_create(scr);
  lv_obj_set_pos(s_bar, 0, 0);
  lv_obj_set_size(s_bar, UI_LCD_HOR_RES, BAR_H);
  style_flat(s_bar, COL_BAR);
  lv_obj_set_style_pad_left(s_bar, 4, 0);
  lv_obj_set_style_pad_right(s_bar, 4, 0);
  // Gap 3 (truoc la 4): du 7 khe khi dang in -> tiet kiem 7 px, xem bang be rong o tren.
  lv_obj_set_style_pad_column(s_bar, 3, 0);
  lv_obj_set_flex_flow(s_bar, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // ---- Nut HOME (icon ngoi nha) o BEN TRAI thanh tieu de, TRUOC ten man ----
  // Bam -> VE THANG man STATUS (A_GOTO_STATUS). Nut BACK (cuoi thanh) giu nguyen = lui 1 cap.
  // Glyph U+E007 cua font icon NHO (cung font voi icon WiFi o header), nhan duoc canh giua bang
  // lv_obj_center() trong mk_btn. Vao group nhu moi nut -> bam duoc bang CA cam ung lan encoder.
  // Kiem glyph luc CHAY: font cu (chua co U+E007) -> doi nhan thanh chu "H" cho nut khong trong.
  {
    lv_obj_t *home = mk_btn(s_bar, HOME_GLYPH, HOME_BTN_W, 20, cb_action,
                            (void *)(intptr_t)A_GOTO_STATUS);
    lv_obj_t *hl = lv_obj_get_child(home, 0);
    const lv_font_t *iconf = wifi_icon_font(false);
    lv_font_glyph_dsc_t gd;
    if (hl && iconf && lv_font_get_glyph_dsc(iconf, &gd, 0xE007U, 0))
      lv_obj_set_style_text_font(hl, iconf, 0);          // co glyph -> nut chi co icon
    else if (hl)
      set_text(hl, "H");                                 // duong lui: chu, van bam duoc
  }

  // ---- Ten man: o be rong CO DINH (TITLE_W) de tong be rong thanh tieu de la con so chac chan
  // (xem bang o tren). Ten dai hon o thi tu cuon trong o, khong day cac nut ra ngoai.
  s_title = mk_label_w(s_bar, "STATUS", COL_TXT, TITLE_W);
#if UI_MONO_FONT == 4
  // DOI CHUNG cho phep thu mono: giu TEN MAN o font Montserrat. Neu man hinh chi con doc duoc ten
  // man ma moi chu khac mat => font mono khong ve duoc. Neu chu khac hien binh thuong => font tot.
  // (Uu tien nhat: neu CA ten man lan chu khac deu mat thi loi nam o cho khac, khong phai font.)
  lv_obj_set_style_text_font(s_title, &lv_font_montserrat_14, 0);
#endif
  // Ten man CUNG bam duoc -> ve STATUS (giong nut HOME). lv_label mac dinh KHONG clickable (xem
  // lv_label.c) nen phai tu bat co; them CLICK_FOCUSABLE + vao group de encoder cung dung duoc.
  lv_obj_add_flag(s_title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_flag(s_title, LV_OBJ_FLAG_CLICK_FOCUSABLE);
  lv_obj_add_event_cb(s_title, cb_action, LV_EVENT_CLICKED, (void *)(intptr_t)A_GOTO_STATUS);
  if (s_group) lv_group_add_obj(s_group, s_title);

  mk_spacer(s_bar, 0, 1, 1);                     // an het cho trong -> day 2 vat sau ve phai
  // Gioi han be rong de ten may dai khong day cac nut P9 ra ngoai thanh tieu de.
  // 64 px: du cho "LINK ERR" (8 ky tu mono x 8 px = 64) va vua du cho 4 nut + icon wifi + ten man
  // (xem bang be rong o dau build_chrome).
  s_link  = mk_label_w(s_bar, "NO LINK", COL_ERR, LINK_LBL_W);
  // Icon trang thai WiFi: ngay SAU ten may in, TRUOC 2 nut PAUSE/STOP va BACK. La CON CUA s_bar
  // nen tu an/hien cung dong tieu de (man STATUS an s_bar -> icon an theo, khong can lam gi).
  // Dung font icon BAN NHO (14 px) - ban to 30 px se tran khoi thanh tieu de 26 px.
  s_wifi = mk_wifi_icon(s_bar, wifi_icon_font(false));
  // P9: 2 nut nho PAUSE / STOP - mac dinh AN, chi hien khi dang in (xem ui_lvgl_task)
  s_btn_pause = mk_btn(s_bar, "II", 26, 20, cb_action, (void *)(intptr_t)A_PAUSE);
  s_btn_stop  = mk_btn(s_bar, "[]", 26, 20, cb_action, (void *)(intptr_t)A_STOP);
  lv_obj_add_flag(s_btn_pause, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(s_btn_stop,  LV_OBJ_FLAG_HIDDEN);
  mk_btn(s_bar, "BACK", 48, 20, cb_action, (void *)(intptr_t)A_BACK);

  // --- Thanh day (sticky nhu dong tieu de): [PREV] [TRANG x/y] [NEXT] ---
  // CHI hien khi noi dung man dang mo BI TRAN theo chieu doc (xem bot_sync()).
  // PHAI tao luc dang HIEN roi moi an: LVGL bo qua tinh layout cho vat HIDDEN, do luc an
  // se ra 0 -> bot_sync() tuong la khong tran va khong bao gio hien thanh nay.
  s_bot = lv_obj_create(scr);
  lv_obj_set_pos(s_bot, 0, BOT_Y);
  lv_obj_set_size(s_bot, UI_LCD_HOR_RES, BOT_H);
  style_flat(s_bot, COL_BAR);
  lv_obj_set_style_pad_left(s_bot, 4, 0);
  lv_obj_set_style_pad_right(s_bot, 4, 0);
  lv_obj_set_style_pad_column(s_bot, 4, 0);
  lv_obj_set_flex_flow(s_bot, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(s_bot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  // Chu "PREV"/"NEXT"/"TRANG x/y" deu la ASCII -> dung Montserrat mac dinh (KHONG vn_font*)
  s_bot_prev = mk_btn(s_bot, "PREV", 56, 18, cb_action, (void *)(intptr_t)A_PAGE_PREV);
  s_bot_info = mk_label(s_bot, "", COL_DIM);
  lv_obj_set_flex_grow(s_bot_info, 1);                                        // gian het cho trong giua
  lv_obj_set_style_text_align(s_bot_info, LV_TEXT_ALIGN_CENTER, 0);
  s_bot_next = mk_btn(s_bot, "NEXT", 56, 18, cb_action, (void *)(intptr_t)A_PAGE_NEXT);

  lv_obj_update_layout(s_bot);                    // do xong (luc dang HIEN) roi moi an
  lv_obj_add_flag(s_bot, LV_OBJ_FLAG_HIDDEN);
  s_bot_shown = false;
}

// ===========================================================================
//  Prompt cua Marlin + thong bao
// ===========================================================================
static lv_obj_t *s_prompt_box, *s_msg_box;
static lv_obj_t *s_pid_box;                                  // P5: hop "tune xong"
static uint32_t  s_msg_shown_ms = 0, s_msg_seen_ms = 0;
static uint32_t  s_pid_seen = 0;                             // moc pid_done_ms da hien

static void box_close(lv_obj_t **box) {
  if (!*box) return;
  // Bo btnmatrix khoi group TRUOC khi xoa (khong de group giu con tro da chet)
  lv_obj_t *bm = lv_msgbox_get_btns(*box);
  if (bm && s_group) lv_group_remove_obj(bm);
  lv_msgbox_close_async(*box);
  *box = NULL;
}

static const char *sev_name(uint8_t sev) {
  switch (sev) {
    case CYDLCD_SEV_INFO:  return "INFO";
    case CYDLCD_SEV_WARN:  return "CANH BAO";
    case CYDLCD_SEV_ERROR: return "LOI";
    case CYDLCD_SEV_FATAL: return "LOI NANG";
    default:              return "THONG BAO";
  }
}

static lv_color_t sev_color(uint8_t sev) {
  switch (sev) {
    case CYDLCD_SEV_INFO:  return COL_ACCENT;
    case CYDLCD_SEV_WARN:  return COL_WARN;
    default:              return COL_ERR;
  }
}

static lv_obj_t *box_make(const char *title, const char *txt, lv_color_t title_col,
                          const char *const *btns) {
  lv_obj_t *box = lv_msgbox_create(NULL, title, txt, (const char **)btns, false);
  lv_obj_set_style_bg_color(box, COL_BAR, 0);
  lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, title_col, 0);
  lv_obj_set_style_text_color(box, COL_TXT, 0);

  lv_obj_t *t = lv_msgbox_get_title(box);
  if (t) lv_obj_set_style_text_color(t, title_col, 0);
  lv_obj_t *x = lv_msgbox_get_text(box);
  if (x) lv_obj_set_style_text_color(x, COL_TXT, 0);

  lv_obj_t *bm = lv_msgbox_get_btns(box);
  if (bm) {
    lv_obj_set_style_bg_color(bm, COL_BTN, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(bm, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, COL_TXT, LV_PART_ITEMS);
    lv_obj_set_style_border_color(bm, COL_ACCENT, LV_PART_ITEMS);
    lv_obj_set_style_border_width(bm, 1, LV_PART_ITEMS);
    lv_obj_set_style_pad_all(bm, 2, LV_PART_ITEMS);
    if (s_group) { lv_group_add_obj(s_group, bm); lv_group_focus_obj(bm); }
  }
  return box;
}

static void refocus_page(void) {
  if (!s_group) return;
  if (s_page[s_page_act]) focus_first_in(s_page[s_page_act]);
  else if (s_bar) lv_group_focus_obj(s_bar);
}

static void cb_prompt_value(lv_event_t *e) {
  lv_obj_t *box = lv_event_get_target(e);
  const char *t = lv_msgbox_get_active_btn_text(box);
  const uint8_t ok = (t && (t[0] == 'O' || t[0] == 'o')) ? 1 : 0;    // OK / CANCEL
  cydlcd_link_answer(ok);          // cydlcd_link_answer() tu dat prompt_active = false
  box_close(&s_prompt_box);
  refocus_page();
}

static void cb_msg_value(lv_event_t *e) {
  (void)e;
  box_close(&s_msg_box);
  refocus_page();
}

// P5: dong hop "tune xong"
static void cb_pid_value(lv_event_t *e) {
  (void)e;
  box_close(&s_pid_box);
  refocus_page();
}

// ---------------------------------------------------------------------------
//  Hop XAC NHAN dung chung (P3/P8/P9): OK moi chay lenh, CANCEL thi thoi.
//  steps = 2 -> phai bam OK 2 lan (dung cho Reboot DFU).
// ---------------------------------------------------------------------------
static lv_obj_t *s_cf_box;
static uint8_t   s_cf_action = 0;
static uint8_t   s_cf_steps  = 1;

static void cb_confirm_value(lv_event_t *e) {
  lv_obj_t *box = lv_event_get_target(e);
  const char *t = lv_msgbox_get_active_btn_text(box);
  const bool ok = (t && (t[0] == 'O' || t[0] == 'o'));
  if (ok && s_cf_steps > 1) {                    // con 1 buoc xac nhan nua (Reboot DFU)
    s_cf_steps--;
    lv_obj_t *x = lv_msgbox_get_text(s_cf_box);
    if (x) set_text(x, "Chac chan? Bam OK lan nua");
    return;
  }
  const uint8_t act = s_cf_action;
  box_close(&s_cf_box);
  refocus_page();
  if (ok) do_confirmed(act);                     // chi chay lenh khi bam OK
}

static void ui_confirm(const char *title, const char *txt, uint8_t action, uint8_t steps) {
  static const char *const btns[] = {"OK", "CANCEL", ""};
  box_close(&s_cf_box);
  s_cf_action = action;
  s_cf_steps  = steps ? steps : 1;
  s_cf_box = box_make(title, txt, COL_WARN, btns);
  lv_obj_add_event_cb(s_cf_box, cb_confirm_value, LV_EVENT_VALUE_CHANGED, NULL);
}

static void boxes_update(void) {
  // ---- prompt cua Marlin: uu tien cao nhat ----
  if (cydlcd.prompt_active) {
    if (!s_prompt_box) {
      box_close(&s_msg_box);                    // thong bao nhuong cho prompt
      static const char *const btns[] = {"OK", "CANCEL", ""};
      s_prompt_box = box_make("MARLIN", cydlcd.prompt_text[0] ? cydlcd.prompt_text : "(prompt)",
                              COL_WARN, btns);
      lv_obj_add_event_cb(s_prompt_box, cb_prompt_value, LV_EVENT_VALUE_CHANGED, NULL);
    }
    else {
      lv_obj_t *x = lv_msgbox_get_text(s_prompt_box);
      if (x) set_text(x, cydlcd.prompt_text[0] ? cydlcd.prompt_text : "(prompt)");
    }
  }
  else box_close(&s_prompt_box);

  // ---- thong bao moi (cydlcd.message_ms doi) ----
  if (!s_prompt_box && cydlcd.message[0] && cydlcd.message_ms != s_msg_seen_ms) {
    s_msg_seen_ms = cydlcd.message_ms;
    box_close(&s_msg_box);
    static const char *const btns[] = {"OK", ""};
    s_msg_box = box_make(sev_name(cydlcd.message_sev), cydlcd.message,
                         sev_color(cydlcd.message_sev), btns);
    lv_obj_add_event_cb(s_msg_box, cb_msg_value, LV_EVENT_VALUE_CHANGED, NULL);
    s_msg_shown_ms = lv_tick_get();
  }
  // tu dong dong thong bao sau 6s (prompt thi de nguoi dung tra loi)
  if (s_msg_box && lv_tick_elaps(s_msg_shown_ms) > 6000U) box_close(&s_msg_box);

  // ---- P5: Marlin bao tune xong (CYDLCD_EV_PID_DONE) -> hop "da xong" 1 lan ----
  if (cydlcd.pid_done_ms && cydlcd.pid_done_ms != s_pid_seen) {
    s_pid_seen = cydlcd.pid_done_ms;
    if (!s_prompt_box && !s_msg_box && !s_cf_box && !s_pid_box) {
      static const char *const btns[] = {"OK", ""};
      s_pid_box = box_make("TUNE XONG", "Marlin da chay xong M303/M306\nNho SAVE EEPROM neu can",
                           COL_OK, btns);
      lv_obj_add_event_cb(s_pid_box, cb_pid_value, LV_EVENT_VALUE_CHANGED, NULL);
    }
  }
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

    // Icon WiFi (header + man STATUS) va mau icon THE (man STATUS): nhip CHAM (~400 ms), chi ghi
    // style khi trang thai DOI. Nam ngoai switch(s_page_act) vi icon thuoc dong tieu de/STATUS.
    wifi_icon_mark();
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
//  API PRESET PREHEAT (khai bao trong ui_lvgl.h) - cho console COM7 va test
//  Kho du lieu nam trong khoi "PREHEAT presets" o phan SETTINGS (NVS namespace "ph").
// ===========================================================================
const char *ui_preheat_label(uint8_t i) { return (i < CYDLCD_PRE_COUNT) ? PRE_LABEL[i] : ""; }

bool ui_preheat_get(uint8_t i, uint16_t *nozzle, uint16_t *bed, uint8_t *fan) {
  if (i >= CYDLCD_PRE_COUNT) return false;
  if (nozzle) *nozzle = s_pre_h[i];
  if (bed)    *bed    = s_pre_b[i];
  if (fan)    *fan    = s_pre_f[i];
  return true;
}

bool ui_preheat_set(uint8_t i, uint16_t nozzle, uint16_t bed, uint8_t fan) {
  if (i >= CYDLCD_PRE_COUNT) return false;
  if (nozzle > PRE_H_MAX) nozzle = PRE_H_MAX;
  if (bed    > PRE_B_MAX) bed    = PRE_B_MAX;
  if (fan    > PRE_F_MAX) fan    = PRE_F_MAX;
  s_pre_h[i] = nozzle;
  s_pre_b[i] = bed;
  s_pre_f[i] = fan;
  preheat_save();          // NVS (CYD) - board khac chi giu RAM
  preheat_inject();        // cap nhat bang thong so de trang SETTINGS thay ngay
  tp_pre_sync();           // + nhan nut PLA/PETG/ABS o man TEMP
  WIFILOG("[PRE] dat %s: nozzle=%u bed=%u fan=%u\n", PRE_LABEL[i],
          (unsigned)s_pre_h[i], (unsigned)s_pre_b[i], (unsigned)s_pre_f[i]);
  return true;
}

#endif  // CYDLCD_USE_LVGL