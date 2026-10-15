#!/usr/bin/env python3
"""
check_lang.py -- kiem tra tinh DAY DU cua cac bang ngon ngu (da ngon ngu kieu Marlin).

VI SAO CO: chuoi cua man hinh CYD nam trong 2 bang `const` (ui_lang_vi.h / ui_lang_en.h) tro theo ID
trong `ui_lang.h`. Thieu 1 dong o 1 bang thi luc chay se roi ve tieng Viet (khong vo man hinh) nhung
nguoi dung se thay "lan lon ngon ngu" ma khong biet vi sao -> script nay bat loi NGAY tren PC.

Cach dung (chay tu thu muc `cydlcd/`):
    python tools/check_lang.py             # chi kiem tra 2 bang (dung trong CI / truoc khi commit)
    python tools/check_lang.py --hardcoded # + liet ke chuoi HIEN THI con HARD-CODE (chua chuyen sang T())

Ma nguon la ASCII thuan (chu co dau viet bang \\x..) nen doc file bang latin-1 cung duoc; o day doc
UTF-8 voi errors='replace' de khong bao loi khi gap byte la.
"""
import argparse
import os
import re
import sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src')

RE_ENUM_ID = re.compile(r'^\s*(UI_T_[A-Z0-9_]+)\s*(?:=\s*\d+)?\s*,', re.M)
RE_TABLE = re.compile(r'^\s*\[(UI_T_[A-Z0-9_]+)\]\s*=\s*("(?:[^"\\]|\\.)*")\s*,?', re.M)


def read(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


def parse_enum(path):
    """Tra ve danh sach ID theo THU TU khai bao (bo UI_T_NONE va UI_T_COUNT)."""
    txt = read(path)
    # Chi lay phan `typedef enum { ... } ui_text_t;`
    m = re.search(r'typedef\s+enum\s*\{(.*?)\}\s*ui_text_t\s*;', txt, re.S)
    if not m:
        sys.exit('KHONG tim thay `typedef enum { ... } ui_text_t;` trong %s' % path)
    ids = RE_ENUM_ID.findall(m.group(1))
    return [i for i in ids if i not in ('UI_T_NONE', 'UI_T_COUNT')]


def parse_table(path):
    """Tra ve dict id -> chuoi (nguyen van, ke ca dau ngoac kep)."""
    out = {}
    dup = []
    for m in RE_TABLE.finditer(read(path)):
        k, v = m.group(1), m.group(2)
        if k in out:
            dup.append(k)
        out[k] = v
    return out, dup


def text_preview(lit, n=40):
    s = lit[1:-1] if len(lit) >= 2 else lit
    s = s.replace('\\x', '~')          # byte UTF-8 -> khong in ra byte la
    return s if len(s) <= n else s[:n] + '...'


def scan_hardcoded():
    """Dem chuoi HIEN THI con hard-code trong cac file giao dien (uoc luong tien do chuyen doi).

    Heuristic: literal Dai >= 4 ky tu, co chu cai, va KHONG phai dinh dang/log/protocol:
      bo qua: bat dau bang '%', chua '\\n' (log), '[UI]'/'[WIFI]'..., 'M<so>' (G-code),
              chuoi trong ngoac vuong/khoa NVS, va cac chuoi KY HIEU ngan ("II","[]","ON","OFF"...).
    Day chi la SO DO TIEN DO - khong phai loi.
    """
    files = sorted(f for f in os.listdir(SRC)
                   if (f.startswith('ui_page_') or f in ('ui_chrome.cpp', 'ui_widgets.cpp',
                                                         'ui_boxes.cpp', 'ui_overlay.cpp',
                                                         'ui_lvgl.cpp'))
                   and f.endswith(('.cpp', '.h')))
    lit_re = re.compile(r'"((?:[^"\\]|\\.)*)"')
    total = 0
    per = []
    for f in files:
        n = 0
        for m in lit_re.finditer(read(os.path.join(SRC, f))):
            s = m.group(1)
            if len(s) < 4 or not re.search(r'[A-Za-z]', s):
                continue
            if s.startswith('%') or '\\n' in s:
                continue
            if s.startswith('[') or s.startswith('M') and re.match(r'^M\d+', s):
                continue
            if s in ('UI BOX', 'UI HIT', 'UI CFG', 'UI BACK', 'UI NEXT', 'UI PREV'):
                continue
            n += 1
        if n:
            per.append((f, n))
            total += n
    per.sort(key=lambda t: -t[1])
    print('\n-- Chuoi HIEN THI con hard-code (uoc luong tien do B2) --')
    for f, n in per:
        print('   %-26s %3d' % (f, n))
    print('   TONG: %d' % total)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--hardcoded', action='store_true', help='liet ke chuoi hien thi con hard-code')
    args = ap.parse_args()

    ids = parse_enum(os.path.join(SRC, 'ui_lang.h'))
    vi, vi_dup = parse_table(os.path.join(SRC, 'ui_lang_vi.h'))
    en, en_dup = parse_table(os.path.join(SRC, 'ui_lang_en.h'))
    id_set = set(ids)

    print('ui_lang.h       : %d ID (UI_T_NONE + UI_T_COUNT la moc, khong tinh)' % len(ids))
    print('ui_lang_vi.h    : %d dong' % len(vi))
    print('ui_lang_en.h    : %d dong' % len(en))

    bad = False

    def check(name, tbl, dup):
        nonlocal bad
        missing = [i for i in ids if i not in tbl]
        extra = [k for k in tbl if k not in id_set]
        empty = [k for k, v in tbl.items() if v in ('""',)]
        if dup:
            print('  [LOI] %s: ID khai bao 2 lan: %s' % (name, ', '.join(sorted(set(dup)))))
            bad = True
        if missing:
            print('  [LOI] %s: THIEU %d dong:' % (name, len(missing)))
            for i in missing:
                print('       %s' % i)
            bad = True
        if extra:
            print('  [LOI] %s: co ID KHONG co trong enum: %s' % (name, ', '.join(sorted(extra))))
            bad = True
        if empty:
            print('  [LOI] %s: chuoi RONG: %s' % (name, ', '.join(sorted(empty))))
            bad = True
        if not (missing or extra or empty or dup):
            print('  [OK]  %s: day du' % name)

    check('VI', vi, vi_dup)
    check('EN', en, en_dup)

    # Thong ke ban dich GIONG NHAU (khong phai loi - nhieu chuoi co tinh khong dich, vd "English")
    same = [i for i in ids if i in vi and i in en and vi[i] == en[i]]
    print('  [i]  %d/%d chuoi GIONG nhau o 2 bang (chuoi co tinh khong dich: ten rieng, ky hieu...).'
          % (len(same), len(ids)))

    if args.hardcoded:
        scan_hardcoded()

    print('\n==> %s' % ('CO LOI - xem danh sach [LOI] o tren' if bad else 'DAT: 2 bang ngon ngu day du'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
