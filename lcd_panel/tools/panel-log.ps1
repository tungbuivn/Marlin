#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Doc log chan doan cua panel CYD qua cong serial USB (mac dinh COM7 @115200) - CHI DOC, khong ghi.

.DESCRIPTION
    Panel CYD in log 1 Hz tren cong serial USB (CH340):
        [CYD] loop OK | link PUSH=1 TRAM=1 rx=1 err=0 heap=<B>
        [BTUP] bt=.. wifi=..(st=..) sd=.. st=.. rx=.. B posted=.. B .. KB/s ring=../4094 drop=.. heap=.. heapmin=.. btcyc=.. lvmem=../..
    va cac dong su kien: "phien moi tu <MAC>", "TAT BT", "bat WiFi", "POST OK", "DONE", "ERR ...".

    Script nay CHI mo cong va doc (khong gui byte nao). DTR duoc bat (can thiet de CH340 day du lieu
    ra); RTS de mac dinh. Dung de doi chieu trong luc script bt-upload-socket.ps1 dang gui file.

    ⚠️ Chi MOT tien trinh duoc giu cong COM: dung mo script nay song song voi Serial Monitor.

.PARAMETER Port
    Cong serial cua panel (mac dinh COM7).
.PARAMETER Baud
    Mac dinh 115200.
.PARAMETER Seconds
    Doc bao nhieu giay (mac dinh 60).
.PARAMETER OutFile
    Ghi them ra file (tuy chon).
.PARAMETER OnlyPanel
    Chi in cac dong co [CYD] / [BTUP] / DONE / ERR / OK / PROG (bo qua rac khac).

.EXAMPLE
    .\lcd_panel\tools\panel-log.ps1 -Seconds 120 -OutFile $env:TEMP\panel.log

.NOTES
    Cong COM nay la USB-serial (nap/log), KHONG phai cong SPP. Viec gui file qua BT dung
    tools/bt-upload-socket.ps1 (socket RFCOMM) hoac tools/bt-upload.ps1 (cong COM SPP neu co).
#>
[CmdletBinding()]
param(
    [string]$Port = 'COM7',
    [int]$Baud = 115200,
    [int]$Seconds = 60,
    [string]$OutFile,
    [switch]$OnlyPanel,
    [switch]$Dtr = $true
)

$ErrorActionPreference = 'Continue'

$sp = New-Object System.IO.Ports.SerialPort $Port, $Baud, 'None', 8, 'One'
$sp.DtrEnable = [bool]$Dtr
$sp.RtsEnable = $false
$sp.ReadTimeout = 200
try {
    $sp.Open()
} catch {
    Write-Host "Khong mo duoc ${Port}: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Cong dang co: $([System.IO.Ports.SerialPort]::GetPortNames() -join ', ')" -ForegroundColor Yellow
    Write-Host "(Neu dang mo Serial Monitor / script khac thi phai dong truoc.)" -ForegroundColor Yellow
    exit 1
}
Write-Host "Dang doc $Port @$Baud trong $Seconds s (Ctrl+C de dung)..." -ForegroundColor Cyan

$buf = ''
$raw = New-Object System.Text.StringBuilder
$sw = [System.Diagnostics.Stopwatch]::StartNew()
while ($sw.Elapsed.TotalSeconds -lt $Seconds) {
    try { $s = $sp.ReadExisting() } catch { $s = '' }
    if ($s.Length -gt 0) {
        [void]$raw.Append($s)
        $buf += $s
        while ($buf.Contains("`n")) {
            $i = $buf.IndexOf("`n")
            $line = $buf.Substring(0, $i).TrimEnd("`r")
            $buf = $buf.Substring($i + 1)
            if ($line.Length -eq 0) { continue }
            if ($OnlyPanel -and -not ($line -match '\[CYD\]|\[BTUP\]|\[BT\]|DONE|ERR|PROG|POST')) { continue }
            $col = 'Gray'
            if ($line -match 'ERR|CANH BAO|!!') { $col = 'Red' }
            elseif ($line -match 'DONE') { $col = 'Green' }
            elseif ($line -match '\[BTUP\]') { $col = 'DarkCyan' }
            Write-Host $line -ForegroundColor $col
        }
    }
    Start-Sleep -Milliseconds 40
}
$sp.Close()

if ($OutFile) {
    try {
        Set-Content -LiteralPath $OutFile -Value $raw.ToString() -Encoding UTF8
        Write-Host "Da ghi log ra: $OutFile" -ForegroundColor Green
    } catch {
        Write-Host "Khong ghi duoc $OutFile : $($_.Exception.Message)" -ForegroundColor Red
    }
}
exit 0
