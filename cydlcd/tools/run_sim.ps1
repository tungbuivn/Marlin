# ===========================================================================
#  cydlcd/tools/run_sim.ps1
#
#  Build + chay ban GIA LAP UI man hinh cydlcd NGAY TREN PC (khong can board, khong can WiFi):
#      pwsh cydlcd/tools/run_sim.ps1                    ; chay kich ban mac dinh: chup anh MOI trang
#      pwsh cydlcd/tools/run_sim.ps1 -Script <file>     ; chay kich ban tu file (xem cydlcd/sim/README.md)
#      pwsh cydlcd/tools/run_sim.ps1 -Rebuild           ; build lai tu dau (ke ca LVGL)
#
#  VI SAO DUNG CODE THAT: script nay build DUNG cac file `cydlcd/src/ui_*.cpp` cua firmware (khong ve
#  lai UI), chi thay lop phan cung bang stub (sim/sim_port.cpp, sim/sim_stubs.cpp). Nho vay bo cuc, mau,
#  font, icon, menu, chuoi da ngon ngu... deu la THAT -> nhin anh la biet loi bo cuc (vd so de len nhan
#  X/Y/Z) ma khong phai nap board.
#
#  CAN: g++ (WinLibs/MinGW) + thu muc nguon LVGL da tai ve (co san sau khi build env cydlcd_esp32 mot lan):
#      pwsh -Command "pio run -d cydlcd -e cydlcd_esp32"
#  LVGL la C nen file .c duoc bien dich bang `gcc` (khong phai g++), UI la C++ dung `g++`.
# ===========================================================================
param(
  [string] $Script  = "",
  [string] $Out     = "",
  [switch] $Rebuild,
  [switch] $NoBuild,
  # -OldXyz: build lai voi BO CUC X/Y/Z CU (gia tri can phai NGAY TAI nhan truc - loi 2026-10-16) de
  # chung minh bay kiem thu `checkxyz` THAT SU bat duoc loi (chay: -OldXyz -> phai bao "DE NHAU!").
  [switch] $OldXyz
)
$ErrorActionPreference = 'Stop'

$root   = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)     # thu muc goc repo
$cdir   = Join-Path $root 'cydlcd'
$src    = Join-Path $cdir 'src'
$sim    = Join-Path $cdir 'sim'
# Thu muc object RIENG cho tung bien the build (-OldXyz dung bo cuc khac => PHAI build lai tu dau,
# khong duoc dung lai object cu). LVGL thi dung CHUNG (khong phu thuoc bo cuc UI).
$bldLib = Join-Path $sim  '.build'
$bld    = Join-Path $sim  ($(if ($OldXyz) { '.build-oldxyz' } else { '.build' }))
$lvgl   = Join-Path $cdir '.pio\libdeps\cydlcd_esp32\lvgl'
$exe    = Join-Path $bld  'sim_ui.exe'
if (-not $Out) { $Out = Join-Path $sim 'out' }

$gpp = (Get-Command g++ -ErrorAction SilentlyContinue).Source
$gcc = (Get-Command gcc -ErrorAction SilentlyContinue).Source
if (-not $gpp -or -not $gcc) { throw "Khong tim thay g++/gcc. Cai WinLibs: winget install BrechtSanders.WinLibs.POSIX.UCRT" }
if (-not (Test-Path (Join-Path $lvgl 'lvgl.h'))) {
  throw "Chua co nguon LVGL o $lvgl. Chay truoc: pio run -d cydlcd -e cydlcd_esp32 (de PlatformIO tai LVGL ve)."
}

New-Item -ItemType Directory -Force -Path $bld, $Out | Out-Null

# ---------------------------------------------------------------------------
#  Co / dinh nghia dung chung
# ---------------------------------------------------------------------------
# CYDLCD_HOST_TEST: cydlcd_port.h dung UART gia (xem sim/sim_stubs.cpp)
# CYDLCD_BOARD_F407 + CYDLCD_USE_LVGL=1: chon nhanh LVGL (man ngang 320x240) - GIONG board that dang dung
# LV_CONF_PATH: cau hinh LVGL rieng cho PC (sim/lv_conf_sim.h)
$defs = @(
  '-DCYDLCD_HOST_TEST=1',
  '-DCYDLCD_BOARD_F407',
  '-DCYDLCD_USE_LVGL=1',
  '-DLV_CONF_PATH=lv_conf_sim.h',
  '-DUI_MONO_FONT=4'
)
if ($OldXyz) {
  # Bo cuc CU: hop gia tri = [LX .. LX+VW] (58/58/82) -> so bat dau ngay tai nhan (dung loi da bao).
  Write-Host "CHE DO -OldXyz: dung bo cuc X/Y/Z CU (de kiem tra bay kiem thu)" -ForegroundColor Yellow
  $defs += '-DMR_XYZ_VX(i)=MR_XYZ_LX(i)'
  $defs += '-DMR_XYZ_VR(i)=(MR_XYZ_LX(i)+((i)==0?58:((i)==1?58:82)))'
  $defs += '-DMR_XYZ_VW(i)=(MR_XYZ_VR(i)-MR_XYZ_VX(i))'
}
$incs = @(
  "-I$(Join-Path $sim 'shim')",     # Arduino.h gia (phai TRUOC moi thu)
  "-I$sim",
  "-I$src",
  "-I$lvgl"
)
$cflags   = @('-O1', '-w', '-std=gnu11')   + $defs + $incs
$cxxflags = @('-O1', '-w', '-std=gnu++17', '-fno-exceptions', '-fno-rtti') + $defs + $incs

function Invoke-Native([string] $what, [scriptblock] $blk) {
  $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
  & $blk
  $rc = $LASTEXITCODE
  $ErrorActionPreference = $old
  if ($rc -ne 0) { throw "$what loi (exit $rc)" }
}

# ---------------------------------------------------------------------------
#  1) Thu vien LVGL (C) - build 1 lan, cache lai (LVGL khong doi khi sua UI)
# ---------------------------------------------------------------------------
$lvglLib = Join-Path $bldLib 'liblvgl.a'
if ($Rebuild -or -not (Test-Path $lvglLib)) {
  Write-Host "Bien dich LVGL (lan dau, hoi lau)..." -ForegroundColor Cyan
  $cSrc = Get-ChildItem -Path (Join-Path $lvgl 'src') -Recurse -Filter *.c | Select-Object -ExpandProperty FullName
  $objs = @()
  $i = 0
  foreach ($f in $cSrc) {
    $i++
    $o = Join-Path $bldLib ('lvgl_' + ($f.Substring($lvgl.Length) -replace '[\\/:]', '_') + '.o')
    $objs += $o
    if ((-not (Test-Path $o)) -or ((Get-Item $f).LastWriteTime -gt (Get-Item $o).LastWriteTime)) {
      if ($i % 40 -eq 0) { Write-Host ("  ... {0}/{1}" -f $i, $cSrc.Count) }
      Invoke-Native "gcc $f" { & $gcc @cflags -c $f -o $o }
    }
  }
  if (Test-Path $lvglLib) { Remove-Item $lvglLib -Force }
  Invoke-Native "ar liblvgl.a" { & (Join-Path (Split-Path $gcc -Parent) 'ar.exe') rcs $lvglLib @objs }
  Write-Host ("  -> {0} file C, xong" -f $cSrc.Count) -ForegroundColor Green
}

# ---------------------------------------------------------------------------
#  2) Code THAT cua firmware (UI + link + tramming) + 3 file cua ban gia lap
# ---------------------------------------------------------------------------
$uiSrc = Get-ChildItem -Path $src -Filter 'ui_*.cpp' |
         Where-Object { $_.Name -notlike 'ui_port_*' } |     # 2 file port that dung TFT_eSPI -> bo qua
         Select-Object -ExpandProperty FullName
$moreSrc = @(
  (Join-Path $src 'cydlcd_link.cpp'),
  (Join-Path $src 'tbt_link.cpp'),
  (Join-Path $src 'lv_font_gfxfont.c'),
  (Join-Path $src 'gfx4_font_icons.c'),        # font icon 4bpp (che do UI_MONO_FONT 4)
  (Join-Path $src 'gfx4_font_icons_big.c'),
  (Join-Path $src 'gfx4_font_text.c'),
  (Join-Path $src 'gfx_font_robotomono.c'),
  (Join-Path $sim 'sim_port.cpp'),
  (Join-Path $sim 'sim_stubs.cpp'),
  (Join-Path $sim 'sim_net_stubs.cpp'),
  (Join-Path $sim 'sim_main.cpp')
)
$sources = @($moreSrc) + $uiSrc

Write-Host "Bien dich UI ($($sources.Count) file)..." -ForegroundColor Cyan
$objs = @()
foreach ($f in $sources) {
  $isC = $f.ToLower().EndsWith('.c')
  $o = Join-Path $bld ((Split-Path $f -Leaf) + '.o')
  $objs += $o
  $newer = (-not (Test-Path $o)) -or ((Get-Item $f).LastWriteTime -gt (Get-Item $o).LastWriteTime)
  if ($Rebuild -or $newer) {
    if ($isC) { Invoke-Native "gcc $f" { & $gcc @cflags  -c $f -o $o } }
    else      { Invoke-Native "g++ $f" { & $gpp @cxxflags -c $f -o $o } }
  }
}

Write-Host "Lien ket..." -ForegroundColor Cyan
Invoke-Native "link" { & $gpp @objs $lvglLib -o $exe -lm }

# ---------------------------------------------------------------------------
#  3) Chay
# ---------------------------------------------------------------------------
$runArgs = @('--out', $Out)
if ($Script) { $runArgs += $Script }
Write-Host "Chay ban gia lap: $exe $($runArgs -join ' ')" -ForegroundColor Cyan
Invoke-Native "chay sim" { & $exe @runArgs }
Write-Host "Anh man hinh: $Out" -ForegroundColor Green
