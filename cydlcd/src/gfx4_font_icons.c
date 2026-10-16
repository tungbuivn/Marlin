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
 *     U+E004  <-  U+E623  'sd_card'                  the nho (than dac, khac o goc tren trai)
 *     U+E005  <-  U+F057  'sd_card_alert'            the nho + dau '!' = the loi / khong co the
 *     U+E006  <-  U+E1C2  'sd_storage'               *** TRUNG BYTE voi U+E004 *** (xem duoi)
 *     U+E007  <-  U+E88A  'home'                     nha (mai dac + tuong dac + cua) = nut BACK
 *
 *   'sd_card_off' va 'sdcard' KHONG co trong font v1.017 (da tra bang ligature GSUB), nen
 *   U+E005 dung 'sd_card_alert' (co that trong font).
 *   ⚠️ U+E006 'sd_storage' cho ra bitmap GIONG HET BYTE U+E004 'sd_card' (2 thiet ke Material
 *   trung nhau o co 14 px / 4bpp) -> U+E006 chi la BI DANH, khong them hinh moi. Muon bo thi
 *   doi --last 0xE005 va bo --map 0xE006=..., tiet kiem 60 byte bitmap.
 *   ⚠️ U+E007: TEN LIGATURE O BAN FONT NAY NGUOC VOI HINH.  Da render ca 2 o co 14 px:
 *        'home'        (U+E88A) -> nha DAC (mai dac + tuong dac, khoet cua)  <- DA CHON
 *        'home_filled' (U+E9B2) -> nha RONG (long trong, chi vien) - ten "filled" nhung KHONG dac
 *      Tieu chi cua yeu cau la "ban DAC cho de nhin o 14 px" nen chon 'home'. Doi sang ban kia
 *      thi doi --map 0xE007=0xE9B2.  ('cottage' U+E587 cao 13 px -> vuot hop dong 12 px, dung.)

 * SO DO (do chinh tool in ra):
 *   size 14 px, 4bpp, xAdvance 14 px, yAdvance 12 px, base_line 1, --yshift +2
 *   box_h max 12, box_w max 14, ascender 11, descender 1
 *   1 dai, 8 o, 8 glyph co net, 527 byte bitmap (~611 byte ca bang glyph + GFXfont + ranges)
 *   -> CAO <= 14 px va RONG <= 14 px (cho hien tai la cham 12x12 trong header cao 26).
 *
 * yAdvance 11 -> 12 khi them 3 glyph the nho: ca 3 deu cao 12 px va net cham xuong 1 px DUOI
 * baseline (yoff -11, h 12), nen hop dong 11 px se CAT mat 1 px do. Doi yAdvance 12 (base_line
 * 1) giu BASELINE VAN O HANG 11 nhu cu -> 4 glyph WiFi khong doi du lieu VA khong doi vi tri
 * tren man (kiem bang tools/cmp_font_headers.py: 4 dong glyph + 287 byte bitmap dau GIONG HET).
 * Them U+E007 'home' (box 12x10, nam hang 1..10) KHONG lam doi yAdvance/ascender/descender.
 *
 * VI SAO yAdvance KHIT NET (12) chu khong phai 17 nhu font chu?
 *   Driver tu tinh base_line = yAdvance - ascender (KHONG nhan base_line tu header), nen voi
 *   font chi gom icon, net LUON nam sat MEP TREN hop dong. Vi vay chieu cao hop dong chinh la
 *   thu quyet dinh vi tri: dat yAdvance = dung chieu cao net thi hop dong KHIT net, nen khi UI
 *   canh giua label trong header 26 px thi ICON CUNG DUOC CANH GIUA theo.
 *   Neu de yAdvance 17 nhu font chu thi hop dong thua nhieu px o duoi -> canh giua label se keo
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
 *      U+E001..U+E007 = "\xEE\x80\x81" .. "\xEE\x80\x87".
 *      U+E007 = "\xEE\x80\x87"  (nha / nut BACK thay cho nut chu "BACK" o thanh tieu de)
 *
 *   (Cach tuong duong, khong can ham tien loi -- giong het kieu dang dung cho font chu:)
 *        extern const GFXfont *const gfx4_font_icons_ranges[];
 *        const lv_font_t *fi = lv_font_gfxfont_get_ex(gfx4_font_icons_ranges,
 *                                                     GFX4_FONT_ICONS_RANGE_COUNT,
 *                                                     LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4);
 *
 * KIEM TRA OFFLINE (khong can nap - in ASCII art + dem pixel co net cho CA 8 glyph):
 *   python tools/test_gfxfont_layout.py --header src/gfx4_font_icons.h --no-compare \
 *       --cps 0xE000,0xE001,0xE002,0xE003,0xE004,0xE005,0xE006,0xE007
 *   python tools/cmp_font_headers.py <header_cu.h> src/gfx4_font_icons.h 7   # glyph cu co doi?
 *
 * SINH LAI (header nay CHI duoc include o DUNG file .c nay, vi mang bitmap/glyph la `static`
 * con cac doi tuong GFXfont co linkage ngoai):
 *   python tools/ttf2gfxfont.py --font D:/fonts/gfxfont/material/MaterialIcons-Regular.ttf \
 *       --size 14 --bpp 4 --name gfx4_font_icons --first 0xE000 --last 0xE007 \
 *       --range-names pua --advance 14 --yadvance 12 --yshift 2 \
 *       --map 0xE000=0xE63E --map 0xE001=0xE648 --map 0xE002=0xEBE1 --map 0xE003=0xEBE4 \
 *       --map 0xE004=0xE623 --map 0xE005=0xF057 --map 0xE006=0xE1C2 --map 0xE007=0xE88A \
 *       -o src/gfx4_font_icons.h
 */
#include "cydlcd_config.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

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

#else  /* CYDLCD_USE_LVGL */

/* Cac env STM32 khong-LVGL cung compile file nay -> giu 1 khai bao cho translation unit
 * khong rong. */
typedef int gfx4_font_icons_not_built_for_this_env;

#endif /* CYDLCD_USE_LVGL */
