/**
 * gfx4_font_robotomono.c -- noi duy nhat include header font 4bpp (16 muc xam).
 *
 * KHAC voi gfx_font_robotomono.h (1 bit/pixel, dung dinh dang Adafruit GFXfont):
 * header 4bpp o day la DINH DANG RIENG CUA TA, ghi thang theo cach LVGL 8.4 doc bitmap
 * bpp4 (chuoi bit LIEN TUC, col chan -> nibble cao, col le -> nibble thap). Vi vay driver
 * tra con tro THANG vao mang bitmap, KHONG phai repack. Xem comment dau header.
 *
 * Header do `tools/ttf2gfxfont.py` sinh ra (mang bitmap/glyph la `static`, 5 doi tuong
 * `const GFXfont ... PROGMEM` va mang `..._ranges[]` co linkage ngoai) nen chi duoc include
 * trong DUNG MOT translation unit (file nay).
 *
 * Sinh lai font (2026-10-15: size 13 -> 15 - YEU CAU NGUOI DUNG "tang co chu cac noi to bang co chu vi
 * tri X,Y,Z o trang STATUS"; dong X,Y,Z dung Montserrat 16 (line_height 18) con font mono ngay truoc
 * do chi cao 9 px net -> nhin nho hon han):
 *   python tools/ttf2gfxfont.py --font D:/fonts/gfxfont/google/RobotoMono-Regular.ttf \
 *       --size 15 --bpp 4 --name gfx4_font_robotomono --first 0x20 --last 0xFF \
 *       --extra-ranges 0x0102-0x0103,0x0110-0x0111,0x01A0-0x01B1,0x1EA0-0x1EF9 \
 *       --range-names ascii,vn1,vn2,vn3,vn4 -o src/gfx4_font_robotomono.h
 *
 * So do cua font dang dung (do chinh tool in ra):
 *   size 15 px, xAdvance 9 px (mono that), yAdvance 20 px, base_line 4 px,
 *   ascender thuc 16, descender thuc 4, box_h max 17, box_w max 10,
 *   5 dai / 336 o / 287 glyph co net / 12797 byte bitmap (~16 KB ca bang glyph, +3 KB flash).
 *
 * ⚠️ DOI SIZE FONT LA DOI CHIEU CAO DONG (17 -> 20 px) nen PHAI xem lai moi cho co chieu cao tinh
 * theo 17: MENU_ROW_H (22 -> 25), FILES_ROW_H (18 -> 21, FILES_ROWS 5 -> 4), ES_ROW_H (20 -> 23),
 * SET_TABLE_H cua man THONG SO (124 -> 122), 2 hang nut man TUNING MPC (28 -> 26), OV_H cua lop phu
 * sua (112 -> 118). Cac trang tran 206 px se TU hien thanh day PREV/NEXT (MENU, CAU HINH, INFO).
 */
#include "CYDLCD_config.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

#include "gfx4_font_robotomono.h"

#else  /* CYDLCD_USE_LVGL */

/* Cac env STM32 khong-LVGL cung compile file nay -> giu 1 khai bao cho translation unit
 * khong rong. */
typedef int gfx4_font_robotomono_not_built_for_this_env;

#endif /* CYDLCD_USE_LVGL */
