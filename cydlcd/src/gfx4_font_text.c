/**
 * gfx4_font_text.c -- noi duy nhat include header font CHU 4bpp (16 muc xam) cua UI.
 *
 * 2026-10-15: THAY FONT 3 LAN theo yeu cau nguoi dung:
 *   - Truoc: RobotoMono-Regular **mono** size 15 (xAdvance 9 co dinh, yAdvance 20) - ten file/ki hieu
 *     cu la `gfx4_font_robotomono` (da xoa).
 *   - Lan 1: BlinkMacSystemFont size 17 -> nhan xet "font nay xau qua".
 *   - Lan 2: Segoe UI size 17 (net chu 12 px) -> nhan xet "chu hoi to qua, chinh cung co voi Roboto
 *     Mono 15 duoc khong ?".
 *   - Lan 3 (DANG DUNG): **Segoe UI Regular size 16** (`C:/Windows/Fonts/segoeui.ttf`), font TI LE,
 *     yAdvance **20**, net chu cao **11 px = DUNG bang Roboto Mono 15** truoc do ('A' 10x11, 'X' 9x11,
 *     '0' 8x11) -> bo cuc quay ve dung cac con so da chot (MENU_ROW_H 25, FILES_ROW_H 21, ES_ROW_H 23,
 *     SET_TABLE_H 122, OV_H 118, TP_ROW_H 30).
 * VI SAO DOI DUOC FONT TI LE: driver `src/lv_font_gfxfont.c` dat `dsc->adv_w = g->xAdvance` cho TUNG
 * glyph (xem ghi chu trong driver), nen chi can tool sinh dung xAdvance tung glyph; da them tuy chon
 * `--proportional` cho `tools/ttf2gfxfont.py` (truoc day tool ep 1 buoc chung -> chi dung cho font mono).
 * LUU Y: font ICON thi VAN phai mono (`--advance`) de cac o icon deu nhau.
 *
 * Sinh lai font (font CHU - co --proportional):
 *   python tools/ttf2gfxfont.py --font 'C:/Windows/Fonts/segoeui.ttf' \
 *       --size 16 --bpp 4 --name gfx4_font_text --proportional \
 *       --first 0x20 --last 0xFF \
 *       --extra-ranges 0x0102-0x0103,0x0110-0x0111,0x01A0-0x01B1,0x1EA0-0x1EF9 \
 *       --range-names ascii,vn1,vn2,vn3,vn4 -o src/gfx4_font_text.h
 *
 * So do cua font dang dung (do chinh tool in ra):
 *   size 16 px, xAdvance RIENG tung glyph ('M' 12, 'i' 4, 'W' 15, '.' 4 px),
 *   yAdvance 20 px, base_line 4 px, ascender thuc 16, box_h max 18, box_w max 15,
 *   5 dai / 336 o / 301 glyph co net / 14746 byte bitmap (~17 KB ca bang glyph).
 *   CO DU DAU tieng Viet (tu kiem tra cua tool: OK). 33 codepoint "khong co trong TTF" deu la ky tu
 *   DIEU KHIEN U+007F..U+009F (khong bao gio dung) - xem tools/ttf2gfxfont.py.
 *
 * VI SAO KHONG dung Adafruit GFXfont: header 4bpp nay la DINH DANG RIENG CUA TA, ghi thang theo cach
 * LVGL 8.4 doc bitmap bpp4 (chuoi bit LIEN TUC, col chan -> nibble cao, col le -> nibble thap). Vi vay
 * driver tra con tro THANG vao mang bitmap, KHONG phai repack. Xem comment dau header.
 */
#include "cydlcd_config.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

#include "gfx4_font_text.h"

#else  /* CYDLCD_USE_LVGL */

/* Cac env STM32 khong-LVGL cung compile file nay -> giu 1 khai bao cho translation unit
 * khong rong. */
typedef int gfx4_font_text_not_built_for_this_env;

#endif /* CYDLCD_USE_LVGL */
