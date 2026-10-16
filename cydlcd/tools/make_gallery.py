#!/usr/bin/env python3
"""
make_gallery.py -- chup TOAN BO man hinh + dialog cua UI cydlcd tren ban GIA LAP roi ghep thanh 1 anh.

VI SAO: kiem tra UI bang cach bam tay tren board rat cham va de sot (14 trang + 6 dialog). Script nay
dung lai dung bo kich ban `sim/scenes_all.txt`, chay ban gia lap, va ghep 20 anh .bmp thanh mot bang co
chu thich -> nhin 1 lan la thay het, khong can board.

Dung:
    python3 cydlcd/tools/make_gallery.py                 # -> cydlcd/sim/out/gallery.png
    python3 cydlcd/tools/make_gallery.py --out /tmp/g.png

Yeu cau: `tools/run_sim.sh` chay duoc (xem README muc 3) + Pillow.
Sua danh sach canh chup trong `sim/scenes_all.txt` (moi dong 1 lenh cua ban gia lap).
"""
import argparse
import glob
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..'))          # cydlcd/
SCENES = os.path.join(ROOT, 'sim', 'scenes_all.txt')
RUN = os.path.join(ROOT, 'tools', 'run_sim.sh')

COLS, CAP = 4, 18


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.join(ROOT, 'sim', 'out', 'gallery.png'))
    ap.add_argument('--keep', action='store_true', help='giu lai thu muc anh .bmp')
    args = ap.parse_args()

    shots = os.path.join(ROOT, 'sim', 'out', 'gallery')
    os.makedirs(shots, exist_ok=True)
    for f in glob.glob(os.path.join(shots, '*')):
        os.remove(f)

    print(f'[*] chay ban gia lap voi {os.path.relpath(SCENES, ROOT)} ...')
    r = subprocess.run([RUN, '--script', SCENES, '--out', shots], cwd=ROOT)
    files = sorted(glob.glob(os.path.join(shots, '*')))
    if r.returncode != 0 and not files:
        print('LỖI: ban gia lap khong chay duoc'); return 1
    print(f'[*] {len(files)} anh')

    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print('LỖI: can Pillow (pip install pillow). Anh .bmp van nam o:', shots)
        return 1

    W, H = 320, 240
    rows = (len(files) + COLS - 1) // COLS
    sheet = Image.new('RGB', (COLS * W, rows * (H + CAP)), (12, 12, 12))
    draw = ImageDraw.Draw(sheet)
    blank = []
    for i, f in enumerate(files):
        im = Image.open(f).convert('RGB')
        px = im.load()
        ink = sum(1 for y in range(H) for x in range(W) if sum(px[x, y]) > 430)
        if ink < 300:
            blank.append(os.path.basename(f))
        x, y = (i % COLS) * W, (i // COLS) * (H + CAP)
        sheet.paste(im, (x, y + CAP))
        draw.text((x + 4, y + 3), os.path.basename(f), fill=(255, 220, 120))

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    sheet.save(args.out)
    print(f'[*] -> {args.out}  ({sheet.size[0]}x{sheet.size[1]})')

    if blank:
        print(f'[!] CANH BAO: {len(blank)} anh gan nhu TRONG (co the la loi UI):')
        for b in blank:
            print('    ', b)
        return 2
    print('[=] khong anh nao trong')
    if not args.keep and os.path.isdir(shots):
        for f in glob.glob(os.path.join(shots, '*')):
            os.remove(f)
        os.rmdir(shots)
    return 0


if __name__ == '__main__':
    sys.exit(main())
