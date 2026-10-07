"""Test hoi quy cho FirstLayerTwice._find_layer_z.

Boi canh loi (da gap that trong D:\\0in\\V300_Part3.gcode):
  voron2_base.def.json cua Ultimaker bat Z-hop (retraction_hop_enabled = true,
  retraction_hop = 0.2), nen buoc G0/G1 co Z DAU TIEN cua layer 0 la buoc NANG
  len (0.2 + 0.2 = 0.4), khong phai chieu cao layer (0.2).
  _find_layer_z() doi cu lay Z dau tien -> 0.4 -> offset = 0.4 - first_pass_z
  = 0.3 (dung phai 0.1) -> chieu cao that 0.2 - 0.3 = -0.1.
  Marlin co Z_MIN_POS = 0 nen kep ve 0 -> pass 1 in ngay tren mat ban.

Chay: python tests/test_first_layer_twice.py
"""
import importlib
import os
import shutil
import sys
import tempfile

SRC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "cura_profile", "scripts", "FirstLayerTwice.py")

STUB = '''\
class Script:
    """Stub toi thieu cua cura.Script: chi nhung gi FirstLayerTwice dung."""

    def getSettingValueByKey(self, key):
        return self._settings.get(key)

    def getValue(self, line, key, default=None):
        if key in line:
            try:
                return line.split(key)[1].split(" ")[0]
            except IndexError:
                return default
        return default

    def putValue(self, line, **kwargs):
        for key, value in kwargs.items():
            if key in line:
                line = line.replace(key + self.getValue(line, key), key + str(value))
            else:
                line += " " + key + str(value)
        return line
'''

# --- gcode gia lap, mo phong dung cau truc Cura sinh ra -------------------
# Cura chia gcode_list theo layer; dong ';LAYER:n' nam trong chunk cua no.
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
    "G0 F600 Z0.4",          # Z-hop cuoi layer (trung chieu cao layer 1)
    "G0 F11250 X30 Y30",
    ";TIME_ELAPSED:10",
    "",
])

CHUNK2 = ";LAYER:1\nM106 S85\n;TYPE:SKIRT\nG1 F900 E2\nG1 F1800 X40 Y40 E3\n;MESH:NONMESH\nG0 F600 Z0.6\n;TIME_ELAPSED:20\n"
CHUNK3 = ";LAYER:2\nM106 S85\n;TYPE:SKIRT\nG1 F900 E3\nG1 F1800 X50 Y50 E4\n;TIME_ELAPSED:30\n"

SETTINGS = {
    "enabled": True,
    "force_temperatures": False,
    "double_first_layer": True,
    "z_mode": "split",
    "first_pass_z": 0.1,
    "z_feedrate": 600,
    "pass1_flow": 20,
    "pass2_flow": 80,
}

PKG = os.path.join(tempfile.gettempdir(), "flt_test_pkg")


def build_module():
    if os.path.isdir(PKG):
        shutil.rmtree(PKG)
    # Cau truc giong Cura: <plugin>/Script.py + <plugin>/scripts/<ten script>.py
    # (FirstLayerTwice dung relative import "from ..Script import Script")
    scripts = os.path.join(PKG, "scripts")
    os.makedirs(scripts)
    for d, name in ((PKG, "__init__.py"), (PKG, "Script.py"), (scripts, "__init__.py")):
        with open(os.path.join(d, name), "w", encoding="utf-8") as f:
            f.write(STUB if name == "Script.py" else "")
    shutil.copyfile(SRC, os.path.join(scripts, "FirstLayerTwice.py"))
    parent = os.path.dirname(PKG)
    if parent not in sys.path:
        sys.path.insert(0, parent)
    return importlib.import_module(os.path.basename(PKG) + ".scripts.FirstLayerTwice")


def z_values(lines):
    out = []
    for line in lines:
        code = line.split(";", 1)[0].strip()
        if not code or code.split()[0] not in ("G0", "G1"):
            continue
        if "Z" in code:
            try:
                out.append(float(code.split("Z", 1)[1].split(" ")[0]))
            except ValueError:
                pass
    return out


def old_find_layer_z(self, body):
    """Ban CU: lay gia tri Z dau tien trong body (chinh la loi)."""
    for line in body:
        code = line.split(";", 1)[0].strip()
        if not code or code.split()[0] not in ("G0", "G1"):
            continue
        value = self.getValue(line, "Z")
        if value is not None:
            return float(value)
    return None


def main():
    mod = build_module()

    # --- 1) _find_layer_z phai tra ve chieu cao layer (0.2), khong phai hop (0.4)
    body = CHUNK1.split("\n")
    detected = mod.FirstLayerTwice._find_layer_z(
        mod.FirstLayerTwice(), body[body.index(";LAYER:0") + 1:])
    print("  _find_layer_z tren body layer 0 : {0}".format(detected))
    assert detected == 0.2, "FAIL: phai la 0.2 (chieu cao layer), dang la {0}".format(detected)

    # --- 2) chay that voi ban da sua
    fixed = mod.FirstLayerTwice()
    fixed._settings = dict(SETTINGS)
    out = fixed.execute([CHUNK0, CHUNK1, CHUNK2, CHUNK3])
    lines = out[1].split("\n")
    all_z = z_values(lines)
    print("  Z trong chunk layer 0          : {0}".format(all_z))
    print("  Z nho nhat                     : {0}".format(min(all_z)))
    assert min(all_z) >= 0, "FAIL: van con Z am -> {0}".format(min(all_z))

    i1 = next(i for i, l in enumerate(lines) if "First Layer Twice: pass 1" in l)
    i2 = next(i for i, l in enumerate(lines) if "ket thuc pass 1" in l)
    i3 = next(i for i, l in enumerate(lines) if "bat dau pass 2" in l)
    z_pass1 = sorted(set(z_values(lines[i1:i2])))
    z_pass2 = sorted(set(z_values(lines[i3:])))
    print("  Z cua pass 1                   : {0}".format(z_pass1))
    print("  Z cua pass 2                   : {0}".format(z_pass2))
    assert z_pass1 == [0.1, 0.3], "FAIL pass 1: mong doi [0.1, 0.3], dang la {0}".format(z_pass1)
    assert z_pass2 == [0.2, 0.4], "FAIL pass 2: mong doi [0.2, 0.4], dang la {0}".format(z_pass2)

    # --- 3) layer 1 tro len khong bi dung toi
    assert out[2] == CHUNK2, "FAIL: chunk layer 1 bi thay doi"
    assert out[3] == CHUNK3, "FAIL: chunk layer 2 bi thay doi"

    # --- 4) DOI CHUNG: code CU phai FAIL, neu khong thi test nay vo nghia
    broken = mod.FirstLayerTwice()
    broken._settings = dict(SETTINGS)
    broken._find_layer_z = old_find_layer_z.__get__(broken, mod.FirstLayerTwice)
    out_old = broken.execute([CHUNK0, CHUNK1, CHUNK2, CHUNK3])
    zmin_old = min(z_values(out_old[1].split("\n")))
    print("  [doi chung] code CU, Z nho nhat: {0}".format(zmin_old))
    assert zmin_old < 0, "FAIL: test khong bat duoc loi cu (Z nho nhat = {0})".format(zmin_old)

    print("")
    print("  TAT CA ASSERTION PASS")


if __name__ == "__main__":
    main()
