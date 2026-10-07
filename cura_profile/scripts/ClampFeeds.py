"""ClampFeeds.py

Cura post-processing script: ep MOI lenh feed (F) trong G-code ve trong gioi han
tung truc cua may.

Vi sao can: profile Cura khong phai luc nao cung toi duoc slicer (xem README muc
11.7 cam bay 19), nen G-code co the chua F vuot tran cua may. Firmware Marlin
KHONG bao loi - no tu kep toc do tung truc xuong tran (planner.cpp:2419), nhung
con so trong file thi sai va khong ai biet.

Marlin gioi han THEO TUNG TRUC, khong theo F don thuan:
    current_speed[i] = steps_dist_mm[i] * inverse_secs;
    if (current_speed[i] > max_feedrate_mm_s[i]) -> giam toc do ca block

Nen mot buoc di chuyen chi hop le khi MOI truc co mat trong buoc do deu <= tran
cua truc ay. Script tinh lai F lon nhat con hop le cho tung buoc va ghi lai.

Vi du: G1 Z2.0 F3000 la buoc thuan Z -> F3000 = 50 mm/s > tran Z 10 mm/s
       -> ghi lai thanh F600.

Cura nap script theo quy tac: TEN CLASS PHAI TRUNG TEN FILE.
"""

import math

from ..Script import Script


class ClampFeeds(Script):

    def getSettingDataString(self):
        return """{
            "name": "Clamp Feeds",
            "key": "ClampFeeds",
            "metadata": {},
            "version": 2,
            "settings":
            {
                "enabled":
                {
                    "label": "Bat",
                    "description": "Bat/tat script nay.",
                    "type": "bool",
                    "default_value": true
                },
                "max_feedrate_xy":
                {
                    "label": "Tran toc do X/Y (mm/s)",
                    "description": "Phai khop M203 X.. Y.. cua firmware.",
                    "type": "float",
                    "default_value": 300,
                    "minimum_value": 1
                },
                "max_feedrate_z":
                {
                    "label": "Tran toc do Z (mm/s)",
                    "description": "Phai khop M203 Z cua firmware.",
                    "type": "float",
                    "default_value": 10,
                    "minimum_value": 0.1
                },
                "max_feedrate_e":
                {
                    "label": "Tran toc do E (mm/s)",
                    "description": "Phai khop M203 E cua firmware. Retract thuong vuot tran nay.",
                    "type": "float",
                    "default_value": 25,
                    "minimum_value": 0.1
                },
                "clamp_acceleration":
                {
                    "label": "Ep ca M204 S",
                    "description": "Ep cac lenh M204 S (acceleration in) ve tran duoi day. Tat neu chi muon sua F.",
                    "type": "bool",
                    "default_value": true
                },
                "max_acceleration":
                {
                    "label": "Tran acceleration (mm/s2)",
                    "description": "Phai khop M201 X/Y cua firmware.",
                    "type": "float",
                    "default_value": 500,
                    "minimum_value": 1
                }
            }
        }"""

    # ------------------------------------------------------------------ #

    @staticmethod
    def _fmt(value):
        """Ghi so kieu Cura: nguyen thi khong co phan thap phan."""
        if abs(value - round(value)) < 0.05:
            return "{0:.0f}".format(round(value))
        return "{0:.1f}".format(value)

    @staticmethod
    def _fmt_down(value):
        """Lam tron XUONG 2 chu so thap phan.

        Bat buoc dung khi KEP: lam tron len se lam toc do vuot tran mot chut
        (vd tran Z 10 mm/s -> F3648.9 lam tron thanh F3649 = 10.0014 mm/s).
        """
        v = math.floor(value * 100.0) / 100.0
        if abs(v - round(v)) < 1e-9:
            return "{0:.0f}".format(int(round(v)))
        return ("{0:.2f}".format(v)).rstrip("0").rstrip(".")

    def _limits(self):
        return {
            "X": float(self.getSettingValueByKey("max_feedrate_xy")),
            "Y": float(self.getSettingValueByKey("max_feedrate_xy")),
            "Z": float(self.getSettingValueByKey("max_feedrate_z")),
            "E": float(self.getSettingValueByKey("max_feedrate_e")),
        }

    # ------------------------------------------------------------------ #

    def _clamp_chunk(self, chunk, state, limits, cap_a, do_acc):
        lines = chunk.split("\n")
        out = []

        # Trang thai nam trong `state` va duoc GIU NGUYEN qua cac chunk: Cura chia
        # gcode thanh nhieu chunk (moi layer mot chunk), neu reset vi tri moi chunk
        # thi delta cua buoc dau tien moi chunk se sai (E la tuyet doi, rat lon).
        pos = state["pos"]
        abs_pos = state["abs_pos"]
        abs_e = state["abs_e"]
        req_f = state["req_f"]
        cur_f = state["cur_f"]

        for line in lines:
            code = line.split(";", 1)[0].strip()
            if not code:
                out.append(line)
                continue

            word = code.split()[0].upper()
            vals = {}
            for tok in code.split()[1:]:
                if tok and tok[0] in "XYZEF" and len(tok) > 1:
                    try:
                        vals[tok[0]] = float(tok[1:])
                    except ValueError:
                        pass

            # --- che do toa do ---
            if word == "G90":
                abs_pos = True
                out.append(line)
                continue
            if word == "G91":
                abs_pos = False
                out.append(line)
                continue
            if word == "M82":
                abs_e = True
                out.append(line)
                continue
            if word == "M83":
                abs_e = False
                out.append(line)
                continue
            if word == "G92":
                for a in ("X", "Y", "Z", "E"):
                    if a in vals:
                        pos[a] = vals[a]
                out.append(line)
                continue

            # --- ep M204 S ---
            if do_acc and word == "M204" and "S" in code:
                head = line.split(";", 1)[0]
                tail = line[len(head):]
                body = head.strip()
                parts = body.split()
                changed = False
                for i, p in enumerate(parts):
                    if p.startswith("S") and len(p) > 1:
                        try:
                            if float(p[1:]) > cap_a:
                                parts[i] = "S" + self._fmt(cap_a)
                                changed = True
                        except ValueError:
                            pass
                if changed:
                    out.append(" ".join(parts) + tail)
                    continue
                out.append(line)
                continue

            if word not in ("G0", "G1"):
                out.append(line)
                continue

            if "F" in vals:
                req_f = vals["F"]

            # --- di chuyen ---
            d = {}
            for a in ("X", "Y", "Z"):
                if a in vals:
                    d[a] = (vals[a] - pos[a]) if abs_pos else vals[a]
                    pos[a] = pos[a] + (d[a] if abs_pos else vals[a])
                else:
                    d[a] = 0.0
            if "E" in vals:
                d["E"] = (vals["E"] - pos["E"]) if abs_e else vals["E"]
                pos["E"] = pos["E"] + (d["E"] if abs_e else vals["E"])
            else:
                d["E"] = 0.0

            dxyz = math.sqrt(d["X"] ** 2 + d["Y"] ** 2 + d["Z"] ** 2)

            if req_f is None or (dxyz <= 0 and abs(d["E"]) <= 0):
                out.append(line)
                continue

            # --- F lon nhat con hop le cho buoc nay (mm/phut) ---
            if dxyz > 0:
                cap_mms = min(limits[a] * dxyz / abs(d[a])
                              for a in ("X", "Y", "Z") if abs(d[a]) > 1e-9)
                if abs(d["E"]) > 1e-9:
                    cap_mms = min(cap_mms, limits["E"] * dxyz / abs(d["E"]))
            else:
                cap_mms = limits["E"]
            cap_f = cap_mms * 60.0

            eff_f = req_f if req_f <= cap_f else cap_f
            if eff_f < 1.0:
                eff_f = 1.0
            writing_down = eff_f < req_f      # dang kep -> phai lam tron xuong

            if "F" in vals:
                out.append(self.putValue(line, F=(self._fmt_down(eff_f) if writing_down else self._fmt(eff_f))))
                cur_f = eff_f
            elif cur_f is None or abs(cur_f - eff_f) > 0.05:
                out.append(line + " F" + (self._fmt_down(eff_f) if writing_down else self._fmt(eff_f)))
                cur_f = eff_f
            else:
                out.append(line)

        state["abs_pos"] = abs_pos
        state["abs_e"] = abs_e
        state["req_f"] = req_f
        state["cur_f"] = cur_f
        return "\n".join(out)

    # ------------------------------------------------------------------ #

    def execute(self, data):
        if not self.getSettingValueByKey("enabled"):
            return data

        limits = self._limits()
        cap_a = float(self.getSettingValueByKey("max_acceleration"))
        do_acc = self.getSettingValueByKey("clamp_acceleration")

        state = {
            "pos": {"X": 0.0, "Y": 0.0, "Z": 0.0, "E": 0.0},
            "abs_pos": True,   # G90 / G91
            "abs_e": True,     # M82 / M83
            "req_f": None,     # mm/phut, F doc tu file (modal)
            "cur_f": None,     # mm/phut, F dang co hieu luc trong file xuat ra
        }

        return [self._clamp_chunk(chunk, state, limits, cap_a, do_acc) for chunk in data]
