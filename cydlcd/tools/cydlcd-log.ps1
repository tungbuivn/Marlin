#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Doc log chan doan cua man hinh cydlcd (CYD) qua cong serial USB (mac dinh COM7 @115200) - CHI DOC, khong ghi.

.DESCRIPTION
    Man hinh in log 1 Hz tren cong serial USB (CH340):
        [CYD] loop OK | t=<ms> | link PUSH=.. TBT=.. rx=.. err=.. heap=<B>
        [UP]  wifi=.. ssid=".." ip=.. rssi=.. sd=.. busy=.. rx=.. written=.. heap=.. heapmin=.. free=..KB | file=.. | <ket qua>
        [MRL] st=.. fl=0x.. prog=..% file=".." | SD r=<doc ok>/<doc loi> w=<ghi ok>/<ghi loi>
    va cac dong su kien:
        [WIFI] quet / ket noi / mat ket noi / luu NVS / trang thai WiFi
        [UP]   tien do upload / mount the TF o khe man hinh / ghi-xoa file / bao Marlin M22-M21
        [MRL]  trang thai Marlin (tu frame PUSH) + bo dem sector phuc vu Marlin
        [PRE]  preset preheat PLA/PETG/ABS (doc, dat, luu NVS)
    cung cac dong ket thuc phien: "phien moi", "POST OK", "DONE", "ERR ...".

    Script nay CHI mo cong va doc (khong gui byte nao). DTR duoc bat (can thiet de CH340 day du lieu
    ra); RTS de mac dinh. Dung de doi chieu trong luc tools/cydlcd-upload.ps1 dang gui file.

    ⚠️ Chi MOT tien trinh duoc giu cong COM: dung mo script nay song song voi Serial Monitor.

.PARAMETER Port
    Cong serial cua man hinh (mac dinh COM7).
.PARAMETER Baud
    Mac dinh 115200.
.PARAMETER Seconds
    Doc bao nhieu giay (mac dinh 60).
.PARAMETER OutFile
    Ghi them ra file (tuy chon).
.PARAMETER OnlyCydlcd
    Chi in cac dong co [CYD] / [UP] / [MRL] / [PRE] / [WIFI] / DONE / ERR (bo qua rac khac).

.EXAMPLE
    .\cydlcd\tools\cydlcd-log.ps1 -Seconds 120 -OutFile $env:TEMP\cydlcd.log

.NOTES
    Cong COM nay la USB-serial (nap firmware + doc log), KHONG phai cong SPP.
    Duong upload file duy nhat nay la HTTP: tools/cydlcd-upload.ps1 (PC/dien thoai POST file len
    web server cua man hinh; man hinh ghi vao the SD o khe man hinh).
#>
[CmdletBinding()]
param(
    [string]$Port = 'COM7',
    [int]$Baud = 115200,
    [int]$Seconds = 60,
    [string]$OutFile,
    [switch]$OnlyCydlcd,
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
            if ($OnlyCydlcd -and -not ($line -match '\[CYD\]|\[UP\]|\[MRL\]|\[PRE\]|\[WIFI\]|DONE|ERR')) { continue }
            $col = 'Gray'
            if ($line -match 'ERR|CANH BAO|!!') { $col = 'Red' }
            elseif ($line -match 'DONE') { $col = 'Green' }
            elseif ($line -match '\[UP\]|\[MRL\]') { $col = 'DarkCyan' }
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
