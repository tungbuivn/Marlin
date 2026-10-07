"""Chay FirstLayerTwice tren mot file gcode THAT, ngoai Cura.

Dung de xem truoc ket qua TRUOC khi phai mo Cura slice lai:

  * nap dung `cura_profile/scripts/FirstLayerTwice.py`
  * doc setting THAT tu machine instance cua Cura (khong hardcode gi)
  * chia gcode theo kieu Cura roi goi `execute()`

    python tests/run_first_layer_twice.py <file-vao> [file-ra]

Mac dinh file ra = <file-vao> bo duoi + ".flt.gcode".
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _cura_stub import build_module, chunk_like_cura, coerce, read_cura_settings

SCRIPT = "FirstLayerTwice"


def find_global_cfg():
    """Tim machine instance cua Cura co bat FirstLayerTwice."""
    base = os.path.join(os.environ.get("APPDATA", ""), "cura")
    for path in sorted(glob.glob(os.path.join(base, "*", "machine_instances", "*.global.cfg"))):
        if read_cura_settings(path, SCRIPT):
            return path
    return None


def axis_values(lines, axis):
    out = []
    for line in lines:
        code = line.split(";", 1)[0].strip()
        if not code or code.split()[0] not in ("G0", "G1"):
            continue
        if axis in code:
            try:
                out.append(float(code.split(axis, 1)[1].split(" ")[0]))
            except ValueError:
                pass
    return out


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(src)[0] + ".flt.gcode"

    cfg = find_global_cfg()
    if cfg is None:
        print("FAIL: khong tim thay machine instance nao cua Cura bat {0}".format(SCRIPT))
        return 1
    raw = read_cura_settings(cfg, SCRIPT)
    settings = dict((k, coerce(v)) for k, v in raw.items())
    print("Config Cura : {0}".format(cfg))
    print("Setting     : {0}".format(
        ", ".join("{0}={1}".format(k, settings[k]) for k in sorted(settings))))

    mod = build_module(SCRIPT)
    script = mod.FirstLayerTwice()
    script._settings = settings

    with open(src, "r", encoding="utf-8") as f:
        text = f.read()

    # Gcode Cura luu ra dia LA gcode da qua hau xu ly, nen file da slice roi van con
    # marker. Chay lai lan hai se chèn chong len nhau -> phai tu choi.
    if "; --- First Layer Twice:" in text:
        print("TU CHOI: file nay DA qua FirstLayerTwice (con marker trong file).")
        print("         Chay lai lan hai se chen chong hai khoi len nhau.")
        print("         Muon xem truoc: TAT FirstLayerTwice trong Cura, slice lai,")
        print("         roi chay harness tren file vua sinh (file do chi con ClampFeeds).")
        return 1

    chunks = chunk_like_cura(text)
    print("Gcode vao   : {0} ({1} chunk, {2} dong)".format(src, len(chunks), text.count("\n") + 1))

    out = script.execute(list(chunks))
    result = "\n".join(out)
    with open(dst, "w", encoding="utf-8", newline="\n") as f:
        f.write(result)
    print("Gcode ra    : {0} ({1} dong)".format(dst, result.count("\n") + 1))

    # --- bao cao ---
    lines = result.split("\n")
    marks = [l for l in lines if l.startswith("; --- First Layer Twice")]
    print("")
    print("=== moc trong file ra ===")
    for m in marks:
        print("    " + m)

    try:
        i1 = next(i for i, l in enumerate(out[1].split("\n")) if "bat dau pass 1" in l)
        i2 = next(i for i, l in enumerate(out[1].split("\n")) if "ket thuc pass 1" in l)
        i3 = next(i for i, l in enumerate(out[1].split("\n")) if "bat dau pass 2" in l)
        i4 = next(i for i, l in enumerate(out[1].split("\n")) if "ket thuc layer 0" in l)
    except StopIteration:
        print("")
        print("CANH BAO: khong thay du moc -> chunk chua ';LAYER:0' co the khac du kien")
    else:
        body = out[1].split("\n")
        print("")
        print("=== khoi giua hai pass ===")
        for l in body[i2:i3 + 1]:
            print("    " + l)
        print("")
        print("  Z pass 1 : {0}".format(sorted(set(axis_values(body[i1:i2], "Z")))))
        print("  Z pass 2 : {0}".format(sorted(set(axis_values(body[i3:i4], "Z")))))
        e1 = axis_values(body[i1:i2], "E")
        e2 = axis_values(body[i3:i4], "E")
        print("  E pass 1 : {0} gia tri, ket thuc {1}".format(len(e1), e1[-1] if e1 else None))
        print("  E pass 2 : {0} gia tri, ket thuc {1}".format(len(e2), e2[-1] if e2 else None))
        print("  E hai pass giong nhau: {0}".format(e1 == e2))
        print("  M221     : {0}".format(
            [l.strip() for l in lines if l.strip().startswith("M221")]))

    print("")
    print("Kiem tra tiep: python tests/verify_gcode.py \"{0}\"".format(dst))
    return 0


if __name__ == "__main__":
    sys.exit(main())
