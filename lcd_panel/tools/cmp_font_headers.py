#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""cmp_font_headers.py -- so sanh DU LIEU 2 header font do ttf2gfxfont.py sinh ra.

Dung khi MO RONG mot font da co (them glyph / noi dai): phai chung minh cac glyph CU
KHONG DOI mot byte nao, neu khong bo cuc UI co the xe dich.

So 2 thu (bo qua comment):
  1) bang glyph: cac dong dau (glyph cu) phai giong het
  2) mang bitmap: doan byte cua glyph cu phai giong het (glyph duoc phat theo thu tu
     codepoint tang dan nen glyph cu luon nam o DAU mang)
Ngoai ra in ro yAdvance / base_line truoc & sau.

Dung:
  python tools/cmp_font_headers.py <header_cu.h> <header_moi.h> [so_glyph_cu]
  (mac dinh so_glyph_cu = so glyph cua header cu)
"""
import re
import sys


def parse(path):
    src = open(path, "r", encoding="utf-8").read()
    bm = re.search(r"static const uint8_t \w+Bitmaps\[\] PROGMEM = \{(.*?)\n\};", src, re.S).group(1)
    bitmap = bytes(int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]{2})", bm))
    gl = re.search(r"static const GFXglyph \w+Glyphs\[\] PROGMEM = \{(.*?)\n\};", src, re.S).group(1)
    glyphs = []
    for line in gl.splitlines():
        nums = re.findall(r"-?\d+", line.split("//")[0])
        if len(nums) == 6:
            glyphs.append(tuple(int(n) for n in nums))
    font = re.search(r"const GFXfont \w+ PROGMEM = \{\s*\(uint8_t\s*\*\)\w+,\s*"
                     r"\(GFXglyph\s*\*\)\w+,\s*0x([0-9A-Fa-f]+),\s*0x([0-9A-Fa-f]+),\s*(\d+)\s*\};", src)
    if not font:
        sys.exit("khong doc duoc GFXfont trong %s" % path)
    return dict(bitmap=bitmap, glyphs=glyphs, first=int(font.group(1), 16),
                last=int(font.group(2), 16), yadv=int(font.group(3)))


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    o, n = parse(sys.argv[1]), parse(sys.argv[2])
    nkeep = int(sys.argv[3]) if len(sys.argv) > 3 else len(o["glyphs"])

    print("CU : U+%04X..U+%04X  yAdvance=%d  %d glyph  %d byte bitmap"
          % (o["first"], o["last"], o["yadv"], len(o["glyphs"]), len(o["bitmap"])))
    print("MOI: U+%04X..U+%04X  yAdvance=%d  %d glyph  %d byte bitmap"
          % (n["first"], n["last"], n["yadv"], len(n["glyphs"]), len(n["bitmap"])))

    ok = True
    print("\n--- %d glyph CU (bitmapOffset, w, h, adv, xoff, yoff) ---" % nkeep)
    for i in range(nkeep):
        same = o["glyphs"][i] == n["glyphs"][i]
        ok &= same
        print("  U+%04X cu=%-24s moi=%-24s %s" % (0xE000 + i, o["glyphs"][i], n["glyphs"][i],
                                                 "GIONG" if same else "*** KHAC ***"))

    nb = len(o["bitmap"])
    same_bm = o["bitmap"] == n["bitmap"][:nb]
    ok &= same_bm
    print("\n--- mang bitmap: %d byte dau ---" % nb)
    print("  %s" % ("GIONG HET BYTE" if same_bm else "*** KHAC ***"))
    if not same_bm:
        for i in range(nb):
            if o["bitmap"][i] != n["bitmap"][i]:
                print("    lech tai byte %d: cu=0x%02X moi=0x%02X"
                      % (i, o["bitmap"][i], n["bitmap"][i]))
                break

    if len(n["glyphs"]) > nkeep:
        print("\n--- glyph MOI ---")
        for i in range(nkeep, len(n["glyphs"])):
            print("  U+%04X %s" % (0xE000 + i, n["glyphs"][i]))

    print("\nKET LUAN:", "%d GLYPH CU KHONG DOI (byte-for-byte)" % nkeep if ok
          else "*** CO THAY DOI ***")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
