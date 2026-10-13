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
 * Sinh lai font:
 *   python tools/ttf2gfxfont.py --font D:/fonts/gfxfont/google/RobotoMono-Regular.ttf \
 *       --size 13 --bpp 4 --name gfx4_font_robotomono --first 0x20 --last 0xFF \
 *       --extra-ranges 0x0102-0x0103,0x0110-0x0111,0x01A0-0x01B1,0x1EA0-0x1EF9 \
 *       --range-names ascii,vn1,vn2,vn3,vn4 -o src/gfx4_font_robotomono.h
 *
 * So do cua font dang dung (do chinh tool in ra - GIONG HET ban 1bpp nen bo cuc UI khong doi):
 *   size 13 px, xAdvance 8 px (mono that), yAdvance 17 px, base_line 3 px,
 *   ascender thuc 14, descender thuc 3, box_h max 14, box_w max 9, |xOffset| max 3,
 *   5 dai / 336 o / 287 glyph co net / 9782 byte bitmap (~12,3 KB ca bang glyph).
 */
#include "CYDLCD_config.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

#include "gfx4_font_robotomono.h"

#else  /* CYDLCD_USE_LVGL */

/* Cac env STM32 khong-LVGL cung compile file nay -> giu 1 khai bao cho translation unit
 * khong rong. */
typedef int gfx4_font_robotomono_not_built_for_this_env;

#endif /* CYDLCD_USE_LVGL */
