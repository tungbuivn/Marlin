"""Render glyphs of a LVGL lv_font_conv .c file as ASCII art under BOTH packing rules.

Packing rule A ("continuous"): glyph occupies ceil(w*h*bpp/8) bytes, rows are a
continuous bit stream with no per-row padding.  (lv_draw_sw_letter.c line 337:
after each row `col_bit += col_bit_row_ofs`, which is 0 for a full-width row.)

Packing rule B ("row-aligned"): every row padded to a byte boundary, i.e.
ceil(w*bpp/8)*h bytes -- the Adafruit GFXfont layout.

Whichever produces a recognisable letter is the rule LVGL really uses.

KET QUA (lv_font_montserrat_14.c, cac ky tu H 8 g !): chi co quy tac A cho ra chu doc duoc;
quy tac B (canh hang, tuc dinh dang Adafruit GFXfont) cho ra RAC. Ket luan: LVGL 8.4 dung
chuoi bit LIEN TUC. Chay lai:
    python tools/probe_lvgl_glyph_render.py <lvgl>/src/font/lv_font_montserrat_14.c 0x48 0x38 0x67 0x21
"""
import re
import sys

C = sys.argv[1]
want = [int(x, 16) for x in sys.argv[2:]] or [0x48, 0x38, 0x67]  # H, 8, g

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
# split bitmap block into (codepoint, bytes) segments; marker has no '*' issue
parts = re.split(r"/\*\s*U\+([0-9A-Fa-f]{4,6})\b", bblk)
segs = []
for i in range(1, len(parts), 2):
    cp = int(parts[i], 16)
    body = parts[i + 1]
    segs.append((cp, bytes(int(v, 16) for v in re.findall(r"0[xX]([0-9A-Fa-f]+)", body))))
print("bpp", bpp, "dsc entries", len(ent), "bitmap segs", len(segs))


def render(data, w, h, rule):
    out = []
    for r in range(h):
        line = []
        for c in range(w):
            if rule == "A":
                bit = (r * w + c) * bpp
            else:
                bit = r * (-(-(w * bpp) // 8)) * 8 + c * bpp
            byte_i = bit >> 3
            shift = 8 - bpp - (bit & 7)
            v = (data[byte_i] >> shift) & ((1 << bpp) - 1) if byte_i < len(data) else 0
            line.append(" .:-=+*#%@"[min(9, v * 9 // ((1 << bpp) - 1))])
        out.append("".join(line))
    return out


for cp in want:
    # bitmap segment for this codepoint
    seg = next((s for s in segs if s[0] == cp), None)
    # dsc entry: comments have no reserved slot, dsc has one -> index offset 1
    idx = [i for i, s in enumerate(segs) if s[0] == cp]
    if seg is None or not idx:
        print("U+%04X: not found" % cp)
        continue
    d = ent[idx[0] + 1]
    w, h = d["box_w"], d["box_h"]
    data = seg[1]
    print("\n=== U+%04X  box %dx%d  bytes=%d  (A needs %d, B needs %d)  ofs_x=%d ofs_y=%d ==="
          % (cp, w, h, len(data), -(-(w * h * bpp) // 8), -(-(w * bpp) // 8) * h,
             d["ofs_x"], d["ofs_y"]))
    a = render(data, w, h, "A")
    b = render(data, w, h, "B")
    print("  rule A (continuous)      |  rule B (row-aligned/GFX)")
    for i in range(h):
        print("   %-*s |  %s" % (w, a[i], b[i]))
