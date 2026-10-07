"""Kiem tra nhanh file gcode xuat ra: FirstLayerTwice (M221) + ClampFeeds.

    python tests/verify_gcode.py <file.gcode> [--full]

Doc setting THAT tu machine instance cua Cura (qua `_cura_stub`) de biet phai
mong doi gi, thay vi hardcode: doi `first_pass_z` trong Cura thi cho nay theo.
"""
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from _cura_stub import coerce, read_cura_settings

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from run_first_layer_twice import find_global_cfg

GCODE = sys.argv[1] if len(sys.argv) > 1 else r"D:\0in\V300_Part3.gcode"
FULL = "--full" in sys.argv
LIMIT = {"X": 300.0, "Y": 300.0, "Z": 10.0, "E": 25.0}
MAX_ACCEL = 500.0
NUM = re.compile(r"([XYZEF])\s*(-?\d*\.?\d+)")


def vals_of(line):
    code = line.split(";", 1)[0].strip()
    return {m.group(1): float(m.group(2)) for m in NUM.finditer(code)}


def load_settings():
    """Setting cua FirstLayerTwice trong Cura; None neu khong doc duoc."""
    cfg = find_global_cfg()
    if cfg is None:
        return None, None
    raw = read_cura_settings(cfg, "FirstLayerTwice")
    return cfg, dict((k, coerce(v)) for k, v in raw.items()) if raw else (cfg, None)


def show(name, values):
    if FULL or len(values) <= 8:
        print("  {0}: {1}".format(name, values))
    else:
        print("  {0}: {1} gia tri, {2} ... {3}".format(
            name, len(values), values[:3], values[-2:]))


def main():
    with open(GCODE, "r", encoding="utf-8", errors="replace") as f:
        lines = f.read().split("\n")

    cfg, st = load_settings()
    print("File  : {0}".format(GCODE))
    print("Dong  : {0}".format(len(lines)))
    if st:
        print("Config: {0}".format(cfg))
        print("Mong doi: first_pass_z={0} pass1_flow={1} pass2_flow={2}".format(
            st.get("first_pass_z"), st.get("pass1_flow"), st.get("pass2_flow")))
    else:
        print("Config: KHONG doc duoc setting Cura -> bo qua vai phep kiem")
    print("")

    # ---------- 1) moc cua FirstLayerTwice ----------
    def find(sub, start=0):
        return next((i for i, l in enumerate(lines) if sub in l and i >= start), None)

    i1 = find("bat dau pass 1")
    i2 = find("ket thuc pass 1")
    i3 = find("bat dau pass 2")
    i4 = find("ket thuc layer 0")
    i5 = find("bat dau pass 1", (i1 or 0) + 1)   # lan chèn thu hai = loi

    print("=== 1) FirstLayerTwice ===")
    print("  moc: pass1 {0} | giua {1} | pass2 {2} | ket thuc {3}".format(i1, i2, i3, i4))
    if i2 is not None and i3 is not None:
        print("  --- khoi giua hai pass ---")
        for l in lines[i2:i3 + 1]:
            print("      " + l)

    problems = []
    if i1 is None:
        print("  !! khong thay marker FirstLayerTwice")
        problems.append("thieu marker FirstLayerTwice")
    if i5 is not None:
        print("  !! marker 'bat dau pass 1' xuat hien lan hai o dong {0}".format(i5))
        problems.append("FirstLayerTwice bi chèn hai lan")

    def m221_between(a, b):
        return [l.strip() for l in lines[a:b + 1] if l.strip().startswith("M221")]

    print("  M221 truoc pass 1 : {0}".format(m221_between(i1, i2) if i1 is not None else "?"))
    print("  M221 giua 2 pass  : {0}".format(m221_between(i2, i3) if i2 is not None else "?"))
    print("  M221 sau layer 0  : {0}".format(m221_between(i4, i4 + 3) if i4 is not None else "?"))

    z1 = z2 = []
    e1 = e2 = []
    if None not in (i1, i2, i3, i4):
        z1 = sorted({vals_of(l).get("Z") for l in lines[i1:i2]
                     if vals_of(l).get("Z") is not None})
        z2 = sorted({vals_of(l).get("Z") for l in lines[i3:i4]
                     if vals_of(l).get("Z") is not None})
        e1 = [vals_of(l)["E"] for l in lines[i1:i2] if "E" in vals_of(l)]
        e2 = [vals_of(l)["E"] for l in lines[i3:i4] if "E" in vals_of(l)]
        show("Z pass 1          ", z1)
        show("Z pass 2          ", z2)
        show("E pass 1          ", e1)
        show("E pass 2          ", e2)
        print("  E hai pass GIONG  : {0}".format(e1 == e2))
        if e1 != e2:
            problems.append("E cua hai pass KHAC nhau")
        if z1 != z2:
            problems.append("Z cua hai pass KHAC nhau (body phai y nguyen)")

        # --- co che: pass 1 o first_pass_z; giua hai pass nang Z THAT len
        #     first_pass_z + <Z goc cua layer 0>, roi G92 khai bao lai la first_pass_z.
        #     Nho vay body dich cua pass 2 nam dung o cao do.
        want1 = st.get("first_pass_z") if st else 0.1
        jump = lines[i2 + 1:i3] if i2 is not None and i3 is not None else []
        j_z = next((vals_of(l).get("Z") for l in jump
                    if l.split(";")[0].strip().startswith("G1") and "Z" in vals_of(l)), None)
        j_g92 = next((vals_of(l).get("Z") for l in jump
                      if l.split(";")[0].strip().startswith("G92") and "Z" in vals_of(l)), None)

        if z1 and want1 is not None and abs(min(z1) - want1) > 1e-6:
            problems.append("Z pass 1 = {0}, mong doi first_pass_z = {1}".format(min(z1), want1))
        if want1 is not None and j_g92 != want1:
            problems.append("G92 Z giua hai pass = {0}, mong doi {1}".format(j_g92, want1))
        if j_z is None:
            problems.append("khong thay buoc nang Z giua hai pass")
        elif want1 is not None:
            # Z goc cua layer 0: uu tien doc tu header Cura, khong thi suy tu chinh buoc nhay
            minz = next((float(l.split(":")[1]) for l in lines
                         if l.startswith(";MINZ:")), None)
            print("  buoc nhay: G1 Z{0} + G92 Z{1} -> pass 2 in o Z vat ly {0}".format(j_z, j_g92))
            if minz is not None:
                print("  header ;MINZ:{0} (Z goc cua layer 0)".format(minz))
                if abs((j_z - want1) - minz) > 1e-6:
                    problems.append("buoc nhay {0} != first_pass_z {1} + MINZ {2}".format(
                        j_z, want1, minz))
            if j_z <= want1:
                problems.append("buoc nhay Z {0} khong cao hon pass 1 ({1})".format(j_z, want1))
        if max(z1 or [0]) > min(z1 or [0]) + 1e-6:
            print("  (Z cao nhat pass 1 = {0} -- la Z-hop, khong phai loi)".format(max(z1)))

        # M221 phai dung thu tu pass1 -> pass2 -> 100
        want_flows = ["M221 S{0} ;".format(st["pass1_flow"]),
                      "M221 S{0} ;".format(st["pass2_flow"]),
                      "M221 S100 ;"] if st else None
        got = [l.strip() for l in lines if l.strip().startswith("M221")]
        if want_flows and (len(got) != len(want_flows)
                           or any(not g.startswith(w) for g, w in zip(got, want_flows))):
            print("  !! M221 nhan duoc: {0}".format(got))
            problems.append("chuoi M221 khac mong doi {0}".format(want_flows))

    # ---------- 2) audit feed/accel cua ClampFeeds ----------
    pos = {"X": 0.0, "Y": 0.0, "Z": 0.0, "E": 0.0}
    abs_pos = abs_e = True
    feed = 0.0
    bad = []
    axis_max = {k: 0.0 for k in LIMIT}
    max_a = 0.0
    mixed = []
    split_marks = 0

    for ln, raw in enumerate(lines, 1):
        if "ClampFeeds: tach XY" in raw:
            split_marks += 1
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
            mixed.append((ln, raw))
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
    print("  M204 S lon nhat      : {0} (tran {1})".format(max_a, MAX_ACCEL))
    print("  buoc vuot tran       : {0}".format(len(bad)))
    for b in bad[:5]:
        print("    dong {0}: {1} F{2:.0f} -> {3} {4:.1f}".format(*b))
    print("  buoc co ca XY lan Z  : {0}".format(len(mixed)))
    for m in mixed[:5]:
        print("    dong {0}: {1}".format(*m))
    print("  moc 'tach XY'        : {0}".format(split_marks))

    if bad:
        problems.append("{0} buoc vuot tran feedrate".format(len(bad)))
    if max_a > MAX_ACCEL + 1e-6:
        problems.append("M204 S{0} > {1}".format(max_a, MAX_ACCEL))
    if mixed:
        problems.append("{0} buoc co ca XY lan Z".format(len(mixed)))

    print("")
    if problems:
        print("  KET LUAN: CON VAN DE")
        for p in problems:
            print("    - {0}".format(p))
    else:
        print("  KET LUAN: TAT CA OK")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
