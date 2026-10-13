/**
 * gfx4_font_icons.c -- font ICON 4bpp (16 muc xam) cho header UI (icon WiFi...).
 *
 * NGUON: Material Icons cua Google
 *        https://raw.githubusercontent.com/google/material-design-icons/master/font/MaterialIcons-Regular.ttf
 *        (ban "Version 1.017", unitsPerEm 512, 2229 glyph). Da luu tai
 *        D:\fonts\gfxfont\material\MaterialIcons-Regular.ttf
 *
 * DOI CODEPOINT (bat buoc): trong font nguon, ten icon chi la LIGATURE (khong co glyph ten
 * nghia); vd ligature "wifi" -> glyph uniE63E -> codepoint U+E63E.  Vi vay U+E000 KHONG co
 * glyph trong font nguon, phai dung --map cua tools/ttf2gfxfont.py.  Codepoint DICH nam o
 * vung Private Use rieng cua ta nen doc lap hoan toan voi font nguon:
 *
 *     U+E000  <-  U+E63E  'wifi'                     fan dac + cham  = da/dang ket noi
 *     U+E001  <-  U+E648  'wifi_off'                 fan + gach cheo = WiFi tat / khong co
 *     U+E002  <-  U+EBE1  'network_wifi_three_bar'   fan vien, 3 vach = dang thu / dang ket noi
 *     U+E003  <-  U+EBE4  'network_wifi_one_bar'     fan vien, 1 vach = song yeu
 *
 * (Chi U+E000 va U+E001 la bat buoc theo hop dong; U+E002/U+E003 la them cho tien.)
 *
 * SO DO (do chinh tool in ra):
 *   size 14 px, 4bpp, xAdvance 14 px, yAdvance 11 px, base_line 0, --yshift +2
 *   box_h max 11, box_w max 14, ascender 11, descender 0
 *   1 dai, 4 o, 4 glyph co net, 287 byte bitmap (~330 byte ca bang glyph + GFXfont)
 *   -> CAO <= 14 px va RONG <= 14 px (cho hien tai la cham 12x12 trong header cao 26).
 *
 * VI SAO yAdvance 11 / base_line 0 (khac font chu gfx4_font_robotomono la 17/3)?
 *   Driver tu tinh base_line = yAdvance - ascender (KHONG nhan base_line tu header), nen voi
 *   font chi gom icon, net LUON nam sat MEP TREN hop dong. Vi vay chieu cao hop dong chinh la
 *   thu quyet dinh vi tri: dat yAdvance = dung chieu cao net (11) thi hop dong KHIT net, nen
 *   khi UI canh giua label trong header 26 px thi ICON CUNG DUOC CANH GIUA theo
 *   (net o hang 7..17, trung voi hang chu 9..17 cua font chu).
 *   Neu de yAdvance 17 nhu font chu thi hop dong thua 6 px o duoi -> canh giua label se keo
 *   icon LEN CAO ~4 px so voi chu. (Muon icon nam dung baseline chu thi phai cho driver nhan
 *   base_line tu header - hien driver CHUA ho tro, xem bao cao.)
 *
 * CACH DUNG TRONG LVGL (UI chi lam 2 viec):
 *   1) khai bao 1 lan o dau file:
 *        extern "C" const lv_font_t *gfx4_font_icons_get(void);
 *   2) gan font cho DUNG cai label chua ky tu icon (khong gan ra ca cay):
 *        lv_obj_t *l = lv_label_create(header);
 *        lv_label_set_text(l, "\xEE\x80\x80");            // U+E000 (wifi)  [UTF-8]
 *        lv_obj_set_style_text_font(l, gfx4_font_icons_get(), 0);
 *      U+E001 = "\xEE\x80\x81", U+E002 = "\xEE\x80\x82", U+E003 = "\xEE\x80\x83".
 *
 *   (Cach tuong duong, khong can ham tien loi -- giong het kieu dang dung cho font chu:)
 *        extern const GFXfont *const gfx4_font_icons_ranges[];
 *        const lv_font_t *fi = lv_font_gfxfont_get_ex(gfx4_font_icons_ranges,
 *                                                     GFX4_FONT_ICONS_RANGE_COUNT,
 *                                                     LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4);
 *
 * KIEM TRA OFFLINE (khong can nap - in ASCII art + dem pixel co net):
 *   python tools/test_gfxfont_layout.py --header src/gfx4_font_icons.h --no-compare \
 *       --cps 0xE000,0xE001,0xE002,0xE003
 *
 * SINH LAI (header nay CHI duoc include o DUNG file .c nay, vi mang bitmap/glyph la `static`
 * con cac doi tuong GFXfont co linkage ngoai):
 *   python tools/ttf2gfxfont.py --font D:/fonts/gfxfont/material/MaterialIcons-Regular.ttf \
 *       --size 14 --bpp 4 --name gfx4_font_icons --first 0xE000 --last 0xE003 \
 *       --range-names pua --advance 14 --yadvance 11 --yshift 2 \
 *       --map 0xE000=0xE63E --map 0xE001=0xE648 --map 0xE002=0xEBE1 --map 0xE003=0xEBE4 \
 *       -o src/gfx4_font_icons.h
 */
#include "panel_config.h"

#if defined(PANEL_USE_LVGL) && PANEL_USE_LVGL

#include "lv_font_gfxfont.h"
#include "gfx4_font_icons.h"

/**
 * Tien loi: tra descriptor lv_font_t cua font icon (bpp 4, 1 dai).
 * Ham cua driver da tu nho descriptor (goi lai nhieu lan khong cap phat them), nen goi
 * o dau cung duoc - mien la SAU lv_init() (o day khong cap phat bo nho nen thuc ra khong bat buoc).
 */
const lv_font_t *gfx4_font_icons_get(void)
{
  return lv_font_gfxfont_get_ex(gfx4_font_icons_ranges, GFX4_FONT_ICONS_RANGE_COUNT,
                                LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4);
}

#else  /* PANEL_USE_LVGL */

/* Cac env STM32 khong-LVGL cung compile file nay -> giu 1 khai bao cho translation unit
 * khong rong. */
typedef int gfx4_font_icons_not_built_for_this_env;

#endif /* PANEL_USE_LVGL */
