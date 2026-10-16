#!/usr/bin/env bash
# ===========================================================================
#  cydlcd/tools/run_sim.sh
#
#  Bản LINUX của cydlcd/tools/run_sim.ps1 (bản gốc chỉ chạy trên Windows:
#  dùng .exe, ar.exe, '\', Get-ChildItem, $LASTEXITCODE).
#
#  Build + chạy bản GIẢ LẬP UI màn hình cydlcd NGAY TRÊN PC (không cần board,
#  không cần WiFi): build ĐÚNG các file `cydlcd/src/ui_*.cpp` của firmware,
#  chỉ thay lớp phần cứng bằng stub (sim/sim_port.cpp, sim/sim_stubs.cpp).
#  Nhờ vậy bố cục, màu, font, icon, menu, chuỗi đa ngôn ngữ... đều là THẬT.
#
#  Dùng:
#      cydlcd/tools/run_sim.sh                        # kịch bản mặc định: chụp ảnh MỌI trang
#      cydlcd/tools/run_sim.sh --script <file>        # chạy kịch bản từ file
#      cydlcd/tools/run_sim.sh --old-xyz --script ... # bố cục X/Y/Z CŨ (checkxyz PHẢI báo lỗi)
#      cydlcd/tools/run_sim.sh --rebuild              # build lại từ đầu (kể cả LVGL)
#      cydlcd/tools/run_sim.sh --no-build             # chỉ chạy, không build
#      cydlcd/tools/run_sim.sh --fetch-lvgl           # tự tải nguồn LVGL nếu chưa có
#      cydlcd/tools/run_sim.sh -j 8                   # số job build song song (mặc định: nproc)
#
#  Ảnh .bmp ghi vào cydlcd/sim/out/ (đổi bằng --out <dir>).
#  Exit code = SỐ KIỂM TRA TỰ ĐỘNG THẤT BẠI (0 = đạt) — giống bản PowerShell,
#  nên dùng được trong CI.
#
#  Cần: g++/gcc/ar + nguồn LVGL. LVGL có sẵn sau khi build env CYD một lần:
#      pio run -d cydlcd -e cydlcd_esp32     # PlatformIO tải LVGL vào cydlcd/.pio/libdeps/
#  hoặc để script tự tải:   cydlcd/tools/run_sim.sh --fetch-lvgl
#  Đổi chỗ LVGL bằng biến môi trường LVGL_DIR, đổi bản bằng LVGL_VER.
# ===========================================================================
set -uo pipefail

SCRIPT=""
OUT=""
REBUILD=0
NOBUILD=0
OLDXYZ=0
FETCH_LVGL=0
JOBS="$(nproc 2>/dev/null || echo 4)"
# GHIM DUNG phien ban LVGL ma board dang chay (platformio.ini: lvgl/lvgl@8.4.0). Truoc day ghi
# 8.3.11 -> ban gia lap va board dung HAI ban LVGL khac nhau, co the lech bo cuc/mau ma kho lan ra.
LVGL_VER="${LVGL_VER:-8.4.0}"

usage() {
  cat <<'EOF'
cydlcd/tools/run_sim.sh — giả lập UI màn hình cydlcd trên PC (Linux)

  --script FILE     chạy kịch bản từ FILE (mặc định: kịch bản dựng sẵn trong sim_main)
  --out DIR         thư mục ghi ảnh .bmp (mặc định: cydlcd/sim/out)
  --old-xyz         build với bố cục X/Y/Z CŨ, để chứng minh checkxyz bắt được lỗi
  --rebuild         build lại từ đầu, kể cả LVGL
  --no-build        chỉ chạy, không build
  --fetch-lvgl      tự tải nguồn LVGL nếu chưa có
  -j N              số job build song song (mặc định: số CPU)
  -h, --help        in trợ giúp này

Exit code = số kiểm tra tự động thất bại (0 = đạt).
EOF
}

die() { echo "LỖI: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
  case "$1" in
    -s|--script)   [ $# -ge 2 ] || die "--script cần đường dẫn"; SCRIPT="$2"; shift 2;;
    -o|--out)      [ $# -ge 2 ] || die "--out cần đường dẫn";    OUT="$2";    shift 2;;
    -j|--jobs)     [ $# -ge 2 ] || die "-j cần số";              JOBS="$2";   shift 2;;
    --rebuild)     REBUILD=1;    shift;;
    --no-build)    NOBUILD=1;    shift;;
    --old-xyz)     OLDXYZ=1;     shift;;
    --fetch-lvgl)  FETCH_LVGL=1; shift;;
    -h|--help)     usage; exit 0;;
    -*)            echo "Tùy chọn lạ: $1" >&2; usage >&2; exit 2;;
    *)             SCRIPT="$1"; shift;;   # cho phép: run_sim.sh <file-kich-ban>
  esac
done

SELF_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SELF_DIR/../.." && pwd)"
CDIR="$ROOT/cydlcd"
SRC="$CDIR/src"
SIM="$CDIR/sim"

# .build/ dùng chung cho LVGL (không phụ thuộc bố cục UI); biến thể -OldXyz phải
# build lại từ đầu nên dùng thư mục object RIÊNG.
BLDLIB="$SIM/.build"
BLD="$SIM/.build"
[ "$OLDXYZ" = 1 ] && BLD="$SIM/.build-oldxyz"

LVGL="${LVGL_DIR:-$CDIR/.pio/libdeps/cydlcd_esp32/lvgl}"
EXE="$BLD/sim_ui"
[ -z "$OUT" ] && OUT="$SIM/out"

GXX="$(command -v g++ || true)"
GCC="$(command -v gcc || true)"
AR="$(command -v ar  || true)"
[ -n "$GXX" ] || die "Không tìm thấy g++. Cài: sudo apt-get install -y g++"
[ -n "$GCC" ] || die "Không tìm thấy gcc. Cài: sudo apt-get install -y gcc"
[ -n "$AR"  ] || die "Không tìm thấy ar (gói binutils)."

# --- nguồn LVGL ------------------------------------------------------------
if [ "$FETCH_LVGL" = 1 ] && [ ! -f "$LVGL/lvgl.h" ]; then
  echo "Tải nguồn LVGL v$LVGL_VER ..."
  mkdir -p "$(dirname "$LVGL")"
  TMPD="$(mktemp -d)"
  curl -fsSL "https://github.com/lvgl/lvgl/archive/refs/tags/v${LVGL_VER}.tar.gz" \
       -o "$TMPD/lvgl.tar.gz" || die "Tải LVGL thất bại"
  tar xzf "$TMPD/lvgl.tar.gz" -C "$TMPD" || die "Giải nén LVGL thất bại"
  rm -rf "$LVGL"
  mv "$TMPD/lvgl-$LVGL_VER" "$LVGL" || die "Không đặt được LVGL vào $LVGL"
  rm -rf "$TMPD"
  echo "  -> $LVGL"
fi
if [ ! -f "$LVGL/lvgl.h" ]; then
  die "Chưa có nguồn LVGL ở:
    $LVGL
  Lấy bằng một trong hai cách:
    pio run -d cydlcd -e cydlcd_esp32      # PlatformIO tải LVGL về
    $0 --fetch-lvgl                         # script tự tải"
fi

mkdir -p "$BLDLIB" "$BLD" "$OUT"

# --- cầu nối Windows -> Linux ----------------------------------------------
# Mã nguồn viết trên Windows nên dựa vào 2 thứ chỉ Windows mới có:
#   1) include "CYDLCD_config.h" — SAI hoa/thường so với file thật
#      "cydlcd_config.h". Windows không phân biệt hoa/thường nên chạy được,
#      Linux thì không. Bốn file gfx4_font_*.c là file SINH TỰ ĐỘNG
#      (tools/make_font.py, tools/ttf2gfxfont.py) nên sửa thẳng vào chúng sẽ bị
#      lần sinh sau ghi đè -> thay vào đó dựng header trỏ về bản thật.
#   2) <direct.h> + _mkdir(path) — Linux dùng <sys/stat.h> + mkdir(path, mode).
# Thư mục này nằm trên đường -I nên KHÔNG đụng tới mã nguồn firmware.
COMPAT="$BLDLIB/compat"
mkdir -p "$COMPAT"
cat > "$COMPAT/CYDLCD_config.h" <<'EOF'
/* Cầu nối Linux: Windows không phân biệt hoa/thường nên
   #include "CYDLCD_config.h" chạy được; Linux thì không. */
#pragma once
#include "cydlcd_config.h"
EOF
cat > "$COMPAT/direct.h" <<'EOF'
/* Shim Linux: Windows có <direct.h> với _mkdir(path) một tham số. */
#pragma once
#if defined(_WIN32)
#  include <direct.h>
#else
#  include <sys/stat.h>
#  include <sys/types.h>
#  include <errno.h>
static inline int _mkdir(const char *path) {
  if (mkdir(path, 0755) == 0) return 0;
  return (errno == EEXIST) ? 0 : -1;   /* đã tồn tại = coi như xong */
}
#endif
EOF

# --- cờ build (giống run_sim.ps1) ------------------------------------------
DEFS=(
  '-DCYDLCD_HOST_TEST=1'
  '-DCYDLCD_BOARD_F407'
  '-DCYDLCD_USE_LVGL=1'
  '-DLV_CONF_PATH=lv_conf_sim.h'
  '-DUI_MONO_FONT=4'
)
if [ "$OLDXYZ" = 1 ]; then
  # Bố cục CŨ: hộp giá trị = [LX .. LX+58] (58/58/82) -> số bắt đầu NGAY tại nhãn.
  echo "CHẾ ĐỘ --old-xyz: dùng bố cục X/Y/Z CŨ (để kiểm tra bẫy kiểm thử)"
  DEFS+=('-DMR_XYZ_VX(i)=MR_XYZ_LX(i)')
  DEFS+=('-DMR_XYZ_VR(i)=(MR_XYZ_LX(i)+((i)==0?58:((i)==1?58:82)))')
  DEFS+=('-DMR_XYZ_VW(i)=(MR_XYZ_VR(i)-MR_XYZ_VX(i))')
fi
INCS=("-I$SIM/shim" "-I$COMPAT" "-I$SIM" "-I$SRC" "-I$LVGL")  # shim/ + compat/ phải TRƯỚC
CFLAGS=(-O1 -w -std=gnu11 "${DEFS[@]}" "${INCS[@]}")
CXXFLAGS=(-O1 -w -std=gnu++17 -fno-exceptions -fno-rtti "${DEFS[@]}" "${INCS[@]}")

# Biên dịch song song.
# KHÔNG dùng `xargs -I{} bash -c '<chuỗi lệnh>'`: xargs tự diễn giải backslash/quote
# trong input, nên `\(` do `printf %q` sinh ra bị ăn mất -> bash nhận ngoặc trần và báo
# "syntax error near unexpected token `('". Cách đúng: sinh một script thợ chứa sẵn MẢNG
# cờ (không phải chuỗi), rồi truyền tham số trực tiếp qua `xargs -0` — ở chế độ null mọi
# ký tự là nguyên văn, không xử lý quote/backslash nữa.
WORKER="$BLD/.cc-worker.sh"
{
  echo '#!/usr/bin/env bash'
  echo 'set -u'
  printf 'CFLAGS=( ';  printf '%q ' "$GCC" "${CFLAGS[@]}";  echo ' )'
  printf 'CXXFLAGS=( '; printf '%q ' "$GXX" "${CXXFLAGS[@]}"; echo ' )'
  cat <<'WORKER_EOF'
kind="$1"; src="$2"; obj="$3"
case "$kind" in
  c)   "${CFLAGS[@]}"   -c "$src" -o "$obj" ;;
  cxx) "${CXXFLAGS[@]}" -c "$src" -o "$obj" ;;
  *)   echo "loại file lạ: $kind" >&2; exit 2 ;;
esac
WORKER_EOF
} > "$WORKER"
chmod +x "$WORKER"

# Ghi 1 job (loại, nguồn, object) phân tách bằng NUL; xargs -0 -n3 đọc thành từng bộ 3.
job_emit() { printf '%s\0%s\0%s\0' "$1" "$2" "$3" >> "$JOBFILE"; }

run_jobs() {
  local label="$1" n="$2"
  [ "$n" -eq 0 ] && return 0
  echo "  $n file cần biên dịch ($JOBS job song song)..."
  xargs -0 -n3 -P "$JOBS" "$WORKER" < "$JOBFILE" || die "Biên dịch $label lỗi"
}

# --- 1) LVGL (C) — build 1 lần rồi cache (LVGL không đổi khi sửa UI) --------
LVGLLIB="$BLDLIB/liblvgl.a"
NEED_LVGL=0
[ "$REBUILD" = 1 ] && NEED_LVGL=1
[ -f "$LVGLLIB" ] || NEED_LVGL=1

if [ "$NOBUILD" = 0 ] && [ "$NEED_LVGL" = 1 ]; then
  echo "Biên dịch LVGL (lần đầu, hơi lâu)..."
  JOBFILE="$BLDLIB/.jobs"; : > "$JOBFILE"; N=0
  while IFS= read -r f; do
    rel="${f#"$LVGL"/}"
    o="$BLDLIB/lvgl_$(printf '%s' "$rel" | tr '/:' '__').o"
    if [ "$REBUILD" = 1 ] || [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then
      job_emit c "$f" "$o"
      N=$((N+1))
    fi
  done < <(find "$LVGL/src" -name '*.c' | sort)
  run_jobs "LVGL" "$N"
  echo "  đóng gói liblvgl.a..."
  rm -f "$LVGLLIB"
  find "$BLDLIB" -maxdepth 1 -name 'lvgl_*.o' -print0 | sort -z | xargs -0 "$AR" rcs "$LVGLLIB" \
    || die "ar liblvgl.a lỗi"
  echo "  -> xong ($(find "$BLDLIB" -maxdepth 1 -name 'lvgl_*.o' | wc -l) object)"
fi

# --- 2) Code THẬT của firmware (UI + link + tramming) + file của bản giả lập
SOURCES=(
  "$SRC/cydlcd_link.cpp"
  "$SRC/tbt_link.cpp"
  "$SRC/lv_font_gfxfont.c"
  "$SRC/gfx4_font_icons.c"          # font icon 4bpp (UI_MONO_FONT 4)
  "$SRC/gfx4_font_icons_big.c"
  "$SRC/gfx4_font_icons_title.c"    # ban 20 px rieng cho thanh tieu de (2026-10-16)
  "$SRC/gfx4_font_text.c"
  "$SRC/gfx_font_robotomono.c"
  "$SIM/sim_port.cpp"
  "$SIM/sim_stubs.cpp"
  "$SIM/sim_net_stubs.cpp"
  "$SIM/sim_main.cpp"
)
# mọi ui_*.cpp của firmware, TRỪ ui_port_* (2 file đó dùng TFT_eSPI thật)
while IFS= read -r f; do
  [ -n "$f" ] && SOURCES+=("$f")
done < <(find "$SRC" -maxdepth 1 -name 'ui_*.cpp' ! -name 'ui_port_*' | sort)

for f in "${SOURCES[@]}"; do
  [ -f "$f" ] || die "Thiếu file nguồn: $f"
done

if [ "$NOBUILD" = 0 ]; then
  echo "Biên dịch UI (${#SOURCES[@]} file)..."
  JOBFILE="$BLD/.jobs"; : > "$JOBFILE"; N=0
  for f in "${SOURCES[@]}"; do
    o="$BLD/$(basename "$f").o"
    if [ "$REBUILD" = 1 ] || [ ! -f "$o" ] || [ "$f" -nt "$o" ]; then
      case "$f" in
        *.c) job_emit c   "$f" "$o";;
        *)   job_emit cxx "$f" "$o";;
      esac
      N=$((N+1))
    fi
  done
  run_jobs "UI" "$N"

  echo "Liên kết..."
  OBJS=()
  for f in "${SOURCES[@]}"; do OBJS+=("$BLD/$(basename "$f").o"); done
  "$GXX" "${OBJS[@]}" "$LVGLLIB" -o "$EXE" -lm || die "Liên kết lỗi"
fi
[ -x "$EXE" ] || die "Chưa có $EXE — bỏ --no-build hoặc build trước."

# --- 3) Chạy ---------------------------------------------------------------
RUN=(--out "$OUT")
[ -n "$SCRIPT" ] && RUN+=("$SCRIPT")
echo "Chạy bản giả lập: $EXE ${RUN[*]}"
"$EXE" "${RUN[@]}"
rc=$?

echo "Ảnh màn hình: $OUT"
if [ "$rc" -eq 0 ]; then
  echo "Kiểm tra tự động: TẤT CẢ ĐẠT (exit 0)"
else
  echo "Kiểm tra tự động: $rc mục THẤT BẠI (exit $rc)"
fi
exit "$rc"
