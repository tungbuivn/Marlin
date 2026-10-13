/**
 * gfx4_font_icons_big.c -- font ICON 4bpp CO LON (~30 px) cho man STATUS.
 *
 * DUNG CHO: man STATUS, o trong that la x 64..95, y 168..197 (rong 32, CAO 30) - nho hon
 * icon speed FR/FL (32x32). Vi vay icon o day duoc chon co sao cho BOX <= 30x30 (cao 32 se
 * de len bitmap FEED y 136..167 hoac thanh tien do y=198).
 * Header (thanh tren) van dung ban NHO 14 px `gfx4_font_icons.h` - KHONG doi.
 *
 * NGUON: Material Icons cua Google (cung file voi ban nho)
 *        D:\fonts\gfxfont\material\MaterialIcons-Regular.ttf  (Version 1.017, unitsPerEm 512)
 *
 * DOI CODEPOINT: y HET ban nho (tra bang LIGATURE GSUB, khong doan) nen nhanh UI dung CUNG
 * chuoi UTF-8 cho ca 2 co:
 *
 *     U+E000  <-  U+E63E  'wifi'                     fan dac + cham  = da/dang ket noi
 *     U+E001  <-  U+E648  'wifi_off'                 fan + gach cheo = WiFi tat / khong co
 *     U+E002  <-  U+EBE1  'network_wifi_three_bar'   fan vien, 3 vach = dang thu / dang ket noi
 *     U+E003  <-  U+EBE4  'network_wifi_one_bar'     fan vien, 1 vach = song yeu
 *
 * SO DO (do chinh tool in ra) - so sanh voi ban nho:
 *                        ban NHO (header)     ban LON (STATUS)
 *     --size             14 px                30 px
 *     xAdvance           14 px                30 px
 *     yAdvance           12 px                25 px
 *     base_line          1                    0
 *     --yshift           +2                   +3
 *     box_h max          12                   23
 *     box_w max          14                   30
 *     |xOffset| max      2                    1
 *     byte bitmap        467                  1232
 *   -> CAO 23 <= 30 va RONG 30 <= 30 => VUA o trong 32x30 cua man STATUS.
 *
 * VI SAO size 30 / yAdvance 25:
 *   - size 31 tro len cho box_w 31..32 -> vuot 30 (do bang Pillow truoc khi sinh).
 *   - Driver tu tinh base_line = yAdvance - ascender (khong nhan tu header) nen voi font chi
 *     co icon, net LUON sat MEP TREN hop dong; dat yAdvance = 25 (dung khoang net chiem:
 *     tu dinh wifi_off den day three_bar/one_bar) thi hop dong KHIT net -> khi UI canh giua
 *     label trong o 30 px thi icon cung duoc canh giua theo. De auto (khong --yshift) thi
 *     yAdvance = 28, thua 3 hang duoi -> icon bi canh giua cao hon ~1,5 px.
 *   - --yshift +3 keo net xuong cho day cua net thap nhat cham day hop dong (descender -1).
 *
 * CACH DUNG TRONG LVGL (giong het ban nho, chi khac ham lay font):
 *   extern "C" const lv_font_t *gfx4_font_icons_big_get(void);
 *   ...
 *   lv_obj_t *l = lv_label_create(lv_scr_act());          // man STATUS / lop phu
 *   lv_label_set_text(l, "\xEE\x80\x80");                 // U+E000 (wifi)  [UTF-8]
 *   lv_obj_set_style_text_font(l, gfx4_font_icons_big_get(), 0);
 *   lv_obj_set_pos(l, 64, 168);                           // o trong 32x30 (x 64..95, y 168..197)
 *   U+E001..U+E003 = "\xEE\x80\x81" .. "\xEE\x80\x83".
 *
 *   (Cach tuong duong khong can ham tien loi:)
 *     extern const GFXfont *const gfx4_font_icons_big_ranges[];
 *     const lv_font_t *fb = lv_font_gfxfont_get_ex(gfx4_font_icons_big_ranges,
 *                                                  GFX4_FONT_ICONS_BIG_RANGE_COUNT,
 *                                                  LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4);
 *
 * KIEM TRA OFFLINE (in ASCII art + dem pixel co net; --max-box 30 vi day la ban lon):
 *   python tools/test_gfxfont_layout.py --header src/gfx4_font_icons_big.h --no-compare \
 *       --max-box 30 --cps 0xE000,0xE001,0xE002,0xE003
 *
 * SINH LAI:
 *   python tools/ttf2gfxfont.py --font D:/fonts/gfxfont/material/MaterialIcons-Regular.ttf \
 *       --size 30 --bpp 4 --name gfx4_font_icons_big --first 0xE000 --last 0xE003 \
 *       --range-names pua --advance 30 --yadvance 25 --yshift 3 \
 *       --map 0xE000=0xE63E --map 0xE001=0xE648 --map 0xE002=0xEBE1 --map 0xE003=0xEBE4 \
 *       -o src/gfx4_font_icons_big.h
 */
#include "CYDLCD_config.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

#include "lv_font_gfxfont.h"
#include "gfx4_font_icons_big.h"

/**
 * Tien loi: tra descriptor lv_font_t cua font icon CO LON (bpp 4, 1 dai).
 * Khai bao o phia UI:  extern "C" const lv_font_t *gfx4_font_icons_big_get(void);
 */
const lv_font_t *gfx4_font_icons_big_get(void)
{
  return lv_font_gfxfont_get_ex(gfx4_font_icons_big_ranges, GFX4_FONT_ICONS_BIG_RANGE_COUNT,
                                LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4);
}

#else  /* CYDLCD_USE_LVGL */

/* Cac env STM32 khong-LVGL cung compile file nay -> giu 1 khai bao cho translation unit
 * khong rong. */
typedef int gfx4_font_icons_big_not_built_for_this_env;

#endif /* CYDLCD_USE_LVGL */
