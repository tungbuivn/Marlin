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


def parse_maps(specs):
    """['0xE000=0xE63E', ...] -> {0xE000: 0xE63E}  (codepoint DICH -> codepoint NGUON).

    Can cho font ICON: font nguon (vd Material Icons) khong co glyph o U+E000, ma lai co
    o U+E63E, nen phai doi codepoint de bang glyph cua ta doc lap voi font nguon.
    """
    m = {}
    for s in specs:
        if "=" not in s:
            raise ValueError("--map phai co dang DICH=NGUON, vd 0xE000=0xE63E (gap: %r)" % s)
        t, src = s.split("=", 1)
        tgt_cp, src_cp = int(t, 0), int(src, 0)
        if not (0 <= tgt_cp <= 0xFFFF and 0 <= src_cp <= 0xFFFF):
            raise ValueError("--map: codepoint ngoai BMP: %s" % s)
        if tgt_cp in m and m[tgt_cp] != src_cp:
            raise ValueError("--map: U+%04X duoc anh xa 2 lan khac nhau" % tgt_cp)
        m[tgt_cp] = src_cp
    return m


class Glyph(object):
    __slots__ = ("cp", "offset", "w", "h", "adv", "xoff", "yoff", "bits", "present", "ink",
                 "src_cp")

    def __init__(self, cp, src_cp=None):
        self.cp = cp
        self.src_cp = cp if src_cp is None else src_cp
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


def pack_nibbles(levels, w, h):
    """pixels: list[h] cua list[w] (0..15) -> bytes theo cach xep bpp4 CUA LVGL.

    LVGL (lv_draw_sw_letter.c) doc bitmap bpp4 nhu MOT CHUOI BIT LIEN TUC:
        bit = row*box_w + col ; col chan -> nibble CAO cua byte (bit>>3), col le -> nibble THAP
    => so byte = ceil(w*h*4/8) = ceil(w*h/2); KHONG canh byte theo tung hang.
    (Da kiem chung bang so lieu: lv_font_montserrat_14.c glyph '!' 3x10 bpp4 = dung 15 byte.)
    """
    out = bytearray(((w * h) * 4 + 7) // 8)
    i = 0
    for r in range(h):
        row = levels[r]
        for c in range(w):
            v = row[c] & 0x0F
            if i & 1:
                out[i >> 1] |= v
            else:
                out[i >> 1] |= v << 4
            i += 1
    return bytes(out)


def rasterize(font, ch, threshold, bpp):
    """Rasterize 1 ky tu voi anchor 'ls' (left/baseline) -> (w, h, xoff, yoff, pixels).

    anchor 'ls' nghia la goc (0,0) nam NGAY TREN BASELINE, nen bbox do duoc da
    truc tiep la toa do so voi baseline: yoff < 0 = net nam tren baseline.

    bpp = 1: nhi phan hoa theo `threshold`; bbox lay theo pixel KHAC 0 cua anh goc
             (GIU NGUYEN nhu ban da nghiem thu tren may that - khong doi mot byte).
    bpp = 4: GIU NGUYEN muc xam (khong threshold), luong tu ve 16 muc (nibble);
             bbox lay theo pixel CO MUC > 0 SAU luong tu, de hop glyph khong bi
             phinh ra boi cac pixel mem (muc 0 sau luong tu).
    """
    size = font.size
    pad = 2 * size + 8
    w_canvas = 4 * size + 16
    h_canvas = 4 * size + 16
    ox, oy = pad, pad + size
    img = Image.new("L", (w_canvas, h_canvas), 0)
    ImageDraw.Draw(img).text((ox, oy), ch, font=font, fill=255, anchor="ls")

    if bpp == 1:
        bb = img.getbbox()
        if bb is None:
            return None
        x0, y0, x1, y1 = bb
        gw, gh = x1 - x0, y1 - y0
        px = img.crop(bb).load()
        rows = [[1 if px[c, r] >= threshold else 0 for c in range(gw)] for r in range(gh)]
        if not any(any(r) for r in rows):
            return None
        return gw, gh, x0 - ox, y0 - oy, rows

    # ---- bpp 4: luong tu 0..255 -> 0..15 (lam tron), roi tim bbox theo muc > 0 ----
    px = img.load()
    lv = [[(px[c, r] * 15 + 127) // 255 for c in range(w_canvas)] for r in range(h_canvas)]
    minx, miny, maxx, maxy = w_canvas, h_canvas, -1, -1
    for r in range(h_canvas):
        row = lv[r]
        for c in range(w_canvas):
            if row[c]:
                if c < minx: minx = c
                if c > maxx: maxx = c
                if r < miny: miny = r
                if r > maxy: maxy = r
    if maxx < 0:
        return None
    gw, gh = maxx - minx + 1, maxy - miny + 1
    rows = [[lv[miny + r][minx + c] for c in range(gw)] for r in range(gh)]
    return gw, gh, minx - ox, miny - oy, rows


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
        label = printable_label(g.src_cp)
        src = "" if g.src_cp == g.cp else " <- U+%04X" % g.src_cp
        lines.append("  { %5d, %2d, %2d, %2d, %3d, %3d },  // 0x%02X%s %s"
                     % (g.offset, g.w, g.h, g.adv, g.xoff, g.yoff, g.cp, src, label))
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
    ap.add_argument("--baseline", type=int, default=0,
                    help="base_line = khoang cach tu DAY hop dong len baseline; 0 = tu tinh "
                         "(= yAdvance - ascender). Dat bang base_line cua font chu (vd 3) neu "
                         "muon ICON thang hang baseline voi chu trong cung mot dong.")
    ap.add_argument("--yshift", type=int, default=0,
                    help="cong them N px vao yOffset cua MOI glyph (N > 0 = dich bitmap XUONG). "
                         "CHI co tac dung khi --yadvance hoac --baseline duoc dat ro: driver tinh "
                         "base_line = yAdvance - ascender, ma --yshift dich ca hai nhu nhau nen "
                         "voi che do tu dong (auto) no KHONG doi vi tri ve. Dung kem --yadvance "
                         "nho hon de 'thu' hop dong lai cho khit net (xem gfx4_font_icons).")
    ap.add_argument("--advance", type=int, default=0,
                    help="ep xAdvance (px) cho moi glyph; 0 = do tu font (font chu: advance mono)")
    ap.add_argument("--map", action="append", default=[], metavar="DICH=NGUON",
                    help="doi codepoint: xuat glyph cua codepoint NGUON vao o codepoint DICH "
                         "(vd --map 0xE000=0xE63E). Can cho font ICON vi font nguon khong co "
                         "glyph o U+E000. Dung duoc nhieu lan.")
    ap.add_argument("--threshold", type=int, default=128,
                    help="nguong 1-bit hoa anti-alias (0..255, mac dinh 128). CHI dung cho --bpp 1")
    ap.add_argument("--bpp", type=int, default=1, choices=(1, 4),
                    help="1 = 1 bit/pixel, bitmap CANH HANG kieu Adafruit GFXfont (mac dinh, "
                         "header dung duoc voi Adafruit_GFX/TFT_eSPI; driver phai repack). "
                         "4 = 4 bit/pixel (16 muc xam, CO antialias), bitmap theo kieu LVGL "
                         "LIEN TUC (khong canh hang) -> KHONG con la Adafruit GFXfont, va driver "
                         "tra con tro THANG khong repack.")
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
    try:
        cp_map = parse_maps(args.map)
    except ValueError as e:
        sys.exit(str(e))

    # ---- do phu codepoint (fontTools) : phan biet 'khong co trong font' vs 'rong' ----
    tt = TTFont(args.font, fontNumber=0)
    cmap = set()
    for t in tt["cmap"].tables:
        cmap |= set(t.cmap.keys())
    upem = tt["head"].unitsPerEm

    font = ImageFont.truetype(args.font, args.size)
    # advance cua font mono: do tren vai ky tu, phai giong nhau
    advs = [font.getlength(c) for c in "M0iW."]
    mono_adv = args.advance if args.advance else int(round(max(advs)))
    adv_spread = max(advs) - min(advs)
    if not (0 < mono_adv < 256):
        sys.exit("xAdvance %d khong hop le (GFXglyph.xAdvance la uint8_t)" % mono_adv)

    print("=" * 78)
    print("ttf2gfxfont: %s" % args.font)
    print("  size (px)      : %d      unitsPerEm %d" % (args.size, upem))
    if args.advance:
        print("  xAdvance       : %d px  (EP bang --advance)" % mono_adv)
    else:
        print("  xAdvance       : %d px  (do lech giua cac ky tu = %.3f px -> %s)"
              % (mono_adv, adv_spread, "MONO that" if adv_spread < 0.01 else "CANH BAO: khong deu!"))
    print("  bpp            : %d  (%s)" % (
        args.bpp,
        "Adafruit GFXfont, moi hang canh dau byte -> driver REPACK" if args.bpp == 1 else
        "16 muc xam, kieu LVGL LIEN TUC -> driver tra con tro THANG (khong phai Adafruit)"))
    if args.bpp == 1:
        print("  nguong 1-bit   : %d" % args.threshold)
    if args.yshift:
        print("  yshift         : %+d px (dich bitmap)" % args.yshift)
    if cp_map:
        print("  doi codepoint  : %s"
              % ", ".join("U+%04X<-U+%04X" % (t, s) for t, s in sorted(cp_map.items())))

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
            src_cp = cp_map.get(cp, cp)          # codepoint NGUON (font icon: doi codepoint)
            g = Glyph(cp, src_cp)
            if src_cp not in cmap:
                missing_in_ttf.append(src_cp)
                g.w = g.h = 0
                g.adv = args.gap_advance
                g.xoff = g.yoff = 0
                g.present = False
            else:
                g.present = True
                g.adv = mono_adv
                ras = rasterize(font, chr(src_cp), args.threshold, args.bpp)
                if ras is None:
                    g.w = g.h = 0
                    g.xoff = g.yoff = 0
                else:
                    gw, gh, xoff, yoff, rows = ras
                    if gw > 255 or gh > 255:
                        sys.exit("glyph U+%04X qua lon: %dx%d" % (src_cp, gw, gh))
                    yoff += args.yshift
                    if not (-128 <= xoff <= 127) or not (-128 <= yoff <= 127):
                        sys.exit("glyph U+%04X offset ngoai int8: x=%d y=%d" % (src_cp, xoff, yoff))
                    g.w, g.h, g.xoff, g.yoff = gw, gh, xoff, yoff
                    g.bits = pack_rows(rows, gw, gh) if args.bpp == 1 else pack_nibbles(rows, gw, gh)
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
    # Chieu cao dong toi thieu. LUU Y: driver dat base_line = yAdvance - ascender va KHONG am,
    # nen phai co yAdvance >= ascender. Khi MOI net nam TREN baseline (desc_eff < 0, vd font
    # icon) thi yAdvance = ascender la du; cong them phan descender chi khi desc_eff > 0.
    need = asc_eff + (desc_eff if desc_eff > 0 else 0)
    yadv = args.yadvance if args.yadvance else need
    if yadv < need:
        warnings.append("yAdvance %d < %d hang can thiet (ascender %d + descender %d) -> "
                        "net se bi cat" % (yadv, need, asc_eff, desc_eff))
    if args.baseline:
        base_line = args.baseline
        # baseline nam o (yAdvance - base_line) tu mep tren dong
        above = yadv - base_line
        if asc_eff > above:
            warnings.append("base_line %d: chi con %d hang TREN baseline nhung glyph can %d -> "
                            "dinh glyph bi cat %d px" % (base_line, above, asc_eff, asc_eff - above))
        if desc_eff > base_line:
            warnings.append("base_line %d: chi con %d hang DUOI baseline nhung glyph can %d -> "
                            "day glyph bi cat %d px" % (base_line, base_line, desc_eff,
                                                        desc_eff - base_line))
    else:
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
    if args.baseline:
        print("  base_line                      : %d  (EP bang --baseline; auto se la %d)"
              % (base_line, yadv - asc_eff))
    else:
        print("  base_line = yAdvance - ascender: %d" % base_line)
    print("  baseline o %d tu mep tren dong, %d tu mep duoi" % (yadv - base_line, base_line))
    print("  box_h max / box_w max          : %d / %d" % (max_h, max_w))
    print("  |xOffset| max                  : %d" % max_xoff)
    print("  TONG: %d o, %d glyph co net, %d byte bitmap" % (total_glyphs, total_ink, total_bytes))

    # ---- tu kiem tra ----
    # CHI kiem cac codepoint tieng Viet NAM TRONG dai duoc yeu cau: font icon (vung PUA) se
    # khong bi canh bao "thieu dau tieng Viet" nua.
    ink_cps = set(g.cp for g in all_ink_glyphs)
    requested = set()
    for lo, hi in ranges:
        requested.update(range(lo, hi + 1))
    mand_in_range = [cp for cp in MANDATORY if cp in requested]
    miss_mand = [cp for cp in mand_in_range if cp not in ink_cps]
    if miss_mand:
        warnings.append("THIEU NET cho %d codepoint tieng Viet bat buoc: %s"
                        % (len(miss_mand), " ".join("U+%04X" % c for c in miss_mand[:16])))
    not_in_ttf_mand = [cp for cp in miss_mand if cp_map.get(cp, cp) not in cmap]
    if not_in_ttf_mand:
        warnings.append("trong do %d codepoint KHONG co trong TTF: %s"
                        % (len(not_in_ttf_mand), " ".join("U+%04X" % c for c in not_in_ttf_mand[:16])))
    if args.map:
        # Voi font icon: moi codepoint DICH phai CO NET, neu khong la loi anh xa.
        empty_mapped = [t for t in sorted(cp_map) if t not in ink_cps]
        if empty_mapped:
            warnings.append("--map nhung KHONG co net: %s"
                            % " ".join("U+%04X<-U+%04X" % (t, cp_map[t]) for t in empty_mapped))
    if missing_in_ttf:
        print("  codepoint khong co trong TTF (o trong, xAdvance=%d): %d"
              % (args.gap_advance, len(missing_in_ttf)))

    # ---- phat header ----
    guard = args.name.upper() + "_H"
    out = []
    out.append("/*")
    out.append(" * %s -- font sinh tu TTF bang tools/ttf2gfxfont.py" % os.path.basename(args.out))
    out.append(" *")
    out.append(" *   font    : %s" % os.path.basename(args.font))
    out.append(" *   size    : %d px (em size FreeType)%s"
               % (args.size, "   nguong 1-bit: %d" % args.threshold if args.bpp == 1 else
                  "   16 muc xam (4 bit/pixel), KHONG threshold"))
    out.append(" *   bpp     : %d" % args.bpp)
    out.append(" *   xAdvance: %d px %s  yAdvance: %d px   base_line: %d px"
               % (mono_adv, "(mono)" if not args.advance else "(EP)", yadv, base_line))
    out.append(" *   do dai  : %d dai, %d o, %d glyph co net, %d byte bitmap"
               % (len(blocks), total_glyphs, total_ink, total_bytes))
    out.append(" *")
    if cp_map:
        out.append(" * DOI CODEPOINT (font icon: font nguon khong co glyph o codepoint dich):")
        for t, s in sorted(cp_map.items()):
            out.append(" *   U+%04X  <-  U+%04X  (font nguon)" % (t, s))
        out.append(" *")
    if args.yshift:
        out.append(" * yOffset da duoc cong %+d px (--yshift) cho moi glyph." % args.yshift)
    out.append(" * yOffset = tu BASELINE len mep tren bitmap (am = o TREN baseline).")
    out.append(" * yAdvance = chieu cao dong; baseline nam o (yAdvance - base_line) tu mep tren dong.")
    if args.baseline:
        out.append(" * base_line duoc EP bang --baseline de TRUNG voi font chu")
        out.append(" * (gfx4_font_robotomono: yAdvance 17 / base_line 3), nho vay icon thang")
        out.append(" * hang baseline voi chu khi nam trong cung mot dong.")
    out.append(" *")
    if args.bpp == 1:
        out.append(" * DINH DANG: Adafruit GFXfont (xem mau that consola9pt7b.h)")
        out.append(" *   - bitmap 1 bit/pixel, MOI HANG CANH DAU BYTE (stride = ceil(width/8)), bit cao truoc")
        out.append(" *")
        out.append(" *  !! LVGL 8.4 KHONG doc duoc truc tiep dinh dang nay: no dao cac hang thanh MOT chuoi")
        out.append(" *     bit LIEN TUC, khong dem padding theo hang (lv_draw_sw_letter.c: `col_bit += ")
        out.append(" *     col_bit_row_ofs;` = 0 voi hang day du; kiem chung: montserrat_14 glyph '!' 3x10")
        out.append(" *     bpp4 chiem 15 byte = ceil(3*10*4/8), khong phai 20).")
        out.append(" *     Kiem chung lai: tools/probe_lvgl_font_bitmap_layout.py va")
        out.append(" *     tools/probe_lvgl_glyph_render.py (in ASCII art theo ca 2 cach xep hang).")
        out.append(" *     -> lv_font_gfxfont.c REPACK lai tung glyph truoc khi tra cho LVGL.")
        out.append(" *     Header nay van dung duoc voi Adafruit_GFX / TFT_eSPI.")
    else:
        out.append(" * DINH DANG: **KHONG phai Adafruit GFXfont** - day la dinh dang RIENG cua ta, viet")
        out.append(" *   thang theo cach LVGL 8.4 doc bitmap bpp4 (lv_draw_sw_letter.c):")
        out.append(" *   - 4 bit/pixel = 16 muc xam (co antialias). Muc n -> do dam n*17 (0..255),")
        out.append(" *     dung bang _lv_bpp4_opa_table[] cua LVGL.")
        out.append(" *   - bitmap la MOT CHUOI BIT LIEN TUC: bit = row*width + col; col CHAN -> nibble CAO")
        out.append(" *     cua byte, col LE -> nibble THAP. KHONG canh byte theo tung hang.")
        out.append(" *   - so byte moi glyph = ceil(width*height*4/8) = ceil(width*height/2)")
        out.append(" *     (vd glyph 3x10 = 15 byte, khong phai 20 nhu kieu canh hang).")
        out.append(" *   - vi vay driver tra con tro THANG vao mang bitmap, KHONG repack.")
        out.append(" *   Kiem chung: tools/probe_lvgl_font_bitmap_layout.py (67/67 va 253/253 glyph cua")
        out.append(" *   font chuan LVGL deu la 'lien tuc') va tools/test_gfxfont_layout.py.")
        out.append(" *   KHONG dung header nay voi Adafruit_GFX / TFT_eSPI (se ra hinh sai).")
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
    out.append("#define %s_BPP %d" % (args.name.upper(), args.bpp))
    out.append("#define %s_YADVANCE %d" % (args.name.upper(), yadv))
    out.append("#define %s_BASELINE %d" % (args.name.upper(), base_line))
    out.append("#define %s_ASCENDER %d" % (args.name.upper(), asc_eff))
    out.append("#define %s_DESCENDER %d" % (args.name.upper(), desc_eff))
    out.append("#define %s_XADVANCE %d" % (args.name.upper(), mono_adv))
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
        print("\n  --- ASCII art (giai ma lai TU BITMAP DA SINH, dung cach xep cua LVGL) ---")
        for cp in want:
            g = by_cp.get(cp)
            if g is None:
                print("  U+%04X: khong co net" % cp)
                continue
            print("  U+%04X  box %dx%d  xAdv %d  xOff %d  yOff %d  (%d byte, bpp %d)"
                  % (cp, g.w, g.h, g.adv, g.xoff, g.yoff, len(g.bits), args.bpp))
            if args.bpp == 1:
                stride = (g.w + 7) // 8
                for r in range(g.h):
                    print("    |%s|" % "".join(
                        "#" if (g.bits[r * stride + (c >> 3)] >> (7 - (c & 7))) & 1 else "."
                        for c in range(g.w)))
            else:
                # giai ma nibble theo dung chuoi bit lien tuc cua LVGL
                shades = " .:-=+*#%@"   # 10 muc cho de nhin (0..15 -> 0..9)
                for r in range(g.h):
                    line = []
                    for c in range(g.w):
                        bit = r * g.w + c
                        v = (g.bits[bit >> 1] >> (4 if (bit & 1) == 0 else 0)) & 0x0F
                        line.append(shades[min(9, v * 9 // 15)])
                    print("    |%s|" % "".join(line))

    return 0


if __name__ == "__main__":
    sys.exit(main())
