"""Kiem tra nhanh file gcode xuat ra: FirstLayerTwice (M221) + ClampFeeds."""
import math
import re
import sys

GCODE = sys.argv[1] if len(sys.argv) > 1 else r"D:\0in\V300_Part3.gcode"
LIMIT = {"X": 300.0, "Y": 300.0, "Z": 10.0, "E": 25.0}
NUM = re.compile(r"([XYZEF])\s*(-?\d*\.?\d+)")


def vals_of(line):
    code = line.split(";", 1)[0].strip()
    return {m.group(1): float(m.group(2)) for m in NUM.finditer(code)}


def main():
    with open(GCODE, "r", encoding="utf-8", errors="replace") as f:
        lines = f.read().split("\n")

    print("File  : {0}".format(GCODE))
    print("Dong  : {0}".format(len(lines)))
    print("")

    # ---------- 1) moc cua FirstLayerTwice ----------
    def find(sub):
        return next((i for i, l in enumerate(lines) if sub in l), None)

    i1 = find("bat dau pass 1")
    i2 = find("ket thuc pass 1")
    i3 = find("bat dau pass 2")
    i4 = find("ket thuc layer 0")
    print("=== 1) FirstLayerTwice ===")
    print("  moc: pass1 {0} | giua {1} | pass2 {2} | ket thuc {3}".format(i1, i2, i3, i4))
    if i2 is not None:
        print("  --- khoi giua hai pass ---")
        for l in lines[i2:i3 + 1]:
            print("      " + l)

    def m221_between(a, b):
        return [l.strip() for l in lines[a:b + 1] if l.strip().startswith("M221")]

    print("  M221 truoc pass 1 : {0}".format(m221_between(i1, i2) if i1 is not None else "?"))
    print("  M221 giua 2 pass  : {0}".format(m221_between(i2, i3) if i2 is not None else "?"))
    print("  M221 sau layer 0  : {0}".format(m221_between(i4, i4 + 3) if i4 is not None else "?"))

    z1 = sorted({vals_of(l).get("Z") for l in lines[i1:i2]
                 if vals_of(l).get("Z") is not None})
    z2 = sorted({vals_of(l).get("Z") for l in lines[i3:i4]
                 if vals_of(l).get("Z") is not None})
    e1 = [vals_of(l)["E"] for l in lines[i1:i2] if "E" in vals_of(l)]
    e2 = [vals_of(l)["E"] for l in lines[i3:i4] if "E" in vals_of(l)]
    print("  Z pass 1          : {0}".format(z1))
    print("  Z pass 2          : {0}".format(z2))
    print("  E pass 1          : {0}".format(e1))
    print("  E pass 2          : {0}".format(e2))
    print("  E hai pass GIONG  : {0}".format(e1 == e2))

    # ---------- 2) audit feed/accel cua ClampFeeds ----------
    pos = {"X": 0.0, "Y": 0.0, "Z": 0.0, "E": 0.0}
    abs_pos = abs_e = True
    feed = 0.0
    bad = []
    axis_max = {k: 0.0 for k in LIMIT}
    max_a = 0.0
    mixed = 0

    for ln, raw in enumerate(lines, 1):
        code = raw.split(";", 1)[0].strip()
        if not code:
            continue
        word = code.split()[0].upper()
        v = vals_of(code)
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
        if ("X" in v or "Y" in v) and "Z" in v:
            mixed += 1
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
                bad.append((ln, word, feed * 60, a, sp))

    print("")
    print("=== 2) ClampFeeds ===")
    for a in ("X", "Y", "Z", "E"):
        print("  {0}: max {1:8.2f} mm/s (tran {2})".format(a, axis_max[a], LIMIT[a]))
    print("  M204 S lon nhat      : {0}".format(max_a))
    print("  buoc vuot tran       : {0}".format(len(bad)))
    for b in bad[:5]:
        print("    dong {0}: {1} F{2:.0f} -> {3} {4:.1f}".format(*b))
    print("  buoc co ca XY lan Z  : {0}".format(mixed))

    ok = (not bad and max_a <= 500.0 + 1e-6 and mixed == 0 and e1 == e2
          and z1 and min(z1) >= 0.19)
    print("")
    print("  KET LUAN: {0}".format("TAT CA OK" if ok else "CON VAN DE"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
