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

# Ban THU NHO cho man STATUS (2026-10-14): 2 o the SD / SETTINGS tren man STATUS chi con 48x48 de
# nhuong cho icon WiFi + chu dBm nam GIUA khoi FR/FL (xem cydlcd/src/ui_page_status.cpp: ST_TILE_SZ).
# Vi sao phai sinh san chu khong dung lv_img_set_zoom(): LVGL 8.4 CHI ho tro transform (zoom/xoay)
# cho TRUE_COLOR / TRUE_COLOR_ALPHA / CHROMA_KEYED / RGB565A8 - xem
# lvgl/src/draw/sw/lv_draw_sw_transform.c (ALPHA_4BIT roi vao `default:` -> KHONG VE GI).
# ICON cua Marlin la ALPHA_4BIT nen zoom se lam icon BIEN MAT.
# (ten file, ten bien, ten symbol xuat ra, rong nguon, cao nguon, rong dich)
SMALL = [
    ("sd_64x64x4.cpp",          "sd_64x64x4",          "ui_icon_sd_48",          64, 64, 48),
    ("settings_64x64x4.cpp",    "settings_64x64x4",    "ui_icon_settings_48",    64, 64, 48),
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


def unpack4(raw: bytes, w: int, h: int) -> list[list[int]]:
    """Giai ma mang 4bpp thanh bang 2 chieu muc alpha 0..15 (pixel trai = nibble CAO)."""
    rows = []
    for y in range(h):
        row = []
        for x in range(w):
            b = raw[(y * w + x) // 2]
            row.append((b >> 4) if (x % 2 == 0) else (b & 0x0F))
        rows.append(row)
    return rows


def pack4(rows: list[list[int]], w: int) -> bytes:
    """Dong goi bang 2 chieu muc alpha 0..15 thanh mang 4bpp (pixel trai = nibble CAO)."""
    out = bytearray()
    for row in rows:
        for x in range(0, w, 2):
            hi = row[x] & 0x0F
            lo = (row[x + 1] & 0x0F) if (x + 1) < w else 0
            out.append((hi << 4) | lo)
    return bytes(out)


def downsample(rows: list[list[int]], src_w: int, src_h: int, dst_w: int) -> list[list[int]]:
    """Thu nho bang TRUNG BINH THEO DIEN TICH (box filter): moi diem dich lay trung binh co trong so
    cua cac diem nguon phu no. 64 -> 48 dung ti le 3/4 nen mot diem dich ung ~1,33 x 1,33 diem nguon.
    Dung trung binh dien tich (khong phai nearest) de net icon khong bi rang cua / mat net manh."""
    dst_h = src_h * dst_w // src_w
    sx = src_w / dst_w                        # so diem nguon tren 1 diem dich (truc x)
    sy = src_h / dst_h
    out = []
    for dy in range(dst_h):
        y0, y1 = dy * sy, (dy + 1) * sy
        row = []
        for dx in range(dst_w):
            x0, x1 = dx * sx, (dx + 1) * sx
            acc = 0.0
            wsum = 0.0
            for syi in range(int(y0), min(int(y1 - 1e-9) + 1, src_h)):
                wy = min(y1, syi + 1) - max(y0, syi)
                if wy <= 0:
                    continue
                for sxi in range(int(x0), min(int(x1 - 1e-9) + 1, src_w)):
                    wx = min(x1, sxi + 1) - max(x0, sxi)
                    if wx <= 0:
                        continue
                    wgt = wx * wy
                    acc += rows[syi][sxi] * wgt
                    wsum += wgt
            row.append(int(round(acc / wsum)) if wsum > 0 else 0)
        out.append(row)
    return out


def outline(rows: list[list[int]], w: int, h: int) -> list[list[int]]:
    """VIEN (border) cua 1 hinh DAC: giu pixel co alpha > 0 ma co it nhat 1 trong 4 lang gieng alpha == 0
    (hoac nam sat mep anh). Dung cho icon DAU IN khi CHUA cap nguon:
    Marlin (man TFT/12864 cu) ve icon dau in DAC khi cap nguon cho hotend, va chi ve VIEN khi ngung cap
    nguon (yeu cau nguoi dung 2026-10-15: "neu marlin cap nguon cho hot end thi co bieu fill dac icon...
    den luc ngung cap nguon thi chi ve border ma khong fill"). Bitmap goc cua Marlin la hinh DAC nen phai
    SINH them ban vien ngay o day (khong the lam bang LVGL: ALPHA_4BIT khong co net/outline).
    """
    out = [[0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if rows[y][x] == 0:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nx, ny = x + dx, y + dy
                if nx < 0 or ny < 0 or nx >= w or ny >= h or rows[ny][nx] == 0:
                    out[y][x] = rows[y][x]      # giu DUNG muc alpha goc de net vien net nhu ban dac
                    break
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
    emitted = 0

    def emit(out_name: str, w: int, h: int, data: bytes, note: str) -> None:
        """Ghi 1 icon ra header (mang bitmap + lv_img_dsc_t)."""
        nonlocal total, emitted
        total += len(data)
        emitted += 1
        lines.append(f"// {note} -> ALPHA_4BIT, {len(data)} byte")
        lines.append(f"static const uint8_t {out_name}_map[{len(data)}] = {{")
        for i in range(0, len(data), 16):
            row = ", ".join(f"0x{b:02X}" for b in data[i:i + 16])
            lines.append(f"  {row},")
        lines.append("};")
        # C++ (khac C) khong hieu designated initializer kieu C89 -> dung ngoac nhon theo thu tu:
        #   lv_img_dsc_t = { { cf, always_zero, reserved, w, h }, data_size, data }
        lines.append(f"static const lv_img_dsc_t {out_name} = {{")
        lines.append(f"  {{ LV_IMG_CF_ALPHA_4BIT, 0, 0, {w}, {h} }},")
        lines.append(f"  {len(data)},")
        lines.append(f"  {out_name}_map")
        lines.append("};")
        lines.append("")

    for fname, symbol, out_name, w, h in ICONS:
        path = IMG_DIR / fname
        if not path.is_file():
            raise SystemExit(f"Khong thay {path}")
        raw = read_bytes(path, symbol, w * h // 2)
        mapped = bytes(level_map(b) for b in raw)
        emit(out_name, w, h, mapped, f"{fname} ({w}x{h}, GREYSCALE4)")
        # Ban VIEN cua icon DAU IN (xem outline()): dung khi hotend CHUA duoc cap nguon.
        if out_name == "ui_icon_hotend":
            line = pack4(outline(unpack4(mapped, w, h), w, h), w)
            emit("ui_icon_hotend_line", w, h, line,
                 f"{fname} ({w}x{h}) -> ban VIEN (hotend chua cap nguon)")

    for fname, symbol, out_name, w, h, dst_w in SMALL:
        path = IMG_DIR / fname
        if not path.is_file():
            raise SystemExit(f"Khong thay {path}")
        raw = read_bytes(path, symbol, w * h // 2)
        mapped = bytes(level_map(b) for b in raw)
        small = pack4(downsample(unpack4(mapped, w, h), w, h, dst_w), dst_w)
        emit(out_name, dst_w, h * dst_w // w, small,
             f"{fname} ({w}x{h}, GREYSCALE4) THU NHO -> {dst_w}x{h * dst_w // w}")

    lines.append("#endif // CYDLCD_USE_LVGL")
    lines.append("")

    OUT.write_text("\n".join(lines), encoding="utf-8", newline="\n")
    print(f"Da ghi {OUT} ({emitted} icon, {total} byte du lieu)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
