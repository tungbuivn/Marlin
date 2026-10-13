#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ttf2gfxfont.py -- TTF  ->  Adafruit-GFX-style 1-bit monochrome font header.

Sinh ra header C dung DUNG dinh dang cua Adafruit fontconvert / GFXfont (doi
chieu voi mau that `consola9pt7b.h`):

    const uint8_t <name>Bitmaps[] PROGMEM = { ... };            // 1 bit/pixel
    const GFXglyph <name>Glyphs[] PROGMEM = {
      { bitmapOffset, width, height, xAdvance, xOffset, yOffset }, // 0x21 '!'
      ... };
    const GFXfont <name> PROGMEM = { (uint8_t*)Bitmaps, (GFXglyph*)Glyphs,
                                     first, last, yAdvance };

QUAN TRONG -- quy uoc toa do (giong Adafruit GFXfont):
  * `yOffset` = khoang cach tu BASELINE len mep TREN cua bitmap (am = o TREN
    baseline).  Doi chieu mau `consola9pt7b.h`: '~' co yOffset -6 / height 4 ->
    day bitmap o -2, tuc NAM TREN baseline; '{' va '|' co day o +5 (duoi
    baseline) -- dung nhu mot font thuc.
  * bitmap: moi HANG canh dau byte (row stride = ceil(width/8)), bit CAO ve
    truoc.  Day la dinh dang Adafruit GFX.
  * `yAdvance` = chieu cao dong; baseline nam o `yAdvance - base_line`, va
    `base_line = yAdvance - (ascender thuc te)`.  Tool in ra base_line nay.

CANH BAO cho nguoi doc driver LVGL: LVGL 8.4 KHONG dung dinh dang nay.  No dao
cac hang thanh MOT chuoi bit LIEN TUC (khong dem padding theo hang) -- xem
`lv_draw_sw_letter.c` dong `col_bit += col_bit_row_ofs;` (col_bit_row_ofs = 0
khi hang day du) va kiem chung bang so lieu that: `lv_font_montserrat_14.c`
glyph '!' (3x10, bpp 4) chiem 15 byte = ceil(3*10*4/8), khong phai 20 byte cua
kieu canh hang.  Vi vay `lv_font_gfxfont.c` phai REPACK lai tung glyph truoc khi
tra cho LVGL.  Header nay van giu dung dinh dang GFX de con dung duoc voi
Adafruit_GFX / TFT_eSPI.

Dung:
  python tools/ttf2gfxfont.py --font <ttf> --size 13 --name gfx_font_robotomono \
      --first 0x20 --last 0xFF \
      --extra-ranges 0x0102-0x0103,0x0110-0x0111,0x01A0-0x01B1,0x1EA0-0x1EF9 \
      --range-names ascii,vn1,vn2,vn3,vn4 --yadvance 17 \
      -o src/gfx_font_robotomono.h
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:  # pragma: no cover
    sys.exit("CAN PILOT: python -m pip install pillow")
try:
    from fontTools.ttLib import TTFont
except ImportError:  # pragma: no cover
    sys.exit("CAN fontTools: python -m pip install fonttools")


# ---------------------------------------------------------------------------
#  Codepoint bat buoc phai co NET (dau tieng Viet).  Thieu -> canh bao do.
# ---------------------------------------------------------------------------
MANDATORY = (
    [0x0102, 0x0103, 0x0110, 0x0111, 0x01A0, 0x01A1, 0x01AF, 0x01B0]
    + list(range(0x1EA0, 0x1EFA))
)

# Ky tu luon duoc phep RONG (co advance nhung khong co net)
BLANK_OK = {0x20, 0xA0, 0x200B}


def parse_ranges(spec):
    """'0x20-0x7E,0xA0-0xFF' -> [(0x20,0x7E),(0xA0,0xFF)]"""
    out = []
    for part in spec.replace(" ", "").split(","):
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-", 1)
            lo, hi = int(a, 0), int(b, 0)
        else:
            lo = hi = int(part, 0)
        if lo > hi:
            raise ValueError("dai nguoc: %s" % part)
        if not (0 <= lo <= 0xFFFF and 0 <= hi <= 0xFFFF):
            raise ValueError("codepoint ngoai BMP: %s" % part)
        out.append((lo, hi))
    return out


def merge_and_sort(ranges):
    """Sap xep va gop cac dai dinh nhau/lo chong; bao loi neu chong lan."""
    rs = sorted(ranges)
    for (a1, b1), (a2, b2) in zip(rs, rs[1:]):
        if a2 <= b1:
            raise ValueError("2 dai chong nhau: U+%04X-U+%04X va U+%04X-U+%04X" % (a1, b1, a2, b2))
    return rs


def default_suffix(first, last, index):
    if index == 0 and first == 0x20:
        return "ascii"
    return "u%04X" % first


class Glyph(object):
    __slots__ = ("cp", "offset", "w", "h", "adv", "xoff", "yoff", "bits", "present", "ink")

    def __init__(self, cp):
        self.cp = cp
        self.offset = 0
        self.w = self.h = 0
        self.adv = 0
        self.xoff = self.yoff = 0
        self.bits = b""
        self.present = False
        self.ink = False


def pack_rows(pixels, w, h):
    """pixels: list[h] cua list[w] (0/1) -> bytes, MOI HANG CANH DAU BYTE (Adafruit GFX)."""
    stride = (w + 7) // 8
    out = bytearray(stride * h)
    for r in range(h):
        base = r * stride
        row = pixels[r]
        for c in range(w):
            if row[c]:
                out[base + (c >> 3)] |= 0x80 >> (c & 7)
    return bytes(out)


def rasterize(font, ch, threshold):
    """Rasterize 1 ky tu voi anchor 'ls' (left/baseline) -> (w, h, xoff, yoff, pixels).

    anchor 'ls' nghia la goc (0,0) nam NGAY TREN BASELINE, nen bbox do duoc da
    truc tiep la toa do so voi baseline: yoff < 0 = net nam tren baseline.
    """
    size = font.size
    pad = 2 * size + 8
    w_canvas = 4 * size + 16
    h_canvas = 4 * size + 16
    ox, oy = pad, pad + size
    img = Image.new("L", (w_canvas, h_canvas), 0)
    ImageDraw.Draw(img).text((ox, oy), ch, font=font, fill=255, anchor="ls")
    bb = img.getbbox()
    if bb is None:
        return None
    x0, y0, x1, y1 = bb
    gw, gh = x1 - x0, y1 - y0
    crop = img.crop(bb)
    px = crop.load()
    rows = []
    for r in range(gh):
        rows.append([1 if px[c, r] >= threshold else 0 for c in range(gw)])
    if not any(any(r) for r in rows):
        return None
    return gw, gh, x0 - ox, y0 - oy, rows


def c_byte_array(name, data, per_line=12):
    lines = ["static const uint8_t %s[] PROGMEM = {" % name]
    for i in range(0, len(data), per_line):
        chunk = data[i:i + per_line]
        lines.append("  " + " ".join("0x%02X," % b for b in chunk))
    lines.append("};")
    return "\n".join(lines)


def c_glyph_array(name, glyphs):
    lines = ["static const GFXglyph %s[] PROGMEM = {" % name]
    for g in glyphs:
        label = printable_label(g.cp)
        lines.append("  { %5d, %2d, %2d, %2d, %3d, %3d },  // 0x%02X %s"
                     % (g.offset, g.w, g.h, g.adv, g.xoff, g.yoff, g.cp, label))
    lines.append("};")
    return "\n".join(lines)


def printable_label(cp):
    ch = chr(cp)
    if ch == "\\":
        return "'\\\\'"
    if ch == "'":
        return '"\'"'
    if cp < 0x20 or cp == 0x7F:
        return "---"
    if not ch.isprintable():
        return "---"
    try:
        ch.encode("ascii")
        return "'%s'" % ch
    except UnicodeEncodeError:
        return "'\\u%04X'" % cp


def main():
    ap = argparse.ArgumentParser(description="TTF -> Adafruit GFXfont header (1-bit mono)")
    ap.add_argument("--font", required=True, help="duong dan file .ttf/.otf")
    ap.add_argument("--size", type=int, required=True, help="co chu theo PIXEL (em size cua FreeType)")
    ap.add_argument("--name", required=True, help="tien to symbol, vd gfx_font_robotomono")
    ap.add_argument("-o", "--out", required=True, help="file .h xuat ra")
    ap.add_argument("--first", default="0x20", help="codepoint dau cua DAI THU NHAT (mac dinh 0x20)")
    ap.add_argument("--last", default="0x7E", help="codepoint cuoi cua DAI THU NHAT (mac dinh 0x7E)")
    ap.add_argument("--extra-ranges", default="",
                    help="cac dai them, vd 0x1EA0-0x1EF9,0x0102-0x0103 (moi dai = 1 GFXfont rieng)")
    ap.add_argument("--range-names", default="",
                    help="ten phu cho tung dai, vd ascii,vn1,vn2 (mac dinh: ascii/uXXXX)")
    ap.add_argument("--yadvance", type=int, default=0,
                    help="chieu cao dong; 0 = tu tinh (= ascender + descender thuc te)")
    ap.add_argument("--threshold", type=int, default=128,
                    help="nguong 1-bit hoa anti-alias (0..255, mac dinh 128)")
    ap.add_argument("--gap-advance", type=int, default=0,
                    help="xAdvance cho o TRONG (codepoint khong co trong font). 0 = danh dau "
                         "trong (driver coi la 'khong co glyph'). Nen de 0.")
    ap.add_argument("--dump", default="",
                    help="in ASCII art cac codepoint nay (vd 0x41,0x30,0x1EC7) de tu kiem tra")
    args = ap.parse_args()

    if not os.path.isfile(args.font):
        sys.exit("khong thay font: %s" % args.font)
    if not (0 <= args.threshold <= 255):
        sys.exit("--threshold phai trong 0..255")

    ranges = merge_and_sort(parse_ranges("%s-%s" % (args.first, args.last))
                            + parse_ranges(args.extra_ranges))
    names = [s for s in args.range_names.split(",") if s]
    if names and len(names) != len(ranges):
        sys.exit("--range-names co %d ten nhung co %d dai" % (len(names), len(ranges)))
    suffixes = names if names else [default_suffix(a, b, i) for i, (a, b) in enumerate(ranges)]

    # ---- do phu codepoint (fontTools) : phan biet 'khong co trong font' vs 'rong' ----
    tt = TTFont(args.font, fontNumber=0)
    cmap = set()
    for t in tt["cmap"].tables:
        cmap |= set(t.cmap.keys())
    upem = tt["head"].unitsPerEm

    font = ImageFont.truetype(args.font, args.size)
    # advance cua font mono: do tren vai ky tu, phai giong nhau
    advs = [font.getlength(c) for c in "M0iW."]
    mono_adv = int(round(max(advs)))
    adv_spread = max(advs) - min(advs)
    if not (0 < mono_adv < 256):
        sys.exit("xAdvance %d khong hop le (GFXglyph.xAdvance la uint8_t)" % mono_adv)

    print("=" * 78)
    print("ttf2gfxfont: %s" % args.font)
    print("  size (px)      : %d      unitsPerEm %d" % (args.size, upem))
    print("  xAdvance       : %d px  (do lech giua cac ky tu = %.3f px -> %s)"
          % (mono_adv, adv_spread, "MONO that" if adv_spread < 0.01 else "CANH BAO: khong deu!"))
    print("  nguong 1-bit   : %d" % args.threshold)

    # ---- sinh tung dai ----
    blocks = []
    total_glyphs = total_ink = total_bytes = 0
    all_ink_glyphs = []
    missing_in_ttf = []
    warnings = []

    for (lo, hi), suf in zip(ranges, suffixes):
        glyphs = []
        bitmap = bytearray()
        for cp in range(lo, hi + 1):
            g = Glyph(cp)
            if cp not in cmap:
                missing_in_ttf.append(cp)
                g.w = g.h = 0
                g.adv = args.gap_advance
                g.xoff = g.yoff = 0
                g.present = False
            else:
                g.present = True
                g.adv = mono_adv
                ras = rasterize(font, chr(cp), args.threshold)
                if ras is None:
                    g.w = g.h = 0
                    g.xoff = g.yoff = 0
                else:
                    gw, gh, xoff, yoff, rows = ras
                    if gw > 255 or gh > 255:
                        sys.exit("glyph U+%04X qua lon: %dx%d" % (cp, gw, gh))
                    if not (-128 <= xoff <= 127) or not (-128 <= yoff <= 127):
                        sys.exit("glyph U+%04X offset ngoai int8: x=%d y=%d" % (cp, xoff, yoff))
                    g.w, g.h, g.xoff, g.yoff = gw, gh, xoff, yoff
                    g.bits = pack_rows(rows, gw, gh)
                    g.ink = True
                    all_ink_glyphs.append(g)
                    bitmap += g.bits
            g.offset = len(bitmap) - (len(g.bits) if g.ink else 0)
            glyphs.append(g)

        if len(bitmap) > 0xFFFF:
            sys.exit("dai U+%04X-U+%04X: bitmap %d byte > 65535 (GFXglyph.bitmapOffset la uint16_t)"
                     % (lo, hi, len(bitmap)))
        ink = sum(1 for g in glyphs if g.ink)
        total_glyphs += len(glyphs)
        total_ink += ink
        total_bytes += len(bitmap)
        blocks.append(dict(lo=lo, hi=hi, suf=suf, glyphs=glyphs, bitmap=bytes(bitmap)))
        print("  dai %-6s U+%04X..U+%04X : %3d o, %3d glyph co net, bitmap %5d byte"
              % (suf, lo, hi, len(glyphs), ink, len(bitmap)))

    # ---- yAdvance / base_line theo so lieu THAT ----
    if not all_ink_glyphs:
        sys.exit("khong co glyph nao co net!")
    asc_eff = max(-g.yoff for g in all_ink_glyphs)                  # so hang can TREN baseline
    desc_eff = max(g.yoff + g.h for g in all_ink_glyphs)            # so hang can DUOI baseline
    need = asc_eff + desc_eff
    yadv = args.yadvance if args.yadvance else need
    if yadv < need:
        warnings.append("yAdvance %d < %d hang can thiet (ascender %d + descender %d) -> "
                        "net se bi cat" % (yadv, need, asc_eff, desc_eff))
    base_line = yadv - asc_eff
    if base_line < 0:
        warnings.append("base_line am (%d) -> ep ve 0" % base_line)
        base_line = 0
    if base_line > yadv:
        warnings.append("base_line %d > yAdvance %d -> ep bang yAdvance" % (base_line, yadv))
        base_line = yadv

    max_h = max(g.h for g in all_ink_glyphs)
    max_w = max(g.w for g in all_ink_glyphs)
    max_xoff = max(abs(g.xoff) for g in all_ink_glyphs)
    print("  --- do that tu bitmap ---")
    print("  ascender  (hang tren baseline) : %d" % asc_eff)
    print("  descender (hang duoi baseline) : %d" % desc_eff)
    print("  => chieu cao dong toi thieu    : %d" % need)
    print("  yAdvance dung                  : %d %s" % (yadv, "(auto)" if not args.yadvance else ""))
    print("  base_line = yAdvance - ascender: %d" % base_line)
    print("  box_h max / box_w max          : %d / %d" % (max_h, max_w))
    print("  |xOffset| max                  : %d" % max_xoff)
    print("  TONG: %d o, %d glyph co net, %d byte bitmap" % (total_glyphs, total_ink, total_bytes))

    # ---- tu kiem tra ----
    ink_cps = set(g.cp for g in all_ink_glyphs)
    miss_mand = [cp for cp in MANDATORY if cp not in ink_cps]
    if miss_mand:
        warnings.append("THIEU NET cho %d codepoint tieng Viet bat buoc: %s"
                        % (len(miss_mand), " ".join("U+%04X" % c for c in miss_mand[:16])))
    not_in_ttf_mand = [cp for cp in miss_mand if cp not in cmap]
    if not_in_ttf_mand:
        warnings.append("trong do %d codepoint KHONG co trong TTF: %s"
                        % (len(not_in_ttf_mand), " ".join("U+%04X" % c for c in not_in_ttf_mand[:16])))
    if missing_in_ttf:
        print("  codepoint khong co trong TTF (o trong, xAdvance=%d): %d"
              % (args.gap_advance, len(missing_in_ttf)))

    # ---- phat header ----
    guard = args.name.upper() + "_H"
    out = []
    out.append("/*")
    out.append(" * %s -- font 1-bit mono sinh tu TTF bang tools/ttf2gfxfont.py" % os.path.basename(args.out))
    out.append(" *")
    out.append(" *   font    : %s" % os.path.basename(args.font))
    out.append(" *   size    : %d px (em size FreeType)   nguong 1-bit: %d" % (args.size, args.threshold))
    out.append(" *   xAdvance: %d px (mono)   yAdvance: %d px   base_line: %d px" % (mono_adv, yadv, base_line))
    out.append(" *   do dai  : %d dai, %d o, %d glyph co net, %d byte bitmap"
               % (len(blocks), total_glyphs, total_ink, total_bytes))
    out.append(" *")
    out.append(" * Dinh dang Adafruit GFXfont (xem mau that consola9pt7b.h):")
    out.append(" *   - bitmap 1 bit/pixel, MOI HANG CANH DAU BYTE (stride = ceil(width/8)), bit cao ve truoc")
    out.append(" *   - yOffset = tu BASELINE len mep tren bitmap (am = o TREN baseline)")
    out.append(" *   - yAdvance = chieu cao dong; baseline nam o (yAdvance - base_line) tu mep tren dong")
    out.append(" *")
    out.append(" *  !! LVGL 8.4 KHONG doc duoc truc tiep dinh dang nay: no dao cac hang thanh MOT chuoi")
    out.append(" *     bit LIEN TUC, khong dem padding theo hang (lv_draw_sw_letter.c: `col_bit += ")
    out.append(" *     col_bit_row_ofs;` = 0 voi hang day du; kiem chung: montserrat_14 glyph '!' 3x10")
    out.append(" *     bpp4 chiem 15 byte = ceil(3*10*4/8), khong phai 20).")
    out.append(" *     Kiem chung lai: tools/probe_lvgl_font_bitmap_layout.py va")
    out.append(" *     tools/probe_lvgl_glyph_render.py (in ASCII art theo ca 2 cach xep hang).")
    out.append(" *     -> lv_font_gfxfont.c REPACK lai tung glyph truoc khi tra cho LVGL.")
    out.append(" *     Header nay van dung duoc voi Adafruit_GFX / TFT_eSPI.")
    out.append(" */")
    out.append("")
    out.append("#ifndef %s" % guard)
    out.append("#define %s" % guard)
    out.append("")
    out.append("#include <stdint.h>")
    out.append("")
    out.append("#ifndef PROGMEM")
    out.append("  #define PROGMEM")
    out.append("#endif")
    out.append("")
    out.append("/* Kieu GFXglyph/GFXfont cua Adafruit. Guard de khong dung do neu Adafruit_GFX")
    out.append(" * (hoac header font khac cung kieu) da duoc include truoc. */")
    out.append("#ifndef LVGL_GFXFONT_STRUCTS_DEFINED")
    out.append("#define LVGL_GFXFONT_STRUCTS_DEFINED")
    out.append("typedef struct {")
    out.append("  uint16_t bitmapOffset;")
    out.append("  uint8_t  width, height;")
    out.append("  uint8_t  xAdvance;")
    out.append("  int8_t   xOffset, yOffset;")
    out.append("} GFXglyph;")
    out.append("")
    out.append("typedef struct {")
    out.append("  uint8_t  *bitmap;")
    out.append("  GFXglyph *glyph;")
    out.append("  uint16_t first, last;")
    out.append("  uint8_t  yAdvance;")
    out.append("} GFXfont;")
    out.append("#endif /* LVGL_GFXFONT_STRUCTS_DEFINED */")
    out.append("")

    for b in blocks:
        sym = "%s_%s" % (args.name, b["suf"])
        out.append("/* ---- dai %s : U+%04X..U+%04X (%d o, %d byte bitmap) ---- */"
                   % (b["suf"], b["lo"], b["hi"], len(b["glyphs"]), len(b["bitmap"])))
        out.append(c_byte_array(sym + "Bitmaps", b["bitmap"]))
        out.append("")
        out.append(c_glyph_array(sym + "Glyphs", b["glyphs"]))
        out.append("")
        out.append("const GFXfont %s PROGMEM = {" % sym)
        out.append("  (uint8_t  *)%sBitmaps," % sym)
        out.append("  (GFXglyph *)%sGlyphs," % sym)
        out.append("  0x%02X, 0x%02X, %d };" % (b["lo"], b["hi"], yadv))
        out.append("")

    out.append("/* Bang cac dai de truyen thang cho lv_font_gfxfont_get(ranges, n) (tra tuan tu).")
    out.append(" * KHONG static: dinh nghia that trong dung 1 translation unit (file .c chi")
    out.append(" * `#include` header nay), nho vay khong bi nhan ban du lieu flash. */")
    out.append("const GFXfont *const %s_ranges[%d] = {" % (args.name, len(blocks)))
    for i, b in enumerate(blocks):
        out.append("  &%s_%s%s" % (args.name, b["suf"], "," if i < len(blocks) - 1 else ""))
    out.append("};")
    out.append("#define %s_RANGE_COUNT %d" % (args.name.upper(), len(blocks)))
    out.append("")
    out.append("#endif /* %s */" % guard)
    out.append("")

    with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
        fh.write("\n".join(out))
    print("  -> da ghi %s (%d byte)" % (args.out, os.path.getsize(args.out)))

    if warnings:
        print("\n  !!! CANH BAO !!!")
        for w in warnings:
            print("    - " + w)
    else:
        print("  tu kiem tra: OK (khong canh bao)")

    # ---- ASCII art tu kiem tra ----
    if args.dump:
        cps = parse_ranges(args.dump.replace("0x", "0x"))
        want = [c for lo, hi in cps for c in range(lo, hi + 1)]
        by_cp = {}
        for b in blocks:
            for g in b["glyphs"]:
                if g.ink:
                    by_cp[g.cp] = g
        print("\n  --- ASCII art (tu bitmap DA SINH, giai ma theo kieu Adafruit canh hang) ---")
        for cp in want:
            g = by_cp.get(cp)
            if g is None:
                print("  U+%04X: khong co net" % cp)
                continue
            stride = (g.w + 7) // 8
            print("  U+%04X  box %dx%d  xAdv %d  xOff %d  yOff %d  (%d byte)"
                  % (cp, g.w, g.h, g.adv, g.xoff, g.yoff, len(g.bits)))
            for r in range(g.h):
                line = "".join("#" if (g.bits[r * stride + (c >> 3)] >> (7 - (c & 7))) & 1 else "."
                               for c in range(g.w))
                print("    |%s|" % line)

    return 0


if __name__ == "__main__":
    sys.exit(main())
