#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Console serial de GUI LENH (G-code / M-code) toi Marlin qua cong cua panel CYD.

.DESCRIPTION
    Dung de thay cac terminal hay bi loi (khong gui dung ky tu ket thuc dong):
    script nay LUON gui kem "`r`n" nen panel/Marlin nhan dung tung lenh mot.

    Duong di cua lenh:  PC --(COM7, 115200)--> panel CYD --(USART1 250000)--> Marlin
    => giong HET nhu ban bam nut tren man hinh LCD.

    Vi du: .\serial-console.ps1                 (mac dinh COM7 115200)
           .\serial-console.ps1 -Port COM4 -Baud 250000   (gui thang vao Marlin qua USB)

.NOTES
    Go 'exit' + Enter de thoat. Ctrl+C cung duoc.
#>
param(
    [string]$Port = "COM7",
    [int]$Baud = 115200,
    # Gui CRLF (chuan cho ca panel va Marlin). Doi thanh "`n" neu ban muon thu kieu khac.
    [string]$Eol = "`r`n"
)

$ErrorActionPreference = "Continue"

if (-not ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Port)) {
    Write-Host "KHONG thay $Port. Cac cong dang co: $([System.IO.Ports.SerialPort]::GetPortNames() -join ', ')" -ForegroundColor Red
    exit 1
}

$sp = New-Object System.IO.Ports.SerialPort
$sp.PortName  = $Port
$sp.BaudRate  = $Baud
$sp.Parity    = [System.IO.Ports.Parity]::None
$sp.DataBits  = 8
$sp.StopBits  = [System.IO.Ports.StopBits]::One
$sp.NewLine   = $Eol
$sp.ReadTimeout = 200
$sp.WriteTimeout = 2000
$sp.DtrEnable = $true          # bat DTR: board CDC moi chiu in log ra
$sp.RtsEnable = $true

try { $sp.Open() } catch {
    Write-Host "Khong mo duoc $Port : $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

Start-Sleep -Milliseconds 600
$sp.DiscardInBuffer()

Write-Host "===================================================" -ForegroundColor Cyan
Write-Host " Serial console: $Port @ $Baud" -ForegroundColor Cyan
Write-Host " Go lenh roi Enter (vi du: G28 / M220 / G91)" -ForegroundColor Cyan
Write-Host " Go 'exit' + Enter de thoat" -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan

# Vua doc vua cho nhap: doc truoc moi vong, roi moi Read-Host (tranh mat phan hoi)
while ($true) {
    try { $chunk = $sp.ReadExisting() } catch { $chunk = "" }
    if ($chunk) { Write-Host -NoNewline $chunk }

    if ([Console]::KeyAvailable) {
        $line = Read-Host
        if ($line -eq "exit") { break }
        if ($line) {
            # Cho xả hết phan hoi cu truoc khi gui
            try { $old = $sp.ReadExisting() } catch { $old = "" }
            if ($old) { Write-Host -NoNewline $old }
            $sp.Write($line + $Eol)
            Write-Host ">>> $line" -ForegroundColor Yellow
        }
    }
    Start-Sleep -Milliseconds 40
}

$sp.Close()
Write-Host "Da dong $Port." -ForegroundColor Cyan
