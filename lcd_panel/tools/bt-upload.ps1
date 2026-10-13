#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Gui file G-code tu WINDOWS sang panel CYD qua Bluetooth SPP (cong COM ao cua TBT-PANEL).

.DESCRIPTION
    Panel CYD nhan file qua Bluetooth SPP roi tu day len module ESP3D (ghi vao the SD cua Marlin).
    Script nay dong vai tro "app dien thoai": mo cong COM SPP, gui dong header co TOKEN, roi gui
    noi dung file theo khoi va in moi phan hoi cua panel (OK/PROG/POST/DONE/ERR).

    CU PHAP HEADER (panel nhan 3 dang, TAT CA deu phai co token):
        PUT <token> <ten_file> <so_byte>   <- script nay gui mac dinh (tu tinh so byte)
        PUT <token> <ten_file> *           <- dung khi co -NoSize (nhan toi khi im 3 giay)
        PUT <token> <ten_file>             <- tuong duong dang "*"

    TOKEN dang dung nam trong lcd_panel/src/panel_config.h (PANEL_BT_TOKEN, mac dinh "TBT-3D-up").

    VI DU:
        .\lcd_panel\tools\bt-upload.ps1 -Token TBT-3D-up -File .\benchy.gco
        .\lcd_panel\tools\bt-upload.ps1 -Port COM8 -Token TBT-3D-up -File .\benchy.gco -Name B2.gco
        .\lcd_panel\tools\bt-upload.ps1 -Token TBT-3D-up -File .\big.gco -NoSize -ChunkKB 2

.NOTES
    PAIR TREN WINDOWS (lam 1 lan):
        Settings -> Bluetooth & devices -> Add device -> Bluetooth -> chon "TBT-PANEL".
        Neu Windows hoi ma (PIN) thi nhap: 74196328   (xem PANEL_BT_PIN trong panel_config.h).
    CONG COM NAO: sau khi pair, Windows thuong tao 2 cong (vd "COM7"/"COM8"): mot "Incoming",
        mot "Outgoing". PHAI dung cong OUTGOING (thuong la cong co ten chua "TBT-PANEL" hoac
        "Bluetooth"). Script tu liet ke cac cong de ban chon neu -Port khong dung.
    BAUD khong quan trong voi SPP (truyen thong), nhung phai dat mot gia tri -> dung 115200.

    Truoc khi chay: panel phai dang BAT (mac dinh PANEL_BT_UPLOAD_AUTOSTART=1) va the TF da cam.
    Theo doi tien trinh tren man hinh panel; neu Marlin dang in thi panel se tu choi (ERR dang in).
#>
param(
    [string]$Port,                                  # vd COM8 - bo trong de script tu liet ke/do
    [Parameter(Mandatory = $true)][string]$Token,    # token trong panel_config.h (PANEL_BT_TOKEN)
    [Parameter(Mandatory = $true)][string]$File,     # duong dan file G-code tren PC
    [string]$Name,                                   # ten file tren the SD Marlin (mac dinh: ten file PC)
    [int]$ChunkKB = 4,                               # gui theo khoi bao nhieu KB (1 KB = 1024 byte)
    [int]$WindowKB = 12,                             # cua so byte "dang bay" toi da - cho theo moc PROG
                                                     # (panel: vong dem 12 KB + PANEL_BT_PROG_STEP 4 KB;
                                                     #  dat > 12 KB co the lam TRAN vong dem -> mat byte)
    [switch]$NoSize,                                 # gui "PUT <token> <ten> *" (khong biet truoc so byte)
    [int]$TimeoutSec = 600,                          # tong thoi gian cho DONE/ERR
    [int]$Baud = 115200
)

$ErrorActionPreference = "Continue"

# ---------------------------------------------------------------------------
# 0) Kiem tra dau vao
# ---------------------------------------------------------------------------
if (-not (Test-Path -LiteralPath $File -PathType Leaf)) {
    Write-Host "KHONG thay file: $File" -ForegroundColor Red
    exit 1
}
$fi = Get-Item -LiteralPath $File
if ($fi.Length -le 0) {
    Write-Host "File rong (0 byte): $File" -ForegroundColor Red
    exit 1
}
if ([string]::IsNullOrWhiteSpace($Token)) {
    Write-Host "Token rong - phai dung dung PANEL_BT_TOKEN trong panel_config.h" -ForegroundColor Red
    exit 1
}
if ($ChunkKB -lt 1) { $ChunkKB = 1 }

$sendName = if ($Name) { $Name } else { $fi.Name }
$size = [int64]$fi.Length

Write-Host "=== Gui file qua Bluetooth SPP ===" -ForegroundColor Cyan
Write-Host ("File   : {0} ({1} byte = {2:N1} KB)" -f $fi.FullName, $size, ($size / 1KB))
Write-Host ("Gui ten: {0}" -f $sendName)
Write-Host ("Header : PUT {0} {1} {2}" -f $Token, $sendName, $(if ($NoSize) { "*" } else { $size }))
Write-Host ("Khoi   : {0} KB, timeout {1} s" -f $ChunkKB, $TimeoutSec)
Write-Host ""
Write-Host "LUU Y: the TF cua panel phai con cho (panel tu kiem tra va tra ERR neu het)." -ForegroundColor DarkYellow
Write-Host ""

# ---------------------------------------------------------------------------
# 1) Tim cong COM cua TBT-PANEL (neu nguoi dung khong chi -Port)
# ---------------------------------------------------------------------------
function Show-SerialPorts {
    $names = [System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object
    Write-Host "Cac cong COM dang co: $($names -join ', ')" -ForegroundColor Yellow
    try {
        $pnp = Get-PnpDevice -Class Ports -ErrorAction Stop |
               Where-Object { $_.Status -eq 'OK' } |
               Select-Object FriendlyName, InstanceId
        foreach ($p in $pnp) {
            $mark = ""
            if ($p.FriendlyName -match 'TBT-PANEL' -or $p.FriendlyName -match 'Bluetooth') { $mark = "  <== co the la cong SPP (chon cong OUTGOING)" }
            Write-Host ("  {0}{1}" -f $p.FriendlyName, $mark)
        }
    } catch {
        try {
            Get-CimInstance Win32_SerialPort -ErrorAction Stop |
                ForEach-Object { Write-Host ("  {0} - {1}" -f $_.DeviceID, $_.Description) }
        } catch { Write-Host "  (khong liet ke duoc ten thiet bi - chi thay so cong o tren)" }
    }
    Write-Host ""
    Write-Host "Pair lan dau: Settings -> Bluetooth & devices -> Add device -> 'TBT-PANEL'" -ForegroundColor Yellow
    Write-Host "Neu duoc hoi ma PIN: 74196328 (PANEL_BT_PIN trong panel_config.h)" -ForegroundColor Yellow
    Write-Host "Sau khi pair, dung cong OUTGOING (thuong co 2 cong: Incoming + Outgoing)." -ForegroundColor Yellow
}

if (-not $Port) {
    $cands = @()
    try {
        $cands = (Get-PnpDevice -Class Ports -ErrorAction Stop |
                  Where-Object { $_.Status -eq 'OK' -and ($_.FriendlyName -match 'TBT-PANEL' -or $_.FriendlyName -match 'Bluetooth') } |
                  ForEach-Object { if ($_.FriendlyName -match '\((COM\d+)\)') { $Matches[1] } }) |
                 Where-Object { $_ } | Sort-Object -Unique
    } catch { }
    if ($cands.Count -ge 1) {
        $Port = $cands[0]
        Write-Host "Tu do duoc cong SPP: $Port" -ForegroundColor Green
        if ($cands.Count -gt 1) { Write-Host ("  (cung thay: {0} - neu loi hay thu cong khac)" -f ($cands -join ', ')) -ForegroundColor Yellow }
    } else {
        Write-Host "KHONG tu do duoc cong nao cua TBT-PANEL." -ForegroundColor Red
        Show-SerialPorts
        Write-Host "Hay chay lai voi -Port COMx." -ForegroundColor Red
        exit 1
    }
}

if (-not ([System.IO.Ports.SerialPort]::GetPortNames() -contains $Port)) {
    Write-Host "KHONG thay $Port." -ForegroundColor Red
    Show-SerialPorts
    exit 1
}

# ---------------------------------------------------------------------------
# 2) Mo cong SPP
# ---------------------------------------------------------------------------
$sp = New-Object System.IO.Ports.SerialPort
$sp.PortName    = $Port
$sp.BaudRate    = $Baud
$sp.Parity      = [System.IO.Ports.Parity]::None
$sp.DataBits    = 8
$sp.StopBits    = [System.IO.Ports.StopBits]::One
$sp.NewLine     = "`n"
$sp.ReadTimeout = 100
$sp.WriteTimeout = 5000
$sp.DtrEnable   = $true
$sp.RtsEnable   = $true

try { $sp.Open() } catch {
    Write-Host "Khong mo duoc $Port : $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Goi y: dong app dang giu cong (Putty/Arduino Serial Monitor), hoac thu cong OUTGOING khac." -ForegroundColor Yellow
    exit 1
}
Write-Host "Da mo $Port (115200 8N1)." -ForegroundColor Green

# ---------------------------------------------------------------------------
# 3) Ham doc phan hoi cua panel (in ra theo thoi gian thuc)
# ---------------------------------------------------------------------------
$script:rxbuf = ""
$script:done = $false
$script:err  = $false
$script:acked = [int64]0          # so byte panel XAC NHAN da tieu hoa (tu dong PROG <n>)
$script:prog_seen = $false        # da thay dong PROG nao chua (de biet panel co ho tro khong)
$script:flow = $true              # bat dieu khien luong theo PROG

function Read-Panel([int]$ms) {
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalMilliseconds -lt $ms) {
        $n = $sp.BytesToRead
        if ($n -gt 0) {
            $b = New-Object byte[] $n
            [void]$sp.Read($b, 0, $n)
            $script:rxbuf += [System.Text.Encoding]::ASCII.GetString($b)
            while ($script:rxbuf.Contains("`n")) {
                $i = $script:rxbuf.IndexOf("`n")
                $line = $script:rxbuf.Substring(0, $i).TrimEnd("`r")
                $script:rxbuf = $script:rxbuf.Substring($i + 1)
                if ($line.Length -gt 0) {
                    $col = "Gray"
                    if ($line -like "ERR*")  { $col = "Red";    $script:err  = $true }
                    if ($line -like "DONE*") { $col = "Green";  $script:done = $true }
                    if ($line -like "OK*")   { $col = "Cyan" }
                    if ($line -like "POST*") { $col = "Magenta" }
                    if ($line -like "PROG*") {
                        $col = "DarkGray"
                        # "PROG <so_byte_da_nhan>" -> moc dieu khien luong
                        if ($line -match '^PROG\s+(\d+)') {
                            $script:acked = [int64]$Matches[1]
                            $script:prog_seen = $true
                        }
                    }
                    # KHONG in moi dong PROG (4 KB/lan se rat nhieu) - chi ghi de dong tien do
                    if ($line -notlike "PROG*") { Write-Host ("  [panel] {0}" -f $line) -ForegroundColor $col }
                }
            }
        } else {
            Start-Sleep -Milliseconds 10
        }
        if ($script:done -or $script:err) { return }
    }
}

function Send-Text([string]$s) {
    $b = [System.Text.Encoding]::ASCII.GetBytes($s)
    $sp.Write($b, 0, $b.Length)
}

# ---------------------------------------------------------------------------
# 4) Gui header
# ---------------------------------------------------------------------------
$header = if ($NoSize) { "PUT $Token $sendName *`r`n" } else { "PUT $Token $sendName $size`r`n" }
Send-Text $header
Write-Host "Da gui header, cho panel tra loi..." -ForegroundColor Cyan
Read-Panel 3000
if ($script:err) { Write-Host "Panel TU CHOI phien (xem dong ERR o tren)." -ForegroundColor Red; $sp.Close(); exit 2 }

# ---------------------------------------------------------------------------
# 5) Gui noi dung file theo khoi + in tien do
# ---------------------------------------------------------------------------
$chunk = $ChunkKB * 1024
$fs = [System.IO.File]::OpenRead($fi.FullName)
$buf = New-Object byte[] $chunk
$sent = [int64]0
$t0 = Get-Date
$lastPrint = -1
$window = [int64]$WindowKB * 1024
$flowToldShown = $false
Write-Host ("Dang gui du lieu... (dieu khien luong theo PROG, cua so {0} KB)" -f $WindowKB) -ForegroundColor Cyan
try {
    while (($read = $fs.Read($buf, 0, $buf.Length)) -gt 0) {
        # --- Cho den khi cua so byte "dang bay" tut xuong duoi nguong (panel bao PROG) ---
        if ($script:flow) {
            $stall0 = Get-Date
            while ((($sent - $script:acked) -gt $window) -and -not $script:done -and -not $script:err) {
                Read-Panel 20
                if (((Get-Date) - $stall0).TotalSeconds -gt 10) {
                    Write-Host "`nCANH BAO: panel khong bao PROG trong 10 s -> tat dieu khien luong." -ForegroundColor Yellow
                    $script:flow = $false
                    break
                }
            }
            # Panel khong ho tro PROG (ban firmware cu): chi canh bao 1 lan roi di tiep
            if (-not $script:prog_seen -and -not $flowToldShown -and $sent -gt (2 * $window)) {
                $flowToldShown = $true
                Write-Host "`nCANH BAO: chua thay dong PROG nao - panel co the la ban cu (khong dieu khien luong)." -ForegroundColor Yellow
                $script:flow = $false
            }
        }
        if ($script:err -or $script:done) { break }

        $sp.Write($buf, 0, $read)
        $sent += $read
        # doc phan hoi (OK/PROG/DONE/ERR) xen ke, khong de day dem
        if ($sp.BytesToRead -gt 0) { Read-Panel 1 }
        if ($script:err) { break }

        $pct = [int](100 * $sent / $size)
        $now = Get-Date
        $sec = ($now - $t0).TotalSeconds
        if ($pct -ne $lastPrint -and ($sec -ge 0.5)) {
            $kbs = if ($sec -gt 0) { ($sent / 1KB) / $sec } else { 0 }
            Write-Host ("  gui {0,3}%  {1,8:N0}/{2:N0} KB  {3,6:N1} KB/s (panel da nhan {4:N0} KB)" -f `
                        $pct, ($sent / 1KB), ($size / 1KB), $kbs, ($script:acked / 1KB)) -NoNewline
            Write-Host "`r" -NoNewline
            $lastPrint = $pct
        }
        if ($script:done -or $script:err) { break }
        if (((Get-Date) - $t0).TotalSeconds -gt $TimeoutSec) {
            Write-Host "`nHET THOI GIAN gui du lieu ($TimeoutSec s)." -ForegroundColor Red
            break
        }
    }
} finally {
    $fs.Close()
    Write-Host ("`nDa gui {0:N0}/{1:N0} byte ({2:N1} KB) trong {3:N1} s; panel xac nhan {4:N0} KB." -f `
                $sent, $size, ($sent / 1KB), ((Get-Date) - $t0).TotalSeconds, ($script:acked / 1KB)) -ForegroundColor Cyan
    if (-not $NoSize -and $script:acked -lt $sent) {
        Write-Host ("CANH BAO: panel moi xac nhan {0}/{1} byte - co the con dang xu ly hoac bi mat byte (xem log COM7 'drop=')." -f $script:acked, $sent) -ForegroundColor Yellow
    }
}

# ---------------------------------------------------------------------------
# 6) Cho panel xu ly (voi -NoSize thi panel ket thuc khi im 3 s; sau do con POST len ESP3D)
# ---------------------------------------------------------------------------
Write-Host "Cho panel xu ly (POST len ESP3D + M22/M21)..." -ForegroundColor Cyan
$deadline = (Get-Date).AddSeconds($TimeoutSec)
while (-not $script:done -and -not $script:err -and (Get-Date) -lt $deadline) {
    Read-Panel 500
}

Write-Host ""
if ($script:done) {
    Write-Host "XONG: panel da ghi file vao the SD cua Marlin va init lai the (M22/M21)." -ForegroundColor Green
    Write-Host "Mo man FILES tren panel (hoac bam REFRESH) de thay file moi." -ForegroundColor Green
    $code = 0
} elseif ($script:err) {
    Write-Host "LOI: xem dong [panel] ERR o tren." -ForegroundColor Red
    $code = 3
} else {
    Write-Host "Het thoi gian cho ma chua thay DONE/ERR. Kiem tra log COM7 cua panel." -ForegroundColor Yellow
    $code = 4
}

try { $sp.Close() } catch { }
exit $code
