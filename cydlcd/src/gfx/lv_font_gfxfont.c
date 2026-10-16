/**
 * lv_font_gfxfont.c -- driver LVGL 8.4 doc font 1-bit Adafruit GFXfont.
 *
 * Xem lv_font_gfxfont.h de biet VI SAO phai repack bitmap (LVGL 8.4 xep cac hang thanh mot
 * chuoi bit LIEN TUC, Adafruit GFXfont lai canh moi hang dau byte).
 *
 * File nay chi co nghia khi CYDLCD_USE_LVGL = 1 (cac env STM32 nho van compile no -> thanh
 * translation unit rong, xem khoi #else o cuoi).
 */
#include "lv_font_gfxfont.h"

#if defined(CYDLCD_USE_LVGL) && CYDLCD_USE_LVGL

#include <string.h>         /* memset */

/* ---------------------------------------------------------------------------
 *  Log ra COM7
 *
 *  File nay la .c nen duoc bien dich bang C, ma `Serial` cua Arduino-ESP32 la doi tuong
 *  C++ (Arduino.h chi include HardwareSerial.h trong #ifdef __cplusplus) -> C KHONG goi
 *  duoc Serial.printf.
 *
 *  => dung printf() cua <stdio.h>: tren ESP32 (Arduino) stdout di thang ra UART0, cung
 *     chan/baud voi `Serial` ma main.cpp da Serial.begin(115200) -> van la COM7.
 *  => neu sau nay doi ten file thanh lv_font_gfxfont.cpp thi tu dong dung Serial.printf
 *     (giong main.cpp), khong phai sua gi.
 * -------------------------------------------------------------------------*/
#if defined(__cplusplus)
  #include <Arduino.h>
  #define GFX_LOG(...)   do { Serial.printf(__VA_ARGS__); } while (0)
#else
  #include <stdio.h>
  #define GFX_LOG(...)   do { printf(__VA_ARGS__); fflush(stdout); } while (0)
#endif

/* ---------------------------------------------------------------------------
 *  Cau hinh
 * -------------------------------------------------------------------------*/

/* So dai toi da trong 1 font (font tieng Viet cua ta dung 5: ascii + 4 dai dau). */
#define LV_FONT_GFXFONT_MAX_RANGES   8U

/* So descriptor font dung dong thoi. Cac descriptor nay chi duoc cap khi co nguoi goi
 * lv_font_gfxfont_get[_ex]() voi tap `ranges` khac nhau; HET SLOT thi driver dung lai slot 0 ->
 * descriptor cu bi ghi de (chu ve bang font khac = hong man hinh), nen so nay PHAI >= so font
 * gfxfont dung dong thoi. Moi slot ton 52 byte DRAM tinh (do bang nm tren lv_font_gfxfont.c.o:
 * s_slots = 104 byte cho 2 slot).
 *
 * ⚠️ DANH SACH THAT (kiem 2026-10-16) — 5 font, KHONG phai 3 nhu comment cu:
 *     1. gfx4_font_text        chu chinh (Segoe UI 16, bpp4)          - moi trang
 *     2. gfx_font_robotomono   chu mono (Roboto Mono 13, bpp1)       - nhan trong nut
 *     3. gfx4_font_icons       icon 14 px                            - header
 *     4. gfx4_font_icons_title icon 20 px (them 2026-10-16)          - header
 *     5. gfx4_font_icons_big   icon 30 px                            - chi man STATUS
 *   (lv_font_montserrat_*) la font GOC cua LVGL, KHONG chiem slot.
 *
 * LOI DA XAY RA THAT: de 3 slot trong khi UI dung 5 -> font thu 4 tro di GHI DE slot 0 (descriptor
 * cua font CHU chinh) -> chu tren RAT NHIEU TRANG ve bang font icon/mono (nguoi dung bao "hong het
 * font chu"). Truoc do chi khi vao man STATUS moi tran (font icon 30 px), con tu 2026-10-16 font
 * icon 20 px duoc cap ngay luc dung thanh tieu de -> hong ngay tu trang dau.
 * => Khi THEM font gfxfont moi, PHAI tang so nay. */
#define LV_FONT_GFXFONT_MAX_FONTS    6U

/* Buffer repack cho font bpp 1 (bitmap Adafruit canh hang). CHI font bpp 1 moi can, va hien tai
 * KHONG co font bpp 1 nao duoc dang ky (UI_MONO_GFX_BPP = 4 -> gfx4_font_text; gfx_font_robotomono
 * chi nam trong nhanh BPP=1 khong bat). Glyph lon nhat cua font 13px la 9x14 -> 16 byte.
 * 2026-10-16: ha 128 -> 32 de lay 96 byte DRAM cho bang slot font (dang thieu cho). Neu sau nay
 * bat lai font bpp 1 ma glyph to hon 32 byte thi driver TU cap phat tu heap (xem nhanh
 * `s_scratch_heap` ben duoi) — khong hong gi, chi khong tiet kiem duoc RAM nua. */
#define LV_FONT_GFXFONT_SCRATCH      32U

/* Cach xep bitmap trong header font:
 *   1 = Adafruit GFXfont: 1 bit/pixel, MOI HANG CANH DAU BYTE  -> driver phai REPACK
 *   4 = dinh dang rieng cua ta: 4 bit/pixel, chuoi bit LIEN TUC dung kieu LVGL 8.4 doc
 *       (col chan -> nibble cao, col le -> nibble thap)      -> driver tra con tro THANG
 * (Da chung minh LVGL 8.4 luon doc dang lien tuc: xem tools/probe_lvgl_font_bitmap_layout.py) */
#define LV_FONT_GFXFONT_LAYOUT_ADAFRUIT_BPP1  1U
#define LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4      4U

/* ---------------------------------------------------------------------------
 *  Kieu noi bo
 * -------------------------------------------------------------------------*/
typedef struct {
  const GFXfont *const *ranges;
  uint8_t               n;
  uint8_t               bpp;             /* 1 hoac 4 - quyet dinh co repack hay khong */
  const GFXfont        *primary;         /* = ranges[0]: nguon yAdvance */
  uint8_t              *scratch;         /* buffer repack (chi font bpp 1 moi can) */
  uint16_t              scratch_cap;
  uint16_t              max_glyph_bytes; /* glyph to nhat trong CA Cac dai (chi de bao cao) */
  uint8_t               in_use;
} gfx_ctx_t;

typedef struct {
  lv_font_t  font;
  gfx_ctx_t  ctx;
} gfx_slot_t;

static gfx_slot_t s_slots[LV_FONT_GFXFONT_MAX_FONTS];

/* Buffer repack dung chung cho MOI font bpp 1 (tiet kiem RAM: truoc day moi slot 128 byte). */
static uint8_t s_scratch[LV_FONT_GFXFONT_SCRATCH];
static uint8_t *s_scratch_heap = NULL;      /* chi dung khi glyph to hon s_scratch */
static uint16_t s_scratch_cap  = LV_FONT_GFXFONT_SCRATCH;

/* ---------------------------------------------------------------------------
 *  Helper
 * -------------------------------------------------------------------------*/

/* Tim glyph theo codepoint trong tat ca cac dai. Tra NULL neu khong dai nao chua. */
static const GFXglyph *gfx_lookup(const gfx_ctx_t *ctx, uint32_t letter, const GFXfont **out_font)
{
  if (letter > 0xFFFFU) return NULL;                 /* GFXfont.first/last la uint16_t */

  for (uint8_t i = 0; i < ctx->n; i++) {
    const GFXfont *f = ctx->ranges[i];
    if (!f || !f->glyph) continue;
    if (letter < (uint32_t)f->first || letter > (uint32_t)f->last) continue;

    const GFXglyph *g = &f->glyph[letter - (uint32_t)f->first];
    /* O TRONG: tools/ttf2gfxfont.py de xAdvance = 0 cho codepoint khong co trong TTF,
     * con glyph CO that nhung khong co net (space, NBSP) thi giu xAdvance = buoc mono.
     * Nho vay phan biet duoc "khong co glyph" voi "glyph rong". */
    if (g->width == 0 && g->height == 0 && g->xAdvance == 0) continue;

    if (out_font) *out_font = f;
    return g;
  }
  return NULL;
}

/* So byte bitmap cua 1 glyph khi da o dung kieu LVGL (chuoi bit lien tuc):
 *   bpp 1 -> ceil(w*h/8)     bpp 4 -> ceil(w*h*4/8) = ceil(w*h/2) */
static uint16_t gfx_bitmap_bytes(uint8_t w, uint8_t h, uint8_t bpp)
{
  return (uint16_t)(((uint32_t)w * (uint32_t)h * (uint32_t)bpp + 7U) >> 3);
}

/**
 * Repack bitmap 1-bit tu kieu Adafruit (moi hang canh dau byte) sang kieu LVGL
 * (chuoi bit lien tuc, khong padding theo hang).
 *
 *   nguon: src + row * ceil(w/8)          bit cao ve truoc
 *   dich : bit thu (row*w + col), bit cao ve truoc
 *
 * Voi font 13px, 1 glyph ~9x14 = 17 byte -> vong lap nay khong dang ke so voi phan blend
 * ma LVGL con phai lam cho chinh glyph do.
 */
static void gfx_repack(const uint8_t *src, uint32_t w, uint32_t h, uint8_t *dst)
{
  const uint32_t stride = (w + 7U) >> 3;
  memset(dst, 0, (size_t)((w * h + 7U) >> 3));

  uint32_t bit = 0;
  for (uint32_t row = 0; row < h; row++) {
    const uint8_t *s = src + (size_t)row * stride;
    uint32_t col = 0;
    for (uint32_t bi = 0; col < w; bi++) {
      uint32_t n = w - col;
      if (n > 8U) n = 8U;
      const uint8_t v = s[bi];
      if (v == 0) {                       /* byte trong: nhay ca 8 cot mot lan */
        bit += n;
        col += n;
      }
      else {
        for (uint32_t k = 0; k < n; k++, bit++, col++) {
          if (v & (uint8_t)(0x80U >> k))
            dst[bit >> 3] |= (uint8_t)(0x80U >> (bit & 7U));
        }
      }
    }
  }
}

/* ---------------------------------------------------------------------------
 *  Callback LVGL
 * -------------------------------------------------------------------------*/

static bool gfx_get_glyph_dsc(const lv_font_t *font, lv_font_glyph_dsc_t *dsc,
                              uint32_t letter, uint32_t letter_next)
{
  (void)letter_next;                       /* font mono: khong kern */
  const gfx_ctx_t *ctx = (const gfx_ctx_t *)font->dsc;
  if (!ctx) return false;

  const GFXglyph *g = gfx_lookup(ctx, letter, NULL);
  if (!g) return false;

  dsc->box_w = g->width;
  dsc->box_h = g->height;
  /* adv_w: LVGL dung don vi PIXEL NGUYEN cho truong nay, KHONG phai 1/16 px.
   * Bang chung (3 cho doc lap trong chinh LVGL 8.4):
   *   - lv_font_fmt_txt.c:177  adv_w = (adv_w + (1 << 3)) >> 4;   <- gia tri *16 trong bang
   *     font duoc DOI VE PIXEL NGUYEN truoc khi ghi vao dsc_out->adv_w
   *   - lv_freetype.c:224      dsc_out->adv_w = horiAdvance >> 6; <- engine font viet tay
   *     (giong ta) ghi thang pixel nguyen
   *   - lv_imgfont.c:116       dsc_out->adv_w = header.w;
   * Va lv_draw_label.c:305 dung `pos.x += letter_w` voi letter_w = adv_w (toa do pixel).
   * => Neu dat xAdvance * 16 thi moi ky tu nhay 8*16 = 128 px -> glyph bi ve ra NGOAI vung
   *    clip cua nhan => MAN HINH TRONG TRON (dung trieu chung "mat het chu" da gap).
   *    (Ghi chu: `adv_w` *16 la dinh dang LUU trong bang cua lv_font_conv, khong phai gia
   *     tri ma mot get_glyph_dsc viet tay phai tra ve.) */
  dsc->adv_w = g->xAdvance;
  dsc->ofs_x = g->xOffset;
  /* Adafruit: bitmap ve tu (baseline + yOffset), yOffset do TU BASELINE.
   * LVGL    : gpos.y = pos.y + (line_height - base_line) - box_h - ofs_y
   *           (xem lv_draw_sw_letter.c va lv_draw_label.c: duong gach chan nam o
   *            pos.y + line_height - base_line, tuc baseline).
   * => can (line_height - base_line) + yOffset = (line_height - base_line) - box_h - ofs_y
   * => ofs_y = -(yOffset + box_h), KHONG phu thuoc line_height/base_line.
   * Kiem chung: 'g' cua font nay co yOffset -7, box_h 10 -> ofs_y = -3 (chan xuong duoi
   * baseline 3 px, dung); 'A' yOffset -9, box_h 9 -> ofs_y = 0 (dung tren baseline). */
  dsc->ofs_y = (int16_t)(-((int16_t)g->yOffset + (int16_t)g->height));
  dsc->bpp = ctx->bpp;                     /* 1 = Adafruit (da repack), 4 = 16 muc xam */
  dsc->is_placeholder = 0;
  return true;
}

static const uint8_t *gfx_get_glyph_bitmap(const lv_font_t *font, uint32_t letter)
{
  gfx_ctx_t *ctx = (gfx_ctx_t *)font->dsc;
  if (!ctx) return NULL;

  const GFXfont *f = NULL;
  const GFXglyph *g = gfx_lookup(ctx, letter, &f);
  if (!g || g->width == 0 || g->height == 0) return NULL;    /* glyph rong: khong ve gi */
  if (!f->bitmap) return NULL;

  if (ctx->bpp == LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4) {
    /* Header 4bpp da ghi san theo dung chuoi bit lien tuc ma LVGL doc -> tra THANG con tro
     * (khong copy, khong repack). Bitmap nam trong .rodata/flash nen khong ton RAM. */
    return &f->bitmap[g->bitmapOffset];
  }

  /* bpp 1: bitmap Adafruit canh hang -> phai repack sang dang lien tuc cua LVGL. */
  if (!ctx->scratch) return NULL;
  const uint16_t need = gfx_bitmap_bytes(g->width, g->height, 1U);
  if (need > ctx->scratch_cap) return NULL;                   /* khong du buffer */

  gfx_repack(&f->bitmap[g->bitmapOffset], g->width, g->height, ctx->scratch);
  return ctx->scratch;
}

/* ---------------------------------------------------------------------------
 *  Khoi tao descriptor
 * -------------------------------------------------------------------------*/

/* Do so byte bitmap cua glyph to nhat (de cap phat buffer repack; chi font bpp 1 can). */
static uint16_t gfx_measure_max_glyph(const gfx_ctx_t *ctx)
{
  uint16_t maxb = 0;
  for (uint8_t i = 0; i < ctx->n; i++) {
    const GFXfont *f = ctx->ranges[i];
    if (!f || !f->glyph) continue;
    for (uint32_t cp = f->first; cp <= (uint32_t)f->last; cp++) {
      const GFXglyph *g = &f->glyph[cp - f->first];
      if (g->width == 0 || g->height == 0) continue;
      const uint16_t b = gfx_bitmap_bytes(g->width, g->height, ctx->bpp);
      if (b > maxb) maxb = b;
    }
  }
  return maxb;
}

/**
 * Ascender thuc te = so hang cao nhat ma mot glyph CO NET can o TREN baseline.
 * Lay -yOffset lon nhat (yOffset am = tren baseline). Phai quet CA Cac dai vi cac glyph
 * tieng Viet (vd U+1EBE 'E' + mu + sac) cao hon ky tu ASCII.
 */
static uint8_t gfx_measure_ascender(const gfx_ctx_t *ctx)
{
  int16_t top = 0;
  for (uint8_t i = 0; i < ctx->n; i++) {
    const GFXfont *f = ctx->ranges[i];
    if (!f || !f->glyph) continue;
    for (uint32_t cp = f->first; cp <= (uint32_t)f->last; cp++) {
      const GFXglyph *g = &f->glyph[cp - f->first];
      if (g->width == 0 || g->height == 0) continue;   /* bo qua o trong + glyph rong */
      if ((int16_t)(-g->yOffset) > top) top = (int16_t)(-g->yOffset);
    }
  }
  if (top < 0) top = 0;
  if (top > 127) top = 127;
  return (uint8_t)top;
}

const lv_font_t *lv_font_gfxfont_get_ex(const GFXfont *const *ranges, uint8_t n, uint8_t bpp)
{
  if (!ranges || n == 0 || n > LV_FONT_GFXFONT_MAX_RANGES) return NULL;
  if (!ranges[0] || !ranges[0]->glyph) return NULL;
  if (bpp != LV_FONT_GFXFONT_LAYOUT_ADAFRUIT_BPP1 && bpp != LV_FONT_GFXFONT_LAYOUT_LVGL_BPP4)
    return NULL;

  /* Da tao roi (cung tham so) -> tra lai dung descriptor cu. */
  for (uint8_t i = 0; i < LV_FONT_GFXFONT_MAX_FONTS; i++) {
    if (s_slots[i].ctx.in_use && s_slots[i].ctx.ranges == ranges &&
        s_slots[i].ctx.n == n && s_slots[i].ctx.bpp == bpp)
      return &s_slots[i].font;
  }

  /* Lay slot trong; het thi dung lai slot 0. */
  uint8_t si = LV_FONT_GFXFONT_MAX_FONTS;
  for (uint8_t i = 0; i < LV_FONT_GFXFONT_MAX_FONTS; i++) {
    if (!s_slots[i].ctx.in_use) { si = i; break; }
  }
  if (si == LV_FONT_GFXFONT_MAX_FONTS) si = 0;

  gfx_slot_t *s = &s_slots[si];

  memset(&s->ctx, 0, sizeof(s->ctx));
  s->ctx.in_use  = 1;
  s->ctx.ranges  = ranges;
  s->ctx.n       = n;
  s->ctx.bpp     = bpp;
  s->ctx.primary = ranges[0];

  s->ctx.max_glyph_bytes = gfx_measure_max_glyph(&s->ctx);
  if (bpp == LV_FONT_GFXFONT_LAYOUT_ADAFRUIT_BPP1) {
    /* Chi font bpp 1 (bitmap canh hang) moi phai repack -> moi can buffer. */
    if (s->ctx.max_glyph_bytes <= LV_FONT_GFXFONT_SCRATCH) {
      s->ctx.scratch     = s_scratch;
      s->ctx.scratch_cap = LV_FONT_GFXFONT_SCRATCH;
    }
    else if (s_scratch_heap == NULL || s_scratch_cap < s->ctx.max_glyph_bytes) {
      if (s_scratch_heap) { lv_mem_free(s_scratch_heap); s_scratch_heap = NULL; }
      s_scratch_heap = (uint8_t *)lv_mem_alloc(s->ctx.max_glyph_bytes);
      s_scratch_cap  = s_scratch_heap ? s->ctx.max_glyph_bytes : 0;
      s->ctx.scratch     = s_scratch_heap;
      s->ctx.scratch_cap = s_scratch_cap;
    }
    else {
      s->ctx.scratch     = s_scratch_heap;
      s->ctx.scratch_cap = s_scratch_cap;
    }
  }
  else {
    s->ctx.scratch     = NULL;      /* bpp 4: tra con tro thang, khong ton RAM */
    s->ctx.scratch_cap = 0;
  }

  /* line_height = yAdvance (moi dai cung font/cung co nen lay tu dai dau).
   * base_line    = yAdvance - ascender_thuc. LVGL dat baseline o
   *                (line_height - base_line) tu mep tren dong, nen baseline nam dung
   *                duoi phan net cao nhat va moi glyph ve duoc trong o dong.
   * Font nay: yAdvance 17, ascender 14 -> base_line 3 (trung voi descender thuc te
   * = 3, vi 17 - 14 = 3; giong montserrat_14 co line_height 16 / base_line 3). */
  const uint8_t yadv = s->ctx.primary->yAdvance;
  const uint8_t asc  = gfx_measure_ascender(&s->ctx);
  int16_t bl = (int16_t)yadv - (int16_t)asc;
  if (bl < 0) bl = 0;
  if (bl > (int16_t)yadv) bl = (int16_t)yadv;

  memset(&s->font, 0, sizeof(s->font));
  s->font.get_glyph_dsc    = gfx_get_glyph_dsc;
  s->font.get_glyph_bitmap = gfx_get_glyph_bitmap;
  s->font.line_height      = yadv;
  s->font.base_line        = bl;
  s->font.subpx            = LV_FONT_SUBPX_NONE;
  s->font.underline_position  = -1;
  s->font.underline_thickness = 1;
  s->font.dsc              = &s->ctx;
  s->font.fallback         = NULL;

  return &s->font;
}

/* Tuong thich: ban cu chi ho tro font 1 bit/pixel dinh dang Adafruit GFXfont. */
const lv_font_t *lv_font_gfxfont_get(const GFXfont *const *ranges, uint8_t n)
{
  return lv_font_gfxfont_get_ex(ranges, n, LV_FONT_GFXFONT_LAYOUT_ADAFRUIT_BPP1);
}

/* ---------------------------------------------------------------------------
 *  Tu kiem tra (in ra Serial / COM7)
 * -------------------------------------------------------------------------*/

static uint32_t gfx_checksum(const uint8_t *p, uint16_t nbytes)
{
  /* Tong co trong so: sum (i+1) * byte[i] - doi 1 byte cung doi ket qua. */
  uint32_t sum = 0;
  for (uint16_t i = 0; i < nbytes; i++) sum += (uint32_t)(i + 1U) * (uint32_t)p[i];
  return sum;
}

void lv_font_gfxfont_selftest(const lv_font_t *f)
{
  if (!f) { GFX_LOG("[GFXFONT] selftest: font NULL\n"); return; }

  const gfx_ctx_t *ctx = (const gfx_ctx_t *)f->dsc;
  const uint8_t bpp = ctx ? ctx->bpp : 1U;
  GFX_LOG("[GFXFONT] selftest --------------------------------------------------\n");
  GFX_LOG("[GFXFONT] line_height=%d base_line=%d (ascender thuc=%d) | bpp=%u (%s)\n",
                (int)f->line_height, (int)f->base_line,
                (int)f->line_height - (int)f->base_line, (unsigned)bpp,
                bpp == 4U ? "16 muc xam, bitmap da kieu LVGL -> tra con tro THANG"
                          : "1 bit, bitmap Adafruit canh hang -> REPACK");
  if (ctx) {
    GFX_LOG("[GFXFONT] so dai=%u  glyph to nhat=%u byte  buffer repack=%u byte\n",
                  (unsigned)ctx->n, (unsigned)ctx->max_glyph_bytes, (unsigned)ctx->scratch_cap);
    for (uint8_t i = 0; i < ctx->n; i++) {
      const GFXfont *r = ctx->ranges[i];
      if (!r) continue;
      GFX_LOG("[GFXFONT]   dai %u: U+%04X..U+%04X  yAdvance=%u\n",
                    (unsigned)i, (unsigned)r->first, (unsigned)r->last, (unsigned)r->yAdvance);
    }
  }

  /* 'A' '0' 'a' U+1EC7 ('e' nang) U+0110 ('D' gach) */
  static const uint32_t probe[5] = { 0x41U, 0x30U, 0x61U, 0x1EC7U, 0x0110U };
  static const char *const pname[5] = { "'A'", "'0'", "'a'", "U+1EC7", "U+0110" };

  for (uint8_t i = 0; i < 5; i++) {
    lv_font_glyph_dsc_t d;
    memset(&d, 0, sizeof(d));
    const bool ok = lv_font_get_glyph_dsc(f, &d, probe[i], 0);
    if (!ok) {
      GFX_LOG("[GFXFONT] %-7s -> KHONG TRA DUOC GLYPH (driver se ve o vuong)\n", pname[i]);
      continue;
    }
    const uint8_t *bmp = lv_font_get_glyph_bitmap(f, probe[i]);
    const uint16_t need = gfx_bitmap_bytes((uint8_t)d.box_w, (uint8_t)d.box_h, d.bpp);
    if (!bmp) {
      GFX_LOG("[GFXFONT] %-7s -> dsc OK (%dx%d) nhung BITMAP NULL\n",
                    pname[i], (int)d.box_w, (int)d.box_h);
      continue;
    }
    const uint16_t n8 = need < 8U ? need : 8U;
    GFX_LOG("[GFXFONT] %-7s -> OK  box=%dx%d adv_w=%d ofs=(%d,%d) bpp=%u bytes=%u | "
                  "checksum8=%lu first8=",
                  pname[i], (int)d.box_w, (int)d.box_h, (int)d.adv_w,
                  (int)d.ofs_x, (int)d.ofs_y, (unsigned)d.bpp, (unsigned)need,
                  (unsigned long)gfx_checksum(bmp, n8));
    for (uint16_t k = 0; k < n8; k++) GFX_LOG("%02X ", bmp[k]);
    GFX_LOG("\n");
  }
  GFX_LOG("[GFXFONT] -----------------------------------------------------------\n");
}

#else  /* CYDLCD_USE_LVGL */

/* Cac env STM32 khong-LVGL van compile file nay -> giu 1 khai bao de translation unit
 * khong rong (tranh warning -Wpedantic cua ISO C). */
typedef int lv_font_gfxfont_not_built_for_this_env;

#endif /* CYDLCD_USE_LVGL */
