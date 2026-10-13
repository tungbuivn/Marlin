/**
 * lv_font_gfxfont.h -- driver LVGL 8.4 cho font 1-bit dinh dang Adafruit GFXfont.
 *
 * VI SAO CO FILE NAY
 *   Cac font sinh bang lv_font_conv (nay da XOA khoi repo) deu lam
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
 *        tools/probe_lvgl_font_bitmap_layout.py tren toan bo bang glyph: 67/67 truong hop phan
 *        biet duoc deu la "lien tuc", 0 truong hop "canh hang" (dejavu_16: 253 lien tuc / 0).
 *   => Tra thang &f->bitmap[g->bitmapOffset] cho LVGL se ra RAC (hoac trong) tuy width.
 *      Driver nay xu ly theo CACH XEP BITMAP cua header (tham so `bpp` cua
 *      lv_font_gfxfont_get_ex()):
 *        * bpp 1 (Adafruit GFXfont): REPACK tung glyph sang dang lien tuc, dung 1 buffer nho
 *          dung chung 128 byte (LVGL chi dung con tro do NGAY trong lv_draw_sw_letter).
 *          Header van dung duoc voi Adafruit_GFX / TFT_eSPI.
 *        * bpp 4 (dinh dang RIENG cua ta, co antialias 16 muc xam): bo sinh DA ghi san theo
 *          dung chuoi bit lien tuc cua LVGL, nen driver tra con tro THANG - khong copy, khong
 *          repack, khong ton RAM. Header loai nay KHONG dung duoc voi Adafruit_GFX.
 *
 * ANH XA SANG LVGL 8.4 (kiem chung bang so lieu that, xem lv_font_gfxfont.c)
 *   get_glyph_dsc : idx = letter - f->first; width==0 && height==0 && xAdvance==0 -> khong co
 *                   box_w = width, box_h = height, adv_w = xAdvance (PIXEL NGUYEN - xem ghi
 *                   chu dai trong lv_font_gfxfont.c: KHONG phai xAdvance*16)
 *                   ofs_x = xOffset
 *                   ofs_y = -(yOffset + height)     <-- yOffset cua Adafruit do tu BASELINE
 *                   bpp   = 1 hoac 4 (theo header font)
 *   get_glyph_bitmap : bpp1 -> buffer da repack sang kieu LIEN TUC cua LVGL
 *                      bpp4 -> con tro thang vao mang bitmap trong header
 *   line_height = f->yAdvance ; base_line = yAdvance - ascender_thuc (tinh tu du lieu glyph)
 *
 * DUNG:
 *   // trong 1 file .c duy nhat:
 *   #include "gfx4_font_robotomono.h"
 *   // trong ui_lvgl.cpp:
 *   extern const GFXfont gfx4_font_robotomono_ascii;  // ... vn1..vn4
 *   static const GFXfont *const R[5] = { &..._ascii, &..._vn1, &..._vn2, &..._vn3, &..._vn4 };
 *   const lv_font_t *f = lv_font_gfxfont_get_ex(R, 5, LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4);
 */
#ifndef LV_FONT_GFXFONT_H
#define LV_FONT_GFXFONT_H

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

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
 * Lay descriptor lv_font_t (tinh, dung lai duoc) cho mot tap dai GFXfont, ban 1 bit/pixel
 * (dinh dang Adafruit GFXfont - driver phai repack). Tuong duong:
 *     lv_font_gfxfont_get_ex(ranges, n, LV_FONT_GFXFONT_LAYOUT_ADAFRUIT_BPP1)
 *
 * @param ranges  mang con tro toi cac GFXfont; moi phan tu la MOT dai codepoint lien tuc.
 * @param n       so dai (1..8)
 * @return con tro lv_font_t dung ngay duoc voi lv_obj_set_style_text_font(), hoac NULL neu
 *         tham so sai. Goi lai voi cung tham so tra ve cung descriptor (khong cap phat lai).
 */
const lv_font_t *lv_font_gfxfont_get(const GFXfont *const *ranges, uint8_t n);

/**
 * Nhu tren nhung chon duoc CACH XEP BITMAP cua header font:
 *
 *   bpp = 1  (LV_FONT_GFXFONT_LAYOUT_ADAFRUIT_BPP1)
 *        Header la Adafruit GFXfont THAT: 1 bit/pixel, MOI HANG CANH DAU BYTE.
 *        LVGL 8.4 KHONG doc duoc dang nay (no doi chuoi bit LIEN TUC), nen driver REPACK
 *        tung glyph vao 1 buffer dung chung 128 byte truoc khi tra.
 *        -> dung cho gfx_font_robotomono.h.
 *
 *   bpp = 4  (LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4)
 *        Header la dinh dang RIENG CUA TA (KHONG phai Adafruit): 4 bit/pixel = 16 muc xam,
 *        bitmap ghi san theo dung chuoi bit LIEN TUC ma LVGL doc (col chan -> nibble CAO,
 *        col le -> nibble THAP; so byte = ceil(w*h*4/8) = ceil(w*h/2)). Vi vay driver tra
 *        con tro THANG vao mang bitmap, KHONG copy/repack, KHONG ton RAM.
 *        -> dung cho gfx4_font_robotomono.h.
 */
const lv_font_t *lv_font_gfxfont_get_ex(const GFXfont *const *ranges, uint8_t n, uint8_t bpp);

/** Gia tri `bpp` truyen cho lv_font_gfxfont_get_ex() - phai khop header font tuong ung. */
#define LV_FONT_GFXFONT_LAYOUT_ADAFRUIT_BPP1  1U
#define LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4      4U

/**
 * Tu kiem tra: in ra COM7 (115200) ket qua tra glyph cho 'A' '0' 'a'
 * U+1EC7 ('e' nang + dau mu) va U+0110 ('D' gach ngang) + checksum 8 byte dau cua
 * bitmap (bpp1: da repack; bpp4: con tro thang vao mang).
 * Muc dich: nap xong chi can doc log la biet driver tra dung du lieu hay khong.
 */
void lv_font_gfxfont_selftest(const lv_font_t *f);

#ifdef __cplusplus
}
#endif

#endif /* CYDLCD_USE_LVGL */
#endif /* LV_FONT_GFXFONT_H */
