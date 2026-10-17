/**
 * ui_lvgl_int.h -- header NOI BO cua tang UI LVGL cho MAN HINH CYD (ESP32-2432S028R).
 *
 * CHI dung trong noi bo firmware, cho cac file: ui_lvgl.cpp (loi + cac man con lai), ui_widgets.cpp
 * (helper tao widget + format so/chuoi), ui_chrome.cpp (dong tieu de, thanh day, icon trang thai),
 * ui_boxes.cpp (prompt + thong bao + hop xac nhan cua Marlin) va 6 file TRANG tach o chang 2/4:
 * ui_page_status.cpp / ui_page_menu.cpp / ui_page_temp.cpp / ui_preheat.cpp / ui_page_rate.cpp /
 * ui_page_jog.cpp.
 * API CONG KHAI giu nguyen trong ui_lvgl.h - file nay KHONG duoc include tu ben ngoai cydlcd/src.
 *
 * Noi dung:
 *   1) Cau hinh dung CHUNG: che do font mono (UI_MONO_xxx), bang mau Marlin (MR_xxx/COL_xxx),
 *      hinh hoc 320x240 va danh sach man (PG_xxx). Cac macro nay truoc day nam o dau ui_lvgl.cpp;
 *      khi tach file thi phai co DUNG MOT dinh nghia cho ca 4 file .cpp nen chung duoc chuyen ve day.
 *   2) Bien toan cuc dung CHUNG (extern): dinh nghia THAT nam o ui_lvgl.cpp.
 *   3) Prototype cac ham goi CHEO file. Ham/bien chi dung trong 1 file thi VAN giu `static`
 *      o file do va KHONG khai bao o day.
 *
 * Thu tu include: "ui_lvgl.h" -> <stdint.h>/<stdbool.h>/<stddef.h> -> <lvgl.h> -> header du an.
 */
#pragma once

#include "ui_lvgl.h"

#if CYDLCD_USE_LVGL

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>                 // size_t (prototype cua fmt_scaled/fmt_time)

#include <lvgl.h>

#include "cydlcd_link.h"            // cydlcd + CYDLCD_ST_*/CYDLCD_SEV_*/CYDLCD_SF_*/CYDLCD_TEMP_*
#include "tbt_link.h"               // tbt: link ASCII cua Bed Tramming Wizard
#include "lv_font_gfxfont.h"        // GFXfont + lv_font_gfxfont_get_ex (che do UI_MONO_FONT 4)


// ---------------------------------------------------------------------------
//  Log chan doan cua UI tren board that (COM7, 115200). Truoc day dinh nghia o dau ui_lvgl.cpp;
//  chuyen ve day vi ui_preheat.cpp cung dung (WIFILOG la macro -> file nao goi thi phai include
//  <Arduino.h> de co `Serial` TRUOC diem goi).
// ---------------------------------------------------------------------------
#if defined(CYDLCD_BOARD_CYD)
  #define WIFILOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #define WIFILOG(...)   do { } while (0)
#endif


// ---------------------------------------------------------------------------
//  Font ICON cho header + man STATUS: gfx4_font_icons.h/.c (ban NHO, 14 px) va
//  gfx4_font_icons_big.h/.c (ban TO 30 px - icon WiFi tren man STATUS, to bang icon FR/FL 32x32).
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
  #if __has_include("gfx4_font_icons_title.h")
    #define UI_HAVE_ICON_FONT_TITLE 1
  #endif
#endif
#if !defined(UI_HAVE_ICON_FONT)
  #define UI_HAVE_ICON_FONT 0
#endif
#if !defined(UI_HAVE_ICON_FONT_BIG)
  #define UI_HAVE_ICON_FONT_BIG 0
#endif
#if !defined(UI_HAVE_ICON_FONT_TITLE)
  #define UI_HAVE_ICON_FONT_TITLE 0
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
#if UI_HAVE_ICON_FONT_TITLE
  // Ban 20 px RIENG cho THANH TIEU DE (2026-10-16): ban 14 px cho glyph 10..13 px -> nguoi dung
  // bao "hoi be"; ban 30 px (big) cho 20..23 px -> qua cao so voi thanh 26 px. Xem
  // gfx4_font_icons_title.c. Chi chua 6 glyph ma thanh tieu de dung (khong phai ca bang 14 px).
  extern "C" const lv_font_t *gfx4_font_icons_title_get(void);
#endif

// Chuoi UTF-8 cua cac glyph - GIONG NHAU o ca 2 co font (ban nho va ban to), nen khong phu thuoc
// viec file font nao co mat. (U+E004/U+E005 'the nho'/'the loi' co trong font nhung KHONG dung:
// man STATUS da co san icon the 64x64 cua Marlin, chi doi mau - xem status_sd_icon_mark().)
#define WIFI_GLYPH_ON    "\xEE\x80\x80"   // U+E000 wifi        -> DA ket noi
#define WIFI_GLYPH_CONN  "\xEE\x80\x82"   // U+E002 3 vach      -> DANG ket noi
#define WIFI_GLYPH_OFF   "\xEE\x80\x81"   // U+E001 wifi_off    -> WiFi tat / chua cau hinh
#define HOME_GLYPH       "\xEE\x80\x87"   // U+E007 ngoi nha    -> nut HOME o dau thanh tieu de
// ICON KET NOI MAY IN — thay cho TEN MAY IN o o `LINK_LBL_W` tren thanh tieu de (2026-10-16).
// Nguon Material Icons: U+E1E0 `usb` (phich cam) va U+E4FA `usb_off` (phich cam + DAU GACH, dung
// kieu nhu `wifi_off`). Day la 2 glyph MOI them vao gfx4_font_icons.h (o U+E00C..U+E014).
// CHI co o FONT NHO: font TO (gfx4_font_icons_big.h) giu dai rieng U+E000..U+E003 cho man STATUS,
// neu mo rong no se DUNG SO O voi font nho (vd U+E004 = the nho ben nho, = icon khac ben to).
// Cac lua chon KHAC da sinh san trong font — doi 1 dong la xong, KHONG can sinh lai font:
//   U+E00E power_input (phich cam vao o cam) · U+E00F cable · U+E010 outlet (o cam tuong)
//   U+E011 power · U+E012 power_off · U+E013 link · U+E014 link_off
#define LINK_GLYPH_ON    "\xEE\x80\x8C"   // U+E00C <- usb      -> DA ket noi
#define LINK_GLYPH_OFF   "\xEE\x80\x8D"   // U+E00D <- usb_off  -> MAT ket noi (co dau gach)
// ---------------------------------------------------------------------------
//  ICON DIEU KHIEN IN (2026-10-16, yeu cau nguoi dung: "cac nut nay dung icon material play, pause,
//  stop, thay ca icon tren topbar"). Lay tu CUNG font MaterialIcons-Regular.ttf nhu cac icon khac -
//  xem lenh sinh o dau gfx4_font_icons_title.c. Ma Material goc:
//      pause      = U+E034  -> o day U+E00E
//      play_arrow = U+E037  -> o day U+E00F
//      stop       = U+E047  -> o day U+E010
//  (Ma goc kiem tra bang bang ligature cua font: 'pause'/'playunderscorearrow'/'stop'.)
//  Ky tu ngoai 5 dai cua font CHU (Latin + tieng Viet) nen PHAI dung font ICON, khong dung font chu.
// ---------------------------------------------------------------------------
#define CTRL_GLYPH_PAUSE "\xEE\x80\x8E"   // U+E00E <- Material pause
#define CTRL_GLYPH_PLAY  "\xEE\x80\x8F"   // U+E00F <- Material play_arrow (chay tiep)
#define CTRL_GLYPH_STOP  "\xEE\x80\x90"   // U+E010 <- Material stop
/** Font ICON cua thanh tieu de (20 px). Co san de man khac dung CUNG bo icon (nut PAUSE/RESUME/STOP
 *  o man STATUS). LUON khac NULL (lui ve font icon 14 px neu ban build nay khong co font title). */
const lv_font_t *title_icon_font_get(void);
/** 1 = font icon hien tai CO du 3 glyph pause/play/stop; 0 = ban build cu -> phai dung chu ASCII. */
uint8_t ctrl_icon_ok(void);
// 1 = `s_link` dang dung FONT ICON (do luc TAO thanh tieu de, xem ui_chrome.cpp). Font cu chua co
// glyph -> 0, va ui_lvgl.cpp lui ve hien CHU nhu truoc (khong hien o vuong).
extern uint8_t g_link_icon_ok;
// Font co DAU tieng Viet: chi con font MONO GFX TU SINH (che do 4) - xem tools/ttf2gfxfont.py
// + lv_font_gfxfont.c. KHONG con font nao sinh bang lv_font_conv: ca 3 file font do da XOA khoi
// repo (che do 1 va 2 da thu tren may that deu MAT CHU; che do 3 chua lam).
//
// Chon font cho UI (chi con 2 che do):
//   4 = font CHU GFX TU SINH (MAC DINH): gfx4_font_text (BlinkMacSystemFont, font TI LE) qua driver
//       lv_font_gfxfont, gan TUNG nhan/nut (xem mono_obj). CO DU DAU tieng Viet (5 dai roi rac -
//       xem UI_GFX_RANGES). Ten man (s_title) giu Montserrat DOI CHUNG. Do phan giai mau: UI_MONO_GFX_BPP.
//   0 = chi Montserrat 14 mac dinh cua LVGL (du phong khi khong muon font GFX).
//       >>> KHONG HIEN DUOC DAU TIENG VIET: Montserrat khong co U+1EA0..U+1EF9, va vi da xoa het
//       font sinh bang lv_font_conv nen KHONG con font nao co dau -> vn_font()/vn_font_btn() thanh
//       no-op va 3 nhan co dau trong lop phu SUA ("Gia tri" / "Buoc" / "Huy") se ra O VUONG.
//       Muon co dau tieng Viet thi PHAI dung che do 4.
// Cho phep ep tu ngoai (-DUI_TEXT_FONT=N) de thu font khac ma khong phai sua file.
//
// 2026-10-16: KHONG dat thang `UI_MONO_FONT` / `UI_MONO_GFX_BPP` nua — CA HAI suy ra tu MOT so
// `UI_TEXT_FONT` trong cydlcd_config.h. Doi font chi can doi so do, khong phai sua cho nao khac.
// Giu nguyen 2 ten cu de phan con lai cua file khong phai sua.
#if UI_TEXT_FONT == 0 || UI_TEXT_FONT == 1
  #define UI_MONO_FONT 4            // font GFX tu sinh
#elif UI_TEXT_FONT == 2
  #define UI_MONO_FONT 0            // font MAC DINH cua LVGL (Montserrat 14)
#else
  #error "UI_TEXT_FONT chi nhan 0 (Segoe UI 16) / 1 (Roboto Mono 13) / 2 (Montserrat 14)"
#endif

// Do phan giai cua font GFX (chi co nghia khi UI_MONO_FONT == 4 — xem lv_font_gfxfont.h):
//   4 = 4 bit/pixel, 16 muc xam -> CO antialias (chu dep hon han 1-bit).
//       Header gfx4_font_text.h KHONG phai Adafruit GFXfont: bitmap da ghi theo dung chuoi bit
//       lien tuc cua LVGL nen driver tra con tro THANG (khong repack, khong ton RAM).
//   1 = 1 bit/pixel, Adafruit GFXfont (gfx_font_robotomono.h) -> driver phai repack.
//       Chu bi rang (khong antialias) nhung nhe flash hon nhieu (39 KB so voi 112 KB).
#if UI_TEXT_FONT == 0
  #define UI_MONO_GFX_BPP 4
#elif UI_TEXT_FONT == 1
  #define UI_MONO_GFX_BPP 1
#else
  #define UI_MONO_GFX_BPP 4         // che do 2 (Montserrat) KHONG dung tri so nay; de 4 cho khoi loi
#endif

#if UI_MONO_FONT == 4
// ---------------------------------------------------------------------------
//  Che do 4: font mono tu sinh.
//  - 5 dai roi rac (khong the la 1 bang 0x20..0x1EF9: ton ~55 KB vo ich).
//  - Moi dai la mot bang glyph rieng; lv_font_gfxfont_get_ex() tra tuan tu.
//  - line_height/base_line lay tu du lieu glyph cua CA 5 dai (glyph tieng Viet cao hon ASCII).
//  - 2026-10-15: font CHU doi sang **BlinkMacSystemFont size 14** (font TI LE, yAdvance 19, xAdvance
//    rieng tung glyph); font ICON van mono (`--advance`) de cac o icon deu nhau.
// ---------------------------------------------------------------------------
#if UI_MONO_GFX_BPP == 4
  #define UI_GFX_LAYOUT LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4
  extern const GFXfont gfx4_font_text_ascii;        // U+0020..U+00FF
  extern const GFXfont gfx4_font_text_vn1;          // U+0102..U+0103  A(breve) a(breve)
  extern const GFXfont gfx4_font_text_vn2;          // U+0110..U+0111  D/ d/ gach ngang
  extern const GFXfont gfx4_font_text_vn3;          // U+01A0..U+01B1  O/ o/ U/ u/ moc
  extern const GFXfont gfx4_font_text_vn4;          // U+1EA0..U+1EF9  to hop dau tieng Viet
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
/**
 * Descriptor font mono GFX tu sinh (che do UI_MONO_FONT 4); dinh nghia trong ui_lvgl.cpp
 * (nap trong ui_lvgl_begin). NULL = font loi -> UI_MONO_FACE lui ve Montserrat.
 */
extern const lv_font_t *s_gfx_mono;
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

// ===========================================================================
//  Hang so dung chung khac
// ===========================================================================
// Trang thai THE SD (the nam o khe MAN HINH, xem cyd_sd.h) cho icon THE NHO tren man STATUS.
// Trang thai THE TF da chuan hoa ve 3 muc - de phan biet "khong co the" voi "khong biet":
//   SD_ST_NONE = khong co accessor  -> icon XAM (khong phai loi)
//   SD_ST_NO   = the chua mount / loi           -> icon DO
//   SD_ST_OK   = the dang dung duoc             -> icon XANH
enum { SD_ST_NONE = 0, SD_ST_NO, SD_ST_OK };
// ===========================================================================
//  Mau -- DONG NHAT theo Marlin/src/lcd/tft/tft_color.h (moi man dung cung bang mau)
// ===========================================================================
#define MR_BG      lv_color_hex(0x1E156E)   // COLOR_BACKGROUND
#define MR_COLD    lv_color_hex(0x00E5FF)   // Material 3 dark cyan A400 (12.07 -> 9.84:1, do choi hon)
#define MR_WARM    lv_color_hex(0xFFD54F)   // Material 3 dark amber 300: MOC GIUA cua dai mau nhiet do
                                            // (xanh -> vang -> do). Xem temp_ramp() o ui_page_status.cpp.
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
// NEN THANH TIEU DE — sang hon nen man hinh de nhin ro no la "header bar".
// VI SAO KHONG THE DAT 3:1 BANG CACH DOI NEN (da tinh, xem README muc "Thanh tieu de"):
//   nen man hinh MR_BG qua toi (L = 0,0176). Doi tieu de TOI hon -> toi da 1,24:1 (nen den tuyet
//   doi cung vay). Doi tieu de SANG hon thi chua toi 3:1 thi chu da tut: can L >= 0,1528 de dat
//   3:1, nhung chu COL_TXT chi con >= 4,5:1 khi L <= 0,1319 -> HAI DIEU KIEN MAU THUAN NHAU.
// Vi vay: (1) doi nen sang #3A2E9E cho KHAC BIET RO (1,49:1, moi chu tren do van >= 4,5:1), va
//         (2) ve DUONG KE 1 px duoi chan bang COL_BTN_OUTLINE -> BIEN cua thanh dat 4,45:1 (>= 3:1).
#define COL_TITLEBAR lv_color_hex(0x3A2E9E)
#define COL_TXT    MR_MENUVAL
#define COL_DIM    lv_color_hex(0xCAC4D0)   // Material 3 dark on-surface-variant: 6.53 -> 8.88:1
#define COL_BTN    lv_color_hex(0x2A2080)   // nen nut
// VIEN cho nut NEN TOI (`COL_BTN`) — kieu "outlined button" cua Material 3.
// VI SAO CAN: `COL_BTN` #2A2080 va nen man hinh `MR_BG` #1E156E cung tong xanh tim, chi chenh
// **1.21:1** (WCAG 1.4.11 can >= 3:1 cho khoi giao dien) -> vien nut gan nhu vo hinh.
// Doi NEN toi hon KHONG cuu duoc: do CA HAI deu toi, nen du nen den tuyet doi thi ti so cung chi
// 1.63:1. Doi nen SANG thi duoc nhung la DOI SANG THEME (chu dang la mau sang -> phai lam lai ca
// bang mau). Vi vay chon cach re nhat: giu nguyen mau, chi ve them duong bao.
// #8A7FD0 dat 4.45:1 voi nen man hinh va 3.68:1 voi nen nut -> nhin ro o ca hai phia.
#define COL_BTN_OUTLINE lv_color_hex(0x8A7FD0)
#define COL_ACCENT MR_COLD                   // focus / diem nhan
// CHU DAT TREN NEN `COL_ACCENT` — dung khi to nen vat dang CHON.
// VI SAO CAN TOKEN RIENG: COL_ACCENT = #00E5FF la mau SANG, nen chu phai TOI moi dat tuong phan:
//     COL_TXT (#E6E1E5) tren accent = 1.19:1  -> KHONG DAT (WCAG/Material 3 can >= 4.5:1)
//     COL_ON_ACCENT     tren accent = 10.66:1 -> DAT
// Truoc day chi 2 cho (danh sach FILE, roller WIFI) tu dat mau toi 0x102030 va lam DUNG; 4 cho khac
// (JOG / MESH / PREHEAT / SETTINGS) chi doi NEN ma quen doi CHU -> chu tren o dang chon gan nhu vo hinh.
// Nay gom ve mot token + mot ham `mk_btn_set_sel()` de khong the quen nua.
#define COL_ON_ACCENT lv_color_hex(0x102030)
#define COL_WARN   MR_RATEALT
// Nen nut STOP (2026-10-16, yeu cau nguoi dung: "nut stop fill mau do"). Material red 800.
// VI SAO KHONG dung MR_HOTEND (#FF8A80): mau do nhat do de lam CHU tren nen toi; to NEN bang no thi
// icon trang chi dat ~2:1 (khong doc duoc). Tren #C62828 thi icon trang ~7,4:1.
#define MR_STOPBG  lv_color_hex(0xC62828)
// Nen MAN HINH LOI NANG (muc FATAL: may da halt, chi tat nguon moi het) - 2026-10-16, yeu cau nguoi
// dung: "cho CYD chiem toan man hinh nen do, chu trang". Cung sac do voi nut STOP, chu trang dat 5,9:1.
#define CRIT_BG    lv_color_hex(0xC62828)
// Nen nut PAUSE (vang) / RESUME (xanh la) - 2026-10-16, yeu cau nguoi dung "nut pause mau vang, va
// resume mau xanh la". Material yellow 800 / green 800. Mau CHU/ICON do mk_btn_set_bg() tu chon theo
// do sang nen (mk_color_on): nen vang -> chu TOI, nen xanh -> chu SANG.
#define MR_PAUSEBG   lv_color_hex(0xF9A825)
#define MR_RESUMEBG  lv_color_hex(0x2E7D32)
#define COL_ERR    MR_HOTEND
#define COL_OK     MR_RATE100
// ===========================================================================
//  Man hinh
// ===========================================================================
enum {
  PG_STATUS = 0, PG_MENU, PG_RATE, PG_TEMP, PG_JOG, PG_FILES, PG_SETTINGS, PG_MESH, PG_CUSTOM,
  PG_TBT, PG_INFO, PG_ENDSTOP,
  PG_WIFI,                 // trang WIFI (mo tu icon WiFi o header/man STATUS hoac muc "WiFi" MENU)
  // 2026-10-15: PG_MPC (trang "TUNING MPC") da BO - xem ghi chu o MENU_ITEMS (ui_page_menu.cpp):
  // 6 tham so M306 + 2 nut autotune deu da co trong Configuration > Advanced Settings > Temperature.
  PG_SUBMENU,              // trang MENU CON (menu "Configuration" kieu 12864): NOI DUNG do bang
                           // SM_GROUPS/SM_ITEMS trong ui_page_submenu.cpp quyet dinh theo `s_sm_g` ->
                           // MOT trang dung chung cho ca cay (khong phai 1 trang cho moi nhom).
  PG_COUNT
};
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
  // 2026-10-15: nap lai thong so tu EEPROM (M501) - muc "Load Settings" cua menu Configuration.
  // KHONG co xac nhan 2 buoc: M501 chi doc EEPROM vao RAM (hoan tac thay doi CHUA luu), vo hai.
  A_LOAD_SETTINGS,
  // 2026-10-15: PREHEAT (danh sach vat lieu do nguoi dung them/bot - xem ui_page_preheat.cpp):
  //   A_PRE_ADD    = mo BAN PHIM dat ten vat lieu moi (dong "+ Them vat lieu" trong menu con);
  //   A_PRE_DELETE = lenh THAT cua nut "Xoa" trong hop sua 3-trong-1 (chi chay sau khi xac nhan 2 buoc).
  A_PRE_ADD, A_PRE_DELETE,
  // P4/P6 - endstop M119 + filament (M600/M412)
  A_GOTO_ENDSTOP, A_REQ_ENDSTOPS, A_FILAMENT_CHANGE, A_RUNOUT_TOGGLE,
  // P5 - tuning nhiet (dung cho Configuration > Advanced Settings > Temperature)
  A_MPC_TUNE, A_PID_BED,
  // 2026-10-15: A_GOTO_MPC da BO cung trang PG_MPC (muc "Tuning (MPC/PID)" o MENU).
  // Man FILE: xoa file SD dang chon (chi chay SAU khi xac nhan 2 buoc - xem do_confirmed)
  A_FILES_DELETE,
  // 2026-10-15: NGON NGU (da ngon ngu kieu Marlin - xem src/ui_lang.h). 2 dong cua nhom
  // Configuration > Advanced Settings > Language; chi doi chu cua MAN HINH, khong gui gi sang Marlin.
  A_LANG_VI, A_LANG_EN,
  // 2026-10-15: menu MOTION giong so do 12864 (xem docs/12864-menu-map.md, nut m20):
  //   A_GOTO_MESH / A_GOTO_TBT = mo trang BED MESH / TRAMMING tu trong menu (MENU mo bang chi so trang,
  //   con menu CON thi phai qua hanh dong);
  //   A_PROBE_DEPLOY / A_PROBE_STOW = M401 / M402 (12864: "Deploy/Stow Z-Probe"; may nay probe CO DINH
  //   (Voron Tap) nen Marlin se bao loi - van de nguyen vi 12864 cung hien 2 muc nay);
  //   A_G29_LEVEL = G29 N (Level Bed), A_G29_INFO = G29 W (Output UBL Info);
  //   A_UBL_T0 / A_UBL_T1 / A_UBL_S1 = G29 T0 (Output for Host) / G29 T1 (CSV) / G29 S-1 (Off Printer Backup);
  //   A_UBL_MANUAL = G29 I999 + G29 P2BT0 (12864: "Manually Build Mesh");
  //   A_M48 = G28 O + M48 P10 (12864: "M48 Probe Test").
  A_GOTO_MESH, A_GOTO_TBT,
  A_PROBE_DEPLOY, A_PROBE_STOW,
  A_G29_LEVEL, A_G29_INFO, A_G29_UBL_MANUAL,
  A_UBL_T0, A_UBL_T1, A_UBL_S1,
  A_M48,
  // Mo menu MOTION (nhom rieng, ngang cap CONFIGURATION - giong 12864: Motion la muc trong Main menu).
  A_GOTO_MOTION,
  // Thanh day: phan trang noi dung man dang mo (xem page_scroll/bot_sync)
  A_PAGE_PREV, A_PAGE_NEXT
};
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
// ---------------------------------------------------------------------------
//  VUNG GIA TRI cua hang X/Y/Z -- KHONG duoc dung chung x voi nhan truc (sua 2026-10-16).
//
//  LOI NGUOI DUNG BAO: "tai truc X 285.98 thi so 2 de vao nhan X".
//  NGUYEN NHAN: nhan dat tai x = LX, gia tri can phai trong hop [LX .. LX+VW] = [10 .. 68] => hop chi
//  58 px ma BAT DAU NGAY tai nhan. Font Montserrat 16 (yeu cau 2026-10-15: "font X/Y/Z to hon 1 chut")
//  rong HON font 8x12 cua man 12864 - do THAT tu lv_font_montserrat_16.c (adv_w/16):
//      '2' = 9,19 px   '8' = 10,31   '.' = 3,62   '-' = 6,12   'X' = 10,75 px
//  => "285.98" = 52,5 px (dai nhat "-285.98" = 58,6 px) => gia tri bat dau o 68 - 52,5 = 15,5
//     -> DE len nhan 'X' (10 .. 20,75). Marlin goc (ui_320x240.cpp) cung can phai ve 68 nhung font
//     cua no hep (~6 px/ky tu) nen 6 ky tu chi ~36 px, khong bao gio cham nhan.
//
//  CACH SUA: nhan GIU NGUYEN (10 / 127 / 219 - dung bo cuc Marlin), con vung gia tri bat dau SAU nhan
//  (LX + 14 = be rong 'X' 10,75 + 3 px khe) va NO RONG sang phai (khung XYZ rong 312 px, giua cac o
//  con nhieu cho): X: 24..90 (66 px) | Y: 141..207 (66 px) | Z: 233..301 (68 px).
//  => ca "-285.98" (58,6 px) cung vua, va khong cham nhan ke tiep (Y o 127, Z o 219).
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  Cho phep GHI DE tu ngoai (2026-10-16) de A/B kiem tra bo cuc MA KHONG can sua file: ban gia lap
//  `cydlcd/sim` build lai voi bo cuc CU roi do khoang trong nhan<->so (xem tools/run_sim.ps1 -OldXyz).
//  Do la bang chung "bay kiem thu nay THAT SU bat duoc loi so de len nhan".
// ---------------------------------------------------------------------------
#ifndef MR_XYZ_VX
  #define MR_XYZ_VX(i) ((lv_coord_t)(MR_XYZ_LX(i) + 14))                        // 24 / 141 / 233
#endif
#ifndef MR_XYZ_VR
  #define MR_XYZ_VR(i) ((lv_coord_t)((i) == 0 ? 90 : ((i) == 1 ? 207 : 301)))   // can phai moi o
#endif
#ifndef MR_XYZ_VW
  #define MR_XYZ_VW(i) ((lv_coord_t)(MR_XYZ_VR(i) - MR_XYZ_VX(i)))              // 66 / 66 / 68
#endif
#define MR_RATE_Y    136                                  // canvas(70/170, 136, 80, 32)
#define MR_RATE_H    32
#define MR_SD_X      0                                    // the SD: Marlin (0,130,64,64)
#define MR_SET_X_MARLIN 256                               // SETTINGS: Marlin (256,130,64,64)
// Hang giua (the SD / FR / FL / SETTINGS) cua man STATUS da THU NHO + doi x de nhuong cho icon
// WiFi + chu dBm (2026-10-14): cac so MOI nam o ui_page_status.cpp (ST_TILE_SZ/ST_TILE_Y/ST_SET_X/
// ST_RATE_W/ST_FR_X/ST_FL_X) - o day giu lai toa do GOC cua Marlin de doi chieu.
// 2026-10-16 (yeu cau nguoi dung: "can lai thoi gian in va phan tram in vao giua progressbar theo
// chieu ngang"): thanh tien do nay la x=4..232 (MR_BAR_W 228) -> TAM o x=118. Nhom [thoi gian][%]
// rong 128 + 2 + ~26 = 156 -> dat bat dau o x=32 de tam nhom roi dung vao 118 (truoc day dat o 96..248
// tuc tam 172 = tam MAN HINH, lech khoi thanh tien do).
// LUU Y do lai: o THOI GIAN la nhan CANH GIUA trong hop 128 px nen chu that su bat dau o ~52 (khong phai
// 40); vi vay phai lui them 8 px nua moi dung tam 118 (do duoc: chu 52..199 -> tam 125 truoc khi lui).
#define MR_TIME_X    32                                   // canvas(32, 176, 128, 20)
#define MR_TIME_Y    176
#define MR_TIME_W    128
#define MR_PCT_X     162                                  // % tien do, NGAY SAU o thoi gian
#define MR_PCT_W     30
#define MR_BAR_X     4                                    // canvas(4, 198, 312, 9)
#define MR_BAR_Y     198
// 2026-10-16 (yeu cau nguoi dung: "dich progress bar sang trai 1 chut"): 312 -> 228. Thanh tien do
// ngan lai tu ben PHAI (dau trai van o x=4) de nhuong cho 2 nut dieu khien o goc duoi phai.
#define MR_BAR_W     228
#define MR_BAR_H     9
#define MR_MSG_X     0                                    // canvas(0, 216, 320, 20)
#define MR_MSG_Y     216
#define MR_MSG_W     320                                  // DAY CHIEU NGANG khi may RANH
// 2026-10-16 (yeu cau nguoi dung): "luc chua in thi van phai fill chieu ngang, luc dang in thi moi co
// chieu rong lai" -> dong trang thai co 2 be rong: ranh = 320 (day), dang in = 232 (nhuong cho 2 nut).
#define MR_MSG_W_RUN 232                                  // luc DANG IN: co lai de nhuong cho 2 nut
// ---------------------------------------------------------------------------
// 2 NUT DIEU KHIEN IN o GOC DUOI PHAI trang STATUS (2026-10-16, yeu cau nguoi dung: "cho xuong goc
// duoi phai du hien thi 2 nut to mot chut, dich progress bar sang trai 1 chut").
//
//  BAN DAU (nay da thay): 3 nut 102x22 trai deu o hang cuoi y=210. Nguoi dung doi thanh 2 nut TO hon o
//  goc duoi phai -> thanh tien do ngan lai (MR_BAR_W 312 -> 228) va dong trang thai ngan lai
//  (MR_MSG_W 320 -> 232) de nhuong cho.
//
//  VI TRI: 38x38 o y=194..232 (vua het vung noi dung), x=238 va 280 -> ket thuc 318 (chua 2 px mep phai).
//  38 px = "to mot chut": to hon han nut HOME o thanh tieu de (26x20) va de cham hon.
//
//  O VI TRI 0 CO HAI NUT CHONG NHAU (PAUSE va RESUME, CUNG toa do): dang in thi hien PAUSE, dang TAM
//  DUNG thi hien RESUME -> nhin ra dung 1 nut TU DOI, dung y nguoi dung ("bam pause thi no chuyen thanh
//  nut resume"). VI SAO CHONG NHAU CHU KHONG DOI NHAN: doi chu bang lv_label_set_text() tren nhan con
//  cua nut KHONG VE LAI duoc (do tren board that: "II" = 102 px muc, doi sang "|>"/">>" deu 0 px).
//  An/hien CA NUT thi chac chan co ve lai (dang dung cho 2 nut o thanh tieu de).
//
//  Nut dung ICON Material pause/play/stop (CTRL_GLYPH_*), lui ve chu neu font thieu glyph.
#define MR_PB_Y      194
#define MR_PB_SZ     38
#define MR_PB_X(i)   (238 + (i) * 42)                     // 2 nut: 238 / 280, ket thuc 318

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
// 2026-10-15: 22 -> 25 (font chu len 20 px, "to bang hang X/Y/Z") -> **28** khi thu Blink (dong 24 px)
// -> **25** khi chot **Segoe UI size 16** (dong 20 px, net chu 11 px = DUNG bang Roboto Mono 15 truoc do;
// nguoi dung: "co chinh cung co voi Roboto Mono 15 duoc khong ? chu hoi to qua").
// 25 - 2 (khoang) = 23 px cho 1 dong chu 20 px.
#define MENU_ROW_H  25

// ===========================================================================
//  Bien toan cuc dung CHUNG giua cac file
//  (bo `static`, khai bao `extern` o day; dinh nghia THAT o ui_lvgl.cpp - rieng `s_cal_req`
//   dinh nghia o ui_page_menu.cpp, noi dat dong "Calibrate touch")
// ===========================================================================
/** Mang container noi dung tung man (chi so = PG_*); NULL = chua tao lan nao. */
extern lv_obj_t *s_page[PG_COUNT];
/** Dong tieu de chung [ten man] ... [LINK][BACK]; an o man STATUS. */
extern lv_obj_t *s_bar;
/** Nhan TEN MAN tren dong tieu de (cung bam duoc -> ve STATUS). */
extern lv_obj_t *s_title;
/** Nhan trang thai link tren dong tieu de (TEN MAY / LINK ERR / NO LINK). */
extern lv_obj_t *s_link;
/** 2 nut nho PAUSE/STOP tren dong tieu de (chi hien khi dang in tu the SD). */
extern lv_obj_t *s_btn_pause, *s_btn_resume, *s_btn_stop;
/** Group chung cho encoder/touch: MOI vat bam duoc cua moi man deu nam trong day. */
extern lv_group_t *s_group;
/** Man dang mo (chi so PG_*). */
extern uint8_t s_page_act;
/** Da giu tay du 1.5 s tren dong "Calibrate touch" (ui_page_menu.cpp) -> ui_lvgl_task() chay hieu chuan. */
extern bool s_cal_req;
/**
 * Input device (encoder hoac cam ung) - gan 1 lan trong ui_lvgl_begin(). Man WIFI dung no de KHOA/MO
 * input trong luc quet mang (net_wifi_scan() chan vai giay): xem ui_page_wifi.cpp.
 */
extern lv_indev_t *s_indev;

// ===========================================================================
//  KHO PRESET PREHEAT - DINH NGHIA trong ui_preheat.cpp (2026-10-15: 3 preset CO DINH -> DANH SACH
//  do nguoi dung tu THEM/BOT, toi da CYDLCD_PRE_MAX = 6 vi trang NHIET DO chi con cho 6 nut).
//
//  Cac GIOI HAN (CYDLCD_PRE_MAX / PRE_NAME_LEN / PRE_H_MAX / PRE_B_MAX / PRE_F_MAX / CYDLCD_PRE_SEED)
//  nam trong **ui_lvgl.h** (header cong khai) vi main.cpp - lenh console `PRE ...` - cung dung.
//  Mang du lieu `s_pre[]` VA ham load/save chi o ui_preheat.cpp; file khac doc/ghi qua API cong khai
//  (ui_lvgl.h): ui_preheat_count/name/get/set/add/remove/rename/rev.
// ===========================================================================

// ---------------------------------------------------------------------------
//  WiFi (weak) - hop dong voi net_wifi.cpp / net_upload.h
// ---------------------------------------------------------------------------
//  Linkage `extern "C"`: net_wifi_state() da duoc khai bao trong net_upload.h trong khoi
//  extern "C" (dinh nghia trong net_wifi.cpp voi linkage C nhu khai bao trong net_wifi.h), nen
//  cac ham WiFi phai cung linkage C moi bind dung ten.
//
//  `weak`: nhanh firmware CHUA merge -> con tro NULL -> trang WIFI van MO DUOC va bao
//  "khong ho tro" (khong crash, khong loi link). Cac khai bao nay dung o NHIEU file
//  (ui_lvgl.cpp: accessor an toan + A_WIFI_FORGET; ui_page_wifi.cpp: quet/ket noi/ngat)
//  nen phai nam MOT cho o day. Manh WIFI con khai bao rieng 4 ham quet ASYNC (scan_begin/poll/
//  cancel/scanning) + cyd_sd_printer_busy() ngay trong ui_page_wifi.cpp.
// ---------------------------------------------------------------------------
extern "C" uint8_t     net_wifi_state(void) __attribute__((weak));
extern "C" const char *net_wifi_ssid(void) __attribute__((weak));
extern "C" const char *net_wifi_ip(void) __attribute__((weak));
extern "C" int8_t      net_wifi_rssi(void) __attribute__((weak));
extern "C" int         net_wifi_scan(void) __attribute__((weak));
extern "C" const char *net_wifi_scan_ssid(int i) __attribute__((weak));
extern "C" int8_t      net_wifi_scan_rssi(int i) __attribute__((weak));
extern "C" bool        net_wifi_connect(const char *ssid, const char *pass, bool save) __attribute__((weak));
extern "C" void        net_wifi_disconnect(bool forget) __attribute__((weak));

// ---------------------------------------------------------------------------
//  Prototype ham dung chung
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_widgets.cpp
// ---------------------------------------------------------------------------
/** In so nguyen `raw` (da nhan `scale`) thanh chuoi thap phan `dec` chu so sau dau cham. */
void fmt_scaled(char *b, size_t n, int32_t raw, uint32_t scale, uint8_t dec);
/** In thoi gian in `s` giay thanh "MM:SS" (hoac "H:MM:SS" khi >= 1 gio). */
void fmt_time(char *b, size_t n, uint32_t s);
/** Ten trang thai in cua Marlin (CYDLCD_ST_*) de hien tren man STATUS. */
const char *state_name(uint8_t st);
/** Lay nhiet do hien tai/target cua 1 heater (frame PUSH, hoac dong "TBT ..." o che do TRAM). */
void temp_lookup(const uint8_t id, int16_t *cur, int16_t *tgt);
/** Set text cho nhan, CHI khi khac noi dung cu (khong nhap nhay, khong ve lai vo ich). */
void set_text(lv_obj_t *lbl, const char *txt);
// ---- Cac ham "chi ghi khi DOI" cho MAU/ANH (LVGL khong tu bo qua gia tri trung) --------------
// VI SAO: `lv_obj_set_style_*` va `lv_img_set_src` deu goi `lv_obj_invalidate()` vo dieu kien (lv_img.c:74)
// => man cap nhat 150 ms/lan ma goi chung moi lan thi moi 150 ms lai ve lai vung do (do that: 7 khung ~70 ms
// moi giay, ~3,3 man hinh/giay day ra SPI) du KHONG CO GI DOI. Dung 4 ham nay thay cho loi goi truc tiep.
void set_text_color(lv_obj_t *o, lv_color_t col);
void set_border_color(lv_obj_t *o, lv_color_t col);
void set_bg_color(lv_obj_t *o, lv_color_t col);
void set_img_src(lv_obj_t *im, const void *src);
/** Gan font CO DAU tieng Viet cho DUNG 1 nhan (che do 4; che do 0 la no-op). */
void vn_font(lv_obj_t *label);
/** Nhu vn_font() nhung cho nhan NAM TRONG 1 nut (mk_btn tao dung 1 label con). */
void vn_font_btn(lv_obj_t *btn);
/** Nen phang 1 vat: 1 mau dac, khong vien, khong bo goc, khong padding, khong cuon. */
void style_flat(lv_obj_t *o, lv_color_t bg);
/** Nhan 1 dong, rong theo noi dung (dat trong flex thi layout tu xep cho). */
lv_obj_t *mk_label(lv_obj_t *parent, const char *txt, lv_color_t col);
/** Nhan 1 dong, be rong CO DINH: chu dai hon thi tu cuon vong (khong bi cat cut). */
lv_obj_t *mk_label_w(lv_obj_t *parent, const char *txt, lv_color_t col, lv_coord_t w);
/** Container trong suot de xep con bang FLEX (flow COLUMN/ROW, pad trong, gap giua cac con). */
lv_obj_t *mk_flex(lv_obj_t *parent, lv_flex_flow_t flow, lv_coord_t w, lv_coord_t h,
                  lv_coord_t pad, lv_coord_t gap);
/** Vat dem trong suot: grow > 0 thi chiem het cho trong con lai cua hang flex. */
lv_obj_t *mk_spacer(lv_obj_t *parent, lv_coord_t w, lv_coord_t h, uint8_t grow);
/** Ve/xoa khung focus cho 1 vat (khong ghi de nen cua vat TU QUAN LY NEN - LV_OBJ_FLAG_USER_1). */
void focus_mark(lv_obj_t *o, const bool on);
/** Callback focus cua group: ve khung cho vat dang focus, xoa khung vat truoc. */
void on_focus_changed(lv_group_t *g);
/** Nut bam: kich thuoc co dinh, vao group, tu canh chu vao giua; cb/ud co the NULL. */
lv_obj_t *mk_btn(lv_obj_t *parent, const char *txt, lv_coord_t w, lv_coord_t h,
                 lv_event_cb_t cb, void *ud);
/** Nut chia deu be rong trong 1 hang flex (khong can biet truoc be rong tung nut). */
lv_obj_t *mk_btn_grow(lv_obj_t *parent, const char *txt, lv_coord_t h,
                      lv_event_cb_t cb, void *ud);
/**
 * Mau CHU tuong phan voi mot mau NEN bat ky: nen SANG -> chu toi (`COL_ON_ACCENT`), nen TOI -> chu sang
 * (`COL_TXT`). Xem `mk_btn_set_bg()`.
 */
lv_color_t mk_color_on(lv_color_t bg);

/**
 * Doi NEN cua mot nut do `mk_btn()` tao ra VA chon mau CHU tuong phan theo chinh mau nen do.
 *
 * VI SAO CO HAM NAY (thay vi moi noi tu goi `lv_obj_set_style_bg_color`):
 * UI co nhieu mau nen SANG (`COL_ACCENT` #00E5FF, `COL_OK` #81C784, `COL_WARN` #FFD54F,
 * `COL_ERR` #FF8A80). `mk_btn()` luon dat chu la `COL_TXT` (sang) bat ke nen, nen hien tuong
 * "doi nen ma quen doi chu" lam chu gan nhu vo hinh:
 *     COL_TXT tren COL_ACCENT = 1.19:1 · tren COL_OK = 1.58:1 · tren COL_WARN = 1.10:1
 *     (WCAG / Material 3 can >= 4.5:1; dung ham nay thi >= 9:1)
 * Da tung co 6 cho mac loi nay o 5 man hinh (JOG · BED MESH x2 · NHIET DO · THONG SO · CUSTOM).
 * Gom vao mot ham thi khong the quen nua.
 *
 * Chi set khi mau THAT SU doi (`lv_obj_set_style_*` luon invalidate vat -> goi vo dieu kien moi chu ky
 * cap nhat se lam man nhap nhay va ton bang thong SPI; cac man nay cap nhat 150-400 ms/lan).
 */
void mk_btn_set_bg(lv_obj_t *btn, lv_color_t bg);

/** Truong hop hay dung nhat: vat dang CHON (nen `COL_ACCENT`) hay khong (nen `COL_BTN`). */
void mk_btn_set_sel(lv_obj_t *btn, bool sel);
/** Vung BAM trong suot co toa do tuyet doi (cb == NULL: khong vao group). */
lv_obj_t *mk_hit(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                 lv_event_cb_t cb, void *ud);
/** Icon cua Marlin (LVGL ALPHA_4BIT): mau ve bang recolor -> doi theo trang thai. */
lv_obj_t *mk_icon(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const lv_img_dsc_t *src,
                  lv_color_t col);
/** Doi mau icon (Marlin doi mau bitmap theo nhiet do / trang thai). */
void icon_color(lv_obj_t *im, const lv_color_t col);

// ---- NUT TRONG HOP THOAI: THONG NHAT moi dialog (yeu cau nguoi dung 2026-10-15) ----------------
// "cac nut bam o cac dialog can vao giua dialog, co khoang trang giua cac nut, nut OK luon mau xanh
//  va huy hoac xoa luon mau do. Ban phim touch chua co mau nen nen khong nhin ro chu."
//
// Truoc day moi dialog tu ve nut mot kieu: co cho dung `mk_btn_grow` (nut dan het be rong -> cac nut
// dinh nhau, khong can giua), co cho dung `lv_msgbox` (nut nam trong `lv_btnmatrix`, be rong chia deu
// ca hop). Nay dung 3 muc nay cho TAT CA:
//   - mk_dlg_row(): hang chua nut, CAN GIUA hop, khoang trang deu nhau (DLG_BTN_GAP);
//   - mk_dlg_btn(): nut be rong CO DINH + mau nen chi dinh (xanh = dong y, do = huy/xoa);
//   - style_keyboard(): ban phim lv_keyboard co NEN + VIEN + mau phim/chu ro rang.
// Mau nen: vang xam (COL_BTN) cho nut thuong, xanh la dam (DLG_OK_BG) cho OK/dong y, do dam
// (DLG_CANCEL_BG) cho Huy/Xoa/Cancel. Chu tren nut la trang tuyet doi de dat tuong phan tren ca 2 mau.
#define DLG_BTN_W      62                                   // be rong 1 nut trong hop thoai
#define DLG_BTN_H      28                                   // cao 1 nut trong hop thoai
#define DLG_BTN_GAP    8                                    // khoang trang giua 2 nut
#define DLG_OK_BG      lv_color_hex(0x1B5E20)               // xanh la dam (Material green 900): chu trang 5.5:1
#define DLG_CANCEL_BG  lv_color_hex(0xB71C1C)               // do dam (Material red 800): chu trang 4.8:1
#define DLG_NEUTRAL_BG COL_BTN                              // nut thuong (-, +, muc buoc, ...)
#define DLG_TXT        lv_color_hex(0xFFFFFF)               // chu tren nut cua hop thoai (trang tuyet doi)
// BAN PHIM TOUCH (lv_keyboard) - kich thuoc + vi tri dung chung cho man WIFI va hop dat ten vat lieu.
// VI SAO chieu cao CO DINH 144 px: `lv_btnmatrix` chia DEU chieu cao cho so HANG
// (lv_btnmatrix.c: content_height / row_cnt) -> de cao 194 px thi 4 hang x ~46 px = phim bi keo CAO,
// hang cuoi sat mep duoi va bi che (nguoi dung bao 2026-10-15: "chu cao qua nen hang cuoi bi tut va
// bi che"). 144 px -> moi hang ~35 px, va CAN DAY man hinh nen khong bao gio bi cat.
// Chieu rong = CA man hinh (320) de phim fill het le trai/phai (truoc day le 4 px + padding).
#define KB_H           144
#define KB_TA_H        34                                   // o nhap (textarea) phia tren ban phim
/** Hang nut cua HOP THOAI: can GIUA hop, cach nhau DLG_BTN_GAP (khong dan het be rong). */
lv_obj_t *mk_dlg_row(lv_obj_t *parent, lv_coord_t w, lv_coord_t h);
/** Nut trong hop thoai: be rong CO DINH + mau nen chi dinh (DLG_OK_BG / DLG_CANCEL_BG / DLG_NEUTRAL_BG). */
lv_obj_t *mk_dlg_btn(lv_obj_t *row, const char *txt, lv_coord_t w, lv_coord_t h,
                     lv_color_t bg, lv_event_cb_t cb, void *ud);
/** Trang tri ban phim `lv_keyboard` + o nhap `ta` cho de nhin (nen dam, vien sang, chu trang). */
void style_keyboard(lv_obj_t *kb, lv_obj_t *ta);

// ---- Dai mau theo NHIET DO TUYET DOI (man STATUS: icon + so cua dau in / ban in) ----------------
// YEU CAU NGUOI DUNG 2026-10-15: "min max hot-end la 10-250; bed 0-60" -> MOC TREN lay dung y (250 / 60).
// MOC DUOI thi KHONG lay 10 / 0 ma keo len MR_TEMP_AMB = 25 do C, VI SAO:
//   - 10 va 0 deu THAP HON nhiet do phong (~29-31 do C trong phong nay) nen khong bao gio dat duoc -> lay lam
//     moc dau thi ca mot doan dai (0..25) khong bao gio dung toi, phan dai con lai bi nen lai.
//   - Nang hon: ban in NGUOI luc nghi = 28.8 do C, voi dai 0-60 thi da o 48% -> mau VANG du heater dang TAT,
//     nhin nhu dang nong. Lay 25 lam moc dau thi ban in nguoi = ~11% -> VAN XANH, dung thuc te.
//   => 2 dau thanh: hotend 25..250 do C, bed 25..60 do C (moc giua MR_WARM).
// Xem temp_ramp() o ui_page_status.cpp (co ca phan VI SAO khong con chia theo target).
#define MR_TEMP_AMB      25
#define MR_TEMP_HOT_MIN  MR_TEMP_AMB
#define MR_TEMP_HOT_MAX  250
#define MR_TEMP_BED_MIN  MR_TEMP_AMB
#define MR_TEMP_BED_MAX  60
/** Mau cho 1 nhiet do trong dai [tmin,tmax]: MR_COLD -> MR_WARM -> hot.
 *  DON VI: CA 3 tham so nhiet do deu la DO C (khong phai 1/100 do C nhu temp_lookup() tra ve) - nho chia 100. */
lv_color_t temp_ramp(int16_t cur, int16_t tmin, int16_t tmax, lv_color_t hot);
/** Nhan 1 dong, can PHAI trong be rong w (Marlin can so theo toa do x phai). */
lv_obj_t *mk_label_right(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                         const char *txt, lv_color_t col);
/** Nhan 1 dong can GIUA trong be rong w. */
lv_obj_t *mk_label_mid(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w,
                       const char *txt, lv_color_t col);
/** Man kin 320x240 theo bo cuc Marlin, KHONG flex, khong padding (chi man STATUS). */
lv_obj_t *mk_page_full(void);
/** Focus vao vat bam duoc dau tien trong 1 nhanh (encoder bat dau o vat dau man moi). */
bool focus_first_in(lv_obj_t *o);
/** Container noi dung cua 1 man con: flex COLUMN, padding PAGE_PAD, cach nhau GAP. */
lv_obj_t *mk_page(lv_obj_t *parent);
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_chrome.cpp
// ---------------------------------------------------------------------------
/** "Co link voi Marlin" - xet CA HAI giao thuc loai tru nhau (PUSH: cydlcd / TRAM: tbt). */
bool link_ok(void);
/** Trang thai link 3 muc: 1 = LINK OK, 2 = LINK ERR (co byte toi nhung khong thanh dong), 0 = NO LINK. */
uint8_t link_state_now(void);
/** Cuon noi dung man dang mo them 1 "trang" (dung lai o 2 dau); dir > 0 = NEXT, dir < 0 = PREV. */
void page_scroll(const int8_t dir);
/** Bat/tat thanh day [PREV][TRANG x/y][NEXT] theo noi dung THUC TE cua man dang mo. */
void bot_sync(const bool remeasure);
/** Thanh day dang HIEN khong (true = noi dung man dang mo BI TRAN) - cho chan doan `UI BOX`. */
bool bot_shown(void);
/** Tao icon WiFi (font NHO, khe phai hang nhiet do) + chu dBm tren man STATUS + giu icon the SD. */
void status_icons_create(lv_obj_t *p, lv_obj_t *sd_icon);
/**
 * Quen 3 vat trang thai cua man STATUS (icon WiFi + chu dBm + icon the SD 64x64). Goi tu
 * page_status_destroy(): neu trang STATUS bi thu hoi thi 3 con tro do se tro vao vat DA BI XOA,
 * ma wifi_icon_mark() / wifi_rssi_mark() / status_sd_icon_mark() lai chay dinh ky (~400 ms) o
 * MOI trang -> phai quen di truoc.
 */
void status_icons_forget(void);
/** Cap nhat icon WiFi (header + STATUS) theo trang thai moi; chi ghi style khi trang thai DOI. */
void wifi_icon_mark(void);
/** Cap nhat CHU do manh tin hieu ("-58" / "--") canh icon WiFi tren man STATUS (chi ghi khi DOI). */
void wifi_rssi_mark(void);
/** RSSI cua ket noi hien tai (dBm, 0 = khong biet) - cho ui_chrome.cpp ve chu canh icon WiFi. */
int8_t ui_wifi_rssi(void);
/** Doi mau icon THE SD co san cua Marlin theo trang thai the TF cua MAN HINH CYD. */
void status_sd_icon_mark(void);
/** Tao dong tieu de chung [HOME][ten man]...[ten may][wifi][II][][BACK] + thanh day. */
void build_chrome(void);
/**
 * Doi ngon ngu: dat lai CHU cua cac vat CO DINH tren chrome (nut BACK / PREV / NEXT). Ten man do
 * `page_rebuild()` dat lai, con "TRANG x/y" va o ten may/LINK ERR thi luot cap nhat tu dat lai.
 */
void chrome_lang_retext(void);
/**
 * Doi ngon ngu (goi SAU `ui_lang_set()`): dat lai chu chrome + THU HOI/TAO LAI trang dang mo va dong
 * cac lop phu dang mo. Dinh nghia o ui_lvgl.cpp (xem ghi chu o do).
 */
void ui_lang_apply_ui(void);
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_boxes.cpp
// ---------------------------------------------------------------------------
/** Hop XAC NHAN dung chung: OK moi chay lenh; steps = 2 -> phai bam OK 2 lan. */
void ui_confirm(const char *title, const char *txt, uint8_t action, uint8_t steps);
/** Tu dong dong hop XAC NHAN sau `ms` mili giay (0 = tat) - dung cho hop TEST tu console. */
void ui_confirm_autoclose(const uint32_t ms);
/**
 * CHAN DOAN (lenh `UI TAP K` tren COM7): bam nut OK cua hop XAC NHAN dang mo, di dung duong su kien
 * that (chon nut 0 cua btnmatrix roi ban VALUE_CHANGED vao btnmatrix - no co EVENT_BUBBLE len hop).
 * Hop xac nhan 2 buoc => phai goi 2 lan moi chay lenh that (vd xoa file/thu muc cua man FILE).
 */
void boxes_test_ok(void);
/** In ra HOP THOAI nao dang mo (prompt/XAC NHAN/THONG BAO/PID) + chu cua no - cho chan doan `UI BOX`. */
void boxes_dump(void);
/** Tra focus ve vat dau tien cua man dang mo (sau khi dong hop thoai). */
void refocus_page(void);
/** Cap nhat prompt cua Marlin + thong bao + hop "tune xong" (goi moi vong ui_lvgl_task). */
void boxes_update(void);
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_lvgl.cpp (duoc cac file khac goi)
// ---------------------------------------------------------------------------
/** Trang thai WiFi cho icon o dong tieu de: 0 = tat/chua cau hinh, 1 = dang ket noi, 2 = da ket noi. */
uint8_t wifi_state_now(void);
/** Co API WiFi khong (nhanh firmware chua merge -> icon/trang bao "khong ho tro"). */
bool wifi_api_ok(void);
/** Trang thai the SD (SD_ST_*) cho icon the tren man STATUS. */
uint8_t sd_state_now(void);
// ---------------------------------------------------------------------------
//  WiFi (weak) + accessor an toan - dinh nghia trong ui_lvgl.cpp, dung o nhieu file
// ---------------------------------------------------------------------------
/** SSID cua mang dang ket noi ("" neu khong co/khong biet) - ui_page_info.cpp. */
const char *wifi_ssid_now(void);
/** IP cua man hinh CYD ("" neu chua ket noi) - ui_page_info.cpp. */
const char *wifi_ip_now(void);
/** RSSI cua ket noi hien tai (dBm, 0 = khong biet) - ui_page_info.cpp + ui_wifi_rssi(). */
int8_t wifi_rssi_now(void);
/** Hien 1 man (tao neu chua co), cap nhat dong tieu de + focus + thanh day. */
void show_page(uint8_t p);
/** Ten trang cho dong tieu de: PG_TITLE[p], rieng PG_SUBMENU lay theo NHOM dang mo. */
const char *page_title(uint8_t p);
/**
 * Dung lai noi dung cua 1 trang DANG MO (thu hoi roi tao lai + cap nhat tieu de/focus/thanh day).
 * Can cho cac trang ma noi dung phu thuoc BIEN TOAN CUC chu khong chi du lieu Marlin: hien tai la
 * PG_SUBMENU (doi nhom menu khi dang o trong trang do).
 */
void page_rebuild(uint8_t p);
/** Chay lenh THAT SU cua cac muc phai xac nhon (M502 / reboot DFU / xoa file SD). */
void do_confirmed(const uint8_t act);
/** Chay 1 hanh dong A_*: gui G-code / mo man / mo lop phu SUA (dung cho ca nut bam lan lop phu). */
void do_action(const int a);
/** Callback CLICKED cua nut: chuyen user_data thanh chi so hanh dong A_* roi goi do_action(). */
void cb_action(lv_event_t *e);
// ---------------------------------------------------------------------------
//  Dinh nghia trong 6 file trang + ui_preheat.cpp (chang 2/4)
//  (menu mo bang nut SETTINGS tren STATUS; STATUS dung kin 320x240, khong co dong tieu de)
// ---------------------------------------------------------------------------
/** Man STATUS: 3 o nhiet/quat + hang XYZ + FR/FL + nut SD/SETTINGS + thoi gian + tien do. */
void page_status_create(lv_obj_t *p);
/** Cap nhat so lieu man STATUS: 3 o nhiet/quat, XYZ, FR/FL, thoi gian, %, dong trang thai. */
void page_status_update(void);
/** Man MENU: danh sach muc (menu doc kieu Marlin) + dong "Calibrate touch" tren board co touch. */
void page_menu_create(lv_obj_t *p);
/** Cap nhat nhan cac muc MENU (mo/lam mo nut theo trang thai may + link). */
void page_menu_update(void);
/** Man TEMP: +/- nhiet dau in/ban in, hang FAN, cac dong preheat + COOLDOWN (nut tuning da bo). */
void page_temp_create(lv_obj_t *p);
/** Cap nhat nhiet hien tai/target, gia tri FAN va 3 nut preset cua man TEMP. */
void page_temp_update(void);
/** Ve lai nhan 3 nut preset PLA/PETG/ABS theo kho preheat (ui_preheat_set goi sau khi sua preset). */
void tp_pre_sync(void);
/**
 * CHAN DOAN: ban su kien CHAM vao nut preset thu `k` (0=PLA, 1=PETG, 2=ABS) cua man TEMP - lenh
 * `UI PRE <k>` tren COM7. Di dung duong su kien that (cb_temp_preheat) de kiem tra tu PC.
 */
void temp_test_preheat(const uint8_t k);
/** Man FEED/FLOW: +/- M220/M221 + RESET 100% (2 o FR/FL tren STATUS mo vao day). */
void page_rate_create(lv_obj_t *p);
/** Cap nhat 2 nhan FR/FL theo M220/M221 cua Marlin. */
void page_rate_update(void);
/** Man JOG: chon truc + cac buoc nhich + HOME/M84 + G28 tung truc + lop phu sua M851 Z. */
void page_jog_create(lv_obj_t *p);
/** Cap nhat toa do 3 truc tren man JOG (chua home -> '?') + to nut truc dang chon. */
void page_jog_update(void);
/** Nap danh sach vat lieu preheat tu NVS luc khoi dong (ui_preheat.cpp; co di tru tu namespace "ph" cu). */
void preheat_load(void);
/** Ghi danh sach vat lieu preheat xuong NVS (CYD); board khac chi giu RAM. */
void preheat_save(void);
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_page_files.cpp (chang 3/4)
// ---------------------------------------------------------------------------
/** Man FILE SD: dong info + FILES_ROWS dong file (moi dong la 1 nut: cham = chon + to sang) + 3 hang nut. */
void page_files_create(lv_obj_t *p);
/** Cap nhat dong info + cac dong file (chi ve lai danh sach khi dau van tay / so file DOI). */
void page_files_update(void);
/**
 * Nha + init lai the SD roi moi xin danh sach file: gui M22 (nha the) + M21 (mount lai) xuong Marlin,
 * sau do dat hen gioi FILES_JOB_DELAY_MS moi xin danh sach (lenh di qua HANG DOI cua Marlin nen xin
 * ngay se nhan danh sach CU). Goi tu show_page() moi lan VAO man FILE, va tu nut DIR/REFRESH.
 */
void files_init_sd(void);
/**
 * Xoa dang chon: FILE -> net_sd_delete_file() (the o khe MAN HINH CYD) hoac M30; THU MUC RONG ->
 * net_sd_delete_dir() (M30 cua Marlin KHONG xoa duoc thu muc). Con muc con thi chi bao ghi chu.
 */
void files_delete_now(void);
/**
 * "Viec cham" cua man FILE, goi o nhip 400 ms trong ui_lvgl_task() ke CA khi da roi man:
 * init lai the SD / xoa file da het thoi gian cho -> xin lai danh sach + mo khoa nut + xoa ghi chu.
 */
void files_job_task(void);
/**
 * CHAN DOAN man FILE (lenh `UI TAP <gi>` tren COM7): ban su kien CHAM vao 1 dong cua TRANG dang xem
 * (`0..FILES_ROWS-1`), vao nut PREV (`P`) / NEXT (`N`) hoac nut XOA FILE (`D`); khong tham so = in
 * trang thai. Di DUNG duong su kien that (cb_files_row/cb_files_prev/...) de kiem tra tu PC.
 */
void files_test_cmd(const char *arg);
/** CHAN DOAN: in so file / trang dang xem / DONG DANG CHON cua man FILE (goi tu `UI BOX`). */
void page_files_dump(void);
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_page_settings.cpp (chang 3/4)
// ---------------------------------------------------------------------------
/** Man SETTINGS: bang thong so (SET_ROWS) + UP/DOWN/SAVE EEPROM + dong info. */
void page_settings_create(lv_obj_t *p);
/** Cap nhat bang thong so (chi ve lai khi Marlin gui gia tri moi), sau khi bom preset preheat. */
void page_settings_update(void);
/** Dong lop phu SUA so (ov_*). show_page() goi TRUOC khi thu hoi trang vi lop phu thuoc lv_scr_act. */
void ov_hide(void);
/** Mo lop phu SUA cho 1 dong thong so theo id (CYDLCD_SET_*). */
void ov_open_id(const uint16_t id);
/** true = Marlin DA GUI gia tri cho id nay - dung de AN cac dong/dong menu khong co du lieu. */
bool settings_has_value(uint16_t id);
/** Format gia tri HIEN TAI cua `id` theo dung so le cua bang (false = khong co dong/gia tri). */
bool settings_fmt_cur(char *b, size_t n, uint16_t id);
/**
 * Dung lop phu SUA neu dang mo (nut BACK bam khi hop dang mo thi chi DONG HOP, khong doi trang).
 * true = vua dong -> noi goi KHONG duoc lam gi them.
 */
bool ov_close_open(void);
/** Lop phu SUA so co dang MO khong (chan doan `UI HIT`). */
bool ov_is_open(void);
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_page_submenu.cpp (chang 3/4) - MENU CON kieu 12864
// ---------------------------------------------------------------------------
//  MOT trang (PG_SUBMENU) dung chung cho ca cay menu "Configuration": bang SM_GROUPS/SM_ITEMS
//  (const -> nam o FLASH) quyet dinh nhom nao co dong nao. Dong `SM_VAL` tro vao bang SET_ROWS cua
//  ui_page_settings.cpp theo ID -> dung lai nguyen lop phu SUA (-/+/OK/buoc) khong phai viet lai.
/** Man MENU CON: ve cac dong cua nhom `s_sm_g` (dong nao khong co gia tri thi tu an). */
void page_submenu_create(lv_obj_t *p);
/** Cap nhat cot gia tri cua cac dong dang hien (nhip CHAM 400 ms nhu trang SETTINGS). */
void page_submenu_update(void);
/** Huy man MENU CON (dat NULL cac con tro widget cua trang - xem quy uoc o cuoi file nay). */
void page_submenu_destroy(void);
/** Mo 1 NHOM cua cay (dang o chinh trang nay thi DUNG LAI noi dung, khong tao trang moi). */
void submenu_open(uint8_t g);
/** Nhom CHA cua nhom dang mo; 255 = dang o nhom GOC -> BACK phai ve MENU. */
uint8_t submenu_parent(void);
/** Gia tri `parent` bao "KHONG co nhom cha" (nhom GOC) - tra ve boi submenu_parent(). */
#define SM_PARENT_NONE 255
/** Ten nhom dang mo, dung cho dong tieu de chung (thay PG_TITLE[PG_SUBMENU]). */
const char *submenu_title(void);
/** Dua trang ve nhom GOC - show_page() goi khi MO trang nay tu mot man KHAC (giong Marlin). */
void submenu_reset_root(void);
/**
 * Luu VI TRI CUON cua NHOM dang xem (goi tu show_page TRUOC khi thu hoi trang MENU CON).
 * VI SAO theo NHOM chu khong theo trang: ca cay menu dung CHUNG 1 trang (PG_SUBMENU) nen mot mang
 * theo trang la khong du - moi nhom co danh sach dai ngan khac nhau.
 */
void submenu_save_scroll(void);
/** Vi tri cuon da luu cua NHOM dang mo (0 = dau danh sach) - de show_page() tra lai cho dang xem. */
lv_coord_t submenu_scroll_y(void);
/** Focus + cuon toi DONG da mo nhom con (goi sau khi trang MENU CON da hien lai). */
void submenu_focus_parent(void);
/** Focus + cuon toi DONG da mo mot man khac cua trang MENU (goi sau khi trang MENU da hien lai). */
void page_menu_focus_sel(void);
/** Mo menu MOTION (nhom ngang cap CONFIGURATION, giong muc "Motion" trong Main menu cua 12864). */
void submenu_open_motion(void);
/** NHOM menu con dang mo (show_page day vao ngan xep BACK). */
uint8_t submenu_group(void);
/** DONG (chi so item) vua duoc bam trong nhom dang mo; 255 = khong co (de BACK ve thi to sang dong do). */
uint8_t submenu_last_item(void);
/**
 * Mo lai 1 NHOM tu NGAN XEP BACK va TO SANG dong `hi` (dong nguoi dung da bam de roi khoi nhom do).
 * Khac submenu_open(): khong tu tinh lai dong can to sang (vi luc quay lai khong con quan he cha/con).
 */
void submenu_open_restore(uint8_t g, uint8_t hi);
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_page_preheat.cpp (chang 3/4) - HOP SUA PREHEAT 3-TRONG-1 + ban phim dat ten
// ---------------------------------------------------------------------------
/** Mo hop sua 3-trong-1 (nozzle/bed/fan CUNG 1 HANG) cho vat lieu thu `slot` cua kho preheat. */
void preheat_edit_open(uint8_t slot);
/** Dong hop sua (khong luu) + tra focus. show_page() goi khi doi trang (hop thuoc lv_layer_top). */
void preheat_edit_hide(void);
/** Hop sua 3-trong-1 co dang mo khong (chan doan `UI PRE` / `UI HIT`). */
bool preheat_edit_active(void);
/** Mo BAN PHIM dat ten vat lieu moi (nut "+ Them vat lieu"); bao loi neu kho da du CYDLCD_PRE_MAX. */
void preheat_add_open(void);
/** Dong ban phim dat ten neu dang mo (idempotent) - show_page() goi khi doi trang. */
void preheat_kb_hide(void);
/** Ban phim dat ten co dang mo khong. */
bool preheat_kb_active(void);
/** Lenh THAT cua nut "Xoa" (goi tu do_confirmed(A_PRE_DELETE) sau khi nguoi dung xac nhan 2 buoc). */
void preheat_delete_pending(void);
// ---------------------------------------------------------------------------
//  Dinh nghia trong ui_page_mesh.cpp / ui_page_custom.cpp / ui_page_endstop.cpp /
//  ui_page_info.cpp / ui_page_wifi.cpp / ui_page_tbt.cpp (chang 4/4)
// ---------------------------------------------------------------------------
/** Man BED MESH: luoi bed mesh dung het be rong + 2 hang nut UBL (G29). */
void page_mesh_create(lv_obj_t *p);
/** Cap nhat luoi bed mesh tu frame Marlin (dung lai bang khi so cot/hang DOI). */
void page_mesh_update(void);
/** Man CUSTOM: G34 / Init SD / M119 endstop / M412 filament / reboot DFU. */
void page_custom_create(lv_obj_t *p);
/** Cap nhat trang CUSTOM: to lai nut M412 theo trang thai cam bien filament. */
void page_custom_update(void);
/** Man ENDSTOP: cac o trang thai kieu M119 (frame CYDLCD_F_ENDSTOPS). */
void page_endstop_create(lv_obj_t *p);
/**
 * Man ENDSTOP - duong RIENG 30 Hz (2026-10-15, yeu cau nguoi dung "doc tin hieu M115 30 lan 1 giay"):
 * xin lai `CYDLCD_REQ_ENDSTOPS` moi 30 ms va ve lai cac nhan. CHI duoc goi tu `ui_lvgl_task_body()` khi
 * `s_page_act == PG_ENDSTOP` (ra khoi man = ngung monitor) va PHAI goi trong khoa trang thai.
 */
void page_endstop_poll(void);
/** Cap nhat cac o trang thai endstop tu frame CYDLCD_F_ENDSTOPS (luot chung UI_REFRESH_MS). */
void page_endstop_update(void);
/** Man INFO: 11 dong thong tin THAT (firmware, board, font, WiFi, protocol, link, SD, he thong). */
void page_info_create(lv_obj_t *p);
/** Dung lai 11 dong thong tin cua man INFO tu du lieu Marlin / cua man hinh. */
void page_info_update(void);
/** Man WIFI: trang thai + quet mang + nhap mat khau + ket noi/ngat/quen cau hinh (NVS). */
void page_wifi_create(lv_obj_t *p);
/** Cap nhat trang thai quet mang + ket noi WiFi (SSID/IP/RSSI) tren man WIFI. */
void page_wifi_update(void);
/**
 * Don moi thu CUA TRANG WIFI khi roi trang: dong ban phim nhap mat khau + lop phu "DANG QUET..."
 * (ca 2 la con cua lv_scr_act, KHONG phai con cua trang) va huy phien quet dang chay (mo lai input).
 * Goi tu show_page() moi khi doi man, va tu page_wifi_destroy() (idempotent).
 */
void wifi_page_leave(void);
/** Man TRAMMING: dieu khien Bed Tramming Wizard qua link ASCII (4 so 4 goc + 4 nut lenh). */
void page_tbt_create(lv_obj_t *p);
/** Cap nhat man TRAMMING tu dong 'TBT ...' cua Marlin (4 goc, huong dan van, mode). */
void page_tbt_update(void);

// ---------------------------------------------------------------------------
//  HUY TRANG (page recycling) - vong doi bo nho cua cac man UI
// ---------------------------------------------------------------------------
//  VI SAO CAN: moi man duoc tao LAZY (show_page -> create_page) va truoc day KHONG BAO GIO duoc
//  giai phong -> mo dan nhieu man la het pool LVGL (LV_MEM_SIZE): do that 2026-10-15 bang log
//  `[CYD] lvmem: used/49152 B`: STATUS 13,4 KB -> +MENU 21,5 -> +TRAMMING 26,5 -> +FEED/FLOW 32,8
//  -> +NHIET DO 43,5 -> mo tiep JOG thi `lv_mem_realloc: couldn't allocate memory` +
//  `lv_obj_add_event_cb: Asserted ... (Out of memory)`. Vi LV_USE_ASSERT_NULL/MALLOC bat san va
//  handler la `while(1)` (xem cydlcd_assert.h) nen panel TREO IM LANG (khong log, khong tra loi
//  console, WiFi chet). Tu day show_page() THU HOI trang CU truoc khi hien trang moi -> bo nho
//  LVGL co CHAN TREN (chi con trang STATUS + trang dang mo).
//
//  QUY UOC CUA MOI ham page_<ten>_destroy():
//    - CHI dat NULL cac con tro widget `static` CUA CHINH TRANG DO. KHONG goi lv_obj_del() cho
//      tung widget con: container trang do show_page() xoa bang lv_obj_del() -> LVGL xoa ca cay con,
//      tu go chung khoi `s_group` va tu refocus (xem page_release() trong ui_lvgl.cpp).
//    - Tra trang thai RIENG CUA TRANG (dem/cache cua trang) ve gia tri ban dau NEU lan tao lai can
//      (vd dem cache danh sach file, kich thuoc luoi mesh da ve, co "da dong bo" cua nut M412).
//      KHONG cham vao du lieu nghiep vu (cydlcd.* / tbt.*) hay kho cua man hinh (NVS preheat/WiFi).
//    - An toan khi goi luc trang CHUA tung duoc tao, hoac goi LAP nhieu lan (idempotent).
//
//  TRANG STATUS: van co page_status_destroy() cho du bo API, nhung show_page() KHONG BAO GIO goi
//  (xem chu thich o show_page() trong ui_lvgl.cpp): STATUS la trang goc + la noi moi nut BACK tro
//  ve, va icon WiFi / icon the SD tren no duoc cap nhat dinh ky (~400 ms) tu ui_lvgl_task() cho MOI
//  trang -> giu lai cho chac (thu hoi cung duoc nhung loi ich khong dang).
// ---------------------------------------------------------------------------
/** Huy man STATUS (dat NULL con tro widget + quen 2 icon trang thai). show_page() khong goi. */
void page_status_destroy(void);
/** Huy man MENU (mn_val[]/mn_val_cal + huy trang thai "dang giu" dong Calibrate touch). */
void page_menu_destroy(void);
/** Huy man TEMP (nhan nhiet + 3 nut preset). Giu `s_profile` (lua chon cua nguoi dung). */
void page_temp_destroy(void);
/** Huy man FEED/FLOW (2 nhan FR/FL). */
void page_rate_destroy(void);
/** Huy man JOG (3 nut truc). Giu `s_axis` (truc nguoi dung dang chon). */
void page_jog_destroy(void);
/** Huy man FILE SD (cac dong file + dong info + nut) va xoa cache danh sach de ve lai danh sach. */
void page_files_destroy(void);
/** Huy man SETTINGS (bang + dong info). Lop phu SUA (ov_*) thuoc lv_scr_act, KHONG xoa o day. */
void page_settings_destroy(void);
/** Huy man BED MESH (bang luoi + 2 nhan) va quen kich thuoc luoi da ve. */
void page_mesh_destroy(void);
/** Huy man CUSTOM (nut M412). */
void page_custom_destroy(void);
/** Huy man TRAMMING (nhan dau + 4 goc + dong huong dan). */
void page_tbt_destroy(void);
/** Huy man INFO (11 nhan). */
void page_info_destroy(void);
/** Huy man ENDSTOP (6 nhan gia tri + dong info). */
void page_endstop_destroy(void);
/** Huy man WIFI (3 nhan + nut mat khau + roller). Don ban phim/lop phu quet truoc khi xoa trang. */
void page_wifi_destroy(void);

#endif  // CYDLCD_USE_LVGL
