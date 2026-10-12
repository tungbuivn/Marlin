"""Dong bo setting DANG LUU cua cac post-processing script trong Cura.

VI SAO CAN
----------
Khi ban bat mot script hau xu ly, Cura luu TOAN BO setting cua no vao khoi
`post_processing_scripts` cua machine instance:

    <config>/<ver>/machine_instances/<May>.global.cfg

Gia tri **dang luu** do DE LEN `default_value` trong file `.py`. Nen sua default
trong script la KHONG DU - Cura van dung con so cu.

Da gap that: doi `ClampFeeds.max_feedrate_xy` 300 -> 150 va `max_acceleration`
500 -> 2000 trong `ClampFeeds.py`, nhung khoi da luu van giu 300/500, nen G-code
xuat ra van bi kep sai. (`ClampFeeds` da duoc GO ngay 2026-10-12 vi firmware da tu
kep theo M203/M201 - xem README muc 11.6; khoi da luu cua no nay bi XOA bang REMOVE.)

Cach lam o day gom hai phan:

  OVERRIDES : chi dong bo nhung gia tri **phai khop** voi rang buoc vat ly cua
              chinh thuat toan (pass 2 cua FirstLayerTwice phai la 100%). Cac gia
              tri khac de nguyen de khong de len phan ban da tinh chinh trong Cura.

  REMOVE    : XOA han khoi setting cua nhung script da GO khoi profile. Cura
              KHONG tu xoa khoi da luu khi file `.py` khong con duoc cai, nen phai
              xoa tay o day.

Chay:
    python cura_profile/fix-pp-settings.py
    python cura_profile/fix-pp-settings.py --what-if
"""
import argparse
import glob
import os
import re
import shutil
import sys
import time

# key -> gia tri dung.
#   - FirstLayerTwice: khong phai gioi han firmware nhung co rang buoc VAT LY:
#       pass2_flow PHAI la 100 vi pass 2 da nam cao hon pass 1 dung mot chieu cao
#       layer init => no la mot lop binh thuong (giam flow chi lam lop do thieu nhua).
#       pass2_feedrate = 60mm/s: chi la gia tri DU PHONG - mac dinh script TU LAY toc do
#       that cua layer 1 trong G-code (pass2_feedrate_auto = true) nen khong can chinh
#       khi doi profile.
#       pass2_z_extra = 0.05mm: nha them khi nhay pass 1 -> pass 2 cho nozzle khoi
#       ca vao lop nhua vua in.
#       pass1_flow = 20% la gia tri KHOI DAU cho lop miet bam ban - sau lan dong bo
#       nay ban van doi duoc trong giao dien Cura.
OVERRIDES = {
    "FirstLayerTwice": {
        "pass1_flow": "20",         # lop miet bam ban (mac dinh moi)
        "pass2_flow": "100",        # lop 2 = lop binh thuong
        "pass2_feedrate": "60",     # pass 2 in o toc do binh thuong (mm/s), khong dung Initial Layer Speed
        "pass2_z_extra": "0.05",    # nang them 0.05mm khi nhay pass 1 -> pass 2
    },
}

# Ten script da GO khoi profile -> XOA toan bo khoi [<ten>] trong blob.
# ClampFeeds go ngay 2026-10-12: firmware da tu kep theo M203/M201 nen script chi
# con gay trung lap (va truoc do con kep SAI vi profile van 150/300 trong khi
# firmware cho 225/5000). File duoc giu lai o cura_profile/scripts_disabled/.
REMOVE = {"ClampFeeds"}

BLOB_KEY = "post_processing_scripts"


def split_blob(blob):
    """Tach blob thanh (list dong logic, list dau phan cach) de ghep lai y nguyen.

    Cura escape newline bang mot cum `\\` + `n` (so dau `\\` khong co dinh giua cac
    phien ban), nen phai giu lai chinh xac tung dau phan cach.
    """
    parts = re.split(r"(\\+n)", blob)
    return parts[0::2], parts[1::2]


def apply_overrides(blob):
    """Sua cac key trong OVERRIDES, tra ve (blob moi, danh sach thay doi)."""
    lines, seps = split_blob(blob)
    current, changes = None, []
    for i, entry in enumerate(lines):
        text = entry.strip()
        if text.startswith("[") and text.endswith("]"):
            current = text[1:-1]
            continue
        if "=" not in text or current not in OVERRIDES:
            continue
        key, value = (p.strip() for p in text.split("=", 1))
        want = OVERRIDES[current].get(key)
        if want is None or value == want:
            continue
        # giu nguyen phan thut dau dong goc
        indent = entry[:len(entry) - len(entry.lstrip())]
        lines[i] = "{0}{1} = {2}".format(indent, key, want)
        changes.append((current, key, value, want))

    out = lines[0]
    for sep, line in zip(seps, lines[1:]):
        out += sep + line
    return out, changes


def apply_removals(blob):
    """Xoa toan bo khoi `[<ten>]` cua cac script trong REMOVE.

    Khoi bat dau o dong `[TenScript]` va keo dai toi TRUOC dong `[` ke tiep (hoac
    het blob). Tra ve (blob moi, danh sach ten da xoa).

    Ghep lai bang cach giu nguyen dau phan cach di TRUOC moi dong con lai: blob
    goc = lines[0] + seps[0] + lines[1] + seps[1] + ... nen chi can bo cac dong
    thuoc khoi va ghep lai theo tung cap (dong, dau phan cach dung truoc no). Lam
    vay khong bao gio de lai dau phan cach mo coi.
    """
    lines, seps = split_blob(blob)
    n = len(lines)

    dropped, removed = set(), []
    i = 0
    while i < n:
        text = lines[i].strip()
        if text.startswith("[") and text.endswith("]") and text[1:-1] in REMOVE:
            name = text[1:-1]
            j = i + 1
            while j < n:
                nxt = lines[j].strip()
                if nxt.startswith("[") and nxt.endswith("]"):
                    break
                j += 1
            dropped.update(range(i, j))
            removed.append(name)
            i = j
            continue
        i += 1

    if not removed:
        return blob, []

    kept = [(idx, lines[idx]) for idx in range(n) if idx not in dropped]
    if not kept:
        return "", removed
    out = kept[0][1]
    for idx, line in kept[1:]:
        out += seps[idx - 1] + line
    return out, removed


def process_file(path, what_if=False):
    with open(path, "r", encoding="utf-8") as f:
        text = f.read()
    lines = text.split("\n")

    start = next((i for i, l in enumerate(lines) if l.startswith(BLOB_KEY)), None)
    if start is None:
        return None

    end = start + 1
    while end < len(lines) and lines[end][:1] in ("\t", " "):
        end += 1

    raw = "\n".join(lines[start:end])
    head, _, blob = raw.partition("=")
    new_blob, changes = apply_overrides(blob)
    new_blob, removed = apply_removals(new_blob)
    if not changes and not removed:
        return [], []

    lines[start:end] = (head + "=" + new_blob).split("\n")
    if not what_if:
        stamp = time.strftime("%Y%m%d-%H%M%S")
        shutil.copyfile(path, "{0}.bak-{1}".format(path, stamp))
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write("\n".join(lines))
    return changes, removed


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=os.path.join(os.environ.get("APPDATA", ""), "cura"),
                    help="thu muc cau hinh Cura (mac dinh %%APPDATA%%\\cura)")
    ap.add_argument("--what-if", action="store_true", help="chi in, khong ghi")
    args = ap.parse_args()

    pattern = os.path.join(args.root, "*", "machine_instances", "*.global.cfg")
    files = sorted(glob.glob(pattern))
    if not files:
        print("Khong thay machine instance nao trong {0}".format(pattern))
        return 1

    total, total_removed = 0, 0
    for path in files:
        res = process_file(path, args.what_if)
        if res is None:
            continue
        changes, removed = res
        name = os.path.basename(path)
        if not changes and not removed:
            print("[PP] {0} : da dung, khong doi".format(name))
            continue
        for script, key, old, new in changes:
            print("[PP] {0} : [{1}] {2}: {3} -> {4}".format(name, script, key, old, new))
            total += 1
        for script in removed:
            print("[PP] {0} : da xoa khoi [{1}]".format(name, script))
            total_removed += 1

    print("")
    print("{0} gia tri {1}, {2} khoi {3}.".format(
        total, "se doi" if args.what_if else "da doi",
        total_removed, "se xoa" if args.what_if else "da xoa"))
    if (total or total_removed) and not args.what_if:
        print("MO LAI CURA de no doc lai khoi nay.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
