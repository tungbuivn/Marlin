#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Gui lenh G-code toi Marlin qua cong serial va in ra phan hoi.

.DESCRIPTION
    Board MKS Monster8 V2 tren may nay dung USB CDC (VID_0483&PID_5740) hien ra
    thanh COM4; BAUDRATE trong Configuration.h la 250000 nhung voi CDC thi con so
    nay khong quan trong, mien hai dau deu dat mot gia tri.

    Script gui tung lenh roi doc cho toi khi het du lieu trong 1 khoang lang, thay
    vi doi chuoi "ok" - vi M997 lam board BIEN MAT (nhay vao DFU) nen khong bao gio
    co "ok".

.EXAMPLE
    .\send-gcode.ps1 -Command M115
    .\send-gcode.ps1 -Command M122,M92,M503
    .\send-gcode.ps1 -Command "M92 X40 Y40" -Command M500 -Show
#>
param(
    [string]$Port = "COM4",
    [int]$Baud = 250000,
    [Parameter(Mandatory = $true)][string[]]$Command,
    # Thoi gian lang coi nhu da doc het phan hoi (ms)
    [int]$QuietMs = 1200,
    [switch]$Show   # in ca lenh gui di
)

$ErrorActionPreference = "Continue"

if (-not [System.IO.Ports.SerialPort]::GetPortNames() -contains $Port) {
    Write-Host "KHONG thay $Port. Cac cong dang co: $([System.IO.Ports.SerialPort]::GetPortNames() -join ', ')" -ForegroundColor Red
    exit 1
}

$sp = New-Object System.IO.Ports.SerialPort
$sp.PortName = $Port
$sp.BaudRate = $Baud
$sp.Parity = [System.IO.Ports.Parity]::None
$sp.DataBits = 8
$sp.StopBits = [System.IO.Ports.StopBits]::One
$sp.NewLine = "`n"
$sp.ReadTimeout = 200
$sp.WriteTimeout = 2000
$sp.DtrEnable = $true
$sp.RtsEnable = $true

try {
    $sp.Open()
}
catch {
    Write-Host "Khong mo duoc $Port : $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# CDC lam board reset khi mo cong -> doi boot xong roi don sach buffer
Start-Sleep -Milliseconds 2500
$sp.DiscardInBuffer()

$all = New-Object System.Text.StringBuilder

foreach ($cmd in $Command) {
    if ($Show) { Write-Host ">>> $cmd" -ForegroundColor DarkCyan }
    try {
        $sp.WriteLine($cmd)
    }
    catch {
        Write-Host "Loi gui '$cmd': $($_.Exception.Message)" -ForegroundColor Red
    }
    # doc cho toi khi lang
    $last = Get-Date
    $deadline = (Get-Date).AddMilliseconds(8000)
    while ($true) {
        $chunk = $sp.ReadExisting()
        if ($chunk.Length -gt 0) {
            [void]$all.Append($chunk)
            $last = Get-Date
        }
        else {
            Start-Sleep -Milliseconds 80
        }
        if (((Get-Date) -gt $deadline) -or (((Get-Date) - $last).TotalMilliseconds -gt $QuietMs)) {
            break
        }
    }
}

$sp.Close()
$sp.Dispose()

($all.ToString() -split "`r?`n") | Where-Object { $_.Trim().Length -gt 0 }
