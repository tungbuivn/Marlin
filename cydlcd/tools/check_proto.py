#!/usr/bin/env python3
"""
check_proto.py -- canh 2 ban `cydlcd_protocol.h` khong bi LECH NHAU.

VI SAO CAN: giao thuc CYD <-> Marlin duoc dinh nghia trong CUNG mot file header nhung co HAI BAN COPY
(project khong chia se duoc file giua 2 cay build):
    Marlin/src/lcd/extui/cydlcd_serial/cydlcd_protocol.h     <- Marlin (STM32) dung
    cydlcd/src/app/cydlcd_protocol.h                         <- man hinh CYD (ESP32) dung
Sua enum/thu tu truong o MOT ben ma quen ben kia thi khung bi lech am tham: CYD hien sai so nhiet do,
sai toa do, hoac khong nhan ra may da halt (muc FATAL) - ma KHONG co loi build nao bao. Script nay so
PHAN MA (bo comment) de bat dung truong hop do.

VI SAO KHONG so TUNG BYTE: comment CO TINH khac nhau - moi ben tham chieu file cua chinh no
(vd `ui_preheat.cpp` o CYD, `ui_lvgl.cpp` o Marlin), va ban Marlin ghi ro hon o vai cho. Chi phan KHAI
BAO (enum, struct, #define, kich thuoc mang) moi bat buoc phai giong.

Dung:  python3 tools/check_proto.py          -> in ket qua, exit 0 = khop
       python3 tools/check_proto.py -v        -> in ca khac biet tung dong
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
CYD = os.path.normpath(os.path.join(HERE, '..', 'src', 'app', 'cydlcd_protocol.h'))
MRL = os.path.normpath(os.path.join(HERE, '..', '..', 'Marlin', 'src', 'lcd',
                                    'extui', 'cydlcd_serial', 'cydlcd_protocol.h'))


def code_lines(path):
    """Tra ve danh sach (so_dong, noi_dung) CHI gom phan ma: bo comment // va /* */, bo dong trong."""
    out = []
    src = open(path, encoding='utf-8', errors='replace').read()
    src = re.sub(r'/\*.*?\*/', '', src, flags=re.S)          # comment khoi
    for n, line in enumerate(src.split('\n'), 1):
        line = re.sub(r'//.*$', '', line).rstrip()           # comment dong (header khong co "//" trong chuoi)
        if line.strip():
            out.append((n, line))
    return out


def main():
    verbose = '-v' in sys.argv
    for p in (CYD, MRL):
        if not os.path.exists(p):
            print(f'LỖI: không thấy {p}')
            return 2

    a, b = code_lines(CYD), code_lines(MRL)
    ta = [x[1].strip() for x in a]
    tb = [x[1].strip() for x in b]

    if ta == tb:
        print(f'==> ĐẠT: 2 bản cydlcd_protocol.h KHỚP phần mã ({len(ta)} dòng khai báo)')
        print(f'    CYD   : {os.path.relpath(CYD)}')
        print(f'    Marlin: {os.path.relpath(MRL)}')
        return 0

    print('LỖI: 2 bản cydlcd_protocol.h LỆCH NHAU ở phần mã!')
    print('     Sửa lại cho khớp rồi chạy lại. Khác biệt:')
    import difflib
    for line in difflib.unified_diff(ta, tb, 'CYD', 'Marlin', lineterm='', n=2):
        print('   ', line)
    if verbose:
        print('\n    (số dòng gốc)')
        print('     CYD   :', ', '.join(str(n) for n, _ in a[:5]), '...')
        print('     Marlin:', ', '.join(str(n) for n, _ in b[:5]), '...')
    return 1


if __name__ == '__main__':
    sys.exit(main())
