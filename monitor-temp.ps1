#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Dat nhiet do hotend va theo doi bang M105, giu MOT ket noi duy nhat.

.DESCRIPTION
    Vi sao can script rieng thay vi goi send-gcode.ps1 nhieu lan: MO LAI cong COM
    giua chung co the reset board -> mat M104 target -> phep thu vo nghia (va
    heater tat giua luc dang do). Script nay mo cong MOT lan, roi gui M104 va doc
    M105 lap lai trong cung phien.

    AN TOAN: neu nhiet do doc duoc vuot qua (target + GuardBand) thi tu dong gui
    M104 S0 va dung ngay. Day chi la lop phu -- nguoi dung van phai dung canh may
    voi dong ho do ngoai va tay gan cong tac nguon, vi chinh cam biến moi la thu
    dang bi nghi ngo.

.EXAMPLE
    .\monitor-temp.ps1 -Target 100 -Seconds 120
    .\monitor-temp.ps1 -Target 0                  # chi tat heater roi thoat
#>
param(
    [string]$Port = "COM4",
    [int]$Baud = 250000,
    [double]$Target = 100,      # do C. 0 = chi tat heater
    [int]$Seconds = 120,
    [int]$IntervalMs = 5000,
    [double]$GuardBand = 20,    # vuot target + so nay thi TU TAT
    [switch]$KeepOn,            # khong gui M104 S0 khi ket thuc
    [switch]$ProbeOnly          # chi doc + in M105 roi thoat, KHONG bao gio bat heater
)

$ErrorActionPreference = "Continue"

if (-not [System.IO.Ports.SerialPort]::GetPortNames() -contains $Port) {
    Write-Host "KHONG thay $Port. Cong dang co: $([System.IO.Ports.SerialPort]::GetPortNames() -join ', ')" -ForegroundColor Red
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

$sp.Open()
Start-Sleep -Milliseconds 2500     # cho board on dinh sau khi mo cong
$sp.DiscardInBuffer()

function Send-Cmd($cmd) {
    try { $sp.WriteLine($cmd) } catch { Write-Host "Loi gui '$cmd': $($_.Exception.Message)" -ForegroundColor Red }
}

function Read-For([int]$ms) {
    $buf = New-Object System.Text.StringBuilder
    $end = (Get-Date).AddMilliseconds($ms)
    while ((Get-Date) -lt $end) {
        try {
            $c = $sp.ReadExisting()
            if ($c.Length -gt 0) { [void]$buf.Append($c) }
            else { Start-Sleep -Milliseconds 60 }
        }
        catch { break }
    }
    return $buf.ToString()
}

$rx = '(?:^|[^A-Za-z])T:\s*(-?[\d.]+)\s*/\s*(-?[\d.]+)'   # T:nhiet /target

function Parse-Temp([string]$text) {
    $m = [regex]::Matches($text, $rx)
    if ($m.Count -eq 0) { return $null }
    $last = $m[$m.Count - 1]
    return @{ Temp = [double]$last.Groups[1].Value; Target = [double]$last.Groups[2].Value }
}

# Duong hien thi duoc tach rieng va dung CHI cac specifier da kiem chung hop le
# ({0,6:N1} va {1,7:F2}). Dau '+' KHONG duoc dat trong phan canh le
# ("{3,+6:F2}") -- .NET nem "Input string was not in a correct format", va lan
# truoc loi do no ra SAU khi M104 da bat heater.
function Format-Reading([double]$elapsed, [double]$temp, [double]$refTarget) {
    $dt = $temp - $refTarget
    $sign = if ($dt -ge 0) { "+" } else { "-" }
    $dtStr = "{0:F2}" -f [math]::Abs($dt)
    return ("  {0,6:N1}s : T = {1,7:F2} C   (target {2}, lech {3}{4})" -f `
        $elapsed, $temp, $refTarget, $sign, $dtStr)
}

try {
    if ($ProbeOnly) {
        Write-Host "Che do chi-doc: KHONG bat heater." -ForegroundColor Cyan
        Send-Cmd "M105"
        $t = Read-For 1500
        $p = Parse-Temp $t
        if ($null -eq $p) {
            Write-Host "KHONG doc duoc M105. Raw:" -ForegroundColor Red
            Write-Host $t
            exit 3
        }
        Write-Host (Format-Reading 0 $p.Temp $p.Target)
        Write-Host ("  raw day du: {0}" -f ($t -replace "`r?`n", " ").Trim())
        exit 0
    }

    if ($Target -le 0) {
        Send-Cmd "M104 S0"
        Start-Sleep -Milliseconds 400
        Write-Host "Da gui M104 S0 (tat hotend)."
        [void](Read-For 800)
        $sp.Close(); $sp.Dispose()
        exit 0
    }

    $abortLimit = $Target + $GuardBand
    Write-Host ("Dat hotend {0} C. TU TAT neu vuot {1} C. Theo doi {2} giay, moi {3} ms." -f $Target, $abortLimit, $Seconds, $IntervalMs) -ForegroundColor Cyan
    Write-Host ""

    # QUAN TRONG: doc + in trang thai TRUOC khi bat heater. Neu con loi dinh dang
    # hay loi bat ky o duong hien thi thi no no ra o day, chu khong no SAU khi
    # M104 da bat heater va bo mac no chay mot minh.
    Send-Cmd "M105"
    $pre = Parse-Temp (Read-For 1200)
    if ($null -eq $pre) {
        Write-Host "KHONG doc duoc M105 -- dung, KHONG bat heater." -ForegroundColor Red
        $sp.Close(); $sp.Dispose()
        exit 3
    }
    Write-Host ("Truoc khi ham: T = {0:F2} C (target hien tai {1:F0})" -f $pre.Temp, $pre.Target) -ForegroundColor DarkCyan
    Write-Host ""

    Send-Cmd ("M104 S{0}" -f $Target)
    Start-Sleep -Milliseconds 500
    [void](Read-For 500)

    $t0 = Get-Date
    $next = 0
    $aborted = $false
    do {
        $wait = $next - ((Get-Date) - $t0).TotalMilliseconds
        if ($wait -gt 0) { Start-Sleep -Milliseconds ([int]$wait) }
        $next += $IntervalMs

        Send-Cmd "M105"
        $text = Read-For 1200
        $p = Parse-Temp $text
        $el = ((Get-Date) - $t0).TotalSeconds

        if ($null -eq $p) {
            Write-Host ("  {0,6:N1}s : (khong doc duoc M105)" -f $el) -ForegroundColor DarkGray
        }
        else {
            $color = if ($p.Temp -gt $abortLimit) { "Red" } elseif ([math]::Abs($p.Temp - $Target) -le 2) { "Green" } else { "Gray" }
            Write-Host (Format-Reading $el $p.Temp $Target) -ForegroundColor $color

            if ($p.Temp -gt $abortLimit) {
                Write-Host ""
                Write-Host ("!! VUOT {0} C -- TU DONG TAT HEATER (M104 S0)." -f $abortLimit) -ForegroundColor Red
                Send-Cmd "M104 S0"
                Start-Sleep -Milliseconds 500
                [void](Read-For 800)
                $aborted = $true
                break
            }
        }
    } while (((Get-Date) - $t0).TotalSeconds -lt $Seconds)

    if (-not $aborted -and -not $KeepOn) {
        Write-Host ""
        Write-Host "Het thoi gian theo doi -- gui M104 S0." -ForegroundColor Yellow
        Send-Cmd "M104 S0"
        Start-Sleep -Milliseconds 500
        [void](Read-For 800)
    }
    elseif ($KeepOn) {
        Write-Host ""
        Write-Host ("GIU NGUYEN target {0} C (co -KeepOn). Tu tat khi xong viec!" -f $Target) -ForegroundColor Yellow
    }

    if ($aborted) { exit 2 }
    exit 0
}
finally {
    if ($sp.IsOpen) { $sp.Close() }
    $sp.Dispose()
}
