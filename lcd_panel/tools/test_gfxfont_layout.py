#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_gfxfont_layout.py -- kiem tra OFFLINE (khong can phan cung) toan bo chuoi:

    header font  ->  anh xa cua lv_font_gfxfont.c  ->  luat ve cua LVGL 8.4

Muc dich: xac nhan ban do glyph (dac biet ofs_y / base_line / adv_w, viec REPACK bitmap
bpp1 tu kieu Adafruit "canh hang" sang kieu LVGL "lien tuc", va cach xep nibble bpp4)
la DUNG, truoc khi nap.

No mo phong dung 3 thu:
  1) get_glyph_dsc  : ofs_y = -(yOffset + height) ; adv_w = xAdvance (PIXEL NGUYEN)
  2) get_glyph_bitmap:
        bpp 1 -> repack hang-canh-byte -> chuoi bit lien tuc
        bpp 4 -> tra THANG (header da ghi san theo chuoi bit lien tuc cua LVGL)
  3) lv_draw_sw_letter (LVGL 8.4):
        gpos.x = pos.x + ofs_x
        gpos.y = pos.y + (line_height - base_line) - box_h - ofs_y
        doc bitmap: bit = row*box_w + col
          bpp1: bit cao truoc trong byte
          bpp4: col chan -> nibble CAO cua byte, col le -> nibble THAP
     roi doi chieu voi ban ve truc tiep bang FreeType (Pillow) cung co chu:
        bpp1 -> so khop nhi phan (nguong 128), phai GIONG HET
        bpp4 -> so khop MUC XAM: |muc_ta - muc_FreeType| <= 1 muc 4-bit (sai so luong tu)

Dung:
  python tools/test_gfxfont_layout.py --header src/gfx4_font_robotomono.h \
      --font D:/fonts/gfxfont/google/RobotoMono-Regular.ttf --size 13 \
      --cps 0x4E,0x48,0x1EC6,0x54 --selftest-expect
"""

import argparse
import re
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("CAN PILOW: python -m pip install pillow")


# ---------------------------------------------------------------------------
#  Doc header font
# ---------------------------------------------------------------------------
def parse_header(path):
    src = open(path, "r", encoding="utf-8").read()
    m = re.search(r"#define\s+(\w+)_BPP\s+(\d+)", src)
    if not m:
        sys.exit("header thieu #define <NAME>_BPP (sinh lai bang tools/ttf2gfxfont.py)")
    bpp = int(m.group(2))
    if bpp not in (1, 4):
        sys.exit("bpp %d khong ho tro" % bpp)

    bitmaps, glyphs, fonts = {}, {}, []
    for mm in re.finditer(r"static const uint8_t (\w+)\[\] PROGMEM = \{(.*?)\n\};", src, re.S):
        bitmaps[mm.group(1)] = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", mm.group(2)))
    for mm in re.finditer(r"static const GFXglyph (\w+)\[\] PROGMEM = \{(.*?)\n\};", src, re.S):
        rows = []
        for line in mm.group(2).splitlines():
            nums = re.findall(r"-?\d+", line.split("//")[0])
            if len(nums) == 6:
                rows.append(tuple(int(n) for n in nums))
        glyphs[mm.group(1)] = rows
    for mm in re.finditer(r"const GFXfont (\w+) PROGMEM = \{\s*\(uint8_t\s*\*\)(\w+),\s*"
                          r"\(GFXglyph\s*\*\)(\w+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(\d+)\s*\};", src):
        fonts.append(dict(name=mm.group(1), bitmaps=bitmaps[mm.group(2)], glyphs=glyphs[mm.group(3)],
                          first=int(mm.group(4), 16), last=int(mm.group(5), 16),
                          yadvance=int(mm.group(6)), bpp=bpp))
    if not fonts:
        sys.exit("khong doc duoc font nao trong %s" % path)
    return fonts, bpp


# ---------------------------------------------------------------------------
#  Mo phong driver lv_font_gfxfont.c
# ---------------------------------------------------------------------------
def driver_lookup(ranges, cp):
    """Tuong ung gfx_lookup() + gfx_get_glyph_dsc()."""
    for f in ranges:
        if not (f["first"] <= cp <= f["last"]):
            continue
        off, w, h, adv, xoff, yoff = f["glyphs"][cp - f["first"]]
        if w == 0 and h == 0 and adv == 0:
            continue                              # o trong
        return dict(bitmap=f["bitmaps"], off=off, w=w, h=h, adv=adv, xoff=xoff, yoff=yoff)


def driver_repack(bitmap, off, w, h):
    """Tuong ung gfx_repack(): Adafruit (moi hang canh byte) -> LVGL (chuoi bit lien tuc)."""
    stride = (w + 7) // 8
    nbytes = (w * h + 7) // 8
    dst = bytearray(nbytes)
    bit = 0
    for row in range(h):
        s = off + row * stride
        col = 0
        bi = 0
        while col < w:
            n = min(8, w - col)
            v = bitmap[s + bi]
            for k in range(n):
                if v & (0x80 >> k):
                    dst[bit >> 3] |= 0x80 >> (bit & 7)
                bit += 1
                col += 1
            bi += 1
    return bytes(dst)


def driver_bitmap(g, bpp):
    """Tuong ung gfx_get_glyph_bitmap(): bpp1 -> repack, bpp4 -> tra thang."""
    if bpp == 1:
        return driver_repack(g["bitmap"], g["off"], g["w"], g["h"])
    need = (g["w"] * g["h"] * bpp + 7) // 8
    return g["bitmap"][g["off"]:g["off"] + need]


def glyph_bytes(w, h, bpp):
    return (w * h * bpp + 7) // 8


def lvgl_level(buf, w, h, row, col, bpp):
    """Tuong ung vong lap doc bitmap cua lv_draw_sw_letter.c."""
    bit = row * w + col
    if bpp == 1:
        return (buf[bit >> 3] >> (7 - (bit & 7))) & 1
    # bpp 4: col chan -> nibble cao, col le -> nibble thap; cac hang LIEN TUC
    return (buf[bit >> 1] >> (4 if (bit & 1) == 0 else 0)) & 0x0F


def render_lvgl(ranges, text, line_height, base_line, bpp,
                canvas_w=420, canvas_h=40, baseline_y=26):
    """Ve 1 dong chu bang dung luat cua LVGL 8.4. Tra ve (canvas muc xam, n_ok, n_missing)."""
    canvas = [[0] * canvas_w for _ in range(canvas_h)]
    pos_x = 2
    pos_y = baseline_y - (line_height - base_line)     # mep tren cua dong
    ok = missing = 0
    for ch in text:
        cp = ord(ch)
        g = driver_lookup(ranges, cp)
        if g is None:
            missing += 1
            continue
        ok += 1
        if g["w"] == 0 or g["h"] == 0:
            pos_x += g["adv"]                          # space: chi nhay, khong ve
            continue
        buf = driver_bitmap(g, bpp)
        # ---- luat cua lv_draw_sw_letter ----
        gx = pos_x + g["xoff"]
        gy = pos_y + (line_height - base_line) - g["h"] - (-(g["yoff"] + g["h"]))
        for r in range(g["h"]):
            for c in range(g["w"]):
                v = lvgl_level(buf, g["w"], g["h"], r, c, bpp)
                if v:
                    yy, xx = gy + r, gx + c
                    if 0 <= yy < canvas_h and 0 <= xx < canvas_w:
                        canvas[yy][xx] = v
        pos_x += g["adv"]
    return canvas, ok, missing


def render_freetype(ttf, size, text, bpp, canvas_w=420, canvas_h=40, baseline_y=26):
    """Ban ve tham chieu: FreeType ve thang voi anchor 'ls' (baseline), luong tu nhu bo sinh."""
    img = Image.new("L", (canvas_w, canvas_h), 0)
    ImageDraw.Draw(img).text((2, baseline_y), text, font=ImageFont.truetype(ttf, size),
                             fill=255, anchor="ls")
    px = img.load()
    if bpp == 1:
        return [[1 if px[x, y] >= 128 else 0 for x in range(canvas_w)] for y in range(canvas_h)]
    return [[(px[x, y] * 15 + 127) // 255 for x in range(canvas_w)] for y in range(canvas_h)]


def art(canvas, bpp):
    shades = " .:-=+*#%@"
    top = 9 if bpp == 1 else 15
    return "\n".join("   |" + "".join(shades[min(9, v * 9 // top)] for v in row) + "|"
                     for row in canvas)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--header", required=True)
    ap.add_argument("--font", required=True)
    ap.add_argument("--size", type=int, required=True)
    ap.add_argument("--text", action="append", default=[])
    ap.add_argument("--cps", action="append", default=[],
                    help="chuoi bang ma codepoint, vd 0x4E,0x48,0x1EC6,0x54 (tranh loi ma hoa "
                         "console Windows khi go chu co dau truc tiep)")
    ap.add_argument("--art", action="store_true", help="in ca hinh ASCII")
    ap.add_argument("--tol", type=float, default=0.02,
                    help="bpp1: ty le pixel lech toi da. bpp4: KHONG dung (xem --tol-levels)")
    ap.add_argument("--tol-levels", type=int, default=1,
                    help="bpp4: sai so MUC toi da cho phep so voi FreeType (mac dinh 1 muc 4-bit)")
    ap.add_argument("--selftest-expect", action="store_true",
                    help="in ra DUNG nhung dong ma lv_font_gfxfont_selftest() se in tren COM7, "
                         "de doi chieu sau khi nap")
    args = ap.parse_args()

    ranges, bpp = parse_header(args.header)
    texts = list(args.text)
    for spec in args.cps:
        texts.append("".join(chr(int(v, 0)) for v in spec.split(",") if v.strip()))
    if not texts:
        texts = ["NHIET DO", "Chon file", "SAVE EEPROM", "BED MESH 100%"]

    print("header   :", args.header)
    print("bpp      : %d  (%s)" % (bpp, {
        1: "1 bit, Adafruit canh hang -> driver REPACK",
        4: "16 muc xam, chuoi bit lien tuc kieu LVGL -> driver tra con tro THANG"}[bpp]))
    print("cac dai  :", ", ".join("%s U+%04X..U+%04X yAdvance=%d" %
                                  (f["name"].split("_")[-1], f["first"], f["last"], f["yadvance"])
                                  for f in ranges))
    # ascender/descender thuc = dung cong thuc cua driver (quet CA cac dai)
    asc = desc = 0
    for f in ranges:
        for off, w, h, adv, xoff, yoff in f["glyphs"]:
            if w == 0 or h == 0:
                continue
            asc = max(asc, -yoff)
            desc = max(desc, yoff + h)
    line_height = ranges[0]["yadvance"]
    base_line = line_height - asc
    print("driver   : ascender=%d descender=%d  line_height=%d base_line=%d "
          "(baseline o %d tu mep tren dong)" % (asc, desc, line_height, base_line,
                                                line_height - base_line))

    fails = 0
    if args.selftest_expect:
        maxb = 0
        for f in ranges:
            for off, w, h, adv, xoff, yoff in f["glyphs"]:
                if w and h:
                    maxb = max(maxb, glyph_bytes(w, h, bpp))
        print("\n--- KY VONG LOG COM7 cua lv_font_gfxfont_selftest() (doi chieu sau khi nap) ---")
        print("[GFXFONT] selftest --------------------------------------------------")
        print("[GFXFONT] line_height=%d base_line=%d (ascender thuc=%d) | bpp=%d (%s)"
              % (line_height, base_line, line_height - base_line, bpp,
                 "16 muc xam, bitmap da kieu LVGL -> tra con tro THANG" if bpp == 4 else
                 "1 bit, bitmap Adafruit canh hang -> REPACK"))
        print("[GFXFONT] so dai=%u  glyph to nhat=%u byte  buffer repack=%u byte"
              % (len(ranges), maxb, 0 if bpp == 4 else max(128, maxb)))
        for i, f in enumerate(ranges):
            print("[GFXFONT]   dai %u: U+%04X..U+%04X  yAdvance=%u"
                  % (i, f["first"], f["last"], f["yadvance"]))
        names = ["'A'", "'0'", "'a'", "U+1EC7", "U+0110"]
        for nm, cp in zip(names, (0x41, 0x30, 0x61, 0x1EC7, 0x0110)):
            g = driver_lookup(ranges, cp)
            if g is None:
                print("[GFXFONT] %-7s -> KHONG TRA DUOC GLYPH (driver se ve o vuong)" % nm)
                continue
            if g["w"] == 0 or g["h"] == 0:
                print("[GFXFONT] %-7s -> dsc OK nhung BITMAP NULL" % nm)
                continue
            buf = driver_bitmap(g, bpp)
            need = glyph_bytes(g["w"], g["h"], bpp)
            n8 = min(8, need)
            csum = sum((i + 1) * buf[i] for i in range(n8))
            ofs_y = -(g["yoff"] + g["h"])
            print("[GFXFONT] %-7s -> OK  box=%dx%d adv_w=%d ofs=(%d,%d) bpp=%d bytes=%d | "
                  "checksum8=%d first8=%s"
                  % (nm, g["w"], g["h"], g["adv"], g["xoff"], ofs_y, bpp, need, csum,
                     " ".join("%02X" % b for b in buf[:n8])))
        print("[GFXFONT] -----------------------------------------------------------")

    for text in texts:
        lv, ok, missing = render_lvgl(ranges, text, line_height, base_line, bpp)
        ft = render_freetype(args.font, args.size, text, bpp)
        diffs = [abs(lv[y][x] - ft[y][x])
                 for y in range(len(lv)) for x in range(len(lv[0]))]
        maxd = max(diffs) if diffs else 0
        npix = sum(1 for d in diffs if d)
        ref = sum(sum(r) for r in ft)
        if bpp == 1:
            bad = (maxd != 0) or (npix / max(1, ref) > args.tol)
            detail = "pixel lech %d / %d" % (npix, ref)
        else:
            bad = (maxd > args.tol_levels) or missing
            detail = "pixel lech %d, sai so MUC toi da %d (cho phep %d)" % (npix, maxd, args.tol_levels)
        if missing:
            bad = True
        status = "FAIL" if bad else "OK "
        if bad:
            fails += 1
        print("\n[%s] %-14s glyph=%d thieu=%d | %s" % (status, '"%s"' % text, ok, missing, detail))
        if args.art or bad:
            print("   --- LVGL (driver cua ta) ---")
            print(art(lv, bpp))
            print("   --- FreeType (tham chieu) ---")
            print(art(ft, bpp))

    print("\nKET LUAN:", "TAT CA OK" if fails == 0 else "%d dong FAIL" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
