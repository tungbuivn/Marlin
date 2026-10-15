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
# Bang GHI DE (EN): danh sach cap `{ UI_T_..., "chuoi" },`
RE_OVR = re.compile(r'^\s*\{\s*(UI_T_[A-Z0-9_]+)\s*,\s*("(?:[^"\\]|\\.)*")\s*\}\s*,?', re.M)


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
    """Bang GOC (VI): `[ID] = "chuoi"`. Tra ve (dict, ID trung, thu tu ID)."""
    out = {}
    order = []
    dup = []
    for m in RE_TABLE.finditer(read(path)):
        k, v = m.group(1), m.group(2)
        if k in out:
            dup.append(k)
        out[k] = v
        order.append(k)
    return out, dup, order


def parse_ovr(path):
    """Bang GHI DE (EN): danh sach cap `{ ID, "chuoi" }`. Tra ve (dict, ID trung, thu tu ID)."""
    out = {}
    order = []
    dup = []
    for m in RE_OVR.finditer(read(path)):
        k, v = m.group(1), m.group(2)
        if k in out:
            dup.append(k)
        out[k] = v
        order.append(k)
    return out, dup, order


# ---------------------------------------------------------------------------
#  AUDIT: con chuoi HIEN THI nao bang TIENG VIET chua chuyen sang T() khong?
# ---------------------------------------------------------------------------
# Quy tac (chinh xac hon `--hardcoded`, dung lam BANG CHUNG hoan thanh B2):
#   1) bo comment /* */ va //;
#   2) tach thanh TUNG CAU LENH (den dau ';');
#   3) cau lenh nao la LOG (Serial./WIFILOG/HLOG/...) -> BO QUA (co tinh khong dich);
#   4) con lai: literal nao "trong nhu tieng Viet" (tu khoa tieng Viet khong dau, hoac chu HOA
#      khong dau kieu "CANH BAO") thi PHAI duoc boc trong `T(...)`, hoac dong do phai co
#      `i18n-ok` (dung cho khoa console PG_KEY - co tinh khong dich).
VN_WORD = re.compile(
    r'\b(khong|duoc|truc|bam|nhiet|chon|xoa|huy|gui|trang|dong|thong|bao|luu|cua|voi|khi|lai|nhu|'
    r'hoac|nguoi|dung|chay|xong|loi|het|day|luc|sau|truoc|nho|ten|gia tri|buoc|chieu|rong|duoi|tren|'
    r'phai|trai|giua|tat|bat|moi|cho|den|khe|nhieu|khoa|khop|danh sach|thu muc|tap tin|man hinh|'
    # cac am tiet tieng Viet hay dung trong chuoi HIEN THI (them 2026-10-15 sau khi audit bat duoc):
    r'quet|ngat|ket noi|dang|nhan|cham|thay|thu lai|qua ngan|hieu chuan|sat|goc|keo|tiep tuc|nhac|'
    r'do lech|doi moc|thu muc|thong so|gia tri|cai dat)\b', re.I)
LOG_STMT = re.compile(r'\b(Serial|WIFELOG|WIFILOG|HLOG|LOG)\b')
UI_FILES = ('ui_chrome.cpp', 'ui_widgets.cpp', 'ui_boxes.cpp', 'ui_overlay.cpp', 'ui_lvgl.cpp',
            'ui_lang.cpp', 'ui_page_submenu.cpp', 'ui_page_settings.cpp')
LIT_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def audit_display():
    """Tra ve danh sach (file, dong, chuoi) = chuoi TIENG VIET con hard-code trong duong HIEN THI."""
    bad = []
    files = sorted(f for f in os.listdir(SRC)
                   if f.endswith('.cpp') and (f.startswith('ui_') or f in UI_FILES))
    for f in files:
        raw = read(os.path.join(SRC, f))
        nocomment = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'), raw, flags=re.S)
        nocomment = '\n'.join(l.split('//')[0] for l in nocomment.split('\n'))
        # tach cau lenh: giu nguyen so dong de bao dung vi tri
        lines = nocomment.split('\n')
        stmt, stmt_line = '', 1
        raw_lines = raw.split('\n')
        for i, line in enumerate(lines):
            if stmt == '':
                stmt_line = i + 1
            stmt += line + '\n'
            if ';' not in line:
                continue
            if not LOG_STMT.search(stmt):
                for m in LIT_RE.finditer(stmt):
                    s = m.group(1)
                    if len(s) < 4 or not re.search(r'[A-Za-z]', s):
                        continue
                    if s.startswith('%') or s.startswith('\\x') or s.startswith('['):
                        continue
                    if re.match(r'^[MG]\d', s):
                        continue
                    if s[:4].lower() in ('http', '/cydl', '/sd/'):
                        continue
                    looks_vn = bool(VN_WORD.search(s))
                    if not looks_vn:
                        continue
                    # dong THUC SU chua literal (khong phai dong bat dau cau lenh)
                    lit_line = stmt_line + stmt[:m.start()].count('\n')
                    src_line = raw_lines[lit_line - 1] if 0 < lit_line <= len(raw_lines) else ''
                    if src_line.lstrip().startswith('#'):      # #warning / #error / #ifndef -> khong phai UI
                        continue
                    # da boc trong T(...) chua?
                    if re.search(r'T\(\s*[A-Z_0-9]*\s*$', stmt[:m.start()]):
                        continue
                    if re.search(r'\bUI_T_[A-Z0-9_]+\s*$', stmt[:m.start()]):
                        continue
                    if any('i18n-ok' in raw_lines[k] for k in range(max(0, lit_line - 3),
                                                                    min(len(raw_lines), lit_line + 2))):
                        continue
                    bad.append((f, lit_line, s))
            stmt = ''
    return bad


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
    ap.add_argument('--fallback', action='store_true',
                    help='liet ke ID cua bang EN dang DUNG CHUNG ban tieng Viet (de soi "quen dich")')
    ap.add_argument('--audit', action='store_true',
                    help='AUDIT: con chuoi HIEN THI nao bang tieng Viet chua boc T() khong (bang chung B2)')
    args = ap.parse_args()

    ids = parse_enum(os.path.join(SRC, 'ui_lang.h'))
    vi, vi_dup, vi_order = parse_table(os.path.join(SRC, 'ui_lang_vi.h'))
    en, en_dup, en_order = parse_ovr(os.path.join(SRC, 'ui_lang_en.h'))
    id_set = set(ids)
    idx = {k: i for i, k in enumerate(ids)}

    print('ui_lang.h       : %d ID (UI_T_NONE + UI_T_COUNT la moc, khong tinh)' % len(ids))
    print('ui_lang_vi.h    : %d dong (BANG GOC - phai day du, LIEN MACH theo enum)' % len(vi))
    print('ui_lang_en.h    : %d dong (BANG GHI DE - duoc phep thieu)' % len(en))

    bad = False

    def check_order(name, order):
        """GCC (xtensa, -std=gnu++11) CHI nhan khoi tao co ten khi chi so TANG dan va LIEN MACH: chi so di
        nguoc HOAC co khoang trong -> `sorry, unimplemented: non-trivial designated initializers not
        supported`. Kiem tra o day de bao TRUOC khi build (loi build rat kho hieu neu khong biet).
        Bang GHI DE (EN) dung danh sach cap nen KHONG bi rang buoc nay."""
        nonlocal bad
        seq = [idx[k] for k in order if k in idx]
        want = list(range(len(seq)))
        if seq != want:
            print('  [LOI] %s: THU TU/VI TRI SAI (GCC se bao "non-trivial designated initializers"):' % name)
            for n, (a, b) in enumerate(zip(seq, want)):
                if a != b:
                    print('       dong thu %d: %s (dang o chi so %d, phai la %d)' % (n, order[n], a, b))
                    break
            bad = True

    def check(name, tbl, dup, require_all):
        nonlocal bad
        missing = [i for i in ids if i not in tbl]
        extra = [k for k in tbl if k not in id_set]
        empty = [k for k, v in tbl.items() if v in ('""',)]
        if dup:
            print('  [LOI] %s: ID khai bao 2 lan: %s' % (name, ', '.join(sorted(set(dup)))))
            bad = True
        if empty:
            print('  [LOI] %s: chuoi RONG: %s' % (name, ', '.join(sorted(empty))))
            bad = True
        if extra:
            print('  [LOI] %s: co ID KHONG co trong enum: %s' % (name, ', '.join(sorted(extra))))
            bad = True
        if missing and require_all:
            print('  [LOI] %s: THIEU %d dong (bang GOC phai day du):' % (name, len(missing)))
            for i in missing:
                print('       %s' % i)
            bad = True
        if not (missing or extra or empty or dup):
            print('  [OK]  %s: day du' % name)
        elif missing and not require_all:
            print('  [OK]  %s: %d dong rieng, %d dong DUNG CHUNG ban VI (hop le - xem ui_lang.h)'
                  % (name, len(tbl), len(missing)))

    # CHINH SACH (xem ghi chu dau ui_lang.h): VI = bang GOC phai day du; EN = bang GHI DE, thieu thi
    # `ui_t()` tu lay chuoi tieng Viet -> dung y nghia "chuoi nay giong het o 2 ngon ngu".
    check('VI', vi, vi_dup, True)
    check('EN', en, en_dup, False)
    check_order('VI', vi_order)

    same = [i for i in ids if i in vi and i in en and vi[i] == en[i]]
    print('  [i]  %d/%d chuoi co ban EN RIENG va GIONG VI (chuoi co tinh khong dich).'
          % (len(same), len(ids)))

    if args.fallback:
        fb = [i for i in ids if i not in en]
        print('\n-- %d ID o ban EN dang DUNG CHUNG chuoi tieng Viet (soi xem co chu Viet nao lot vao) --'
              % len(fb))
        for i in fb:
            print('   %-42s %s' % (i, vi.get(i, '<THIEU CA VI>')))


    if args.hardcoded:
        scan_hardcoded()

    if args.audit:
        left = audit_display()
        print('\n-- AUDIT: chuoi TIENG VIET con hard-code trong duong HIEN THI --')
        if left:
            for f, ln, s in left:
                print('   [CAN DICH] %s:%d  "%s"' % (f, ln, s[:70]))
            print('   ==> con %d cho (hoac boc T() hoac them `i18n-ok` neu co tinh khong dich)' % len(left))
            bad = True
        else:
            print('   [OK] khong con chuoi hien thi nao bang tieng Viet ngoai log/console/G-code')

    print('\n==> %s' % ('CO LOI - xem danh sach [LOI] o tren' if bad else 'DAT: 2 bang ngon ngu day du'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
