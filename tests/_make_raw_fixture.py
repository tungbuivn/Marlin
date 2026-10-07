"""One-off: dung lai file 'raw' (truoc hau xu ly) tu gcode da qua FirstLayerTwice.

Body cua pass 2 chinh la body GOC cua layer 0 (script khong sua E, va pass 2 in
trong he toa do goc), nen chi can bo pass 1 + khoi nhay di la thu duoc file raw.
"""
import io

SRC = r"D:\0in\V300_Part3.gcode"
DST = r"D:\0in\V300_Part3.raw.gcode"

lines = io.open(SRC, encoding="utf-8").read().split("\n")


def find(prefix):
    for i, l in enumerate(lines):
        if l.startswith(prefix):
            return i
    raise SystemExit("khong thay: " + prefix)


i_p1 = find("; --- First Layer Twice: bat dau pass 1")
i_p2 = find("; --- First Layer Twice: bat dau pass 2")
i_end = find("; --- First Layer Twice: ket thuc layer 0")

raw = lines[:i_p1] + lines[i_p2 + 1:i_end] + lines[i_end + 2:]
io.open(DST, "w", encoding="utf-8", newline="\n").write("\n".join(raw))
print("bo {0} dong (pass 1 + khoi nhay), {1} -> {2} dong".format(
    len(lines) - len(raw), len(lines), len(raw)))
print("con marker FirstLayerTwice: {0}".format(
    sum(1 for l in raw if "First Layer Twice" in l)))
