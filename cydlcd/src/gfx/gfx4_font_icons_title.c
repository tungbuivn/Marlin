/**
 * gfx4_font_icons_title.c -- font ICON 20 px RIENG cho THANH TIEU DE (2026-10-16).
 *
 * VI SAO CO BAN RIENG:
 *   Truoc day icon thanh tieu de (HOME, WiFi, ket noi) dung chung font 14 px
 *   (gfx4_font_icons.c) -> glyph chi 10..13 px. Nguoi dung bao "icon tren thanh title co ve hoi
 *   be". Thanh tieu de cao 26 px, nen:
 *       14 px -> glyph 10..13 px  : qua nho
 *       30 px -> glyph 20..23 px  : qua cao (ban TO dang dung cho icon WiFi man STATUS)
 *       20 px -> glyph 14..18 px  : vua  <-- ban nay
 *
 *   ⚠️ TACH RIENG chu KHONG nang co ban 14 px: ban 14 px con dung o man STATUS (mui ten, the
 *   nho) va o cac nut dieu huong man BED MESH — nang co no len se doi bo cuc nhung cho do.
 *   Cung KHONG dung ban 30 px vi no chi co 4 glyph WiFi (U+E000..U+E003).
 *
 * NGUON + CACH SINH (cung TTF, cung kieu --map nhu gfx4_font_icons.c — doc file do de biet URL
 * tai font nguon; file .ttf KHONG nam trong repo):
 *
 *   python tools/ttf2gfxfont.py --font tools/MaterialIcons-Regular.ttf \
 *       --size 20 --bpp 4 --name gfx4_font_icons_title --first 0xE000 --last 0xE002 \
 *       --extra-ranges 0xE007-0xE007,0xE00C-0xE00D --range-names wifi,home,link \
 *       --advance 20 \
 *       --map 0xE000=0xE63E --map 0xE001=0xE648 --map 0xE002=0xEBE1 --map 0xE007=0xE88A \
 *       --map 0xE00C=0xE1E0 --map 0xE00D=0xE4FA \
 *       -o src/gfx4_font_icons_title.h
 *
 *   yAdvance/base_line de TU DONG (=19/0) chu KHONG ep: glyph cao nhat 18 px nam gon trong hop
 *   19 px (hang 1..18) nen khi UI canh giua label trong thanh 26 px thi icon cung duoc canh giua.
 *
 * GLYPH CO TRONG BAN NAY (chi nhung thu thanh tieu de dung — KHONG phai ca bang cua ban 14 px):
 *     U+E000 <- U+E63E  'wifi'                     icon WiFi (da/dang ket noi)
 *     U+E001 <- U+E648  'wifi_off'                 icon WiFi tat (co gach cheo)
 *     U+E002 <- U+EBE1  'network_wifi_three_bar'   WiFi dang thu
 *     U+E007 <- U+E88A  'home'                     nut HOME trai cung
 *     U+E00C <- U+E1E0  'usb'                      icon ket noi — DA ket noi
 *     U+E00D <- U+E4FA  'usb_off'                  icon ket noi — MAT ket noi (co gach cheo)
 *
 * DOI SANG BAN 14 px hay 30 px: doi ham tra font o ui_chrome.cpp (wifi_icon_font()).
 */
#include "cydlcd_config.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

#include "lv_font_gfxfont.h"
#include "gfx4_font_icons_title.h"

/** Tien loi: tra descriptor lv_font_t cua font icon thanh tieu de (bpp 4, 3 dai). */
const lv_font_t *gfx4_font_icons_title_get(void)
{
  return lv_font_gfxfont_get_ex(gfx4_font_icons_title_ranges, GFX4_FONT_ICONS_TITLE_RANGE_COUNT,
                                LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4);
}

#else  /* CYDLCD_USE_LVGL */

/* Cac env STM32 khong-LVGL cung compile file nay -> giu 1 khai bao cho translation unit
 * khong rong. */
typedef int gfx4_font_icons_title_not_built_for_this_env;

#endif /* CYDLCD_USE_LVGL */
