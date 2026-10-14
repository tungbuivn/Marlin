# test_stop_full_queue.ps1 -- TEST HIL (chay tren may that) cho loi "bam STOP khong dung duoc
# ban in + LCD tu restart" (sua 2026-10-15).
#
# KICH BAN: lam DAY hang doi lenh cua Marlin (ban in dung o M190 cho nong ban, nen ban khong
# nong duoc -> Marlin ngung nap them lenh tu file: theo doi SD r= trong log COM7 DUNG YEN),
# roi:
#   A) HOST (COM4) gui M114 -> hang doi day nen Marlin KHONG doc duoc dong nay (khong tra loi);
#   B) PANEL (COM7 = dung duong cua nut STOP) gui M108 + M524 -> phai HUY DUOC ban in ngay;
#   C) HOST gui M114 lai -> phai tra loi (hang doi da trong).
# Doi chieu them: lenh THUONG tu panel (M220 S42) trong luc hang doi day thi BI BO (tran 2 s).
#
# LUU Y: ca 2 cong mo voi DTR=false - bat DTR tren COM4 se RESET board Marlin.
# Cach dung:  powershell -File cydlcd\tools\test_stop_full_queue.ps1 [-File83 BODY10~1.GCO]
param(
    [string]$PanelPort = 'COM7',
    [string]$HostPort  = 'COM4',
    [string]$File83    = 'BODY10~1.GCO'
)
# Kich ban: lam DAY hang doi lenh cua Marlin (ban in dung o M190 cho nong, PSU tat) roi gui M524
# tu PANEL (dung duong cua nut STOP) va do xem Marlin co nhan lai lenh khong.
$ErrorActionPreference = 'Continue'
$ph = New-Object System.IO.Ports.SerialPort $HostPort, 250000, 'None', 8, 'One'
$pp = New-Object System.IO.Ports.SerialPort $PanelPort, 115200, 'None', 8, 'One'
# DTR=false CA HAI: bat DTR tren COM4 se RESET board Marlin (mat trang thai dang test)
$ph.DtrEnable = $false; $ph.RtsEnable = $false; $ph.ReadTimeout = 150
$pp.DtrEnable = $false; $pp.RtsEnable = $false; $pp.ReadTimeout = 150
$ph.Open(); $pp.Open()

function Pump([double]$sec) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $sec) {
        try { $h = $ph.ReadExisting() } catch { $h = '' }
        if ($h.Length) { $h -split "`n" | Where-Object { $_.Trim() } | ForEach-Object { Write-Host "[MARLIN] $($_.TrimEnd())" -ForegroundColor Yellow } }
        try { $pn = $pp.ReadExisting() } catch { $pn = '' }
        if ($pn.Length) { $pn -split "`n" | Where-Object { $_ -match 'st=|FWD|PANEL|txq|drop' } | ForEach-Object { Write-Host "[PANEL ] $($_.TrimEnd())" -ForegroundColor Gray } }
        Start-Sleep -Milliseconds 20
    }
}
function CmdPanel([string]$c) { Write-Host ">>> PANEL: $c" -ForegroundColor Cyan;    $pp.WriteLine($c); Pump 2.5 }
function CmdHost([string]$c)  { Write-Host ">>> HOST : $c" -ForegroundColor Magenta; $ph.WriteLine($c); Pump 2.5 }

Pump 2
CmdPanel 'M21'
CmdPanel "M23 $File83"
CmdPanel 'M24'
Write-Host "--- cho 15 s cho ban in chay toi cho M190 va hang doi DAY ---" -ForegroundColor Yellow
Pump 15
Write-Host "=== BUOC A: HOST gui M114 - neu hang doi DAY thi Marlin KHONG doc duoc dong nay ===" -ForegroundColor Yellow
CmdHost 'M114'
Write-Host "=== BUOC B: PANEL gui M108 + M524 (dung duong nut STOP) ===" -ForegroundColor Yellow
$pp.WriteLine('M108'); Pump 0.5; $pp.WriteLine('M524'); Pump 6
Write-Host "=== BUOC C: HOST gui M114 lai - phai tra loi duoc (hang doi da trong) ===" -ForegroundColor Yellow
CmdHost 'M114'
CmdHost 'M27'
$ph.Close(); $pp.Close()
Write-Host "---- xong ----" -ForegroundColor Cyan
