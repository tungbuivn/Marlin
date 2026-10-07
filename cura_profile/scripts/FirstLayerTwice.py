# FirstLayerTwice.py
#
# Cura post-processing script: in layer 0 hai lan.
#
#   pass 1 - body da HA xuong first_pass_z (mac dinh 0.1), flow rieng (20%)
#   giua   - nhac Z len first_pass_z + <Z goc cua layer 0>, roi G92 khai bao lai
#            day la <Z goc cua layer 0>
#   pass 2 - body GOC, KHONG ha, flow rieng (80%)
#   ket thuc layer 0 - tra flow ve 100%
#
# Layer 1 tro len KHONG bi dung toi.
#
# VI SAO PASS 2 DUNG BODY GOC, KHONG DUNG BODY DA HA:
#   Neu pass 2 dung lai body da ha thi toa do Z cua no la first_pass_z (0.1) trong
#   khi layer 1 cua Cura la 0.4 -> buoc nhay tu layer 0 len layer 1 thanh 0.3, tuc
#   ho 0.1 mm khong khi. Do bang tests/_z_report.py tren file sach:
#       pass1 0.1 -> pass2 0.3 (buoc 0.2 OK) -> layer1 0.6 (buoc 0.30 SAI)
#   Dung body goc (Z = 0.2) + G92 Z0.2 thi toa do tro ve dung he goc, moi buoc sau
#   deu 0.2 nhu binh thuong:
#       pass1 0.1 -> pass2 0.3 (0.2) -> layer1 0.5 (0.2) -> layer2 0.7 (0.2)
#   Doi lai: ca ban in cao hon model dung first_pass_z (0.1) vi pass 1 da in ra
#   nhua that. Khoang cach giua cac layer VAN dung.
#
# FLOW dung bang M221 (flow percentage cua Marlin) thay vi nhan lai tung gia tri E.
# Nho vay KHONG phai tinh toan E: pass 1 va pass 2 ghi ra CUNG mot gia tri E, va
# `G92 E<e_dau_layer_0>` truoc pass 2 lam cho moi buoc cua pass 2 co delta E y het
# pass 1. Marlin tu nhan E voi flow_percentage.
#
#   pass 1 @ M221 S20 -> 20% nhua
#   pass 2 @ M221 S80 -> 80% nhua
#   tong = 100% = dung mot lop binh thuong
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
                    "label": "Bat",
                    "description": "Bat/tat script nay.",
                    "type": "bool",
                    "default_value": true
                },
                "force_temperatures":
                {
                    "label": "Ep nhiet do toan file",
                    "description": "Ep MOI lenh M104/M109/M140/M190 trong ca file ve nhiet do duoi day, ke ca khoi init va khoi ket thuc.",
                    "type": "bool",
                    "default_value": true
                },
                "hotend_temp":
                {
                    "label": "Nhiet dau in (C)",
                    "description": "M104/M109 bi ep ve so nay.",
                    "type": "int",
                    "default_value": 230,
                    "minimum_value": 0
                },
                "bed_temp":
                {
                    "label": "Nhiet ban in (C)",
                    "description": "M140/M190 bi ep ve so nay.",
                    "type": "int",
                    "default_value": 60,
                    "minimum_value": 0
                },
                "double_first_layer":
                {
                    "label": "In layer 0 hai lan",
                    "description": "Bat/tat viec in layer 0 hai lan.",
                    "type": "bool",
                    "default_value": true
                },
                "pass1_flow":
                {
                    "label": "Flow lan 1 (%)",
                    "description": "M221 S<so nay> truoc pass 1.",
                    "type": "int",
                    "default_value": 20,
                    "minimum_value": 1,
                    "maximum_value": 100
                },
                "pass2_flow":
                {
                    "label": "Flow lan 2 (%)",
                    "description": "M221 S<so nay> truoc pass 2. pass1_flow + pass2_flow nen bang 100.",
                    "type": "int",
                    "default_value": 80,
                    "minimum_value": 1,
                    "maximum_value": 100
                },
                "z_feedrate":
                {
                    "label": "Toc do nhac Z (mm/phut)",
                    "description": "Feedrate cho buoc nhac Z giua hai pass.",
                    "type": "int",
                    "default_value": 600,
                    "minimum_value": 1
                },
                "z_mode":
                {
                    "label": "z_mode (khong dung nua)",
                    "description": "Giu lai chi de config cu khong loi. Script nay khong con dung den.",
                    "type": "enum",
                    "options": { "split": "split", "shift": "shift" },
                    "default_value": "split"
                },
                "first_pass_z":
                {
                    "label": "Z cua pass 1 (mm)",
                    "description": "pass 1 in o Z nay (mac dinh 0.1). pass 2 dung lai body GOC nen in o Z = so nay + Z goc cua layer 0 (vd 0.1 + 0.2 = 0.3).",
                    "type": "float",
                    "default_value": 0.1,
                    "minimum_value": 0.01
                },
                "z_raise":
                {
                    "label": "z_raise (khong dung nua)",
                    "description": "Giu lai chi de config cu khong loi. Script tu tinh tu chieu cao layer 0.",
                    "type": "float",
                    "default_value": 0.4
                },
                "layer0_z":
                {
                    "label": "layer0_z (khong dung nua)",
                    "description": "Giu lai chi de config cu khong loi.",
                    "type": "float",
                    "default_value": 0.2
                }
            }
        }"""

    # ------------------------------------------------------------------ #
    # Ep nhiet do
    # ------------------------------------------------------------------ #

    TEMP_COMMANDS = {"M104": "hotend", "M109": "hotend", "M140": "bed", "M190": "bed"}

    def _force_temperatures(self, chunk, hotend, bed):
        out = []
        for line in chunk.split("\n"):
            code = line.split(";", 1)[0].strip()
            if not code:
                out.append(line)
                continue
            word = code.split()[0].upper()
            which = self.TEMP_COMMANDS.get(word)
            if which is None:
                out.append(line)
                continue

            # Chi ep khi nhiet do duoc dat > 0: S0 / R0 la lenh TAT, phai giu nguyen.
            target = None
            value = self.getValue(line, "S")
            if value is None:
                value = self.getValue(line, "R")
            try:
                if value is not None and float(value) > 0:
                    target = hotend if which == "hotend" else bed
            except ValueError:
                target = None
            if target is None:
                out.append(line)
                continue

            out.append(self.putValue(line, **{("S" if "S" in code else "R"): target}))

        return "\n".join(out)

    # ------------------------------------------------------------------ #
    # Doc thong tin tu G-code
    # ------------------------------------------------------------------ #

    def _find_layer_z(self, body):
        """Z cua layer 0: gia tri Z NHO NHAT trong body.

        KHONG duoc lay Z dau tien: profile Voron cua Ultimaker bat Z-hop
        (voron2_base: retraction_hop_enabled = true, retraction_hop = 0.2), nen
        buoc G0/G1 co Z dau tien cua layer 0 la buoc NANG len (chieu cao layer
        + hop), khong phai chieu cao layer. Layer 0 cao 0.2 thi body mo dau bang:

            G1 F600 Z0.4     <- Z-hop (0.2 + 0.2)
            G0 ... X.. Y..   <- di chuyen o do cao hop
            ;TYPE:SKIRT
            G1 F600 Z0.2     <- ha ve dung chieu cao layer 0

        Lay Z dau tien se ra 0.4 -> tru 0.4 vao moi Z thi chieu cao that 0.2
        thanh am. Trong mot layer, moi Z-hop deu CAO HON chieu cao layer, nen
        min() luon tra ve dung chieu cao layer.
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
        """Tru offset khoi MOI gia tri Z.

        Cung tru mot hang so cho moi Z nen do cao tuong doi giua chung duoc giu
        nguyen: Z-hop van cao hon chieu cao layer dung nhu cu.
        """
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
        """Quet cac chunk truoc do -> (dang E tuong doi?, gia tri E cuoi cung)."""
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

    # ------------------------------------------------------------------ #

    def execute(self, data):
        if not self.getSettingValueByKey("enabled"):
            return data

        # --- 1) Ep nhiet do cho TOAN BO file, ke ca khoi init va khoi ket thuc ---
        # Lam truoc va doc lap voi phan layer 0: Cura van phat M104/M140 rieng khi
        # nhiet do layer 0 khac cac layer sau (theo vat lieu), nen Start G-code
        # thoi khong du het de giu 230/60.
        if self.getSettingValueByKey("force_temperatures"):
            hotend = int(self.getSettingValueByKey("hotend_temp"))
            bed = int(self.getSettingValueByKey("bed_temp"))
            data = [self._force_temperatures(chunk, hotend, bed) for chunk in data]

        if not self.getSettingValueByKey("double_first_layer"):
            return data

        index, offset = self._find_layer(data, 0)
        if index is None:
            return data

        flow1 = int(self.getSettingValueByKey("pass1_flow"))
        flow2 = int(self.getSettingValueByKey("pass2_flow"))
        z_feedrate = int(self.getSettingValueByKey("z_feedrate"))
        first_pass_z = float(self.getSettingValueByKey("first_pass_z"))

        lines = data[index].split("\n")
        prefix = lines[:offset + 1]          # ... dong ';LAYER:0'
        body = lines[offset + 1:]

        # Phai quet CA phan dau cua chinh chunk nay (start G-code + duong purge nam
        # cung chunk voi ';LAYER:0' trong file that). Neu chi quet data[:index] thi
        # se bo sot lenh retract cuoi cung truoc layer 0, va pass 2 se mat mot lan
        # unretract dung bang so do.
        relative, e_at_layer0 = self._scan_mode_and_e(data[:index] + ["\n".join(prefix)])
        layer_z = self._find_layer_z(body)

        # --- pass 1: ha Z cua body xuong first_pass_z, KHONG doi E ---
        # Khong nhan lai E: M221 lo viec do o firmware.
        do_shift = layer_z is not None and 0 < first_pass_z < layer_z
        pass1 = self._shift_z(body, layer_z - first_pass_z) if do_shift else body

        head = ["; --- First Layer Twice: bat dau pass 1 ({0}%) ---".format(flow1),
                "M221 S{0} ; flow pass 1".format(flow1)]

        # --- giua hai pass ---
        jump = ["", "; --- First Layer Twice: ket thuc pass 1 ---", "M400 ; doi in xong lop thu nhat",
                "G90 ; toa do tuyet doi"]

        if do_shift:
            # pass 1 in o first_pass_z (0.1). pass 2 dung lai BODY GOC (Z = layer_z,
            # vd 0.2), nen phai nhac Z THAT len first_pass_z + layer_z (0.1 + 0.2 =
            # 0.3) roi G92 khai bao lai day la layer_z -> body goc roi ve dung cho
            # cua no, va toa do tu day ve sau y het ban in binh thuong.
            jump += [
                "G1 Z{0} F{1} ; nang Z len first_pass_z + Z goc cua layer 0".format(
                    self._fmt_num(first_pass_z + layer_z), z_feedrate),
                "G92 Z{0} ; khai bao lai day la Z goc cua layer 0".format(
                    self._fmt_num(layer_z)),
            ]
        else:
            jump.append("; CANH BAO: first_pass_z khong hop le -> hai pass cung do cao")

        # Reset extruder ve DUNG gia tri E luc bat dau layer 0, de moi buoc cua
        # pass 2 co delta E y het pass 1 -> khong phai tinh lai E.
        jump += [
            "G92 E{0} ; reset extruder ve E dau layer 0".format(self._fmt_e(e_at_layer0)),
            "M221 S{0} ; flow pass 2".format(flow2),
            "; --- First Layer Twice: bat dau pass 2 ({0}%) ---".format(flow2),
        ]

        # --- pass 2: BODY GOC, khong ha. Xem giai thich o dau file. ---
        # Phan tu CUOI phai la chuoi rong: moi chunk cua Cura ket thuc bang '\n', ma
        # "\n".join() khong tu them dau phan cach o cuoi. Thieu dong rong nay thi
        # chunk sau (bat dau bang ';LAYER:1') se bi dinh lien vao dong cuoi.
        tail = ["", "; --- First Layer Twice: ket thuc layer 0 -> tra flow ve 100% ---",
                "M221 S100 ; flow 100% cho layer 1 tro len", ""]

        data[index] = "\n".join(prefix + head + pass1 + jump + body + tail)
        return data

    @staticmethod
    def _fmt_num(value):
        """Ghi so gon: 0.1 + 0.2 = 0.30000000000000004 -> '0.3'."""
        s = "{0:.5f}".format(value).rstrip("0").rstrip(".")
        return s if s else "0"

    @staticmethod
    def _fmt_e(value):
        """Ghi E kieu Cura: nguyen thi khong co phan thap phan."""
        if abs(value - round(value)) < 1e-9:
            return "{0:.0f}".format(int(round(value)))
        return ("{0:.5f}".format(value)).rstrip("0").rstrip(".")
