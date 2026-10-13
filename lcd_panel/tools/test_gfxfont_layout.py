#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
test_gfxfont_layout.py -- kiem tra OFFLINE (khong can phan cung) toan bo chuoi:

    header GFXfont  ->  anh xa cua lv_font_gfxfont.c  ->  luat ve cua LVGL 8.4

Muc dich: xac nhan ban do glyph (dac biet la ofs_y / base_line / adv_w va viec REPACK
bitmap tu kieu Adafruit "canh hang" sang kieu LVGL "lien tuc") la DUNG, truoc khi nap.

No mo phong dung 3 thu:
  1) get_glyph_dsc  : ofs_y = -(yOffset + height) ; adv_w = xAdvance (PIXEL NGUYEN)
  2) get_glyph_bitmap: repack hang-canh-byte -> chuoi bit lien tuc
  3) lv_draw_sw_letter (LVGL 8.4):
        gpos.x = pos.x + ofs_x
        gpos.y = pos.y + (line_height - base_line) - box_h - ofs_y
        va doc bitmap theo chitiet: bit = row*box_w + col  (lien tuc)
     roi doi chieu voi ban ve truc tiep bang FreeType (Pillow) cung co chu.

Dung:
  python tools/test_gfxfont_layout.py --header src/gfx_font_robotomono.h \
      --font D:/fonts/gfxfont/google/RobotoMono-Regular.ttf --size 13 \
      --text "NHIET DO 200/60" --text "Chon file" --art
"""

import argparse
import re
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("CAN PILOW: python -m pip install pillow")


# ---------------------------------------------------------------------------
#  Doc header GFXfont
# ---------------------------------------------------------------------------
def parse_header(path):
    src = open(path, "r", encoding="utf-8").read()
    bitmaps, glyphs, fonts = {}, {}, []

    for m in re.finditer(r"static const uint8_t (\w+)\[\] PROGMEM = \{(.*?)\n\};", src, re.S):
        bitmaps[m.group(1)] = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(2)))
    for m in re.finditer(r"static const GFXglyph (\w+)\[\] PROGMEM = \{(.*?)\n\};", src, re.S):
        rows = []
        for line in m.group(2).splitlines():
            nums = re.findall(r"-?\d+", line.split("//")[0])
            if len(nums) == 6:
                rows.append(tuple(int(n) for n in nums))
        glyphs[m.group(1)] = rows
    for m in re.finditer(r"const GFXfont (\w+) PROGMEM = \{\s*\(uint8_t\s*\*\)(\w+),\s*"
                         r"\(GFXglyph\s*\*\)(\w+),\s*(0x[0-9A-Fa-f]+),\s*(0x[0-9A-Fa-f]+),\s*(\d+)\s*\};", src):
        fonts.append(dict(name=m.group(1), bitmaps=bitmaps[m.group(2)], glyphs=glyphs[m.group(3)],
                          first=int(m.group(4), 16), last=int(m.group(5), 16),
                          yadvance=int(m.group(6))))
    if not fonts:
        sys.exit("khong doc duoc GFXfont nao trong %s" % path)
    return fonts


# ---------------------------------------------------------------------------
#  Mo phong driver lv_font_gfxfont.c
# ---------------------------------------------------------------------------
def driver_lookup(ranges, cp):
    """Tuong ung gfx_lookup() + gfx_get_glyph_dsc()."""
    for f in ranges:
        if not (f["first"] <= cp <= f["last"]):
            continue
        g = f["glyphs"][cp - f["first"]]
        off, w, h, adv, xoff, yoff = g
        if w == 0 and h == 0 and adv == 0:
            continue                              # o trong
        return dict(bitmap=f["bitmaps"], off=off, w=w, h=h, adv=adv, xoff=xoff, yoff=yoff)
    return None


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


def lvgl_pixel(buf, w, h, row, col):
    """Tuong ung vong lap doc bitmap cua lv_draw_sw_letter.c (bit = row*w + col, bit cao truoc)."""
    bit = row * w + col
    return (buf[bit >> 3] >> (7 - (bit & 7))) & 1


def render_lvgl(ranges, text, line_height, base_line, canvas_w=420, canvas_h=40, baseline_y=26):
    """Ve 1 dong chu bang dung luat cua LVGL 8.4. Tra ve (canvas, n_ok, n_missing)."""
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
        buf = driver_repack(g["bitmap"], g["off"], g["w"], g["h"])
        # ---- luat cua lv_draw_sw_letter ----
        gx = pos_x + g["xoff"]
        gy = pos_y + (line_height - base_line) - g["h"] - (-(g["yoff"] + g["h"]))
        for r in range(g["h"]):
            for c in range(g["w"]):
                if lvgl_pixel(buf, g["w"], g["h"], r, c):
                    yy, xx = gy + r, gx + c
                    if 0 <= yy < canvas_h and 0 <= xx < canvas_w:
                        canvas[yy][xx] = 1
        pos_x += g["adv"]
    return canvas, ok, missing


def render_freetype(ttf, size, text, canvas_w=420, canvas_h=40, baseline_y=26):
    """Ban ve tham chieu: FreeType ve thang voi anchor 'ls' (baseline)."""
    img = Image.new("L", (canvas_w, canvas_h), 0)
    ImageDraw.Draw(img).text((2, baseline_y), text, font=ImageFont.truetype(ttf, size),
                             fill=255, anchor="ls")
    px = img.load()
    return [[1 if px[x, y] >= 128 else 0 for x in range(canvas_w)] for y in range(canvas_h)]


def art(canvas, rows=None):
    return "\n".join("   |" + "".join("#" if v else "." for v in row) + "|"
                     for row in (canvas if rows is None else canvas[:rows]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--header", required=True)
    ap.add_argument("--font", required=True)
    ap.add_argument("--size", type=int, required=True)
    ap.add_argument("--text", action="append", default=[])
    ap.add_argument("--cps", action="append", default=[],
                    help="chuoi bang ma codepoint, vd 0x4E,0x48,0x1EC6,0x54 (tranh loi ma hoa "
                         "console Windows khi go chu co dau truc tiep)")
    ap.add_argument("--art", action="store_true", help="in ca hinh ASCII (dai)")
    ap.add_argument("--tol", type=float, default=0.02, help="ty le pixel lech toi da cho phep")
    ap.add_argument("--selftest-expect", action="store_true",
                    help="in ra DUNG nhung dong ma lv_font_gfxfont_selftest() se in tren COM7, "
                         "de doi chieu sau khi nap")
    args = ap.parse_args()

    ranges = parse_header(args.header)
    texts = list(args.text)
    for spec in args.cps:
        texts.append("".join(chr(int(v, 0)) for v in spec.split(",") if v.strip()))
    if not texts:
        texts = ["NHIET DO", "Chon file", "SAVE EEPROM", "BED MESH 100%"]

    print("header   :", args.header)
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
                    maxb = max(maxb, (w * h + 7) // 8)
        print("\n--- KY VONG LOG COM7 cua lv_font_gfxfont_selftest() (doi chieu sau khi nap) ---")
        print("[GFXFONT] selftest --------------------------------------------------")
        print("[GFXFONT] line_height=%d base_line=%d (ascender thuc=%d) vi font 1bpp"
              % (line_height, base_line, line_height - base_line))
        print("[GFXFONT] so dai=%u  do dai glyph to nhat=%u byte  buffer repack=%u byte"
              % (len(ranges), maxb, max(128, maxb)))
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
            buf = driver_repack(g["bitmap"], g["off"], g["w"], g["h"])
            need = (g["w"] * g["h"] + 7) // 8
            n8 = min(8, need)
            csum = sum((i + 1) * buf[i] for i in range(n8))
            ofs_y = -(g["yoff"] + g["h"])
            print("[GFXFONT] %-7s -> OK  box=%dx%d adv_w=%d ofs=(%d,%d) bpp=1 linear=%d byte | "
                  "checksum8=%d first8=%s"
                  % (nm, g["w"], g["h"], g["adv"], g["xoff"], ofs_y, need, csum,
                     " ".join("%02X" % b for b in buf[:n8])))
        print("[GFXFONT] -----------------------------------------------------------")

    for text in texts:
        lv, ok, missing = render_lvgl(ranges, text, line_height, base_line)
        ft = render_freetype(args.font, args.size, text)
        diff = sum(1 for y in range(len(lv)) for x in range(len(lv[0])) if lv[y][x] != ft[y][x])
        ref = sum(sum(r) for r in ft)
        ratio = diff / max(1, ref)
        status = "OK " if ratio <= args.tol and missing == 0 else "FAIL"
        if status == "FAIL":
            fails += 1
        print("\n[%s] %-14s glyph=%d thieu=%d | pixel lech %d / %d (%.1f%%)"
              % (status, '"%s"' % text, ok, missing, diff, ref, 100.0 * ratio))
        if args.art or status == "FAIL":
            print("   --- LVGL (driver cua ta) ---")
            print(art(lv))
            print("   --- FreeType (tham chieu) ---")
            print(art(ft))

    print("\nKET LUAN:", "TAT CA OK" if fails == 0 else "%d dong FAIL" % fails)
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
