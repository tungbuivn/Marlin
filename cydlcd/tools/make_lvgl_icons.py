#!/usr/bin/env python3
"""make_lvgl_icons.py -- chuyen bitmap GREYSCALE4 cua Marlin (UI TFT ILI9341) sang
LVGL 8.3 ALPHA_4BIT de cydlcd dung lai DUNG icon cua man hinh Marlin.

Vi sao phai doi muc xam:
  Marlin (TFT_Queue::add_image, Marlin/src/lcd/tft/tft_queue.cpp) dung bang mau 15 muc:
      muc 0..7  : gradient(background -> shadow)   (vien toi / bong)
      muc 8..15 : gradient(main -> background)     (muc 8 = nen, 15 = mau chinh)
  LVGL ALPHA_4BIT chi co 1 mau (recolor) + do trong suot (0..15 -> 0..255), nen:
      muc <= 8  -> 0        (trong suot = nen; lop bong 0..7 gan nhu khong thay tren nen toi)
      muc 9..15 -> (muc-8)*15/7   (tam 0..15 -> dung bang ramp 8..15 cua Marlin)
  => icon hien ra giong het ban Marlin khi recolor = mau main va nen = COLOR_BACKGROUND.

Cach dung (chay tu thu muc goc repo):
    python cydlcd/tools/make_lvgl_icons.py
Ket qua: cydlcd/src/ui_lvgl_icons.h
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]          # ...\<repo>
IMG_DIR = ROOT / "Marlin" / "src" / "lcd" / "tft" / "images"
OUT = Path(__file__).resolve().parents[1] / "src" / "ui_lvgl_icons.h"

# (ten file .cpp cua Marlin, ten bien trong file, ten symbol xuat ra, rong, cao)
# Luu y: mot vai file chua 2 mang (vd heated_bed_64x64x4.cpp co ca bed_64x64x4).
ICONS = [
    ("hotend_64x64x4.cpp",      "hotend_64x64x4",      "ui_icon_hotend",     64, 64),
    ("heated_bed_64x64x4.cpp",  "bed_64x64x4",         "ui_icon_bed",        64, 64),
    ("heated_bed_64x64x4.cpp",  "bed_heated_64x64x4",  "ui_icon_bed_heated", 64, 64),
    ("fan_64x64x4.cpp",         "fan0_64x64x4",        "ui_icon_fan",        64, 64),
    ("fan_slow_64x64x4.cpp",    "fan_slow0_64x64x4",   "ui_icon_fan_slow",   64, 64),
    ("fan_fast_64x64x4.cpp",    "fan_fast0_64x64x4",   "ui_icon_fan_fast",   64, 64),
    ("sd_64x64x4.cpp",          "sd_64x64x4",          "ui_icon_sd",         64, 64),
    ("settings_64x64x4.cpp",    "settings_64x64x4",    "ui_icon_settings",   64, 64),
    ("feedrate_32x32x4.cpp",    "feedrate_32x32x4",    "ui_icon_feedrate",   32, 32),
    ("flowrate_32x32x4.cpp",    "flowrate_32x32x4",    "ui_icon_flowrate",   32, 32),
]


def read_bytes(path: Path, symbol: str, expect: int) -> bytes:
    """Doc mang `const uint8_t <symbol>[N] = { 0x.., ... };` trong file .cpp cua Marlin."""
    txt = path.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"\b" + re.escape(symbol) + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", txt, re.S)
    if not m:
        # Marlin co file dat ten bien khac ten file -> bao ro de con sua bang tay
        raise SystemExit(f"Khong tim thay mang '{symbol}' trong {path.name}")
    data = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", m.group(1)))
    if len(data) != expect:
        raise SystemExit(f"{path.name}: {len(data)} byte, mong doi {expect}")
    return data


def level_map(byte: int) -> int:
    """Doi 2 muc xam 4-bit cua Marlin thanh 2 muc alpha 4-bit cua LVGL."""
    out = 0
    for shift in (4, 0):                      # nibble cao truoc (LVGL: pixel trai = nibble cao)
        lv = (byte >> shift) & 0x0F
        a = 0 if lv <= 8 else ((lv - 8) * 15 + 3) // 7
        out |= (a & 0x0F) << shift
    return out


def main() -> int:
    lines = [
        "/**",
        " * ui_lvgl_icons.h -- icon 64x64/32x32 cua man hinh TFT ILI9341 trong Marlin,",
        " * chuyen sang LVGL 8.3 ALPHA_4BIT (recolor bang mau trang thai).",
        " *",
        " * FILE NAY DUOC SINH TU DONG -- dung sua tay:",
        " *     python cydlcd/tools/make_lvgl_icons.py",
        " * Nguon: Marlin/src/lcd/tft/images/ (dinh dang GREYSCALE4) -- xem script de biet bang",
        " * doi muc xam (muc 8..15 cua Marlin -> alpha 0..15 cua LVGL; muc 0..7 = lop bong -> trong suot).",
        " *",
        " * Dung: lv_img_create() + lv_img_set_src(img, &ui_icon_hotend) +",
        " *       lv_obj_set_style_img_recolor(img, color, 0) + recolor_opa = LV_OPA_COVER",
        " */",
        "#pragma once",
        "",
        "#if CYDLCD_USE_LVGL",
        "",
        '#include <lvgl.h>',
        "",
    ]
    total = 0
    for fname, symbol, out_name, w, h in ICONS:
        path = IMG_DIR / fname
        if not path.is_file():
            raise SystemExit(f"Khong thay {path}")
        raw = read_bytes(path, symbol, w * h // 2)
        mapped = bytes(level_map(b) for b in raw)
        total += len(mapped)

        lines.append(f"// {fname} ({w}x{h}, GREYSCALE4) -> ALPHA_4BIT, {len(mapped)} byte")
        lines.append(f"static const uint8_t {out_name}_map[{len(mapped)}] = {{")
        for i in range(0, len(mapped), 16):
            row = ", ".join(f"0x{b:02X}" for b in mapped[i:i + 16])
            lines.append(f"  {row},")
        lines.append("};")
        # C++ (khac C) khong hieu designated initializer kieu C89 -> dung ngoac nhon theo thu tu:
        #   lv_img_dsc_t = { { cf, always_zero, reserved, w, h }, data_size, data }
        lines.append(f"static const lv_img_dsc_t {out_name} = {{")
        lines.append(f"  {{ LV_IMG_CF_ALPHA_4BIT, 0, 0, {w}, {h} }},")
        lines.append(f"  {len(mapped)},")
        lines.append(f"  {out_name}_map")
        lines.append("};")
        lines.append("")

    lines.append("#endif // CYDLCD_USE_LVGL")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"Da ghi {OUT} ({len(ICONS)} icon, {total} byte du lieu)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
