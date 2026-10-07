"""Test cho ClampFeeds: chay tren file gcode that va kiem chung tung truc.

Chay: python tests/test_clamp_feeds.py [duong_dan_gcode]
"""
import importlib
import math
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), "cura_profile", "scripts", "ClampFeeds.py")
GCODE = sys.argv[1] if len(sys.argv) > 1 else r"D:\0in\V300_Part3.gcode"

LIMIT = {"X": 300.0, "Y": 300.0, "Z": 10.0, "E": 25.0}
MAX_A = 500.0

STUB = '''\
class Script:
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

PKG = os.path.join(tempfile.gettempdir(), "clampfeeds_pkg")

NUM = re.compile(r"([XYZEF])\s*(-?\d*\.?\d+)")


def build_module():
    if os.path.isdir(PKG):
        shutil.rmtree(PKG)
    scripts = os.path.join(PKG, "scripts")
    os.makedirs(scripts)
    for d, n in ((PKG, "__init__.py"), (PKG, "Script.py"), (scripts, "__init__.py")):
        with open(os.path.join(d, n), "w", encoding="utf-8") as f:
            f.write(STUB if n == "Script.py" else "")
    shutil.copyfile(SRC, os.path.join(scripts, "ClampFeeds.py"))
    parent = os.path.dirname(PKG)
    if parent not in sys.path:
        sys.path.insert(0, parent)
    return importlib.import_module(os.path.basename(PKG) + ".scripts.ClampFeeds")


def split_chunks(lines):
    """Chia giong Cura: chunk dau la header, moi chunk sau bat dau bang ';LAYER:'."""
    chunks, cur = [], []
    for ln in lines:
        if ln.startswith(";LAYER:") and cur:
            chunks.append("\n".join(cur))
            cur = []
        cur.append(ln)
    if cur:
        chunks.append("\n".join(cur))
    return chunks


def audit(lines):
    """Tra ve (vi_pham, max_tung_truc, max_M204)."""
    pos = {"X": 0.0, "Y": 0.0, "Z": 0.0, "E": 0.0}
    abs_pos = abs_e = True
    feed = 0.0
    bad = []
    axis_max = {k: 0.0 for k in LIMIT}
    max_a = 0.0

    for ln, raw in enumerate(lines, 1):
        code = raw.split(";", 1)[0].strip()
        if not code:
            continue
        word = code.split()[0].upper()
        v = {m.group(1): float(m.group(2)) for m in NUM.finditer(code)}
        if word == "G90":
            abs_pos = True; continue
        if word == "G91":
            abs_pos = False; continue
        if word == "M82":
            abs_e = True; continue
        if word == "M83":
            abs_e = False; continue
        if word == "G92":
            for a in ("X", "Y", "Z", "E"):
                if a in v:
                    pos[a] = v[a]
            continue
        if word == "M204":
            for p in code.split()[1:]:
                if p.startswith("S") and len(p) > 1:
                    try:
                        max_a = max(max_a, float(p[1:]))
                    except ValueError:
                        pass
            continue
        if word not in ("G0", "G1"):
            continue
        if "F" in v:
            feed = v["F"] / 60.0
        if feed <= 0:
            continue

        d = {}
        for a in ("X", "Y", "Z"):
            d[a] = ((v[a] - pos[a]) if abs_pos else v[a]) if a in v else 0.0
            if a in v:
                pos[a] = pos[a] + (d[a] if abs_pos else v[a])
        if "E" in v:
            d["E"] = (v["E"] - pos["E"]) if abs_e else v["E"]
            pos["E"] = pos["E"] + (d["E"] if abs_e else v["E"])
        else:
            d["E"] = 0.0

        dxyz = math.sqrt(d["X"] ** 2 + d["Y"] ** 2 + d["Z"] ** 2)
        if dxyz > 0:
            t = dxyz / feed
        elif abs(d["E"]) > 0:
            t = abs(d["E"]) / feed
        else:
            continue
        for a in LIMIT:
            sp = abs(d[a]) / t
            axis_max[a] = max(axis_max[a], sp)
            if sp > LIMIT[a] + 1e-6:
                bad.append((ln, word, feed * 60, a, sp, LIMIT[a]))
    return bad, axis_max, max_a


def main():
    with open(GCODE, "r", encoding="utf-8", errors="replace") as f:
        lines = f.read().split("\n")
    chunks = split_chunks(lines)

    mod = build_module()
    script = mod.ClampFeeds()
    script._settings = {
        "enabled": True,
        "max_feedrate_xy": 300, "max_feedrate_z": 10, "max_feedrate_e": 25,
        "clamp_acceleration": True, "max_acceleration": 500,
    }

    print("File : {0}".format(GCODE))
    print("Chunk: {0}".format(len(chunks)))

    bad0, mx0, a0 = audit(lines)
    print("")
    print("=== TRUOC ===")
    for a in ("X", "Y", "Z", "E"):
        print("  {0}: max {1:8.2f} mm/s (tran {2})".format(a, mx0[a], LIMIT[a]))
    print("  M204 S lon nhat: {0}".format(a0))
    print("  so buoc vuot tran: {0}".format(len(bad0)))

    out_chunks = script.execute(chunks)
    out_lines = "\n".join(out_chunks).split("\n")

    bad1, mx1, a1 = audit(out_lines)
    print("")
    print("=== SAU ===")
    for a in ("X", "Y", "Z", "E"):
        print("  {0}: max {1:8.2f} mm/s (tran {2})".format(a, mx1[a], LIMIT[a]))
    print("  M204 S lon nhat: {0}".format(a1))
    print("  so buoc vuot tran: {0}".format(len(bad1)))
    if bad1:
        for b in bad1[:8]:
            print("    dong {0}: {1} F{2:.0f} -> truc {3} {4:.1f} > {5}".format(*b))

    n_changed = sum(1 for a, b in zip(lines, out_lines) if a != b)
    print("")
    print("  so dong bi sua: {0} / {1}".format(n_changed, len(lines)))

    assert not bad1, "FAIL: van con {0} buoc vuot tran".format(len(bad1))
    assert a1 <= MAX_A + 1e-6, "FAIL: M204 S{0} > {1}".format(a1, MAX_A)
    assert len(out_lines) == len(lines), "FAIL: so dong thay doi"

    # cac buoc truoc day vuot tran phai duoc sua
    print("")
    print("=== vi du cac dong da sua ===")
    shown = 0
    for a, b in zip(lines, out_lines):
        if a != b and shown < 8:
            print("  - {0}".format(a.strip()))
            print("  + {0}".format(b.strip()))
            shown += 1

    print("")
    print("  TAT CA ASSERTION PASS")


if __name__ == "__main__":
    main()
