#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tim codepoint THAT cua cac glyph icon trong font (KHONG doan).

KET QUA voi MaterialIcons-Regular.ttf (Version 1.017, unitsPerEm 512):
  * Trong font nay TEN ICON CHI LA LIGATURE (GSUB LookupType 4), khong co glyph ten nghia:
    vd ligature "wifi" -> glyph `uniE63E` -> codepoint U+E63E.
  * Ligature cua tool dung 'underscore' thay cho '_': "wifi_off" -> "wifiunderscoreoff".
  * Cac codepoint dung cho gfx4_font_icons.h:
        wifi                    U+E63E
        wifi_off                U+E648
        network_wifi_three_bar  U+EBE1
        network_wifi_one_bar    U+EBE4
    => U+E000 KHONG co glyph trong font nguon, phai dung `--map 0xE000=0xE63E` cua
       tools/ttf2gfxfont.py.

Cach lam:
  1) getGlyphOrder() -> ten glyph (font nay la `uniXXXX`, khong mang nghia)
  2) getBestCmap() -> {codepoint: ten glyph}; dao nguoc de biet glyph ung voi codepoint nao
  3) doc GSUB LookupType 4 de liet ke LIGATURE (nguon su that cho ten icon)
"""
import sys
from fontTools.ttLib import TTFont


def load_ligatures(tt):
    """{ten_lowercase_khong_gach: glyph_name} tu bang GSUB LookupType 4."""
    ligs = {}
    if "GSUB" not in tt:
        return ligs
    for lk in tt["GSUB"].table.LookupList.Lookup:
        if lk.LookupType != 4:
            continue
        for st in lk.SubTable:
            if not hasattr(st, "ligatures"):
                continue
            for first, arr in st.ligatures.items():
                for lig in arr:
                    comps = lig.Component
                    if not isinstance(comps, (list, tuple)):
                        comps = [comps]
                    ligs[first + "".join(comps)] = lig.LigGlyph
    return ligs


def lig_word(word):
    """Ten ligature trong font nay dung TEN GLYPH cua tung ky tu, ma ky tu '_' co ten glyph la
    'underscore' -> phai doi lai moi so sanh duoc voi ten icon that ('wifi_off')."""
    return word.replace("underscore", "_")


def main():
    if len(sys.argv) < 2:
        sys.exit("dung: probe_icon_codepoints.py <font.ttf> [ten_icon ...]")
    path = sys.argv[1]
    want = sys.argv[2:] or ["wifi"]

    tt = TTFont(path, fontNumber=0)
    print("font            :", path)
    for rec in tt["name"].names:
        if rec.nameID in (1, 4, 5):
            try:
                print("  nameID %d = %s" % (rec.nameID, rec.toUnicode()))
            except Exception:
                pass
    print("head.unitsPerEm :", tt["head"].unitsPerEm)
    print("so glyph        :", len(tt.getGlyphOrder()))

    cmap = tt.getBestCmap()
    print("so codepoint    :", len(cmap))
    pua = sorted(c for c in cmap if c >= 0xE000)
    if pua:
        print("vung PUA        : %d codepoint (U+%04X..U+%04X)" % (len(pua), pua[0], pua[-1]))

    name2cp = {}
    for cp, gname in cmap.items():
        name2cp.setdefault(gname, []).append(cp)

    ligs = load_ligatures(tt)
    print("so ligature     :", len(ligs))

    for w in want:
        key = w.lower()
        print("\n=== %r ===" % w)
        found = False
        for word, gname in sorted(ligs.items()):
            disp = lig_word(word)
            if key in disp.lower():
                cps = name2cp.get(gname, [])
                print("  icon %-34s -> %-10s %s"
                      % (disp, gname, " ".join("U+%04X" % c for c in cps) or "(khong co cmap)"))
                found = True
        if not found:
            print("  (khong tim thay ligature nao chua %r)" % w)


if __name__ == "__main__":
    main()
