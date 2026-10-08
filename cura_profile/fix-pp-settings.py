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
xuat ra van bi kep sai.

Cach lam o day: chi dong bo nhung gia tri **phai khop gioi han cua firmware**
(M203/M201). Cac gia tri khac de nguyen de khong de len phan ban da tinh chinh
trong giao dien Cura.

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

# key -> gia tri dung. Chi gom gia tri bi rang buoc boi firmware.
OVERRIDES = {
    "ClampFeeds": {
        "max_feedrate_xy": "150",   # = M203 X/Y
        "max_acceleration": "300",  # = M201 X/Y
    },
}

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
    if not changes:
        return []

    lines[start:end] = (head + "=" + new_blob).split("\n")
    if not what_if:
        stamp = time.strftime("%Y%m%d-%H%M%S")
        shutil.copyfile(path, "{0}.bak-{1}".format(path, stamp))
        with open(path, "w", encoding="utf-8", newline="") as f:
            f.write("\n".join(lines))
    return changes


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

    total = 0
    for path in files:
        changes = process_file(path, args.what_if)
        if changes is None:
            continue
        name = os.path.basename(path)
        if not changes:
            print("[PP] {0} : da dung, khong doi".format(name))
            continue
        for script, key, old, new in changes:
            print("[PP] {0} : [{1}] {2}: {3} -> {4}".format(name, script, key, old, new))
            total += 1

    print("")
    print("{0} gia tri {1}.".format(total, "se doi" if args.what_if else "da doi"))
    if total and not args.what_if:
        print("MO LAI CURA de no doc lai khoi nay.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
