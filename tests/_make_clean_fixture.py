"""One-off: dung file gcode 'raw' SACH (3 layer) de do buoc Z giua cac layer."""
import io

L = [";FLAVOR:Marlin", ";Layer height: 0.2", ";MINZ:0.2", ";MAXZ:0.6",
     ";POSTPROCESSED", ";Generated with Cura_SteamEngine 5.13.0",
     "M140 S60", "M104 S230", "G28", "G92 E0", "M82",
     # start gcode + purge nam CUNG chunk voi ;LAYER:0 (giong Cura that)
     "G1 Z2.0 F3000", "G1 X2 Y10 F5000", "G1 Z0.3 F600", "G1 X2 Y100 F1500 E15",
     "G92 E0", "G1 Z2.0 F3000", "G1 F900 E-0.75",
     ";LAYER_COUNT:3",
     ";LAYER:0",
     "M107", "M204 S500",
     "G1 F600 Z0.4",                 # Z-hop 0.2 tren chieu cao layer
     "G0 F6750 X138.675 Y138.232",
     ";TYPE:SKIRT",
     "G1 F600 Z0.2",                 # chieu cao THAT cua layer 0
     "G1 F900 E0",
     "G1 F1800 X140 Y140 E1",
     "G1 X150 Y150 E2",
     "G0 F600 Z0.4",                 # Z-hop cuoi layer
     "G0 F6750 X10 Y10",
     ";LAYER:1",
     "M106 S85", "M204 S500",
     "G1 F600 Z0.6",                 # layer 1 = 0.4 + hop 0.2
     ";TYPE:SKIRT",
     "G1 F600 Z0.4",                 # chieu cao THAT cua layer 1
     "G1 F900 E2",
     "G1 F1800 X160 Y160 E3",
     "G0 F600 Z0.6",
     ";LAYER:2",
     "M106 S85", "M204 S500",
     "G1 F600 Z0.8",
     ";TYPE:SKIRT",
     "G1 F600 Z0.6",
     "G1 F900 E3",
     "G1 F1800 X170 Y170 E4",
     "",
     ";End of Gcode",
     ""]

io.open(r"D:\0in\clean_raw.gcode", "w", encoding="utf-8", newline="\n").write("\n".join(L))
print("ghi D:\\0in\\clean_raw.gcode ({0} dong)".format(len(L)))
