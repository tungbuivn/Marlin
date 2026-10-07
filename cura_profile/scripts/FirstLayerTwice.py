# FirstLayerTwice.py
#
# Cura post-processing script: in layer 0 hai lan.
#
#   pass 1 - in lai toan bo layer 0 voi flow rieng (mac dinh 20%)
#   giua   - nang Z len, roi G92 gan lai vi tri do thanh Z cua layer 0, de cac
#            buoc cua pass 2 roi xuong cao hon pass 1 dung mot lop
#   pass 2 - layer 0 voi flow rieng (mac dinh 80%)
#
# Layer 1 tro len KHONG bi dung toi (100%).
#
# Chi nhan E cua nhung buoc CO X hoac Y. Buoc retract / unretract la buoc thuan E
# (khong co XY) nen duoc giu nguyen - nhan chung voi 20% se bien lan retract
# thanh vo nghia.
#
# Cura nap script theo quy tac: TEN CLASS PHAI TRUNG TEN FILE.

from ..Script import Script


class FirstLayerTwice(Script):

    def getSettingDataString(self):
        return """{
            "name": "First Layer Twice",
            "key": "FirstLayerTwice",
            "metadata": {},
            "version": 2,
            "settings":
            {
                "enabled":
                {
                    "label": "Enable this script",
                    "description": "You must enable the script for it to run.",
                    "type": "bool",
                    "default_value": true,
                    "enabled": true
                },
                "force_temperatures":
                {
                    "label": "Force temperatures",
                    "description": "Ep nhiet do cho TOAN BO file, ke ca cac lenh init. Lenh tat nhiet (S0) duoc giu nguyen.",
                    "type": "bool",
                    "default_value": true,
                    "enabled": "enabled"
                },
                "hotend_temp":
                {
                    "label": "Hotend temperature",
                    "description": "M104 / M109 se bi ep ve so nay, bo qua vat lieu.",
                    "unit": "C",
                    "type": "int",
                    "default_value": 230,
                    "minimum_value": 0,
                    "enabled": "enabled and force_temperatures"
                },
                "bed_temp":
                {
                    "label": "Bed temperature",
                    "description": "M140 / M190 se bi ep ve so nay, bo qua vat lieu.",
                    "unit": "C",
                    "type": "int",
                    "default_value": 60,
                    "minimum_value": 0,
                    "enabled": "enabled and force_temperatures"
                },
                "double_first_layer":
                {
                    "label": "Print layer 0 twice",
                    "description": "Bat phan in layer 0 hai lan. Tat thi script chi ep nhiet do.",
                    "type": "bool",
                    "default_value": true,
                    "enabled": "enabled"
                },
                "pass1_flow":
                {
                    "label": "Pass 1 flow",
                    "description": "Luong nhua cho lan in dau cua layer 0, tinh theo % so voi binh thuong.",
                    "unit": "%",
                    "type": "float",
                    "default_value": 20,
                    "minimum_value": 0,
                    "maximum_value": 200,
                    "enabled": "enabled and double_first_layer"
                },
                "pass2_flow":
                {
                    "label": "Pass 2 flow (layer 0)",
                    "description": "Luong nhua cho lan in thu hai cua layer 0. Cac layer tren van 100%.",
                    "unit": "%",
                    "type": "float",
                    "default_value": 80,
                    "minimum_value": 0,
                    "maximum_value": 200,
                    "enabled": "enabled and double_first_layer"
                },
                "z_mode":
                {
                    "label": "Cach xu ly Z",
                    "description": "split = chia layer 0 thanh 2 lop mong, KHONG doi he toa do (ban in dung cao). shift = nang Z roi G92 (ban in cao hon 1 lop).",
                    "type": "enum",
                    "options":
                    {
                        "split": "Chia layer 0 thanh 2 lop (khong doi Z)",
                        "shift": "Nang Z roi G92 (cao hon 1 lop)"
                    },
                    "default_value": "split",
                    "enabled": "enabled and double_first_layer"
                },
                "first_pass_z":
                {
                    "label": "Z cua pass 1 (che do split)",
                    "description": "In pass 1 o do cao nay, pass 2 tro ve Z goc cua layer 0. Phai NHO HON chieu cao lop dau; mac dinh 0.1 voi lop 0.2.",
                    "unit": "mm",
                    "type": "float",
                    "default_value": 0.1,
                    "minimum_value": 0.01,
                    "enabled": "enabled and double_first_layer and z_mode == 'split'"
                },
                "z_raise":
                {
                    "label": "Z to raise to before pass 2 (che do shift)",
                    "description": "Z tuyet doi ma nozzle di toi truoc khi in lai layer 0.",
                    "unit": "mm",
                    "type": "float",
                    "default_value": 0.4,
                    "minimum_value": 0,
                    "enabled": "enabled and double_first_layer and z_mode == 'shift'"
                },
                "layer0_z":
                {
                    "label": "Z to declare after the raise (che do shift)",
                    "description": "G92 Z<so nay> sau khi nang. PHAI <= Z nang, neu khong he toa do se dich XUONG va nozzle dam vao ban.",
                    "unit": "mm",
                    "type": "float",
                    "default_value": 0.2,
                    "minimum_value": 0,
                    "enabled": "enabled and double_first_layer and z_mode == 'shift'"
                },
                "z_feedrate":
                {
                    "label": "Z feedrate",
                    "description": "Feedrate cho buoc di chuyen Z giua hai pass.",
                    "unit": "mm/min",
                    "type": "int",
                    "default_value": 600,
                    "minimum_value": 1,
                    "enabled": "enabled and double_first_layer"
                }
            }
        }"""

    # ------------------------------------------------------------------ #
    # helpers
    # ------------------------------------------------------------------ #

    # Cac lenh dat nhiet do. 'S' la gia tri dich, 'R' la ban "cho nguoi" cua M109/M190.
    TEMP_COMMANDS = {
        "M104": "hotend",
        "M109": "hotend",
        "M140": "bed",
        "M190": "bed",
    }

    def _force_temperatures(self, chunk, hotend, bed):
        """Ep nhiet do trong mot chunk.

        Quy tac quan trong: lenh TAT nhiet (S0 / R0) duoc giu nguyen. Ep chung ve
        230 se bat lai hotend ngay tai buoc ket thuc in.
        """
        out = []
        for line in chunk.split("\n"):
            code = line.split(";", 1)[0].strip()
            if not code:
                out.append(line)
                continue

            which = self.TEMP_COMMANDS.get(code.split()[0])
            if which is None:
                out.append(line)
                continue

            key = "S"
            value = self.getValue(line, "S")
            if value is None:
                key = "R"
                value = self.getValue(line, "R")
            if value is None:
                out.append(line)
                continue

            if float(value) <= 0:
                out.append(line)       # tat nhiet -> KHONG bat lai
                continue

            target = hotend if which == "hotend" else bed
            kwargs = {key: target}
            out.append(self.putValue(line, **kwargs))

        return "\n".join(out)

    def _find_layer_z(self, body):
        """Z cua layer 0: gia tri Z NHO NHAT trong body.

        KHONG duoc lay Z dau tien. Profile Voron cua Ultimaker bat Z-hop
        (voron2_base.def.json: retraction_hop_enabled = true, retraction_hop = 0.2),
        nen buoc G0/G1 co Z dau tien cua layer 0 la buoc NANG len (chieu cao layer
        + hop), khong phai chieu cao layer. Layer 0 cao 0.2 thi body mo dau bang:

            G1 F600 Z0.4     <- Z-hop (0.2 + 0.2)
            G0 ... X.. Y..   <- di chuyen o do cao hop
            ;TYPE:SKIRT
            G1 F600 Z0.2     <- ha ve dung chieu cao layer 0

        Lay Z dau tien se ra 0.4 -> offset = 0.4 - first_pass_z = 0.3 (dung phai
        la 0.1), va khi tru 0.3 vao MOI Z thi chieu cao that 0.2 thanh -0.1. Marlin
        co Z_MIN_POS = 0 nen kep ve 0 (motion.cpp: NOLESS(target.z, soft_endstop.min.z))
        -> pass 1 in ngay tren mat ban.

        Trong mot layer, moi Z-hop deu CAO HON chieu cao layer, nen min() luon tra
        ve dung chieu cao layer.
        """
        zs = []
        for line in body:
            code = line.split(";", 1)[0].strip()
            if not code:
                continue
            if code.split()[0] not in ("G0", "G1"):
                continue
            value = self.getValue(line, "Z")
            if value is not None:
                zs.append(float(value))
        return min(zs) if zs else None

    def _shift_z(self, lines, offset):
        """Tru offset khoi MOI gia tri Z (giu nguyen Z-hop, vi cung tru mot hang so)."""
        out = []
        for line in lines:
            code = line.split(";", 1)[0].strip()
            if not code or code.split()[0] not in ("G0", "G1"):
                out.append(line)
                continue
            value = self.getValue(line, "Z")
            if value is None:
                out.append(line)
                continue
            out.append(self.putValue(line, Z="{0:.5f}".format(float(value) - offset)))
        return out

    def _find_layer(self, data, number):
        """Tim chunk chua ';LAYER:<number>'. Tra ve (index, offset dong) hoac (None, None)."""
        want = ";LAYER:{0}".format(number)
        for index, chunk in enumerate(data):
            for offset, line in enumerate(chunk.split("\n")):
                if line.strip() == want:
                    return index, offset
        return None, None

    def _scan_mode_and_e(self, chunks):
        """quet cac chunk truoc do -> (dang E tuong doi?, gia tri E cuoi cung)"""
        relative = False
        e = 0.0
        for chunk in chunks:
            for line in chunk.split("\n"):
                code = line.split(";", 1)[0].strip()
                if not code:
                    continue
                word = code.split()[0]
                if word == "M83":
                    relative = True
                    continue
                if word == "M82":
                    relative = False
                    continue
                if word == "G92":
                    value = self.getValue(line, "E")
                    if value is not None:
                        e = float(value)
                    continue
                if word in ("G0", "G1"):
                    value = self.getValue(line, "E")
                    if value is not None:
                        e = float(value)
        return relative, e

    def _rescale(self, body, e_orig_start, e_new_start, factor, relative):
        """Nhan luong nhua cua mot lan in.

        Can HAI bo dem: E goc (gia tri trong file, dung de tinh delta) va E moi
        (gia tri se ghi ra). Khong the dung chung mot bien, vi sau lan nhan dau
        tien delta se duoc tinh tren gia tri da bi nhan.

        Pass 2 chay lai dung body do, nen E goc xuat phat tu CUNG mot diem nhu
        pass 1; chi E moi la tiep noi tu cuoi pass 1 (vi dau extruder dang o do).

        Tra ve (danh sach dong moi, E goc cuoi, E moi cuoi).
        """
        out = []
        e_orig = e_orig_start
        e_new = e_new_start
        for line in body:
            code = line.split(";", 1)[0].strip()
            if not code:
                out.append(line)
                continue

            word = code.split()[0]

            if word == "G92":
                value = self.getValue(line, "E")
                if value is not None:
                    e_orig = float(value)
                    e_new = float(value)
                out.append(line)
                continue

            if word not in ("G0", "G1"):
                out.append(line)
                continue

            value = self.getValue(line, "E")
            if value is None:
                out.append(line)
                continue

            value = float(value)
            if relative:
                delta = value
                e_orig = value
            else:
                delta = value - e_orig
                e_orig = value

            # Chi nhan khi buoc co di chuyen XY (dang do nhua thuc su).
            # Buoc thuan E la retract/unretract -> giu nguyen.
            if delta > 0 and ("X" in code or "Y" in code):
                delta *= factor

            e_new += delta
            out.append(self.putValue(line, E="{0:.5f}".format(e_new)))

        return out, e_orig, e_new

    # ------------------------------------------------------------------ #

    def execute(self, data):
        if not self.getSettingValueByKey("enabled"):
            return data

        # --- 1) Ep nhiet do cho TOAN BO file, ke ca khoi init va khoi ket thuc ---
        # Lam truoc va doc lap voi phan layer 0: Cura van phat M104/M140 rieng khi
        # nhiet do layer 0 khac cac layer sau (theo vat lieu), nen Start G-code
        # thoi khong du de giu 230/60.
        if self.getSettingValueByKey("force_temperatures"):
            hotend = int(self.getSettingValueByKey("hotend_temp"))
            bed = int(self.getSettingValueByKey("bed_temp"))
            data = [self._force_temperatures(chunk, hotend, bed) for chunk in data]

        if not self.getSettingValueByKey("double_first_layer"):
            return data

        index, offset = self._find_layer(data, 0)
        if index is None:
            return data

        factor1 = float(self.getSettingValueByKey("pass1_flow")) / 100.0
        factor2 = float(self.getSettingValueByKey("pass2_flow")) / 100.0
        z_mode = self.getSettingValueByKey("z_mode")
        z_feedrate = int(self.getSettingValueByKey("z_feedrate"))

        lines = data[index].split("\n")
        prefix = lines[:offset + 1]          # ... dong ';LAYER:0'
        body = lines[offset + 1:]

        relative, e_at_layer0 = self._scan_mode_and_e(data[:index])

        layer_z = self._find_layer_z(body)

        # ---- pass 1 ----
        pass1, _e_orig, e_after_pass1 = self._rescale(body, e_at_layer0, e_at_layer0, factor1, relative)

        jump = [""]

        if z_mode == "split":
            # Chia layer 0 thanh 2 lop mong. KHONG dung G92: chi ha Z cua pass 1
            # xuong, roi pass 2 in lai o dung Z goc. He toa do khong doi nen ban
            # in khong cao hon mo hinh.
            first_pass_z = float(self.getSettingValueByKey("first_pass_z"))
            if layer_z is not None and 0 < first_pass_z < layer_z:
                pass1 = self._shift_z(pass1, layer_z - first_pass_z)
                jump += [
                    "; --- First Layer Twice: ket thuc pass 1 (Z{0}) ---".format(first_pass_z),
                    "M400 ; doi in xong lop thu nhat",
                    "G90 ; toa do tuyet doi",
                    "G1 Z{0} F{1} ; len lai Z goc cua layer 0".format(layer_z, z_feedrate),
                    "; --- First Layer Twice: bat dau pass 2 (Z{0}, {1:.0f}%) ---".format(layer_z, factor2 * 100),
                ]
            else:
                # Tham so khong hop le -> hai pass cung do cao, an toan (khong ha Z,
                # khong dung G92).
                jump += [
                    "; --- First Layer Twice: first_pass_z khong hop le (Z layer 0 = {0}), hai pass cung do cao ---".format(layer_z),
                    "M400",
                    "G90",
                    "; --- First Layer Twice: bat dau pass 2 ({0:.0f}%) ---".format(factor2 * 100),
                ]
        else:
            z_raise = float(self.getSettingValueByKey("z_raise"))
            layer0_z = float(self.getSettingValueByKey("layer0_z"))
            jump += [
                "; --- First Layer Twice: ket thuc pass 1 ({0:.0f}%) ---".format(factor1 * 100),
                "M400 ; doi in xong lop thu nhat",
                "G90 ; toa do tuyet doi",
                "G1 Z{0} F{1} ; nang Z truoc khi in lai layer 0".format(z_raise, z_feedrate),
            ]
            if layer0_z <= z_raise:
                jump.append("G92 Z{0} ; khai bao lai day la Z cua layer 0".format(layer0_z))
            else:
                # Dich he toa do XUONG se day nozzle vao ban -> bo qua G92.
                jump.append("; CANH BAO: layer0_z ({0}) > z_raise ({1}) -> BO QUA G92 de an toan".format(layer0_z, z_raise))
            jump.append("; --- First Layer Twice: bat dau pass 2 ({0:.0f}%) ---".format(factor2 * 100))

        pass2, _e_orig2, e_after_pass2 = self._rescale(body, e_at_layer0, e_after_pass1, factor2, relative)

        data[index] = "\n".join(
            prefix
            + ["; --- First Layer Twice: pass 1 ({0:.0f}%) ---".format(factor1 * 100)]
            + pass1
            + jump
            + pass2
        )
        return data
