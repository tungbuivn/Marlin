"""Definitive probe: LVGL glyph bitmap row layout, using glyph_dsc bitmap_index deltas.

KET QUA (chay tren chinh font cua LVGL 8.4 di kem):
    lv_font_montserrat_14.c : 67 CONTINUOUS / 0 row-aligned / 88 ambiguous
    lv_font_dejavu_16_*.c   : 253 CONTINUOUS / 0 row-aligned / 341 ambiguous
=> LVGL 8.4 xep cac hang glyph thanh MOT CHUOI BIT LIEN TUC, KHONG dem padding theo hang.
   Adafruit GFXfont thi CANH moi hang dau byte => KHONG the tra thang con tro bitmap cho LVGL.
   (Xem tools/probe_lvgl_glyph_render.py de co bang chung bang hinh; xem chuc nang repack
    trong src/lv_font_gfxfont.c.)

For consecutive glyphs in glyph_dsc order, the number of bytes a glyph occupies is
    next.bitmap_index - cur.bitmap_index   (the last glyph uses the array length).
Compare that against:
  (a) ceil(w*h*bpp/8)      -> rows packed CONTINUOUSLY (no per-row padding)
  (b) ceil(w*bpp/8) * h    -> every row padded to a byte boundary (Adafruit GFXfont)
"""
import re
import sys
import collections

C = sys.argv[1]
src = open(C, "r", encoding="utf-8", errors="replace").read()

bpp = int(re.search(r"\.bpp\s*=\s*(\d+)", src).group(1))
blk = re.search(r"glyph_dsc\[\]\s*=\s*\{(.*?)\n\};", src, re.S).group(1)
ent = []
for m in re.finditer(r"\{([^}]*)\}", blk):
    g = {}
    for k, v in re.findall(r"\.(\w+)\s*=\s*(-?\d+)", m.group(1)):
        g[k] = int(v)
    ent.append(g)

bblk = re.search(r"glyph_bitmap\[\]\s*=\s*\{(.*?)\n\};", src, re.S).group(1)
nbytes_total = len(re.findall(r"0[xX][0-9A-Fa-f]+", bblk))

print("file        :", C)
print("bpp         :", bpp)
print("line_height :", re.search(r"\.line_height\s*=\s*(\d+)", src).group(1))
print("base_line   :", re.search(r"\.base_line\s*=\s*(\d+)", src).group(1))
print("glyph_dsc   :", len(ent), " bitmap bytes:", nbytes_total)

stat = collections.Counter()
bad = []
for i in range(len(ent) - 1):
    a, b = ent[i], ent[i + 1]
    got = b.get("bitmap_index", 0) - a.get("bitmap_index", 0)
    w, h = a.get("box_w", 0), a.get("box_h", 0)
    if w == 0 or h == 0:
        stat["empty(0 bytes)" if got == 0 else "empty(!!%d bytes)" % got] += 1
        continue
    cont = -(-(w * h * bpp) // 8)
    rowal = -(-(w * bpp) // 8) * h
    if got == cont == rowal:
        stat["ambiguous(same)"] += 1
    elif got == cont:
        stat["CONTINUOUS"] += 1
    elif got == rowal:
        stat["ROW-ALIGNED"] += 1
    else:
        stat["NEITHER"] += 1
        bad.append((i, w, h, got, cont, rowal))

print("\nverdict:", dict(stat))
if bad:
    print("NEITHER examples (idx w h got continuous rowaligned):")
    for r in bad[:12]:
        print("   ", r)
