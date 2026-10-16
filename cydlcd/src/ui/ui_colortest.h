/**
 * ui_colortest.h -- bang mau de KIEM TRA DUONG MAU cua man hinh (lenh `UI COLORTEST`).
 *
 * VI SAO CO (2026-10-16): nguoi dung bao *"mau tren board that khong giong ban sim"* va hoi
 * *"co phai thu tu RGB khong dung?"*. Duong mau co dung 3 cho co the lam lech mau, va bang nay
 * tach chung ra de biet CHINH XAC cho nao:
 *
 *   1) MAU GOC (file nay + ui_lvgl_int.h): dung `#RRGGBB` chuan, giong `tft_color.h` cua Marlin
 *      (vd `MR_BG = lv_color_hex(0x1E156E)` <=> Marlin `COLOR_BACKGROUND 0x20AC // #1E156E`).
 *      LVGL doi sang RGB565 chuan (R o bit 11..15) => KHONG co gi sai o lop nay.
 *   2) DUONG FLUSH: CYD day pixel bang `pushColors(px, n, swap=true)` - tham so `true` chi DOI 2 BYTE
 *      (endianness cho ST7789), KHONG doi kenh mau. `lv_conf.h`: LV_COLOR_DEPTH 16 + LV_COLOR_16_SWAP 0
 *      => nhat quan (neu bat SWAP 1 thi moi thu sai het).
 *   3) PANEL (TFT_eSPI): thu tu kenh do `TFT_RGB_ORDER` quyet dinh. Trong
 *      `tools/tft_setup_cyd/User_Setup.h` dong nay DANG COMMENT => TFT_eSPI 2.5.43 lay mac dinh
 *      `TFT_MAD_RGB` (TFT_Drivers/ST7789_Defines.h: "TFT_MAD_COLOR_ORDER TFT_MAD_RGB").
 *      Neu PANEL THAT su la BGR thi do la cho lam do<->xanh duong doi nhau.
 *      Ngoai ra `ST7789_Init.h` gui `ST7789_INVON` VO DIEU KIEN (khong doc TFT_INVERSION_ON/OFF!)
 *      => neu panel khong can dao mau thi ca man hinh bi "am ban".
 *
 * CACH DUNG: tren board go `UI COLORTEST` -> man hinh lan luot to 8 mau toan man hinh, moi mau 700 ms,
 * VA in ra Serial mau nao dang hien. Doi chieu:
 *   - Mau do hien ra XANH DUONG  => thu tu kenh sai: bo comment `#define TFT_RGB_ORDER TFT_BGR`
 *     trong tools/tft_setup_cyd/User_Setup.h (hoac nguoc lai: doi sang TFT_RGB).
 *   - Trang <-> den bi nguoc (am ban) => dao mau cua panel: bat `-DCYDLCD_CYD_INVERT_OFF=1`
 *     (xem ui_port_cyd_st7789.cpp: TFT_eSPI 2.5.43 luon gui INVON nen phai tat bang tay sau khi init).
 *   - Mau dung nhu mo ta => duong mau OK, khac biet nam cho khac (do sang/tuong phan/backlight).
 *
 * Ban GIA LAP tren PC (`cydlcd/sim`) dung CHINH bang nay de xuat 8 anh mau -> so sanh truc tiep
 * voi man hinh that (xem cydlcd/sim/README.md, lenh kich ban `checkcolor`).
 */
#pragma once

#include <stdint.h>

// Mau kiem tra: 5 mau co ban (dung de nhan biet dao kenh / am ban) + 3 mau THAT cua UI.
typedef struct { const char *name; uint32_t rgb888; } ui_colortest_bar_t;

static const ui_colortest_bar_t ui_colortest_bars[] = {
  { "RED",    0xFF0000u },   // do  -> neu hien XANH DUONG: dao kenh (BGR)
  { "GREEN",  0x00FF00u },   // la  -> neu hien TIM/HONG: sai ca 2 kenh R<->B
  { "BLUE",   0x0000FFu },   // xanh duong -> neu hien DO: dao kenh (BGR)
  { "WHITE",  0xFFFFFFu },   // trang -> neu hien DEN: panel bi dao mau (inversion)
  { "BLACK",  0x000000u },   // den   -> neu hien TRANG: panel bi dao mau (inversion)
  { "BG",     0x1E156Eu },   // nen Marlin (COLOR_BACKGROUND) - mau nen cua moi trang UI
  { "CYAN",   0x00E5FFu },   // MR_COLD (nhiet do nguoi/qua, thanh tien do)
  { "HOTEND", 0xFF8A80u }    // MR_HOTEND (nhiet do dau in)
};
#define UI_COLORTEST_N ((uint8_t)(sizeof(ui_colortest_bars) / sizeof(ui_colortest_bars[0])))

// #RRGGBB -> RGB565 chuan (R o bit 11..15), CUNG cong thuc voi LVGL va TFT_eSPI
// (TFT_eSPI.h: TFT_RED 0xF800 / TFT_BLUE 0x001F => dung thu tu nay).
static inline uint16_t ui_colortest_rgb565(const uint32_t rgb) {
  return (uint16_t)(((rgb >> 8) & 0xF800u) | ((rgb >> 5) & 0x07E0u) | ((rgb >> 3) & 0x001Fu));
}
