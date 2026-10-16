/**
 * gfx_font_robotomono.c -- noi duy nhat include header font 1-bit Adafruit GFXfont.
 *
 * Header do `tools/ttf2gfxfont.py` sinh ra co:
 *   - mang bitmap/glyph `static` (khong ban ra ngoai)
 *   - 5 doi tuong `const GFXfont ... PROGMEM` (linkage ngoai)
 *   - mang `const GFXfont *const gfx_font_robotomono_ranges[]` (linkage ngoai)
 * nen no chi duoc include trong DUNG MOT translation unit (file nay). Neu include o nhieu
 * file .c se bi loi "multiple definition" khi link.
 *
 * Sinh lai font (vi du doi co chu):
 *   python tools/ttf2gfxfont.py --font D:/fonts/gfxfont/google/RobotoMono-Regular.ttf \
 *       --size 13 --name gfx_font_robotomono --first 0x20 --last 0xFF \
 *       --extra-ranges 0x0102-0x0103,0x0110-0x0111,0x01A0-0x01B1,0x1EA0-0x1EF9 \
 *       --range-names ascii,vn1,vn2,vn3,vn4 --yadvance 17 \
 *       -o src/gfx_font_robotomono.h
 *
 * So do that cua font dang dung (do chinh tool in ra):
 *   size 13 px, xAdvance 8 px (mono that), yAdvance 17 px, base_line 3 px,
 *   ascender thuc 14, descender thuc 3, box_h max 14, box_w max 9, |xOffset| max 3,
 *   5 dai / 336 o / 287 glyph co net / 2926 byte bitmap (~5,6 KB ca bang glyph).
 */
#include "cydlcd_config.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

#include "gfx_font_robotomono.h"

#else  /* CYDLCD_USE_LVGL */

/* Cac env STM32 khong-LVGL cung compile file nay -> giu 1 khai bao cho translation unit
 * khong rong. */
typedef int gfx_font_robotomono_not_built_for_this_env;

#endif /* CYDLCD_USE_LVGL */
