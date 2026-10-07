"""Test hoi quy cho FirstLayerTwice.

Kiem tra co che dung M221 flow thay vi nhan lai E:

  pass 1  - body da HA xuong first_pass_z (0.1), chi doi flow bang M221 S20
  giua    - nhac Z THAT len first_pass_z + <Z goc layer 0> (0.1 + 0.2 = 0.3),
            roi G92 khai bao lai day la <Z goc layer 0> (0.2), reset extruder ve
            E dau layer 0, M221 S80
  pass 2  - BODY GOC, khong ha -> E cua 2 pass phai GIONG HET NHAU
  ket thuc- M221 S100

Boi canh ba loi da gap:
  1) _find_layer_z() doi cu lay Z dau tien -> vuong Z-hop 0.4 -> sinh ra Z am.
  2) phai lay Z NHO NHAT trong body (moi Z-hop deu cao hon chieu cao layer).
  3) pass 2 dung lai body DA HA -> buoc tu layer 0 len layer 1 thanh 0.3, tuc ho
     0.1 mm khong khi. Phai dung body GOC thi moi buoc moi dung 0.20.

Chay: python tests/test_first_layer_twice.py
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _cura_stub import build_module

# --- gcode gia lap, mo phong dung cau truc Cura sinh ra -------------------
CHUNK0 = ";FLAVOR:Marlin\n;Layer height: 0.2\n;MINZ:0.2\n;MAXZ:1\n"

CHUNK1 = "\n".join([
    "M140 S60",
    "M104 S230",
    "; ----- Voron2 300 / MKS Monster8 V2 / Voron Tap -----",
    "G28", "G34 Q99", "G28 Z",
    "G92 E0", "G1 Z2.0 F3000", "M82", "G92 E0",
    "G1 X2 Y10 Z0.3 F5000",
    "G1 X2 Y100 Z0.3 F1500 E15",
    "G92 E0", "G1 Z2.0 F3000", "G92 E0", "G1 F900 E-0.75",
    ";LAYER_COUNT:5",
    ";LAYER:0",
    "M107",
    "M204 S150",
    "G1 F600 Z0.4",          # <-- Z-HOP (0.2 layer + 0.2 hop), KHONG phai chieu cao layer
    "G0 F11250 X10 Y10",
    ";TYPE:SKIRT",
    "G1 F600 Z0.2",          # <-- chieu cao THAT cua layer 0
    "G1 F900 E0",
    "G1 F1800 X20 Y20 E1",
    "G1 X30 Y30 E2",
    ";MESH:NONMESH",
    "G0 F600 Z0.4",          # Z-hop cuoi layer
    "G0 F11250 X30 Y30",
    ";TIME_ELAPSED:10",
    "",
])

CHUNK2 = ";LAYER:1\nM106 S85\nG1 F600 Z0.6\n;TYPE:SKIRT\nG1 F600 Z0.4\nG1 F900 E2\nG1 F1800 X40 Y40 E3\n;MESH:NONMESH\nG0 F600 Z0.6\n;TIME_ELAPSED:20\n"
CHUNK3 = ";LAYER:2\nM106 S85\nG1 F600 Z0.8\n;TYPE:SKIRT\nG1 F600 Z0.6\nG1 F900 E3\nG1 F1800 X50 Y50 E4\n;TIME_ELAPSED:30\n"

SETTINGS = {
    "enabled": True,
    "force_temperatures": False,
    "double_first_layer": True,
    "pass1_flow": 20,
    "pass2_flow": 80,
    "z_feedrate": 600,
    "first_pass_z": 0.1,
}

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


def old_find_layer_z(self, body):
    """Ban CU: lay gia tri Z dau tien trong body (chinh la loi - vuong Z-hop)."""
    for line in body:
        code = line.split(";", 1)[0].strip()
        if not code or code.split()[0] not in ("G0", "G1"):
            continue
        value = self.getValue(line, "Z")
        if value is not None:
            return float(value)
    return None


def main():
    mod = build_module("FirstLayerTwice")

    # --- 1) _find_layer_z phai tra ve chieu cao layer (0.2), khong phai hop (0.4)
    body = CHUNK1.split("\n")
    detected = mod.FirstLayerTwice._find_layer_z(
        mod.FirstLayerTwice(), body[body.index(";LAYER:0") + 1:])
    print("  _find_layer_z tren body layer 0 : {0}".format(detected))
    assert detected == 0.2, "FAIL: phai la 0.2 (chieu cao layer), dang la {0}".format(detected)

    # --- 2) chay that
    fixed = mod.FirstLayerTwice()
    fixed._settings = dict(SETTINGS)
    out = fixed.execute([CHUNK0, CHUNK1, CHUNK2, CHUNK3])
    lines = out[1].split("\n")

    i1 = next(i for i, l in enumerate(lines) if "bat dau pass 1" in l)
    i2 = next(i for i, l in enumerate(lines) if "ket thuc pass 1" in l)
    i3 = next(i for i, l in enumerate(lines) if "bat dau pass 2" in l)
    i4 = next(i for i, l in enumerate(lines) if "ket thuc layer 0" in l)
    print("  moc: pass1 dong {0} | giua {1}-{2} | pass2 tu {3} | ket thuc {4}".format(
        i1 + 1, i2 + 1, i3 + 1, i3 + 1, i4 + 1))

    print("")
    print("=== khoi giua hai pass ===")
    for l in lines[i2:i3 + 1]:
        print("    " + l)

    z1 = sorted(set(axis_values(lines[i1:i2], "Z")))
    z2 = sorted(set(axis_values(lines[i3:i4], "Z")))
    e1 = axis_values(lines[i1:i2], "E")
    e2 = axis_values(lines[i3:i4], "E")

    print("")
    print("  Z pass 1            : {0}".format(z1))
    print("  Z pass 2            : {0}".format(z2))
    print("  so gia tri E pass 1 : {0}".format(len(e1)))
    print("  so gia tri E pass 2 : {0}".format(len(e2)))
    print("  E pass 1            : {0}".format(e1))
    print("  E pass 2            : {0}".format(e2))

    # pass 1 ha xuong first_pass_z = 0.1 (Z-hop 0.4 - 0.1 = 0.3)
    assert z1 == [0.1, 0.3], "FAIL pass 1: mong doi [0.1, 0.3], dang la {0}".format(z1)
    # pass 2 chay BODY GOC -> Z goc cua layer 0 la 0.2 (Z-hop 0.4)
    assert z2 == [0.2, 0.4], "FAIL pass 2: mong doi [0.2, 0.4], dang la {0}".format(z2)

    # DIEM MAU CHOT: E cua hai pass GIONG HET NHAU -> khong nhan lai E
    assert e1 == e2, "FAIL: E cua hai pass phai giong het nhau\n  pass1={0}\n  pass2={1}".format(e1, e2)

    # moc M221
    assert any("M221 S20" in l for l in lines[i1:i2]), "FAIL: thieu M221 S20 truoc pass 1"
    assert any("M221 S80" in l for l in lines[i2:i3]), "FAIL: thieu M221 S80 truoc pass 2"
    assert any("M221 S100" in l for l in lines[i4:]), "FAIL: thieu M221 S100 sau layer 0"

    # nang Z THAT len first_pass_z + Z goc layer 0 = 0.1 + 0.2 = 0.3, roi G92 ve 0.2
    # (ve Z GOC cua layer 0, de body goc roi dung cho va moi layer sau giu dung buoc 0.2)
    assert any("G1 Z0.3 F600" in l for l in lines[i2:i3]), "FAIL: buoc nang Z sai"
    assert any("G92 Z0.2" in l for l in lines[i2:i3]), "FAIL: thieu G92 Z0.2 (Z goc layer 0)"
    # reset extruder ve E dau layer 0 (trong gcode nay la -0.75, sau lenh retract)
    assert any("G92 E-0.75" in l for l in lines[i2:i3]), "FAIL: thieu G92 E dau layer 0"

    # --- 2b) BUOC Z VAT LY giua cac layer phai dung bang chieu cao layer ---
    # Day la phep kiem hoi quy cho loi THAT da gap: neu pass 2 dung lai body da ha
    # (Z = 0.1) thi buoc tu layer 0 len layer 1 thanh 0.3 -> ho 0.1 mm khong khi.
    # Do tren file sach: pass1 0.1 -> pass2 0.3 (0.2 OK) -> layer1 0.6 (0.30 SAI).
    jump_lines = lines[i2:i3]
    j_z = next(float(l.split("Z", 1)[1].split(" ")[0]) for l in jump_lines
               if l.split(";")[0].strip().startswith("G1") and "Z" in l)
    j_g92 = next(float(l.split("Z", 1)[1].split(" ")[0]) for l in jump_lines
                 if l.split(";")[0].strip().startswith("G92") and "Z" in l)
    offset = j_z - j_g92                      # vat ly = toa do + offset
    press = [("pass 1", min(z1)),              # truoc buoc nhay: offset = 0
             ("pass 2", min(z2) + offset),
             ("layer 1", min(axis_values(CHUNK2.split("\n"), "Z")) + offset),
             ("layer 2", min(axis_values(CHUNK3.split("\n"), "Z")) + offset)]
    print("")
    print("  Z VAT LY tung luot (offset sau buoc nhay = {0}):".format(round(offset, 4)))
    for k, (name, z) in enumerate(press):
        step = "" if k == 0 else "   buoc {0:.2f}".format(z - press[k - 1][1])
        print("    {0:8s} Z = {1:.2f}{2}".format(name, z, step))
    for k in range(1, len(press)):
        step = press[k][1] - press[k - 1][1]
        assert abs(step - 0.2) < 1e-9, (
            "FAIL: buoc {0} -> {1} = {2:.2f}, phai la 0.20 (chieu cao layer)".format(
                press[k - 1][0], press[k][0], step))

    # doi chung: pass 2 dung lai body DA HA (Z = 0.1) thi buoc nay thanh 0.30
    old_step = (min(axis_values(CHUNK2.split("\n"), "Z")) + offset) - (min(z1) + offset)
    print("")
    print("  [doi chung] neu pass 2 dung lai body DA HA (Z = 0.1):")
    print("    buoc layer 0 -> layer 1 = {0:.2f}  (phai la 0.20)".format(old_step))
    assert abs(old_step - 0.3) < 1e-9, "FAIL: doi chung khong the hien duoc loi"

    # --- 3) layer 1 tro len khong bi dung toi
    assert out[2] == CHUNK2, "FAIL: chunk layer 1 bi thay doi"
    assert out[3] == CHUNK3, "FAIL: chunk layer 2 bi thay doi"

    # --- 3b) chunk phai KET THUC bang '\n' ---
    # Moi chunk cua Cura ket thuc bang newline va Cura noi chung truc tiep (khong
    # tu them dau phan cach). Thieu newline cuoi -> chunk sau bi dinh lien vao dong
    # cuoi (da gap that: ';LAYER:1' dinh vao dong 'M221 S100').
    for n, chunk in ((0, out[0]), (1, out[1])):
        assert chunk.endswith("\n"), "FAIL: chunk {0} khong ket thuc bang newline".format(n)

    # --- 4) DOI CHUNG: lay nham Z-hop (0.4) lam chieu cao layer -> dich 0.3 -> Z am
    broken = mod.FirstLayerTwice()
    broken._settings = dict(SETTINGS)
    broken._find_layer_z = old_find_layer_z.__get__(broken, mod.FirstLayerTwice)
    out_old = broken.execute([CHUNK0, CHUNK1, CHUNK2, CHUNK3])
    zs_old = axis_values(out_old[1].split("\n"), "Z")
    print("")
    print("  [doi chung] code CU (lay Z dau tien = 0.4):")
    print("    Z nho nhat = {0}   (danh sach {1})".format(min(zs_old), sorted(set(zs_old))))
    assert min(zs_old) < 0, "FAIL: doi chung khong the hien loi (mong doi co Z am)"

    print("")
    print("  TAT CA ASSERTION PASS")


if __name__ == "__main__":
    main()
