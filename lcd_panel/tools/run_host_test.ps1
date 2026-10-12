# ===========================================================================
#  lcd_panel/tools/run_host_test.ps1
#
#  Chay kiem thu giao thuc panel_serial NGAY TREN MAY TINH (khong can board, khong can mang):
#      pwsh lcd_panel/tools/run_host_test.ps1
#
#  Can 1 trinh bien dich C++ cho host (g++ cua MinGW/WinLibs). Neu chua co:
#      winget install BrechtSanders.WinLibs.POSIX.UCRT
# ===========================================================================
$ErrorActionPreference = 'Stop'
$root  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$src   = Join-Path $root 'lcd_panel\tools\test_panel_link.cpp'
$outd  = Join-Path $root 'lcd_panel\.pio-host-test'
$exe   = Join-Path $outd 'test_panel_link.exe'

$gxx = (Get-Command g++ -ErrorAction SilentlyContinue)
if (-not $gxx) { throw "Khong tim thay g++. Cai MinGW (winget install BrechtSanders.WinLibs.POSIX.UCRT) roi chay lai." }
New-Item -ItemType Directory -Force -Path $outd | Out-Null

Write-Host "Bien dich: $src" -ForegroundColor Cyan
& $gxx.Source -std=gnu++17 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-dangling-pointer -DPANEL_HOST_TEST `
    "-I$(Join-Path $root 'lcd_panel\src')" $src -o $exe
if ($LASTEXITCODE -ne 0) { throw "Bien dich loi (exit $LASTEXITCODE)" }

Write-Host "Chay test..." -ForegroundColor Cyan
& $exe
$rc = $LASTEXITCODE
if ($rc -eq 0) { Write-Host "TAT CA PASS" -ForegroundColor Green } else { Write-Host "CO TEST FAIL ($rc)" -ForegroundColor Red }
exit $rc
