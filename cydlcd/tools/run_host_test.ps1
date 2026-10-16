# ===========================================================================
#  cydlcd/tools/run_host_test.ps1
#
#  Chay kiem thu giao thuc cydlcd_serial NGAY TREN MAY TINH (khong can board, khong can mang):
#      pwsh cydlcd/tools/run_host_test.ps1
#
#  Can 1 trinh bien dich C++ cho host (g++ cua MinGW/WinLibs). Neu chua co:
#      winget install BrechtSanders.WinLibs.POSIX.UCRT
# ===========================================================================
$ErrorActionPreference = 'Stop'
$root  = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$src   = Join-Path $root 'cydlcd\tools\test_cydlcd_link.cpp'
$outd  = Join-Path $root 'cydlcd\.pio-host-test'
$exe   = Join-Path $outd 'test_cydlcd_link.exe'

$gxx = (Get-Command g++ -ErrorAction SilentlyContinue)
if (-not $gxx) { throw "Khong tim thay g++. Cai MinGW (winget install BrechtSanders.WinLibs.POSIX.UCRT) roi chay lai." }
New-Item -ItemType Directory -Force -Path $outd | Out-Null

Write-Host "Bien dich: $src" -ForegroundColor Cyan
# !! KHONG de $ErrorActionPreference = 'Stop' trong luc goi trinh bien dich !!
# Tren Windows PowerShell 5.1, MOI dong ma g++ ghi ra stderr (ke ca WARNING) bi doi thanh
# NativeCommandError = loi KET THUC -> script bao "loi" va KHONG chay test, du binary da build xong.
# (Loi that 2026-10-15: nguoi dung chay script thay 'Bien dich ... [exit code: 1]' roi thoi.)
$old = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
& $gxx.Source -std=gnu++17 -O1 -Wall -Wextra -Wno-unused-parameter -Wno-dangling-pointer -DCYDLCD_HOST_TEST `
    "-I$(Join-Path $root 'cydlcd\src')" $src -o $exe
$cc = $LASTEXITCODE
$ErrorActionPreference = $old
if ($cc -ne 0) { throw "Bien dich loi (exit $cc)" }

Write-Host "Chay test..." -ForegroundColor Cyan
$ErrorActionPreference = 'Continue'
& $exe
$rc = $LASTEXITCODE
$ErrorActionPreference = $old
if ($rc -eq 0) { Write-Host "TAT CA PASS" -ForegroundColor Green } else { Write-Host "CO TEST FAIL ($rc)" -ForegroundColor Red }
exit $rc
