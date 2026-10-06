#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Upload Marlin firmware to MKS Monster8 V2 via DFU bootloader

.DESCRIPTION
    Builds and uploads firmware using DFU (Device Firmware Upgrade) protocol.
    This requires the STM32 bootloader to be in DFU mode.

    Prerequisites:
    - dfu-util installed      (hoac: pio pkg install -g -t tool-dfuutil)
    - Board in DFU mode (hold BOOT0, press RESET, release BOOT0)
    - USB connected (not ST-Link)

    IMPORTANT - Flash address:
    MKS Monster8 has an MKS bootloader occupying 0xC000 (48KB) at the start of
    flash (sectors 0-2, 16KB each). The Marlin firmware is linked at
    0x08000000 + 0xC000 = 0x0800C000 (see board_build.offset and
    board_upload.offset_address in ini/stm32f4.ini), so dfu-util MUST write to
    0x0800C000. Writing to 0x08000000 erases the MKS bootloader and the board
    will no longer boot.

.PARAMETER Environment
    PlatformIO environment (default: mks_monster8)

.PARAMETER DfuDevice
    DFU device selector. Format: "index" or "vid:pid"
    Leave empty to auto-detect

.PARAMETER BuildOnly
    Only build, don't upload (default: false)

.PARAMETER NoPrompt
    Skip confirmation prompt (default: false)

.EXAMPLE
    # Build and upload via DFU
    .\upload-dfu.ps1

    # Build only
    .\upload-dfu.ps1 -BuildOnly

    # Specify DFU device
    .\upload-dfu.ps1 -DfuDevice "0"

#>

param(
    [string]$Environment = "mks_monster8",
    [string]$DfuDevice,
    [int]$TransferSize = 512,
    [switch]$BuildOnly,
    [switch]$NoPrompt
)

$ErrorActionPreference = "Stop"

# ---------------------------------------------------------------------------
# DIA CHI FLASH - RAT QUAN TRONG, KHONG SUA NEU KHONG HIEU RO
#
#   MKS Monster8 (STM32F407) co bootloader chiem 0xC000 = 48KB dau flash
#   (sector 0-2, moi sector 16KB). Firmware Marlin duoc link tai:
#       0x08000000 + 0xC000 = 0x0800C000
#   xem board_build.offset / board_upload.offset_address trong ini/stm32f4.ini
#
#   => dfu-util PHAI ghi vao 0x0800C000.
#      Ghi vao 0x08000000 se XOA bootloader MKS => board khong boot duoc.
#
#   Kiem chung bang: dfu-util --list
#     @Internal Flash /0x08000000/04*016Kg,01*064Kg,07*128Kg
#     3 sector dau = 3 x 16KB = 48KB = 0xC000  ->  do la bootloader.
# ---------------------------------------------------------------------------
$flashAddress = "0x0800C000"

# Validate dfu-util
Write-Host "Checking dfu-util..." -ForegroundColor Cyan
$dfu = Get-Command dfu-util -ErrorAction SilentlyContinue

if (-not $dfu) {
    # PlatformIO quan ly san tool-dfuutil, khong can cai dat admin
    $pioDfu = Join-Path $env:USERPROFILE ".platformio\packages\tool-dfuutil\bin\dfu-util.exe"
    if (Test-Path $pioDfu) {
        $dfu = Get-Item $pioDfu
        Write-Host "OK: dung dfu-util tu PlatformIO" -ForegroundColor Green
    } else {
        Write-Host "Error: dfu-util not found in PATH" -ForegroundColor Red
        Write-Host "Install: pio pkg install -g -t tool-dfuutil" -ForegroundColor Yellow
        Write-Host "     or: winget install dfu-util" -ForegroundColor Yellow
        exit 1
    }
} else {
    Write-Host "OK: dfu-util found" -ForegroundColor Green
}

$dfuExe = $dfu.Source

# Validate PlatformIO
Write-Host "Checking PlatformIO..." -ForegroundColor Cyan
$pio = Get-Command pio -ErrorAction SilentlyContinue

if (-not $pio) {
    Write-Host "Error: PlatformIO not found in PATH" -ForegroundColor Red
    exit 1
}

Write-Host "OK: PlatformIO found" -ForegroundColor Green

$projectRoot = Split-Path -Parent $PSCommandPath

Write-Host ""
Write-Host "===== Marlin Firmware Upload (DFU) =====" -ForegroundColor Cyan
Write-Host "Project:     $projectRoot"
Write-Host "Environment: $Environment"
Write-Host "Hardware:    MKS Monster8 V2 (STM32F407VGT6)"
Write-Host "Protocol:    DFU (USB Bootloader)"
Write-Host "Flash addr:  $flashAddress   (0x08000000 la bootloader MKS - DUNG GHI VAO)"
Write-Host ""
Write-Host "IMPORTANT - Board must be in DFU mode:" -ForegroundColor Yellow
Write-Host "  1. Press and hold BOOT0 button"
Write-Host "  2. Press RESET button briefly"
Write-Host "  3. Release BOOT0 button"
Write-Host "  4. Board appears as STM32 DFU device"
Write-Host ""
Write-Host "Detecting DFU devices..." -ForegroundColor Cyan

if (-not $BuildOnly) {
    # dfu-util co the ghi canh bao ra stderr -> khong de ErrorActionPreference dung script
    $prevEAP = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $devices = & $dfuExe --list 2>&1
    $ErrorActionPreference = $prevEAP

    if ($devices -match "STM32") {
        Write-Host "Found DFU device(s):" -ForegroundColor Green
        $devices | Where-Object { $_ -match "STM32|Found" } | ForEach-Object {
            Write-Host "  $_"
        }
    } else {
        Write-Host "WARNING: No DFU devices detected" -ForegroundColor Yellow
        Write-Host "Make sure board is in DFU mode and USB is connected"
    }
}

Write-Host ""

if (-not $NoPrompt) {
    $confirm = Read-Host "Continue? (y/n)"
    if ($confirm -ne "y" -and $confirm -ne "Y") {
        Write-Host "Cancelled" -ForegroundColor Yellow
        exit 0
    }
}

Write-Host ""
Write-Host "Building firmware..." -ForegroundColor Cyan

$buildArgs = @("run", "-d", $projectRoot, "-e", $Environment)

# Marlin luon co the phat #warning ra stderr. Voi $ErrorActionPreference = "Stop",
# PowerShell coi stderr cua lenh native la loi terminating va script se dung giua
# chung du build THANH CONG. Vi vay tam ha xuong "Continue" va tu kiem tra exit code.
$prevEAP = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& pio @buildArgs
$buildExit = $LASTEXITCODE
$ErrorActionPreference = $prevEAP

if ($buildExit -ne 0) {
    Write-Host "Error: Build failed" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "SUCCESS: Build completed!" -ForegroundColor Green

# ---------------------------------------------------------------------------
# Xac dinh file firmware.
# board_build.rename trong ini/stm32f4.ini doi ten firmware.bin -> <env>.bin,
# nen firmware.bin KHONG con ton tai sau khi build.
# ---------------------------------------------------------------------------
$binPath = Join-Path $projectRoot ".pio\build\$Environment\$Environment.bin"

if (-not (Test-Path $binPath)) {
    # Fallback cho environment khong dat board_build.rename
    $binPath = Join-Path $projectRoot ".pio\build\$Environment\firmware.bin"
}

if (-not (Test-Path $binPath)) {
    Write-Host "Error: Firmware binary not found in .pio\build\$Environment\" -ForegroundColor Red
    Write-Host "  Da tim: $Environment.bin va firmware.bin" -ForegroundColor Yellow
    exit 1
}

$binSize = [math]::Round((Get-Item $binPath).Length / 1KB, 2)

if ($BuildOnly) {
    Write-Host ""
    Write-Host "Build-only mode. Firmware is ready at:" -ForegroundColor Cyan
    Write-Host "  $binPath"
    Write-Host "  Size: $binSize KB"
    exit 0
}

Write-Host ""
Write-Host "Uploading via DFU..." -ForegroundColor Cyan
Write-Host "  Binary:  $binPath"
Write-Host "  Size:    $binSize KB"
Write-Host "  Address: $flashAddress"
Write-Host ""

# -t la SO BYTE MOI GOI USB - day chinh la ly do flash cham.
#
# Mac dinh cua dfu-util la 2048. Da kiem chung tren MKS Monster8: voi 2048 thi viec ghi
# flash hay bi rot ket noi USB giua chung ("Error during download get_status"), nen da ha
# xuong 512 cho on dinh - doi lai CHAM HON KHOANG 4 LAN (258 KB can ~500 goi thay vi ~126).
#
# Cach lam cho nhanh lai:
#   1. Cam board TRUC TIEP vao may, khong qua USB hub. Hub lam moi goi USB cham hon, ma
#      voi 512 byte/goi thi so goi rat nhieu. Kiem tra bang 'dfu-util --list': neu path la
#      dang "2-4.4" nghia la dang qua hub (2-4) roi port 4.
#   2. Cam truc tiep roi thu: .\upload-dfu.ps1 -TransferSize 2048
#   3. 2048 ma van on dinh thi dung luon; neu khong thi thu 1024.
#   4. Hoac dung ST-Link: .\upload-firmware.ps1  (nhanh hon nhieu, khong phu thuoc USB DFU)
$dfuArgs = @("-a", "0", "-s", "${flashAddress}:leave", "-t", "$TransferSize")

if ($DfuDevice) {
    $dfuArgs += "-d", $DfuDevice
}

$dfuArgs += "-D", $binPath

# dfu-util canh bao "Invalid DFU suffix signature" ra stderr -> cung phai ha EAP
$prevEAP = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $dfuExe @dfuArgs
$dfuExit = $LASTEXITCODE
$ErrorActionPreference = $prevEAP

if ($dfuExit -eq 0) {
    $hash = (Get-FileHash $binPath -Algorithm SHA256).Hash
    Write-Host ""
    Write-Host "SUCCESS: DFU upload completed!" -ForegroundColor Green
    Write-Host "  Wrote:   $binPath"
    Write-Host "  To:      $flashAddress"
    Write-Host "  SHA256:  $hash"
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "  1. Power cycle the board"
    Write-Host "  2. Connect USB serial monitor: pio device monitor -b 250000"
    Write-Host "  3. Verify firmware booted: send M115"
} else {
    Write-Host ""
    Write-Host "Error: DFU upload failed" -ForegroundColor Red
    Write-Host ""
    Write-Host "Troubleshooting:" -ForegroundColor Yellow
    Write-Host "  - Check board is in DFU mode"
    Write-Host "  - List devices: dfu-util --list"
    Write-Host "  - Try power cycling and re-entering DFU mode"
    Write-Host "  - Warning 'Invalid DFU suffix signature' la binh thuong (file .bin tho)"
    exit 1
}

exit 0
