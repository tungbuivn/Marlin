"""ClampFeeds.py

Cura post-processing script, lam 3 viec tren G-code:

1) EP MOI LENH FEED (F) ve trong tran tung truc cua may.
   Marlin gioi han theo TUNG TRUC, khong theo F don thuan:
       current_speed[i] = steps_dist_mm[i] * inverse_secs;
       if (current_speed[i] > max_feedrate_mm_s[i]) -> giam toc do ca block
   (planner.cpp:2415-2419). Nen mot buoc chi hop le khi MOI truc co mat trong
   buoc do deu <= tran cua truc ay. Firmware khong bao loi - no tu kep - nhung
   con so trong file thi sai.

2) TACH MOI BUOC CO CA XY LAN Z thanh hai buoc.
   Mot lenh nhu  G1 X2 Y10 Z0.3 F5000  lam dau in VUA chay ngang VUA ha dan,
   tao mot duong doc cat qua mat ban. Tach ra: di ngang o Z hien tai truoc, roi
   moi ha Z. Day la loi that da gap trong duong purge cu.

3) VE TAM sau khi purge (tuy chon).
   Chen mot buoc di ngang toi `after_purge_xy` ngay truoc `;LAYER_COUNT`, tuc la
   sau khi purge da nhac Z len.

Vi sao dung post-processing script: profile Cura khong phai luc nao cung toi duoc
slicer (xem README cam bay 19 - Cura xoa setting khi ghi lai container), trong khi
post-processing script la co che da chung minh chay duoc tren may nay.

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
                    "description": "Phai khop M203 X.. Y.. cua firmware. May nay la 150: motor co dien cam 17 mH nen o 24 V chi keo du dong dinh muc toi ~225 mm/s, va stealthChop bat toan dai keo tran thuc dung xuong ~100-150. Xem README 3.3.",
                    "type": "float",
                    "default_value": 150,
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
                    "description": "Phai khop M201 X/Y cua firmware. May nay la 2000. LUU Y: day chi la KEP XUONG - neu M204 S trong file nho hon 2000 thi no giu nguyen, nen Cura van phai phat ra dung so (xem README 3.3: machine_max_acceleration_x/y quyet dinh con so Cura phat).",
                    "type": "float",
                    "default_value": 2000,
                    "minimum_value": 1
                },
                "split_xyz_moves":
                {
                    "label": "Tach buoc XY+Z",
                    "description": "Tach moi buoc co ca XY lan Z thanh 2 buoc: di ngang truoc, roi moi ha Z. Tranh dau in ha dan trong luc dang di ngang.",
                    "type": "bool",
                    "default_value": true
                },
                "after_purge_xy":
                {
                    "label": "Ve tam sau purge (X,Y)",
                    "description": "Chen mot buoc di ngang toi toa do nay ngay truoc ;LAYER_COUNT (tuc sau khi purge da nhac Z len). De trong de tat.",
                    "type": "str",
                    "default_value": "152.5,152.5"
                },
                "after_purge_f":
                {
                    "label": "Toc do ve tam (mm/phut)",
                    "description": "Feedrate cho buoc ve tam.",
                    "type": "float",
                    "default_value": 6000,
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

    @staticmethod
    def _axis_text(axis, value):
        return "{0}{1}".format(axis, repr(float(value)))

    def _max_feed_mm_per_min(self, d, limits):
        """F lon nhat con hop le cho mot buoc (mm/phut)."""
        dxyz = math.sqrt(d["X"] ** 2 + d["Y"] ** 2 + d["Z"] ** 2)
        if dxyz > 0:
            cap = min(limits[a] * dxyz / abs(d[a])
                      for a in ("X", "Y", "Z") if abs(d[a]) > 1e-9)
            if abs(d["E"]) > 1e-9:
                cap = min(cap, limits["E"] * dxyz / abs(d["E"]))
        else:
            cap = limits["E"]
        return cap * 60.0

    # ------------------------------------------------------------------ #

    def _clamp_chunk(self, chunk, state, limits, cap_a, do_acc, do_split, purge_xy, purge_f):
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
            raw_code = line.split(";", 1)[0].strip()

            # --- chen buoc ve tam ngay truoc moc layer dau tien ---
            # Phai kiem tra tren DONG GOC: dong ';LAYER_COUNT' la comment nen sau khi
            # cat comment thi raw_code rong.
            stripped = line.strip()
            if (purge_xy and not state["purge_done"]
                    and (stripped.startswith(";LAYER_COUNT") or stripped.startswith(";LAYER:"))):
                if abs_pos:
                    out.append("; --- ClampFeeds: ve tam sau purge ---")
                    out.append("G1 F{0} X{1} Y{2}".format(self._fmt(purge_f), purge_xy[0], purge_xy[1]))
                    pos["X"] = purge_xy[0]
                    pos["Y"] = purge_xy[1]
                state["purge_done"] = True

            if not raw_code:
                out.append(line)
                continue

            word = raw_code.split()[0].upper()
            vals = {}
            for tok in raw_code.split()[1:]:
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
            if do_acc and word == "M204" and "S" in raw_code:
                head = line.split(";", 1)[0]
                tail = line[len(head):]
                parts = head.strip().split()
                changed = False
                for i, p in enumerate(parts):
                    if p.startswith("S") and len(p) > 1:
                        try:
                            if float(p[1:]) > cap_a:
                                parts[i] = "S" + self._fmt(cap_a)
                                changed = True
                        except ValueError:
                            pass
                out.append((" ".join(parts) + tail) if changed else line)
                continue

            if word not in ("G0", "G1"):
                out.append(line)
                continue

            if "F" in vals:
                req_f = vals["F"]

            has_xy = ("X" in vals) or ("Y" in vals)
            has_z = "Z" in vals

            # --- TACH BUOC XY+Z ---
            if do_split and has_xy and has_z:
                # phan XY (va E) di o Z hien tai truoc
                d_xy = {"X": 0.0, "Y": 0.0, "Z": 0.0, "E": 0.0}
                for a in ("X", "Y"):
                    if a in vals:
                        d_xy[a] = (vals[a] - pos[a]) if abs_pos else vals[a]
                if "E" in vals:
                    d_xy["E"] = (vals["E"] - pos["E"]) if abs_e else vals["E"]

                f_xy = req_f
                if req_f is not None and (abs(d_xy["X"]) > 1e-9 or abs(d_xy["Y"]) > 1e-9
                                          or abs(d_xy["E"]) > 1e-9):
                    cap_xy = self._max_feed_mm_per_min(d_xy, limits)
                    if f_xy > cap_xy:
                        f_xy = cap_xy

                t = [word]
                if f_xy is not None:
                    t.append("F" + self._fmt_down(f_xy))
                for a in ("X", "Y", "E"):
                    if a in vals:
                        t.append(self._axis_text(a, vals[a]))
                out.append(" ".join(t) + " ; ClampFeeds: tach XY")
                for a in ("X", "Y"):
                    if a in vals:
                        pos[a] = vals[a] if abs_pos else pos[a] + vals[a]
                if "E" in vals:
                    pos["E"] = vals["E"] if abs_e else pos["E"] + vals["E"]
                cur_f = f_xy

                # roi moi ha Z (chi phat neu Z thuc su doi)
                d_z = {"X": 0.0, "Y": 0.0, "Z": (vals["Z"] - pos["Z"]) if abs_pos else vals["Z"], "E": 0.0}
                if abs(d_z["Z"]) > 1e-9:
                    f_z = req_f
                    if req_f is not None:
                        cap_z = self._max_feed_mm_per_min(d_z, limits)
                        if f_z > cap_z:
                            f_z = cap_z
                    out.append(" ".join([word] + (["F" + self._fmt_down(f_z)] if f_z is not None else [])
                                        + [self._axis_text("Z", vals["Z"])]) + " ; ClampFeeds: ha Z sau")
                    pos["Z"] = vals["Z"] if abs_pos else pos["Z"] + vals["Z"]
                    cur_f = f_z
                continue

            # --- di chuyen binh thuong ---
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

            if req_f is None or (math.sqrt(d["X"] ** 2 + d["Y"] ** 2 + d["Z"] ** 2) <= 0 and abs(d["E"]) <= 0):
                out.append(line)
                continue

            cap_f = self._max_feed_mm_per_min(d, limits)
            eff_f = req_f if req_f <= cap_f else cap_f
            if eff_f < 1.0:
                eff_f = 1.0
            writing_down = eff_f < req_f      # dang kep -> phai lam tron xuong
            txt = self._fmt_down(eff_f) if writing_down else self._fmt(eff_f)

            if "F" in vals:
                out.append(self.putValue(line, F=txt))
                cur_f = eff_f
            elif cur_f is None or abs(cur_f - eff_f) > 0.05:
                out.append(line + " F" + txt)
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
        do_split = self.getSettingValueByKey("split_xyz_moves")
        purge_f = float(self.getSettingValueByKey("after_purge_f"))

        purge_xy = None
        txt = (self.getSettingValueByKey("after_purge_xy") or "").strip()
        if txt:
            parts = [p.strip() for p in txt.replace(" ", "").split(",")]
            if len(parts) == 2:
                try:
                    purge_xy = (float(parts[0]), float(parts[1]))
                except ValueError:
                    purge_xy = None

        state = {
            "pos": {"X": 0.0, "Y": 0.0, "Z": 0.0, "E": 0.0},
            "abs_pos": True,     # G90 / G91
            "abs_e": True,       # M82 / M83
            "req_f": None,       # mm/phut, F doc tu file (modal)
            "cur_f": None,       # mm/phut, F dang co hieu luc trong file xuat ra
            "purge_done": False,
        }

        return [self._clamp_chunk(chunk, state, limits, cap_a, do_acc, do_split, purge_xy, purge_f)
                for chunk in data]
