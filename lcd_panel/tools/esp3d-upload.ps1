#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Duong CU (du phong): tai file G-code THANG len module ESP3D qua WiFi (POST /files) -> the SD Marlin.
    Day chi la VO BOC cua tools/panel-upload.ps1 voi -Target esp3d (mot ban code duy nhat).

.DESCRIPTION
    MAC DINH NEN DUNG tools/panel-upload.ps1 (gui len PANEL): panel lo phan day sang ESP3D va tu gui
    M22/M21, va PC chi can o CUNG MANG WiFi voi panel (mang nha) - khong phai doi mang.

    Script nay giu lai duong cu vi hai ly do:
      * Du phong khi panel chua co HTTP server / chua vao duoc WiFi nha;
      * Nhanh gon khi PC dang noi AP "Voron2308" cua module va chi muon day file len the SD.

    ⚠️ CHI CHAY KHI PC NOI DUOC TOI ESP3D: PC phai noi AP "Voron2308" cua module (IP tinh
    192.168.0.1 - mat internet tam thoi), HOAC ESP3D da duoc dat che do AP+STA de no cung vao
    mang nha. Khi PC o mang nha (TP-Link_31A0) thi 192.168.0.1 chinh la ROUTER, khong phai ESP3D
    (script kiem chu ky [ESP800] nen se tu choi chu khong gui nham vao router).

    ⚠️ ESP3D KHONG tu gui M22/M21: nen dung -InitSd -MarlinPort COM4, hoac mo man FILES tren panel.

    Toan bo tham so / cach do / ma thoat giong het panel-upload.ps1 - xem help cua script do.
    Script nay KHONG tao lai code: no goi lai panel-upload.ps1 voi -Target esp3d.

.EXAMPLE
    .\esp3d-upload.ps1 -File .\benchy.gcode                        # tu tim may in roi gui
.EXAMPLE
    .\esp3d-upload.ps1 -File big.gcode -Host 192.168.0.1           # ep IP (van xac nhan [ESP800])
.EXAMPLE
    .\esp3d-upload.ps1 -File part.gcode -InitSd -MarlinPort COM4   # sau khi gui thi gui M22/M21

.NOTES
    MA THOAT: 0 = upload OK | 2 = loi server | 3 = khong tim thay may in | 4 = loi file/tham so
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true, Position = 0)][string]$File,
    [Alias('Host')][string]$EspHost,
    [int]$Port = 80,
    [string]$Name,
    [switch]$InitSd,
    [string]$MarlinPort = 'COM4',
    [int]$TimeoutSec = 300,
    [switch]$NoDiscover,
    [Parameter(ValueFromRemainingArguments = $true)][string[]]$Rest
)

$ErrorActionPreference = 'Continue'

$tool = Join-Path $PSScriptRoot 'panel-upload.ps1'
if (-not (Test-Path -LiteralPath $tool)) {
    Write-Host "Thieu script: $tool" -ForegroundColor Red
    exit 4
}

# Duong cu = gui THANG len ESP3D. ESP3D KHONG tu gui M22/M21 => tu truyen -InitSd khi can.
$argv = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $tool,
          '-File', $File, '-Target', 'esp3d', '-Port', "$Port", '-TimeoutSec', "$TimeoutSec")
if ($EspHost) { $argv += @('-Host', $EspHost) }
if ($Name) { $argv += @('-Name', $Name) }
if ($MarlinPort) { $argv += @('-MarlinPort', $MarlinPort) }
if ($NoDiscover) { $argv += '-NoDiscover' }
if ($InitSd) { $argv += '-InitSd' }
if ($Rest) { $argv += $Rest }

& powershell @argv
exit $LASTEXITCODE
