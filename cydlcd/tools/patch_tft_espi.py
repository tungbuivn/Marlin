# ===========================================================================
#  cydlcd/tools/patch_tft_espi.py
#
#  Chay TRUOC moi lan build env CYD. Khai bao trong cydlcd/platformio.ini:
#      extra_scripts = pre:tools/patch_tft_espi.py
#
#  ---------------------------------------------------------------------------
#  VI SAO CO — rui ro "mang sang may khac build"
#  ---------------------------------------------------------------------------
#  API chup man hinh `/screen.bmp` doc NGUOC RAM cua panel ST7789 qua TFT_eSPI, va phan
#  giai ma trong src/ui_port_cyd_st7789.cpp dua tren HANH VI CU THE cua TFT_eSPI 2.5.43:
#
#    1) TFT_Drivers/ST7789_Init.h dat COLMOD = 0x55 (16 bit/pixel), nhung
#       TFT_eSPI.cpp `readRect()`/`readRectRGB()` lai doc 3 BYTE/pixel (dinh dang 18-bit).
#       => code phai TAM doi COLMOD sang 0x66 trong luc doc.
#    2) `readRectRGB()` doc dung 1 byte "dummy" o dau, roi moi pixel 3 byte R,G,B
#       (gia tri 6 bit nam o 6 bit CAO).
#    3) Du lieu doc ve bi DICH PHAI 1 BIT (bit-slip phia panel) — bu bang `<< 1`.
#
#  `lib_deps` truoc day ghi `^2.5.43` (dau `^` cho phep 2.5.44, 2.6.x...). Mot may khac
#  build se keo BAN KHAC, va neu ban do doi duong doc thi anh chup se SAI MAU AM THAM —
#  khong compiler nao bao loi, chi nhin anh moi biet.
#
#  Script nay chan dieu do ngay luc build.
#
#  ---------------------------------------------------------------------------
#  VI SAO KHONG SUA TAY FILE THU VIEN
#  ---------------------------------------------------------------------------
#  PlatformIO quan ly `libdeps/` theo version + checksum (`integrity.dat`); sua tay vao do
#  se bi GHI DE o lan `pio pkg install` / `pio run -t clean` sau, va tren may khac thi
#  khong co ban sua do => dung lai loi cu. Cach "patch" ben vung la:
#      (a) GHIM dung version trong platformio.ini (khong dung `^`), VA
#      (b) kiem tra ma nguon thuc te khop voi gia dinh cua code — chinh la script nay.
#
#  Neu ban buoc phai doi version TFT_eSPI: chay lai `UI READTEST` tren board (console COM7)
#  SAU khi nap. No to 5 mau da biet (ke ca tong trung gian BG 0x18AD) roi chup lai va so
#  sanh — 5/5 dat thi phan giai ma con dung; lech thi phai chinh lai cong thuc trong
#  ui_port_capture_band().
# ===========================================================================

import json
import os
import sys

EXPECTED_TFT_VERSION = "2.5.43"

BAR = "=" * 74


def fail(title, lines):
    print("\n" + BAR)
    print("LOI: patch_tft_espi.py — " + title)
    print(BAR)
    for l in lines:
        print(l)
    print(BAR + "\n")
    sys.exit(1)


def read(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return None


Import("env")  # noqa: F821  (SCons/PlatformIO)

libdeps = env.subst("$PROJECT_LIBDEPS_DIR") or os.path.join(env.subst("$PROJECT_DIR"), ".pio", "libdeps")
pioenv = env.subst("$PIOENV") or os.environ.get("PIOENV", "cydlcd_esp32")
tft_dir = os.path.join(libdeps, pioenv, "TFT_eSPI")

# --- 1) thu vien co ton tai (lan build dau se cai truoc khi chay pre-script) ------------
if not os.path.isdir(tft_dir):
    # PlatformIO chay `pre:` sau khi cai lib_deps, nhung neu vi ly do nao do chua co thi
    # bao ro rang thay vi crash kho hieu.
    fail("khong tim thay TFT_eSPI",
         ["  Duong dan: " + tft_dir,
          "",
          "  Chay lai:  pio run -d cydlcd -e " + pioenv + "   (de PlatformIO tai thu vien ve)"])

# --- 2) dung phien ban ------------------------------------------------------------------
ver = None
manifest = read(os.path.join(tft_dir, "library.json"))
if manifest:
    try:
        ver = json.loads(manifest).get("version")
    except ValueError:
        ver = None

if ver != EXPECTED_TFT_VERSION:
    fail("sai phien ban TFT_eSPI",
         ["  Dang cai : " + str(ver),
          "  Can      : " + EXPECTED_TFT_VERSION,
          "",
          "  Duong doc nguoc panel trong /screen.bmp da duoc do dung voi " + EXPECTED_TFT_VERSION + ".",
          "  Ban khac co the doi so byte/pixel hoac thu tu byte => anh chup sai mau ma khong bao loi.",
          "",
          "  Cach xu ly:",
          "    - Xoa libdeps de PlatformIO tai lai dung ban ghim trong platformio.ini:",
          "        rm -rf .pio/libdeps/" + pioenv + "/TFT_eSPI",
          "    - Roi build lai. Neu van sai: kiem tra dong `bodmer/TFT_eSPI@...` trong",
          "      cydlcd/platformio.ini (KHONG duoc co dau `^`).",
          "    - Muon nang version that: sua platformio.ini, build, nap, roi chay `UI READTEST`",
          "      tren board de xac nhan 5/5 dat TRUOC khi tin ket qua /screen.bmp."])

# --- 3) ma nguon con dung gia dinh cua phan giai ma -------------------------------------
src = read(os.path.join(tft_dir, "TFT_eSPI.cpp"))
init = read(os.path.join(tft_dir, "TFT_Drivers", "ST7789_Init.h"))
if src is None or init is None:
    fail("thieu file nguon TFT_eSPI",
         ["  Khong doc duoc TFT_eSPI.cpp hoac TFT_Drivers/ST7789_Init.h trong",
          "  " + tft_dir,
          "",
          "  Chay lai:  rm -rf .pio/libdeps/" + pioenv + "/TFT_eSPI  roi build lai."])

# Cac manh ma nguon ma phan giai ma dua vao. Moi muc: (ten, file, chuoi can co, vi sao)
CHECKS = [
    ("COLMOD 16-bit trong init", init,
     "writedata(0x55)",
     "ui_port_capture_band() doi COLMOD sang 0x66 khi doc, dua tren viec init dat 0x55"),
    ("readRectRGB SPI doc 3 byte/pixel", src,
     "void  TFT_eSPI::readRectRGB",
     "phan giai ma doc BYTE THO qua readRectRGB()"),
    ("readRectRGB co dung 1 byte dummy", src,
     "  // Dummy read to throw away don't care value\n  tft_Read_8();",
     "layout byte thua o raw[0] (bo qua 1 byte dau) dua tren dung 1 lan doc dummy"),
    ("readRectRGB doc R,G,B lien tiep", src,
     "*data++ = tft_Read_8();\n    *data++ = tft_Read_8();\n    *data++ = tft_Read_8();",
     "thu tu 3 byte R6,G6,B6 moi pixel"),
]

missing = [c for c in CHECKS if c[2] not in c[1]]
if missing:
    lines = ["  Ma nguon TFT_eSPI " + EXPECTED_TFT_VERSION + " khong con khop gia dinh cua phan giai ma:",
             ""]
    for name, _f, _pat, why in missing:
        lines.append("    [THIEU] " + name)
        lines.append("            can cho: " + why)
    lines += ["",
              "  Cach xu ly:",
              "    - Rat co the thu vien da bi sua tay / tai loi. Xoa va tai lai:",
              "        rm -rf .pio/libdeps/" + pioenv + "/TFT_eSPI",
              "    - Neu dang CO Y nang version TFT_eSPI: phai doc lai duong doc panel va cap nhat",
              "      ui_port_capture_band() trong src/ui_port_cyd_st7789.cpp, roi nap va chay",
              "      `UI READTEST` tren board de xac nhan 5/5 dat."]
    fail("gia dinh ve duong doc panel khong con dung", lines)

print("[patch_tft_espi] TFT_eSPI %s: da ghim version + kiem tra ma nguon khop gia dinh "
      "(duong doc nguoc panel cho /screen.bmp)" % EXPECTED_TFT_VERSION)
