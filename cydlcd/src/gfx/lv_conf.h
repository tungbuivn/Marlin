/**
 * lv_conf.h -- cau hinh LVGL 8.3 cho MAN HINH CYD (chi build khi CYDLCD_USE_LVGL = 1).
 *
 * Vi sao toi gian: LVGL 8.3 co san gia tri mac dinh cho MOI tuy chon trong
 * `lv_conf_internal.h` (moi muc la `#ifndef LV_X  #define LV_X <default>  #endif`), nen o day chi
 * can khai bao nhung muc KHAC mac dinh + tat cac widget khong dung cho nhe flash.
 *
 * File nay duoc LVGL include qua `-DLV_CONF_INCLUDE_SIMPLE` + `-I<PROJECT_DIR>/src`
 * (xem platformio.ini, env *_lvgl).
 *
 * LUU Y: khong include gi o day ngoai <stdint.h>. Cac board define (CYDLCD_BOARD_*) den tu
 * build_flags cua PlatformIO nen co ca khi bien dich cac file .c cua LVGL.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ---------------------------------------------------------------------------
 * Mau / bo nho
 * -------------------------------------------------------------------------*/
#define LV_COLOR_DEPTH      16
/* 0: buffer giu nguyen little-endian nhu LVGL sinh ra; phan swap byte duoc lam
 * trong ui_lvgl_port.cpp (STM32: dao 2 byte khi day SPI, CYD: tft.setSwapBytes). */
#define LV_COLOR_16_SWAP    0

#define LV_MEM_CUSTOM       0
#if defined(CYDLCD_BOARD_CYD) || defined(CYDLCD_BOARD_F407)
  #define LV_MEM_SIZE       (48U * 1024U)   /* ESP32 ~320KB RAM / F407 128KB+CCM */
#else
  #define LV_MEM_SIZE       (20U * 1024U)   /* F411CC 128KB RAM: de danh RAM cho buffer ve */
#endif
#define LV_MEM_ADR          0               /* 0 = LVGL tu lay mang tinh */

/* DPI: 100 cho man 2.4"/2.8" -> widget mac dinh nho gon hon (mac dinh LVGL la 130). */
#define LV_DPI_DEF          100

/* Tick: UI tu goi lv_tick_inc(millis delta) trong ui_lvgl_task() -> khong dung tick custom. */
#define LV_TICK_CUSTOM      0

/* Chu ky ve / doc input (ms) */
#define LV_DISP_DEF_REFR_PERIOD   30
#define LV_INDEV_DEF_READ_PERIOD  20

/* ---------------------------------------------------------------------------
 * Font: 14px la MAC DINH (du cho 240x320), tat het font khac cho nhe flash.
 * 2026-10-15: BẬT THEM Montserrat 16 cho HANG X/Y/Z cua man STATUS (yeu cau nguoi dung: "font X,Y,Z o trang
 * status to hon 1 chut"). O XYZ cao 24 px, chu 16 px co line_height 19 + le tren 3 = 22 <= 24 nen VAN VUA
 * (chu 18 px line_height 22 -> 25 > 24 la tran, nen 16 la muc to nhat giu nguyen bo cuc).
 * -------------------------------------------------------------------------*/
#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_14

/* ---------------------------------------------------------------------------
 * Widget CAN dung
 * -------------------------------------------------------------------------*/
#define LV_USE_LABEL        1
#define LV_USE_BTN          1
#define LV_USE_BTNMATRIX    1   /* lv_msgbox BAT BUOC can (khong co se #error) */
#define LV_USE_MSGBOX       1   /* prompt cua Marlin + thong bao */
#define LV_USE_LIST         1
#define LV_USE_TABLE        1   /* man SETTINGS + MESH */
#define LV_USE_BAR          1   /* thanh tien do */
#define LV_USE_ROLLER       1   /* danh sach file SD */
#define LV_USE_IMG          1   /* icon 64x64 cua Marlin (ALPHA_4BIT + recolor) */
#define LV_USE_FLEX         1   /* lv_list dung flex layout */

/* Bang: mesh toi da 16x16 diem (CYDLCD_MESH_MAX = 16) -> can 16 cot */
#define LV_TABLE_COL_MAX    16

/* ---------------------------------------------------------------------------
 * Widget KHONG dung -> tat (tiet kiem flash; khong widget nao o tren phu thuoc chung)
 * -------------------------------------------------------------------------*/
#define LV_USE_ANIMIMG      0
#define LV_USE_ARC          0
#define LV_USE_CALENDAR     0
#define LV_USE_CANVAS       0
#define LV_USE_CHART        0
#define LV_USE_CHECKBOX     0
#define LV_USE_COLORWHEEL   0
#define LV_USE_DROPDOWN     0
#define LV_USE_IMGBTN       0
#define LV_USE_KEYBOARD     0
#define LV_USE_LED          0
#define LV_USE_LINE         0
#define LV_USE_METER        0
#define LV_USE_SLIDER       0
#define LV_USE_SPAN         0
#define LV_USE_SPINBOX      0
#define LV_USE_SPINNER      0
#define LV_USE_SWITCH       0
#define LV_USE_TABVIEW      0
#define LV_USE_TEXTAREA     0
#define LV_USE_TILEVIEW     0
#define LV_USE_WIN          0

/* ---------------------------------------------------------------------------
 * Theme: TAT ca. UI tu set mau cho tung doi tuong (xem ui_widgets.cpp / ui_page_*.cpp) nen khong phu thuoc
 * theme, va tiet kiem ~10KB flash. (lv_msgbox van chay: no chi goi API lv_theme_get_*,
 * ham nay tra ve mau mac dinh khi khong co theme.)
 * -------------------------------------------------------------------------*/
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_BASIC   0

/* Khong dung printf cua LVGL voi so thuc (tranh keo float printf vao firmware) */
#define LV_SPRINTF_CUSTOM    0
#define LV_SPRINTF_USE_FLOAT 0

#endif /* LV_CONF_H */
