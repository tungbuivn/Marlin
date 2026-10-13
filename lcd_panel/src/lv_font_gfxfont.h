/**
 * lv_font_gfxfont.h -- driver LVGL 8.4 cho font 1-bit dinh dang Adafruit GFXfont.
 *
 * VI SAO CO FILE NAY
 *   Cac font sinh bang lv_font_conv (font_consola_vn_14.c, font_robotomono_vn_14.c) deu lam
 *   MAN HINH THAT mat het chu, du file hop le. Thay vi di tim nguyen nhan trong dinh dang
 *   nang do, ta tu sinh font 1-bit (nho, kiem soat hoan toan du lieu) va tu viet driver.
 *
 * VI SAO PHAI REPACK (doc ky truoc khi sua)
 *   Adafruit GFXfont va LVGL deu la 1 bit/pixel nhung KHAC NHAU ve cach xep hang:
 *     - Adafruit GFXfont: MOI HANG CANH DAU BYTE  (stride = ceil(width/8))
 *     - LVGL 8.4        : cac hang la MOT CHUOI BIT LIEN TUC, khong dem padding theo hang
 *   Chung minh (khong phai suy doan):
 *     1) lv_draw_sw_letter.c, trong vong lap theo hang:
 *            col_bit += col_bit_row_ofs;         // col_bit_row_ofs = (box_w + col_start - col_end) * bpp
 *            map_p   += (col_bit >> 3);
 *        Voi hang day du (col_start = 0, col_end = box_w) thi col_bit_row_ofs == 0, nghia la
 *        con tro bit chay TIEP vao hang sau chu khong quay ve dau byte.
 *     2) So lieu that cua chinh font LVGL: lv_font_montserrat_14.c, glyph '!' box 3x10 bpp 4
 *        chiem DUNG 15 byte = ceil(3*10*4/8); neu canh hang thi phai 20 byte. Kiem tra bang
 *        tools/_probe_lvgl_bitmap_layout.py tren toan bo bang glyph: 67/67 truong hop phan biet
 *        duoc deu la "lien tuc", 0 truong hop "canh hang" (dejavu_16: 253 lien tuc / 0 canh hang).
 *   => Tra thang &f->bitmap[g->bitmapOffset] cho LVGL se ra RAC (hoac trong) tuy width.
 *      Driver nay repack tung glyph sang dang lien tuc truoc khi tra, dung mot buffer nho
 *      dung chung (LVGL chi dung con tro do NGAY trong lv_draw_sw_letter, khong giu lai).
 *      Header font van giu dung dinh dang GFX de con dung duoc voi Adafruit_GFX / TFT_eSPI.
 *
 * ANH XA SANG LVGL 8.4 (kiem chung bang so lieu that, xem lv_font_gfxfont.c)
 *   get_glyph_dsc : idx = letter - f->first; width==0 && height==0 && xAdvance==0 -> khong co
 *                   box_w = width, box_h = height, adv_w = xAdvance*16 (LVGL dung 1/16 px)
 *                   ofs_x = xOffset
 *                   ofs_y = -(yOffset + height)     <-- yOffset cua Adafruit do tu BASELINE
 *   get_glyph_bitmap : tra buffer da repack sang kieu LIEN TUC cua LVGL (khong the tra thang)
 *   line_height = f->yAdvance ; base_line = yAdvance - ascender_thuc (tinh tu du lieu glyph)
 *
 * DUNG:
 *   // trong 1 file .c duy nhat:
 *   #include "gfx_font_robotomono.h"
 *   // trong ui_lvgl.cpp:
 *   extern const GFXfont *const gfx_font_robotomono_ranges[];
 *   const lv_font_t *f = lv_font_gfxfont_get(gfx_font_robotomono_ranges, 5);
 */
#ifndef LV_FONT_GFXFONT_H
#define LV_FONT_GFXFONT_H

#if defined(PANEL_USE_LVGL) && PANEL_USE_LVGL

#include <stdint.h>
#include <stdbool.h>

/* Header font sinh ra co the duoc include o noi khong co Arduino.h -> tu dinh nghia PROGMEM
 * (tren ESP32 PROGMEM rong; tren STM32 la macro cua framework Arduino). */
#ifndef PROGMEM
  #define PROGMEM
#endif

/* Kieu cua Adafruit GFX. THU TU + KIEU TRUONG PHAI DUNG nhu duoi day (khop voi
 * consola9pt7b.h that). Guard de khong dung do neu Adafruit_GFX da duoc include. */
#ifndef LVGL_GFXFONT_STRUCTS_DEFINED
#define LVGL_GFXFONT_STRUCTS_DEFINED
typedef struct {
  uint16_t bitmapOffset;      /* chi so byte vao mang bitmap cua dai */
  uint8_t  width, height;     /* kich thuoc bitmap, pixel */
  uint8_t  xAdvance;          /* buoc nhay ngang, pixel */
  int8_t   xOffset, yOffset;  /* yOffset: tu BASELINE len mep tren bitmap (am = tren baseline) */
} GFXglyph;

typedef struct {
  uint8_t  *bitmap;           /* cac glyph, 1 bit/pixel, MOI HANG CANH DAU BYTE */
  GFXglyph *glyph;
  uint16_t first, last;       /* dai codepoint lien tuc ma bang glyph[] bao phu */
  uint8_t  yAdvance;          /* chieu cao dong */
} GFXfont;
#endif /* LVGL_GFXFONT_STRUCTS_DEFINED */

#include <lvgl.h>

/* ---------------------------------------------------------------------------
 *  API
 *
 *  Ham duoc dinh nghia trong lv_font_gfxfont.c (bien dich bang C), nen khai bao o day
 *  phai co linkage "C" de ui_lvgl.cpp (C++) link duoc (khong bi doi ten - name mangling).
 * -------------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Lay descriptor lv_font_t (tinh, dung lai duoc) cho mot tap dai GFXfont.
 *
 * @param ranges  mang con tro toi cac GFXfont; moi phan tu la MOT dai codepoint lien tuc.
 *                Cac dai phai cung font/cung co (cung yAdvance) - thuong lay tu 1 header
 *                do tools/ttf2gfxfont.py sinh.
 * @param n       so dai (1..8)
 * @return con tro lv_font_t dung ngay duoc voi lv_obj_set_style_text_font(), hoac NULL neu
 *         tham so sai. Goi lai voi cung tham so tra ve cung descriptor (khong cap phat lai).
 */
const lv_font_t *lv_font_gfxfont_get(const GFXfont *const *ranges, uint8_t n);

/**
 * Tu kiem tra: in ra COM7 (115200) ket qua tra glyph cho 'A' '0' 'a'
 * U+1EC7 ('e' nang + dau mu) va U+0110 ('D' gach ngang) + checksum 8 byte dau cua
 * bitmap da repack.
 * Muc dich: nap xong chi can doc log la biet driver tra dung du lieu hay khong.
 */
void lv_font_gfxfont_selftest(const lv_font_t *f);

#ifdef __cplusplus
}
#endif

#endif /* PANEL_USE_LVGL */
#endif /* LV_FONT_GFXFONT_H */
